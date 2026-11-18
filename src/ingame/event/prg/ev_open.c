/* ==========================================================================
 *  ingame/event/prg/ev_open.c
 *
 *  The event control centre: FF3 0x19baf8..0x19c658, FF2 ev_open.c (MioPan's
 *  port is the base; see ev_open.h for the condition streams).
 *
 *  FF3 differs from FF2: the tables hold 450 rows (FF2 250); the condition
 *  table and most tests moved to ev_change.c, and a condition's arguments
 *  are decoded by its format string (EvGetCmdArgs) into a stack array the
 *  row's args points at while it is judged; the asserts, the "SET ERROR"
 *  prints and the full-table error are gone, and EventCtrlCenterMain no longer
 *  resets a photographed object (FF3 has no ev_photo_obj: the photo
 *  conditions read the develop table and the photographed flags instead).
 *  New: the save getters, the save fixup and a debug name getter.
 * ======================================================================== */
#include "ingame/event/prg/ev_open.h"

#include <stdint.h>                             // uintptr_t
#include <string.h>                             // memcpy

#include "common/get_byte.h"                    // Get1Byte
#include "ingame/enemy/enemy.h"                 // EneIdFlgCheck
#include "ingame/event/prg/ev_change.h"         // GetEvCondWrk, EV_COND_*, GetEvState, SetEventState, EV_STATE_*
#include "ingame/event/prg/ev_exe.h"            // SetEventInitStatus, SetEventEndExeStatus
#include "ingame/photo/photo_obj_dat.h"         // PhotoObjIsPhotographed
#include "ingame/event/prg/ev_get.h"            // EvGetExeAddr, EvGetSubId, EvGetCmdArgs, EV_TBL_*, EV_SUB_ID_MAX
#include "ingame/story_develop.h"               // develop_film

// Callees in unassigned runs (move to their headers when the runs are defined).
// unassigned@00229478:

#define EV_DEV_TBL_NUM  DEVELOP_FILM_NUM

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static EV_CTRL_CENTER ev_ctrl_center[EV_CTRL_CENTER_MAX];  // bss 0x3e4008 (8 bytes a row on the EE)
static EV_PRI_WRK     ev_pri_wrk;                          // bss 0x3e4e18
static EV_COND_CTRL   ev_cond_ctrl;                        // sdata 0x373080

// ──────────────────────────────────────────────────────────────────────
// Save block 8. PC: the table's host size (the stream addresses are
// pointers); FF3 returns 0xe10. FF3 0x0019baf8.

int EvCtrlCenterSaveGet(void **addr)
{
    *addr = ev_ctrl_center;
    return sizeof(ev_ctrl_center);
}

// Save block 50. FF3 0x0019bb10.

int EvPriCtrlSaveGet(void **addr)
{
    *addr = &ev_pri_wrk;
    return sizeof(ev_pri_wrk);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019bb28; FF2 SetEvCtrlCenter.

static void SetEvCtrlCenter(EV_CTRL_CENTER *center, short condition, short event_id, u_char *exe_addr)
{
    center->exe_addr = exe_addr;
    center->condition = condition;
    center->event_id = event_id;
}

// FF3 0x0019bb48; FF2 EvCtrlCenterInit.

void EvCtrlCenterInit(void)
{
    int i;

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        SetEvCtrlCenter(&ev_ctrl_center[i], -1, -1, nullptr);
    }
}

// FF3 0x0019bb98; FF2 EvCondCtrlInit.

void EvCondCtrlInit(void)
{
    ev_cond_ctrl.end_switch = 1;
    ev_cond_ctrl.open_switch = 1;
}

// Every claim dropped; once a frame, before the conditions are judged.
// FF3 0x0019bbb0; FF2 EvPriCtrlInit.

static void EvPriCtrlInit(void)
{
    int i;

    ev_pri_wrk.num = 0;
    for (i = 0; i < EV_PRI_CTRL_MAX; i++) {
        ev_pri_wrk.ctrl[i].event_id = -1;
        ev_pri_wrk.ctrl[i].priority = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The event claims priority for this frame; a second claim overwrites the
// first. FF3 0x0019bbe8; FF2 SetEventPriority.

void SetEventPriority(int event_id, u_char priority)
{
    int i;

    for (i = 0; i < EV_PRI_CTRL_MAX; i++) {
        if (ev_pri_wrk.ctrl[i].event_id == event_id) {
            ev_pri_wrk.ctrl[i].priority = priority;
            ev_pri_wrk.ctrl[i].event_id = event_id;
            break;
        }
    }

    if (i == EV_PRI_CTRL_MAX) {
        // PC: the EE writes past the table when it is full; drop the claim.
        if (ev_pri_wrk.num >= EV_PRI_CTRL_MAX)
            return;
        ev_pri_wrk.ctrl[ev_pri_wrk.num].event_id = event_id;
        ev_pri_wrk.ctrl[ev_pri_wrk.num].priority = priority;
        ev_pri_wrk.num++;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Posting a stream
// ──────────────────────────────────────────────────────────────────────

static int EventSetCondition(int event_id, u_char *exe_addr);

// The event waits for its open condition; sub-events parked when it last
// ended wait for theirs again. FF3 0x0019bc68; FF2 EventSetOpenCondition.

void EventSetOpenCondition(int event_id)
{
    u_char *open_addr;
    int     id_tbl[EV_SUB_ID_MAX];
    int     i;

    open_addr = EvGetExeAddr(EV_TBL_OPEN_COND, event_id);
    EvGetSubId(event_id, id_tbl);
    EventSetCondition(event_id, open_addr);

    for (i = 0; i < EV_SUB_ID_MAX; i++) {
        if (id_tbl[i] != -1 && GetEvState(id_tbl[i]) == EV_STATE_SUSPEND) {
            SetEventState(id_tbl[i], EV_STATE_WAIT_OPEN);
        }
    }
}

// FF3 0x0019bd08; FF2 EventSetCloseCondition.

void EventSetCloseCondition(int event_id)
{
    u_char *close_addr;

    close_addr = EvGetExeAddr(EV_TBL_CLOSE_COND, event_id);
    SetEventState(event_id, EV_STATE_WAIT_END);
    EventSetCondition(event_id, close_addr);
}

// A stream without conditions: the event starts (or ends) at once.
// FF3 0x0019bd58; FF2 EventAutoStart.

static void EventAutoStart(int event_id)
{
    switch (GetEvState(event_id)) {
    case EV_STATE_WAIT_OPEN:
        SetEventInitStatus(event_id);
        break;
    case EV_STATE_WAIT_END:
        SetEventEndExeStatus(event_id);
        break;
    }
}

// FF3 0x0019bda8; FF2 EventDelCondition.

void EventDelCondition(int event_id)
{
    int i;

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        if (ev_ctrl_center[i].event_id == (short)event_id) {
            SetEvCtrlCenter(&ev_ctrl_center[i], -1, -1, nullptr);
        }
    }
}

// The first free row, nullptr when all are used.
// FF3 0x0019be10 (FF2 had the loop in EventSetCondition).

static EV_CTRL_CENTER *EvCtrlCenterGetFree(void)
{
    int i;

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        if (ev_ctrl_center[i].condition == -1)
            return &ev_ctrl_center[i];
    }
    return nullptr;
}

// Posts the first condition of each group of the stream; only it is
// tested every frame, the rest of its group once it passes. A stream that
// posts nothing starts the event at once. Always 1 (FF2 returned 0 when the
// table was full). FF3 0x0019be50; FF2 EventSetCondition.

static int EventSetCondition(int event_id, u_char *exe_addr)
{
    int regist_flg;
    int label;

    regist_flg = 0;

    while (*exe_addr != EV_COND_END) {
        label = Get1Byte(exe_addr);
        EV_COND_WRK *wrk = GetEvCondWrk(label);

        if (label == EV_COND_OR) {
            regist_flg = 0;             // a new group: its first condition is posted too
            exe_addr += 4;
            continue;
        }

        if (regist_flg == 0) {
            EV_CTRL_CENTER *center = EvCtrlCenterGetFree();

            regist_flg = 1;
            // PC: the EE writes the row at address 0 when the table is full.
            if (center != nullptr)
                SetEvCtrlCenter(center, (short)label, (short)event_id, exe_addr);
        }
        exe_addr += wrk->size;
    }

    if (regist_flg == 0)
        EventAutoStart(event_id);

    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Judging
// ──────────────────────────────────────────────────────────────────────

static void EventPriorityJudge(int *ev_exe_wait, int ev_wait_cnt);
static int  EventOpenJudge(int event_id, u_char *exe_addr);
static int  EventCloseJudge(int event_id, u_char *exe_addr);

// Once a frame (EventMain): every posted condition is tested; one that
// passes has the rest of its group judged, and then its event waits to
// start (open) or ends (close). FF3 0x0019bf48; FF2 EventCtrlCenterMain.

void EventCtrlCenterMain(void)
{
    int ev_exe_wait[EV_CTRL_CENTER_MAX];
    int ev_wait_cnt;
    int args[16];                       // 0x40 bytes of stack on the EE
    int i;
    int j;

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        ev_exe_wait[i] = -1;
    }
    ev_wait_cnt = 0;

    EvPriCtrlInit();

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        EV_CTRL_CENTER *center = &ev_ctrl_center[i];
        int             event_id = center->event_id;
        u_char         *exe_addr = center->exe_addr;
        EV_COND_WRK    *wrk;
        EV_COND_FUNC    func;

        if (event_id == -1)
            continue;

        wrk = GetEvCondWrk(center->condition);
        func = wrk->func;
        wrk->args = args;
        EvGetCmdArgs(args, exe_addr, wrk->fmt);

        // PC: only EV_OR's row has no test, and it is never posted; the EE
        // would jump to 0.
        if (func == nullptr || func(event_id, exe_addr) != 1)
            continue;

        exe_addr += wrk->size;          // the rest of the group follows

        switch (GetEvState(event_id)) {
        case EV_STATE_WAIT_OPEN:
            if (ev_cond_ctrl.open_switch != 0 && EventOpenJudge(event_id, exe_addr) == 1) {
                // once per event: it can post a condition from several groups
                for (j = 0; j < ev_wait_cnt; j++) {
                    if (ev_exe_wait[j] == event_id)
                        break;
                }
                if (j == ev_wait_cnt) {
                    ev_exe_wait[ev_wait_cnt] = event_id;
                    ev_wait_cnt++;
                }
            }
            break;

        case EV_STATE_WAIT_END:
            if (ev_cond_ctrl.end_switch != 0 && EventCloseJudge(event_id, exe_addr) == 1) {
                SetEventEndExeStatus(event_id);
            }
            break;
        }
    }

    EventPriorityJudge(ev_exe_wait, ev_wait_cnt);
}

// Of the events whose open condition passed this frame, those that claimed
// the highest priority any of them claimed start, and those that claimed
// none. FF3 0x0019c108; FF2 EventPriorityJudge.

static void EventPriorityJudge(int *ev_exe_wait, int ev_wait_cnt)
{
    u_char ev_pri_high;
    int    i;
    int    j;

    ev_pri_high = 0;
    for (i = 0; i < ev_pri_wrk.num; i++) {
        EV_PRI_CTRL *pri = &ev_pri_wrk.ctrl[i];

        if (pri->event_id == -1)
            continue;
        for (j = 0; j < ev_wait_cnt; j++) {
            if (ev_exe_wait[j] == pri->event_id && ev_pri_high < pri->priority)
                ev_pri_high = pri->priority;
        }
    }

    for (i = 0; i < ev_wait_cnt; i++) {
        int priority_flg = 0;
        int set_priority = 0;

        for (j = 0; j < ev_pri_wrk.num; j++) {
            if (ev_pri_wrk.ctrl[j].event_id != -1 && ev_pri_wrk.ctrl[j].event_id == ev_exe_wait[i]) {
                priority_flg = 1;
                set_priority = j;
                break;
            }
        }

        if (priority_flg == 0 || ev_pri_wrk.ctrl[set_priority].priority == ev_pri_high)
            SetEventInitStatus(ev_exe_wait[i]);
    }
}

static int EventConditionJudge(int event_id, u_char *exe_addr);

// FF3 0x0019c260; FF2 EventOpenJudge.

static int EventOpenJudge(int event_id, u_char *exe_addr)
{
    return EventConditionJudge(event_id, exe_addr);
}

// FF3 0x0019c278; FF2 EventCloseJudge.

static int EventCloseJudge(int event_id, u_char *exe_addr)
{
    return EventConditionJudge(event_id, exe_addr);
}

// The rest of a stream from the condition after a posted one: conditions
// AND (only tested while the group still holds), EV_COND_OR with the group
// holding means the condition is met, else the next group starts at 1;
// EV_COND_END returns what the last group came to.
// FF3 0x0019c290; FF2 EventConditionJudge.

static int EventConditionJudge(int event_id, u_char *exe_addr)
{
    int judge_res;
    int args[16];                       // 0x40 bytes of stack on the EE

    judge_res = 1;

    while (1) {
        int label = Get1Byte(exe_addr);
        int size;

        if (label == EV_COND_OR) {
            if (judge_res == 1)
                return 1;
            judge_res = 1;
            size = 4;
        } else if (label == EV_COND_END) {
            return judge_res;
        } else {
            EV_COND_WRK *wrk = GetEvCondWrk(label);

            size = wrk->size;
            if (judge_res == 1) {
                wrk->args = args;
                EvGetCmdArgs(args, exe_addr, wrk->fmt);
                // PC: a row without a test would jump to 0 on the EE.
                judge_res = wrk->func != nullptr ? wrk->func(event_id, exe_addr) : 0;
            }
        }
        exe_addr += size;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The switches
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0019c380; FF2 SetOpenCondSwitch.

void SetOpenCondSwitch(u_char flg)
{
    ev_cond_ctrl.open_switch = flg;
}

// FF3 0x0019c390.

u_char GetOpenCondSwitch(void)
{
    return ev_cond_ctrl.open_switch;
}

// FF3 0x0019c398; FF2 SetEndCondSwitch.

void SetEndCondSwitch(u_char flg)
{
    ev_cond_ctrl.end_switch = flg;
}

// ──────────────────────────────────────────────────────────────────────
// Save
// ──────────────────────────────────────────────────────────────────────

// Each posted row's stream address relative to its event's open stream
// (close stream while it waits to end): to_addr 0 address -> offset,
// 1 back. FF3 0x0019c3a8.

static void EvCtrlCenterSaveFixup(int to_addr)
{
    int i;

    for (i = 0; i < EV_CTRL_CENTER_MAX; i++) {
        EV_CTRL_CENTER *center = &ev_ctrl_center[i];
        int             tbl;
        uintptr_t       base;

        if (center->event_id == -1)
            continue;

        tbl = GetEvState(center->event_id) == EV_STATE_WAIT_END ? EV_TBL_CLOSE_COND : EV_TBL_OPEN_COND;
        base = (uintptr_t)EvGetExeAddr(tbl, center->event_id);
        if (to_addr != 0)
            center->exe_addr = (u_char *)((uintptr_t)center->exe_addr + base);
        else
            center->exe_addr = (u_char *)((uintptr_t)center->exe_addr - base);
    }
}

// FF3 0x0019c450 (EventEnd).

void EvCtrlCenterToSave(void)
{
    EvCtrlCenterSaveFixup(0);
}

// FF3 0x0019c468 (EventAfterLoad).

void EvCtrlCenterFromSave(void)
{
    EvCtrlCenterSaveFixup(1);
}

// Row no's condition keyword ("PLYR_AREAIN") and, through event_id when
// given, its event; nullptr for a free row. Nothing calls it (a debug
// display's, like FF2 EvDbgDispCenter). FF3 0x0019c480.

const char *EvCtrlCenterGetCondName(u_int no, int *event_id)
{
    if (no >= EV_CTRL_CENTER_MAX || ev_ctrl_center[no].event_id == -1)
        return nullptr;

    if (event_id != nullptr)
        *event_id = ev_ctrl_center[no].event_id;
    return GetEvCondWrk(ev_ctrl_center[no].condition)->name;
}

// ──────────────────────────────────────────────────────────────────────
// The photo conditions
// ──────────────────────────────────────────────────────────────────────

// PHOTO_IS_DEV: a film is being developed (a develop-table row in state 1).
// FF3 0x0019c508.

int EvPhotoIsDev(int event_id, u_char *addr)
{
    int i;

    (void)event_id;
    (void)addr;
    for (i = 0; i < EV_DEV_TBL_NUM; i++) {
        if (develop_film[i].state == DEVELOP_STATE_SHOT)
            return 1;
    }
    return 0;
}

// PHOTO_STATE_ "112": state +1 0 -> flag +2 is set, 1 -> it is clear.
// FF3 0x0019c550.

int EvPhotoState(int event_id, u_char *addr)
{
    struct { u_char code; s_char state; short no; } a;
    u_int flg;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));
    flg = PhotoObjIsPhotographed((u_int)(int)a.no);

    if (a.state == 0)
        return flg != 0;
    if (a.state == 1)
        return flg == 0;
    return 0;
}

// PHOTO_OBJ "1124": object +1 was photographed: types 0..2 an enemy (+4,
// its photographed flag 1), 8 the flag +4 of the table at 0x3f7770; other
// types never. FF3 0x0019c5b0; FF2 EvPhotoObj (which compared FF2's
// ev_photo_obj).

int EvCondPhotoObj(int event_id, u_char *addr)
{
    struct { u_char code; s_char type; short _2; int id; } a;
    u_int flg;

    (void)event_id;
    memcpy(&a, addr, sizeof(a));

    if (a.type < 0)
        return 0;
    if (a.type < 3)
        flg = EneIdFlgCheck((u_char)a.type, (u_short)a.id, 1);
    else if (a.type == 8)
        flg = PhotoObjIsPhotographed((u_int)a.id);
    else
        return 0;
    return flg != 0;
}

// NOT_PHOTO_OBJ "1124". FF3 0x0019c630; FF2 EvNotPhotoObj.

int EvNotPhotoObj(int event_id, u_char *addr)
{
    return EvCondPhotoObj(event_id, addr) != 1;
}
