/* ==========================================================================
 *  system/b2d/BpObj.c
 *
 *  A layout played as one object (see BpObj.h). FF3 0x136df8..0x1374d8,
 *  FF3's own code.
 * ======================================================================== */
#include "system/b2d/BpObj.h"

#include <new>                          // operator new / delete

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "ingame/menu/menu_bg.h"      // MenuScrollArrowDisp
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtl*, BpPlayLabel, B2dCtlGetLongestPart, B2dPartIsPlaying
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// The layout
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00136df8.
BP_OBJ::BP_OBJ(int file)
{
    this->file = file;
    _014 = 0;
    state = BP_OBJ_NEW;
    played = 0;
    bar = nullptr;
    csr = nullptr;
}

// Requests the layout on the first call; 1 once it is loaded.
// FF3 0x00136e18.
int BP_OBJ::Load()
{
    if (state == BP_OBJ_NEW)
    {
        BpCtlLoadReq(file, nullptr);
        state = BP_OBJ_LOADING;
    }
    if (state == BP_OBJ_LOADING && BpCtlIsDataReady(BpCtlGetHandleByFileNo(file)) != 0)
        state = BP_OBJ_LOADED;
    return state == BP_OBJ_LOADED;
}

// Plays label when loaded (or played out); 1 when it started.
// FF3 0x00136e98.
int BP_OBJ::In(const char *label)
{
    int state = this->state;

    if (state == BP_OBJ_LOADED)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        this->state = BP_OBJ_IN;
    }
    return state == BP_OBJ_LOADED;
}

// 1 once the in label has finished (the longest part stopped).
// FF3 0x00136ef8.
int BP_OBJ::IsInEnd()
{
    if (state == BP_OBJ_IN
        && B2dPartIsPlaying(B2dCtlGetLongestPart(BpCtlGetHandleByFileNo(file))) == 0)
        state = BP_OBJ_SHOWN;
    return state == BP_OBJ_SHOWN;
}

// Plays label once shown, then waits for it; 1 when it has finished.
// FF3 0x00136f60.
int BP_OBJ::Out(const char *label)
{
    if (state == BP_OBJ_SHOWN)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        state = BP_OBJ_OUT;
    }
    if (state == BP_OBJ_OUT
        && B2dPartIsPlaying(B2dCtlGetLongestPart(BpCtlGetHandleByFileNo(file))) == 0)
        state = BP_OBJ_LOADED;
    return state == BP_OBJ_LOADED;
}

// FF3 0x00137000.
void BP_OBJ::Release()
{
    BpCtlRelease(BpCtlGetHandleByFileNo(file));
}

// ──────────────────────────────────────────────────────────────────────
// The scroll bar
// ──────────────────────────────────────────────────────────────────────

// A three-piece scroll bar on the layout's items, its track placed
// relative to item def->origin.
// FF3 0x00137020.
// callcheck: operator_new, B2dScrollBar3Ctor (new B2d_ScrollBar3)
void BP_OBJ::AddScrollBar(const BP_OBJ_SCROLL_DEF *def)
{
    B2d_ScrollBar3 *bar = new B2d_ScrollBar3(file, def->frame, def->top, def->mid, def->bottom);

    this->bar = bar;
    bar->SetOrigin(file, def->origin);
}

// FF3 0x00137090.
void BP_OBJ::DelScrollBar()
{
    if (bar != nullptr)
        delete bar;
    bar = nullptr;
}

// The bar for the list state pos, then the arrows (dim when the list
// cannot scroll that way). Nothing without a bar.
// FF3 0x001370d0.
// callcheck: B2dScrollBar3Draw (B2d_ScrollBar3::Draw)
void BP_OBJ::DrawScrollBar(const BP_OBJ_SCROLL_POS *pos, const BP_OBJ_SCROLL_ARROWS *arrows)
{
    int handle, up, down;

    if (bar == nullptr)
        return;

    bar->SetRange(pos->rows, pos->page);
    bar->Draw(pos->cursor, pos->first);
    handle = BpCtlGetHandleByFileNo(file);
    up     = bar->CanScrollUp();
    down   = bar->CanScrollDown();
    MenuScrollArrowDisp(handle, arrows->up, arrows->down, arrows->up_fx, arrows->down_fx, up, down);
}

// ──────────────────────────────────────────────────────────────────────
// Item cursors
// ──────────────────────────────────────────────────────────────────────

// Adds a cursor on item id unless the list has one: a grid sprite with
// step dx / dy whose origin row is moved by yofs. Nodes are appended.
// FF3 0x001371b0.
// callcheck: operator_new, B2dGridSprCtor (new B2D_GRID_SPR)
void BP_OBJ::AddCsr(int id, short dx, short dy, short yofs)
{
    BP_OBJ_CSR *prev = nullptr;
    BP_OBJ_CSR *node;
    B2D_GRID_SPR *spr;
    B2D_ITEM *item;

    for (node = csr; node != nullptr; node = node->next)
    {
        prev = node;
        if (node->id == id)
            return;
    }

    node = (BP_OBJ_CSR *)heapCtrlMalloc(GetSystemHeapWrkP(), sizeof(BP_OBJ_CSR), 0, 0, 0);
    node->id = id;
    spr = new B2D_GRID_SPR(file, id, dx, dy);
    node->spr = spr;

    item = B2dCtlFindItem(BpCtlGetHandleByFileNo(file), id);
    spr->x = (short)B2dSprGetPartData(item, B2D_PART_X);
    spr->y = (short)(B2dSprGetPartData(item, B2D_PART_Y) + yofs);
    node->next = nullptr;
    if (prev == nullptr)
        csr = node;
    else
        prev->next = node;
}

// Every cursor back at its origin, deleted; the list emptied.
// FF3 0x001372f8.
// callcheck: operator_delete, B2dGridSprDtor (delete)
void BP_OBJ::FreeCsr()
{
    BP_OBJ_CSR *node = csr;

    while (node != nullptr)
    {
        BP_OBJ_CSR *next = node->next;

        node->spr->SetPos(0, 0);
        if (node->spr != nullptr)
        {
            delete node->spr;
        }
        heapCtrlFree(GetSystemHeapWrkP(), node, 0, 0, 0);
        node = next;
    }
    csr = nullptr;
}

// FF3 0x00137398.
void BP_OBJ::SetCsrPos(int id, short col, short row)
{
    for (BP_OBJ_CSR *node = csr; node != nullptr; node = node->next)
    {
        if (node->id == id)
        {
            node->spr->SetPos(col, row);
            return;
        }
    }
}

// FF3 0x001373f8.
void BP_OBJ::SetCsrDisp(int id, int disp)
{
    for (BP_OBJ_CSR *node = csr; node != nullptr; node = node->next)
    {
        if (node->id == id)
        {
            node->spr->SetDisp(disp);
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// One-shot labels
// ──────────────────────────────────────────────────────────────────────

// Plays label the first time it is asked (until PlayOnceReset);
// 1 once the layout's longest part has stopped.
// FF3 0x00137448.
int BP_OBJ::PlayOnce(const char *label)
{
    if (played == 0)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        played = 1;
    }
    return B2dPartIsPlaying(B2dCtlGetLongestPart(BpCtlGetHandleByFileNo(file))) == 0;
}

// FF3 0x001374b8.
void BP_OBJ::PlayOnceReset()
{
    played = 0;
}

// FF3 0x001374c0.
int BP_OBJ::GetHandle()
{
    return BpCtlGetHandleByFileNo(file);
}
