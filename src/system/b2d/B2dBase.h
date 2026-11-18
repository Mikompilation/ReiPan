/* ==========================================================================
 *  system/b2d/B2dBase.h
 *
 *  CB2dBase (FF3 RTTI, vtable 0x340f48): one Bp layout handle with the
 *  common load / play / wait calls (FF3 0x00108f78..0x00109250), then the
 *  menu row highlight set (B2D_SEL) and item placement helpers (B2D_PLACE)
 *  up to 0x00109728. FF3-only (no FF2 counterpart). CGameClear and
 *  CmenuCamMain own one; CmenuCamBase derives from it.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DBASE_H
#define _SYSTEM_B2D_B2DBASE_H

#include "sdk/scetypes.h"

#include "system/b2d/B2dCtl.h"          // B2D_ITEM

// 8 bytes on the EE: the vptr, then the BpCtl handle (-1 without a layout).
// layout: host -- the vptr is 8 bytes on 64-bit hosts, so handle sits at
// +8 there; nothing reads it by offset.
class CB2dBase
{
public:
    CB2dBase();                         // FF3 0x00108f90 (B2dBaseCtor; base-object twin 0x00108f78 B2dBaseCtorBase)
    virtual ~CB2dBase();                // EE vtable +0x00 0x00108fd0 (base 0x00108fa8), deleting +0x04 0x00108ff8

    void Load(int file);                // FF3 0x00109030: releases the old layout, BpCtlLoadReq(file, nullptr)
    void LoadLast(int file);            // FF3 0x00109070: as Load with BpCtlLoadReqLast
    int IsReady();                      // FF3 0x001090b0: BpCtlIsDataReady != 0 (1 without a layout)
    void Release();                     // FF3 0x001090e8: BpCtlRelease, handle -1
    int IsPlayEnd();                    // FF3 0x00109130: the longest part stopped (1 without a layout)
    void SetAlpha(int alpha);           // FF3 0x00109170: B2dCtlSetAlpha when it has a layout
    void SetPos(int x, int y);          // FF3 0x001091a0: B2dCtlSetPos when it has a layout
    int Play(const char *label);        // FF3 0x001091d0: BpPlayLabel; 1
    int PlayEx(const char *label);      // FF3 0x001091f0: BpPlayLabelEx(handle, label, 0); 1
    B2D_ITEM *FindItem(int id);         // FF3 0x00109218: nullptr without a layout

    int handle;                         // EE +0x04: BpCtl handle, -1 without a layout
};

// ---- the row highlight set (FF3 0x00109250..0x00109410) ----
// Up to ten items, one per menu row; the selected one takes the alpha
// (part 10) of a cursor item, the others 0. CmenuCamStat keeps one.
#define B2D_SEL_MAX 10

// layout: host -- 0xc bytes on the EE (item a pointer).
struct B2D_SEL_ENT
{
    int       alpha;                    // EE +0x00: 0x80 when added, -1 cleared (nothing reads it)
    int       unk_04;                   // EE +0x04: -1 (nothing sets it otherwise)
    B2D_ITEM *item;                     // EE +0x08
};

// 0x84 bytes on the EE. C++ in FF3 (a C1/C2 constructor pair; CmenuCamStat's
// constructors build its member with the complete one).
// layout: host -- item pointers; EE: csr +0x04, num +0x08, ent[] +0x0c (0xc each).
struct B2D_SEL
{
    B2D_SEL();                          // FF3 0x00109288 (B2dSelInit; base-object twin 0x00109250 B2dSelInitTwin): entries -1 / -1 / nullptr, none, cursor nullptr
    void Add(int handle, int id);       // FF3 0x001092c0 (B2dSelAdd): also freezes the item's part 10
    void SetCsr(int handle, int id);    // FF3 0x00109350 (B2dSelSetCsr)
    void Set(int n);                    // FF3 0x00109380 (B2dSelSet): cur = n, Update
    void Update();                      // FF3 0x00109398 (B2dSelUpdate): per frame

    int         cur;                    // EE +0x00: selected entry
    B2D_ITEM   *csr;                    // EE +0x04: the item whose alpha the selected entry copies
    int         num;                    // EE +0x08
    B2D_SEL_ENT ent[B2D_SEL_MAX];       // EE +0x0c
};

// ---- item positions of a layout (FF3 0x00109410..0x001096a0) ----
// Parts 6 / 7 of an item, plus an offset that MoveItem adds. C++ in FF3 (a
// C1/C2 constructor pair); callers build one on the stack per layout handle.
struct B2D_PLACE
{
    B2D_PLACE(int handle);              // FF3 0x00109420 (B2dPlaceInit; base-object twin 0x00109410 B2dPlaceInitTwin): offset 0
    void MoveItem(int ref, int id);                     // FF3 0x00109430 (B2dPlaceMoveItem): item id to item ref's parts 6 / 7 + offset
    void MoveItemByName(int ref, const char *name);     // FF3 0x001094b8 (B2dPlaceMoveItemByName)
    void MoveItemByNames(const char *ref, const char *name);    // FF3 0x00109500 (B2dPlaceMoveItemByNames, unreferenced)
    void GetXY(int id, int *xy);                        // FF3 0x00109560 (B2dPlaceGetXY): parts 6, 7 of item id (no offset)
    void GetXYByName(const char *name, int *xy);        // FF3 0x001095b8 (B2dPlaceGetXYByName)
    void SetXYByName(const char *name, int x, int y);   // FF3 0x00109610 (B2dPlaceSetXYByName)
    int  GetW(int id);                                  // FF3 0x00109650 (B2dPlaceGetW): part 3 of item id
    int  GetH(int id);                                  // FF3 0x00109678 (B2dPlaceGetH): part 4 of item id (unreferenced)

    /* 0x00 */ int handle;
    /* 0x04 */ int dx;
    /* 0x08 */ int dy;
};
static_assert(sizeof(B2D_PLACE) == 0xc, "B2D_PLACE is 0xc bytes");

// B2dNumSetFont(handle, prefix, nullptr), B2dNumSetTex, B2dNumDraw, then
// back to the plain GS registers and no current texture. FF3 0x001096a0.
void B2dNumDrawFont(int value, int handle, const char *prefix, int x, int y, u_char alpha, int digits);

#endif /* _SYSTEM_B2D_B2DBASE_H */
