/* ==========================================================================
 *  ingame/map/MapRec.h
 *
 *  CMapRec (FF3 RTTI, vtable 0x341038): the screen rectangle a Bp layout
 *  covers, measured by drawing it once through B2d hooks that only grow
 *  the rectangle (FF3 0x0010e230..0x0010e650; FF3-only, no FF2 file). The
 *  map menu's CMarkDat (MarkDat.h) measures its floor layouts with it.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPREC_H
#define _INGAME_MAP_MAPREC_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// 0x28 bytes on the EE (bss 0x379a10..0x379a38; the 2D map flags follow).
// layout: host -- the vptr is 4 bytes on the EE: empty is at EE +4, rect +8,
// view +0x18.
class CMapRec
{
public:
    CMapRec();                          // FF3 0x0010e368 (MapRecCtor; twin 0x0010e388)
    virtual ~CMapRec();                 // EE vtable +0x00 0x0010e3b8, deleting +0x04 0x0010e3c8

    void Reset();                       // FF3 0x0010e3e8: empty again
    void AddSprite(int x, int y, int w, int h, int sx, int sy);  // FF3 0x0010e408 (sx, sy: 0x400 = 1.0)
    void AddQuad(const int *xy);        // FF3 0x0010e4a0: four x, y points
    void Measure(int handle);           // FF3 0x0010e538: draw handle through the measuring hooks

    int empty;                          // EE +0x04: 1 until the first add (CMapRec::Reset)
    int rect[4];                        // EE +0x08: x0, y0, x1, y1 (screen, inclusive of every add)
    int view[4];                        // EE +0x18: the map screen's visible window; CMapScreen::Draw writes x0, y0, x1, y1 (open-items: MarkDoor reads x, y, w, h); the
                                        // map draw FF3 0x001f87b0 sets it, the mark draw (CMarkDoor::Draw)
                                        // and the map sprite hook 0x001f7978 clip against it
};
static_assert(offsetof(CMapRec, rect) == offsetof(CMapRec, empty) + 4, "CMapRec.rect follows empty");
static_assert(sizeof(((CMapRec *)nullptr)->rect) == 0x10, "CMapRec.rect is 0x10 bytes");
static_assert(offsetof(CMapRec, view) == offsetof(CMapRec, empty) + 0x14, "CMapRec.view (EE +0x18)");

CMapRec *MapRecGet(void);               // FF3 0x0010e3f8: the static one (bss 0x379a10)

#endif /* _INGAME_MAP_MAPREC_H */
