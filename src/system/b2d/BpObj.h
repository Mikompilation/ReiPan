/* ==========================================================================
 *  system/b2d/BpObj.h
 *
 *  A layout played as one object (FF3 0x136df8..0x1374d8, FF3-only).
 *  Load requests and waits for the layout file, In plays a label and
 *  IsInEnd waits for it, Out plays another and waits; Release frees the
 *  layout. A class in FF3 (operator new + the constructor 0x136df8). Optional extras: a scroll bar
 *  (B2d_ScrollBar3, system/b2d/ScrollBar.c) and a list of item cursors
 *  (B2D_GRID_SPR by item id). Users: OutgameBg (outgame_bg.c), the save /
 *  load screens, the save point menu, the album screens, menu_cassette,
 *  alg_manage, the yes/no menu 0x1ed7f8.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_BPOBJ_H
#define _SYSTEM_B2D_BPOBJ_H

#include <stddef.h>                     // offsetof

#include "system/b2d/B2dGridSpr.h"      // B2D_GRID_SPR
#include "system/b2d/ScrollBar.h"       // B2d_ScrollBar3

// BP_OBJ.state.
enum
{
    BP_OBJ_NEW     = 0,                 // Load requests the file
    BP_OBJ_LOADING = 1,
    BP_OBJ_LOADED  = 2,                 // loaded, or played out
    BP_OBJ_IN      = 3,                 // playing the in label
    BP_OBJ_SHOWN   = 4,
    BP_OBJ_OUT     = 5,                 // playing the out label
};

// One item cursor of a BP_OBJ (EE 0xc bytes from the system heap).
// layout: host -- next and spr are pointers.
struct BP_OBJ_CSR
{
    /* 0x00 */ BP_OBJ_CSR   *next;
    /* 0x04 */ int           id;        // layout item id
    /* 0x08 */ B2D_GRID_SPR *spr;
};

// The items of a scroll bar (BP_OBJ::AddScrollBar; five ROM words).
struct BP_OBJ_SCROLL_DEF                /* 0x14 */
{
    /* 0x00 */ int frame;               // the track item (its h is the bar's length)
    /* 0x04 */ int top;                 // the knob's three pieces
    /* 0x08 */ int mid;
    /* 0x0c */ int bottom;
    /* 0x10 */ int origin;              // the track is placed relative to this item (B2d_ScrollBar3::SetOrigin)
};
static_assert(sizeof(BP_OBJ_SCROLL_DEF) == 0x14, "BP_OBJ_SCROLL_DEF is 0x14 bytes");

// The list state BP_OBJ::DrawScrollBar shows (four shorts).
struct BP_OBJ_SCROLL_POS                /* 0x8 */
{
    /* 0x00 */ short first;             // first row shown
    /* 0x02 */ short cursor;            // selected row
    /* 0x04 */ short rows;              // list rows
    /* 0x06 */ short page;              // rows shown at once
};
static_assert(sizeof(BP_OBJ_SCROLL_POS) == 0x8, "BP_OBJ_SCROLL_POS is 8 bytes");
static_assert(offsetof(BP_OBJ_SCROLL_POS, rows) == 0x4, "BP_OBJ_SCROLL_POS.rows");

// The scroll arrows of a BP_OBJ list (layout item ids; MenuScrollArrowDisp):
// each arrow is dimmed and frozen when the list cannot scroll that way, its
// effect item shown only when it can.
struct BP_OBJ_SCROLL_ARROWS             /* 0x10 */
{
    /* 0x00 */ int up;
    /* 0x04 */ int down;
    /* 0x08 */ int up_fx;
    /* 0x0c */ int down_fx;
};
static_assert(sizeof(BP_OBJ_SCROLL_ARROWS) == 0x10, "BP_OBJ_SCROLL_ARROWS is 0x10 bytes");

// EE 0x18 bytes (operator new'd by the users, then the constructor).
// layout: host -- bar and csr are pointers; the words before them keep
// their EE offsets.
struct BP_OBJ
{
    BP_OBJ(int file);                   // FF3 0x00136df8 (BpObjCtor)
    int  Load();                        // FF3 0x00136e18 (BpObjLoad): 1 when loaded
    int  In(const char *label);         // FF3 0x00136e98 (BpObjIn): 1 when it started
    int  IsInEnd();                     // FF3 0x00136ef8 (BpObjIsInEnd)
    int  Out(const char *label);        // FF3 0x00136f60 (BpObjOut): 1 when done
    void Release();                     // FF3 0x00137000 (BpObjRelease)
    void AddScrollBar(const BP_OBJ_SCROLL_DEF *def);                // FF3 0x00137020 (BpObjAddScrollBar)
    void DelScrollBar();                                            // FF3 0x00137090 (BpObjDelScrollBar)
    void DrawScrollBar(const BP_OBJ_SCROLL_POS *pos,
                       const BP_OBJ_SCROLL_ARROWS *arrows);         // FF3 0x001370d0 (BpObjDrawScrollBar)
    void AddCsr(int id, short dx, short dy, short yofs);            // FF3 0x001371b0 (BpObjAddCsr)
    void FreeCsr();                                                 // FF3 0x001372f8 (BpObjFreeCsr)
    void SetCsrPos(int id, short col, short row);                   // FF3 0x00137398 (BpObjSetCsrPos)
    void SetCsrDisp(int id, int disp);                              // FF3 0x001373f8 (BpObjSetCsrDisp)
    int  PlayOnce(const char *label);   // FF3 0x00137448 (BpObjPlayOnce): 1 once it stopped
    void PlayOnceReset();               // FF3 0x001374b8 (BpObjPlayOnceReset)
    int  GetHandle();                   // FF3 0x001374c0 (BpObjGetHandle)

    /* 0x00 */ int         file;        // layout file number
    /* 0x04 */ int         state;       // BP_OBJ_*
    /* 0x08 */ int         played;      // PlayOnce: the label was started
    /* 0x0c */ B2d_ScrollBar3 *bar;     // AddScrollBar
    /* 0x10 */ BP_OBJ_CSR *csr;         // AddCsr list
    /* 0x14 */ int         _014;        // cleared by the ctor; no reader seen
};
static_assert(offsetof(BP_OBJ, state) == 0x04, "BP_OBJ.state");
static_assert(offsetof(BP_OBJ, played) == 0x08, "BP_OBJ.played");

#endif /* _SYSTEM_B2D_BPOBJ_H */
