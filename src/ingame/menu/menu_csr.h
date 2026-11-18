/* ==========================================================================
 *  ingame/menu/menu_csr.h
 *
 *  Menu cursors (FF3-only; 0x153460..0x153d68, between anim_2d.c and
 *  autoload.c): MenuCsr* take a cursor id and work on the per-id cursor work
 *  at bss 0x391b28 + id * 0x3c (MenuCsrGetWork); MenuCsrWork* (0x153908..)
 *  are the same operations on a work pointer. Under them is the grid cursor
 *  engine, common/csr_ctrl.h: a cursor is a w x h grid of item ids, and
 *  "pos" here is the item id under the cursor (MenuCsrSetPos/GetPos).
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_CSR_H
#define _INGAME_MENU_MENU_CSR_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "common/csr_ctrl.h"

// One cursor id's work (EE 0x3c bytes).
// layout: host -- item holds a pointer; the views keep their EE distance.
struct MENU_CSR_WRK
{
    /* 0x00 */ CSR_ITEM item;
    /* 0x1c */ CSR_VIEW vx;
    /* 0x2c */ CSR_VIEW vy;
};
static_assert(offsetof(MENU_CSR_WRK, item) == 0, "MENU_CSR_WRK.item");
static_assert(offsetof(MENU_CSR_WRK, vy) - offsetof(MENU_CSR_WRK, vx) == 0x2c - 0x1c, "MENU_CSR_WRK.vy");

// Cursor ids in use run up to 0x77; the array's end in bss is not settled
// (open-items.md), 128 covers every id seen.
#define MENU_CSR_MAX    128

extern MENU_CSR_WRK menu_csr_wrk[MENU_CSR_MAX];     // bss 0x391b28

// A fixed cursor layout (data 0x2f63f0, 0xc bytes each; one entry known).
// layout: host -- tbl is a pointer.
struct MENU_CSR_TBL
{
    /* 0x00 */ short *tbl;              // w * h item ids, -1 = empty cell
    /* 0x04 */ short  w;
    /* 0x06 */ short  h;
    /* 0x08 */ int    _08;
};
static_assert(offsetof(MENU_CSR_TBL, h) - offsetof(MENU_CSR_TBL, w) == 0x06 - 0x04, "MENU_CSR_TBL.h");

// A cursor whose items a callback enables: func(tbl, w, h) gets the w * h
// table filled with tbl[k] = k and writes -1 into the cells of disabled
// items (init_Title_Mode's difficulty cursor).
typedef void (*MENU_CSR_FILTER)(short *tbl, short w, short h);

MENU_CSR_WRK *MenuCsrGetWork(int id);               // FF3 0x00153460
void  MenuCsrInitTbl(int id, int tbl_no, int cfg);  // FF3 0x00153488: layout from MenuCsrGetTbl
void  MenuCsrInit(int id, int w, int h, int cfg);   // FF3 0x001534d0: cfg CSR_CFG_*
void  MenuCsrInitWithFilter(MENU_CSR_FILTER func, int id, int w, int h, int cfg);  // FF3 0x00153538
void  MenuCsrSetViewX(int id, int size, int margin);  // FF3 0x001535b0: x scroll view
void  MenuCsrSetViewY(int id, int size, int margin);  // FF3 0x00153600: y scroll view
void  MenuCsrUpdate(int id, int pad);               // FF3 0x00153650: once per frame
void  MenuCsrRelease(int id);                       // FF3 0x00153688: frees the item table
int   MenuCsrIsMovedSe(int id, int se);             // FF3 0x001536b0: moved this frame (plays se >= 0)
short MenuCsrSetPos(int id, int pos);               // FF3 0x001536e8: put the cursor on item pos
short MenuCsrGetPos(int id);                        // FF3 0x00153728: the item under the cursor
short MenuCsrGetTopX(int id);                       // FF3 0x00153750: x scroll top, -1 when off
short MenuCsrGetTopY(int id);                       // FF3 0x00153778: y scroll top, -1 when off
short MenuCsrGetPosX(int id);                       // FF3 0x001537a0: x position in the view, -1 when off
short MenuCsrGetPosY(int id);                       // FF3 0x001537c8: y position in the view, -1 when off
int   MenuCsrPagePrevX(int id);                     // FF3 0x001537f0
int   MenuCsrPageNextX(int id);                     // FF3 0x00153818
int   MenuCsrPagePrevY(int id);                     // FF3 0x00153840
int   MenuCsrPageNextY(int id);                     // FF3 0x00153868
short MenuCsrSetFirst(int id);                      // FF3 0x00153890: item 0
void  MenuCsrSetLast(int id);                       // FF3 0x001538b8: item h - 1, bottom of the view
int   MenuCsrIsWrapY(int id);                       // FF3 0x001538e0: the last move left the y range

void  MenuCsrWorkInitTbl(MENU_CSR_WRK *wrk, int tbl_no, int cfg);                   // FF3 0x00153908
void  MenuCsrWorkInitNoFilter(MENU_CSR_WRK *wrk, int w, int h, int cfg);            // FF3 0x00153960
void  MenuCsrWorkInit(MENU_CSR_FILTER func, MENU_CSR_WRK *wrk, int w, int h, int cfg);  // FF3 0x001539a0
void  MenuCsrWorkSetViewX(MENU_CSR_WRK *wrk, int size, int margin);  // FF3 0x00153a68
void  MenuCsrWorkSetViewY(MENU_CSR_WRK *wrk, int size, int margin);  // FF3 0x00153a98
void  MenuCsrWorkUpdate(MENU_CSR_WRK *wrk, int pad);                 // FF3 0x00153ac8
void  MenuCsrWorkRelease(MENU_CSR_WRK *wrk);                         // FF3 0x00153af0
int   MenuCsrWorkIsMovedSe(MENU_CSR_WRK *wrk, int se);               // FF3 0x00153b50
short MenuCsrWorkSetPos(MENU_CSR_WRK *wrk, int pos);                 // FF3 0x00153ba0
short MenuCsrWorkGetPos(MENU_CSR_WRK *wrk);                          // FF3 0x00153bd0
short MenuCsrWorkGetTopX(MENU_CSR_WRK *wrk);                         // FF3 0x00153bd8
short MenuCsrWorkGetTopY(MENU_CSR_WRK *wrk);                         // FF3 0x00153bf8
short MenuCsrWorkGetPosX(MENU_CSR_WRK *wrk);                         // FF3 0x00153c18
short MenuCsrWorkGetPosY(MENU_CSR_WRK *wrk);                         // FF3 0x00153c38
int   MenuCsrWorkPagePrevX(MENU_CSR_WRK *wrk);                       // FF3 0x00153c58
int   MenuCsrWorkPageNextX(MENU_CSR_WRK *wrk);                       // FF3 0x00153c70
int   MenuCsrWorkPagePrevY(MENU_CSR_WRK *wrk);                       // FF3 0x00153c88
int   MenuCsrWorkPageNextY(MENU_CSR_WRK *wrk);                       // FF3 0x00153ca0
short MenuCsrWorkSetFirst(MENU_CSR_WRK *wrk);                        // FF3 0x00153cb8
void  MenuCsrWorkSetLast(MENU_CSR_WRK *wrk);                         // FF3 0x00153cd8
int   MenuCsrWorkIsWrapY(MENU_CSR_WRK *wrk);                         // FF3 0x00153d38
MENU_CSR_TBL *MenuCsrGetTbl(int no);                                 // FF3 0x00153d48

#endif /* _INGAME_MENU_MENU_CSR_H */
