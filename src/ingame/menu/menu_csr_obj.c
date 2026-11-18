// ingame/menu/menu_csr_obj.c -- cursor objects (FF3-only, 0x20b148..0x20b590).
// See menu_csr_obj.h; the same operations as menu_csr.c's MenuCsrWork*.

#include "ingame/menu/menu_csr_obj.h"

#include "common/csr_ctrl.h"
#include "common/heapctrl.h"                // heapCtrlMalloc, heapCtrlFree
#include "ingame/sound/snd_ctrl.h"          // SndSePlay
#include "system/os/sys_heap.h"             // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// A fixed layout (MenuCsrGetTbl's table, not freed).
// FF3 0x0020b148

void MenuCsrObjInitTbl(MENU_CSR_WRK *wrk, int tbl_no, int cfg)
{
    MENU_CSR_TBL *tbl;

    tbl = MenuCsrGetTbl(tbl_no);
    CsrInit(&wrk->item, &wrk->vx, &wrk->vy, tbl->tbl, tbl->w, tbl->h, 1, (u_short)cfg);
}

// ──────────────────────────────────────────────────────────────────────
// A w x h cursor with item k in cell k, the table from the system heap.
// FF3 0x0020b1a8

void MenuCsrObjInit(MENU_CSR_WRK *wrk, int w, int h, int cfg)
{
    short *tbl;
    short *p;
    int    i;
    int    j;
    int    k;

    w   = (short)w;
    h   = (short)h;
    tbl = (short *)heapCtrlMalloc(GetSystemHeapWrkP(), w * h * 2, 0, 0, 0);
    p   = tbl;
    k   = 0;
    for (i = 0; i < w; i++)             // FF3's loop order: w runs of h (tbl[k] = k)
    {
        for (j = 0; j < h; j++)
            *p++ = (short)(j + k);
        k += h;
    }
    CsrInit(&wrk->item, &wrk->vx, &wrk->vy, tbl, (short)w, (short)h, 0, (u_short)cfg);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b290

void MenuCsrObjSetViewX(MENU_CSR_WRK *wrk, int size, int margin)
{
    CsrViewInitX(&wrk->item, &wrk->vx, (short)size, (short)margin);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b2c0

void MenuCsrObjSetViewY(MENU_CSR_WRK *wrk, int size, int margin)
{
    CsrViewInitY(&wrk->item, &wrk->vy, (short)size, (short)margin);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b2f0

void MenuCsrObjUpdate(MENU_CSR_WRK *wrk, int pad)
{
    CsrPadCtrl(&wrk->item, &wrk->vx, &wrk->vy, (u_char)pad);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b318

void MenuCsrObjRelease(MENU_CSR_WRK *wrk)
{
    CsrRelease(&wrk->item, &wrk->vx, &wrk->vy);
    if (wrk->item.ext != 0)
        return;
    heapCtrlFree(GetSystemHeapWrkP(), wrk->item.tbl, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Whether the cursor moved this frame; plays `se` then (none when se < 0).
// FF3 0x0020b378

int MenuCsrObjIsMovedSe(MENU_CSR_WRK *wrk, int se)
{
    int moved = 0;

    if (wrk->item.flags & CSR_EV_MOVED)
    {
        if (se >= 0)
            SndSePlay(se, 100, 0x1000, 0, nullptr, 0);
        moved = 1;
    }
    return moved;
}

// ──────────────────────────────────────────────────────────────────────
// Puts the cursor on item pos, then, per axis with pos_x / pos_y >= 0,
// sets the view's top first when top_x / top_y >= 0 and moves the cursor
// to that place in the view. Returns CsrSetId's result.
// FF3 0x0020b3c8

short MenuCsrObjSetPos(MENU_CSR_WRK *wrk, int pos, int pos_x, int pos_y, int top_x, int top_y)
{
    short ret;

    ret = CsrSetId(&wrk->item, &wrk->vx, &wrk->vy, (short)pos);
    if ((short)pos_x >= 0)
    {
        if ((short)top_x >= 0)
            wrk->vx.top = (short)top_x;
        CsrViewSetPosX(&wrk->item, &wrk->vx, (short)pos_x);
    }
    if ((short)pos_y >= 0)
    {
        if ((short)top_y >= 0)
            wrk->vy.top = (short)top_y;
        CsrViewSetPosY(&wrk->item, &wrk->vy, (short)pos_y);
    }
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b4a8

short MenuCsrObjGetPos(MENU_CSR_WRK *wrk)
{
    return wrk->item.id;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b4b0

short MenuCsrObjGetTopX(MENU_CSR_WRK *wrk)
{
    return CsrViewGetTop(&wrk->vx);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b4d0

short MenuCsrObjGetTopY(MENU_CSR_WRK *wrk)
{
    return CsrViewGetTop(&wrk->vy);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b4f0

short MenuCsrObjGetViewPosX(MENU_CSR_WRK *wrk)
{
    return CsrViewGetPos(&wrk->vx);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0020b510

short MenuCsrObjGetViewPosY(MENU_CSR_WRK *wrk)
{
    return CsrViewGetPos(&wrk->vy);
}

// ──────────────────────────────────────────────────────────────────────
// A page left / right / up / down.
// FF3 0x0020b530 / 0x0020b548 / 0x0020b560 / 0x0020b578

int MenuCsrObjPagePrevX(MENU_CSR_WRK *wrk)
{
    return CsrPagePrevX(&wrk->item, &wrk->vx);
}

// FF3 0x0020b548
int MenuCsrObjPageNextX(MENU_CSR_WRK *wrk)
{
    return CsrPageNextX(&wrk->item, &wrk->vx);
}

// FF3 0x0020b560
int MenuCsrObjPagePrevY(MENU_CSR_WRK *wrk)
{
    return CsrPagePrevY(&wrk->item, &wrk->vy);
}

// FF3 0x0020b578
int MenuCsrObjPageNextY(MENU_CSR_WRK *wrk)
{
    return CsrPageNextY(&wrk->item, &wrk->vy);
}
