/* ==========================================================================
 *  ingame/event/prg/ev_open.h
 *
 *  The event control centre: FF3 0x19baf8..0x19c658, FF2 ev_open.c.
 *
 *  Every event has an open and a close condition stream in the event file
 *  (EvGetExeAddr tables 0 / 3). A stream is a list of conditions (a label
 *  byte, the row of ev_change.c's condition table, then its arguments), EV_COND_OR
 *  (0x30, 4 bytes) separates alternative groups and EV_COND_END (0xff) ends
 *  it: conditions of a group AND, groups OR. EventSetOpenCondition /
 *  EventSetCloseCondition post the first condition of each group into
 *  ev_ctrl_center; EventCtrlCenterMain tests every posted row once a frame
 *  and, when it passes, judges the rest of its group (EventConditionJudge).
 *  Events whose open condition passed in the same frame start only if they
 *  claimed the highest priority any of them claimed (SET_EV_PRIORITY,
 *  PLYR_AREAIN / AREAOUT), or none.
 *
 *  FF3 moved the condition table and most condition tests to ev_change.c;
 *  this file keeps FF2's photo conditions. The posted rows' stream addresses
 *  become offsets around a memory-card copy (EvCtrlCenterToSave / FromSave).
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_OPEN_H
#define _INGAME_EVENT_PRG_EV_OPEN_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One posted condition (FF2 EV_CTRL_CENTER); condition -1: a free row.
// layout: host -- exe_addr is a pointer (8 bytes on the EE, 16 on the host).
struct EV_CTRL_CENTER
{
    /* 0x0 */ short   condition;        // label byte: the row of the condition table
    /* 0x2 */ short   event_id;         // -1 in a free row
    /* 0x4 */ u_char *exe_addr;         // the condition in the event's stream
};
static_assert(offsetof(EV_CTRL_CENTER, event_id) == 0x2, "EV_CTRL_CENTER.event_id");

#define EV_CTRL_CENTER_MAX  450         // bss 0x3e4008..0x3e4e18 (save block 8; FF2 250)

// A priority an event claimed this frame (FF2 EV_PRI_CTRL).
struct EV_PRI_CTRL                      /* 0x8 */
{
    /* 0x0 */ int    event_id;          // -1 unused
    /* 0x4 */ u_char priority;
};
static_assert(sizeof(EV_PRI_CTRL) == 0x8, "EV_PRI_CTRL is 8 bytes");
static_assert(offsetof(EV_PRI_CTRL, priority) == 0x4, "EV_PRI_CTRL.priority");

#define EV_PRI_CTRL_MAX     450

// The claims and their count, saved as one block (bss 0x3e4e18, save block 50).
struct EV_PRI_WRK                       /* 0xe14 */
{
    /* 0x000 */ int         num;        // FF2 ev_pri_num: rows used this frame
    /* 0x004 */ EV_PRI_CTRL ctrl[EV_PRI_CTRL_MAX];
};
static_assert(sizeof(EV_PRI_WRK) == 0xe14, "EV_PRI_WRK is 0xe14 bytes");
static_assert(offsetof(EV_PRI_WRK, ctrl) == 0x4, "EV_PRI_WRK.ctrl");

// The two halves' master switches (FF2 EV_COND_CTRL, sdata 0x373080).
struct EV_COND_CTRL                     /* 0x2 */
{
    /* 0x0 */ u_char open_switch;       // 0: no open condition is judged
    /* 0x1 */ u_char end_switch;        // 0: no close condition is judged
};
static_assert(sizeof(EV_COND_CTRL) == 0x2, "EV_COND_CTRL is 2 bytes");
static_assert(offsetof(EV_COND_CTRL, end_switch) == 0x1, "EV_COND_CTRL.end_switch");

int  EvCtrlCenterSaveGet(void **addr);  // FF3 0x0019baf8: {ev_ctrl_center, 0xe10 on the EE} (save block 8)
int  EvPriCtrlSaveGet(void **addr);     // FF3 0x0019bb10: {ev_pri_wrk, 0xe14} (save block 50)
void EvCtrlCenterInit(void);            // FF3 0x0019bb48: every ev_ctrl_center row free (EventInit)
void EvCondCtrlInit(void);              // FF3 0x0019bb98: both switches on (EventInit)
void SetEventPriority(int event_id, u_char priority);   // FF3 0x0019bbe8: PLYR_AREAIN / AREAOUT, SET_EV_PRIORITY
void EventSetOpenCondition(int event_id);   // FF3 0x0019bc68: the open conditions posted, parked sub-events re-armed
void EventSetCloseCondition(int event_id);  // FF3 0x0019bd08: EV_STATE_WAIT_END, the close conditions posted (Event_EvEnd)
void EventDelCondition(int event_id);       // FF3 0x0019bda8: drops the event's posted conditions
void EventCtrlCenterMain(void);         // FF3 0x0019bf48: judges the posted conditions (EventMain)
void SetOpenCondSwitch(u_char flg);     // FF3 0x0019c380: SET_CONDITION_CHECK 0; PlyrDeathCtrl 1 on the game over
u_char GetOpenCondSwitch(void);         // FF3 0x0019c390 (unreferenced)
void SetEndCondSwitch(u_char flg);      // FF3 0x0019c398: SET_CONDITION_CHECK non-0
void EvCtrlCenterToSave(void);          // FF3 0x0019c450: stream addresses to offsets (EventEnd)
void EvCtrlCenterFromSave(void);        // FF3 0x0019c468: offsets back to addresses (EventAfterLoad)
const char *EvCtrlCenterGetCondName(u_int no, int *event_id);  // FF3 0x0019c480: row no's keyword and event (unreferenced)

// The photo conditions (rows of ev_change.c's condition table).
int  EvPhotoIsDev(int event_id, u_char *addr);      // FF3 0x0019c508  PHOTO_IS_DEV
int  EvPhotoState(int event_id, u_char *addr);      // FF3 0x0019c550  PHOTO_STATE_
int  EvCondPhotoObj(int event_id, u_char *addr);    // FF3 0x0019c5b0  PHOTO_OBJ
int  EvNotPhotoObj(int event_id, u_char *addr);     // FF3 0x0019c630  NOT_PHOTO_OBJ

#endif /* _INGAME_EVENT_PRG_EV_OPEN_H */
