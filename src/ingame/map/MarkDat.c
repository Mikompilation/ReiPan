// ingame/map/MarkDat.c -- CMarkDat, the map menu's floor layouts.
// FF3 0x0010e650..0x0010f150; FF3-only (the class file sorts between
// MapRec and MarkDoor; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/map/MarkDat.h"

#include "ingame/ingame_wrk.h"          // ingame_wrk.b03 (the world)
#include "ingame/map/MapRec.h"          // MapRecGet, CMapRec::Reset, CMapRec::Measure
#include "ingame/map/MapSnap.h"         // MapSnapGetFirstLayer, MapSnapGetStat, MapSnapWorldToMap*, MapSnapLayerGetRect
#include "reipan/reipan_memory.h"       // ReiPan_GetHostPointer
#include "system/b2d/B2dCtl.h"          // B2dCtlGetScale, B2dCtlSetPos, B2dCtlSetScale
#include "system/b2d/BpCtl.h"           // BpCtl*, BP_CTL_FLAG_PAUSE
#include "system/eeiop/cd_file_dat.h"   // MAPS_*_B2D
#include "system/eeiop/cddat.h"         // GetFileSize

// An empty world rect (x, z far right, w, h far negative): data
// 0x36b6c0 / 0x36b6c4 (and the same pair at 0x36b6c8 for CMarkDat::Clear).
#define MARK_DAT_RECT_FAR   2147483520.0f   // 0x4effffff, the largest float below 2^31
#define MARK_DAT_RECT_NEG   -2147483648.0f  // 0xcf000000

// Where MarkDatLoadAll puts the layouts on the EE (the files follow each
// other, 16-aligned).
#define MARK_DAT_LOAD_ADDR  0xfcf000

// ──────────────────────────────────────────────────────────────────────
// data 0x34c400: the rec layouts per floor and layer. Floors 0..4 are
// B2F..3F, 5..7 world 1's 1F..3F (CMarkDat::GetFloor). Each file's _PHF
// (the next number) is the matching map picture (CMarkDat::GetPhfNo).

int mark_dat_file[CMARK_FLOOR_NUM][CMARK_LAYER_NUM] =
{
    { -1,             MAPS_B2F_2_B2D, -1,             -1,             MAPS_B2F_5_B2D },
    { -1,             MAPS_B1F_2_B2D, MAPS_B1F_3_B2D, MAPS_B1F_4_B2D, MAPS_B1F_5_B2D },
    { MAPS_1F_1_B2D,  MAPS_1F_2_B2D,  MAPS_1F_3_B2D,  MAPS_1F_4_B2D,  MAPS_1F_5_B2D  },
    { MAPS_2F_1_B2D,  MAPS_2F_2_B2D,  MAPS_2F_3_B2D,  MAPS_2F_4_B2D,  MAPS_2F_5_B2D  },
    { MAPS_3F_1_B2D,  MAPS_3F_2_B2D,  MAPS_3F_3_B2D,  -1,             MAPS_3F_5_B2D  },
    { MAPS_1F_KSW_B2D, -1,            -1,             -1,             -1             },
    { MAPS_2F_KSW_B2D, -1,            -1,             -1,             -1             },
    { MAPS_3F_KSW_B2D, -1,            -1,             -1,             -1             },
};

// The label every layer plays once loaded (data 0x34c3f0).
static const char mark_dat_label[] = "NewLabel_0";

// ──────────────────────────────────────────────────────────────────────
// A layer with no layout: handle -1, an empty rect.

static void MarkDatLayerInit(CMARK_LAYER *layer)
{
    layer->handle  = -1;
    layer->_00     = 1;
    layer->played  = 0;
    layer->rect[0] = MARK_DAT_RECT_FAR;
    layer->rect[1] = MARK_DAT_RECT_FAR;
    layer->rect[2] = MARK_DAT_RECT_NEG;
    layer->rect[3] = MARK_DAT_RECT_NEG;
}

// ──────────────────────────────────────────────────────────────────────
// Every layer takes the Bp handle of its file (loaded at boot by
// MarkDatLoadAll), -1 when the table has none.
// FF3 0x0010e770 (MarkDatCtor; FF3 0x0010e650 is the same body).

CMarkDat::CMarkDat()
{
    busy   = 0;
    loaded = 0;

    for (int f = 0; f < CMARK_FLOOR_NUM; f++)
    {
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            int file = mark_dat_file[f][l];

            MarkDatLayerInit(&layer[f][l]);
            if (file >= 0)
                layer[f][l].handle = BpCtlGetHandleByFileNo(file);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e8c0 (the deleting one: 0x0010e8f0).

CMarkDat::~CMarkDat()
{
    loaded = 0;
    busy   = 0;
    Clear();
}

// ──────────────────────────────────────────────────────────────────────
// The table floor of a map floor: world 1 uses floors 5..7 for 2..4
// (except while MapSnapLayerGetRect runs, which passes table floors);
// above 7 folds back by 5.
// FF3 0x0010e930.

int CMarkDat::GetFloor(int floor)
{
    if (ingame_wrk.b03 == 1 && busy == 0)
        floor += 3;
    if (floor < 0)
        floor = 0;
    if (floor > 7)
        floor -= 5;
    return floor;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e968.

int CMarkDat::GetPhfNo(int floor, int layer)
{
    int file = mark_dat_file[GetFloor(floor)][layer];

    if (file < 0)
        return -1;
    return file + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e9c0.

int CMarkDat::GetWorldFloor(int floor)
{
    if (ingame_wrk.b03 == 1)
        floor += 3;
    return floor;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e9e0.

int CMarkDat::GetHandle(int floor, int layer)
{
    if (loaded == 0)
        return -1;

    int handle = this->layer[GetFloor(floor)][layer].handle;
    if (handle < 0)
        return -1;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// Places every layer of a floor at x, y: each layer's map origin is taken
// relative to 1F's first layer (floor 2, layer 0), scaled by the floor's
// first layer's B2d scale (0x400 = 1.0).
// FF3 0x0010ea48.

void CMarkDat::SetPos(int floor, int x, int y)
{
    int first = MapSnapGetFirstLayer(floor);

    // PC: FF3 reads layer -1 (outside the floor) when the floor has no
    // layout; every table floor has one, so this never returns early there.
    if (first < 0)
        return;

    int handle = GetHandle(floor, first);
    if (handle < 0)
        return;

    const MAP_SNAP_STAT *base = MapSnapGetStat(2, 0);
    if (base == nullptr)                // PC: FF3 would read through nullptr
        return;

    int base_x = (int)MapSnapWorldToMapX(0.0f, base);
    int base_y = (int)MapSnapWorldToMapY(0.0f, base);
    int sx, sy;
    B2dCtlGetScale(handle, &sx, &sy);

    for (int l = 0; l < CMARK_LAYER_NUM; l++)
    {
        int h = GetHandle(floor, l);
        if (h < 0)
            continue;

        const MAP_SNAP_STAT *stat = MapSnapGetStat(floor, l);
        if (stat == nullptr)            // PC: as above
            continue;

        int px = (x + base_x - (int)MapSnapWorldToMapX(0.0f, stat)) * sx / 1024;
        int py = (y + base_y - (int)MapSnapWorldToMapY(0.0f, stat)) * sy / 1024;
        B2dCtlSetPos(h, px, py);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010ebf8.

void CMarkDat::SetScale(int floor, int sx, int sy)
{
    for (int l = 0; l < CMARK_LAYER_NUM; l++)
    {
        int h = GetHandle(floor, l);
        if (h >= 0)
            B2dCtlSetScale(h, sx, sy);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The map floors of the current world: 0..4 (B2F..3F), world 1 only 2..4
// (1F..3F).
// FF3 0x0010ec80.

void MarkDatGetFloorRange(int *lo, int *hi)
{
    static const int range[2][2] = { { 0, 5 }, { 2, 5 } };
    int w = ingame_wrk.b03 == 1;

    *lo = range[w][0];
    *hi = range[w][1];
}

// ──────────────────────────────────────────────────────────────────────
// Places and scales every floor of the world, draws each layer once
// through MapRec's measuring hooks (pause flag off meanwhile) and returns
// the screen rect they cover (x0, y0, x1, y1).
// FF3 0x0010ece0 (MapSnapMarkDatDraw's).

void CMarkDat::Measure(int *rect, int x, int y, int sx, int sy)
{
    CMapRec *rec = MapRecGet();
    int      lo, hi;

    rec->Reset();
    MarkDatGetFloorRange(&lo, &hi);
    for (int f = lo; f < hi; f++)
    {
        SetPos(f, x, y);
        SetScale(f, sx, sy);
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            int h = GetHandle(f, l);
            if (h < 0)
                continue;

            BpCtlClearFlag(h, BP_CTL_FLAG_PAUSE);
            rec->Measure(h);
            BpCtlSetFlag(h, BP_CTL_FLAG_PAUSE);
        }
    }

    for (int i = 0; i < 4; i++)
        rect[i] = rec->rect[i];
}

// ──────────────────────────────────────────────────────────────────────
// Boot: requests every layout of the table to EE 0xfcf000.., one after the
// other (16-aligned), paused; a file the disc does not have (size -1 or 0)
// leaves the table.
// FF3 0x0010ee40 (was FurnBpLoadAll).

void MarkDatLoadAll(void)
{
    uintptr_t addr = MARK_DAT_LOAD_ADDR;

    for (int f = 0; f < CMARK_FLOOR_NUM; f++)
    {
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            int file = mark_dat_file[f][l];
            if (file < 0)
                continue;

            if (GetFileSize(file) == (u_int)-1 || GetFileSize(file) == 0)
            {
                mark_dat_file[f][l] = -1;
                continue;
            }

            int handle = BpCtlLoadReq(file, ReiPan_GetHostPointer(addr));
            addr += (GetFileSize(file) + 15) & ~15u;
            BpCtlSetFlag(handle, BP_CTL_FLAG_PAUSE);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// A file whose Bp entry is gone (BpCtlIsDataReady -1) counts as ready.
// FF3 0x0010ef50.

int CMarkDat::IsReady()
{
    if (loaded != 0)
        return 1;

    for (int f = 0; f < CMARK_FLOOR_NUM; f++)
    {
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            int file = mark_dat_file[f][l];
            if (file >= 0 && BpCtlIsDataReady(BpCtlGetHandleByFileNo(file)) == 0)
                return 0;
        }
    }

    loaded = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Once loaded, every layer not yet set up starts its label and gets the
// world rect of its room items. The floor passed on is the table floor.
// FF3 0x0010f010.

void CMarkDat::SetupLayers()
{
    if (loaded == 0)
        return;

    for (int f = 0; f < CMARK_FLOOR_NUM; f++)
    {
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            CMARK_LAYER *layer = &this->layer[f][l];

            if (layer->handle >= 0 && layer->played == 0)
            {
                BpPlayLabel(layer->handle, mark_dat_label);
                MapSnapLayerGetRect(f, l, layer->rect);
                layer->played = 1;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010f0d8.

void CMarkDat::Clear()
{
    for (int f = 0; f < CMARK_FLOOR_NUM; f++)
    {
        for (int l = 0; l < CMARK_LAYER_NUM; l++)
        {
            if (layer[f][l].handle >= 0)
                MarkDatLayerInit(&layer[f][l]);
        }
    }
}
