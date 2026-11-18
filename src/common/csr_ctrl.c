/* ==========================================================================
 *  common/csr_ctrl.c
 *
 *  The grid cursor engine (see csr_ctrl.h). FF3-only, 0x1edd90..0x1eefe8:
 *  the cursor wrappers, the item grid (CsrItem*), the scroll views
 *  (CsrView*) and the page steps.
 * ======================================================================== */
#include "common/csr_ctrl.h"
#include "ingame/menu/menu_mouse.h"     // PC: MenuMouseCsrApply

#include "system/pad/pad.h"             // pad_wrk

// Pad buttons the grid reads (SCE_PAD bits): the d-pad and the shoulders.
#define CSR_PAD_UP      0x1000
#define CSR_PAD_RIGHT   0x2000
#define CSR_PAD_DOWN    0x4000
#define CSR_PAD_LEFT    0x8000
#define CSR_PAD_L2      0x0001
#define CSR_PAD_R2      0x0002
#define CSR_PAD_L1      0x0004
#define CSR_PAD_R1      0x0008
#define CSR_PAD_SHOULDER (CSR_PAD_L1 | CSR_PAD_R1 | CSR_PAD_L2 | CSR_PAD_R2)
#define CSR_PAD_ALL     0xf00f

#define CSR_RPT_FIRST   10              // frames before the first repeat
#define CSR_RPT_NEXT    5               // frames between repeats

// ──────────────────────────────────────────────────────────────────────
// A cursor over tbl (w x h; ext: the table is the caller's), both views off.
// FF3 0x001edd90

void CsrInit(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short *tbl, short w, short h, int ext, u_short cfg)
{
    CsrItemInit(item, tbl, w, h, ext, cfg);
    CsrViewReset(vx);
    CsrViewReset(vy);
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: move by pad `pad`, then let the views follow.
// FF3 0x001eddf8

void CsrPadCtrl(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, u_char pad)
{
    CsrItemPadCtrl(item, pad);
    CsrViewUpdate(item, vx, vy);
    MenuMouseCsrClaim(item);            // PC: a cursor here is a menu
    MenuMouseCsrApply(item, vx, vy);    // PC: a move the mouse asked for
}

// FF3 0x001ede50
void CsrRelease(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy)
{
    CsrItemRelease(item);
}

// ──────────────────────────────────────────────────────────────────────
// Put the cursor on item `id`; both views put it on their first cell.
// Returns the item id, -1 when the table has no such item.
// FF3 0x001ede88

short CsrSetId(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short id)
{
    short ret;

    ret = CsrItemSetId(item, id);
    CsrViewSetPosX(item, vx, 0);
    CsrViewSetPosY(item, vy, 0);
    return ret;
}

// FF3 0x001edf00
short CsrSetXY(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy, short x, short y)
{
    short ret;

    ret = CsrItemSetXY(item, x, y);
    CsrViewSetPosX(item, vx, 0);
    CsrViewSetPosY(item, vy, 0);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Set the grid up at (0, 0). A table that is not the caller's gets item k
// in cell k.
// FF3 0x001edf88

void CsrItemInit(CSR_ITEM *item, short *tbl, short w, short h, int ext, u_short cfg)
{
    item->tbl = tbl;
    item->ext = ext;
    item->x = 0;
    item->w = w;
    item->h = h;
    item->y = 0;
    item->rpt = 1;
    item->flags = cfg & CSR_CFG_MASK;
    item->id = tbl[0];                  // before the fill; CsrItemSkipOff rereads it
    item->held[0] = 0;
    item->held[1] = 0;
    if (w == 0)
        item->w = 1;
    if (item->h == 0)
        item->h = 1;

    if (w > 0 && h > 0 && ext == 0)
    {
        for (int i = 0; i < w; i++)
        {
            for (int j = 0; j < h; j++)
                tbl[i * h + j] = (short)(i * h + j);
        }
    }
    CsrItemSkipOff(item);
}

// ──────────────────────────────────────────────────────────────────────
// Read pad `pad` and move. A new press acts at once, a held direction
// repeats after 10 frames and then every 5. Shoulder buttons (when the
// configuration takes them) only raise their event bit. A d-pad step goes
// on over empty cells (-1) and over cells of the same item, wrapping or
// stopping at the edges (CSR_CFG_CLAMP_*; a clamped step turns back), until
// it reaches another item or comes back to where it started.
// FF3 0x001ee060

void CsrItemPadCtrl(CSR_ITEM *item, int pad)
{
    u_short cfg;
    u_short old;
    u_short held;
    u_short trg;
    int     press;
    short   ox;
    short   oy;
    short   oid;
    short   w;
    short   h;
    int     dx;
    int     dy;
    short   x;
    short   y;

    cfg = item->flags;
    item->flags = cfg & CSR_CFG_MASK;

    old  = item->held[pad];
    held = pad_wrk[pad].held & CSR_PAD_ALL;
    trg  = (old ^ held) & held;
    if (!(cfg & CSR_CFG_L1R1))
    {
        trg  &= ~(CSR_PAD_L1 | CSR_PAD_R1);
        held &= ~(CSR_PAD_L1 | CSR_PAD_R1);
    }
    if (!(cfg & CSR_CFG_L2R2))
    {
        held &= ~(CSR_PAD_L2 | CSR_PAD_R2);
        trg  &= ~(CSR_PAD_L2 | CSR_PAD_R2);
    }

    press = 0;
    if ((held & old) == 0)
    {
        if (trg != 0)
        {
            item->rpt = 0;
            press = 1;
            item->flags = (cfg & CSR_CFG_MASK) | CSR_EV_PRESS;
        }
    }
    else
    {
        item->rpt--;
    }

    ox  = item->x;
    oy  = item->y;
    item->held[pad] = held;
    oid = item->id;
    if (item->rpt > 0)
        return;
    item->rpt = press ? CSR_RPT_FIRST : CSR_RPT_NEXT;

    if (held & CSR_PAD_SHOULDER)
    {
        if (held & CSR_PAD_L1)
            item->flags |= CSR_EV_L1;
        else if (held & CSR_PAD_R1)
            item->flags |= CSR_EV_R1;
        else if (held & CSR_PAD_L2)
            item->flags |= CSR_EV_L2;
        else if (held & CSR_PAD_R2)
            item->flags |= CSR_EV_R2;
        return;
    }

    dx = 0;
    dy = 0;
    if (held & CSR_PAD_UP)
    {
        dy = -1;
        item->flags |= CSR_EV_UP;
    }
    else if (held & CSR_PAD_DOWN)
    {
        dy = 1;
        item->flags |= CSR_EV_DOWN;
    }
    else if (held & CSR_PAD_RIGHT)
    {
        dx = 1;
        item->flags |= CSR_EV_RIGHT;
    }
    else if (held & CSR_PAD_LEFT)
    {
        dx = -1;
        item->flags |= CSR_EV_LEFT;
    }

    w = item->w;
    h = item->h;
    x = ox;
    y = oy;
    for (;;)
    {
        u_short flags;

        item->x = (short)(x + dx);
        item->y = (short)(y + dy);
        flags = item->flags & ~CSR_EV_MOVED;
        item->flags = flags;

        if (item->x >= w)
        {
            item->flags = flags | CSR_EV_EDGE_X;
            if (!(flags & CSR_CFG_CLAMP_X))
            {
                item->x = 0;
            }
            else
            {
                item->x = w - 1;
                dx = -dx;
            }
        }
        else if (item->x < 0)
        {
            item->flags = flags | CSR_EV_EDGE_X;
            if (flags & CSR_CFG_CLAMP_X)
            {
                item->x = 0;
                dx = -dx;
            }
            else
            {
                item->x = w - 1;
            }
        }

        if (item->y >= h)
        {
            flags = item->flags;
            item->flags = flags | CSR_EV_EDGE_Y;
            if (!(flags & CSR_CFG_CLAMP_Y))
            {
                item->y = 0;
            }
            else
            {
                item->y = h - 1;
                dy = -dy;
            }
        }
        else if (item->y < 0)
        {
            flags = item->flags;
            item->flags = flags | CSR_EV_EDGE_Y;
            if (flags & CSR_CFG_CLAMP_Y)
            {
                item->y = 0;
                dy = -dy;
            }
            else
            {
                item->y = h - 1;
            }
        }

        x = item->x;
        y = item->y;
        item->id = item->tbl[x + y * w];

        // Back where it started: nothing else to stand on.
        if (item->id == oid && x == ox && y == oy)
        {
            item->x = ox;
            item->y = oy;
            item->id = item->tbl[ox + oy * w];
            return;
        }
        if (x != ox || y != oy)
            item->flags |= CSR_EV_MOVED;
        if (item->id != -1 && item->id != oid)
            return;
    }
}

// FF3 0x001ee478
void CsrItemRelease(CSR_ITEM *item)
{
}

// ──────────────────────────────────────────────────────────────────────
// Put the cursor on the first cell holding `id`. Returns the item id under
// the cursor, -1 when the table has no such item (cursor unchanged).
// FF3 0x001ee480

short CsrItemSetId(CSR_ITEM *item, short id)
{
    int xy;

    xy = CsrItemFind(item, id);
    if (xy == -1)
        return -1;
    item->x = (short)(xy >> 16);
    item->y = (short)xy;
    CsrItemSkipOff(item);
    return item->id;
}

// ──────────────────────────────────────────────────────────────────────
// Read the item under the cursor; on an empty cell step back (left, then
// to the end of the row above) until an item is found.
// FF3 0x001ee4d8

void CsrItemSkipOff(CSR_ITEM *item)
{
    short x;
    short y;
    int   w;

    y = item->y;
    x = item->x;
    w = item->w;
    item->id = item->tbl[x + y * w];
    while (item->id == -1)
    {
        x--;
        item->x = x;
        if (x < 0 || w <= x)
        {
            y--;
            item->x = item->w - 1;
            item->y = y;
            if (y < 0)
            {
                item->y = 0;
                y = 0;
            }
            else if (item->h <= y)
            {
                item->y = 0;
                item->x = 0;
                return;
            }
        }
        x = item->x;
        item->id = item->tbl[x + y * w];
    }
}

// ──────────────────────────────────────────────────────────────────────
// The first cell holding `id`: x << 16 | y, -1 when none does.
// FF3 0x001ee5c8

int CsrItemFind(CSR_ITEM *item, short id)
{
    short w;
    int   i;

    w = item->w;
    for (i = 0; ; i++)
    {
        if (i >= item->h * w)
            return -1;
        if (item->tbl[i] == id)
            break;
    }
    return (u_short)(i / w) | (i % w) << 16;
}

// ──────────────────────────────────────────────────────────────────────
// Put the cursor on cell (x, y), stepping back off an empty one. Returns
// the item id, -1 when (x, y) is refused.
// FF3 0x001ee638

short CsrItemSetXY(CSR_ITEM *item, short x, short y)
{
    if (x < 0 || x >= item->w)
        return -1;
    if (y < 0)
        return -1;
    if (x >= item->h)                   // sic: FF3 tests x against h; y is not bounded
        return -1;

    item->x = x;
    item->y = y;
    item->id = item->tbl[x * y];        // sic: FF3 reads tbl[x * y]; CsrItemSkipOff rereads tbl[x + y * w]
    CsrItemSkipOff(item);
    return item->id;
}

// ──────────────────────────────────────────────────────────────────────
// A view that is off: CsrViewGetTop / CsrViewGetPos return -1, the steps
// do nothing.
// FF3 0x001ee6c0

void CsrViewReset(CSR_VIEW *view)
{
    view->on = 0;
    view->top = -1;
    view->size = -1;
    view->margin = -1;
    view->lim = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Switch a view on: `size` of `total` cells visible (all of them when size
// is not in 1..total-1), scrolling back keeps `margin` cells before the
// cursor, scrolling on starts past size - margin - 1.
// FF3 0x001ee6e0

void CsrViewInit(CSR_VIEW *view, short size, short margin, short total)
{
    short lim;

    view->on = 1;
    if (size < 1 || total <= size)
    {
        if (total < 1)
            total = 1;
        view->size = total;
    }
    else
    {
        view->size = size;
    }

    lim = view->size;
    if (margin < 1)
    {
        view->margin = 0;
    }
    else
    {
        view->margin = margin;
        lim -= margin;
    }
    view->top = 0;
    view->lim = lim - 1;
    view->pos = 0;
}

// FF3 0x001ee760
void CsrViewInitX(CSR_ITEM *item, CSR_VIEW *vx, short size, short margin)
{
    CsrViewInit(vx, size, margin, item->w);
}

// FF3 0x001ee798
void CsrViewInitY(CSR_ITEM *item, CSR_VIEW *vy, short size, short margin)
{
    CsrViewInit(vy, size, margin, item->h);
}

// ──────────────────────────────────────────────────────────────────────
// The cursor wrapped to the start: show from cell 0.
// FF3 0x001ee7d0

void CsrViewJumpFirst(CSR_VIEW *view, short pos)
{
    if (view->on)
    {
        view->pos = pos;
        view->top = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The cursor wrapped to the end: show the last `size` cells.
// FF3 0x001ee7f0

void CsrViewJumpLast(CSR_VIEW *view, short pos, short total)
{
    if (view->on)
    {
        view->top = total - view->size;
        view->pos = pos - (total - view->size);
    }
}

// ──────────────────────────────────────────────────────────────────────
// One cell back: scroll while the cursor is inside the margin, else move it.
// FF3 0x001ee828

void CsrViewDec(CSR_VIEW *view)
{
    short pos;

    if (!view->on)
        return;
    pos = view->pos;
    if (view->top > 0 && pos <= view->margin)
    {
        view->top--;
        return;
    }
    if (pos > 0)
        view->pos = pos - 1;
}

// ──────────────────────────────────────────────────────────────────────
// One cell on: scroll while there is more and the cursor is past lim, else
// move it.
// FF3 0x001ee888

void CsrViewInc(CSR_VIEW *view, short total)
{
    if (!view->on)
        return;
    if (view->top < total - view->size && view->pos >= view->lim)
    {
        view->top++;
        return;
    }
    if (view->pos < view->size - 1)
        view->pos++;
}

// ──────────────────────────────────────────────────────────────────────
// After a move, step the view of its axis, or jump it when the step
// wrapped (to the end when it wrapped backwards or stopped at the end).
// FF3 0x001ee910

void CsrViewUpdate(CSR_ITEM *item, CSR_VIEW *vx, CSR_VIEW *vy)
{
    u_short flags;

    flags = item->flags;
    if (!(flags & CSR_EV_MOVED))
        return;

    if (flags & CSR_EV_UP)
    {
        if (!(flags & CSR_EV_EDGE_Y))
            CsrViewDec(vy);
        else if (flags & CSR_CFG_CLAMP_Y)
            CsrViewJumpFirst(vy, item->y);
        else
            CsrViewJumpLast(vy, item->y, item->h);
    }
    else if (flags & CSR_EV_DOWN)
    {
        if (!(flags & CSR_EV_EDGE_Y))
            CsrViewInc(vy, item->h);
        else if (!(flags & CSR_CFG_CLAMP_Y))
            CsrViewJumpFirst(vy, item->y);
        else
            CsrViewJumpLast(vy, item->y, item->h);
    }
    else if (flags & CSR_EV_LEFT)
    {
        if (!(flags & CSR_EV_EDGE_X))
            CsrViewDec(vx);
        else if (flags & CSR_CFG_CLAMP_X)
            CsrViewJumpFirst(vx, item->x);
        else
            CsrViewJumpLast(vx, item->x, item->w);
    }
    else if (flags & CSR_EV_RIGHT)
    {
        if (!(flags & CSR_EV_EDGE_X))
            CsrViewInc(vx, item->w);
        else if (!(flags & CSR_CFG_CLAMP_X))
            CsrViewJumpFirst(vx, item->x);
        else
            CsrViewJumpLast(vx, item->x, item->w);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Scroll so the view starts at cell `top` (0..w-1), keeping the cursor.
// FF3 0x001eea30

void CsrViewSetTopX(CSR_ITEM *item, CSR_VIEW *vx, short top)
{
    int n = top;

    if (!vx->on)
        return;
    if (n >= item->w)
        n = (short)(item->w - 1);
    if (n < 0)
        n = 0;
    vx->top = (short)n;
    vx->pos = item->x - (short)n;
}

// FF3 0x001eea80
void CsrViewSetTopY(CSR_ITEM *item, CSR_VIEW *vy, short top)
{
    int n = top;

    if (!vy->on)
        return;
    if (n >= item->h)
        n = (short)(item->h - 1);
    if (n < 0)
        n = 0;
    vy->top = (short)n;
    vy->pos = item->y - (short)n;
}

// ──────────────────────────────────────────────────────────────────────
// Scroll so the cursor shows in view cell `pos` (0..size-1).
// FF3 0x001eead0

void CsrViewSetPosX(CSR_ITEM *item, CSR_VIEW *vx, short pos)
{
    int n = pos;

    if (!vx->on)
        return;
    if (n >= vx->size)
        n = (short)(vx->size - 1);
    if (n < 0)
        n = 0;
    vx->pos = (short)n;
    vx->top = item->x - (short)n;
}

// FF3 0x001eeb20
void CsrViewSetPosY(CSR_ITEM *item, CSR_VIEW *vy, short pos)
{
    int n = pos;

    if (!vy->on)
        return;
    if (n >= vy->size)
        n = (short)(vy->size - 1);
    if (n < 0)
        n = 0;
    vy->pos = (short)n;
    vy->top = item->y - (short)n;
}

// FF3 0x001eeb70
short CsrViewGetTop(CSR_VIEW *view)
{
    return view->on ? view->top : -1;
}

// FF3 0x001eeb88
short CsrViewGetPos(CSR_VIEW *view)
{
    return view->on ? view->pos : -1;
}

// ──────────────────────────────────────────────────────────────────────
// One page (view size) left; from the first column to the last. Returns
// whether the cursor is on the first column. Only when the grid is wider
// than the view.
// FF3 0x001eeba0

int CsrPagePrevX(CSR_ITEM *item, CSR_VIEW *vx)
{
    short size;
    short pos;

    if (!vx->on)
        return 0;
    size = vx->size;
    if (item->w <= size)
        return 0;

    if (item->x == 0)
    {
        item->x = item->w - 1;
        pos = size - 1;
    }
    else if (size < item->x)
    {
        item->x = item->x - size;
        pos = (vx->pos <= item->x) ? vx->pos : item->x;
    }
    else
    {
        item->x = 0;
        pos = 0;
    }
    CsrItemSetXY(item, item->x, item->y);
    CsrViewSetPosX(item, vx, pos);
    return item->x == 0;
}

// ──────────────────────────────────────────────────────────────────────
// One page right; from the last column to the first. Returns whether the
// cursor is on the last column.
// FF3 0x001eec90

int CsrPageNextX(CSR_ITEM *item, CSR_VIEW *vx)
{
    short size;
    short w;
    short pos;

    if (!vx->on)
        return 0;
    size = vx->size;
    w = item->w;
    if (size >= w)
        return 0;

    if (item->x == w - 1)
    {
        item->x = 0;
        pos = 0;
    }
    else if (item->x + size < w - 1)
    {
        item->x = item->x + size;
        pos = vx->pos;
    }
    else
    {
        item->x = w - 1;
        pos = size - 1;
    }
    CsrItemSetXY(item, item->x, item->y);
    CsrViewSetPosX(item, vx, pos);
    if (item->w - vx->size < vx->top)
        CsrViewSetTopX(item, vx, item->w - vx->size);
    return item->x == item->w - 1;
}

// FF3 0x001eedc0
int CsrPagePrevY(CSR_ITEM *item, CSR_VIEW *vy)
{
    short size;
    short pos;

    if (!vy->on)
        return 0;
    size = vy->size;
    if (item->h <= size)
        return 0;

    if (item->y == 0)
    {
        item->y = item->h - 1;
        pos = size - 1;
    }
    else if (size < item->y)
    {
        item->y = item->y - size;
        pos = (vy->pos <= item->y) ? vy->pos : item->y;
    }
    else
    {
        item->y = 0;
        pos = 0;
    }
    CsrItemSetXY(item, item->x, item->y);
    CsrViewSetPosY(item, vy, pos);
    return item->y == 0;
}

// FF3 0x001eeeb0
int CsrPageNextY(CSR_ITEM *item, CSR_VIEW *vy)
{
    short size;
    short h;
    short pos;

    if (!vy->on)
        return 0;
    size = vy->size;
    h = item->h;
    if (size >= h)
        return 0;

    if (item->y == h - 1)
    {
        item->y = 0;
        pos = 0;
    }
    else if (item->y + size < h - 1)
    {
        item->y = item->y + size;
        pos = vy->pos;
    }
    else
    {
        item->y = h - 1;
        pos = size - 1;
    }
    CsrItemSetXY(item, item->x, item->y);
    CsrViewSetPosY(item, vy, pos);
    if (item->h - vy->size < vy->top)
        CsrViewSetTopY(item, vy, item->h - vy->size);
    return item->y == item->h - 1;
}
