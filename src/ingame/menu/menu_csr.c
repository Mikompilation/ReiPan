/* ==========================================================================
 *  ingame/menu/menu_csr.c
 *
 *  Menu cursors by id (see menu_csr.h). FF3-only, 0x153460..0x153d68: the
 *  id wrappers, then the same operations on a work over the grid cursor
 *  engine (common/csr_ctrl.c).
 * ======================================================================== */
#include "ingame/menu/menu_csr.h"

#include "common/heapctrl.h"            // heapCtrlMalloc / heapCtrlFree
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "ingame/sound/snd_ctrl.h"      // SndSePlay

MENU_CSR_WRK menu_csr_wrk[MENU_CSR_MAX];

// Cursor layout 0: two columns of four rows; item 2 fills two rows of the
// first column, item 3 has no cell. FF3 data 0x2f63e0.
static short menu_csr_tbl0[] = {
    0, 4,
    1, 5,
    2, 6,
    2, 7,
};

// FF3 data 0x2f63f0
static MENU_CSR_TBL menu_csr_tbl[] = {
    { menu_csr_tbl0, 2, 4, 0 },
};

// FF3 0x00153460
MENU_CSR_WRK *MenuCsrGetWork(int id)
{
    return &menu_csr_wrk[(short)id];
}

// FF3 0x00153488
void MenuCsrInitTbl(int id, int tbl_no, int cfg)
{
    MenuCsrWorkInitTbl(MenuCsrGetWork(id), tbl_no, cfg);
}

// FF3 0x001534d0
void MenuCsrInit(int id, int w, int h, int cfg)
{
    MenuCsrWorkInit(nullptr, MenuCsrGetWork(id), w, h, cfg);
}

// FF3 0x00153538
void MenuCsrInitWithFilter(MENU_CSR_FILTER func, int id, int w, int h, int cfg)
{
    MenuCsrWorkInit(func, MenuCsrGetWork(id), w, h, cfg);
}

// FF3 0x001535b0
void MenuCsrSetViewX(int id, int size, int margin)
{
    MenuCsrWorkSetViewX(MenuCsrGetWork(id), size, margin);
}

// FF3 0x00153600
void MenuCsrSetViewY(int id, int size, int margin)
{
    MenuCsrWorkSetViewY(MenuCsrGetWork(id), size, margin);
}

// FF3 0x00153650
void MenuCsrUpdate(int id, int pad)
{
    MenuCsrWorkUpdate(MenuCsrGetWork(id), pad);
}

// FF3 0x00153688
void MenuCsrRelease(int id)
{
    MenuCsrWorkRelease(MenuCsrGetWork(id));
}

// FF3 0x001536b0
int MenuCsrIsMovedSe(int id, int se)
{
    return MenuCsrWorkIsMovedSe(MenuCsrGetWork(id), se);
}

// FF3 0x001536e8
short MenuCsrSetPos(int id, int pos)
{
    return MenuCsrWorkSetPos(MenuCsrGetWork(id), pos);
}

// FF3 0x00153728
short MenuCsrGetPos(int id)
{
    return MenuCsrWorkGetPos(MenuCsrGetWork(id));
}

// FF3 0x00153750
short MenuCsrGetTopX(int id)
{
    return MenuCsrWorkGetTopX(MenuCsrGetWork(id));
}

// FF3 0x00153778
short MenuCsrGetTopY(int id)
{
    return MenuCsrWorkGetTopY(MenuCsrGetWork(id));
}

// FF3 0x001537a0
short MenuCsrGetPosX(int id)
{
    return MenuCsrWorkGetPosX(MenuCsrGetWork(id));
}

// FF3 0x001537c8
short MenuCsrGetPosY(int id)
{
    return MenuCsrWorkGetPosY(MenuCsrGetWork(id));
}

// FF3 0x001537f0
int MenuCsrPagePrevX(int id)
{
    return MenuCsrWorkPagePrevX(MenuCsrGetWork(id));
}

// FF3 0x00153818
int MenuCsrPageNextX(int id)
{
    return MenuCsrWorkPageNextX(MenuCsrGetWork(id));
}

// FF3 0x00153840
int MenuCsrPagePrevY(int id)
{
    return MenuCsrWorkPagePrevY(MenuCsrGetWork(id));
}

// FF3 0x00153868
int MenuCsrPageNextY(int id)
{
    return MenuCsrWorkPageNextY(MenuCsrGetWork(id));
}

// FF3 0x00153890
short MenuCsrSetFirst(int id)
{
    return MenuCsrWorkSetFirst(MenuCsrGetWork(id));
}

// FF3 0x001538b8
void MenuCsrSetLast(int id)
{
    MenuCsrWorkSetLast(MenuCsrGetWork(id));
}

// FF3 0x001538e0
int MenuCsrIsWrapY(int id)
{
    return MenuCsrWorkIsWrapY(MenuCsrGetWork(id));
}

// ──────────────────────────────────────────────────────────────────────
// A cursor over fixed layout `tbl_no` (the table is not copied or freed).
// FF3 0x00153908

void MenuCsrWorkInitTbl(MENU_CSR_WRK *wrk, int tbl_no, int cfg)
{
    MENU_CSR_TBL *tbl;

    tbl = MenuCsrGetTbl(tbl_no);
    CsrInit(&wrk->item, &wrk->vx, &wrk->vy, tbl->tbl, tbl->w, tbl->h, 1, (short)cfg);
}

// FF3 0x00153960
void MenuCsrWorkInitNoFilter(MENU_CSR_WRK *wrk, int w, int h, int cfg)
{
    MenuCsrWorkInit(nullptr, wrk, (short)w, (short)h, (short)cfg);
}

// ──────────────────────────────────────────────────────────────────────
// A w x h cursor with item k in cell k, the table from the system heap;
// func, when given, then knocks out disabled items.
// FF3 0x001539a0

void MenuCsrWorkInit(MENU_CSR_FILTER func, MENU_CSR_WRK *wrk, int w, int h, int cfg)
{
    short *tbl;

    w = (short)w;
    h = (short)h;
    tbl = (short *)heapCtrlMalloc(GetSystemHeapWrkP(), w * h * 2, 0, 0, 0);
    CsrInit(&wrk->item, &wrk->vx, &wrk->vy, tbl, w, h, 0, (short)cfg);
    if (func != nullptr)
        func(tbl, w, h);
}

// FF3 0x00153a68
void MenuCsrWorkSetViewX(MENU_CSR_WRK *wrk, int size, int margin)
{
    CsrViewInitX(&wrk->item, &wrk->vx, size, margin);
}

// FF3 0x00153a98
void MenuCsrWorkSetViewY(MENU_CSR_WRK *wrk, int size, int margin)
{
    CsrViewInitY(&wrk->item, &wrk->vy, size, margin);
}

// FF3 0x00153ac8
void MenuCsrWorkUpdate(MENU_CSR_WRK *wrk, int pad)
{
    CsrPadCtrl(&wrk->item, &wrk->vx, &wrk->vy, (u_char)pad);
}

// FF3 0x00153af0
void MenuCsrWorkRelease(MENU_CSR_WRK *wrk)
{
    CsrRelease(&wrk->item, &wrk->vx, &wrk->vy);
    if (wrk->item.ext != 0)
        return;
    heapCtrlFree(GetSystemHeapWrkP(), wrk->item.tbl, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Whether the cursor moved this frame; plays `se` then (none when se < 0).
// FF3 0x00153b50

int MenuCsrWorkIsMovedSe(MENU_CSR_WRK *wrk, int se)
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

// FF3 0x00153ba0
short MenuCsrWorkSetPos(MENU_CSR_WRK *wrk, int pos)
{
    return CsrSetId(&wrk->item, &wrk->vx, &wrk->vy, pos);
}

// FF3 0x00153bd0
short MenuCsrWorkGetPos(MENU_CSR_WRK *wrk)
{
    return wrk->item.id;
}

// FF3 0x00153bd8
short MenuCsrWorkGetTopX(MENU_CSR_WRK *wrk)
{
    return CsrViewGetTop(&wrk->vx);
}

// FF3 0x00153bf8
short MenuCsrWorkGetTopY(MENU_CSR_WRK *wrk)
{
    return CsrViewGetTop(&wrk->vy);
}

// FF3 0x00153c18
short MenuCsrWorkGetPosX(MENU_CSR_WRK *wrk)
{
    return CsrViewGetPos(&wrk->vx);
}

// FF3 0x00153c38
short MenuCsrWorkGetPosY(MENU_CSR_WRK *wrk)
{
    return CsrViewGetPos(&wrk->vy);
}

// FF3 0x00153c58
int MenuCsrWorkPagePrevX(MENU_CSR_WRK *wrk)
{
    return CsrPagePrevX(&wrk->item, &wrk->vx);
}

// FF3 0x00153c70
int MenuCsrWorkPageNextX(MENU_CSR_WRK *wrk)
{
    return CsrPageNextX(&wrk->item, &wrk->vx);
}

// FF3 0x00153c88
int MenuCsrWorkPagePrevY(MENU_CSR_WRK *wrk)
{
    return CsrPagePrevY(&wrk->item, &wrk->vy);
}

// FF3 0x00153ca0
int MenuCsrWorkPageNextY(MENU_CSR_WRK *wrk)
{
    return CsrPageNextY(&wrk->item, &wrk->vy);
}

// FF3 0x00153cb8
short MenuCsrWorkSetFirst(MENU_CSR_WRK *wrk)
{
    return CsrSetId(&wrk->item, &wrk->vx, &wrk->vy, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Item h - 1 (the last row of a one-column list), shown at the bottom of
// the y view.
// FF3 0x00153cd8

void MenuCsrWorkSetLast(MENU_CSR_WRK *wrk)
{
    CsrSetId(&wrk->item, &wrk->vx, &wrk->vy, wrk->item.h - 1);
    CsrViewSetPosY(&wrk->item, &wrk->vy, wrk->vy.size - 1);
}

// FF3 0x00153d38
int MenuCsrWorkIsWrapY(MENU_CSR_WRK *wrk)
{
    return (wrk->item.flags >> 13) & 1;
}

// FF3 0x00153d48
MENU_CSR_TBL *MenuCsrGetTbl(int no)
{
    return &menu_csr_tbl[no];
}
