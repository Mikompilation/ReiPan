/* ==========================================================================
 *  system/b2d/ScrollBar.c
 *
 *  List scroll bars drawn with B2d layout items (see ScrollBar.h). FF3
 *  0x135e28..0x136df8 and the inline virtuals 0x2acae0..0x2acfb8, FF3's own
 *  code. GCC 3 emitted every constructor twice (complete and base-object
 *  copies, same body) and three destructors per class; one C++ definition
 *  stands for each group here.
 * ======================================================================== */
#include "system/b2d/ScrollBar.h"

#include "system/b2d/B2dSpr.h"          // B2dSprFreezePart(ByName), B2dSprSet*
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo

// An item's part value as FF3 reads it (item->part[no].value, no getter).
// PC: 0 without an item, where FF3 read low memory.
static int ScrollItemPart(B2D_ITEM *item, int no)
{
    return item != nullptr ? item->part[no].value : 0;
}

// The knob parts the bar sets itself, frozen by the constructors in this
// order: per pair every piece, then the colour of every piece.
static const int scroll_knob_pair[2][2] = { { B2D_PART_X, B2D_PART_Y }, { B2D_PART_SX, B2D_PART_SY } };

// ──────────────────────────────────────────────────────────────────────
// ScrollBar
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00135e28.
ScrollBar::ScrollBar()
{
    length = 0;
    rows   = 0;
    page   = 0;
    cursor = 0;
    first  = 0;
}

// FF3 0x00135e88 (complete; base-object copy 0x00135e78, deleting 0x00135e98):
// only the vptr is restored.
ScrollBar::~ScrollBar()
{
}

// first / rows clamped to 0..1; 0 without rows.
// FF3 0x002acc60.
float ScrollBar::GetPosRate()
{
    float n = (float)rows;
    float rate;

    if (n <= 0.0f)
        return 0.0f;
    rate = (float)first / n;
    if (rate < 0.0f)
        return 0.0f;
    if (1.0f < rate)
        return 1.0f;
    return rate;
}

// page / rows clamped to 0..1; 1 (the knob fills the track) without rows.
// FF3 0x002accc0.
float ScrollBar::GetPageRate()
{
    float n = (float)rows;
    float rate;

    if (n <= 0.0f)
        return 1.0f;
    rate = (float)page / n;
    if (rate < 0.0f)
        return 0.0f;
    if (1.0f < rate)
        return 1.0f;
    return rate;
}

// FF3 0x002acbc8.
void ScrollBar::SetRange(short n, short per_page)
{
    rows = n;
    page = per_page;
    if (n < 0)
        rows = 0;
    if (page < 0)
        page = 0;
}

// FF3 0x002acc00.
void ScrollBar::SetPos(short cursor_row, short first_row)
{
    cursor = cursor_row;
    first  = first_row;
    if (cursor_row < 0)
        cursor = 0;
    if (first < 0)
        first = 0;
}

// FF3 0x002acae0.
void ScrollBar::SetLength(int len)
{
    length = len;
}

// FF3 0x002acc38.
int ScrollBar::CanScrollUp()
{
    return 0 < first;
}

// FF3 0x002acc48.
int ScrollBar::CanScrollDown()
{
    return first < rows - page;
}

// FF3 0x002acae8.
int ScrollBar::IsScrollable()
{
    if (CanScrollUp() == 0 && CanScrollDown() == 0)
        return 0;
    return 1;
}

// FF3 0x002acb48.
int ScrollBar::GetKnobPos()
{
    return (int)((float)length * GetPosRate());
}

// FF3 0x002acb88.
int ScrollBar::GetKnobLen()
{
    return (int)((float)length * GetPageRate());
}

// ──────────────────────────────────────────────────────────────────────
// B2d_ScrollBar3: a three-piece knob
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001364a0 (copy 0x001362a8).
// callcheck: ScrollBarCtor (the base constructor, implicit)
B2d_ScrollBar3::B2d_ScrollBar3(int file, int frame, int top, int mid, int bottom)
{
    int handle = BpCtlGetHandleByFileNo(file);
    B2D_ITEM *frame_item = B2dCtlFindItem(handle, frame);

    piece[0] = B2dCtlFindItem(handle, top);
    piece[1] = B2dCtlFindItem(handle, mid);
    piece[2] = B2dCtlFindItem(handle, bottom);
    length   = ScrollItemPart(frame_item, B2D_PART_H);
    x        = ScrollItemPart(frame_item, B2D_PART_X);
    y        = ScrollItemPart(frame_item, B2D_PART_Y);

    for (const int *pair : scroll_knob_pair)
    {
        B2dSprFreezePart(handle, top, pair[0], 0);
        B2dSprFreezePart(handle, top, pair[1], 0);
        B2dSprFreezePart(handle, mid, pair[0], 0);
        B2dSprFreezePart(handle, mid, pair[1], 0);
        B2dSprFreezePart(handle, bottom, pair[0], 0);
        B2dSprFreezePart(handle, bottom, pair[1], 0);
    }
    B2dSprFreezePart(handle, top, B2D_PART_RGB, 0);
    B2dSprFreezePart(handle, mid, B2D_PART_RGB, 0);
    B2dSprFreezePart(handle, bottom, B2D_PART_RGB, 0);
}

// FF3 0x001360b0 (copy 0x00135eb8).
// callcheck: ScrollBarCtor (the base constructor, implicit)
B2d_ScrollBar3::B2d_ScrollBar3(int file, const char *frame, const char *top, const char *mid,
                               const char *bottom)
{
    int handle = BpCtlGetHandleByFileNo(file);
    B2D_ITEM *frame_item = B2dCtlFindItemByName(handle, frame);

    piece[0] = B2dCtlFindItemByName(handle, top);
    piece[1] = B2dCtlFindItemByName(handle, mid);
    piece[2] = B2dCtlFindItemByName(handle, bottom);
    length   = ScrollItemPart(frame_item, B2D_PART_H);
    x        = ScrollItemPart(frame_item, B2D_PART_X);
    y        = ScrollItemPart(frame_item, B2D_PART_Y);

    for (const int *pair : scroll_knob_pair)
    {
        B2dSprFreezePartByName(handle, top, pair[0], 0);
        B2dSprFreezePartByName(handle, top, pair[1], 0);
        B2dSprFreezePartByName(handle, mid, pair[0], 0);
        B2dSprFreezePartByName(handle, mid, pair[1], 0);
        B2dSprFreezePartByName(handle, bottom, pair[0], 0);
        B2dSprFreezePartByName(handle, bottom, pair[1], 0);
    }
    B2dSprFreezePartByName(handle, top, B2D_PART_RGB, 0);
    B2dSprFreezePartByName(handle, mid, B2D_PART_RGB, 0);
    B2dSprFreezePartByName(handle, bottom, B2D_PART_RGB, 0);
}

// FF3 0x001366c0 (complete; base-object copy 0x00136698, deleting 0x001366e8).
// callcheck: ScrollBarDtorBase (the base class destructor)
B2d_ScrollBar3::~B2d_ScrollBar3()
{
}

// Moves the track origin by item id's position (the bar's items sit in a
// layout drawn relative to that item).
// FF3 0x001367d0.
void B2d_ScrollBar3::SetOrigin(int file, int id)
{
    B2D_ITEM *item = B2dCtlFindItem(BpCtlGetHandleByFileNo(file), id);

    x -= ScrollItemPart(item, B2D_PART_X);
    y -= ScrollItemPart(item, B2D_PART_Y);
}

// FF3 0x00136720.
void B2d_ScrollBar3::Draw(short cursor_row, short first_row)
{
    int off, len, bright;

    SetPos(cursor_row, first_row);
    off    = GetKnobPos();
    len    = GetKnobLen();
    bright = IsScrollable();
    B2dScrollBarDrawKnob3(x, y + off, len, piece, bright);
}

// Top piece at x, y, the middle stretched to fill, the bottom after it.
// Without a middle height the middle is hidden and both ends are scaled by
// (top h + bottom h) / len and the bottom moved by that rate -- FF3's
// ratio, the inverse of shrinking the ends into len. Dim (0x50) when the
// list cannot scroll.
// FF3 0x002acd28.
void B2dScrollBarDrawKnob3(int x, int y, int len, B2D_ITEM **piece, int bright)
{
    int top_h, bottom_h, mid_h, mid_len, sy;
    u_int c;

    if (len <= 0)
        return;

    top_h    = ScrollItemPart(piece[0], B2D_PART_H);
    bottom_h = ScrollItemPart(piece[2], B2D_PART_H);
    mid_h    = ScrollItemPart(piece[1], B2D_PART_H);
    mid_len  = len - top_h - bottom_h;

    if (mid_h > 0)
    {
        float rate = (float)mid_len / (float)mid_h;

        B2dSprSetDisp(piece[0], 1);
        B2dSprSetDisp(piece[1], 1);
        B2dSprSetDisp(piece[2], 1);
        B2dSprSetPos(piece[0], x, y);
        B2dSprSetPos(piece[1], x, y + top_h);
        B2dSprSetPos(piece[2], x, y + top_h + mid_len);
        B2dSprSetScale(piece[1], 0x400, (int)(rate * 1024.0f + 0.5f));
    }
    else
    {
        float rate;

        B2dSprSetDisp(piece[0], 1);
        B2dSprSetDisp(piece[1], 0);
        B2dSprSetDisp(piece[2], 1);
        rate = (float)(top_h + bottom_h) / (float)len;
        B2dSprSetPos(piece[0], x, y);
        B2dSprSetPos(piece[2], x, y + (int)((float)top_h * rate + 0.5f));
        sy = (int)(rate * 1024.0f + 0.5f);
        B2dSprSetScale(piece[0], 0x400, sy);
        B2dSprSetScale(piece[2], 0x400, sy);
    }

    c = bright != 0 ? 0x80 : 0x50;
    B2dSprSetRgb(piece[0], c, c, c);
    B2dSprSetRgb(piece[1], c, c, c);
    B2dSprSetRgb(piece[2], c, c, c);
}

// ──────────────────────────────────────────────────────────────────────
// B2d_ScrollBar1: a one-item knob
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00136a20 (copy 0x00136b18; no caller seen).
// callcheck: ScrollBarCtor (the base constructor, implicit)
B2d_ScrollBar1::B2d_ScrollBar1(int file, int frame, int knob_id)
{
    int handle = BpCtlGetHandleByFileNo(file);
    B2D_ITEM *frame_item = B2dCtlFindItem(handle, frame);

    knob   = B2dCtlFindItem(handle, knob_id);
    length = ScrollItemPart(frame_item, B2D_PART_H);
    x      = ScrollItemPart(frame_item, B2D_PART_X);
    y      = ScrollItemPart(frame_item, B2D_PART_Y);
    for (const int *pair : scroll_knob_pair)
    {
        B2dSprFreezePart(handle, knob_id, pair[0], 0);
        B2dSprFreezePart(handle, knob_id, pair[1], 0);
    }
    B2dSprFreezePart(handle, knob_id, B2D_PART_RGB, 0);
}

// FF3 0x00136928 (copy 0x00136830).
// callcheck: ScrollBarCtor (the base constructor, implicit)
B2d_ScrollBar1::B2d_ScrollBar1(int file, const char *frame, const char *knob_name)
{
    int handle = BpCtlGetHandleByFileNo(file);
    B2D_ITEM *frame_item = B2dCtlFindItemByName(handle, frame);

    knob   = B2dCtlFindItemByName(handle, knob_name);
    length = ScrollItemPart(frame_item, B2D_PART_H);
    x      = ScrollItemPart(frame_item, B2D_PART_X);
    y      = ScrollItemPart(frame_item, B2D_PART_Y);
    for (const int *pair : scroll_knob_pair)
    {
        B2dSprFreezePartByName(handle, knob_name, pair[0], 0);
        B2dSprFreezePartByName(handle, knob_name, pair[1], 0);
    }
    B2dSprFreezePartByName(handle, knob_name, B2D_PART_RGB, 0);
}

// FF3 0x00136c38 (complete; base-object copy 0x00136c10, deleting 0x00136c60).
// callcheck: ScrollBarDtorBase (the base class destructor)
B2d_ScrollBar1::~B2d_ScrollBar1()
{
}

// The knob at x, y + knob pos, stretched to the knob length; dim (0x50)
// when the list cannot scroll. Nothing when the knob has no length.
// FF3 0x00136c98.
void B2d_ScrollBar1::Draw(short cursor_row, short first_row)
{
    int off, len, bright;
    u_int c;

    SetPos(cursor_row, first_row);
    off    = GetKnobPos();
    len    = GetKnobLen();
    bright = IsScrollable();
    if (len <= 0)
        return;

    B2dSprSetPos(knob, x, y + off);
    B2dSprSetScale(knob, 0x400,
                   (int)((float)len / (float)ScrollItemPart(knob, B2D_PART_H) * 1024.0f + 0.5f));
    B2dSprSetDisp(knob, 1);
    c = bright != 0 ? 0x80 : 0x50;
    B2dSprSetRgb(knob, c, c, c);
}
