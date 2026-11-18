/* ==========================================================================
 *  ingame/plyr/man_data.c
 *
 *  The rest of CMapScreen, the map menu's screen object (m_plyr_camera.h):
 *  loading a floor into the back layer, opening and closing the frame and
 *  menu layouts, the four scroll arrows, and the draw. FF3 0x1f8230..
 *  0x1f8af8, FF3-only; the file name is still the link-order guess (FF2's
 *  MAN_DATA / map_bgm matches here were wrong low matches; the code shares
 *  m_plyr_camera.c's sdata and sbss, so the two are probably one FF3
 *  object: open-items.md).
 * ======================================================================== */

#include "ingame/photo/m_plyr_camera.h"

#include <libgraph.h>                   // SCE_GS_* / SCE_GIF_*

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::Draw
#include "graphics/graph2d/g2d_draw.h"  // StartDmaDirectTrans, EndDmaDirectTrans
#include "graphics/gs_reg.h"            // SetGsRegCached
#include "graphics/motion/anicode.h"    // B2dSetHookList
#include "ingame/map/MapRec.h"          // CMapRec, MapRecGet
#include "ingame/map/MapSnap.h"         // MAP_SNAP_ROOM, MAP_SNAP_FLG_VISITED
#include "ingame/menu/menu_draw_tex.h"  // MENU_PIC, Menu_DrawTex
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dSetSpriteFunc, B2dSetQuadFunc
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData, B2dSprFreezePart, B2dSprSetAlpha
#include "system/b2d/BpCtl.h"           // BpCtl*, BpPlayLabel*, B2dCtlGetLongestPart, B2dPartIsPlaying
#include "system/eeiop/cd_file_dat.h"   // RS101_R01_TM2
#include "system/pad/pad_ctrl.h"        // CheckPadNowPlaySe

// The four scroll arrows of the MAPMENU layout and the pad direction that
// lights each (rodata 0x35b868, right after the hook list).
struct MAP_SCREEN_ARROW
{
    int id;                             // menu item
    int pad;                            // held mask
};
static_assert(sizeof(MAP_SCREEN_ARROW) == 8, "MAP_SCREEN_ARROW is 8 bytes (rodata 0x35b868)");

static const MAP_SCREEN_ARROW map_screen_arrow[4] =
{
    { 0x407, 0x1000 },                  // up
    { 0x3f9, 0x8000 },                  // left
    { 0x3f8, 0x4000 },                  // down
    { 0x408, 0x2000 },                  // right
};

// FF3 walks the layout handles as one int array from frame: frame, menu,
// then on into the first words of layer[0] (its handle[]): three of them
// in IsClosed, five in IsReady and Release. Same words on the host
// (layer[0].handle is at +8 there too); spelled out here.

static int *MapScreenHandle(CMapScreen *s, int i)
{
    if (i == 0)
    {
        return &s->frame;
    }
    if (i == 1)
    {
        return &s->menu;
    }
    return &s->layer[0].handle[i - 2];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8230: the floor's layouts into the back layer (SwapLayer shows
// it once loaded). zoomed is not read.

void CMapScreen::LoadFloor(int floor, int zoomed)
{
    (void)zoomed;
    Back()->Load(floor);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8260: the frame and menu play label ("in", or "select_in" from
// the save point), the window and backdrop items, and the room picture
// (0x6c x 0x3c, drawn by MapScreenRoomPicHook).
// callcheck: operator_new, MenuPicCtor (new MENU_PIC)

void CMapScreen::Open(const char *label)
{
    help = 0;
    BpPlayLabel(frame, label);
    BpPlayLabel(menu, label);
    win     = B2dCtlFindItem(frame, 0x3ec);
    bg_item = B2dCtlFindItem(frame, 0x3eb);

    map_screen_pic    = new MENU_PIC(0);
    map_screen_pic->v = 0;
    map_screen_pic->u = 0;
    map_screen_pic->h = 0x3c;
    map_screen_pic->w = 0x6c;
    if (map_screen_pic->load[0] != nullptr)
    {
        map_screen_pic->load[0]->direct = 1;
    }
    if (map_screen_pic->load[1] != nullptr)
    {
        map_screen_pic->load[1]->direct = 1;
    }
    map_screen_pic->enable = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8328: 1 once both layers are still and no label plays on the
// frame, the menu or layer[0]'s first layout (MapScreenHandle).

int CMapScreen::IsClosed()
{
    for (int i = 0; i < 2; i++)
    {
        if (!layer[i].IsStill())
        {
            return 0;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        int handle = *MapScreenHandle(this, i);

        if (handle >= 0 && B2dPartIsPlaying(B2dCtlGetLongestPart(handle)))
        {
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f83d0: the menu's help line on or off, played only on a
// change. No callers in FF3.

void CMapScreen::SetHelp(int on)
{
    if (on != help)
    {
        help = on;
        BpPlayLabel(menu, on != 0 ? "help_on" : "help_off");   // sdata 0x370fe8 / rodata
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8418: the menu first, then the frame ("out" closes the map).

void CMapScreen::PlayLabel(const char *label)
{
    BpPlayLabelEx(menu, label, 0);
    BpPlayLabelEx(frame, label, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8460: the mean of the four arrows' positions, each moved by a
// quarter of its size (parts 3 / 4) -- the map's centre on screen.

void CMapScreen::GetCentre(int *x, int *y)
{
    *y = 0;
    *x = 0;
    for (int i = 0; i < 4; i++)
    {
        B2D_ITEM *item = B2dCtlFindItem(menu, map_screen_arrow[i].id);
        int       ix   = B2dSprGetPartData(item, 6);
        int       iy   = B2dSprGetPartData(item, 7);

        ix += B2dSprGetPartData(item, 3) >> 2;
        iy += B2dSprGetPartData(item, 4) >> 2;
        *x += ix;
        *y += iy;
    }
    *x /= 4;
    *y /= 4;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8580: while the map scrolls (moving), an arrow whose direction
// is held runs its alpha track; the others stop at alpha 0x20. Not moving:
// every arrow stopped and hidden.

void CMapScreen::ArrowUpdate(int moving)
{
    for (int i = 0; i < 4; i++)
    {
        int       id   = map_screen_arrow[i].id;
        B2D_ITEM *item = B2dCtlFindItem(menu, id);

        if (moving == 0)
        {
            B2dSprFreezePart(menu, id, 10, 0);
            B2dSprSetAlpha(item, 0);
        }
        else if (CheckPadNowPlaySe((u_short)map_screen_arrow[i].pad, -1, 0) == 0)
        {
            B2dSprFreezePart(menu, id, 10, 0);
            B2dSprSetAlpha(item, 0x20);
        }
        else
        {
            B2dSprFreezePart(menu, id, 10, 1);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8668: both layers loaded, then the frame, the menu and
// layer[0]'s first three layouts (MapScreenHandle) each stepped once and
// ready.

int CMapScreen::IsReady()
{
    for (int i = 0; i < 2; i++)
    {
        if (!layer[i].IsReady())
        {
            return 0;
        }
    }

    for (int i = 0; i < 5; i++)
    {
        int handle = *MapScreenHandle(this, i);

        if (handle >= 0)
        {
            BpCtlDrawOne(handle);
            if (BpCtlGetState(handle) != BP_CTL_STATE_READY)
            {
                return 0;
            }
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f8710: both layers, the frame and menu (and layer[0]'s first
// three handles, already -1 by then: MapScreenHandle), the room picture.
// callcheck: MenuPicDtor, operator_delete (delete MENU_PIC)

void CMapScreen::Release()
{
    for (int i = 0; i < 2; i++)
    {
        layer[i].Release();
    }

    for (int i = 0; i < 5; i++)
    {
        int *handle = MapScreenHandle(this, i);

        if (*handle >= 0)
        {
            BpCtlRelease(*handle);
            *handle = -1;
        }
    }

    if (map_screen_pic != nullptr)
    {
        delete map_screen_pic;
        map_screen_pic = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f87b0: the map menu's frame. Backdrop (the menu background's
// layout bg, or with bg < 0 a black screen at 0.4 x the backdrop item's
// alpha), the frame; then, scissored to the frame's window item, the shown
// floor scrolled to (x, y) through the map sprite / quad hooks, the
// current room on top and the door marks; then the menu layout with its
// item hooks (area name, room name, room picture) and the common fade
// layer. Nothing on a frame BpCtl skips.

void CMapScreen::Draw(int x, int y, int bg)
{
    if (BpCtlIsFrameDone())
    {
        return;
    }

    BpCtlFrameSkip();
    door.Clear();
    Cur()->SetScroll(x, y);
    map_screen_spr_num = 0;
    map_screen_pic->Step();
    map_screen_drawn = this;
    BpCtlDrawBegin();

    if (bg < 0)
    {
        int     alpha = (int)((float)B2dSprGetPartData(bg_item, 10) * 0.4f);  // sdata 0x370ff8
        u_long *p     = (u_long *)StartDmaDirectTrans();
        u_int  *q;

        p[0] = SCE_GIF_SET_TAG(1, 1, 1, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, 1, 0, 0, 0, 0),
                               SCE_GIF_PACKED, 3);
        p[1] = (u_long)SCE_GS_XYZF2 << 8 | (u_long)SCE_GS_XYZF2 << 4 | SCE_GS_RGBAQ;

        q     = (u_int *)&p[2];
        q[0]  = 0;                      // RGBAQ: black
        q[1]  = 0;
        q[2]  = 0;
        q[3]  = alpha;
        q[4]  = 0x6bf0;                 // XYZF2: (-1, -1) .. (640, 480) at the GS offset (1728, 1824)
        q[5]  = 0x71f0;
        q[6]  = 0;
        q[7]  = 0;
        q[8]  = 0x9400;
        q[9]  = 0x9000;
        q[10] = 0;
        q[11] = 0;
        EndDmaDirectTrans((qword *)&q[12]);
    }
    else
    {
        BpCtlDrawOne(bg);
    }
    BpCtlDrawOne(frame);

    // The window item's rectangle: CMapRec.view (x0, y0, x1, y1 as
    // written here) and the scissor.
    {
        int      wx  = B2dSprGetPartData(win, 6);
        int      wy  = B2dSprGetPartData(win, 7);
        CMapRec *rec = MapRecGet();
        int      ww  = B2dSprGetPartData(win, 3);
        int      wh  = B2dSprGetPartData(win, 4);

        rec->view[1] = wy;
        rec->view[2] = wx + ww;
        rec->view[3] = wy + wh;
        rec->view[0] = wx;
        SetGsRegCached(SCE_GS_SET_SCISSOR(rec->view[0], rec->view[2], rec->view[1], rec->view[3]),
                       SCE_GS_SCISSOR_1, 1);
    }

    B2dSetSpriteFunc(MapScreenSpriteHook);
    B2dSetQuadFunc(MapScreenQuadHook);
    SetGsRegCached(0, SCE_GS_CLAMP_1, 1);

    map_screen_bg_item = bg_item;
    map_screen_row     = Cur()->row;
    if (map_screen_row != nullptr && (map_screen_row->flags & MAP_SNAP_FLG_VISITED))
    {
        map_screen_pic->pic = map_screen_row->snap_no + RS101_R01_TM2;     // the room's picture
    }
    else
    {
        map_screen_pic->pic = -2;       // blank
    }

    SetGsRegCached(0x100000021ULL, SCE_GS_TEX1_1, 1);          // LCM 1, MMAG linear, K 1
    Cur()->Draw();
    MapScreenDrawCurRoom();
    door.Draw();
    SetGsRegCached(SCE_GS_SET_SCISSOR(0, 639, 0, 447), SCE_GS_SCISSOR_1, 1);   // sdata 0x370ff0
    BpCtlSetHooks();
    SetGsRegCached(0x100000000ULL, SCE_GS_TEX1_1, 1);

    // FF3 copies the hook list to the stack and passes the copy.
    B2dSetHookList(menu, map_screen_menu_hooks);
    BpCtlDrawOne(menu);
    GetCmnFadeLayer()->Draw();
    BpCtlDrawEnd();
    map_screen_drawn = nullptr;
}
