/* ==========================================================================
 *  common/csr_ctrl.h
 *
 *  The grid cursor engine (FF3-only, 0x1edd90..0x1eefe8, after the yes/no
 *  menu module and before heapctrl.c). A cursor is an item grid (CSR_ITEM:
 *  a w x h table of item ids, -1 = no item there) moved by the d-pad with
 *  key repeat, plus two scroll views (CSR_VIEW) that follow it on x and y.
 *  ingame/menu/menu_csr.c keeps one of each per cursor id (MENU_CSR_WRK).
 * ======================================================================== */
#ifndef _COMMON_CSR_CTRL_H
#define _COMMON_CSR_CTRL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// CSR_ITEM.flags, low 4 bits: the configuration given to CsrItemInit.
#define CSR_CFG_CLAMP_Y     0x0001      // stop at the first / last row (else wrap)
#define CSR_CFG_CLAMP_X     0x0002      // stop at the first / last column (else wrap)
#define CSR_CFG_L1R1        0x0004      // L1 / R1 report CSR_EV_L1 / CSR_EV_R1
#define CSR_CFG_L2R2        0x0008      // L2 / R2 report CSR_EV_L2 / CSR_EV_R2
#define CSR_CFG_MASK        0x000f

// CSR_ITEM.flags, high bits: this frame's events (cleared by CsrItemPadCtrl).
#define CSR_EV_RIGHT        0x0010      // x + 1
#define CSR_EV_LEFT         0x0020      // x - 1
#define CSR_EV_UP           0x0040      // y - 1
#define CSR_EV_DOWN         0x0080      // y + 1
#define CSR_EV_L1           0x0100
#define CSR_EV_L2           0x0200
#define CSR_EV_R1           0x0400
#define CSR_EV_R2           0x0800
#define CSR_EV_MOVED        0x1000      // the cursor stands on another cell
#define CSR_EV_EDGE_Y       0x2000      // the step left the y range (wrapped or clamped)
#define CSR_EV_EDGE_X       0x4000      // the step left the x range
#define CSR_EV_PRESS        0x8000      // a button was newly pressed this frame

// The item grid (EE 0x1c bytes).
// layout: host -- tbl is a pointer (4 bytes on the EE); the fields after it
// keep their EE distances from w (asserted below).
struct CSR_ITEM
{
    /* 0x00 */ short  *tbl;             // w * h item ids, row-major; -1 = empty cell
    /* 0x04 */ short   w;               // columns
    /* 0x06 */ short   h;               // rows
    /* 0x08 */ short   id;              // tbl[x + y * w]: the item under the cursor
    /* 0x0a */ short   x;
    /* 0x0c */ short   y;
    /* 0x0e */ short   rpt;             // frames to the next repeat step (10 first, then 5)
    /* 0x10 */ u_short held[2];         // per pad: last frame's buttons (masked)
    /* 0x14 */ int     ext;             // tbl is the caller's: not filled, not freed
    /* 0x18 */ u_short flags;           // CSR_CFG_* | CSR_EV_*
};
static_assert(offsetof(CSR_ITEM, w) == sizeof(short *), "CSR_ITEM.w follows tbl");
static_assert(offsetof(CSR_ITEM, h)     - offsetof(CSR_ITEM, w) == 0x06 - 0x04, "CSR_ITEM.h");
static_assert(offsetof(CSR_ITEM, id)    - offsetof(CSR_ITEM, w) == 0x08 - 0x04, "CSR_ITEM.id");
static_assert(offsetof(CSR_ITEM, x)     - offsetof(CSR_ITEM, w) == 0x0a - 0x04, "CSR_ITEM.x");
static_assert(offsetof(CSR_ITEM, y)     - offsetof(CSR_ITEM, w) == 0x0c - 0x04, "CSR_ITEM.y");
static_assert(offsetof(CSR_ITEM, rpt)   - offsetof(CSR_ITEM, w) == 0x0e - 0x04, "CSR_ITEM.rpt");
static_assert(offsetof(CSR_ITEM, held)  - offsetof(CSR_ITEM, w) == 0x10 - 0x04, "CSR_ITEM.held");
static_assert(offsetof(CSR_ITEM, ext)   - offsetof(CSR_ITEM, w) == 0x14 - 0x04, "CSR_ITEM.ext");
static_assert(offsetof(CSR_ITEM, flags) - offsetof(CSR_ITEM, w) == 0x18 - 0x04, "CSR_ITEM.flags");

// One scroll view along x or y (0x10 bytes): which part of the w (h) cells
// shows, and where the cursor sits in it.
struct CSR_VIEW
{
    /* 0x00 */ short size;              // cells visible
    /* 0x02 */ short margin;            // cells kept before the edge when scrolling back
    /* 0x04 */ short lim;               // size - margin - 1: scrolling on starts past it
    /* 0x06 */ short pos;               // cursor cell within the view
    /* 0x08 */ short top;               // first visible cell
    /* 0x0a */ short _0a;
    /* 0x0c */ int   on;                // 0: no view (CsrViewReset)
};
static_assert(sizeof(CSR_VIEW) == 0x10, "CSR_VIEW is 0x10 bytes on the PS2");
static_assert(offsetof(CSR_VIEW, margin) == 0x02, "CSR_VIEW.margin");
static_assert(offsetof(CSR_VIEW, lim) == 0x04, "CSR_VIEW.lim");
static_assert(offsetof(CSR_VIEW, pos) == 0x06, "CSR_VIEW.pos");
static_assert(offsetof(CSR_VIEW, top) == 0x08, "CSR_VIEW.top");
static_assert(offsetof(CSR_VIEW, on) == 0x0c, "CSR_VIEW.on");

// A cursor: grid + views.
void  CsrInit(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short *tbl, short w, short h, int ext, u_short cfg);  // FF3 0x001edd90
void  CsrPadCtrl(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, u_char pad);   // FF3 0x001eddf8: once per frame
void  CsrRelease(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy);               // FF3 0x001ede50
short CsrSetId(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short id);       // FF3 0x001ede88
short CsrSetXY(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short x, short y);  // FF3 0x001edf00

// The grid.
void  CsrItemInit(CSR_ITEM *item, short *tbl, short w, short h, int ext, u_short cfg);  // FF3 0x001edf88
void  CsrItemPadCtrl(CSR_ITEM *item, int pad);                  // FF3 0x001ee060
void  CsrItemRelease(CSR_ITEM *item);                           // FF3 0x001ee478 (empty)
short CsrItemSetId(CSR_ITEM *item, short id);                   // FF3 0x001ee480: -1 when absent
void  CsrItemSkipOff(CSR_ITEM *item);                           // FF3 0x001ee4d8
int   CsrItemFind(CSR_ITEM *item, short id);                    // FF3 0x001ee5c8: x << 16 | y, -1
short CsrItemSetXY(CSR_ITEM *item, short x, short y);           // FF3 0x001ee638: -1 when out of range

// The views.
void  CsrViewReset(CSR_VIEW *view);                             // FF3 0x001ee6c0
void  CsrViewInit(CSR_VIEW *view, short size, short margin, short total);  // FF3 0x001ee6e0
void  CsrViewInitX(CSR_ITEM *item, CSR_VIEW *vx, short size, short margin);  // FF3 0x001ee760
void  CsrViewInitY(CSR_ITEM *item, CSR_VIEW *vy, short size, short margin);  // FF3 0x001ee798
void  CsrViewJumpFirst(CSR_VIEW *view, short pos);              // FF3 0x001ee7d0
void  CsrViewJumpLast(CSR_VIEW *view, short pos, short total);  // FF3 0x001ee7f0
void  CsrViewDec(CSR_VIEW *view);                               // FF3 0x001ee828
void  CsrViewInc(CSR_VIEW *view, short total);                  // FF3 0x001ee888
void  CsrViewUpdate(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy);  // FF3 0x001ee910
void  CsrViewSetTopX(CSR_ITEM *item, CSR_VIEW *vx, short top);  // FF3 0x001eea30
void  CsrViewSetTopY(CSR_ITEM *item, CSR_VIEW *vy, short top);  // FF3 0x001eea80
void  CsrViewSetPosX(CSR_ITEM *item, CSR_VIEW *vx, short pos);  // FF3 0x001eead0
void  CsrViewSetPosY(CSR_ITEM *item, CSR_VIEW *vy, short pos);  // FF3 0x001eeb20
short CsrViewGetTop(CSR_VIEW *view);                            // FF3 0x001eeb70: -1 when off
short CsrViewGetPos(CSR_VIEW *view);                            // FF3 0x001eeb88: -1 when off

// Page steps (one view size); each returns whether the cursor ended on the
// first (Prev) / last (Next) cell.
int   CsrPagePrevX(CSR_ITEM *item, CSR_VIEW *vx);               // FF3 0x001eeba0
int   CsrPageNextX(CSR_ITEM *item, CSR_VIEW *vx);               // FF3 0x001eec90
int   CsrPagePrevY(CSR_ITEM *item, CSR_VIEW *vy);               // FF3 0x001eedc0
int   CsrPageNextY(CSR_ITEM *item, CSR_VIEW *vy);               // FF3 0x001eeeb0

#endif /* _COMMON_CSR_CTRL_H */
