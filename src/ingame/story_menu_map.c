/* ==========================================================================
 *  ingame/story_menu_map.c
 *
 *  The map menu, phase 58 StoryMenu_Map; see story_menu_map.h.
 *  FF3 0x1fea48..0x1ff6e8 + 0x2bd008..0x2bd080.
 *
 *  The phase drives the map screen object (CMapScreen, bss 0x3f3360) through
 *  a step function: StoryMapStepMain scrolls (d-pad, 2 px a frame), zooms
 *  (OK: 1.5 <-> 3.0), changes floor (L1 / R1: fade the shown layer out,
 *  load the new floor into the back layer, swap, fade it in) and closes
 *  (cancel: the "out" label, back to the phase that opened the map).
 *
 *  Not written: FF3 0x001ff608, the compiler's static initialisation and
 *  destruction of the map screen object (here the C++ global's constructor
 *  and destructor), and 0x001ff688, the GPhaseCtrl singleton's atexit thunk
 *  (as in every file that includes the getter).
 * ======================================================================== */
#include "ingame/story_menu_map.h"

#include "common/cmn_fade_layer.h"              // GetCmnFadeLayer
#include "ingame/ingame.h"                      // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                  // ingame_wrk.b03
#include "ingame/map/MapSnap.h"                 // MapSnapGetFloor, MapSnapIsGroupVisited, MapSnapMarkDatDraw
#include "ingame/menu/menu_bg.h"              // MenuBgGetHandle, MenuBgStop
#include "ingame/photo/m_plyr_camera.h"         // CMapScreen, MapRecGetRect, MapScreenClut*
#include "ingame/plyr/plyr_mdl.h"               // PlyrIsFinderModeOff
#include "ingame/story_menu.h"                  // IsMenuEnable, IsLockTblOn
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "system/b2d/BpCtl.h"                   // BpCtlMain
#include "system/pad/pad.h"                     // pad_wrk
#include "system/pad/pad_ctrl.h"                // CheckPad*PlaySe, pad_key

#include <string.h>                             // memset
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// The lock id that keeps the map closed (story_menu.h: the story-menu map row).
#define LOCK_TBL_MAP    0x1b

#define STORY_MAP_SCROLL_STEP   2.0f            // data 0x3710ec: pixels a frame

static short StoryMapStepMain(void);

// data 0x32b4b0. The ROM's data image starts zoomed in (zoom 1).
static STORY_MAP_STATE story_map = { 1, 0, 0.0f, 0.0f, 0, 0, { 1.5f, 3.0f }, nullptr, 0, 0 };

// bss 0x3f3360, constructed by the static initialisation (FF3 0x001ff608).
static CMapScreen story_map_screen;

// sdata gp+0x60: the phase that opened the map (GPHASE_CTRL.from at open):
// STORY_MAP_FROM_MENU, or the in-game phase the map button left.
static short story_map_from;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002bd008 (StoryMapSetFloor; an out-of-line template in the
// linkonce run).

static int StoryMapClamp(int v, int lo, int hi)
{
    if (v < lo)
    {
        v = lo;
    }
    if (hi < v)
    {
        v = hi;
    }
    return v;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002bd020 (StoryMapSetZoom): v brought into [lo, hi) by steps of
// hi - lo.

static int StoryMapWrap(int v, int lo, int hi)
{
    int span = hi - lo;

    if (v >= hi)
    {
        do
        {
            v -= span;
        } while (v >= hi);
    }
    while (v < lo)
    {
        v += span;
    }
    return v;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fea48

int IsMapEnable(void)
{
    return IsMenuEnable() && PlyrIsFinderModeOff() && !IsLockTblOn(LOCK_TBL_MAP);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fea90 (1024 from data 0x3710e0)

int StoryMapGetScale(void)
{
    return (int)(story_map.scale[story_map.zoom & 1] * 1024.0f);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001feac8 (StoryMapStepMain): the floor asked for, clamped to the
// world's floors (world 1: 2..4), when it is new and visited. 1 switched.

static int StoryMapSetFloor(int floor)
{
    floor = StoryMapClamp(floor, ingame_wrk.b03 == 1 ? 2 : 0, STORY_MAP_FLOOR_MAX);
    if (story_map.floor == floor || MapSnapIsGroupVisited((s_char)floor) == 0)
    {
        return 0;
    }
    story_map.floor = floor;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001feb68 (StoryMapSetZoom): the scroll moved by a third of the
// screen centre so the centre stays put across the zoom change (1.5 <-> 3.0);
// out to zoom 0 subtracts, in to zoom 1 adds. The ROM passes the old zoom
// as a fourth word it does not read. Returns scale[zoom].

static float StoryMapZoomScroll(float *x, float *y, int zoom)
{
    const float third = 1.0f / 3.0f;    // data 0x3710e4

    if (zoom == 0)
    {
        *x -= (float)story_map.cx * third;
        *y -= (float)story_map.cy * third;
    }
    else
    {
        *x += (float)story_map.cx * third;
        *y += (float)story_map.cy * third;
    }
    return story_map.scale[zoom];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ff270 (init_StoryMenu_Map)

static void StoryMapStateInit(void)
{
    int zoom = story_map.zoom;

    memset(&story_map, 0, sizeof(story_map));
    story_map.zoom     = zoom & 1;
    story_map._14      = 0;
    story_map.moved    = 0;
    story_map.floor    = MapSnapGetFloor();
    story_map.step     = StoryMapStepMain;
    story_map.cy       = 224;
    story_map.cx       = 320;
    story_map.scale[0] = 1.5f;          // data 0x3710f0
    story_map.scale[1] = 3.0f;          // data 0x3710f4
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001feb40

void StoryMapRelease(void)
{
    MapScreenClutFree();
    story_map_screen.Release();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fec00 (StoryMapStepMain): zoom 0 / 1 (wrapping); a change keeps
// the screen centre and rescales the shown layer. The caller ignores the
// return.

static int StoryMapSetZoom(int zoom)
{
    int scale;

    zoom = StoryMapWrap(zoom, 0, 2);
    if (story_map.zoom != zoom)
    {
        StoryMapZoomScroll(&story_map.scroll_x, &story_map.scroll_y, zoom);
        story_map.zoom = zoom;
        scale = StoryMapGetScale();
        story_map_screen.Cur()->SetScale(scale, scale);
        zoom = story_map.zoom;
    }
    return zoom;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fec90 (StoryMapStepMain): the held d-pad scrolls while the map's
// screen rect (CMapRec.rect) still reaches past the centre that way.

// PC: the mouse. Dragging moves the map, the wheel is the same zoom as OK.
static void StoryMapMouse(const int *rect)
{
    static int   dragging;
    static float last_x;
    static float last_y;
    ReiPan_MenuMouse mouse;

    if (!ReiPan_MenuMouseBegin(0, &mouse) || !mouse.left_held)
    {
        dragging = 0;
    }
    else
    {
        if (dragging)
        {
            const float dx = last_x - mouse.x;
            const float dy = last_y - mouse.y;

            if ((dx < 0.0f && rect[0] < story_map.cx) || (dx > 0.0f && story_map.cx < rect[2]))
            {
                story_map.scroll_x += dx;
                story_map.moved = 1;
            }
            if ((dy < 0.0f && rect[1] < story_map.cy) || (dy > 0.0f && story_map.cy < rect[3]))
            {
                story_map.scroll_y += dy;
                story_map.moved = 1;
            }
        }
        dragging = 1;
        last_x = mouse.x;
        last_y = mouse.y;
    }
    if (mouse.wheel != 0)
        ReiPan_MenuMousePress(REIPAN_MENU_CROSS);
}

static void StoryMapScrollInput(float step)
{
    int *rect = MapRecGetRect();

    story_map.moved = 0;
    StoryMapMouse(rect);
    if (CheckPadNowPlaySe(0x8000, -1, 0) && rect[0] < story_map.cx)    // left
    {
        story_map.moved = 1;
        story_map.scroll_x -= step;
    }
    if (CheckPadNowPlaySe(0x2000, -1, 0) && story_map.cx < rect[2])    // right
    {
        story_map.moved = 1;
        story_map.scroll_x += step;
    }
    if (CheckPadNowPlaySe(0x1000, -1, 0) && rect[1] < story_map.cy)    // up
    {
        story_map.moved = 1;
        story_map.scroll_y -= step;
    }
    if (CheckPadNowPlaySe(0x4000, -1, 0) && story_map.cy < rect[3])    // down
    {
        story_map.moved = 1;
        story_map.scroll_y += step;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fedc8: the new floor fading in; back to the main step once the
// fade is over.

static short StoryMapStepFloorIn(void)
{
    story_map_screen.Cur()->SetPlayer(story_map.floor, story_map.cx, story_map.cy);
    story_map_screen.Cur()->Pick(story_map.cx, story_map.cy);
    if (story_map_screen.Cur()->fade_frames <= story_map_screen.Cur()->fade_time)
    {
        story_map.step = StoryMapStepMain;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fee78: once the old floor has faded out and the new one is
// loaded, swap the layers, release the old one and fade the new one in.

static short StoryMapStepFloorSwap(void)
{
    int scale;

    if (story_map_screen.Cur()->fade_frames <= story_map_screen.Cur()->fade_time &&
        story_map_screen.Back()->IsReady())
    {
        story_map_screen.SwapLayer();
        story_map_screen.Back()->Release();
        scale = StoryMapGetScale();
        story_map_screen.Cur()->SetScale(scale, scale);
        story_map_screen.Cur()->Fade(0, 0x80, 8);
        story_map_screen.Cur()->PlayDefault();
        story_map_screen.Cur()->SetPlayer(story_map.floor, story_map.cx, story_map.cy);
        story_map.step = StoryMapStepFloorIn;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fefa8: the floor changed -- load it into the back layer and
// fade the shown one out.

static short StoryMapStepFloorOut(void)
{
    story_map_screen.LoadFloor(story_map.floor, story_map.zoom == 1);
    story_map_screen.Cur()->Fade(0x80, 0, 8);
    story_map.step = StoryMapStepFloorSwap;
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ff028: scroll, then OK zooms, L1 / R1 change floor, cancel
// (unless PAD_KEY_10 is pressed too) closes back to the phase that opened
// the map.

static short StoryMapStepMain(void)
{
    short next = -1;
    int   floor;

    StoryMapScrollInput(STORY_MAP_SCROLL_STEP);
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
    {
        StoryMapSetZoom(story_map.zoom + 1);
        return -1;
    }
    if (CheckPadRptPlaySe(0x0004, -1, 0))                               // L1: down a floor
    {
        floor = story_map.floor - 1;
    }
    else if (CheckPadRptPlaySe(0x0008, -1, 0))                          // R1: up a floor
    {
        floor = story_map.floor + 1;
    }
    else
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_10], -1, 0) &&
            CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], -1, 0))
        {
            story_map_screen.PlayLabel("out");                         // sdata 0x3710e8
            next = story_map_from;
            if (story_map_from != STORY_MAP_FROM_MENU)
            {
                MenuBgStop();
            }
        }
        goto draw;
    }
    if (StoryMapSetFloor(floor))
    {
        story_map.step = StoryMapStepFloorOut;
        return -1;
    }

draw:
    story_map_screen.Cur()->SetPlayer(story_map.floor, story_map.cx, story_map.cy);
    story_map_screen.ArrowUpdate(story_map.moved);
    story_map_screen.Cur()->Pick(story_map.cx, story_map.cy);
    return next;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ff1d0: every frame of the phase, the end included.

static void StoryMapDraw(void)
{
    int rect[4];
    int scale;

    GetCmnFadeLayer()->Update();
    story_map_screen.Draw((int)-story_map.scroll_x, (int)-story_map.scroll_y, MenuBgGetHandle());
    scale = StoryMapGetScale();
    MapSnapMarkDatDraw(rect, (int)-story_map.scroll_x, (int)-story_map.scroll_y, scale, scale);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 58 StoryMenu_Map
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001ff310. Step 0: the state, the layouts, the shown floor and the
// CLUTs start loading (the map button also turns the stick into a d-pad);
// step 1: once all is loaded, play the way in, scale and show the floor,
// find the screen centre, put the fade layer on the menu layout and start
// the scroll where the player marker sits at the centre.

int init_StoryMenu_Map(void)
{
    CMapLayer *layer;
    int        scale;

    BpCtlMain();
    GetCmnFadeLayer()->Update();
    switch (GetGPhaseCtrl()->leaf->state_cnt)
    {
    case 0:
        StoryMapStateInit();
        story_map_from = GetGPhaseCtrl()->from;
        if (story_map_from != STORY_MAP_FROM_MENU)
        {
            pad_wrk[0].stick_dpad = 1;
        }
        story_map_screen.LoadReq();
        story_map_screen.LoadFloor(story_map.floor, story_map.zoom == 1);
        MapScreenClutLoad();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        break;

    case 1:
        if (!story_map_screen.IsReady() || !MapScreenClutIsLoaded())
        {
            break;
        }
        story_map_screen.Open(story_map_from == STORY_MAP_FROM_MENU ? "in" : "select_in");  // sdata 0x3710f8
        story_map_screen.SwapLayer();
        scale = StoryMapGetScale();
        layer = story_map_screen.Cur();
        layer->SetScale(scale, scale);
        layer->Fade(0x80, 0x80, 0);
        layer->PlayDefault();
        story_map_screen.GetCentre(&story_map.cx, &story_map.cy);
        GetCmnFadeLayer()->SetItem(story_map_screen.menu, 0x3ff);
        GetCmnFadeLayer()->Show(3);
        layer->SetPlayer(story_map.floor, story_map.cx, story_map.cy);
        story_map.scroll_x = (float)layer->x;
        story_map.scroll_y = (float)layer->y;
        return 1;
    }
    return 0;
}

// FF3 0x001ff548: drawn until the way out has played, then the release.

int end_StoryMenu_Map(void)
{
    StoryMapDraw();
    if (!story_map_screen.IsClosed())
    {
        return 0;
    }
    if (story_map_from != STORY_MAP_FROM_MENU)
    {
        pad_wrk[0].stick_dpad = 0;
    }
    StoryMapRelease();
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    return 1;
}

// FF3 0x001ff5b0

int one_StoryMenu_Map(void)
{
    short next = story_map.step();

    StoryMapDraw();
    if (next != -1)
    {
        GPhaseCtrlRequest(next, 0);
    }
    return next != -1;
}
