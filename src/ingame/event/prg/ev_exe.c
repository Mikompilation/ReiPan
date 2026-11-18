/* ==========================================================================
 *  ingame/event/prg/ev_exe.c
 *
 *  Event execution: FF3 0x19abe8..0x19b158, FF2 ev_exe.c function for
 *  function (MioPan's port is the base). ev_exe_ctrl is the list of running
 *  event programs; EventExe steps each one a frame through EventExeFuncCall
 *  (ev_macro.c). The Set*Status functions move an event between states and
 *  put its init / main / end program on the list; the init program is run
 *  at once off a local slot instead, so it finishes in the frame its open
 *  condition passed. Every transition also re-parents the event's
 *  sub-events: one still waiting to open follows its parent, one already
 *  running is left alone.
 *
 *  FF3 differs from FF2: the queue-full banner and the asserts are gone,
 *  the save getter hands out the table (FF2 SetSave_EvExeCtrl filled an
 *  MC_SAVE_DATA), EvExeGetCtrl is new, and the slots' program addresses are
 *  turned into offsets around a memory-card copy (EvExeCtrlToSave /
 *  FromSave), which also keeps the host's saves independent of where the
 *  event file sits in memory.
 * ======================================================================== */
#include "ingame/event/prg/ev_exe.h"

#include <stdint.h>                             // uintptr_t

#include "common/reipan_stub.h"
#include "ingame/event/prg/ev_change.h"         // SetEventState, GetEvState, EV_STATE_*
#include "ingame/event/prg/ev_get.h"            // EvGetExeAddr, EvGetSubId, EV_TBL_*, EV_SUB_ID_MAX
#include "ingame/event/prg/ev_macro.h"          // EventMacroInit, EventExeFuncCall
#include "ingame/event/prg/ev_open.h"           // EventDelCondition, EventSetOpenCondition

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static EV_EXE_CTRL ev_exe_ctrl[EV_EXE_CTRL_MAX];  // bss 0x3e31e8 (0x18 bytes a slot on the EE)

// ──────────────────────────────────────────────────────────────────────
// Save block 7. PC: the table's host size (the program addresses are
// pointers); FF3 returns 0xe10. FF3 0x0019abe8.

int EvExeCtrlSaveGet(void **addr)
{
    *addr = ev_exe_ctrl;
    return sizeof(ev_exe_ctrl);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019ac00; FF2 EventExeCtrlInit.

static void EventExeCtrlInit(EV_EXE_CTRL *ctrl, int event_id, u_char *event_addr)
{
    ctrl->event_id = event_id;
    ctrl->event_addr = event_addr;
    ctrl->process = 0;
    ctrl->stop_timer = 0;
    ctrl->if_state = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The slot SetEventExeCtrl should write: -2 when the event already has one,
// else the first free slot, -1 when all are used.
// FF3 0x0019ac18; FF2 GetExeCtrlEmpty.

static int GetExeCtrlEmpty(int event_id)
{
    int i;

    if (EvExeGetCtrl(event_id) != nullptr)
        return -2;

    for (i = 0; ev_exe_ctrl[i].event_id != -1; ) {
        if (++i >= EV_EXE_CTRL_MAX)
            return -1;
    }
    return i;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019ac78; FF2 EventExeInit.

void EventExeInit(void)
{
    int i;

    for (i = 0; i < EV_EXE_CTRL_MAX; i++) {
        EventExeCtrlInit(&ev_exe_ctrl[i], -1, nullptr);
    }
    EventMacroInit();
}

// ──────────────────────────────────────────────────────────────────────
// The event opens: its init program runs now, off a local slot, and its
// sub-events become eligible to open (a parked one is re-armed first).
// FF3 0x0019acc8; FF2 SetEventInitStatus.

void SetEventInitStatus(int event_id)
{
    int id_tbl[EV_SUB_ID_MAX];
    EV_EXE_CTRL init_event;
    int i;

    EvGetSubId(event_id, id_tbl);
    EvExeRelease(event_id);
    SetEventState(event_id, EV_STATE_INIT);
    EventDelCondition(event_id);

    EventExeCtrlInit(&init_event, event_id, EvGetExeAddr(EV_TBL_INIT_PRG, event_id));
    EventExeFuncCall(&init_event);

    for (i = 0; i < EV_SUB_ID_MAX; i++) {
        if (id_tbl[i] == -1)
            continue;

        switch (GetEvState(id_tbl[i])) {
        case EV_STATE_SUSPEND:
            SetEventState(id_tbl[i], EV_STATE_WAIT_OPEN);
            // fall through
        case EV_STATE_WAIT_OPEN:
            EventSetOpenCondition(id_tbl[i]);
            break;
        default:
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The event's main program goes on a slot. FF3 0x0019adb0; FF2
// SetEventExeStatus.

void SetEventExeStatus(int event_id)
{
    EvExeRelease(event_id);
    EventDelCondition(event_id);
    SetEventExeCtrl(event_id, EV_TBL_EXE_PRG);
    SetEventState(event_id, EV_STATE_EXE);
}

// ──────────────────────────────────────────────────────────────────────
// The event closes: its end program goes on a slot and its sub-events that
// have not opened are parked. FF3 0x0019adf0; FF2 SetEventEndExeStatus.

void SetEventEndExeStatus(int event_id)
{
    EventDelCondition(event_id);
    EvExeRelease(event_id);
    SetEventState(event_id, EV_STATE_END);
    SetEventExeCtrl(event_id, EV_TBL_END_PRG);
    EventEnd_SubStateChange(event_id);
}

// ──────────────────────────────────────────────────────────────────────
// Sub-events still waiting to open lose their conditions and are parked;
// running ones pass the same on to their own sub-events.
// FF3 0x0019ae38; FF2 EventEnd_SubStateChange.

void EventEnd_SubStateChange(int event_id)
{
    int id_tbl[EV_SUB_ID_MAX];
    int i;

    EvGetSubId(event_id, id_tbl);

    for (i = 0; i < EV_SUB_ID_MAX; i++) {
        if (id_tbl[i] == -1)
            continue;

        switch (GetEvState(id_tbl[i])) {
        case EV_STATE_WAIT_OPEN:
            EventDelCondition(id_tbl[i]);
            SetEventState(id_tbl[i], EV_STATE_SUSPEND);
            break;
        case EV_STATE_INIT:
        case EV_STATE_EXE:
        case EV_STATE_WAIT_END:
            EventEnd_SubStateChange(id_tbl[i]);
            break;
        default:
            break;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The event's program from table use_table onto a free slot; nothing when
// it already has one or none is free (FF2 printed a banner for the latter).
// FF3 0x0019aee0; FF2 SetEventExeCtrl.

void SetEventExeCtrl(int event_id, u_char use_table)
{
    int empty_area = GetExeCtrlEmpty(event_id);

    if (empty_area >= 0)
        EventExeCtrlInit(&ev_exe_ctrl[empty_area], event_id, EvGetExeAddr(use_table, event_id));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019af58.

EV_EXE_CTRL *EvExeGetCtrl(int event_id)
{
    int i;

    for (i = 0; i < EV_EXE_CTRL_MAX; i++) {
        if (ev_exe_ctrl[i].event_id == event_id)
            return &ev_exe_ctrl[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019af90; FF2 EventExe.

void EventExe(void)
{
    int i;

    for (i = 0; i < EV_EXE_CTRL_MAX; i++) {
        if (ev_exe_ctrl[i].event_id != -1)
            EventExeFuncCall(&ev_exe_ctrl[i]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019aff0; FF2 EventExeRelease.

void EvExeRelease(int event_id)
{
    EV_EXE_CTRL *ctrl = EvExeGetCtrl(event_id);

    if (ctrl != nullptr)
        EventExeCtrlInit(ctrl, -1, nullptr);
}

// FF3 0x0019b028; FF2 CheckEventExeEntry.
int CheckEventExeEntry(int event_id)
{
    return (EvExeGetCtrl(event_id) != nullptr) ? 1 : -1;
}

// ──────────────────────────────────────────────────────────────────────
// Adds (to_addr 1) or subtracts (0) the start of each running program's
// table entry -- the init program for EV_STATE_INIT, the end program for
// EV_STATE_END, else the main program -- so the saved slots hold offsets.
// FF3 0x0019b058.

static void EvExeCtrlSaveFixup(int to_addr)
{
    int i;

    for (i = 0; i < EV_EXE_CTRL_MAX; i++) {
        EV_EXE_CTRL *ctrl = &ev_exe_ctrl[i];
        int tbl;
        uintptr_t base;

        if (ctrl->event_id == -1)
            continue;

        switch (GetEvState(ctrl->event_id)) {
        case EV_STATE_INIT: tbl = EV_TBL_INIT_PRG; break;
        case EV_STATE_END:  tbl = EV_TBL_END_PRG;  break;
        default:            tbl = EV_TBL_EXE_PRG;  break;
        }
        base = (uintptr_t)EvGetExeAddr(tbl, ctrl->event_id);
        if (to_addr != 0)
            ctrl->event_addr = (u_char *)((uintptr_t)ctrl->event_addr + base);
        else
            ctrl->event_addr = (u_char *)((uintptr_t)ctrl->event_addr - base);
    }
}

// FF3 0x0019b128 (EventAfterLoad).
void EvExeCtrlFromSave(void)
{
    EvExeCtrlSaveFixup(1);
}

// FF3 0x0019b140 (EventEnd).
void EvExeCtrlToSave(void)
{
    EvExeCtrlSaveFixup(0);
}

// ---- stubs (tools/reverse/stubgen.py) ----

