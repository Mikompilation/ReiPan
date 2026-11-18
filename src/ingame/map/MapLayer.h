/* ==========================================================================
 *  ingame/map/MapLayer.h
 *
 *  CMapLayer: one floor of the map menu -- the up to five Bp layouts of a
 *  floor (MarkDatGetPhfNo(floor, i)), drawn together with an alpha fade,
 *  the player marker ("R player") and the room under the screen centre.
 *  The map screen object (CMapScreen, m_plyr_camera.h) holds two and swaps
 *  them when the floor changes. FF3 0x1f38d0..0x1f4448; FF3-only (the
 *  0x1f3920.. names were wrong FF2 kaza2_pzl.c matches). No vtable: a plain
 *  class whose methods take it in a0.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPLAYER_H
#define _INGAME_MAP_MAPLAYER_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

struct B2D_ITEM;
struct MAP_SNAP_ROOM;

#define MAP_LAYER_LAYOUT_NUM 5          // layouts per floor

// 0x3c bytes on the EE.
// layout: host -- row is a pointer (EE 4 bytes at +0x2c); hit, x and y follow it.
class CMapLayer
{
public:
    CMapLayer();                                        // FF3 0x001f38d0 (MapLayerCtor)

    void Fade(int from, int to, int frames);            // FF3 0x001f3920: alpha from -> to over frames (Draw steps it)
    void Load(int floor);                               // FF3 0x001f3948: releases, then loads the floor's layouts
    int  IsReady();                                     // FF3 0x001f3a08: every layout loaded (sets the from alpha once)
    void Draw();                                        // FF3 0x001f3aa8: steps the fade, draws the layouts
    void PlayLabel(const char *label);                  // FF3 0x001f3b80
    void PlayDefault();                                 // FF3 0x001f3be8: PlayLabel("NewLabel_0")
    void Release();                                     // FF3 0x001f3c08
    int  IsStill();                                     // FF3 0x001f3c80: no layout part playing
    void SetAlpha(int alpha);                           // FF3 0x001f3cf8
    void SetScroll(int x, int y);                       // FF3 0x001f3d58: places the layouts for the map scrolled to (x, y)
    void SetRun(int run);                               // FF3 0x001f3ef8: 0 pauses every layout (BP_CTL_FLAG_PAUSE); no callers
    void SetScale(int sx, int sy);                      // FF3 0x001f3f68: 1024 = 1.0 (StoryMapGetScale)
    B2D_ITEM *FindItemAt(int x, int y);                 // FF3 0x001f3fd8: sets hit
    B2D_ITEM *Pick(int x, int y);                       // FF3 0x001f4078: FindItemAt and its room into row
    B2D_ITEM *SetPlayer(int floor, int cx, int cy);     // FF3 0x001f4208: the player marker; x / y centre it on (cx, cy)

    /* 0x00 */ int   handle[MAP_LAYER_LAYOUT_NUM];  // BpCtl handles, -1: none
    /* 0x14 */ int   alpha_from;                    // 0x80 after the ctor
    /* 0x18 */ int   alpha_to;
    /* 0x1c */ int   fade_frames;
    /* 0x20 */ int   fade_time;                     // the fade is over once fade_frames <= fade_time
    /* 0x24 */ int   ready;                         // IsReady saw every layout loaded
    /* 0x28 */ int   floor;                         // the floor Load asked for
    /* 0x2c */ MAP_SNAP_ROOM *row;                  // the room under the screen centre (Pick), read by the map draw
    /*  EE 0x30 */ int hit;                         // the layout FindItemAt hit, -1: none
    /*  EE 0x34 */ int x;                           // the origin that centres the player marker (SetPlayer):
    /*  EE 0x38 */ int y;                           //   the map menu's scroll at open
};
static_assert(offsetof(CMapLayer, alpha_from) == 0x14, "CMapLayer.alpha_from");
static_assert(offsetof(CMapLayer, fade_frames) == 0x1c, "CMapLayer.fade_frames");
static_assert(offsetof(CMapLayer, fade_time) == 0x20, "CMapLayer.fade_time");
static_assert(offsetof(CMapLayer, ready) == 0x24, "CMapLayer.ready");
static_assert(offsetof(CMapLayer, floor) == 0x28, "CMapLayer.floor");
static_assert(offsetof(CMapLayer, hit) == offsetof(CMapLayer, row) + sizeof(void *), "CMapLayer.hit (EE 0x30)");
static_assert(offsetof(CMapLayer, x) == offsetof(CMapLayer, hit) + 4, "CMapLayer.x (EE 0x34)");
static_assert(offsetof(CMapLayer, y) == offsetof(CMapLayer, x) + 4, "CMapLayer.y (EE 0x38)");

#endif /* _INGAME_MAP_MAPLAYER_H */
