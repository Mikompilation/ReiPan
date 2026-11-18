/* ==========================================================================
 *  system/b2d/ScrollBar.h
 *
 *  List scroll bars drawn with B2d layout items (FF3 0x135e28..0x136df8 plus
 *  the inline virtuals 0x2acae0..0x2acfb8, FF3-only). ScrollBar (RTTI) keeps
 *  the list state -- rows, rows per page, the first row shown -- and turns
 *  it into a knob position and length along a track. Two drawn variants:
 *  B2d_ScrollBar3 stacks a three-piece knob (top, stretched middle, bottom),
 *  B2d_ScrollBar1 stretches a single knob item. Both take the track's length
 *  and origin from a frame item of the layout and freeze the knob items'
 *  position, scale and colour parts so the layout's animation leaves them
 *  alone. Users: BP_OBJ (BP_OBJ::AddScrollBar), the game result screen, FurnCtl,
 *  the item menu, the option menu (ScrollBar1), the story menu's ghost list.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_SCROLLBAR_H
#define _SYSTEM_B2D_SCROLLBAR_H

#include <stddef.h>                     // offsetof

#include "system/b2d/B2dCtl.h"          // B2D_ITEM

// The list state and the knob geometry (EE 0x10 bytes, vtable 0x341378).
// Every virtual is FF3's inline satellite; the derived classes override only
// the destructor. layout: host -- the vptr (EE 4 bytes) is 8 here; EE
// offsets in the comments.
class ScrollBar
{
public:
    ScrollBar();                                        // FF3 0x00135e28

    virtual float GetPosRate();                         // EE vtable +0x00: first / rows, 0..1
    virtual float GetPageRate();                        // EE vtable +0x04: page / rows, 0..1 (1 without rows)
    virtual ~ScrollBar();                               // EE vtable +0x08 complete, +0x0c deleting
    virtual void  SetRange(short rows, short page);     // EE vtable +0x10: negatives become 0
    virtual void  SetPos(short cursor, short first);    // EE vtable +0x14: negatives become 0
    virtual void  SetLength(int len);                   // EE vtable +0x18
    virtual int   CanScrollUp();                        // EE vtable +0x1c: rows above the page
    virtual int   CanScrollDown();                      // EE vtable +0x20: rows below the page
    virtual int   IsScrollable();                       // EE vtable +0x24: either
    virtual int   GetKnobPos();                         // EE vtable +0x28: knob offset along the track
    virtual int   GetKnobLen();                         // EE vtable +0x2c: knob length

    /* 0x04 */ short rows;                  // list rows
    /* 0x06 */ short page;                  // rows shown at once
    /* 0x08 */ short cursor;                // selected row (kept, not read by the bar)
    /* 0x0a */ short first;                 // first row shown
    /* 0x0c */ int   length;                // track length in pixels (the frame item's h)
};
static_assert(offsetof(ScrollBar, rows) == sizeof(void *) + 0x00, "ScrollBar.rows (EE 0x04)");
static_assert(offsetof(ScrollBar, page) == sizeof(void *) + 0x02, "ScrollBar.page (EE 0x06)");
static_assert(offsetof(ScrollBar, cursor) == sizeof(void *) + 0x04, "ScrollBar.cursor (EE 0x08)");
static_assert(offsetof(ScrollBar, first) == sizeof(void *) + 0x06, "ScrollBar.first (EE 0x0a)");
static_assert(offsetof(ScrollBar, length) == sizeof(void *) + 0x08, "ScrollBar.length (EE 0x0c)");

// A three-piece knob (EE 0x24 bytes, vtable 0x341340; operator new(0x24)).
// layout: host -- piece[] are pointers (EE 4 bytes each); EE offsets in the
// comments.
class B2d_ScrollBar3 : public ScrollBar
{
public:
    // Items by id / by name: the track frame, then the knob's top, middle
    // and bottom pieces.
    B2d_ScrollBar3(int file, int frame, int top, int mid, int bottom);
    B2d_ScrollBar3(int file, const char *frame, const char *top, const char *mid, const char *bottom);
    ~B2d_ScrollBar3() override;

    void Draw(short cursor_row, short first_row);       // FF3 0x00136720
    void SetOrigin(int file, int id);                   // FF3 0x001367d0: track origin relative to item id

    /* 0x10 */ B2D_ITEM *piece[3];          // knob top, middle (stretched), bottom
    /* 0x1c */ int       x;                 // track origin (the frame item's x / y)
    /* 0x20 */ int       y;
};
static_assert(offsetof(B2d_ScrollBar3, piece) >= offsetof(ScrollBar, length) + sizeof(int), "B2d_ScrollBar3.piece follows ScrollBar (EE 0x10)");
static_assert(offsetof(B2d_ScrollBar3, x) == offsetof(B2d_ScrollBar3, piece) + 3 * sizeof(B2D_ITEM *), "B2d_ScrollBar3.x (EE 0x1c)");
static_assert(offsetof(B2d_ScrollBar3, y) == offsetof(B2d_ScrollBar3, x) + 4, "B2d_ScrollBar3.y (EE 0x20)");

// A one-item knob (EE 0x1c bytes, vtable 0x341308).
// layout: host -- knob is a pointer; EE offsets in the comments.
class B2d_ScrollBar1 : public ScrollBar
{
public:
    B2d_ScrollBar1(int file, int frame, int knob);
    B2d_ScrollBar1(int file, const char *frame, const char *knob);
    ~B2d_ScrollBar1() override;

    void Draw(short cursor_row, short first_row);       // FF3 0x00136c98

    /* 0x10 */ B2D_ITEM *knob;
    /* 0x14 */ int       x;                 // track origin (the frame item's x / y)
    /* 0x18 */ int       y;
};
static_assert(offsetof(B2d_ScrollBar1, knob) >= offsetof(ScrollBar, length) + sizeof(int), "B2d_ScrollBar1.knob follows ScrollBar (EE 0x10)");
static_assert(offsetof(B2d_ScrollBar1, x) == offsetof(B2d_ScrollBar1, knob) + sizeof(B2D_ITEM *), "B2d_ScrollBar1.x (EE 0x14)");
static_assert(offsetof(B2d_ScrollBar1, y) == offsetof(B2d_ScrollBar1, x) + 4, "B2d_ScrollBar1.y (EE 0x18)");

// The three knob pieces stacked from x, y over len pixels (FF3 0x002acd28).
void B2dScrollBarDrawKnob3(int x, int y, int len, B2D_ITEM **piece, int bright);

#endif /* _SYSTEM_B2D_SCROLLBAR_H */
