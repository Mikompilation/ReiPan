// ingame/photo/m_plyr_camera.c -- the map screen's draw hooks, CLUTs and the
// first CMapScreen methods (see m_plyr_camera.h; file name inferred from link
// order). FF3 0x1f74f8..0x1f8230 and the out-of-line rect test 0x2bcf78.
//
// While CMapScreen::Draw (man_data.c) draws the floor layouts, the sprite
// and quad hooks here decide per item: rooms are drawn through one of three
// CLUTs by their visited state (the room under the centre is queued and
// drawn last, MapScreenDrawCurRoom), doors and door-less marks are recorded
// in the screen's CMarkDoor and drawn afterwards in their doors' colours.
// The MAPMENU layout's hooks print the room's area and name and draw its
// picture.

#include "common/reipan_stub.h"
#include "ingame/photo/m_plyr_camera.h"

#include <string.h>                     // strstr, strncmp

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/gs_reg.h"            // GS_REG_SET, SetGsRegCached, SetGsRegsCached
#include "graphics/graph2d/message.h"   // MsgPrintPage
#include "ingame/map/MapRec.h"          // CMapRec, MapRecGet
#include "ingame/map/MapSnap.h"         // MAP_SNAP_ROOM, MapSnapItemGetRow, MapSnapCheckItemRect
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC
#include "sdk/libgraph.h"               // SCE_GS_TEST_1, SCE_GS_ZBUF_1, SCE_GS_TEX1_1
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData
#include "system/b2d/BpCtl.h"           // BpCtlLoadReq, BpGetDrawHandle, B2dDrawSpriteDirect
#include "system/b2d/BpTex.h"           // BpTexSetFunc, BpTexSetClut
#include "system/eeiop/cd_file_dat.h"   // MAPBG_PHF, MAPMENU_PHF, MAPCL_*_CL2
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage

// ──────────────────────────────────────────────────────────────────────
// The draw state (m_plyr_camera.h).

int            map_screen_spr_num;          // sdata 0x370fd0
MENU_PIC      *map_screen_pic;              // sdata 0x370fd4
B2D_ITEM      *map_screen_bg_item;          // sbss 0x37350c
MAP_SNAP_ROOM *map_screen_row;              // sbss 0x373510
int            map_screen_floor;            // sbss 0x373514
int            map_screen_layer;            // sbss 0x373518
CMapScreen    *map_screen_drawn;            // sbss 0x37351c
MAP_SCREEN_SPR map_screen_spr[MAP_SCREEN_SPR_MAX];   // bss 0x3f16f8

// The three CLUT files, data 0x32a6a8 ({file, buffer, load id}; 0xc bytes
// each on the EE).
// layout: host -- addr is a pointer.
struct MAP_SCREEN_CLUT
{
    int   file;
    void *addr;                         // system heap; nullptr when not loaded
    int   id;                           // LoadReq id, -1 none
};

static MAP_SCREEN_CLUT map_screen_clut[3] =
{
    { MAPCL_TOUHA_CL2,    nullptr, -1 },    // explored rooms
    { MAPCL_MITOUHA_CL2,  nullptr, -1 },    // rooms not explored
    { MAPCL_GENZAITI_CL2, nullptr, -1 },    // the current room
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f74f8: CMapLayer::Draw names the layout it draws next.

void MapSnapSetDrawLayer(int floor, int layer)
{
    map_screen_floor = floor;
    map_screen_layer = layer;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f7508.

int *MapRecGetRect(void)
{
    return MapRecGet()->rect;
}

// ──────────────────────────────────────────────────────────────────────
// A door item carries its door mark id in the frames word of its X part's
// first key; a "door" item without one is 0 (sdata 0x370fd8), any other
// item -1.
// FF3 0x001f7528.

int MapScreenItemGetDoor(B2D_ITEM *item)
{
    const B2D_KEY *key = item->part[B2D_PART_X].keys;

    if (key == nullptr)
        return -1;
    if (key->frames != 0)
        return key->frames;
    if (item->name != nullptr && strstr(item->name, "door") != nullptr)
        return 0;
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// A door is marked once: by the first layout that draws it.
// FF3 0x001f7580.

int MapScreenDoorCanAdd(int mark)
{
    MARK_DOOR_REC *rec = map_screen_drawn->door.Find(mark);

    if (rec == nullptr)
        return 1;
    return rec->layer == map_screen_layer;
}

// ──────────────────────────────────────────────────────────────────────
// How the sprite hook draws a room item (MAP_SCREEN_ROOM_*); handle is not
// read. FF3 0x001f75c0.

int MapSnapItemGetState(int handle, B2D_ITEM *item)
{
    (void)handle;

    MAP_SNAP_ROOM *row = MapSnapItemGetRow(item);

    if (row == nullptr)
        return MAP_SCREEN_ROOM_NONE;
    if ((row->flags & MAP_SNAP_FLG_VISITED) == 0)
        return MAP_SCREEN_ROOM_HIDDEN;
    if (row->flags & MAP_SNAP_FLG_01)
        return (row == map_screen_row) ? MAP_SCREEN_ROOM_CUR : MAP_SCREEN_ROOM_DONE;
    if (row->flags & MAP_SNAP_FLG_02)
        return MAP_SCREEN_ROOM_PART;
    return MAP_SCREEN_ROOM_HIDDEN;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f7628.

void MapScreenSetClut(int handle, int tex, int clut)
{
    BpTexSetClut(handle, tex, map_screen_clut[clut].addr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f7658 (init_StoryMenu_Map).

void MapScreenClutLoad(void)
{
    for (int i = 0; i < 3; i++)
    {
        MAP_SCREEN_CLUT *clut = &map_screen_clut[i];

        clut->addr = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(clut->file), 0, 0, 0);
        clut->id   = LoadReq(clut->file, clut->addr);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f76e8.

int MapScreenClutIsLoaded(void)
{
    for (int i = 0; i < 3; i++)
    {
        if (!IsLoadEnd(map_screen_clut[i].id))
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f7748 (StoryMapRelease).

void MapScreenClutFree(void)
{
    for (int i = 0; i < 3; i++)
    {
        MAP_SCREEN_CLUT *clut = &map_screen_clut[i];

        if (clut->addr != nullptr)
        {
            heapCtrlFree(GetSystemHeapWrkP(), clut->addr, 0, 0, 0);
            clut->addr = nullptr;
        }
        clut->id = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The current room's sprites, last queued first, through the GENZAITI
// CLUT with the alpha test GEQUAL 0 and the depth test GREATER on; then
// both back to ALWAYS and the z writes off.
// FF3 0x001f77c8 (CMapScreen::Draw, after the floor layers).

int MapScreenDrawCurRoom(void)
{
    if (map_screen_spr_num <= 0)
        return 0;

    int           handle = map_screen_spr[0].handle;
    int           tex    = map_screen_spr[0].tex;
    B2D_HOOK_ARGS set    = {};

    // The texture-set hook's words: a0 handle, a1 texture.
    set.a0 = handle;
    set.a1 = tex;
    BpTexSetFunc(&set);
    MapScreenSetClut(handle, tex, 2);

    GS_REG_SET regs[2] =
    {
        { 0x7000d,   SCE_GS_TEST_1 },
        { 0xa000118, SCE_GS_ZBUF_1 },
    };
    SetGsRegsCached(regs, 2, 1);

    for (int i = map_screen_spr_num - 1; i >= 0; i--)
    {
        const MAP_SCREEN_SPR *spr = &map_screen_spr[i];

        if (spr->handle != handle || spr->tex != tex)
        {
            handle = spr->handle;
            tex    = spr->tex;
            set.a0 = handle;
            set.a1 = tex;
            BpTexSetFunc(&set);
            MapScreenSetClut(handle, tex, 2);
        }

        B2D_HOOK_ARGS draw = {};

        draw.a0       = spr->tex;
        draw.a1       = spr->u;
        draw.a2       = spr->v;
        draw.a3       = spr->w;
        draw.t0       = spr->h;
        draw.t1       = spr->x;
        draw.t2       = spr->y;
        draw.t3       = spr->sx;
        draw.stack[0] = spr->sy;
        draw.stack[1] = spr->alpha;
        draw.stack[2] = spr->rgb;
        draw.stack[3] = spr->flags;
        B2dDrawSpriteDirect(&draw);
    }

    SetGsRegCached(0x30003, SCE_GS_TEST_1, 1);
    SetGsRegCached(0x10a000118ULL, SCE_GS_ZBUF_1, 1);
    B2dResetCurTex();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f7948 (reads the name's first word).

int MapSnapNameIsRsaRex(const char *name)
{
    if (name == nullptr)                // PC: FF3 reads address 0
        return 0;
    return strncmp(name, "R sa", 4) == 0 || strncmp(name, "R ex", 4) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// The floor layouts' sprite hook (a0 texture, a1 u, a2 v, a3 w, t0 h,
// t1 x, t2 y, t3 sx, stack sy, alpha, rgb, flags). Sprites outside the
// map window and items the events hide are skipped; door items become
// door marks (once per door, inside a visited room); "R sa" / "R ex"
// items only inside a visited room; room items draw by their state.
// FF3 0x001f7978.

int MapScreenSpriteHook(const B2D_HOOK_ARGS *args)
{
    int   tex   = (int)args->a0;
    int   u     = (int)args->a1;
    int   v     = (int)args->a2;
    int   w     = (int)args->a3;
    int   h     = (int)args->t0;
    int   x     = (int)args->t1;
    int   y     = (int)args->t2;
    int   sx    = (int)args->t3;
    int   sy    = (int)args->stack[0];
    int   alpha = (int)args->stack[1];
    u_int rgb   = (u_int)args->stack[2];
    u_int flags = (u_int)args->stack[3];

    // The sprite and the window overlap (the span of both is no wider
    // than the sum).
    const int *view = MapRecGet()->view;
    int        dw   = w * sx / 1024;
    int        dh   = h * sy / 1024;
    int        x0   = view[0] < x ? view[0] : x;
    int        y0   = view[1] < y ? view[1] : y;
    int        x1   = x + dw < view[0] + view[2] ? view[0] + view[2] : x + dw;
    int        y1   = y + dh < view[1] + view[3] ? view[1] + view[3] : y + dh;

    if (view[2] + dw < x1 - x0 || view[3] + dh < y1 - y0)
        return 0;

    B2D_ITEM *item = B2dGetDrawItem();

    if (!MarkEventIsDrawn(map_screen_floor, map_screen_layer, item->id))
        return 0;

    alpha = alpha * B2dSprGetPartData(map_screen_bg_item, B2D_PART_ALPHA) >> 7;

    int door   = MapScreenItemGetDoor(item);
    int handle = BpGetDrawHandle();
    int mark;

    if (door >= 0)
    {
        if (!MapSnapCheckItemRect(item, handle, map_screen_floor, map_screen_layer))
            return 0;
        if (!MapScreenDoorCanAdd(door))
            return 0;
        mark = door;
    }
    else
    {
        if (MapSnapNameIsRsaRex(item->name) &&
            !MapSnapCheckItemRect(item, handle, map_screen_floor, map_screen_layer))
            return 0;

        int state = MapSnapItemGetState(handle, item);

        // An event can put another picture in the item's place.
        int nx = x, ny = y, nu = u, nv = v, nw = w, nh = h;
        if (MarkEventSpecialSprite(map_screen_floor, map_screen_layer, item->id,
                                   &nx, &ny, &nu, &nv, &nw, &nh))
        {
            x = nx;
            y = ny;
            u = nu;
            v = nv;
            w = nw;
            h = nh;
        }

        switch (state)
        {
        case MAP_SCREEN_ROOM_DONE:
        case MAP_SCREEN_ROOM_PART:
        {
            B2D_HOOK_ARGS draw = {};

            MapScreenSetClut(handle, tex, state);
            draw.a0       = tex;
            draw.a1       = u;
            draw.a2       = v;
            draw.a3       = w;
            draw.t0       = h;
            draw.t1       = x;
            draw.t2       = y;
            draw.t3       = sx;
            draw.stack[0] = sy;
            draw.stack[1] = alpha;
            draw.stack[2] = rgb;
            draw.stack[3] = flags;
            B2dDrawSpriteDirect(&draw);
            B2dResetCurTex();
            return 0;
        }

        case MAP_SCREEN_ROOM_CUR:
        {
            // PC: bounded; FF3 has no check and would run into MapObjWrk.
            if (map_screen_spr_num >= MAP_SCREEN_SPR_MAX)
                return 0;

            MAP_SCREEN_SPR *spr = &map_screen_spr[map_screen_spr_num++];

            spr->handle = handle;
            spr->tex    = tex;
            spr->u      = u;
            spr->v      = v;
            spr->w      = w;
            spr->h      = h;
            spr->x      = x;
            spr->y      = y;
            spr->sx     = sx;
            spr->sy     = sy;
            spr->alpha  = alpha;
            spr->rgb    = rgb;
            spr->flags  = flags;
            return 0;
        }

        case MAP_SCREEN_ROOM_NONE:
            mark = MARK_DOOR_NONE;
            break;

        default:
            return 0;
        }
    }

    map_screen_drawn->door.AddSprite(map_screen_layer, mark, handle, tex, u, v, w, h,
                                     x, y, sx, sy, alpha, rgb, flags);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the two rects (x, y, w, h) overlap; the first argument FF3
// passes is not read.
// FF3 0x002bcf78 (out of line; the int twin of MapSnap.c's 0x002bced8).

static int MapScreenRectIsHit(const int *a, const int *b)
{
    int x0 = (a[0] < b[0]) ? a[0] : b[0];
    int x1 = (b[0] + b[2] < a[0] + a[2]) ? a[0] + a[2] : b[0] + b[2];
    int y0 = (a[1] < b[1]) ? a[1] : b[1];
    int y1 = (b[1] + b[3] < a[1] + a[3]) ? a[1] + a[3] : b[1] + b[3];

    return (x1 - x0 <= a[2] + b[2]) && (y1 - y0 <= a[3] + b[3]);
}

// ──────────────────────────────────────────────────────────────────────
// The floor layouts' quad hook (a0 texture, a1 xy[8], a2 uv[8], a3 alpha,
// t0 rgb, t1 flags): every quad in the window that the events show is
// recorded as a mark -- a door's (once per door, inside a visited room)
// or a door-less one; "R sa" / "R ex" items only inside a visited room.
// FF3 0x001f7d70 (Ghidra had it inside 0x001f7978).

int MapScreenQuadHook(const B2D_HOOK_ARGS *args)
{
    int        tex   = (int)args->a0;
    const int *xy    = (const int *)args->a1;
    const int *uv    = (const int *)args->a2;
    int        alpha = (int)args->a3;
    u_int      rgb   = (u_int)args->t0;
    u_int      flags = (u_int)args->t1;
    CMapRec   *map   = MapRecGet();

    // The quad's bounding rect (x, y, w, h).
    int rect[4] = { xy[0], xy[1], xy[0], xy[1] };

    for (int i = 1; i < 4; i++)
    {
        int px = xy[i * 2];
        int py = xy[i * 2 + 1];

        if (rect[2] < px)
            rect[2] = px;
        if (rect[3] < py)
            rect[3] = py;
        if (px < rect[0])
            rect[0] = px;
        if (py < rect[1])
            rect[1] = py;
    }
    rect[2] -= rect[0];
    rect[3] -= rect[1];

    if (!MapScreenRectIsHit(map->view, rect))
        return 0;

    B2D_ITEM *item = B2dGetDrawItem();

    if (!MarkEventIsDrawn(map_screen_floor, map_screen_layer, item->id))
        return 0;

    int handle = BpGetDrawHandle();
    int mark   = MARK_DOOR_NONE;
    int door   = MapScreenItemGetDoor(item);

    if (door < 0)
    {
        if (MapSnapNameIsRsaRex(item->name) &&
            !MapSnapCheckItemRect(item, handle, map_screen_floor, map_screen_layer))
            return 0;
    }
    else
    {
        if (!MapSnapCheckItemRect(item, handle, map_screen_floor, map_screen_layer))
            return 0;
        mark = door;
        if (!MapScreenDoorCanAdd(door))
            return 0;
    }

    alpha = alpha * B2dSprGetPartData(map_screen_bg_item, B2D_PART_ALPHA) >> 7;
    map_screen_drawn->door.AddQuad(map_screen_layer, mark, handle, tex, xy, uv, alpha, rgb, flags);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The MAPMENU hook items (kind 2: t1 xy[8], t3 rgb, stack alpha at [2])
// print for a visited room under the centre.
// FF3 0x001f7f88 (item 0x400): the area name, message (0, row +5).

static int MapScreenAreaNameHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    if (map_screen_row != nullptr && (map_screen_row->flags & MAP_SNAP_FLG_VISITED))
    {
        SetCurColorPacked((u_int)args->t3);
        MsgPrintPage(0, map_screen_row->area_msg, xy[0], xy[1], 0x2d, (int)args->stack[2], 0, 0, 0, 0);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x001f8008 (item 0x402): the room name, message (0x7a, place).

static int MapScreenRoomNameHook(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    if (map_screen_row != nullptr && (map_screen_row->flags & MAP_SNAP_FLG_VISITED))
    {
        SetCurColorPacked((u_int)args->t3);
        MsgPrintPage(0x7a, map_screen_row->place, xy[0], xy[1], 0x2d, (int)args->stack[2], 0, 0, 0, 0);
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x001f8088 (item 0x3fd): the room picture (CMapScreen::Draw picks
// it) at the item's first point, faded with the item; FF3 reads the low
// halves of xy[0] and xy[1].

static int MapScreenRoomPicHook(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    MENU_PIC  *pic = map_screen_pic;

    pic->scale = (float)(int)args->stack[2] * (1.0f / 128.0f);     // sdata 0x370fe0
    pic->pri   = 0xa0;
    pic->x     = (short)xy[0];
    pic->y     = (short)xy[1];
    pic->z     = 0;
    pic->Draw();
    pic->DrawNext();
    SetGsRegCached(0x100000000ULL, SCE_GS_TEX1_1, 1);
    SetDefaultGsDrawState();
    return 0;
}

// rodata 0x35b828.
const B2D_HOOK_REC map_screen_menu_hooks[4] =
{
    { MapScreenAreaNameHook, 0,  0x400, nullptr },
    { MapScreenRoomNameHook, 0,  0x402, nullptr },
    { MapScreenRoomPicHook,  0,  0x3fd, nullptr },
    { nullptr,               -1, -1,    nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8168 (FF3 0x001f8118 is the same body): the two layers, the
// door and event marks, then the fields.
// callcheck: MapLayerCtor, MarkDoorCtor, MarkEventCtor (the members' constructors, run implicitly)

CMapScreen::CMapScreen()
{
    help = 0;
    cur = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f81b8: the back layer is shown now; its room comes with the
// next draw.

void CMapScreen::SwapLayer()
{
    cur ^= 1;
    map_screen_row = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f81d0: the frame and the language's menu layout.

void CMapScreen::LoadReq()
{
    frame = BpCtlLoadReq(MAPBG_PHF, nullptr);
    menu  = BpCtlLoadReq(MAPMENU_PHF + GetSysWrk()->GetLanguage(), nullptr);
}
