// system/b2d/B2dSpr.c -- FF3's B2d sprite parts (FF3 0x105998..0x106370;
// assert string "B2dSpr.c" from 0x105f78). No FF2 counterpart: written from
// the FF3 decompilation. The sine tables and the freeze functions before the
// first assert have no string of their own; they are kept here, the file
// between B2dInp.c and BpCtl.c (tus.json "why").

#include <math.h>                       // sin, cos
#include <string.h>                     // strcmp

#include "system/b2d/B2dSpr.h"
#include "system/b2d/B2dCtl.h"          // B2dCtlGetItemList, B2dErrPrint

int b2d_sin_tbl[B2D_TRIG_TBL];          // 0x375730
int b2d_cos_tbl[B2D_TRIG_TBL];          // 0x376730

static const char b2d_spr_file[] = "B2dSpr.c";  // rodata 0x34bec8

// Parts B2dSprFreezeItem and friends freeze: 6 (x) to 17 (rgb).
#define B2D_FREEZE_FIRST    B2D_PART_X

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105998.

int B2dSin(int angle)
{
    return b2d_sin_tbl[angle & (B2D_TRIG_TBL - 1)];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001059b8.

int B2dCos(int angle)
{
    return b2d_cos_tbl[angle & (B2D_TRIG_TBL - 1)];
}

// ──────────────────────────────────────────────────────────────────────
// Rotates n x/y pairs around (cx, cy) by `deg` degrees, 12-bit fixed point
// with the shifts rounding toward zero (B2dItemGetQuad).
// FF3 0x001059d8.

void B2dRotatePoints(int *xy, int n, int cx, int cy, int deg)
{
    if (deg == 0)
        return;

    int a = (deg << 10) / 360;
    int s = B2dSin(a);
    int c = B2dCos(a);

    for (; n > 0; n--, xy += 2)
    {
        int dx = xy[0] - cx;
        int dy = xy[1] - cy;

        xy[0] = (dx * c - dy * s) / 0x1000 + cx;
        xy[1] = (dx * s + dy * c) / 0x1000 + cy;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Fills both tables: entry i is sin / cos(i * 2pi / 1024) * 4096, truncated.
// PC: the host libm may differ from newlib's in the last bit, which could move
// an entry that lands exactly on an integer by one.
// callcheck: litodp, __muldf3, __fixdfsi (the EE's soft-float helpers for the
// double maths and casts below)
// FF3 0x00105ad0.

void B2dMakeSinCosTbl(int *sin_tbl, int *cos_tbl)
{
    const double step = 0x1.921fb54411744p-8;   // sdata 0x36b560: 2pi / 1024 (from a rounded pi)

    for (int i = 0; i < B2D_TRIG_TBL; i++)
    {
        double a = step * (double)i;

        sin_tbl[i] = (int)(sin(a) * 4096.0);
        cos_tbl[i] = (int)(cos(a) * 4096.0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105bb8.

void B2dTrigInit(void)
{
    B2dMakeSinCosTbl(b2d_sin_tbl, b2d_cos_tbl);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105be0.

void B2dTrigEnd(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Mode 0 sets the frozen bit, 1 clears it, other modes leave it.

static void B2dPartFreeze(B2D_PART *part, int mode)
{
    if (mode == 0)
        part->flags |= 1;
    else if (mode == 1)
        part->flags &= ~1;
}

static void B2dItemFreeze(B2D_ITEM *item, int mode)
{
    for (int i = B2D_FREEZE_FIRST; i < B2D_ITEM_PARTS; i++)
        B2dPartFreeze(&item->part[i], mode);
}

// FF3 compares a null name as "" (sdata 0x36b568).
static const char *B2dItemName(const B2D_ITEM *item)
{
    return (item->name != nullptr) ? item->name : "";
}

// ──────────────────────────────────────────────────────────────────────
// Freezes (mode 0) or thaws (mode 1) part `part` of every item `id`. FF3
// does not check `part`. FF3 0x00105be8.

int B2dSprFreezePart(int handle, int id, int part, int mode)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (item == nullptr)
        return -1;
    for (; item != nullptr; item = item->next)
    {
        if (item->id == id)
            B2dPartFreeze(&item->part[part], mode);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105ca8.

int B2dSprFreezePartByName(int handle, const char *name, int part, int mode)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (item == nullptr)
        return -1;
    for (; item != nullptr; item = item->next)
    {
        if (strcmp(B2dItemName(item), name) == 0)
            B2dPartFreeze(&item->part[part], mode);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105da0.

int B2dSprFreezeItem(int handle, int id, int mode)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (item == nullptr)
        return -1;
    for (; item != nullptr; item = item->next)
    {
        if (item->id == id)
            B2dItemFreeze(item, mode);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// No caller in FF3 retail. FF3 0x00105e38.

int B2dSprFreezeItemByName(int handle, const char *name, int mode)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (item == nullptr)
        return -1;
    for (; item != nullptr; item = item->next)
    {
        if (strcmp(B2dItemName(item), name) == 0)
            B2dItemFreeze(item, mode);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// No caller in FF3 retail. FF3 0x00105ef8.

int B2dSprFreezeAll(int handle, int mode)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (item == nullptr)
        return -1;
    for (; item != nullptr; item = item->next)
        B2dItemFreeze(item, mode);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// disp 0 hides the item (kind bit 0x10, B2dCtlDraw skips it and its
// children), 1 shows it. FF3 0x00105f78.

int B2dSprSetDisp(B2D_ITEM *item, int disp)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0x81, 0);
        return -1;
    }
    if (disp == 0)
        item->kind |= 0x10;
    else if (disp == 1)
        item->kind &= ~0x10;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00105fe0.

void B2dSprSetPos(B2D_ITEM *item, int x, int y)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xa7, 0);
        return;
    }
    item->part[B2D_PART_Y].value = y;
    item->part[B2D_PART_X].value = x;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106028.

void B2dSprSetScale(B2D_ITEM *item, int sx, int sy)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xb0, 0);
        return;
    }
    item->part[B2D_PART_SY].value = sy;
    item->part[B2D_PART_SX].value = sx;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106070.

void B2dSprSetAlpha(B2D_ITEM *item, int alpha)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xb8, 0);
        return;
    }
    item->part[B2D_PART_ALPHA].value = alpha;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001060b0.

void B2dSprSetText(B2D_ITEM *item, int text)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xc0, 0);
        return;
    }
    item->part[B2D_PART_TEXT].value = text;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001060f0.

void B2dSprSetRepeat(B2D_ITEM *item, int x, int y)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xc9, 0);
        return;
    }
    item->part[B2D_PART_REPY].value = y;
    item->part[B2D_PART_REPX].value = x;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106138.

void B2dSprSetAnchor(B2D_ITEM *item, int x, int y)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xd2, 0);
        return;
    }
    item->part[B2D_PART_ANCHORY].value = y;
    item->part[B2D_PART_ANCHORX].value = x;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106180.

void B2dSprSetRot(B2D_ITEM *item, int deg)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xda, 0);
        return;
    }
    item->part[B2D_PART_ROT].value = deg;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001061c0.

void B2dSprSetRgb(B2D_ITEM *item, u_int r, u_int g, u_int b)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0xf4, 0);
        return;
    }
    item->part[B2D_PART_RGB].value = (int)((r & 0xff) | (g & 0xff) << 8 | (b & 0xff) << 16 | 0x80000000);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00106258.

int B2dSprGetPartData(B2D_ITEM *item, u_int no)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0x100, 0);
        return -1;
    }
    if (no >= B2D_ITEM_PARTS)
    {
        B2dErrPrint(-2, b2d_spr_file, 0x104, 0);
        return -2;
    }
    return item->part[no].value;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001062d0.

u_short B2dSprGetKind(B2D_ITEM *item)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0x111, 0);
        return 0;
    }
    return item->kind & 0xf;
}

// ──────────────────────────────────────────────────────────────────────
// The item's parent (part 5 holds its id), nullptr without one.
// FF3 0x00106320 (no Ghidra function before 2026-10-20: inside B2dSprGetKind).

B2D_ITEM *B2dSprGetParent(int handle, B2D_ITEM *item)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, b2d_spr_file, 0x11e, 0);
        return nullptr;
    }
    int parent = item->part[B2D_PART_PARENT].value;
    if (parent == -1)
        return nullptr;
    return B2dCtlFindItem(handle, parent);
}
