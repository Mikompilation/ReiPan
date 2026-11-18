// FILE: ingame/map/MapFog.c
//
// Map fog (FF2 MapFog.c, FF3 0x118900..0x119348). Three MAP_FOG_DAT slots:
//
//   MapFogDat[0]  interpolation start
//   MapFogDat[1]  interpolation end -- the setting being moved towards
//   MapFogDat[2]  the setting in force this frame
//
// Every frame MapFogProc() asks which fog rectangle the camera stands in and
// either snaps MapFogDat[2] to its setting or arms a 16-frame walk towards
// it. It snaps when the camera moved 51 units or more on the ground plane
// since the last frame (FF2: 701, all three axes) or when the camera obscura
// was just raised or lowered, and walks otherwise.
//
// What FF3 changed from FF2:
//  - The setting is floats for the ramp, shorts for the density and bytes
//    for the colour; MapFogAnim() works on MapFogDat directly.
//  - A fog rectangle's stat record carries only an id; the two settings live
//    in the table below (MapFogGetTblDat).
//  - Outside every fog rectangle the area's own fog (its area_data.ecb row)
//    is used; MAPFOG_F_HOLD now marks "on the area fog" so the walk to it is
//    armed once.
//  - Finder mode is the player's camera-up flag (or the obscura views), not
//    FF2's plyr mode 6; the room test also watches the floor.
//  - The sky cross-fade is gone (no MapSkySetAlpha); the 0x20 / 0x40 / 0x80
//    region-class bits are still kept up to date but nothing reads them.
//  - The frame is skipped entirely while the player's light is on.

#include "ingame/map/MapFog.h"

#include "ingame/map/MapHit.h"                  // MapHitRegistCallback
#include "ingame/map/RegDat.h"                  // MB_OUT_RECT, rectangle lookups
#include "ingame/map/hit_check_base.h"          // HcBaseIsInRectXZ
#include "ingame/story_door.h"                  // MapAreaDataGet
#include "ingame/ingame.h"                      // CharPlayerModel
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/plyr/player.h"                 // PLYR_WRK, PLYR_FLG_800000
#include "graphics/graph3d/gra3d.h"             // camera, fog

#include <math.h>                               // sqrtf
#include <string.h>                             // memcpy

// MapFogFlg bits (FF2's names where the bit kept its role).
#define MAPFOG_F_ANIM       0x01    // interpolation running
#define MAPFOG_F_HOLD       0x02    // on the area fog (no region): its walk is armed once
#define MAPFOG_F_FINDER     0x04    // camera up -> dat[1]
#define MAPFOG_F_EVENT      0x08    // scripted override up, regions ignored
#define MAPFOG_F_CHANGE     0x10    // finder mode flipped this frame -> snap
#define MAPFOG_F_SKY        0x20    // camera is in a type-5 region
#define MAPFOG_F_FOG        0x40    // camera is in a type-13 region
#define MAPFOG_F_NOFADE     0x80    // region class unchanged

// Rectangle types the fog regions are registered under; 13 is tested first.
#define MAPFOG_REG_TYPE_FOG_SW  13
#define MAPFOG_REG_TYPE_FOG     5

#define MAPFOG_ANIM_FRAME       16

// A camera move of at least this much (x / z) in one frame is a cut.
#define MAPFOG_CUT_DIST         51

#define MAPFOG_TBL_NUM          0xac

// data 0x2efc28 -- the fog of each fog rectangle id (MDAT_FOG.id): [0]
// normal, [1] with the camera up.
static MAP_FOG_TBL MapFogTbl[MAPFOG_TBL_NUM] =
{
#include "MapFogTbl.inc"
};

// data 0x2f1458 -- [0] interpolation start, [1] end, [2] in force. All three
// carry the same initialiser (FF2's values; the fourth colour byte is 40).
static MAP_FOG_DAT MapFogDat[3] =
{
    { 2000.0f, 18550.0f, 50, 200, 40, 40, 40, 40 },
    { 2000.0f, 18550.0f, 50, 200, 40, 40, 40, 40 },
    { 2000.0f, 18550.0f, 50, 200, 40, 40, 40, 40 },
};

/* bss 0x37fd10 */ static int    MapFogFlg;
/* bss 0x37fd14 */ static int    MapFogLastArea;
/* bss 0x37fd18 */ static int    MapFogLastFloor;
/* bss 0x37fd20 */ static float  MapFogBfoCam[4];   // last frame's camera position
/* bss 0x37fd30 */ static int    MapFogFrame;       // interpolation length
/* bss 0x37fd34 */ static int    MapFogNowFrame;    // frames done so far
/* bss 0x37fd38 */ static float *MapFogRect;        // corners of the rectangle the fog came from

// FF3 0x00118900
MAP_FOG_DAT *MapFogGetTblDat(int id, int finder)
{
    int i;

    for (i = 0; i < MAPFOG_TBL_NUM; i++)
    {
        if (MapFogTbl[i].id == id)
        {
            return &MapFogTbl[i].dat[finder];
        }
    }

    return nullptr;
}

// The area row's setting (FF3 copies it field by field and clears the
// fourth colour byte).
static void MapFogCopyAreaDat(MAP_FOG_DAT *dst, const MAP_FOG_DAT *src)
{
    dst->near_ = src->near_;
    dst->far_  = src->far_;
    dst->min   = src->min;
    dst->max   = src->max;
    dst->r     = src->r;
    dst->g     = src->g;
    dst->b     = src->b;
    dst->a     = 0;
}

// FF3 0x00118950
void MapFogAnimStart(int frame)
{
    MapFogFrame    = frame;
    MapFogNowFrame = 0;
    MapFogFlg     |= MAPFOG_F_ANIM;

    MapFogDat[0] = MapFogDat[2];
}

// FF3 0x001189a0
// One step of the walk from MapFogDat[0] to MapFogDat[1]; the last step
// copies MapFogDat[1] whole.
void MapFogAnim(void)
{
    MAP_FOG_DAT *now = &MapFogDat[2];
    MAP_FOG_DAT *st  = &MapFogDat[0];
    MAP_FOG_DAT *en  = &MapFogDat[1];
    float        wf;

    if (MapFogNowFrame + 1 < MapFogFrame)
    {
        wf = (float)MapFogNowFrame / (float)MapFogFrame;
        MapFogNowFrame++;

        now->near_ = (en->near_ - st->near_) * wf + st->near_;
        now->far_  = (en->far_ - st->far_) * wf + st->far_;
        now->min   = (short)(int)(((float)en->min - (float)st->min) * wf + (float)st->min);
        now->max   = (short)(int)(((float)en->max - (float)st->max) * wf + (float)st->max);
        now->r     = (u_char)(int)(((float)en->r - (float)st->r) * wf + (float)st->r);
        now->g     = (u_char)(int)(((float)en->g - (float)st->g) * wf + (float)st->g);
        now->b     = (u_char)(int)(((float)en->b - (float)st->b) * wf + (float)st->b);
        return;
    }

    *now = *en;
    MapFogFlg &= ~MAPFOG_F_ANIM;
}

// FF3 0x00118b50
// First rectangle of type in slot buff_id whose x / z area holds pos.
static MB_OUT_RECT *MapFogGetRegDat(int buff_id, float *pos, int type)
{
    MB_OUT_RECT *region_p;
    int          reg_num;
    int          i;

    region_p = RegDatGetVecPtr(buff_id, type);
    reg_num  = RegDatGetVecNum(buff_id, type);

    for (i = 0; i < reg_num; i++)
    {
        if (HcBaseIsInRectXZ(pos, &region_p->vec[0][0]))
        {
            return region_p;
        }

        region_p++;
    }

    return nullptr;
}

// FF3 0x00118bf0
// Resolve pos against one slot; type 13 wins over type 5. When neither
// matches MapFogFlg is left alone.
static MDAT_FOG *MapFogUpdate(int buff_id, float *pos)
{
    MB_OUT_RECT *rp;
    MDAT_FOG    *fop;

    rp = MapFogGetRegDat(buff_id, pos, MAPFOG_REG_TYPE_FOG_SW);
    if (rp != nullptr)
    {
        if (MapFogFlg & MAPFOG_F_SKY)
        {
            MapFogFlg = (MapFogFlg & ~(MAPFOG_F_SKY | MAPFOG_F_NOFADE)) | MAPFOG_F_FOG;
        }
        else
        {
            MapFogFlg |= MAPFOG_F_FOG | MAPFOG_F_NOFADE;
        }
    }
    else
    {
        rp = MapFogGetRegDat(buff_id, pos, MAPFOG_REG_TYPE_FOG);
        if (rp == nullptr)
        {
            return nullptr;
        }

        if (MapFogFlg & MAPFOG_F_FOG)
        {
            MapFogFlg = (MapFogFlg & ~(MAPFOG_F_FOG | MAPFOG_F_NOFADE)) | MAPFOG_F_SKY;
        }
        else
        {
            MapFogFlg |= MAPFOG_F_SKY | MAPFOG_F_NOFADE;
        }
    }

    fop = (MDAT_FOG *)RegDatGetStPtr(buff_id, rp->reg_id);  // FF2's MapFogGetRegStat, inlined
    if (fop != nullptr)
    {
        MapFogRect = &rp->vec[0][0];
    }

    return fop;
}

// FF3 0x00118cd8
void MapFogStartFogEv(float near_, float far_, int r, int g, int b, int min, int max)
{
    MapFogAnimStart(MAPFOG_ANIM_FRAME);

    MapFogDat[1].near_ = near_;
    MapFogDat[1].far_  = far_;
    MapFogDat[1].r     = (u_char)r;
    MapFogDat[1].g     = (u_char)g;
    MapFogDat[1].b     = (u_char)b;
    MapFogDat[1].min   = (short)min;
    MapFogDat[1].max   = (short)max;

    MapFogFlg |= MAPFOG_F_EVENT;
}

// FF3 0x00118d90
// Dropping the event flag lets the next MapFogProc() sample the map again.
void MapFogEndFogEv(int frame)
{
    (void)frame;

    MapFogRect = nullptr;
    MapFogFlg &= ~MAPFOG_F_EVENT;
}

// FF3 0x00118db0
// The fog record covering pos. Several slots holding pos: the first one with
// a fog rectangle there wins.
static MDAT_FOG *MapFogGetNewDat(int area_no, int floor, float *pos)
{
    int       buff_id;
    int      *lp;
    int       i;
    MDAT_FOG *fop;

    (void)area_no;

    buff_id = RegDatGetBuffIDG(floor, pos);

    if (-4 < buff_id && buff_id < -1)
    {
        lp = RegDatGetHitList();

        for (i = 0; i < RegDatGetHitNum(); i++)
        {
            fop = MapFogUpdate(*lp++, pos);
            if (fop != nullptr)
            {
                return fop;
            }
        }

        return nullptr;
    }

    if (buff_id == -1)
    {
        return nullptr;
    }

    return MapFogUpdate(buff_id, pos);
}

// FF3 0x00118e68
// Arm a walk towards the region pos is in, or (once) towards the area's fog.
static void MapFogAnimNowDat(int area_no, int floor, float *pos)
{
    MDAT_FOG     *fop;
    MAP_AREA_DAT *area;
    MAP_FOG_DAT  *tbl;

    fop = MapFogGetNewDat(area_no, floor, pos);

    if (fop == nullptr)
    {
        if (MapFogFlg & MAPFOG_F_HOLD)
        {
            return;
        }

        MapFogRect = nullptr;
        MapFogAnimStart(MAPFOG_ANIM_FRAME);

        area = MapAreaDataGet((s_char)area_no);
        if (area == nullptr)                    // PC: guard (the ROM reads past a null row)
        {
            return;
        }

        MapFogCopyAreaDat(&MapFogDat[1], &area->fog[(MapFogFlg >> 2) & 1]);
        MapFogFlg |= MAPFOG_F_ANIM | MAPFOG_F_HOLD;
        return;
    }

    MapFogAnimStart(MAPFOG_ANIM_FRAME);

    tbl = MapFogGetTblDat(fop->id, (MapFogFlg >> 2) & 1);
    if (tbl != nullptr)                         // PC: guard (the ROM reads a missing id at 0)
    {
        MapFogDat[1] = *tbl;
    }

    MapFogFlg = (MapFogFlg & ~MAPFOG_F_HOLD) | MAPFOG_F_ANIM;
}

// FF3 0x00118f98
// The snap path: the setting goes straight into MapFogDat[2]; any walk in
// flight is cancelled.
static void MapFogSetNowDat(int area_no, int floor, float *pos)
{
    MDAT_FOG     *fop;
    MAP_AREA_DAT *area;
    MAP_FOG_DAT  *tbl;

    fop = MapFogGetNewDat(area_no, floor, pos);

    if (fop == nullptr)
    {
        MapFogRect = nullptr;

        area = MapAreaDataGet((s_char)area_no);
        if (area != nullptr)                    // PC: guard (the ROM reads past a null row)
        {
            MapFogCopyAreaDat(&MapFogDat[2], &area->fog[(MapFogFlg >> 2) & 1]);
        }
    }
    else
    {
        tbl = MapFogGetTblDat(fop->id, (MapFogFlg >> 2) & 1);
        if (tbl != nullptr)                     // PC: guard (the ROM reads a missing id at 0)
        {
            MapFogDat[2] = *tbl;
        }

        MapFogFlg &= ~MAPFOG_F_HOLD;

        if (MapFogFlg & MAPFOG_F_SKY)
        {
            MapFogFlg |= MAPFOG_F_NOFADE;
        }
        else if (MapFogFlg & MAPFOG_F_FOG)
        {
            MapFogFlg |= MAPFOG_F_NOFADE;
        }
    }

    MapFogFlg &= ~MAPFOG_F_ANIM;
}

// FF3 0x001190c8
void MapFogReset(void)
{
    MapFogFlg       = 0;
    MapFogLastFloor = -1;
    MapFogFrame     = 0;
    MapFogNowFrame  = 0;
    MapFogRect      = nullptr;
    MapFogLastArea  = -1;
}

// FF3 0x001190f0
// Per frame. The region is sampled at the camera, not the player.
void MapFogProc(int area_no, int floor)
{
    float           *pos = gra3dcamGetPosition();
    PLYR_WRK        *plyr;
    CharPlayerModel *mdl;
    int              finder;
    float            dx;
    float            dz;

    if (MapHitRegistCallback(area_no, floor, pos) != 0)
    {
        return;
    }

    plyr = ingame_wrk.plyr_wrk;
    if (plyr != nullptr && plyr->mdl != nullptr)
    {
        mdl = static_cast<CharPlayerModel *>(plyr->mdl);
        if (mdl->fl_on != 0)
        {
            return;                             // the player's light is on: fog left as it is
        }
    }

    MapFogFlg &= ~MAPFOG_F_CHANGE;

    finder = 0;
    if (plyr != nullptr)                        // PC: guard (the ROM reads through a null work)
    {
        if ((plyr->flags & PLYR_FLG_800000) || (plyr->_0e0 & 1))
        {
            finder = MAPFOG_F_FINDER;
        }
    }

    if ((MapFogFlg ^ finder) & MAPFOG_F_FINDER)
    {
        MapFogFlg |= MAPFOG_F_CHANGE;
    }

    MapFogFlg = (MapFogFlg & ~MAPFOG_F_FINDER) | finder;

    if (MapFogLastArea != area_no || MapFogLastFloor != floor)
    {
        MapFogLastArea  = area_no;
        MapFogLastFloor = floor;
        MapFogFlg      &= ~MAPFOG_F_HOLD;
    }

    if ((MapFogFlg & MAPFOG_F_EVENT) == 0)
    {
        dx = pos[0] - MapFogBfoCam[0];
        dz = pos[2] - MapFogBfoCam[2];

        if ((int)sqrtf(dx * dx + dz * dz) >= MAPFOG_CUT_DIST || (MapFogFlg & MAPFOG_F_CHANGE))
        {
            MapFogSetNowDat(area_no, floor, pos);
        }
        else if (MapFogRect == nullptr || HcBaseIsInRectXZ(pos, MapFogRect) == 0)
        {
            MapFogAnimNowDat(area_no, floor, pos);
        }
    }

    memcpy(MapFogBfoCam, pos, sizeof(MapFogBfoCam));

    if (MapFogFlg & MAPFOG_F_ANIM)
    {
        MapFogAnim();
    }

    gra3dSetFog((float)MapFogDat[2].min, (float)MapFogDat[2].max, MapFogDat[2].near_, MapFogDat[2].far_);
    gra3dSetFogColor(MapFogDat[2].r, MapFogDat[2].g, MapFogDat[2].b);
    gra3dApplyFog();
}
