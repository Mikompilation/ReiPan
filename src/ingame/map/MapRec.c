// ingame/map/MapRec.c -- CMapRec, the screen rect a Bp layout covers.
// FF3 0x0010e230..0x0010e650; FF3-only (the class file sorts between
// MapDBuff and MarkDat; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/map/MapRec.h"

#include "ingame/photo/m_plyr_camera.h" // MapScreenItemGetDoor, MapSnapItemGetState, MapSnapNameIsRsaRex
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS, B2dGetDrawItem, B2dSet*Func
#include "system/b2d/BpCtl.h"           // BpCtlDrawOne, BpCtlSetHooks, BpGetDrawHandle

// ──────────────────────────────────────────────────────────────────────
// The static one, bss 0x379a10. FF3 0x0010e598 (the file's
// __static_initialization_and_destruction_0, MapRecStaticInit) constructs
// it at start-up (FF3 0x0010e610, _GLOBAL__I_) and destroys it at exit
// (FF3 0x0010e630, _GLOBAL__D_); the C++ static does both here.

static CMapRec map_rec;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e368 (MapRecCtor; FF3 0x0010e388 is the same body).

CMapRec::CMapRec()
{
    Reset();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e3b8 (the deleting one: 0x0010e3c8).

CMapRec::~CMapRec()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e3e8.

void CMapRec::Reset()
{
    empty = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e3f8.

CMapRec *MapRecGet(void)
{
    return &map_rec;
}

// ──────────────────────────────────────────────────────────────────────
// Grows the rect by a sprite at x, y of w x h scaled by sx, sy (0x400 =
// 1.0); the first add after a reset sets it.
// FF3 0x0010e408.

void CMapRec::AddSprite(int x, int y, int w, int h, int sx, int sy)
{
    int x1 = x + w * sx / 1024;
    int y1 = y + h * sy / 1024;

    if (empty != 0)
    {
        rect[0] = x;
        rect[1] = y;
        rect[2] = x1;
        rect[3] = y1;
        empty   = 0;
        return;
    }

    if (x < rect[0])
        rect[0] = x;
    if (y < rect[1])
        rect[1] = y;
    if (rect[2] < x1)
        rect[2] = x1;
    if (rect[3] < y1)
        rect[3] = y1;
}

// ──────────────────────────────────────────────────────────────────────
// Grows the rect by four x, y points; the first add after a reset starts
// from the first point.
// FF3 0x0010e4a0.

void CMapRec::AddQuad(const int *xy)
{
    if (empty != 0)
    {
        rect[0] = xy[0];
        rect[1] = xy[1];
        rect[2] = xy[0];
        rect[3] = xy[1];
        empty   = 0;
    }

    for (int i = 0; i < 4; i++, xy += 2)
    {
        if (xy[0] < rect[0])
            rect[0] = xy[0];
        if (xy[1] < rect[1])
            rect[1] = xy[1];
        if (rect[2] < xy[0])
            rect[2] = xy[0];
        if (rect[3] < xy[1])
            rect[3] = xy[1];
    }
}

// ──────────────────────────────────────────────────────────────────────
// The measuring sprite hook: map items (not the special-name one, not the
// "R sa" / "R ex" items) whose row state is 0..2 grow the static rect.
// Draws nothing.
// FF3 0x0010e230.

static int MapRecSpriteHook(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM *item = B2dGetDrawItem();

    if (MapScreenItemGetDoor(item) >= 0)
        return 0;
    if (MapSnapNameIsRsaRex(item->name) != 0)
        return 0;

    int state = MapSnapItemGetState(BpGetDrawHandle(), item);
    if (state >= 0 && state < 3)
    {
        map_rec.AddSprite((int)args->t1, (int)args->t2, (int)args->a3, (int)args->t0,
                        (int)args->t3, (int)args->stack[0]);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The measuring quad hook: as the sprite one without the row state test.
// FF3 0x0010e300.

static int MapRecQuadHook(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM *item = B2dGetDrawItem();

    if (MapScreenItemGetDoor(item) >= 0)
        return 0;
    if (MapSnapNameIsRsaRex(item->name) != 0)
        return 0;

    map_rec.AddQuad((const int *)args->a1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The measuring draw sets no texture. FF3 0x0010e360.

static int MapRecTexSetHook(const B2D_HOOK_ARGS *args)
{
    (void)args;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Draws the layout through the measuring hooks, then puts BpCtl's own
// hooks back. The hooks grow the static rect whatever rec is (FF3 passes
// MapRecGet's).
// FF3 0x0010e538.

void CMapRec::Measure(int handle)
{
    if (handle < 0)
        return;

    B2dSetSpriteFunc(MapRecSpriteHook);
    B2dSetQuadFunc(MapRecQuadHook);
    B2dSetTexSetFunc(MapRecTexSetHook);
    BpCtlDrawOne(handle);
    BpCtlSetHooks();
}
