/* ==========================================================================
 *  ingame/event/prg/ev_macro.h
 *
 *  Event macros: FF3 0x19b4a8..0x19b768 (FF2 ev_macro.c's functions): the
 *  macro-state reset, the load restore, and the interpreter that steps a
 *  running event program command by command through the command table
 *  (data 0x3556b8). FF3 moved the handlers themselves to ev_command.c. (The
 *  code once inferred under this name, 0x19a0a0..0x19ac18, is ev_chapter.c's
 *  and ev_exe.c's.)
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_MACRO_H
#define _INGAME_EVENT_PRG_EV_MACRO_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct EV_EXE_CTRL;                     // ingame/event/prg/ev_exe.h

// ── the command table (data 0x3556b8) ─────────────────────────────────────

// A command handler: 1 when the command is done (the program steps over it),
// 0 to be called again next frame.
typedef int (*EV_EXE_FUNC)(EV_EXE_CTRL *ctrl);

// One row (FF2 EV_EXE_WRK, grown by the format string): indexed by the
// command's opcode byte. size is the command's bytes in the program, opcode
// included; IF_ .. ENDIF_ add their condition's bytes (EV_EXE_CTRL::cond_size).
// layout: host -- func and fmt are pointers (EE +0x04 / +0x0c), so they and
// the fields after them sit at other host offsets (EE offsets in the comments).
struct EV_EXE_WRK                       /* 0x30 on the EE */
{
    /* 0x00 */ int          label;      // == the row
    /* 0x04 */ EV_EXE_FUNC  func;
    /* 0x08 */ int          size;       // the command's bytes: 4, 8 or 12
    /* 0x0c */ const char  *fmt;        // argument format ("1124", ev_command.h)
    /* 0x10 */ char         name[0x20]; // "PLYR_LOCK"
};
static_assert(offsetof(EV_EXE_WRK, label) == 0x00, "EV_EXE_WRK.label");

#define EV_EXE_WRK_NUM  235             // rows; opcodes 0xeb..0xfe are unused
#define EV_COM_IF       0x93            // IF_ .. ENDIF_ (0x93..0x96) run even in a skipped branch
#define EV_COM_ENDIF    0x96
#define EV_COM_END      0xff            // ends a program; not a row (Event_EvEnd)

void EventMacroInit(void);              // FF3 0x0019b4a8: every macro control back to its start (EventExeInit)
void EventLoadRestore(void);            // FF3 0x0019b4f0: the saved dither and screen effect again (IngameLoadRestore)
void EventExeFuncCall(EV_EXE_CTRL *ctrl);   // FF3 0x0019b618: one step of a running program (EventExe, SetEventInitStatus)

// PC: the command table's row for opcode com (nullptr past the table), for
// the Debug menu's event browser (reipan/debug/reipan_events.cpp). Not in FF3.
const EV_EXE_WRK *EvMacroGetWrk(int com);

#endif /* _INGAME_EVENT_PRG_EV_MACRO_H */
