/* ==========================================================================
 *  ingame/event/prg/ev_timer.c
 *
 *  Event timers: FF3 0x19ca60..0x19cd28, FF2 ev_timer.c (MioPan's port is
 *  the base; see ev_timer.h).
 *
 *  The counter is frozen while an event holds the game (GetEvWrkWaitFlg),
 *  so a delay is measured in frames of play; expiry is still checked every
 *  frame. A zero-frame delay never matches: the counter has moved past the
 *  registered value by the next check, and the slot waits for the wrap
 *  (ROM behaviour, kept).
 * ======================================================================== */
#include "ingame/event/prg/ev_timer.h"

#include "ingame/event/prg/ev_change.h"         // GetEvWrkWaitFlg, Req_CompulsionSetEventState

// The counter wraps here: the test is against EV_TIMER_LOOP - 1 and the
// fold-back subtracts EV_TIMER_LOOP (the ROM adds 0xe8287c00).
#define EV_TIMER_LOOP 400000000

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static EV_TIMER_CTRL ev_timer_ctrl;     // bss 0x3e65e0

static void  RegistTimerInit(REGIST_TIMER *timer);
static void  EvTimerCount(void);
static void  EvTimerExe(void);
static int   GetEmptyRegistTimer(void);
static u_int CalTimerFill(u_int timer);

// ──────────────────────────────────────────────────────────────────────
// Save block 10. FF3 0x0019ca60.

int EvTimerCtrlSaveGet(void **addr)
{
    *addr = &ev_timer_ctrl;
    return sizeof(ev_timer_ctrl);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019ca78

void EvTimerCtrlInit(void)
{
    int i;

    ev_timer_ctrl.event_timer = 0;
    for (i = 0; i < REGIST_TIMER_MAX; i++)
    {
        RegistTimerInit(&ev_timer_ctrl.regist_timer[i]);
    }
}

// FF3 0x0019cac8

static void RegistTimerInit(REGIST_TIMER *timer)
{
    timer->req_state    = 0;
    timer->req_event_id = -1;
    timer->event_id     = -1;
    timer->timer        = 0;
}

// FF3 0x0019cae0

void EvTimerMain(void)
{
    if (GetEvWrkWaitFlg() == 0)
    {
        EvTimerCount();
    }

    EvTimerExe();
}

// FF3 0x0019cb10

static void EvTimerCount(void)
{
    ev_timer_ctrl.event_timer++;

    if (ev_timer_ctrl.event_timer > EV_TIMER_LOOP - 1)
    {
        ev_timer_ctrl.event_timer = 0;
    }
}

// Fire every slot whose deadline is this exact tick. FF3 0x0019cb38.

static void EvTimerExe(void)
{
    int i;

    for (i = 0; i < REGIST_TIMER_MAX; i++)
    {
        REGIST_TIMER *timer = &ev_timer_ctrl.regist_timer[i];

        if (timer->event_id != -1 && timer->timer == ev_timer_ctrl.event_timer)
        {
            Req_CompulsionSetEventState(timer->event_id, timer->req_event_id, timer->req_state);
            RegistTimerInit(timer);
        }
    }
}

// FF3 0x0019cbc0

void EvTimerRegist(int event_id, u_int timer, int req_event_id, u_char state)
{
    int empty_area;

    empty_area = GetEmptyRegistTimer();
    if (empty_area == -1)
    {
        return;
    }

    ev_timer_ctrl.regist_timer[empty_area].event_id     = event_id;
    ev_timer_ctrl.regist_timer[empty_area].timer        = CalTimerFill(timer);
    ev_timer_ctrl.regist_timer[empty_area].req_state    = state;
    ev_timer_ctrl.regist_timer[empty_area].req_event_id = req_event_id;
}

// First free slot, or -1 when all 30 are taken. FF3 0x0019cc38.

static int GetEmptyRegistTimer(void)
{
    int i;

    for (i = 0; i < REGIST_TIMER_MAX; i++)
    {
        if (ev_timer_ctrl.regist_timer[i].event_id == -1)
        {
            return i;
        }
    }

    return -1;
}

// Delay -> absolute deadline, folded back onto the counter's modulus.
// FF3 0x0019cc80.

static u_int CalTimerFill(u_int timer)
{
    u_int fill_time;

    fill_time = ev_timer_ctrl.event_timer + timer;
    if (fill_time > EV_TIMER_LOOP - 1)
    {
        fill_time -= EV_TIMER_LOOP;
    }

    return fill_time;
}

// FF3 0x0019ccb0

void EvTimerRelease(int event_id)
{
    int i;

    for (i = 0; i < REGIST_TIMER_MAX; i++)
    {
        if (ev_timer_ctrl.regist_timer[i].event_id == event_id)
        {
            RegistTimerInit(&ev_timer_ctrl.regist_timer[i]);
        }
    }
}
