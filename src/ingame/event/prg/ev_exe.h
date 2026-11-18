/* ==========================================================================
 *  ingame/event/prg/ev_exe.h
 *
 *  Event execution (FF3 0x19abe8..0x19b158; FF2 ev_exe.c, function for
 *  function): the 150 execution slots (ev_exe_ctrl, bss 0x3e31e8), one per
 *  running event program, which EventExe steps every frame; the status
 *  changes that put an event's init / main / end program on them; and FF3's
 *  save fixup, which turns the slots' program addresses into offsets around
 *  a memory-card copy.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_EXE_H
#define _INGAME_EVENT_PRG_EV_EXE_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"

// One running event program, as FF3's command handlers see it (FF2
// EV_EXE_CTRL, grown). The dispatcher 0x19b5b8 decodes the command's
// arguments by the handler's format string (first digit: bytes to skip, then
// one 1/2/4-byte signed argument per digit, see ingame/event/prg/ev_command.h)
// into a 0x80-byte int array on its stack and points `args` at it before the
// call. 0x18 bytes on the EE (the slots' stride; EventExeCtrlInit sets
// event_id, stop_timer, event_addr, process and if_state).
// layout: host -- event_addr and args are pointers (EE +0x08 / +0x0c), so the
// bytes after them sit at other host offsets (EE offsets in the comments).
struct EV_EXE_CTRL
{
    /* 0x00 */ int     event_id;    // the event's number (0..0x6f4)
    /* 0x04 */ u_int   stop_timer;  // EV_STOP's frames left
    /* 0x08 */ u_char *event_addr;  // the command in the event program
    /* 0x0c */ int    *args;        // decoded arguments (dispatcher's stack)
    /* 0x10 */ short   work;        // EV_SET_WORK / EV_ADD_WORK / EV_SUB_WORK; EV_WORK_CHECK compares it
    /* 0x12 */ u_char  process;     // a handler's step; 0 when the command starts
    /* 0x13 */ u_char  if_state;    // EV_IF_* (IF_ / ELSE_ / ELSEIF_ / ENDIF_)
    /* 0x14 */ u_char  cond_size;   // IF_ / ELSEIF_: the condition's argument bytes (EvCondGetSize)
};
static_assert(offsetof(EV_EXE_CTRL, event_id) == 0x00, "EV_EXE_CTRL.event_id");
static_assert(offsetof(EV_EXE_CTRL, stop_timer) == 0x04, "EV_EXE_CTRL.stop_timer");

// EV_EXE_CTRL.if_state: 0 outside an IF_ block; 1 the branch taken, 2 a
// branch skipped, 3 a branch already taken (the later ELSE_ / ELSEIF_ skip).
#define EV_IF_NONE      0
#define EV_IF_TRUE      1
#define EV_IF_FALSE     2
#define EV_IF_DONE      3

// ── the execution slots ───────────────────────────────────────────────────

#define EV_EXE_CTRL_MAX 150             // bss 0x3e31e8..0x3e3ff8 (save block 7)

int    EvExeCtrlSaveGet(void **addr);   // FF3 0x0019abe8: {ev_exe_ctrl, 0xe10 on the EE} (save block 7)
void   EventExeInit(void);              // FF3 0x0019ac78: every slot free, then EventMacroInit (EventInit)
void   SetEventInitStatus(int event_id);    // FF3 0x0019acc8: EV_STATE_INIT, the init program run at once, sub-events armed
void   SetEventExeStatus(int event_id);     // FF3 0x0019adb0: EV_STATE_EXE, the main program queued
void   SetEventEndExeStatus(int event_id);  // FF3 0x0019adf0: EV_STATE_END, the end program queued, sub-events parked
void   EventEnd_SubStateChange(int event_id);   // FF3 0x0019ae38: sub-events waiting to open are parked
void   SetEventExeCtrl(int event_id, u_char use_table);  // FF3 0x0019aee0: the program of table use_table into a free slot
EV_EXE_CTRL *EvExeGetCtrl(int event_id);    // FF3 0x0019af58: the event's slot, nullptr if none
void   EventExe(void);                  // FF3 0x0019af90: one step of every running program (EventMain)
void   EvExeRelease(int event_id);      // FF3 0x0019aff0: FF2 EventExeRelease: frees the event's slot, if any
int    CheckEventExeEntry(int event_id);    // FF3 0x0019b028: 1 when the event has a slot, else -1
void   EvExeCtrlFromSave(void);         // FF3 0x0019b128: slot offsets back to addresses (EventAfterLoad)
void   EvExeCtrlToSave(void);           // FF3 0x0019b140: slot addresses to offsets (EventEnd)

#endif /* _INGAME_EVENT_PRG_EV_EXE_H */
