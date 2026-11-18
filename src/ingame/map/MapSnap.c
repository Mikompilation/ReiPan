// ingame/map/MapSnap.c -- where the player is on the in-game 2D map (FF3-only,
// 0x1f5ba0..0x1f73b8 and 0x2bce40..0x2bcf78; see MapSnap.h).
#include <ctype.h>                      // isdigit
#include <string.h>                     // memset, strstr, strchr

#include "ingame/map/MapSnap.h"
#include "common/heapctrl.h"            // heapCtrlMalloc / heapCtrlFree
#include "graphics/ecb.h"               // EcbGetNum / EcbGetData
#include "ingame/camera/cam_rail.h"     // CamSegCrossXZ
#include "sdk/vu0_intrin.h"              // sceVu0FVECTOR
#include "ingame/ingame_wrk.h"          // ingame_wrk (b03 the world, _28 the place, plyr_wrk)
#include "ingame/map/MarkEvent.h"       // MarkEventIsSpecialRoom
#include "ingame/map/MarkDat.h"         // CMarkDat
#include "ingame/map/MapLoad.h"         // MapLoadGetRoomNoNow
#include "ingame/map/hit_check_base.h"  // HcBaseIsInTriXZ
#include "ingame/photo/m_plyr_camera.h" // MapScreenItemGetDoor
#include "ingame/plyr/player.h"         // PLYR_WRK
#include "system/b2d/B2dCtl.h"          // B2D_ITEM, B2dItemGetQuad
#include "system/b2d/B2dSpr.h"
#include "system/b2d/BpCtl.h"           // BpCtlIsDataReady, B2dCtlFindItemIf
#include "system/eeiop/cd_file_dat.h"   // MAP_MENU_ECB, MAP_MENU_STAT_ECB
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static MAP_SNAP_WRK   map_snap_wrk;     // bss 0x3f16d0
static CMarkDat      *map_snap_mark;    // sdata 0x370f94 (gp-0x255c)
static void          *map_snap_room;    // sdata 0x370fb0 (gp-0x2540): map_menu.ecb, MAP_SNAP_ROOM rows
static void          *map_snap_stat;    // sdata 0x370fc8 (gp-0x2528): map_menu_stat.ecb, MAP_SNAP_STAT rows

// A tuning hook on one layer's transform (data 0x32a688, only read here):
// while 'set' is 0 the current transform of layer 'layer' is copied into
// it, otherwise it replaces that transform. Nothing in FF3 sets it.
struct MAP_SNAP_ADJ                     /* 0x18 */
{
    /* 0x00 */ int   set;
    /* 0x04 */ int   layer;
    /* 0x08 */ float ofs_x, ofs_y, scale_x, scale_y;
};
static_assert(sizeof(MAP_SNAP_ADJ) == 0x18, "MAP_SNAP_ADJ is 0x18 bytes");
static MAP_SNAP_ADJ map_snap_adj = { 0, 0, 0.0f, 0.0f, 1.0f, 1.0f };

static MAP_SNAP_ROOM *MapSnapFindRoom(const void *ecb, short room, s_char sub);
static MAP_SNAP_ROOM *MapSnapFindPlace(const void *ecb, s_char area, short place);
static int   MapSnapIsInQuad(const float *p, const float *quad);
static float MapSnapCross2D(const float *a, const float *b, const float *c);
static void  MapSnapPosXZ(float *out, const float *p);
static void  MapSnapRectNormalize(float *rect);
static int   MapSnapRectIsHit(const float *a, const float *b);
static s_char MapSnapGetFreeWorld(void);
static void  MapSnapRoomLoad(void);
static void  MapSnapRoomFree(void);
static void  MapSnapStatLoad(void);
static void  MapSnapStatFree(void);

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f5ba0.

CMarkDat *MapSnapGetMarkDat(void)
{
    return map_snap_mark;
}

// ──────────────────────────────────────────────────────────────────────
// CMarkDat::Measure on the CMarkDat (FUN_001ff1d0's call).
// FF3 0x001f5ba8.

void MapSnapMarkDatDraw(int *rect, int x, int y, int sx, int sy)
{
    map_snap_mark->Measure(rect, x, y, sx, sy);
}

// ──────────────────────────────────────────────────────────────────────
// The first layer of floor that has a layout, -1.
// FF3 0x001f5be8.

int MapSnapGetFirstLayer(int floor)
{
    for (int i = 0; i < CMARK_LAYER_NUM; i++)
    {
        if (map_snap_mark->GetPhfNo(floor, i) >= 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// MAP_UPDATE.
// FF3 0x001f5c48.

void MapSnapSetUpdate(int on)
{
    map_snap_wrk.update = on;
}

// ──────────────────────────────────────────────────────────────────────
// World x / z to layout x / y and back (world = (layout + ofs) * scale).
// FF3 0x001f5c58, 0x001f5c78, 0x001f5c98, 0x001f5cb0.

float MapSnapWorldToMapX(float x, const MAP_SNAP_STAT *stat)
{
    return x / stat->scale_x - stat->ofs_x;
}

float MapSnapWorldToMapY(float z, const MAP_SNAP_STAT *stat)
{
    return z / stat->scale_y - stat->ofs_y;
}

static float MapSnapMapToWorldX(float x, const MAP_SNAP_STAT *stat)
{
    return (x + stat->ofs_x) * stat->scale_x;
}

static float MapSnapMapToWorldZ(float y, const MAP_SNAP_STAT *stat)
{
    return (y + stat->ofs_y) * stat->scale_y;
}

// ──────────────────────────────────────────────────────────────────────
// The world XZ bounding rect (x, z, w, h) of a layout item's quad. Items
// MapScreenItemGetDoor accepts are measured at a fixed scale (x * 500 / 1024,
// y * 950 / 1024 of their own), restored afterwards.
// FF3 0x001f5cc8.

static void MapSnapItemGetWorldRect(float *rect, B2D_ITEM *item, const MAP_SNAP_STAT *stat)
{
    int quad[8];
    int sx = B2dSprGetPartData(item, 8);
    int sy = B2dSprGetPartData(item, 9);

    if (MapScreenItemGetDoor(item) >= 0)
        B2dSprSetScale(item, (sx * 500 + 0x200) / 0x400, (sy * 950 + 0x200) / 0x400);

    B2dItemGetQuad(quad, item, nullptr);

    rect[1] = rect[0] = 2147483520.0f;  /* sdata 0x370f98 */
    rect[3] = rect[2] = -2147483648.0f; /* sdata 0x370f9c */
    for (int i = 0; i < 4; i++)
    {
        float x = MapSnapMapToWorldX((float)quad[i * 2], stat);
        float z = MapSnapMapToWorldZ((float)quad[i * 2 + 1], stat);

        if (x < rect[0])
            rect[0] = x;
        if (rect[2] < x)
            rect[2] = x;
        if (z < rect[1])
            rect[1] = z;
        if (rect[3] < z)
            rect[3] = z;
    }
    rect[2] -= rect[0];
    rect[3] -= rect[1];

    B2dSprSetScale(item, sx, sy);
}

// ──────────────────────────────────────────────────────────────────────
// A world position on layer 'layer' of the current floor in layout space:
// (map x, y, map y, 1). No callers.
// FF3 0x001f5e80.

int MapSnapWorldToMapPos(float *out, const float *pos, int layer)
{
    MAP_SNAP_STAT *stat = MapSnapGetStat(map_snap_wrk.floor, layer);

    out[0] = MapSnapWorldToMapX(pos[0], stat);
    out[1] = pos[1];
    out[2] = MapSnapWorldToMapY(pos[2], stat);
    out[3] = 1.0f;                      /* sdata 0x370fa0 */
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f5f08.

short MapSnapGetCurPlace(void)
{
    short place = 0;

    if (map_snap_wrk.room != nullptr)
        place = map_snap_wrk.room->place;
    return place;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f5f28, 0x001f5f38.

MAP_SNAP_WRK *MapSnapGetWrk(void)
{
    return &map_snap_wrk;
}

int MapSnapGetFloor(void)
{
    return map_snap_wrk.floor;
}

// ──────────────────────────────────────────────────────────────────────
// A file into a fresh system-heap block (size < 1: the file's size); *id
// gets the load id.
// FF3 0x001f5f48.

static void *MapSnapFileLoad(int file, int *id, int size)
{
    int fsize = GetFileSize(file);

    if (size < 1)
        size = fsize;
    void *buf = heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
    int   ret = LoadReq(file, buf);
    if (id != nullptr)
        *id = ret;
    return buf;
}

// ──────────────────────────────────────────────────────────────────────
// B2dCtlFindItemIf predicate of MapSnapUpdate: the item's world rect holds
// (x, z) (two triangles in XZ) and its row is in the current area. The row
// found becomes the current room even when the area differs.
// FF3 0x001f5fc8.

static int MapSnapItemHitPos(B2D_ITEM *item, intptr_t x, intptr_t z)
{
    sceVu0FVECTOR v0, v1, v2, v3, p;
    float         r[4];
    int           hit;

    if (B2dSprGetKind(item) == 3)
        return 0;

    MapSnapItemGetWorldRect(r, item, map_snap_wrk.stat);

    float y = ingame_wrk.plyr_wrk->pos[1];
    v0[0] = r[0];        v0[1] = y; v0[2] = r[1];        v0[3] = 1.0f;  /* sdata 0x370fa4 */
    v1[0] = r[0];        v1[1] = y; v1[2] = r[1] + r[3]; v1[3] = 1.0f;
    v2[0] = r[0] + r[2]; v2[1] = y; v2[2] = r[1] + r[3]; v2[3] = 1.0f;
    v3[0] = r[0] + r[2]; v3[1] = y; v3[2] = r[1];        v3[3] = 1.0f;
    p[0]  = (float)x;    p[1]  = 0.0f; p[2] = (float)z;  p[3] = 1.0f;

    hit = 1;
    if (HcBaseIsInTriXZ(p, v0, v1, v2) == 0)
        hit = HcBaseIsInTriXZ(p, v0, v2, v3) != 0;

    if (!hit)
        return 0;

    map_snap_wrk.room = MapSnapItemGetRow(item);
    if (map_snap_wrk.room == nullptr)
        return 0;
    if (map_snap_wrk.room->area != (s_char)MapLoadGetRoomNoNow())
        return 0;
    return hit;
}

// ──────────────────────────────────────────────────────────────────────
// B2dCtlFindItemIf predicate of MapSnapCheckItemRect: the item's world rect
// meets 'area' and its room is visited. With work 'exact' set (FF3 never
// sets it) 'area' is a quad of four XZ points tested edge by edge; that
// loop tests the quad's own edges against each other and its inner loop
// runs on the outer counter (FF3's code, kept as it is).
// FF3 0x001f6178.

static int MapSnapItemHitRect(B2D_ITEM *item, intptr_t area_arg, intptr_t handle)
{
    float *area = (float *)area_arg;
    float  r[4];
    int    hit;

    (void)handle;
    if (B2dSprGetKind(item) == 3)
        return 0;

    MapSnapItemGetWorldRect(r, item, map_snap_wrk.stat);
    MapSnapRectNormalize(r);

    if (map_snap_wrk.exact == 0)
    {
        hit = MapSnapRectIsHit(r, area);
    }
    else
    {
        float corner[8] = {
            r[0],        r[1],
            r[0],        r[1] + r[3],
            r[0] + r[2], r[1] + r[3],
            r[0] + r[2], r[1],
        };
        sceVu0FVECTOR a, b, c, d, out;

        hit = 0;
        for (int i = 0; i < 4 && !hit; i++)
        {
            MapSnapPosXZ(a, &area[i * 2]);
            MapSnapPosXZ(b, &area[((i + 1) & 3) * 2]);
            for (int j = 0; i < 4; j++)     // FF3: tests i, not j
            {
                MapSnapPosXZ(c, &area[j * 2]);
                MapSnapPosXZ(d, &area[((j + 1) & 3) * 2]);
                if (CamSegCrossXZ(out, a, b, c, d) != 0)
                {
                    hit = 1;
                    break;
                }
            }
        }
        if (!hit)
        {
            hit = 1;
            if (MapSnapIsInQuad(area, corner) == 0)
                hit = MapSnapIsInQuad(corner, area) != 0;
        }
    }

    if (!hit)
        return 0;

    MAP_SNAP_ROOM *room = MapSnapItemGetRow(item);
    if (room == nullptr || (room->flags & MAP_SNAP_FLG_VISITED) == 0)
        return 0;
    if (room->area == 0x2a && (u_char)(room->sub - 1) < 2)
        return MarkEventIsSpecialRoom() == 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the item's world rect (layer 'layer' of floor) meets a visited room
// item on any layer of floor whose measured rect it meets.
// FF3 0x001f6478.

int MapSnapCheckItemRect(B2D_ITEM *item, int arg, int floor, int layer)
{
    float r[4];

    (void)arg;
    MapSnapItemGetWorldRect(r, item, MapSnapGetStat(floor, layer));
    MapSnapRectNormalize(r);

    for (int i = 0; i < CMARK_LAYER_NUM; i++)
    {
        int handle = map_snap_mark->GetHandle(floor, i);
        if (handle < 0 || BpCtlIsDataReady(handle) == 0)
            continue;

        int f = map_snap_mark->GetWorldFloor(floor);
        if (MapSnapRectIsHit(map_snap_mark->layer[f][i].rect, r) == 0)
            continue;

        map_snap_wrk.stat = MapSnapGetStat(floor, i);
        if (B2dCtlFindItemIf(handle, nullptr, MapSnapItemHitRect, (intptr_t)r, handle) != nullptr)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// B2dCtlFindItemIf predicate of MapSnapLayerGetRect: rect grows by every
// room item's world rect. Never stops the search.
// FF3 0x001f65b0.

static int MapSnapItemAddRect(B2D_ITEM *item, intptr_t rect_arg, intptr_t handle)
{
    float *rect = (float *)rect_arg;
    float  r[4];

    (void)handle;
    if (B2dSprGetKind(item) == 3 || MapSnapItemGetRow(item) == nullptr)
        return 0;

    MapSnapItemGetWorldRect(r, item, map_snap_wrk.stat);
    MapSnapRectNormalize(r);

    float x0 = rect[0];
    if (r[0] < x0)
        x0 = r[0];
    float y0 = r[1];
    if (rect[1] <= r[1])
        y0 = rect[1];
    float x1 = r[0] + r[2];
    if (r[0] + r[2] <= rect[0] + rect[2])
        x1 = rect[0] + rect[2];
    rect[2] = x1;
    float y1 = rect[1] + rect[3];
    if (y1 < r[1] + r[3])
        y1 = r[1] + r[3];
    rect[3] = y1;
    rect[0] = x0;
    rect[2] = rect[2] - x0;
    rect[1] = y0;
    rect[3] = y1 - y0;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// CMarkDat's layer rect: the world rect the room items of a loaded layer
// cover (CMarkDat busy meanwhile, so the floor index ignores the world).
// FF3 0x001f66d0.

int MapSnapLayerGetRect(int floor, int layer, float *rect)
{
    int ret;

    map_snap_mark->busy = 1;
    int handle = map_snap_mark->GetHandle(floor, layer);
    if (handle < 0 || BpCtlIsDataReady(handle) == 0)
    {
        ret = 0;
    }
    else
    {
        map_snap_wrk.stat = MapSnapGetStat(floor, layer);
        B2dCtlFindItemIf(handle, nullptr, MapSnapItemAddRect, (intptr_t)rect, handle);
        ret = 1;
    }
    map_snap_mark->busy = 0;
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame (IngameDrawSub): the room item under the player on the
// current floor becomes the current room; with MAP_UPDATE on it gets the
// visited bits and the current world id (a row of another world takes the
// whole world over); ingame_wrk._28 follows its place.
// FF3 0x001f6790.

int MapSnapUpdate(float *pos)
{
    if (map_snap_wrk.floor < 0)
        return 0;

    map_snap_mark->SetupLayers();
    map_snap_wrk.room = nullptr;

    for (int i = 0; i < CMARK_LAYER_NUM; i++)
    {
        int handle = map_snap_mark->GetHandle(map_snap_wrk.floor, i);
        if (handle < 0 || BpCtlIsDataReady(handle) == 0)
            continue;

        MAP_SNAP_STAT *stat = MapSnapGetStat(map_snap_wrk.floor, i);
        map_snap_wrk.stat = stat;
        if (map_snap_adj.layer == i)
        {
            if (map_snap_adj.set == 0)
            {
                map_snap_adj.ofs_x   = stat->ofs_x;
                map_snap_adj.ofs_y   = stat->ofs_y;
                map_snap_adj.scale_x = stat->scale_x;
                map_snap_adj.scale_y = stat->scale_y;
            }
            else
            {
                stat->ofs_x   = map_snap_adj.ofs_x;
                stat->ofs_y   = map_snap_adj.ofs_y;
                stat->scale_x = map_snap_adj.scale_x;
                stat->scale_y = map_snap_adj.scale_y;
            }
        }

        B2D_ITEM *item = B2dCtlFindItemIf(handle, nullptr, MapSnapItemHitPos, (int)pos[0], (int)pos[2]);
        if (item == nullptr)
            continue;
        map_snap_wrk.room = MapSnapItemGetRow(item);
        if (map_snap_wrk.room == nullptr)
            continue;

        if (map_snap_wrk.update != 0)
        {
            map_snap_wrk.room->flags |= MAP_SNAP_FLG_01 | MAP_SNAP_FLG_02 | MAP_SNAP_FLG_VISITED;
            if (map_snap_wrk.room->world == 0)
                map_snap_wrk.room->world = map_snap_wrk.world;
            else if (map_snap_wrk.world != map_snap_wrk.room->world)
                MapSnapRemapWorld(map_snap_wrk.room->world, map_snap_wrk.world);
        }
        break;
    }

    if (map_snap_wrk.room != nullptr)
        ingame_wrk._28 = MapSnapGetCurPlace();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The player's map floor (PlyrMapPosUpdate): index (floor - 11) / 2 + 2.
// FF3 0x001f6950.

int MapSnapSetFloor(short floor)
{
    int no = (floor - 11) / 2 + 2;

    if (no < 0)
        no = 0;
    if (map_snap_wrk.floor != no)
        map_snap_wrk.floor = no;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f6988.

int MapSnapIsLoaded(void)
{
    return map_snap_mark->IsReady() != 0;
}

// ──────────────────────────────────────────────────────────────────────
// A new world id for the rows entered from now on (the world switches).
// FF3 0x001f69b0, 0x001f69e0.

s_char MapSnapUpdateWorld(void)
{
    map_snap_wrk.world = MapSnapGetFreeWorld();
    return map_snap_wrk.world;
}

void MapSnapStateUpdate(void)
{
    MapSnapUpdateWorld();
}

// ──────────────────────────────────────────────────────────────────────
// IngameInit / IngameEnd.
// callcheck: operator_new, MarkDatCtor (new CMarkDat)
// FF3 0x001f69f8, 0x001f6a68.

int MapSnapInit(void)
{
    memset(&map_snap_wrk, 0, sizeof(map_snap_wrk));
    map_snap_wrk.floor  = -1;
    map_snap_wrk.update = 1;
    map_snap_mark = new CMarkDat;       // operator new(0x46c) + MarkDatCtor
    MapSnapRoomLoad();
    MapSnapStatLoad();
    return 0;
}

int MapSnapEnd(void)
{
    if (map_snap_mark != nullptr)
        delete map_snap_mark;           // vtable +4: the deleting destructor
    map_snap_mark = nullptr;
    MapSnapRoomFree();
    MapSnapStatFree();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when point p lies on the inner side of all four edges of quad (four XZ
// points), as MapSnapCross2D measures it.
// FF3 0x001f6ab0, 0x001f6b38.

static int MapSnapIsInQuad(const float *p, const float *quad)
{
    for (int i = 0; i < 4; i++)
    {
        if (0.0f <= MapSnapCross2D(p, &quad[i * 2], &quad[((i + 1) & 3) * 2]))
            return 0;
    }
    return 1;
}

static float MapSnapCross2D(const float *a, const float *b, const float *c)
{
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]);
}

// ──────────────────────────────────────────────────────────────────────
// Save block 53: the room rows.
// FF3 0x001f6b70.

int MapSnapSaveGet(void **addr)
{
    *addr = map_snap_room;
    return 0x800;
}

// ──────────────────────────────────────────────────────────────────────
// Room rows by (room, sub) and by (area, place).
// FF3 0x001f6b80, 0x001f6c28.

static MAP_SNAP_ROOM *MapSnapFindRoom(const void *ecb, short room, s_char sub)
{
    int num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_SNAP_ROOM *row = (MAP_SNAP_ROOM *)EcbGetData(ecb, i);
        if (row->room == room && row->sub == sub)
            return row;
    }
    return nullptr;
}

static MAP_SNAP_ROOM *MapSnapFindPlace(const void *ecb, s_char area, short place)
{
    int num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_SNAP_ROOM *row = (MAP_SNAP_ROOM *)EcbGetData(ecb, i);
        if (row->area == area && row->place == place)
            return row;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The decimal number in str[ofs .. ofs + len), -1 on a non-digit.
// callcheck: FUN_002a0380 (isdigit)
// FF3 0x001f6cd0.

static int MapSnapAtoi(const char *str, int ofs, int len)
{
    int end = ofs + len;
    int val = 0;

    while (ofs < end)
    {
        char c = str[ofs++];
        if (!isdigit((u_char)c))
            return -1;
        val = val * 10 + c - '0';
    }
    return val;
}

// ──────────────────────────────────────────────────────────────────────
// The room row a layout item stands for, from its name: "Rs" + three-digit
// room (or "rre" + two digits), then the sub-room after the next 'r'.
// callcheck: FUN_002a0380, FUN_002a305c (isdigit, strchr)
// FF3 0x001f6d70 (strings sdata 0x370fb8 "Rs", 0x370fc0 "rre").

MAP_SNAP_ROOM *MapSnapItemGetRow(B2D_ITEM *item)
{
    const char *s;
    int         room;

    if (item == nullptr)
        return nullptr;

    s = strstr(item->name, "Rs");
    if (s != nullptr)
    {
        room = MapSnapAtoi(s, 2, 3);
    }
    else
    {
        s = strstr(item->name, "rre");
        if (s == nullptr)
            return nullptr;
        room = MapSnapAtoi(s, 3, 2);
    }

    int sub = 0;
    const char *p = strchr(s + 5, 'r');
    while (isdigit((u_char)p[1]))
    {
        p++;
        sub = sub * 10 + *p - '0';
    }
    return MapSnapFindRoom(map_snap_room, (short)room, (s_char)sub);
}

// ──────────────────────────────────────────────────────────────────────
// The save screens' room picture of a play data place, -1.
// FF3 0x001f6e70.

short MapSnapGetNo(s_char area, short place)
{
    MAP_SNAP_ROOM *row = MapSnapFindPlace(map_snap_room, area, place);
    short          no  = -1;

    if (row != nullptr)
        no = row->snap_no;
    return no;
}

// ──────────────────────────────────────────────────────────────────────
// Flag bits on every room row.
// FF3 0x001f6ea8, 0x001f6f20.

void MapSnapSetFlagAll(u_char mask)
{
    void *ecb = map_snap_room;
    int   num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
        ((MAP_SNAP_ROOM *)EcbGetData(ecb, i))->flags |= mask;
}

void MapSnapClearFlagAll(u_char mask)
{
    void *ecb = map_snap_room;
    int   num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
        ((MAP_SNAP_ROOM *)EcbGetData(ecb, i))->flags &= ~mask;
}

// ──────────────────────────────────────────────────────────────────────
// The first world id (0..255) no row carries, -1.
// FF3 0x001f6fa0.

static s_char MapSnapGetFreeWorld(void)
{
    char  used[0x100];
    void *ecb = map_snap_room;

    memset(used, 0, sizeof(used));
    int num = EcbGetNum(ecb);
    for (int i = 0; i < num; i++)
        used[(u_char)((MAP_SNAP_ROOM *)EcbGetData(ecb, i))->world] = 1;

    for (int i = 0; i < 0x100; i++)
    {
        if (used[i] == 0)
            return (s_char)i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Rows of world 'from' join world 'to', visited.
// FF3 0x001f7050.

void MapSnapRemapWorld(s_char from, s_char to)
{
    void *ecb = map_snap_room;
    int   num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_SNAP_ROOM *row = (MAP_SNAP_ROOM *)EcbGetData(ecb, i);
        if (row->world == from)
        {
            row->world  = to;
            row->flags |= MAP_SNAP_FLG_VISITED;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f70e0.

int MapSnapIsGroupVisited(s_char group)
{
    void *ecb = map_snap_room;
    int   num = EcbGetNum(ecb);

    for (int i = 0; i < num; i++)
    {
        MAP_SNAP_ROOM *row = (MAP_SNAP_ROOM *)EcbGetData(ecb, i);
        if (row->group == group && (row->flags & MAP_SNAP_FLG_VISITED) != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The two ECB files in the system heap.
// FF3 0x001f7178, 0x001f71a0, 0x001f7348, 0x001f7370.

static void MapSnapRoomLoad(void)
{
    map_snap_room = MapSnapFileLoad(MAP_MENU_ECB, nullptr, 0x800);
}

static void MapSnapRoomFree(void)
{
    if (map_snap_room != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), map_snap_room, 0, 0, 0);
    map_snap_room = nullptr;
}

static void MapSnapStatLoad(void)
{
    map_snap_stat = MapSnapFileLoad(MAP_MENU_STAT_ECB, nullptr, -1);
}

static void MapSnapStatFree(void)
{
    if (map_snap_stat != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), map_snap_stat, 0, 0, 0);
    map_snap_stat = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The stat row of (floor, layer) when the layer has a layout.
// FF3 0x001f71e8, 0x001f72c0, 0x001f72e8.

static MAP_SNAP_STAT *MapSnapFindStat(const void *ecb, int floor, int layer)
{
    CMarkDat *md = MapSnapGetMarkDat();

    if (md->GetPhfNo(floor, layer) < 0)
        return nullptr;

    int   num = EcbGetNum(ecb);
    short f   = (short)md->GetFloor(floor);
    for (int i = 0; i < num; i++)
    {
        MAP_SNAP_STAT *row = (MAP_SNAP_STAT *)EcbGetData(ecb, i);
        if (row->floor == f && row->layer == (short)layer)
            return row;
    }
    return nullptr;
}

MAP_SNAP_STAT *MapSnapGetStat(int floor, int layer)
{
    return MapSnapFindStat(map_snap_stat, floor, layer);
}

MAP_SNAP_STAT *MapSnapGetFirstStat(int floor)
{
    for (int i = 0; i < CMARK_LAYER_NUM; i++)
    {
        MAP_SNAP_STAT *stat = MapSnapFindStat(map_snap_stat, floor, i);
        if (stat != nullptr)
            return stat;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Out-of-line rect helpers (FF3 passes an unused first argument).
// FF3 0x002bce40: (p.x, 0, p.y, 1).

static void MapSnapPosXZ(float *out, const float *p)
{
    out[0] = p[0];
    out[1] = 0.0f;
    out[3] = 1.0f;                      /* sdata 0x370fa8 */
    out[2] = p[1];
}

// FF3 0x002bce68: (x, y, w, h) with negative w / h flipped.

static void MapSnapRectNormalize(float *rect)
{
    float x0 = rect[0];
    float y0 = rect[1];
    float x1 = rect[2] + x0;
    float y1 = rect[3] + y0;

    rect[2] = x1;
    rect[3] = y1;
    if (x1 < x0)
    {
        rect[2] = x0;
        rect[0] = x1;
        x0      = x1;
    }
    if (rect[3] < y0)
    {
        rect[1] = rect[3];
        rect[3] = y0;
        y0      = rect[1];
    }
    rect[3] = rect[3] - y0;
    rect[2] = rect[2] - x0;
}

// FF3 0x002bced8: 1 when the two rects overlap.

static int MapSnapRectIsHit(const float *a, const float *b)
{
    float x0 = (a[0] < b[0]) ? a[0] : b[0];
    float x1 = (b[0] + b[2] < a[0] + a[2]) ? a[0] + a[2] : b[0] + b[2];
    float y0 = (a[1] < b[1]) ? a[1] : b[1];
    float y1 = (b[1] + b[3] < a[1] + a[3]) ? a[1] + a[3] : b[1] + b[3];

    return (x1 - x0 <= a[2] + b[2]) && (y1 - y0 <= a[3] + b[3]);
}
