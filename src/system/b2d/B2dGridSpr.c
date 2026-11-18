/* ==========================================================================
 *  system/b2d/B2dGridSpr.c
 *
 *  A layout item moved on a grid (see B2dGridSpr.h). FF3 0x130638..0x130950
 *  plus the inline initialiser FF3 emitted at 0x2aca58.
 * ======================================================================== */
#include "system/b2d/B2dGridSpr.h"

#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData, B2dSprFreezePart*, B2dSprSetDisp, B2dSprSetPos
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo

// Origin from the item's position parts, step from the arguments.
// FF3 0x002aca58.
void B2D_GRID_SPR::Init(short dx, short dy)
{
    x  = (short)B2dSprGetPartData(item, B2D_PART_X);
    y  = (short)B2dSprGetPartData(item, B2D_PART_Y);
    this->dx = dx;
    this->dy = dy;
}

// The item by name; its position parts are frozen so the layout's keys no
// longer move it. FF3 0x00130638; 0x001306e0 is the same constructor's
// second copy.
B2D_GRID_SPR::B2D_GRID_SPR(int file, const char *name, short dx, short dy)
{
    int handle = BpCtlGetHandleByFileNo(file);

    item = B2dCtlFindItemByName(handle, name);
    Init(dx, dy);
    B2dSprFreezePartByName(handle, name, B2D_PART_X, 0);
    B2dSprFreezePartByName(handle, name, B2D_PART_Y, 0);
}

// As the constructor by name, the item by id. FF3 0x00130830; 0x00130788 is
// the second copy.
B2D_GRID_SPR::B2D_GRID_SPR(int file, int id, short dx, short dy)
{
    int handle = BpCtlGetHandleByFileNo(file);

    item = B2dCtlFindItem(handle, id);
    Init(dx, dy);
    B2dSprFreezePart(handle, id, B2D_PART_X, 0);
    B2dSprFreezePart(handle, id, B2D_PART_Y, 0);
}

// FF3 0x001308e0.
B2D_GRID_SPR::~B2D_GRID_SPR()
{
}

// FF3 0x001308e8.
void B2D_GRID_SPR::SetDisp(int disp)
{
    B2dSprSetDisp(item, disp != 0);
}

// FF3 0x00130908.
void B2D_GRID_SPR::SetPos(short col, short row)
{
    B2dSprSetPos(item, x + dx * col, y + dy * row);
}
