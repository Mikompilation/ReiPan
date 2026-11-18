/* ==========================================================================
 *  system/b2d/B2dGridSpr.h
 *
 *  A layout item moved on a grid (FF3 0x130638..0x130950, FF3-only): the
 *  item's layout position is the grid origin, SetPos puts it at
 *  column / row times the step. Menus use it for cursors (the save / load
 *  screens, BpObj's cursor list, FurnCtl, the camera edit menu). A C++ class
 *  in FF3 (operator new'd, both constructor copies emitted), so members
 *  here.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DGRIDSPR_H
#define _SYSTEM_B2D_B2DGRIDSPR_H

#include <stddef.h>                     // offsetof

#include "system/b2d/B2dCtl.h"          // B2D_ITEM

// EE 0xc bytes (operator new(0xc)). layout: host -- item is a pointer.
struct B2D_GRID_SPR
{
    B2D_GRID_SPR(int file, const char *name, short dx, short dy);   // FF3 0x00130638 (B2dGridSprCtorByName; copy 0x001306e0): the item by name
    B2D_GRID_SPR(int file, int id, short dx, short dy);             // FF3 0x00130830 (B2dGridSprCtor; copy 0x00130788): the item by id
    ~B2D_GRID_SPR();                                                // FF3 0x001308e0 (B2dGridSprDtor): empty
    void Init(short dx, short dy);                                  // FF3 0x002aca58 (B2dGridSprInit, inline, emitted out of line)
    void SetDisp(int disp);                                         // FF3 0x001308e8 (B2dGridSprSetDisp)
    void SetPos(short col, short row);                              // FF3 0x00130908 (B2dGridSprSetPos)

    /* 0x00 */ B2D_ITEM *item;
    /* 0x04 */ short     x;             // part 6 at construction
    /* 0x06 */ short     y;             // part 7 at construction
    /* 0x08 */ short     dx;            // column step
    /* 0x0a */ short     dy;            // row step
};
static_assert(offsetof(B2D_GRID_SPR, x) == sizeof(B2D_ITEM *), "B2D_GRID_SPR.x follows the item");
static_assert(offsetof(B2D_GRID_SPR, dy) - offsetof(B2D_GRID_SPR, x) == 6, "B2D_GRID_SPR shorts x, y, dx, dy");

#endif /* _SYSTEM_B2D_B2DGRIDSPR_H */
