// ingame/map/map_hit_check.c -- wall collision and line of sight.
// FF3 0x001fe3d8..0x001fea48; FF2 map_hit_check.c (MioPan port for the two
// push helpers), the rest FF3's own.
//
// FF3 against FF2: MapHitCollisionLine / Point are FF2's (the float pair now
// first in the C prototype; the EE passes them in f12 / f13 either way).
// FF2's MapHitCheck walked map_rectangle.c's hit-rectangle and door caches;
// FF3 hands one quad at a time to MapHitPushOutQuad from the room's rcb walk
// (RcbPushOut) and the door / event quads (MapLBuffWallPushOut), and
// MrecWallPushOut / MapHitTourouPushOut run the 4-round loop around them.
// FF2's MapHitLineCheck (both buffers, widened box) became a test of the
// segment against the RegDat type 0x18 rectangles plus the door / event quads.

#include "ingame/map/map_hit_check.h"

#include <math.h>                               // sqrtf

#include "ingame/map/MapLight.h"                // MapLBuffWallPushOut, MapLBuffLineCheck
#include "ingame/map/MapLoad.h"                 // MapLoadGetBuffID, MapLoadGetFreeMemAddr, MapLoadGetRoomData
#include "ingame/map/MapRcb.h"                  // RcbPushOut, RCB_KIND_HIT, RCB_KIND_TOUROU
#include "ingame/map/RegDat.h"                  // RegDatGetBuffIDG, RegDatGetHitList, ...
#include "ingame/map/hit_check_base.h"          // HcBasePointRectangle, HcBaseIsLineHitRectXZ
#include "ingame/map/map_rectangle.h"           // MREC_HIT
#include "sdk/libvu0.h"

// The RegDat rectangle type the line test reads (blocks the line of sight).
#define MAP_HIT_LINE_RECT   0x18

// Rounds of push-out before a circle counts as wedged.
#define MAP_HIT_PUSH_MAX    4

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe3d8. The 0.01 keeps the next frame from starting exactly on
// the edge; the ROM adds it in double precision (data 0x3710c8).
// callcheck: fptodp, __adddf3, dptofp (that soft-float double add)

int MapHitCollisionLine(float len, float r, float *v0, float *v1, float *a, float *b)
{
    alignas(16) float ab[4];
    alignas(16) float av[4];
    alignas(16) float ver[4];

    sceVu0SubVector(ab, b, a);
    sceVu0SubVector(av, v1, a);

    ver[0] = ab[2];
    ver[1] = 0.0f;
    ver[2] = -ab[0];
    ver[3] = 0.0f;

    // The sign of the XZ cross product says which side of the edge v1 is on.
    if (ab[2] * av[0] - ab[0] * av[2] < 0.0f)
    {
        sceVu0ScaleVector(ver, ver, -1.0f);
    }

    float ver_len = sqrtf(sceVu0InnerProduct(ver, ver));

    sceVu0ScaleVector(ver, ver, ((float)((double)r + 0.01) - len) / ver_len);
    sceVu0AddVector(v0, ver, v1);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe520 (0.01 from data 0x3710d8).
// callcheck: fptodp, __adddf3, dptofp (the double add, as above)

int MapHitCollisionPoint(float len, float r, float *v0, float *v1, float *a)
{
    alignas(16) float av[4];

    sceVu0SubVector(av, v1, a);
    av[1] = 0.0f;

    float av_len = sqrtf(sceVu0InnerProduct(av, av));

    sceVu0ScaleVector(av, av, ((float)((double)r + 0.01) - len) / av_len);
    sceVu0AddVector(v0, av, v1);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe5f8 (MapLBuffWallPushOut, RcbPushOutCb).

int MapHitPushOutQuad(float r, float *pos, const float *quad)
{
    alignas(16) float a[4];
    alignas(16) float b[4];
    float len;

    switch (HcBasePointRectangle(&len, pos, a, b, quad, r))
    {
    case 1:                             // nearest feature a corner
        MapHitCollisionPoint(len, r, pos, pos, a);
        return 1;
    case 2:                             // an edge
        MapHitCollisionLine(len, r, pos, pos, a, b);
        return 1;
    default:
        return 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe6a8 (MapAreaHitCircle). A round ends the loop when neither the
// room walls nor the door / event quads moved the circle.

int MrecWallPushOut(float r, float *out, float *pos, float *old, int area, short floor, int mh_no)
{
    MAP_ROOM_DAT *dat = MapLoadGetFreeMemAddr(MapLoadGetBuffID(area));
    alignas(16) float tmp[4];
    int i;

    sceVu0CopyVector(tmp, pos);
    if (dat == nullptr)
    {
        return 0;
    }

    for (i = 0; i < MAP_HIT_PUSH_MAX; i++)
    {
        void *hit = MapLoadGetRoomData(dat, 3);

        // The ROM passes dat->room as MapLBuffWallPushOut's unread first word.
        if (RcbPushOut(r, hit, tmp, tmp, (short)mh_no, RCB_KIND_HIT, 0) == 0
            && MapLBuffWallPushOut(r, floor, tmp) == 0)
        {
            break;
        }
    }

    if (i == 0)
    {
        sceVu0CopyVector(out, pos);     // nothing hit
        return 0;
    }
    sceVu0CopyVector(out, i == MAP_HIT_PUSH_MAX ? old : tmp);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe7d8 (LanternCtrlStep). Unlike MrecWallPushOut a wedged circle
// returns 2, and a null hit returns 0 without touching out.

int MapHitTourouPushOut(float r, float *out, float *pos, float *old, void *hit, int mh_no)
{
    alignas(16) float tmp[4];
    int i;

    if (hit == nullptr)
    {
        return 0;
    }

    sceVu0CopyVector(tmp, pos);
    for (i = 0; i < MAP_HIT_PUSH_MAX; i++)
    {
        if (RcbPushOut(r, hit, tmp, tmp, (short)mh_no, RCB_KIND_TOUROU, 0) == 0)
        {
            break;
        }
    }

    if (i == 0)
    {
        sceVu0CopyVector(out, pos);
        return 0;
    }
    if (i == MAP_HIT_PUSH_MAX)
    {
        sceVu0CopyVector(out, old);
        return 2;
    }
    sceVu0CopyVector(out, tmp);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fe8c8 (PlyrLineHitCheck, PlyrAreaLineHitCheck; tail-calls
// MapLBuffLineCheck). A crossed rectangle whose record id is 1 lets the
// line through outside mode 10; the ROM then retests the same rectangle
// (it does not step to the next one) until the slot's count runs out, so
// the rest of that slot is skipped -- kept as a break.

int MapHitLineCheck(float *from, float *to, int room, short floor, int mode10)
{
    if (RegDatGetBuffIDG(floor, from) == -1)
    {
        return 1;
    }

    int *list = RegDatGetHitList();
    int  num  = RegDatGetHitNum();

    for (int i = 0; i < num; i++)
    {
        int          rect_num = RegDatGetVecNum(list[i], MAP_HIT_LINE_RECT);
        MB_OUT_RECT *rect     = RegDatGetVecPtr(list[i], MAP_HIT_LINE_RECT);

        for (int j = 0; j < rect_num; j++)
        {
            if (HcBaseIsLineHitRectXZ(from, to, rect->vec[0]) == 0)
            {
                rect = RegDatNextVec(rect);
                continue;
            }

            MREC_HIT *st = static_cast<MREC_HIT *>(RegDatGetStPtr(list[i], rect->reg_id));

            if (mode10 != 0 || st->id != 1)
            {
                return 1;
            }
            break;
        }
    }

    return MapLBuffLineCheck(room, floor, from, to);
}
