/* ==========================================================================
 *  ingame/photo/m_plyr_camera.h
 *
 *  (file name inferred from link order; the code is the map menu's screen,
 *  not a camera: open-items.md.) FF3 0x1f74f8..0x1f8230: the map sprite /
 *  quad hooks and their helpers, the map's three CLUT files, and the first
 *  methods of the map screen object CMapScreen; its other methods follow
 *  in ingame/plyr/man_data.c (0x1f8230..0x1f8af8).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_M_PLYR_CAMERA_H
#define _INGAME_PHOTO_M_PLYR_CAMERA_H

#include "ingame/map/MapLayer.h"        // CMapLayer
#include "ingame/map/MarkDoor.h"        // CMarkDoor
#include "ingame/map/MarkEvent.h"       // CMarkEvent
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC
#include "system/b2d/B2dCtl.h"          // B2D_HOOK_ARGS

#include <stddef.h>                     // offsetof

struct B2D_ITEM;
struct MAP_SNAP_ROOM;
struct MENU_PIC;

// The map screen object: the map menu's frame and menu layouts, its two
// floor layers (the shown one and the one loading), the door marks. One
// instance, bss 0x3f3360 (story_menu_map.c owns and constructs it). No
// vtable of its own; the members' vtables are reset by the static
// destruction (FF3 0x001ff608).
// layout: host -- the B2D_ITEM pointers and the members' vptrs grow on the
// host; EE offsets in the comments.
class CMapScreen
{
public:
    CMapScreen();                                       // FF3 0x001f8168 (MapScreenCtor; twin 0x001f8118)

    void SwapLayer();                                   // FF3 0x001f81b8: cur ^= 1
    void LoadReq();                                     // FF3 0x001f81d0: MAPBG_PHF, MAPMENU_PHF + language
    void LoadFloor(int floor, int zoomed);              // FF3 0x001f8230: into the back layer; zoomed is not read
    void Open(const char *label);                       // FF3 0x001f8260: the frame, the menu's label ("in" / "select_in")
    int  IsClosed();                                    // FF3 0x001f8328: nothing playing any more
    void SetHelp(int on);                               // FF3 0x001f83d0: "help_on" / "help_off" on the menu; no callers
    void PlayLabel(const char *label);                  // FF3 0x001f8418: on menu and frame ("out")
    void GetCentre(int *x, int *y);                     // FF3 0x001f8460: the centre of the 4 scroll arrows
    void ArrowUpdate(int moving);                       // FF3 0x001f8580: the arrows lit while held
    int  IsReady();                                     // FF3 0x001f8668: both layers and the layouts loaded
    void Release();                                     // FF3 0x001f8710
    void Draw(int x, int y, int bg);                    // FF3 0x001f87b0: at scroll (x, y); bg < 0: a black backdrop

    CMapLayer *Cur() { return &layer[cur]; }
    CMapLayer *Back() { return &layer[cur ^ 1]; }

    /* 0x00 */ int        frame;                        // BpCtl handle: MAPBG_PHF
    /* 0x04 */ int        menu;                         // BpCtl handle: MAPMENU_PHF + language
    /* 0x08 */ CMapLayer  layer[2];
    /*  EE 0x80 */ int    cur;                          // the shown layer; 1 after the ctor
    /*  EE 0x84 */ B2D_ITEM *win;                       // frame item 0x3ec: the map window (its rect becomes CMapRec.view)
    /*  EE 0x88 */ B2D_ITEM *bg_item;                   // frame item 0x3eb: its alpha fades the black backdrop
    /*  EE 0x8c */ int    help;                         // the help label shown (SetHelp); cleared by the ctor and Open
    /*  EE 0x90 */ CMarkDoor  door;
    /*  EE 0x1e98 */ CMarkEvent event;
};
static_assert(offsetof(CMapScreen, layer) == 0x08, "CMapScreen.layer");
static_assert(offsetof(CMapScreen, cur) == 0x08 + 2 * sizeof(CMapLayer), "CMapScreen.cur (EE 0x80)");

// One sprite of the current room, queued by the sprite hook and drawn
// after the floor layers with the GENZAITI CLUT (MapScreenDrawCurRoom):
// the B2d sprite hook's words. 0x34 bytes.
// layout: EE (ints only)
struct MAP_SCREEN_SPR                   /* 0x34 */
{
    /* 0x00 */ int   handle;            // BpTexSetFunc a0 (the Bp handle drawn)
    /* 0x04 */ int   tex;               // BpTexSetFunc a1; the draw's a0
    /* 0x08 */ int   u, v, w, h;
    /* 0x18 */ int   x, y;
    /* 0x20 */ int   sx, sy;            // 0x400 = 1.0
    /* 0x28 */ int   alpha;             // already times the backdrop item's alpha / 128
    /* 0x2c */ u_int rgb;
    /* 0x30 */ u_int flags;
};
static_assert(sizeof(MAP_SCREEN_SPR) == 0x34, "MAP_SCREEN_SPR is 0x34 bytes");
static_assert(offsetof(MAP_SCREEN_SPR, x) == 0x18, "MAP_SCREEN_SPR.x");
static_assert(offsetof(MAP_SCREEN_SPR, alpha) == 0x28, "MAP_SCREEN_SPR.alpha");
static_assert(offsetof(MAP_SCREEN_SPR, flags) == 0x30, "MAP_SCREEN_SPR.flags");

#define MAP_SCREEN_SPR_MAX  16          // bss 0x3f16f8..0x3f1a38 (MapObjWrk follows)

// The map draw's state, shared with CMapScreen::Draw (ingame/plyr/man_data.c,
// FF3 0x001f87b0; both files use the same sdata and sbss, probably one
// FF3 object: open-items.md).
extern int            map_screen_spr_num;   // sdata 0x370fd0: queued sprites; the draw resets it
extern MENU_PIC      *map_screen_pic;       // sdata 0x370fd4: the room picture (Open makes it, Release deletes it)
extern B2D_ITEM      *map_screen_bg_item;   // sbss gp+0x1c: CMapScreen.bg_item during the draw (its alpha scales the marks)
extern MAP_SNAP_ROOM *map_screen_row;       // sbss gp+0x20: the shown layer's row (the room under the centre); SwapLayer clears it
extern int            map_screen_floor;     // sbss gp+0x24: the floor of the layout drawn now (MapSnapSetDrawLayer)
extern int            map_screen_layer;     // sbss gp+0x28: its layout 0..4
extern CMapScreen    *map_screen_drawn;     // sbss gp+0x2c: the screen object while CMapScreen::Draw runs
extern MAP_SCREEN_SPR map_screen_spr[MAP_SCREEN_SPR_MAX];   // bss 0x3f16f8

// The MAPMENU layout's item hooks (rodata 0x35b828; CMapScreen::Draw copies
// it and passes it to B2dSetHookList for the menu handle).
extern const B2D_HOOK_REC map_screen_menu_hooks[4];

void MapSnapSetDrawLayer(int floor, int layer);         // FF3 0x001f74f8: CMapLayer::Draw, before each layout
int *MapRecGetRect(void);               // FF3 0x001f7508: CMapRec.rect of MapRecGet() (x0, y0, x1, y1): the map's screen rect
int  MapScreenItemGetDoor(B2D_ITEM *item);              // FF3 0x001f7528: the item's door mark id, 0 a "door" item without one, -1 not a door
int  MapScreenDoorCanAdd(int mark);     // FF3 0x001f7580: the door mark is not recorded yet, or on the layout drawn now
int  MapSnapItemGetState(int handle, B2D_ITEM *item);   // FF3 0x001f75c0: MAP_SCREEN_ROOM_* of the item's row
void MapScreenSetClut(int handle, int tex, int clut);   // FF3 0x001f7628: one of the three CLUT files onto the texture
void MapScreenClutLoad(void);           // FF3 0x001f7658: MAPCL_TOUHA / MAPCL_MITOUHA / MAPCL_GENZAITI (table 0x32a6a8)
int  MapScreenClutIsLoaded(void);       // FF3 0x001f76e8
void MapScreenClutFree(void);           // FF3 0x001f7748
int  MapScreenDrawCurRoom(void);        // FF3 0x001f77c8: the queued sprites; 1 when there were any
int  MapSnapNameIsRsaRex(const char *name);  // FF3 0x001f7948: the name starts with "R sa" or "R ex"
int  MapScreenSpriteHook(const B2D_HOOK_ARGS *args);    // FF3 0x001f7978: the floor layouts' sprite hook during CMapScreen::Draw
int  MapScreenQuadHook(const B2D_HOOK_ARGS *args);      // FF3 0x001f7d70: their quad hook: every quad becomes a mark

// MapSnapItemGetState's answers: how the sprite hook draws a room item.
enum MAP_SCREEN_ROOM_STATE
{
    MAP_SCREEN_ROOM_DONE    = 0,        // visited, flag 0x01: CLUT 0 (MAPCL_TOUHA, explored)
    MAP_SCREEN_ROOM_PART    = 1,        // visited, flag 0x02: CLUT 1 (MAPCL_MITOUHA, not explored)
    MAP_SCREEN_ROOM_CUR     = 2,        // flag 0x01 and the room under the centre: queued, CLUT 2 (MAPCL_GENZAITI)
    MAP_SCREEN_ROOM_HIDDEN  = 3,        // not visited (or neither flag): not drawn
    MAP_SCREEN_ROOM_NONE    = 4,        // no row: a mark without doors (MARK_DOOR_NONE)
};

#endif /* _INGAME_PHOTO_M_PLYR_CAMERA_H */
