/* ==========================================================================
 *  ingame/map/MapSnap.h
 *
 *  Where the player is on the in-game 2D map (FF3-only, 0x1f5ba0..0x1f73b8
 *  plus the rect helpers 0x2bce40..0x2bcf78; FF2's kaza_pzl.c link slot).
 *
 *  The map menu's floor layouts (maps_*.b2d, eight floors x five layers,
 *  table 0x34c400) belong to CMarkDat (MarkDat.h), made by MapSnapInit. The
 *  rooms are the rows of MAP_MENU_ECB (2ddata/map_menu.ecb, 125 rows) held in
 *  a 0x800-byte heap buffer that goes into the save as it is (block 53); each
 *  layout item names its row ("Rs<room>..r<sub>" or "rre<room>"). Every
 *  frame MapSnapUpdate finds the item under the player through the layer's
 *  world transform (MAP_MENU_STAT_ECB rows) and makes its row the current
 *  room: map_bgm picks the music from it, and with MAP_UPDATE on the row gets
 *  its visited bits.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPSNAP_H
#define _INGAME_MAP_MAPSNAP_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

class CMarkDat;                         // ingame/map/MarkDat.h
struct B2D_ITEM;                        // system/b2d/B2dCtl.h

// Room row flags (+0xa).
#define MAP_SNAP_FLG_01      0x01       // set on entering (with 0x02, 0x10); the world switch clears 0x01 / 0x10
#define MAP_SNAP_FLG_02      0x02
#define MAP_SNAP_FLG_VISITED 0x10       // MapSnapItemHitRect / FUN_001feac8 test it

// One row of map_menu.ecb (0xc bytes).
struct MAP_SNAP_ROOM                    /* 0xc */
{
    /* 0x00 */ short  room;             // 1..509 (floor * 100 + room), "Rs<room>"
    /* 0x02 */ s_char sub;              // the sub-room, "..r<sub>"
    /* 0x03 */ s_char group;            // FUN_001f70e0: any visited row of the group
    /* 0x04 */ s_char area;             // MapSnapFindPlace
    /* 0x05 */ s_char area_msg;         // message type 0 id: the area name the map menu prints (MapScreenAreaNameHook)
    /* 0x06 */ short  place;            // row number; ingame_wrk._28 while the player is in it (the save header's "chapter")
    /* 0x08 */ short  snap_no;          // MapSnapGetNo: + RS101_R01_TM2 is the save screen's room picture
    /* 0x0a */ u_char flags;            // MAP_SNAP_FLG_*
    /* 0x0b */ s_char world;            // 0: unset; MapSnapUpdate stamps the current world id
};
static_assert(sizeof(MAP_SNAP_ROOM) == 0xc, "MAP_SNAP_ROOM is 0xc bytes");
static_assert(offsetof(MAP_SNAP_ROOM, sub) == 0x02, "MAP_SNAP_ROOM.sub");
static_assert(offsetof(MAP_SNAP_ROOM, group) == 0x03, "MAP_SNAP_ROOM.group");
static_assert(offsetof(MAP_SNAP_ROOM, area) == 0x04, "MAP_SNAP_ROOM.area");
static_assert(offsetof(MAP_SNAP_ROOM, place) == 0x06, "MAP_SNAP_ROOM.place");
static_assert(offsetof(MAP_SNAP_ROOM, snap_no) == 0x08, "MAP_SNAP_ROOM.snap_no");
static_assert(offsetof(MAP_SNAP_ROOM, flags) == 0x0a, "MAP_SNAP_ROOM.flags");
static_assert(offsetof(MAP_SNAP_ROOM, world) == 0x0b, "MAP_SNAP_ROOM.world");

// One row of map_menu_stat.ecb (0x14 bytes): a floor layer's layout ->
// world transform, world = (layout + ofs) * scale (x -> x, y -> z).
struct MAP_SNAP_STAT                    /* 0x14 */
{
    /* 0x00 */ short floor;             // CMarkDat floor index (CMarkDat::GetFloor)
    /* 0x02 */ short layer;             // 0..4
    /* 0x04 */ float ofs_x;
    /* 0x08 */ float ofs_y;
    /* 0x0c */ float scale_x;           // 4.0 in the ROM
    /* 0x10 */ float scale_y;           // -4.0
};
static_assert(sizeof(MAP_SNAP_STAT) == 0x14, "MAP_SNAP_STAT is 0x14 bytes");
static_assert(offsetof(MAP_SNAP_STAT, ofs_x) == 0x04, "MAP_SNAP_STAT.ofs_x");
static_assert(offsetof(MAP_SNAP_STAT, ofs_y) == 0x08, "MAP_SNAP_STAT.ofs_y");
static_assert(offsetof(MAP_SNAP_STAT, scale_x) == 0x0c, "MAP_SNAP_STAT.scale_x");
static_assert(offsetof(MAP_SNAP_STAT, scale_y) == 0x10, "MAP_SNAP_STAT.scale_y");

// The work, bss 0x3f16d0 (MapSnapInit clears 0x18 bytes).
// layout: host -- stat and room are EE pointers (4 bytes at +0 / +8); the
// words between and after keep the EE offsets in the comments.
struct MAP_SNAP_WRK                     /* 0x18 on the EE */
{
    /* 0x00 */ MAP_SNAP_STAT *stat;     // the transform of the layer being searched
    /* 0x04 */ int            floor;    // floor index (MapSnapSetFloor); -1 before the first
    /* 0x08 */ MAP_SNAP_ROOM *room;     // the room the player is in; nullptr between rooms
    /* 0x0c */ int            update;   // MAP_UPDATE: rooms entered get their visited bits (1 after init)
    /* 0x10 */ int            exact;    // MapSnapItemHitRect: 0 bounding rects, else the edge test (never set here)
    /* 0x14 */ s_char         world;    // the world id rows entered get (MapSnapUpdateWorld)
};
static_assert(offsetof(MAP_SNAP_WRK, floor) == sizeof(void *), "MAP_SNAP_WRK.floor follows stat");

MAP_SNAP_WRK *MapSnapGetWrk(void);      // FF3 0x001f5f28

// Save blocks (system/mc/dat/save_data.c).
int   MapSnapSaveGet(void **addr);      // FF3 0x001f6b70: {the room buffer, 0x800} (block 53)

CMarkDat *MapSnapGetMarkDat(void);      // FF3 0x001f5ba0
void  MapSnapMarkDatDraw(int *rect, int x, int y, int sx, int sy);  // FF3 0x001f5ba8: CMarkDat::Measure on the CMarkDat (screen rect x0, y0, x1, y1)
int   MapSnapGetFirstLayer(int floor);  // FF3 0x001f5be8: first layer with a layout, -1
void  MapSnapSetUpdate(int on);         // FF3 0x001f5c48: MAP_UPDATE
float MapSnapWorldToMapX(float x, const MAP_SNAP_STAT *stat);   // FF3 0x001f5c58
float MapSnapWorldToMapY(float z, const MAP_SNAP_STAT *stat);   // FF3 0x001f5c78
int   MapSnapWorldToMapPos(float *out, const float *pos, int layer);    // FF3 0x001f5e80: (x, y, z) -> (map x, y, map y, 1); 0
short MapSnapGetCurPlace(void);         // FF3 0x001f5f08: the current row's place, 0
int   MapSnapGetFloor(void);            // FF3 0x001f5f38
int   MapSnapCheckItemRect(B2D_ITEM *item, int arg, int floor, int layer);  // FF3 0x001f6478: the item meets a visited room
int   MapSnapLayerGetRect(int floor, int layer, float *rect);   // FF3 0x001f66d0: CMarkDat's layer rect; 0 when not loaded
int   MapSnapUpdate(float *pos);        // FF3 0x001f6790: IngameDrawSub, the player's position; 0
int   MapSnapSetFloor(short floor);     // FF3 0x001f6950: PlyrMapPosUpdate; 0
int   MapSnapIsLoaded(void);            // FF3 0x001f6988: every map layout loaded
s_char MapSnapUpdateWorld(void);        // FF3 0x001f69b0: the world switches
void  MapSnapStateUpdate(void);         // FF3 0x001f69e0: IngameNowLoadingMain
int   MapSnapInit(void);                // FF3 0x001f69f8: IngameInit; 0
int   MapSnapEnd(void);                 // FF3 0x001f6a68: IngameEnd; 0
MAP_SNAP_ROOM *MapSnapItemGetRow(B2D_ITEM *item);   // FF3 0x001f6d70: from the item's name, nullptr
short MapSnapGetNo(s_char area, short place);       // FF3 0x001f6e70: the room picture no. (+ RS101_R01_TM2) of a place, -1 none
void  MapSnapSetFlagAll(u_char mask);   // FF3 0x001f6ea8
void  MapSnapClearFlagAll(u_char mask); // FF3 0x001f6f20: the world switches pass 0x11
void  MapSnapRemapWorld(s_char from, s_char to);    // FF3 0x001f7050: rows of world from -> to, visited
int   MapSnapIsGroupVisited(s_char group);          // FF3 0x001f70e0
MAP_SNAP_STAT *MapSnapGetStat(int floor, int layer);// FF3 0x001f72c0: nullptr when the layer has no layout
MAP_SNAP_STAT *MapSnapGetFirstStat(int floor);      // FF3 0x001f72e8: no callers

#endif /* _INGAME_MAP_MAPSNAP_H */
