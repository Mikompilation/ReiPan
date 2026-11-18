// system/b2d/B2dBase.c -- CB2dBase (FF3 RTTI, vtable 0x340f48) and the
// B2d menu helpers after it (FF3 0x108f78..0x109728). FF3-only, no FF2
// counterpart: written from the FF3 disassembly. CB2dBase wraps one Bp
// layout handle; B2D_SEL highlights one menu row by copying a cursor item's
// alpha; B2D_PLACE reads and moves items by their parts 6 / 7 (x / y).
// GCC 3 emitted each constructor twice (C1/C2: 0x109250 / 0x109288,
// 0x109410 / 0x109420); one body each here.

#include "system/b2d/B2dBase.h"
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem*, B2dCtlSetAlpha / SetPos, B2dResetCurTex
#include "system/b2d/B2dSpr.h"          // B2dSprFreezePart, B2dSprSetPos / SetAlpha, B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtlLoadReq*, BpCtlRelease, BpPlayLabel*, B2dNum*

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108f78 (B2dBaseCtorBase) is the base-object twin with the same body.
// FF3 0x00108f90 (B2dBaseCtor).

CB2dBase::CB2dBase()
{
    handle = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00108fa8 (B2dBaseDtorBase) is the base-object twin with the same
// body; FF3 0x00108ff8 (B2dBaseDtorDelete) adds operator delete.
// FF3 0x00108fd0 (B2dBaseDtor).

CB2dBase::~CB2dBase()
{
    Release();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00109030

void CB2dBase::Load(int file)
{
    Release();
    handle = BpCtlLoadReq(file, nullptr);
}

// FF3 0x00109070

void CB2dBase::LoadLast(int file)
{
    Release();
    handle = BpCtlLoadReqLast(file, nullptr);
}

// FF3 0x001090b0: BpCtlIsDataReady's -1 (no entry) counts as ready too.

int CB2dBase::IsReady()
{
    if (handle == -1)
        return 1;

    return BpCtlIsDataReady(handle) != 0;
}

// FF3 0x001090e8

void CB2dBase::Release()
{
    if (handle != -1)
    {
        BpCtlRelease(handle);
        handle = -1;
    }
}

// FF3 0x00109130

int CB2dBase::IsPlayEnd()
{
    if (handle == -1)
        return 1;

    return B2dPartIsPlaying(B2dCtlGetLongestPart(handle)) == 0;
}

// FF3 0x00109170

void CB2dBase::SetAlpha(int alpha)
{
    if (handle != -1)
        B2dCtlSetAlpha(handle, alpha);
}

// FF3 0x001091a0

void CB2dBase::SetPos(int x, int y)
{
    if (handle != -1)
        B2dCtlSetPos(handle, x, y);
}

// FF3 0x001091d0: no handle check (BpPlayLabel answers -1 itself).

int CB2dBase::Play(const char *label)
{
    BpPlayLabel(handle, label);
    return 1;
}

// FF3 0x001091f0

int CB2dBase::PlayEx(const char *label)
{
    BpPlayLabelEx(handle, label, 0);
    return 1;
}

// FF3 0x00109218

B2D_ITEM *CB2dBase::FindItem(int id)
{
    if (handle == -1)
        return nullptr;

    return B2dCtlFindItem(handle, id);
}

// ──────────────────────────────────────────────────────────────────────
// The row highlight set.

// FF3 0x00109250 (B2dSelInitTwin) is the base-object twin with the same
// body; nothing calls it.
// FF3 0x00109288 (B2dSelInit): cur is left as it is.

B2D_SEL::B2D_SEL()
{
    for (int i = 0; i < B2D_SEL_MAX; i++)
    {
        ent[i].item   = nullptr;
        ent[i].alpha  = -1;
        ent[i].unk_04 = -1;
    }
    csr = nullptr;
    num = 0;
}

// FF3 0x001092c0: no bound check against B2D_SEL_MAX.

void B2D_SEL::Add(int handle, int id)
{
    B2D_SEL_ENT *e    = &ent[num];
    B2D_ITEM    *item = B2dCtlFindItem(handle, id);

    e->alpha = 0x80;
    e->item  = item;
    B2dSprFreezePart(handle, id, 10, 0);
    num++;
}

// FF3 0x00109350

void B2D_SEL::SetCsr(int handle, int id)
{
    csr = B2dCtlFindItem(handle, id);
}

// FF3 0x00109380

void B2D_SEL::Set(int n)
{
    cur = n;
    Update();
}

// FF3 0x00109398: the selected row takes the cursor item's alpha (part
// 10), the others 0.

void B2D_SEL::Update()
{
    for (int i = 0; i < num; i++)
    {
        int alpha = 0;
        if (i == cur)
            alpha = B2dSprGetPartData(csr, 10);
        B2dSprSetAlpha(ent[i].item, alpha);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Item placement.

// FF3 0x00109410 (B2dPlaceInitTwin) is the base-object twin with the same
// body; nothing calls it.
// FF3 0x00109420 (B2dPlaceInit).

B2D_PLACE::B2D_PLACE(int handle)
{
    this->handle = handle;
    dy = 0;
    dx = 0;
}

// FF3 0x00109430: item id goes where item ref is, plus the offset; its
// x / y parts are frozen first so the layout's animation keeps off them.

void B2D_PLACE::MoveItem(int ref, int id)
{
    int xy[2];

    GetXY(ref, xy);
    B2dSprFreezePart(handle, id, 6, 0);
    B2dSprFreezePart(handle, id, 7, 0);
    B2dSprSetPos(B2dCtlFindItem(handle, id), xy[0] + dx, xy[1] + dy);
}

// FF3 0x001094b8

void B2D_PLACE::MoveItemByName(int ref, const char *name)
{
    B2D_ITEM *item = B2dCtlFindItemByName(handle, name);

    MoveItem(ref, item->id);
}

// FF3 0x00109500: nothing calls it.

void B2D_PLACE::MoveItemByNames(const char *ref, const char *name)
{
    B2D_ITEM *item = B2dCtlFindItemByName(handle, name);
    B2D_ITEM *refitem = B2dCtlFindItemByName(handle, ref);

    MoveItem(refitem->id, item->id);
}

// FF3 0x00109560

void B2D_PLACE::GetXY(int id, int *xy)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    xy[0] = B2dSprGetPartData(item, 6);
    xy[1] = B2dSprGetPartData(item, 7);
}

// FF3 0x001095b8

void B2D_PLACE::GetXYByName(const char *name, int *xy)
{
    B2D_ITEM *item = B2dCtlFindItemByName(handle, name);

    xy[0] = B2dSprGetPartData(item, 6);
    xy[1] = B2dSprGetPartData(item, 7);
}

// FF3 0x00109610: no freeze (unlike MoveItem) and no offset.

void B2D_PLACE::SetXYByName(const char *name, int x, int y)
{
    B2dSprSetPos(B2dCtlFindItemByName(handle, name), x, y);
}

// FF3 0x00109650

int B2D_PLACE::GetW(int id)
{
    return B2dSprGetPartData(B2dCtlFindItem(handle, id), 3);
}

// FF3 0x00109678: nothing calls it.

int B2D_PLACE::GetH(int id)
{
    return B2dSprGetPartData(B2dCtlFindItem(handle, id), 4);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001096a0: a number in a layout's digit glyphs (prefix'0'..'9'),
// right-aligned at x; then back to the plain GS registers.

void B2dNumDrawFont(int value, int handle, const char *prefix, int x, int y, u_char alpha, int digits)
{
    B2dNumSetFont(handle, prefix, nullptr);
    B2dNumSetTex();
    B2dNumDraw(value, x, y, alpha, digits);
    BpSetDrawGsRegs();
    B2dResetCurTex();
}
