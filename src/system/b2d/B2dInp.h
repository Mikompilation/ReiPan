/* ==========================================================================
 *  system/b2d/B2dInp.h
 *
 *  FF3's B2d input (file string "B2dInp.c", 0x1055d8..0x105998): per Bp
 *  handle 18 labels that pad events play (B2dInpUpdate). The layout's
 *  input commands fill them (B2dCmdInput), B2dFileRunCmds clears them.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DINP_H
#define _SYSTEM_B2D_B2DINP_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM_HOOK_FUNC, B2D_ENTRY_MAX

#define B2D_INP_LABELS  18              // 0..7 d-pad directions, 8..17 buttons (B2dInpUpdate's table)

// The input of one handle (0x54 bytes on the EE, 45 at 0x374868).
// layout: host -- the labels are pointers (EE +0x0c, host +0x10 on 64-bit);
// the words before them keep their EE offsets.
struct B2D_INP
{
    /* 0x00 */ int         state;       // < 0 rejects the handle; nothing writes it (bss 0)
    /* 0x04 */ int         btn_arg;     // a1 of the button hook (B2dInpSetBtnArg)
    /* 0x08 */ int         wait;        // a0 of the direction hook, counted down per update (B2dInpSetWait)
    /* 0x0c */ const char *label[B2D_INP_LABELS];
};
static_assert(offsetof(B2D_INP, state) == 0x00, "B2D_INP.state");
static_assert(offsetof(B2D_INP, btn_arg) == 0x04, "B2D_INP.btn_arg");
static_assert(offsetof(B2D_INP, wait) == 0x08, "B2D_INP.wait");

extern B2D_INP b2d_inp[B2D_ENTRY_MAX];  // 0x374868

B2D_INP     *B2dInpGetEntry(int handle);                // nullptr (and an error print) for a bad handle
const char **B2dInpGetLabels(int handle);               // the entry's label slots, nullptr for a bad handle
int          B2dInpSetLabel(int handle, u_int n, const char *label);   // event n (< 18) plays `label`; -1 bad n, -2 bad handle
void         B2dInpClearLabels(int handle);
void         B2dInpSetDirFunc(B2D_ITEM_HOOK_FUNC func); // pad hook: a0 the wait, returns the event bits (BpPadGetDir8)
void         B2dInpSetBtnArg(int handle, int arg);
void         B2dInpSetWait(int handle, int wait);       // B2dCmdLabel's argument 2
int          B2dInpGetWait(int handle);                 // 0 for a bad handle
void         B2dInpInit(void);                          // B2dSystemInit, BpCtlTerm
int          B2dInpUpdate(int handle);                  // plays the label of this frame's pad event; no caller in FF3 retail
void         B2dInpEnd(void);                           // empty, no caller

#endif /* _SYSTEM_B2D_B2DINP_H */
