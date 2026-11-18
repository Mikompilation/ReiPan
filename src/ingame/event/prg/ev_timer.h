/* ==========================================================================
 *  ingame/event/prg/ev_timer.h
 *
 *  Event timers: FF3 0x19ca60..0x19cd28, FF2 ev_timer.c (MioPan's port is
 *  the base; FF3 dropped the debug prints and moved the save callback to
 *  the head of the file, in FF3's {addr, size} form).
 *
 *  One free-running frame counter plus 30 registration slots. A slot does
 *  not hold a countdown: EvTimerRegist converts the requested delay into an
 *  absolute value of the counter (wrapping at 400,000,000), and EvTimerExe
 *  fires the slot on the frame the counter is exactly equal to it. Firing
 *  posts Req_CompulsionSetEventState, so the target event changes state
 *  through ev_change.c's deferred queue.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_TIMER_H
#define _INGAME_EVENT_PRG_EV_TIMER_H

#include <stddef.h>

#include "sdk/scetypes.h"

// One registered timer. event_id -1 marks a free slot; (req_event_id,
// req_state) is the state change posted when the counter reaches timer.
typedef struct                          // 0x10
{
    /* 0x0 */ int    event_id;
    /* 0x4 */ u_int  timer;
    /* 0x8 */ int    req_event_id;
    /* 0xc */ u_char req_state;
} REGIST_TIMER;

#define REGIST_TIMER_MAX 30

// The counter and the slots are one save block (block 10, 0x1e4 bytes), so
// a pending timer survives a save and reload.
typedef struct                          // 0x1e4
{
    /* 0x000 */ u_int        event_timer;
    /* 0x004 */ REGIST_TIMER regist_timer[REGIST_TIMER_MAX];
} EV_TIMER_CTRL;

static_assert(sizeof(REGIST_TIMER) == 0x10, "REGIST_TIMER is 0x10 bytes");
static_assert(offsetof(REGIST_TIMER, timer) == 0x4, "REGIST_TIMER.timer");
static_assert(offsetof(REGIST_TIMER, req_event_id) == 0x8, "REGIST_TIMER.req_event_id");
static_assert(offsetof(REGIST_TIMER, req_state) == 0xc, "REGIST_TIMER.req_state");
static_assert(sizeof(EV_TIMER_CTRL) == 0x1e4, "EV_TIMER_CTRL is 0x1e4 bytes (save block 10)");
static_assert(offsetof(EV_TIMER_CTRL, regist_timer) == 0x4, "EV_TIMER_CTRL.regist_timer");

int  EvTimerCtrlSaveGet(void **addr);   // FF3 0x0019ca60: save block 10 {0x3e65e0, 0x1e4}
void EvTimerCtrlInit(void);             // FF3 0x0019ca78: EventInit
void EvTimerMain(void);                 // FF3 0x0019cae0: EventMain, every in-game frame

// Post state to req_event_id once timer frames of play have passed (the
// event command EvEvSetTimer). Nothing when all 30 slots are taken.
void EvTimerRegist(int event_id, u_int timer, int req_event_id, u_char state);  // FF3 0x0019cbc0

// Drop every timer event_id registered. No caller in FF3.
void EvTimerRelease(int event_id);      // FF3 0x0019ccb0

#endif /* _INGAME_EVENT_PRG_EV_TIMER_H */
