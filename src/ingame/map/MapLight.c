// ingame/map/MapLight.c -- FF3 0x1197c8..0x11afa0. Two parts:
// - 0x1197c8..0x119fb8, FF3-only: the rooms and doors of the hit vec table
//   (MapLBuff.h): a room's door leaves become wall quads, the push-out and
//   line-of-sight tests walk them, MapDoorVec* pick the door being taken.
//   It uses MapLBuff.c's table and current door vec, so it may be the end
//   of MapLBuff.c instead (FF2 starts MapLight.c after MapLBuffInit).
// - 0x119fb8.., FF2 MapLight.c in FF3 form: the player-only light, a room's
//   real lights with the player light (MapLightMakeRoomReal), the prelit
//   room, and the door blend: MapLightMakeDual picks the strongest point and
//   spot lights of both rooms at the player into three sets (0 start, 1 end,
//   2 current; the new room's lights start at range 0.1 in 0 and 2),
//   MapLightDualProc moves set 2's ranges from set 0's to set 1's over 30
//   steps. FF2's prelight walkers, MapLightSelect and the Mei light are gone.
// MapLight_GLOBAL_I 0x11af80, the object's global constructor, is empty.

#include <math.h>                       // sqrtf
#include <string.h>                     // memcpy

#include "ingame/map/MapLight.h"
#include "graphics/graph3d/g3dLight.h"          // G3DLIGHT
#include "graphics/graph3d/g3dLightEx.h"        // gra3dSetLightIntens
#include "graphics/graph3d/gra3d.h"             // GRA3DLIGHTDATA, gra3dLightEnable*, gra3dGet*Ref, gra3dSetLightData
#include "graphics/graph3d/gra3dMisc.h"         // GRA3D_LID_*
#include "graphics/graph3d/sgd_types.h"         // SGDCOORDINATE
#include "graphics/mmanage.h"                   // ModelManager
#include "graphics/model_object.h"              // ModelObject::GetCoord
#include "ingame/ingame.h"                      // CharPlayerModel
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/map/DoorStat.h"                // MAP_DOOR_DAT, DoorStatGetData
#include "ingame/map/FurnLoad.h"                // FurnNameGetNum
#include "ingame/map/MapDraw.h"                 // MapDrawGetLightPtr4BuffBank
#include "ingame/map/MapHit.h"                  // MapHitRegistDoorVec
#include "ingame/map/MapLBuff.h"                // MAP_LBUFF, map_lbuff, map_door_vec, MapLBuff*
#include "ingame/map/MapLoad.h"                 // MAP_ROOM_DAT, MapLoadGetFreeMemAddr, MapLoadGetOffsetVector, map_room_rot
#include "ingame/map/MapObjWrk.h"               // MAP_OBJ_MDL, MapObjReqMdlRoom
#include "ingame/map/MapPut.h"                  // MapPutGetFlag, MapPutGetMdl, MAP_PUT_NODRAW
#include "ingame/map/RegDat.h"                  // RegDatGetTopAddr, the record walk, REG_DAT_DOOR
#include "ingame/map/hit_check_base.h"          // HcBaseIsLineHitRectXZ
#include "ingame/map/map_hit_check.h"           // MapHitPushOutQuad
#include "ingame/plyr/player.h"                 // PLYR_WRK, PLYR_FLG_800000, PLYR_FLG_MODE10
#include "ingame/map/MapDoor.h"      // door table, door motion pack
#include "ingame/story_menu.h"                  // IsLockTblOn

// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// unassigned@001fe5f8 (map rectangle hit)

// ──────────────────────────────────────────────────────────────────────

#define MAP_LIGHT_DUAL_STEPS 30.0f      // sdata 0x36b850
#define MAP_LIGHT_DUAL_DONE  31.0f      // sdata 0x36b84c: past the end (MapLightDualProc copies set 1)
#define MAP_LIGHT_DUAL_STEP  1.0f       // sdata 0x36b844
#define MAP_LIGHT_FADE_RANGE 0.1f       // sdata 0x36b848: a fading-in light's start range

// What the real-light sets are scaled by (data 0x2f14f0), by
// MapLightGetScale's row: [0] the player light's intensity, [1] its
// diffuse, [2] the ambient, [3] the light ranges. Rows 0 / 1 for real-light
// puts, 2 / 3 for sway puts; odd rows with the viewfinder up.
static const float map_light_scale[4][4] =
{
    { 0.6f, 1.3f, 40.0f, 1.4f },
    { 0.6f, 1.4f, 60.0f, 1.5f },
    { 0.6f, 1.5f, 20.0f, 1.3f },
    { 0.3f, 1.3f, 10.0f, 1.5f },
};

// The door blend (bss 0x382340..0x386580).
static float          map_light_dual_max;       // bss 0x382340: its steps
static float          map_light_dual_t;         // bss 0x382344: steps done
static GRA3DLIGHTDATA map_light_dual[3];        // bss 0x382350: 0 start, 1 end, 2 current

// A candidate light of MapLightMakeDualSub (FF2 MAP_LIGHT_SORT; on the stack).
// layout: host -- addr and st_addr are EE pointers (0x10 bytes there).
struct MAP_LIGHT_SORT
{
    int               flg;              // 2: from light1 (fades in), 0: from light2
    G3DLIGHT         *addr;
    GRA3DLIGHTSTATUS *st_addr;
    float             power;            // MapLightGetPower
};

// A chosen light (FF2 MAP_LIGHT_DAT, 0x90; FF3's status is 0x20).
struct MAP_LIGHT_DAT                    /* 0xa0 */
{
    /* 0x00 */ int              flg;
    /* 0x04 */ int              aiPad[3];
    /* 0x10 */ G3DLIGHT         ldat;
    /* 0x80 */ GRA3DLIGHTSTATUS lstat;
};
static_assert(sizeof(MAP_LIGHT_DAT) == 0xa0, "MAP_LIGHT_DAT is 0xa0 bytes");
static_assert(offsetof(MAP_LIGHT_DAT, ldat) == 0x10, "MAP_LIGHT_DAT.ldat");
static_assert(offsetof(MAP_LIGHT_DAT, lstat) == 0x80, "MAP_LIGHT_DAT.lstat");

typedef int (*MAP_LIGHT_FUNC)(GRA3DLIGHTSTATUS *st, G3DLIGHT *light, int id);

// The quad bank the hit tests read: player flags2 bit 25.
static int MapLBuffBank(void)
{
    return (int)(ingame_wrk.plyr_wrk->flags2 >> 25) & 1;
}

// ══════════════════════════════════════════════════════════════════════
//  The hit vec table
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// A room buffer is torn down: its vecs are freed, and each door the other
// buffer also has (same number) gets its put shown again (MapDrawRoomObjRegist
// hid the second room's copy of a shared door).
// FF3 0x001197c8 (was the wrong FF2 match MapLightSetSortDat).

void MapLBuffDeleteRoom(int buff)
{
    for (MAP_LBUFF *vec = map_lbuff; vec < &map_lbuff[MAP_LBUFF_NUM]; vec++)
    {
        if (vec->buff != buff)
            continue;

        for (MAP_LBUFF *other = map_lbuff; other < &map_lbuff[MAP_LBUFF_NUM]; other++)
        {
            if (other->buff == (buff ^ 1) && (other->flags & MAP_LBUFF_REG) == 0 &&
                other->no == vec->no)
            {
                *MapPutGetFlag(other->put) &= ~MAP_PUT_NODRAW;
            }
        }
        vec->buff = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The vecs of id with one of mask's flags are freed (MapLightRegOff: an
// event label's walls, mask MAP_LBUFF_REG).
// FF3 0x001198c8.

void MapLBuffDeleteId(int id, u_int mask)
{
    for (MAP_LBUFF *vec = map_lbuff; vec < &map_lbuff[MAP_LBUFF_NUM]; vec++)
    {
        if (vec->id == id && (vec->flags & mask) != 0)
            vec->buff = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// func(quad, arg) for buffer buff's vecs of label (< 0: any), the second
// quad too when used; 1 at the first non-zero result, else 0.
// FF3 0x00119918 (FUN_00138670 passes 0x138648).
// callcheck: MapLBuffBank (inline in the ROM), func (the callback)

int MapLBuffForEachQuad(int buff, int label, int (*func)(float *quad, void *arg), void *arg)
{
    int bank = MapLBuffBank();

    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = &map_lbuff[i];

        if (vec->buff != buff || (label >= 0 && label != vec->label))
            continue;
        if (func(vec->quad[bank][0], arg) != 0)
            return 1;
        if ((vec->flags & MAP_LBUFF_QUAD2) != 0 && func(vec->quad2[bank][0], arg) != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The circle r at pos is pushed out of the shown quads of label (< 0: any;
// the current door vec's always); 1 at the first hit.
// FF3 0x00119a20 (MrecWallPushOut).
// callcheck: MapLBuffBank (inline in the ROM)

int MapLBuffWallPushOut(float r, int label, float *pos)
{
    int bank = MapLBuffBank();

    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = &map_lbuff[i];

        if (vec->buff < 0 || (vec->flags & MAP_LBUFF_HIDE) != 0)
            continue;
        if (map_door_vec != vec && label >= 0 && label != vec->label)
            continue;
        if (MapHitPushOutQuad(r, pos, vec->quad[bank][0]) != 0)
            return 1;
        if ((vec->flags & MAP_LBUFF_QUAD2) != 0 && MapHitPushOutQuad(r, pos, vec->quad2[bank][0]) != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the segment p0-p1 crosses a quad of label (< 0: any) that takes
// part in line tests. a0 is not read.
// FF3 0x00119b48 (MapHitLineCheck).
// callcheck: MapLBuffBank (inline in the ROM)

int MapLBuffLineCheck(int a0, int label, float *p0, float *p1)
{
    int bank = MapLBuffBank();

    (void)a0;
    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = &map_lbuff[i];

        if (vec->buff < 0 || (vec->flags & (MAP_LBUFF_HIDE | MAP_LBUFF_NO_LINE)) != 0)
            continue;
        if ((vec->flags & MAP_LBUFF_LINE_HOLD) != 0 && (ingame_wrk.plyr_wrk->flags & PLYR_FLG_MODE10) == 0)
            continue;
        if (label >= 0 && label != vec->label)
            continue;
        if (HcBaseIsLineHitRectXZ(p0, p1, vec->quad[bank][0]) != 0)
            return 1;
        if ((vec->flags & MAP_LBUFF_QUAD2) != 0 && HcBaseIsLineHitRectXZ(p0, p1, vec->quad2[bank][0]) != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A door vec's quads from its door model: leaf 0 through the model's
// coordinate 1, leaf 1 (when the door has one) through coordinate 2. -1: no
// vec or no DOOR_DAT, -2: free; 0 one leaf, 1 two.
// FF3 0x00119c98.

int MapLBuffSetDoorCoord(MAP_LBUFF *vec)
{
    if (vec == nullptr)
        return -1;
    if (vec->buff < 0)
        return -2;

    MAP_OBJ_MDL *mdl = MapPutGetMdl(vec->put);
    SGDCOORDINATE *coord = mdl->mot->ani_ctrl->GetCoord(0);
    DOOR_DAT *dd = DoorDatGet((short)(vec->flags >> 16));
    if (dd == nullptr)
        return -1;

    MapLBuffSetDoorQuad(vec->quad, dd->leaf[0], coord[1].matLocalWorld);
    if (dd->leaf[1] < 0)
        return 0;
    vec->flags |= MAP_LBUFF_QUAD2;
    MapLBuffSetDoorQuad(vec->quad2, dd->leaf[1], coord[2].matLocalWorld);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The current door vec follows its door's anime.
// FF3 0x00119d48 (MapDoorModeAft / Now, MapDoorRoomChange).
// callcheck: MapLBuffSetDoorCoord (a tail jump)

void MapDoorVecCoordSet(void)
{
    MapLBuffSetDoorCoord(map_door_vec);
}

// ──────────────────────────────────────────────────────────────────────
// Door id's vec becomes the current door vec, and the other side's vecs of
// that door are hidden; id < 0 drops the current one and shows them again.
// 0, -1 when no vec has the id.
// FF3 0x00119d60 (init_Story_Door_Open, MapDoorRoomChange).

int MapDoorVecSelect(int id)
{
    if (id < 0)
    {
        if (map_door_vec != nullptr)
        {
            MapLBuffRegist(map_door_vec->id, map_door_vec->no, MAP_LBUFF_HIDE, 0);
            map_door_vec = nullptr;
        }
        return 0;
    }

    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = MapHitRegistDoorVec(i);

        if (vec != nullptr && (vec->flags & MAP_LBUFF_REG) == 0 && vec->id == id)
        {
            map_door_vec = vec;
            MapLBuffRegist(vec->id, vec->no, MAP_LBUFF_HIDE, 1);
            return 0;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119e18 (MapPlayStartReset), 0x00119e38 (MapReleaseAll).

void MapDoorVecInit(void)
{
    MapLBuffInit();
    map_door_vec = nullptr;
}

void MapDoorVecRelease(void)
{
    map_door_vec = nullptr;
    MapLBuffInit();
}

// ──────────────────────────────────────────────────────────────────────
// A door put of room (name "D<number>..."): every door record (type 0x12)
// of the room's RegDat slots whose door_stat row is this door gets the
// door anime set on the put's model and a door vec with its quads. Returns
// the last door id, -1 without the room, -2 without a match.
// FF3 0x00119e50 (MapDrawRoomObjRegist).

int MapLBuffLoadDoor(int room, const char *name, MAP_PUT *put)
{
    MAP_ROOM_DAT *dat = MapLoadGetFreeMemAddr(room);
    if (dat == nullptr)
        return -1;

    int   ret = -2;
    short no  = (short)FurnNameGetNum(name, 1, 3);

    for (int i = 0; i < REG_LIST_NUM; i++)
    {
        int id = dat->reg_list[i];
        if (id == -1)
            return ret;

        MB_OUT_HEAD  *reg = RegDatGetTopAddr(id);
        REG_DAT_DOOR *rec;

        RegDatGetStPtrStart(id, RECORD_TYPE_DOOR);
        while ((rec = (REG_DAT_DOOR *)RegDatGetNextStPtr(id)) != nullptr)
        {
            MAP_DOOR_DAT *door = DoorStatGetData(rec->door);

            if (door == nullptr || door->dbuff != no)
                continue;

            short anime  = door->dat_id;
            ret          = rec->door;
            int   motion = DoorDatGetMotNo(anime);
            u_int flags  = (u_int)((int)anime << 16);

            MapPutGetMdl(put)->SetPose(motion);
            if ((short)door->_10 == 0)
                flags |= MAP_LBUFF_NO_LINE;
            MapLBuffSetDoorCoord(MapLBuffLoad(room, ret, no, put, reg->kai, flags));
        }
    }
    return ret;
}

// ══════════════════════════════════════════════════════════════════════
//  The lights
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The real-light scale row: sway puts use rows 2 / 3, and the odd row when
// the viewfinder is up (or the player's _0e0 bit 0).
// FF3 0x00119fb8.

static const float *MapLightGetScale(int sway)
{
    PLYR_WRK *plyr   = ingame_wrk.plyr_wrk;
    int       finder = 0;

    if ((plyr->flags & PLYR_FLG_800000) != 0 || (plyr->_0e0 & 1) != 0)
        finder = 1;
    return map_light_scale[(sway & 1) * 2 + finder];
}

// ──────────────────────────────────────────────────────────────────────
// Slot id of ld, or of the current gra3d bank for nullptr.
// FF3 0x0011a048, 0x0011a078.

static GRA3DLIGHTSTATUS *MapLightGetStatusPtr(int id, GRA3DLIGHTDATA *ld)
{
    if (ld == nullptr)
        return gra3dGetLightStatusRef(id);
    return &ld->aStatus[id];
}

static G3DLIGHT *MapLightGetLightPtr(int id, GRA3DLIGHTDATA *ld)
{
    if (ld == nullptr)
        return gra3dGetLightRef(id);
    return &ld->aLight[id];
}

// ──────────────────────────────────────────────────────────────────────
// func over point slots 3..26 and spot slots 30..37 (each range when its
// flag is set); the count of non-zero results. ROM: the slots always come
// from the current gra3d bank -- ld is not passed on.
// FF3 0x0011a0a8.
// callcheck: func (the callback)

static int MapLightForEach(MAP_LIGHT_FUNC func, GRA3DLIGHTDATA *ld, int point, int spot)
{
    int cnt = 0;

    (void)ld;
    if (point != 0)
    {
        for (int id = GRA3D_START_LIGHT_POINT; id < GRA3D_LID_POINT_FLASHLIGHT; id++)
        {
            GRA3DLIGHTSTATUS *st    = MapLightGetStatusPtr(id, nullptr);
            G3DLIGHT         *light = MapLightGetLightPtr(id, nullptr);

            if (func(st, light, id) != 0)
                cnt++;
        }
    }
    if (spot != 0)
    {
        for (int id = GRA3D_START_LIGHT_SPOT; id < GRA3D_LID_SPOT_FLASHLIGHT; id++)
        {
            GRA3DLIGHTSTATUS *st    = MapLightGetStatusPtr(id, nullptr);
            G3DLIGHT         *light = MapLightGetLightPtr(id, nullptr);

            if (func(st, light, id) != 0)
                cnt++;
        }
    }
    return cnt;
}

// ──────────────────────────────────────────────────────────────────────
// MapLightForEach callback: a light that is not a prelight light is
// switched on.
// FF3 0x0011a188.

static int MapLightEnableRealOne(GRA3DLIGHTSTATUS *st, G3DLIGHT *light, int id)
{
    (void)light;
    if (st->bEnableToPrelight == 0)
        gra3dLightEnable(id, 1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the player carries light: flags 0x8000 (the light motion set,
// PLYR_WRK._2ec 1) or the model's player light on.
// FF3 0x0011a1b8.

static int MapLightIsPlayerLightOn(void)
{
    PLYR_WRK        *plyr = ingame_wrk.plyr_wrk;
    CharPlayerModel *mdl;

    if ((plyr->flags & 0x8000) != 0)
        return 1;
    mdl = (CharPlayerModel *)plyr->mdl;
    if (mdl == nullptr)
        return 0;
    return mdl->fl_on != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The player light and the rooms' real (not prelit) lights. No callers in
// FF3.
// FF3 0x0011a1f0 (FF2 MapLightSetPlayerReal's slot).
// callcheck: MapLightForEach (a tail jump)

void MapLightSetPlayerReal(void)
{
    MapLightSetPlayerOnly();
    if (IsLockTblOn(6) == 0)
        MapLightForEach(MapLightEnableRealOne, nullptr, 1, 1);
}

// ──────────────────────────────────────────────────────────────────────
// Every light off but the player light (slot 38), and that one only while
// the player carries light and lock 6 is off.
// FF3 0x0011a238 (CMapDBuff::Draw, MapPutSetLight, LanternPutDraw).
// callcheck: gra3dLightEnable (a tail jump)

void MapLightSetPlayerOnly(void)
{
    gra3dLightEnableAll(0);
    if (IsLockTblOn(6) == 0 && MapLightIsPlayerLightOn() != 0)
        gra3dLightEnable(GRA3D_LID_SPOT_FLASHLIGHT, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The ambient and the ranges of point / spot slots 3..37 scaled.
// FF3 0x0011a280.

static void MapLightScaleRoom(GRA3DLIGHTDATA *ld, const float *scale)
{
    for (int i = 0; i < 4; i++)
        ld->vAmbient[i] *= scale[2];
    for (int id = GRA3D_START_LIGHT_POINT; id < GRA3D_LID_SPOT_FLASHLIGHT; id++)
        ld->aLight[id].fMaxRange *= scale[3];
}

// ──────────────────────────────────────────────────────────────────────
// out = src scaled for a real-light put, with the player's light in the
// spot slot 38 (range, intensity and diffuse scaled) while the player
// carries light; slot 38 is on exactly then.
// FF3 0x0011a2c8 (MapPutSetLight; FF2 MapLightMakeRoomReal).

void MapLightMakeRoomReal(GRA3DLIGHTDATA *out, GRA3DLIGHTDATA *src, int sway)
{
    memcpy(out, src, sizeof(GRA3DLIGHTDATA));

    const float *scale = MapLightGetScale(sway);
    MapLightScaleRoom(out, scale);

    int              on  = MapLightIsPlayerLightOn();
    CharPlayerModel *mdl = (CharPlayerModel *)ingame_wrk.plyr_wrk->mdl;
    if (mdl == nullptr)                 // PC: the player model is not built yet (flags 0x8000 alone)
        on = 0;

    if (on != 0)
    {
        G3DLIGHT *light = &out->aLight[GRA3D_LID_SPOT_FLASHLIGHT];

        *light = mdl->fl;
        light->fMaxRange *= scale[3];
        gra3dSetLightIntens(light, light->afPad0[0] * scale[0]);
        for (int i = 0; i < 3; i++)     // vmulx.xyz
            light->vDiffuse[i] *= scale[1];
    }
    out->aStatus[GRA3D_LID_SPOT_FLASHLIGHT].bEmulateToDirectionalLight = on;
    out->aStatus[GRA3D_LID_SPOT_FLASHLIGHT].bEnable = on;
}

// ──────────────────────────────────────────────────────────────────────
// Buffer buff's room model prelit with the buffer's bank-0 light set at the
// room's origin (clear 0), or its prelighting cleared (1); then the room's
// object models the same (MapObjReqMdlRoom).
// FF3 0x0011a3d0 (gra3dPrelight; name invented).

void MapDrawPrelightRoom(int buff, int clear)
{
    MAP_ROOM_DAT *dat = MapLoadGetFreeMemAddr(buff);
    if (dat == nullptr)
        return;

    gra3dSetLightData(MapDrawGetLightPtr4BuffBank(buff, 0), nullptr);
    if (clear == 0)
    {
        sceVu0FVECTOR ofs;

        MapLoadGetOffsetVector(ofs, buff);
        dat->mdl.mot->ExecPrelight(ofs, (float *)map_room_rot);
    }
    else
    {
        dat->mdl.mot->ClearPrelight();
    }
    MapObjReqMdlRoom(buff, clear);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011a470.

GRA3DLIGHTDATA *MapLightGetDualWork(int no)
{
    return &map_light_dual[no];
}

// ──────────────────────────────────────────────────────────────────────
// Blend set no = *mst (FF2 MapLightSetScale: *out = *mst).
// FF3 0x0011a490.

static void MapLightSetScale(int no, GRA3DLIGHTDATA *mst)
{
    map_light_dual[no] = *mst;
}

// ──────────────────────────────────────────────────────────────────────
// One step of the door blend: the range of each enabled point / spot light
// of set 2 goes t / max of the way from set 0's to set 1's; once t is past
// max, set 2 is set 1. Returns set 2.
// FF3 0x0011a508 (MapDoorModeNow, every frame of the door anime).

GRA3DLIGHTDATA *MapLightDualProc(void)
{
    if (map_light_dual_max < map_light_dual_t)
    {
        map_light_dual[2] = map_light_dual[1];
    }
    else
    {
        // ROM: a stack array {3, 27, 30, 38}, walked as two inclusive ranges.
        static const int range[2][2] =
        {
            { GRA3D_START_LIGHT_POINT, GRA3D_LID_POINT_FLASHLIGHT },
            { GRA3D_START_LIGHT_SPOT,  GRA3D_LID_SPOT_FLASHLIGHT  },
        };
        float rate = map_light_dual_t / map_light_dual_max;

        for (int r = 0; r < 2; r++)
        {
            for (int id = range[r][0]; id <= range[r][1]; id++)
            {
                if (map_light_dual[2].aStatus[id].bEnable == 0)
                    continue;
                float from = map_light_dual[0].aLight[id].fMaxRange;
                map_light_dual[2].aLight[id].fMaxRange =
                    (map_light_dual[1].aLight[id].fMaxRange - from) * rate + from;
            }
        }
        map_light_dual_t += MAP_LIGHT_DUAL_STEP;
    }
    return &map_light_dual[2];
}

// ──────────────────────────────────────────────────────────────────────
// What a light is worth at pos: the length of its diffuse colour within
// fMinRange, fMaxRange / distance within twice fMaxRange, else 0 (FF2
// scaled the colour length by the range instead).
// FF3 0x0011a6a8 (FF2 MapLightGetPower; was the wrong match MapMeiRegistLightOne).
// callcheck: sqrtf (VU0 vsqrt in the ROM)

static float MapLightGetPower(G3DLIGHT *light, float *pos)
{
    float power = sqrtf(light->vDiffuse[0] * light->vDiffuse[0] +
                        light->vDiffuse[1] * light->vDiffuse[1] +
                        light->vDiffuse[2] * light->vDiffuse[2]);
    float dx    = light->vPosition[0] - pos[0];
    float dy    = light->vPosition[1] - pos[1];
    float dz    = light->vPosition[2] - pos[2];
    float len   = sqrtf(dx * dx + dy * dy + dz * dz);

    if (len <= light->fMinRange)
        return power;
    if (light->fMaxRange + light->fMaxRange <= len)
        return 0.0f;
    return light->fMaxRange / len;
}

// ──────────────────────────────────────────────────────────────────────
// Appends the enabled prelight lights st..en of ld to list with their power
// at pos and flg; the number appended.
// FF3 0x0011a738 (FF2 MapLightSetSortDat, which took a count and a start index).

static int MapLightSetSortDat(int st, int en, float *pos, MAP_LIGHT_SORT *list, GRA3DLIGHTDATA *ld, int flg)
{
    int cnt = 0;

    for (int id = st; id <= en; id++)
    {
        GRA3DLIGHTSTATUS *s     = &ld->aStatus[id];
        G3DLIGHT         *light = &ld->aLight[id];

        if (s->bEnable == 0 || s->bEnableToPrelight == 0)
            continue;
        list->power   = MapLightGetPower(light, pos);
        list->addr    = light;
        list->st_addr = s;
        list->flg     = flg;
        list++;
        cnt++;
    }
    return cnt;
}

// ──────────────────────────────────────────────────────────────────────
// The candidates st..en of both sets (light1's flagged 2), the strongest
// first, into w_light[st..en] until the range is full; the candidate count.
// FF3 0x0011a818 (FF2 MapLightMakeDualSub).

static int MapLightMakeDualSub(MAP_LIGHT_DAT *w_light, GRA3DLIGHTDATA *light1, GRA3DLIGHTDATA *light2,
                               int st, int en, float *pos)
{
    MAP_LIGHT_SORT list[NUM_GRA3DLIGHTID * 2];
    int            n;

    n  = MapLightSetSortDat(st, en, pos, list, light1, 2);
    n += MapLightSetSortDat(st, en, pos, &list[n], light2, 0);
    if (n == 0)
        return 0;

    MAP_LIGHT_DAT  *out  = &w_light[st];
    MAP_LIGHT_SORT *last = &list[n];

    for (int left = n; left > 0; left--)
    {
        MAP_LIGHT_SORT *best = &list[0];

        for (int k = 1; k < left; k++)
        {
            if (best->power < list[k].power)
                best = &list[k];
        }
        out->ldat  = *best->addr;
        out->lstat = *best->st_addr;
        out->flg   = best->flg;
        out++;
        if (en < ++st)
            return n;
        *best = *--last;                // the last candidate takes its place
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// MapLightForEach callback: 1 for an enabled prelight light.
// FF3 0x0011a9e0 (was inside MapLightMakeDualSub's body in Ghidra).

static int MapLightIsPrelightOne(GRA3DLIGHTSTATUS *st, G3DLIGHT *light, int id)
{
    (void)light;
    (void)id;
    if (st->bEnable == 0)
        return 0;
    return st->bEnableToPrelight != 0;
}

// ──────────────────────────────────────────────────────────────────────
// ld->aiNumInitial[type] = the enabled prelight point lights (type 1) or
// spot lights (else) -- ROM: counted in the current gra3d bank
// (MapLightForEach), not in ld.
// FF3 0x0011aa00.

static void MapLightCountInitial(GRA3DLIGHTDATA *ld, int type)
{
    ld->aiNumInitial[type] = MapLightForEach(MapLightIsPrelightOne, ld, type == 1, type != 1);
}

// ──────────────────────────────────────────────────────────────────────
// Chosen light id into blend sets 1, 0 and 2 (or the slot off there when
// none was chosen); one from light1 starts at range 0.1 in sets 0 and 2,
// so it fades in.
// FF3 0x0011aa58 (FF2 MapLightUpdate's loop body).

static void MapLightUpdateOne(MAP_LIGHT_DAT *w_light, int id)
{
    static const int order[3] = { 1, 0, 2 };   // ROM: a stack array
    MAP_LIGHT_DAT   *w        = &w_light[id];

    for (int i = 0; i < 3; i++)
    {
        GRA3DLIGHTDATA *ld = MapLightGetDualWork(order[i]);

        if (w->lstat.bEnable == 0)
        {
            ld->aStatus[id].bEnable = 0;
        }
        else
        {
            ld->aLight[id]  = w->ldat;
            ld->aStatus[id] = w->lstat;
        }
        if ((w->flg & 2) != 0 && order[i] != 1)
        {
            ld->aLight[id].fMaxRange = MAP_LIGHT_FADE_RANGE;
            ld->aLight[id].fMinRange = 0.0f;
        }
    }
}

// FF3 0x0011abf8 (FF2 MapLightUpdate).

static void MapLightUpdate(MAP_LIGHT_DAT *w_light, int st, int en)
{
    for (; st <= en; st++)
        MapLightUpdateOne(w_light, st);
}

// ──────────────────────────────────────────────────────────────────────
// The directional slots, the player's point slots 27..29 and the spot
// flashlight 38 off.
// FF3 0x0011ac58 (was the wrong match MapMeiInit).

static void MapLightClearPlayerSlots(GRA3DLIGHTDATA *ld)
{
    for (int id = GRA3D_START_LIGHT_DIRECTIONAL; id <= GRA3D_END_LIGHT_DIRECTIONAL; id++)
        ld->aStatus[id].bEnable = 0;
    for (int id = GRA3D_LID_POINT_FLASHLIGHT; id <= GRA3D_LID_POINT_SELFREFLECTION; id++)
        ld->aStatus[id].bEnable = 0;
    ld->aStatus[GRA3D_LID_SPOT_FLASHLIGHT].bEnable = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Starts the door blend between two rooms' light sets at pos (the player):
// every set is light2, then each point / spot slot gets the strongest of
// both rooms' prelight lights (light1's fading in), the player slots off;
// MapLightDualProc steps it. With one set, or the same one twice, all three
// sets are that one and the blend is over at once.
// FF3 0x0011acb0 (FF2 MapLightMakeDual; MapDoorModeWait passes the new room first).

void MapLightMakeDual(GRA3DLIGHTDATA *light1, GRA3DLIGHTDATA *light2, float *pos)
{
    GRA3DLIGHTDATA *end   = MapLightGetDualWork(1);
    GRA3DLIGHTDATA *start = MapLightGetDualWork(0);
    GRA3DLIGHTDATA *cur   = MapLightGetDualWork(2);

    if (light1 == nullptr && light2 == nullptr)
        return;

    GRA3DLIGHTDATA *one = nullptr;
    if (light1 == nullptr)
        one = light2;
    if (light2 == nullptr)
        one = light1;
    if (light1 == light2)
        one = light2;

    if (one != nullptr)
    {
        *end = *one;
        for (int i = 0; i < 3; i++)
            MapLightSetScale(i, one);
        map_light_dual_t = MAP_LIGHT_DUAL_DONE;
        return;
    }

    MAP_LIGHT_DAT w_point[NUM_GRA3DLIGHTID];
    MAP_LIGHT_DAT w_spot[NUM_GRA3DLIGHTID];

    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        w_point[i].lstat.bEnable = 0;
        w_spot[i].lstat.bEnable  = 0;
    }
    MapLightMakeDualSub(w_point, light1, light2, GRA3D_START_LIGHT_POINT, GRA3D_LID_POINT_FLASHLIGHT, pos);
    MapLightMakeDualSub(w_spot, light1, light2, GRA3D_START_LIGHT_SPOT, GRA3D_LID_SPOT_FLASHLIGHT, pos);
    for (int i = 0; i < 3; i++)
        MapLightSetScale(i, light2);
    MapLightUpdate(w_point, GRA3D_START_LIGHT_POINT, GRA3D_LID_POINT_FLASHLIGHT);
    MapLightUpdate(w_spot, GRA3D_START_LIGHT_SPOT, GRA3D_LID_SPOT_FLASHLIGHT);
    for (int i = 0; i < 3; i++)
        MapLightClearPlayerSlots(MapLightGetDualWork(i));

    MapLightCountInitial(end, 1);
    MapLightCountInitial(end, 2);
    cur->aiNumInitial[1] = start->aiNumInitial[1] = end->aiNumInitial[1];
    cur->aiNumInitial[2] = start->aiNumInitial[2] = end->aiNumInitial[2];
    map_light_dual_t   = 0.0f;
    map_light_dual_max = MAP_LIGHT_DUAL_STEPS;
}
