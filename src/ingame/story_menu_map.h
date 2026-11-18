/* ==========================================================================
 *  ingame/story_menu_map.h
 *
 *  The map menu (phase 58, StoryMenu_Map: the floor map the map button or
 *  the story menu's "R map" row opens). FF3 0x1fea48..0x1ff6e8 plus the two
 *  int helpers 0x2bd008..0x2bd080 in the linkonce run; FF3-only file (FF2
 *  had no map menu), name follows the phase.
 *
 *  The phase drives the map screen object (CMapScreen, bss 0x3f3360,
 *  constructed by this file's static init 0x1ff608; its methods are in
 *  ingame/photo/m_plyr_camera.c and ingame/plyr/man_data.c, its two floor
 *  layers are CMapLayer, ingame/map/MapLayer.c) through a step function in
 *  the state block below.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_MAP_H
#define _INGAME_STORY_MENU_MAP_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#define STORY_MAP_FLOOR_MAX 4           // StoryMapSetFloor's upper clamp
#define STORY_MAP_FROM_MENU  0x39       // phase 57 StoryMenu_Top: opened from the story menu (else from the map button: pad_wrk[0].stick_dpad on while open)

// One map menu step (state.step): the phase to go to, -1 to stay.
typedef short (*STORY_MAP_STEP)(void);

// The map menu state, data 0x32b4b0 (0x2c bytes on the EE; StoryMapStateInit
// clears it keeping the zoom bit).
// layout: host -- step is a pointer (EE 4 bytes at +0x20); the offsets
// before it match the ROM, cx / cy follow it.
struct STORY_MAP_STATE
{
    /* 0x00 */ int            zoom;     // 0 / 1: scale[zoom]; data starts at 1
    /* 0x04 */ int            floor;    // shown floor 0..4 (MapSnapGetFloor at open)
    /* 0x08 */ float          scroll_x; // map scroll (StoryMapScrollInput; drawn negated)
    /* 0x0c */ float          scroll_y;
    /* 0x10 */ int            moved;    // the scroll moved this frame
    /* 0x14 */ int            _14;      // cleared at open, never read here
    /* 0x18 */ float          scale[2]; // 1.5 / 3.0 (data 0x3710f0 / 0x3710f4)
    /* 0x20 */ STORY_MAP_STEP step;     // the current step (one_StoryMenu_Map)
    /*  EE 0x24 */ int        cx;       // the screen centre the zoom keeps: 320
    /*  EE 0x28 */ int        cy;       // 224
};
static_assert(offsetof(STORY_MAP_STATE, floor) == 0x04, "STORY_MAP_STATE.floor");
static_assert(offsetof(STORY_MAP_STATE, scroll_x) == 0x08, "STORY_MAP_STATE.scroll_x");
static_assert(offsetof(STORY_MAP_STATE, moved) == 0x10, "STORY_MAP_STATE.moved");
static_assert(offsetof(STORY_MAP_STATE, scale) == 0x18, "STORY_MAP_STATE.scale");
static_assert(offsetof(STORY_MAP_STATE, step) == 0x20, "STORY_MAP_STATE.step");

int  IsMapEnable(void);                 // FF3 0x001fea48: the map button may open the map menu (IngameMain, CmnFadeLayerIsIdleOk)
int  StoryMapGetScale(void);            // FF3 0x001fea90: scale[zoom & 1] * 1024 as an int (the layer and mark draws)
void StoryMapRelease(void);             // FF3 0x001feb40: releases the map screen's layouts (GameSystemEnd, end_StoryMenu_Map)

// Phase 58 StoryMenu_Map (no pre / after)
int  init_StoryMenu_Map(void);          // FF3 0x001ff310
int  one_StoryMenu_Map(void);           // FF3 0x001ff5b0
int  end_StoryMenu_Map(void);           // FF3 0x001ff548

#endif /* _INGAME_STORY_MENU_MAP_H */
