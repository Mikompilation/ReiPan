/* ==========================================================================
 *  ingame/map/MapObjWrk.c
 *
 *  The map object works (see MapObjWrk.h). FF3 0x1f8af8..0x1f9b48;
 *  FF3-only, written from the FF3 decompilation. The functions at
 *  0x1f8af8..0x1f8d68 were once matched to FF2's map_bgm.c; they read
 *  this table.
 * ======================================================================== */
#include "ingame/map/MapObjWrk.h"
#include "ingame/map/MapCtrl.h"              // MapAreaHitLineCheck

#include <math.h>                               // sqrtf
#include <stdio.h>                              // sprintf
#include <string.h>                             // memset

#include "graphics/mmanage.h"                   // ModelManager, ModelCcs
#include "graphics/motion/motion.h"             // MDL_MOT
#include "ingame/map/FurnLoad.h"                // FurnLoadCallFunc, FurnLoadSetRecFlag, FurnLoadClearPrelit, FurnLoadGetMdlPk4 ..
#include "ingame/map/FurnSave.h"                // FURN_SAVE_ROW, FurnSave*
#include "ingame/map/MapDraw.h"                 // MapDrawGetLightPtr4BuffID
#include "ingame/map/MapLoad.h"                 // MapLoadGetRoomArea
#include "ingame/map/MapPut.h"                  // MapPut*
#include "ingame/plyr/player.h"                 // PlyrLineHitCheck, GetPlyrAreaNo
#include "ingame/plyr/plyr_mdl.h"               // PlyrLook*
#include "ingame/plyr/plyr_sub.h"               // PlyrGetJointPos

// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@0011cea8

// ──────────────────────────────────────────────────────────────────────

static MAP_OBJ_WRK map_obj_wrk[MAP_OBJ_WRK_NUM];                       /* bss 3f1a38 */

// Wind sway by strength 0..3 (SET_FURN_WIND): power, period; direction 0..3.
static const float wind_power_tbl[4]  = { 0.0f, 0.1f, 0.3f, 0.0f };   /* data 35b890 */
static const int   wind_period_tbl[4] = { 0, 120, 90, 0 };             /* data 35b8a0 */
static const sceVu0FVECTOR wind_dir_tbl[4] =                           /* data 35b8b0 */
{
    {  0.0f, 0.0f,  1.0f, 0.0f },
    {  1.0f, 0.0f,  0.0f, 0.0f },
    {  0.0f, 0.0f, -1.0f, 0.0f },
    { -1.0f, 0.0f,  0.0f, 0.0f },
};

// ──────────────────────────────────────────────────────────────────────
// The work type of an object name: "d..." door, "f..." furniture.
// FF3 0x001f8af8.

int MapObjGetType(const char *name)
{
    if (*name == 'd')
    {
        return MAP_OBJ_TYPE_DOOR;
    }
    if (*name == 'f')
    {
        return MAP_OBJ_TYPE_FURN;
    }
    return -1;
}

// FF3 0x001f8b20.
MAP_OBJ_WRK *MapObjFindWrk(int area, int no, int type)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list >= 0 && wrk->area == area && wrk->no == no && wrk->type == type)
        {
            return wrk;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The model of the room's object called name (its save row of the name's
// kind); that row's visibility bit 3 is cleared on the way. No callers.
// FF3 0x001f8b78.

MAP_OBJ_MDL *MapObjGetMdlHide(int room, const char *name)
{
    int   no = FurnNameGetNo(name);
    short kind;

    if (no < 0)
    {
        return nullptr;
    }
    kind = (short)FurnSaveGetKind(name);
    if (kind < 0)
    {
        return nullptr;
    }

    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list == room && wrk->no == no && wrk->row != nullptr && wrk->row->kind == kind)
        {
            MapObjSetVisible(room, kind, no, 8, 0);
            return &wrk->mdl;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// MapPutGetPos(out, put) for the object called name in area: 0, -1 for a
// name without a number, -4 for one of no work type, -2 without a work, -3
// when MapPutGetPos fails (GetObjectPos).
// FF3 0x001f8c38.

int MapObjPutCheck(void *out, int area, const char *name)
{
    int          no = FurnNameGetNo(name);
    int          type;
    MAP_OBJ_WRK *wrk;

    if (no < 0)
    {
        return -1;
    }
    type = MapObjGetType(name);
    if (type < 0)
    {
        return -4;
    }
    wrk = MapObjFindWrk(area, no, type);
    if (wrk == nullptr)
    {
        return -2;
    }
    if (MapPutGetPos(out, wrk->put) < 0)
    {
        return -3;
    }
    return 0;
}

// FF3 0x001f8cd8.
float *MapObjGetMatrix(int area, const char *name)
{
    int          no;
    int          type;
    MAP_OBJ_WRK *wrk;

    if ((no = FurnNameGetNo(name)) >= 0 && (type = MapObjGetType(name)) >= 0
        && (wrk = MapObjFindWrk(area, no, type)) != nullptr)
    {
        return MapPutGetMatrix(wrk->put);
    }
    return nullptr;
}

// FF3 0x001f8d68.
int MapObjGetNoByPut(MAP_PUT *put)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list >= 0 && wrk->type == MAP_OBJ_TYPE_FURN && wrk->put == put)
        {
            return wrk->no;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: each shown object takes its row's current alpha; a looping
// one restarts the row's motion when it ends.
// FF3 0x001f8db8.

void MapObjMain(void)
{
    for (int i = 0; i < MAP_OBJ_WRK_NUM; i++)
    {
        MAP_OBJ_WRK *wrk = &map_obj_wrk[i];

        if (wrk->put == nullptr || wrk->row == nullptr)
        {
            continue;
        }
        wrk->mdl.SetAlpha(wrk->row->alpha_cur);
        if ((wrk->flag & MAP_OBJ_FLG_LOOP) && wrk->mdl.mot->IsMotionEnd() != 0)
        {
            wrk->mdl.PlayMotion(wrk->row->req_action, 1.0f);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The work showing row plays the row's motion by its action type: 1 once
// (the type becomes 2), 2 straight to its end, 3 in a loop.
// FF3 0x001f8e68.
// callcheck: MapObjFindWrkByRow, MapObjMdlPlayMotion x3 (one call per case)

void MapObjSetAction(FURN_SAVE_ROW *row)
{
    MAP_OBJ_WRK *wrk    = MapObjFindWrkByRow(row);
    int          motion = row->req_action;

    wrk->flag &= ~MAP_OBJ_FLG_LOOP;
    switch (row->action_type)
    {
    case 1:
        wrk->mdl.PlayMotion(motion, 1.0f);
        row->action_type = 2;
        break;
    case 2:
        wrk->mdl.PlayMotion(motion, 100000.0f);
        break;
    case 3:
        wrk->mdl.PlayMotion(motion, 1.0f);
        wrk->flag |= MAP_OBJ_FLG_LOOP;
        break;
    }
}

// FF3 0x001f8f48.
void MapObjSetActionRoom(int room)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->put != nullptr && wrk->row != nullptr && wrk->list == room)
        {
            MapObjSetAction(wrk->row);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// A cleared free work given to room, nullptr when all are used.
// FF3 0x001f8fc0.

static MAP_OBJ_WRK *MapObjGetFreeWrk(int room)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list < 0)
        {
            memset(wrk, 0, sizeof(*wrk));
            wrk->list = room;
            return wrk;
        }
    }
    return nullptr;
}

// FF3 0x001f9030.
static void MapObjWrkRelease(MAP_OBJ_WRK *wrk)
{
    if (wrk == nullptr)
    {
        return;
    }
    if (wrk->put != nullptr)
    {
        MpaPutDeleteOneObj(wrk->put);
        wrk->put = nullptr;
    }
    wrk->mdl.Release();
    wrk->list = -1;
}

// FF3 0x001f9080.
MAP_OBJ_WRK *MapObjFindWrkByRow(FURN_SAVE_ROW *row)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list >= 0 && wrk->row == row)
        {
            return wrk;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Puts the object called name into room: a work with its number, type and
// save row, its model from the furniture record's PK4s (mot_pk4 nullptr:
// the record's), the put object (flags 2; 0x20002 in area 0x12). Swaying
// models (handle kinds 1 / 2) get the room's pointer and put flag 0x800 /
// 0x4000; objects FurnLoadCallFunc accepts clear their model request and
// mark the record. Returns the put object, nullptr on failure.
// FF3 0x001f90c0.
// callcheck: MapLoadGetRoomArea, MapLoadGetRoomArea (the area again for the flags)

MAP_PUT *MapObjRegist(int room, const char *name, float *pos, float *rot, float *scale, void *mot_pk4)
{
    MAP_OBJ_WRK *wrk = MapObjGetFreeWrk(room);
    int          kind;
    u_int        flags;

    if (wrk == nullptr)
    {
        return nullptr;
    }

    if ((wrk->area = MapLoadGetRoomArea(room)) < 0
        || (wrk->no = FurnNameGetNo(name)) < 0
        || (wrk->type = MapObjGetType(name)) < 0)
    {
        MapObjWrkRelease(wrk);
        return nullptr;
    }
    wrk->row = FurnSaveFindName(wrk->area, name);

    if (mot_pk4 == nullptr)
    {
        mot_pk4 = FurnLoadGetMotPk4(room, name);
    }
    kind  = wrk->mdl.Build(FurnLoadGetMdlPk4(room, name), mot_pk4);
    flags = MapLoadGetRoomArea(room) == 0x12 ? 0x20002 : 2;
    wrk->put = MapPutSetObj((short)room, &wrk->mdl, pos, rot, scale, nullptr, flags);

    if (kind == 1 || kind == 2)
    {
        u_int *flag;

        MapPutSetLitPtr(wrk->put, MapDrawGetLightPtr4BuffID(room, 0));
        flag = MapPutGetFlag(wrk->put);
        if (flag != nullptr)            // PC: stub guard (MapPutGetFlag is a stub)
        {
            *flag |= kind == 1 ? 0x800 : 0x4000;
        }
    }

    if (wrk->put == nullptr)
    {
        MapObjWrkRelease(wrk);
        return nullptr;
    }
    if (FurnLoadCallFunc(room, name) != 0)
    {
        wrk->mdl.mot->ExecPrelight(pos, rot);
        FurnLoadSetRecFlag(room, name, FURN_REC_PRELIT);
    }
    return wrk->put;
}

// ──────────────────────────────────────────────────────────────────────
// EvPlyrMdlChangeWait: every door's model again.
// FF3 0x001f92c8.

void MapObjDoorMdlRebuildAll(void)
{
    for (int i = 0; i < MAP_OBJ_WRK_NUM; i++)
    {
        MAP_OBJ_WRK *wrk = &map_obj_wrk[i];

        if (wrk->list >= 0 && wrk->type == MAP_OBJ_TYPE_DOOR)
        {
            wrk->mdl.Rebuild();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The prelight of the room's objects that FurnLoadCallFunc accepts:
// cleared, then (unless keep) lit again at the object's matrix, and the
// furniture record marked.
// FF3 0x001f9328.

void MapObjReqMdlRoom(int room, int keep)
{
    char name[128];

    FurnLoadClearPrelit(room);
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list != room)
        {
            continue;
        }
        sprintf(name, "%s%03d", wrk->type != 0 ? "f" : "d", wrk->no);  /* rodata 371020, 371010, 371018 */
        if (FurnLoadCallFunc(room, name) != 0)
        {
            MDL_MOT *mot = wrk->mdl.mot;

            mot->ClearPrelight();
            if (keep == 0)
            {
                mot->ExecPrelightMatrix(MapPutGetMatrix(wrk->put));
            }
            FurnLoadSetRecFlag(room, name, FURN_REC_PRELIT);
        }
    }
}

// FF3 0x001f9428.
void MapObjReleaseRoom(int room)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list == room)
        {
            MapObjWrkRelease(wrk);
        }
    }
}

// FF3 0x001f9488.
void MapObjSetVisible(int room, short kind, int no, u_int mask, u_char val)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        if (wrk->list == room && wrk->row != nullptr
            && (kind == -1 || wrk->row->kind == kind)
            && (no == -1 || wrk->no == no))
        {
            FurnSaveSetVisible(wrk->row, mask, val);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The room's objects again from their rows: wind sway by the put object's
// sway kind, then shown when the row's visibility for vis_mask is 1 (-1:
// hidden).
// FF3 0x001f9550.
// callcheck: MapPutGetFlag, MapPutGetFlag (the flag word fetched again after the sway)

void MapObjUpdateRoom(int room, int vis_mask)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        int    vis = 0;
        u_int *flag;

        if (wrk->list != room || wrk->row == nullptr)
        {
            continue;
        }
        if (vis_mask != -1)
        {
            vis = FurnSaveGetVisible(wrk->row, vis_mask);
        }
        if (wrk->put == nullptr)
        {
            continue;
        }

        flag = MapPutGetFlag(wrk->put);
        if (flag == nullptr)            // PC: stub guard (MapPutGetFlag is a stub)
        {
            continue;
        }
        if ((*flag & 0x65820) == 0x800)
        {
            MapObjSetWind(wrk);
        }
        else if ((*flag & 0x65820) == 0x4000)
        {
            MapObjSetWindCloth(wrk);
        }

        flag = MapPutGetFlag(wrk->put);
        if (flag != nullptr)
        {
            if (vis == 1)
            {
                *flag &= ~8u;
            }
            else
            {
                *flag |= 8;
            }
        }
    }
}

// FF3 0x001f9688.
void MapObjUpdate(int vis_mask)
{
    if (vis_mask < 0)
    {
        vis_mask = FurnSaveGetVisMask();
    }
    MapObjUpdateRoom(0, vis_mask);
    MapObjUpdateRoom(1, vis_mask);
}

// FF3 0x001f96d0.
void MapObjInit(void)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        memset(wrk, 0, sizeof(*wrk));
        wrk->list = -1;
    }
}

// FF3 0x001f9730.
void MapObjReleaseAll(void)
{
    MapObjReleaseRoom(0);
    MapObjReleaseRoom(1);
}

// ──────────────────────────────────────────────────────────────────────
// Look 9: the nearest shown furniture whose row allows a look, within the
// look distance of the player's joint 2 and with no wall between. When
// one is found and the look may change, its position and distance go to
// the look work: 1. Nothing found clears the look's targets.
// FF3 0x001f9750.

int MapObjLookSearch(void)
{
    int            found = 0;
    float          limit = PlyrLookGetDist(9);
    float          best  = 9999.0f;                                     /* rodata 371028 */
    PLYR_LOOK_WRK *look  = PlyrLookGetWrk(9);
    sceVu0FVECTOR  joint;
    sceVu0FVECTOR  best_pos = { 0.0f, 0.0f, 0.0f, 0.0f };

    look->prm = PlyrLookGetPrm(9);
    PlyrGetJointPos(2, joint, nullptr);

    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        float        *mtx;
        sceVu0FVECTOR pos;
        float         dx, dy, dz, dist;

        if (wrk->list < 0 || wrk->type != MAP_OBJ_TYPE_FURN
            || FurnSaveGetVisible(wrk->row, 1) == 0 || wrk->row->look == 0)
        {
            continue;
        }

        mtx = MapPutGetMatrix(wrk->put);
        if (mtx == nullptr)             // PC: stub guard (MapPutGetMatrix is a stub)
        {
            continue;
        }
        pos[0] = mtx[12];
        pos[1] = mtx[13];
        pos[2] = mtx[14];
        pos[3] = mtx[15];

        // VU0 in FF3: the length of (pos - joint).xyz
        dx   = pos[0] - joint[0];
        dy   = pos[1] - joint[1];
        dz   = pos[2] - joint[2];
        dist = sqrtf(dx * dx + dy * dy + dz * dz);

        if (dist < limit && PlyrLineHitCheck(pos) == 0
            && MapAreaHitLineCheck(pos, joint, GetPlyrAreaNo()) == 0 && dist < best)
        {
            best_pos[0]  = pos[0];
            best_pos[1]  = pos[1];
            best_pos[2]  = pos[2];
            best_pos[3]  = pos[3];
            look->target = wrk;
            found        = 1;
            best         = dist;
        }
    }

    if (!found)
    {
        look->target_old = nullptr;
        look->target     = nullptr;
        return 0;
    }
    if (PlyrLookTimeCheck(9) == 0)
    {
        return 0;
    }
    look->pos[0] = best_pos[0];
    look->pos[1] = best_pos[1];
    look->pos[2] = best_pos[2];
    look->pos[3] = best_pos[3];
    look->dist   = best;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The row's wind on a swaying model: strength 0 stops it, else it sways
// towards the row's direction. 1 when the work has a row and a model.
// FF3 0x001f9948.

int MapObjSetWind(MAP_OBJ_WRK *wrk)
{
    FURN_SAVE_ROW *row = wrk->row;
    MDL_MOT       *mot;
    int            power;

    if (row == nullptr || (mot = wrk->mdl.mot) == nullptr)
    {
        return 0;
    }
    power = row->wind[0];
    if (power == 0)
    {
        mot->StopClothWind(-1);
        return 1;
    }
    mot->SetClothWind(-1, wind_dir_tbl[row->wind[1]], wind_period_tbl[power], wind_power_tbl[power]);
    return 1;
}

// The row's wind on a cloth model: strength 5 has its own call.
// FF3 0x001f99e0.
int MapObjSetWindCloth(MAP_OBJ_WRK *wrk)
{
    short power;
    ModelCcs *cloth;

    if (wrk->row == nullptr || wrk->mdl.mot == nullptr)
    {
        return 0;
    }
    power = wrk->row->wind[0];
    cloth = wrk->mdl.mot->GetCcs();
    if (cloth == nullptr)               // PC: FF3 writes through it (a model without a ccs member)
    {
        return 1;
    }
    if (power == 5)
    {
        cloth->StopWind();
    }
    else
    {
        cloth->SetWind(power);
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Shows (show 1) or hides the put objects of (room, type, no); -1 any.
// FF3 0x001f9a50.

void MapObjSetHide(int room, int type, int no, int show)
{
    for (MAP_OBJ_WRK *wrk = map_obj_wrk; wrk < map_obj_wrk + MAP_OBJ_WRK_NUM; wrk++)
    {
        u_int *flag;

        if (room >= 0 && wrk->list != room)
        {
            continue;
        }
        if (wrk->put == nullptr || (type != -1 && wrk->type != type) || (no != -1 && wrk->no != no))
        {
            continue;
        }
        flag = MapPutGetFlag(wrk->put);
        if (flag != nullptr)
        {
            if (show == 1)
            {
                *flag &= ~8u;
            }
            else
            {
                *flag |= 8;
            }
        }
    }
}
