/* ==========================================================================
 *  ingame/event/prg/ev_change.h
 *
 *  FF3 0x1992d0..0x19a0a0: the first event conditions, the condition table
 *  and the event state core. FF3's file sorts between ev_get.c and
 *  ev_macro.c; its name is not known, so it is named after FF2's ev_change.c,
 *  whose functions make up most of it:
 *
 *    - conditions (rows of the condition table 0x3242e8) that FF2 keeps in
 *      ev_open.c, and FF3-only ones (research, develop, telephone, tape,
 *      cine film, stream, day side, mission counter); the rest of the
 *      conditions are in ingame/event/prg/ev_command.c;
 *    - the condition table itself and its row getters;
 *    - ev_wrk (bss 0x3e0098): the state byte of every event, and its run
 *      count; SetEventState / GetEvState and the wait flag;
 *    - the parked state changes (FF2 ev_change.c): Req_ / Del_ /
 *      CompulsionSetEventState and the per-frame CompulsionSetEventStateMain.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_CHANGE_H
#define _INGAME_EVENT_PRG_EV_CHANGE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// ── event states (ev_wrk) ─────────────────────────────────────────────────

#define EV_EVENT_NUM    0x6f5           // event ids 0..0x6f4 (EVENT_REBOOT's loop)

// EVENT_STATE::state. FF2's labels (EvDbg_EventStatePrint) in the comments.
enum EV_STATE
{
    EV_STATE_WAIT_OPEN = 0,             // "EV_UNSTART": open conditions posted
    EV_STATE_INIT      = 1,             // "EV_EXE_INIT": init program queued / running
    EV_STATE_EXE       = 2,             // "EV_EXE_NOW": main program running (counted in cnt)
    EV_STATE_WAIT_END  = 3,             // "EV_END_WAIT": close conditions posted
    EV_STATE_SUSPEND   = 4,             // "EV_EXE_SLEEP": parked, re-armed on re-open
    EV_STATE_END       = 5,             // "EV_EXE_END"
    EV_STATE_LOCK      = 6              // "EV_FORCED_END": never moved again
};

// One event's run state (FF2 EVENT_STATE; FF3 uses dumy1 as a run count).
struct EVENT_STATE                      /* 0x4 */
{
    /* 0x0 */ u_char state;             // EV_STATE
    /* 0x1 */ s_char cnt;               // times the event entered EV_STATE_EXE, up to 100
    /* 0x2 */ short  _2;
};
static_assert(sizeof(EVENT_STATE) == 0x4, "EVENT_STATE is 4 bytes");
static_assert(offsetof(EVENT_STATE, cnt) == 0x1, "EVENT_STATE.cnt");

// The event work (FF2 EV_WRK, bss 0x3e0098; save block 11 is all of it).
struct EV_WRK                           /* 0x1be0 */
{
    /* 0x0000 */ int         step;
    /* 0x0004 */ int         ev_no;
    /* 0x0008 */ EVENT_STATE ev_state[EV_EVENT_NUM];
    /* 0x1bdc */ u_char      wait_flg;  // 1 while an event holds the game (a handler's window is open)
};
static_assert(sizeof(EV_WRK) == 0x1be0, "EV_WRK is 0x1be0 bytes");
static_assert(offsetof(EV_WRK, ev_no) == 0x0004, "EV_WRK.ev_no");
static_assert(offsetof(EV_WRK, ev_state) == 0x0008, "EV_WRK.ev_state");
static_assert(offsetof(EV_WRK, wait_flg) == 0x1bdc, "EV_WRK.wait_flg");

extern EV_WRK ev_wrk;                   // bss 0x3e0098

void   EvWrkInit(void);                 // FF3 0x00199be0: EventInit
int    EvWrkSaveGet(void **addr);       // FF3 0x00199b18: {ev_wrk, 0x1be0} (save block 11)
void   SetEventState(int event_id, u_char state);  // FF3 0x00199b30: not out of EV_STATE_LOCK; EV_STATE_EXE counts a run
u_char GetEvState(int event_id);        // FF3 0x00199b88
s_char GetEvExeCount(int event_id);     // FF3 0x00199ba0: EVENT_STATE::cnt
void   SetEventWaitFlg(u_char flg);     // FF3 0x00199bb8
u_char GetEvWrkWaitFlg(void);           // FF3 0x00199bd0

// The dream-world player block (sdata gp-0x4c0, 8 bytes, save block 16):
// SET_DREAM_PLAYER writes word 0, GET_DREAM_PLAYER / PLYR_IS_SELECT read it;
// word 1 is set by SET_SP_EVENT 0 (MapSpEventSet, ingame/map/MapDBuff.c).
int   *GetEvDreamPlayerWrk(void);       // FF3 0x00199ad8
void   EvDreamPlayerInit(void);         // FF3 0x00199ae8: EventInit
int    EvDreamPlayerSaveGet(void **addr);   // FF3 0x00199b08: {the block, 8}

// ── parked state changes (FF2 ev_change.c) ────────────────────────────────

// One parked state change; change_id -1: a free row (FF2 EV_CHANGE_CTRL).
struct EV_CHANGE_CTRL                   /* 0xc */
{
    /* 0x0 */ int    set_event_id;      // the event that asked
    /* 0x4 */ int    change_id;         // the event to move
    /* 0x8 */ u_char change_state;      // EV_STATE
};
static_assert(sizeof(EV_CHANGE_CTRL) == 0xc, "EV_CHANGE_CTRL is 0xc bytes");
static_assert(offsetof(EV_CHANGE_CTRL, change_id) == 0x4, "EV_CHANGE_CTRL.change_id");
static_assert(offsetof(EV_CHANGE_CTRL, change_state) == 0x8, "EV_CHANGE_CTRL.change_state");

#define EV_CHANGE_CTRL_MAX  30          // bss 0x3e1c78..0x3e1de0 (save block 5)

void EvChangeCtrlInit(void);            // FF3 0x00199c98: EventInit
int  EvChangeCtrlSaveGet(void **addr);  // FF3 0x00199c80: {ev_change_ctrl, 0x168} (save block 5)
void CompulsionSetEventStateMain(void); // FF3 0x00199cd0: EventMain
void CompulsionSetEventState(int change_id, u_char state);  // FF3 0x00199d78
void Req_CompulsionSetEventState(int set_event_id, int change_id, u_char state);  // FF3 0x00199f78: EV_SET_STATE, the event timer
void Del_CompulsionSetEventState(int change_id);    // FF3 0x00199fd8

// ── the condition table (data 0x3242e8) ───────────────────────────────────

// A condition handler: (event id, the condition's bytes: +0 its label, then
// its arguments) -> 1 when true.
typedef int (*EV_COND_FUNC)(int event_id, u_char *addr);

// One row (FF2 EV_COND_WRK, grown): the script keyword and how many bytes the
// condition takes in an event program. Indexed by the label byte.
// layout: host -- func and fmt are pointers (EE +0x04 / +0x08), so they and
// the fields after them sit at other host offsets (EE offsets in the comments).
struct EV_COND_WRK                      /* 0x34 on the EE */
{
    /* 0x00 */ int          label;      // == the row
    /* 0x04 */ EV_COND_FUNC func;       // nullptr for EV_OR
    /* 0x08 */ const char  *fmt;        // argument format ("1124"), as the command table's
    /* 0x0c */ int          size;       // the condition's bytes: 4 or 8
    /* 0x10 */ int         *args;       // the decoded arguments while the row is judged (EventCtrlCenterMain / EventConditionJudge's stack); nullptr in the table
    /* 0x14 */ char         name[0x20]; // "PLYR_AREAIN"
};
static_assert(offsetof(EV_COND_WRK, label) == 0x00, "EV_COND_WRK.label");

#define EV_COND_NUM     72              // rows; label 0x30 (48) is EV_OR, the group separator
#define EV_COND_OR      0x30
#define EV_COND_END     0xff            // ends a condition stream

EV_COND_WRK *GetEvCondWrk(int label);   // FF3 0x00199a40
int  EvCondCallJudge(u_char label, int event_id, u_char *addr);    // FF3 0x00199a78: IF_ / ELSEIF_
int  EvCondGetSize(u_char label);       // FF3 0x00199ab8

// The conditions of this file (rows of the table above).
int  EvGetResearchStat(int event_id, u_char *addr);     // FF3 0x001992d0  GET_RESEARCH_STAT
int  EvGetDevelopStatus(int event_id, u_char *addr);    // FF3 0x00199338  GET_DEVELOP_STATUS_
int  EvPushPad(int event_id, u_char *addr);             // FF3 0x001993d8  PUSH_PAD
int  EvEvRandom(int event_id, u_char *addr);            // FF3 0x001994c0  EV_RANDOM
int  EvFinderMode(int event_id, u_char *addr);          // FF3 0x00199508  FINDER_MODE
int  EvObjIntoPlyrSight(int event_id, u_char *addr);    // FF3 0x001995c8  OBJ_INTO_PLYR_SIGHT
int  EvObjIntoFinder(int event_id, u_char *addr);       // FF3 0x001996a0  OBJ_INTO_FINDER
int  EvChapterLoadWait(int event_id, u_char *addr);     // FF3 0x001996d8  CHAPTER_LOAD_WAIT
int  EvSetEvPriority(int event_id, u_char *addr);       // FF3 0x001996e0  SET_EV_PRIORITY
int  EvPuzzleSuccess(int event_id, u_char *addr);       // FF3 0x00199720  PUZZLE_SUCCESS
int  EvCondNowSynchroMode(int event_id, u_char *addr);  // FF3 0x00199768  NOW_SYNCHRO_MODE
int  EvNotNowSynchroMode(int event_id, u_char *addr);   // FF3 0x00199770  NOT_NOW_SYNCHRO_MODE
int  EvCheckGameWorld(int event_id, u_char *addr);      // FF3 0x00199798  CHECK_GAME_WORLD
int  EvPeepingEnd(int event_id, u_char *addr);          // FF3 0x001997d0  PEEPING_END
int  EvTelStatus(int event_id, u_char *addr);           // FF3 0x001997f0  TEL_STATUS
int  EvTelTalkEnd(int event_id, u_char *addr);          // FF3 0x00199820  TEL_TALK_END
int  EvTapeCheck(int event_id, u_char *addr);           // FF3 0x00199840  TAPE_CHECK
int  EvTapeNotHave(int event_id, u_char *addr);         // FF3 0x00199870  TAPE_NOT_HAVE
int  EvCineMenuEnd(int event_id, u_char *addr);         // FF3 0x00199890  CINE_MENU_END
int  EvCineFilmEnd(int event_id, u_char *addr);         // FF3 0x001998b0  CINE_FILM_END
int  EvCineFlagCheck(int event_id, u_char *addr);       // FF3 0x001998d0  CINE_FLAG_CHECK
int  EvCineFilmNotHave(int event_id, u_char *addr);     // FF3 0x00199900  CINE_FILM_NOT_HAVE
int  EvStreamIsReady(int event_id, u_char *addr);       // FF3 0x00199920  EV_STREAM_ISREADY
int  EvCheckDayFlag(int event_id, u_char *addr);        // FF3 0x00199960  CHECK_DAY_FLAG
int  EvMissionCounterIsMore(int event_id, u_char *addr);    // FF3 0x001999a0  MISSION_COUNTER_IS_MORE

#endif /* _INGAME_EVENT_PRG_EV_CHANGE_H */
