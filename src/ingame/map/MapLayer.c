// ingame/map/MapLayer.c -- CMapLayer, one floor of the map menu (see MapLayer.h).
// FF3 0x001f38d0..0x001f4448; FF3-only (the FF2 matches in this range were
// wrong kaza2_pzl.c ones). Every method walks the floor's five Bp layouts and
// skips the empty slots (handle -1).
#include "ingame/map/MapLayer.h"

#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/map/MapSnap.h"                 // MapSnapGetStat, MapSnapWorldToMap*, MapSnapItemGetRow
#include "ingame/map/MarkDat.h"                 // CMarkDat::GetPhfNo
#include "ingame/photo/m_plyr_camera.h"         // MapSnapSetDrawLayer
#include "ingame/plyr/player.h"                 // PLYR_WRK pos / rot
#include "system/b2d/B2dCtl.h"                  // B2dCtlSetAlpha / SetScale / SetPos / GetScale, B2dItemGetRect
#include "system/b2d/B2dSpr.h"                  // B2dSprSetDisp / SetRot / SetPos
#include "system/b2d/BpCtl.h"                   // BpCtlLoadReq, BpCtlDrawOne, BpPlayLabel
#include "system/eeiop/cddat.h"                 // GetFileSize

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f38d0

CMapLayer::CMapLayer()
{
    fade_frames = 0;
    alpha_to    = 0x80;
    alpha_from  = 0x80;
    fade_time   = 0;
    ready       = 0;
    row         = nullptr;
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        handle[i] = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3920: the alpha goes from -> to over frames frames, stepped by
// Draw; the layouts start at from.

void CMapLayer::Fade(int from, int to, int frames)
{
    alpha_to    = to;
    alpha_from  = from;
    fade_frames = frames;
    fade_time   = 0;
    SetAlpha(from);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3948: the floor's layouts (MAPS_*_PHF), each one that exists on
// the disc and is not empty.

void CMapLayer::Load(int floor)
{
    Release();
    this->floor = floor;
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        int file = MapSnapGetMarkDat()->GetPhfNo(floor, i);
        if (file >= 0 && GetFileSize(file) != (u_int)-1 && GetFileSize(file) != 0)
        {
            handle[i] = BpCtlLoadReq(file, nullptr);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3a08: once every layout is READY the layer takes its start
// alpha and stays ready until Release. BpCtlDrawOne steps the load.

int CMapLayer::IsReady()
{
    if (ready == 0)
    {
        for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
        {
            if (handle[i] >= 0)
            {
                BpCtlDrawOne(handle[i]);
                if (BpCtlGetState(handle[i]) != BP_CTL_STATE_READY)
                    return ready;
            }
        }
        SetAlpha(alpha_from);
        ready = 1;
    }
    return ready;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3aa8 (CMapScreen::Draw): one fade step, then the layouts, each
// announced to the map hooks (MapSnapSetDrawLayer) first.

void CMapLayer::Draw()
{
    if (IsReady() == 0)
        return;

    if (fade_time < fade_frames)
    {
        SetAlpha((alpha_to - alpha_from) * fade_time / fade_frames + alpha_from);
        fade_time++;
    }
    else
    {
        SetAlpha(alpha_to);
    }

    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        MapSnapSetDrawLayer(floor, i);
        if (handle[i] >= 0)
            BpCtlDrawOne(handle[i]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3b80

void CMapLayer::PlayLabel(const char *label)
{
    if (ready == 0)
        return;
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] >= 0)
            BpPlayLabel(handle[i], label);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3be8

void CMapLayer::PlayDefault()
{
    PlayLabel("NewLabel_0");                    // rodata 0x35b780
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3c08

void CMapLayer::Release()
{
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] >= 0)
        {
            BpCtlRelease(handle[i]);
            handle[i] = -1;
        }
    }
    ready = 0;
    hit   = -1;
    row   = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3c80 (CMapScreen::IsClosed): no layout's longest part plays.
// A layer that is not ready counts as still.

int CMapLayer::IsStill()
{
    if (ready != 0)
    {
        for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
        {
            if (handle[i] >= 0 && B2dPartIsPlaying(B2dCtlGetLongestPart(handle[i])) != 0)
                return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3cf8

void CMapLayer::SetAlpha(int alpha)
{
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] >= 0)
            B2dCtlSetAlpha(handle[i], alpha);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3d58 (CMapScreen::Draw): positions every layout for the map
// scrolled to (x, y). The world origin through floor 2 layout 0's
// transform is the common reference; each layout is shifted by where the
// origin falls in its own transform, at the first layout's scale.

void CMapLayer::SetScroll(int x, int y)
{
    if (ready == 0)
        return;

    const MAP_SNAP_STAT *stat = MapSnapGetStat(2, 0);
    int ox = (int)MapSnapWorldToMapX(0.0f, stat);
    int oy = (int)MapSnapWorldToMapY(0.0f, stat);

    // FF3 indexes handle[] with MapSnapGetFirstLayer unchecked; -1 (a floor
    // without layouts) reads the word before the object. No handle is then
    // valid and the loop below places nothing, so the port skips the read.
    int sx    = 0x400;
    int sy    = 0x400;
    int first = MapSnapGetFirstLayer(floor);
    if (first >= 0)
        B2dCtlGetScale(handle[first], &sx, &sy);

    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] < 0)
            continue;
        stat = MapSnapGetStat(floor, i);
        int px = (x + ox - (int)MapSnapWorldToMapX(0.0f, stat)) * sx / 1024;
        int py = (y + oy - (int)MapSnapWorldToMapY(0.0f, stat)) * sy / 1024;
        B2dCtlSetPos(handle[i], px, py);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3ef8 (no callers)

void CMapLayer::SetRun(int run)
{
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] < 0)
            continue;
        if (run == 0)
            BpCtlSetFlag(handle[i], BP_CTL_FLAG_PAUSE);
        else
            BpCtlClearFlag(handle[i], BP_CTL_FLAG_PAUSE);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3f68

void CMapLayer::SetScale(int sx, int sy)
{
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] >= 0)
            B2dCtlSetScale(handle[i], sx, sy);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f3fd8: the first layout with an item at (x, y); hit names it.

B2D_ITEM *CMapLayer::FindItemAt(int x, int y)
{
    if (ready != 0)
    {
        for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
        {
            if (handle[i] < 0)
                continue;
            B2D_ITEM *item = B2dCtlFindItemAt(handle[i], nullptr, x, y);
            if (item != nullptr)
            {
                hit = i;
                return item;
            }
        }
        hit = -1;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f4078: the item at (x, y) and its room (nullptr without one).

B2D_ITEM *CMapLayer::Pick(int x, int y)
{
    B2D_ITEM *item = FindItemAt(x, y);
    row = MapSnapItemGetRow(item);
    return item;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f40b8 (MapLayerCentreOnItem): the layout origin (*ox, *oy) that
// puts the item's pivot (its anchor point, at the layout's scale) on the
// screen point (cx, cy). Not a method: a0 is the layout handle.

static void MapLayerCentreOnItem(int handle, B2D_ITEM *item, int *ox, int *oy, int cx, int cy)
{
    B2D_RECT r;
    int      sx;
    int      sy;

    B2dItemGetRect(&r, item, nullptr);
    r.x += r.w * item->part[B2D_PART_ANCHORX].value / 1024;
    r.y += r.h * item->part[B2D_PART_ANCHORY].value / 1024;

    B2dCtlGetScale(handle, &sx, &sy);
    r.x = (int)((float)(r.x * sx) * (1.0f / 1024.0f));  // rodata 0x370f3c
    r.y = (int)((float)(r.y * sy) * (1.0f / 1024.0f));
    *ox = ((r.x - cx) * 0x400) / sx;
    *oy = ((r.y - cy) * 0x400) / sy;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001f4208: the player marker ("R player", one per layout). Only the
// first layout of the player's floor shows it: turned to the player's
// facing, placed at the player's position, and the layer's origin (x, y)
// set to centre it on (cx, cy) -- measured with the marker placed through
// floor 2 layout 0's transform, the reference SetScroll uses. Every other
// layout's marker is hidden. Returns the shown marker, nullptr.

B2D_ITEM *CMapLayer::SetPlayer(int floor, int cx, int cy)
{
    // The facing in degrees 0..360 (rodata 0x370f40 180, 0x370f44 pi, 0x370f48 360).
    float rot = ingame_wrk.plyr_wrk->rot[1];
    int   deg;
    if (rot < 0.0f)
        deg = (int)(rot * 180.0f / 3.14159265f + 360.0f);
    else
        deg = (int)(rot * 180.0f / 3.14159265f);

    int       shown  = 0;
    B2D_ITEM *marker = nullptr;
    for (int i = 0; i < MAP_LAYER_LAYOUT_NUM; i++)
    {
        if (handle[i] < 0)
            continue;

        B2D_ITEM *item = B2dCtlFindItemByName(handle[i], "R player");   // rodata 0x35b790
        if (MapSnapGetFloor() != floor || shown)
        {
            B2dSprSetDisp(item, 0);
            continue;
        }

        shown = 1;
        B2dSprSetDisp(item, 1);
        marker = item;
        B2dSprSetRot(item, deg + 180);

        const MAP_SNAP_STAT *stat = MapSnapGetStat(2, 0);
        B2dSprSetPos(item, (int)MapSnapWorldToMapX(ingame_wrk.plyr_wrk->pos[0], stat),
                           (int)MapSnapWorldToMapY(ingame_wrk.plyr_wrk->pos[2], stat));
        MapLayerCentreOnItem(handle[i], item, &x, &y, cx, cy);

        stat = MapSnapGetStat(floor, i);
        B2dSprSetPos(item, (int)MapSnapWorldToMapX(ingame_wrk.plyr_wrk->pos[0], stat),
                           (int)MapSnapWorldToMapY(ingame_wrk.plyr_wrk->pos[2], stat));
    }
    return marker;
}
