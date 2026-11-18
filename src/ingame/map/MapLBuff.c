// FILE: ingame/map/MapLBuff.c
//
// The hit vec table (FF3 0x119390..0x1197c8; FF3-only code in FF2
// MapLBuff.c's slot). 32 wall quads the player is pushed out of and lines
// of sight are tested against (MapLight.c's first part walks them):
//
//   door vecs        the leaves of a room's doors, made by MapLBuffLoadDoor
//                    from the door model (MapLBuffSetDoorQuad), id = the
//                    door's door_stat row, no = its number;
//   event walls      MAP_LBUFF_REG, 4 corners of a RegDat vector record
//                    (MapLBuffLoadVec, MapLightRegOn).
//
// The same door is loaded once per room it joins; while the player takes a
// door (map_door_vec, MapDoorVecSelect) the other side's vecs are hidden.
// The table is save block 51 (MapLBuffSaveGet in MapHit.c).

#include "ingame/map/MapLBuff.h"

#include "ingame/map/FurnLoad.h"                // FurnNameGetNum
#include "ingame/map/MapHit.h"                  // MapHitRegistDoorVec
#include "ingame/map/MapPut.h"                  // MapPutGetMdl

#include <string.h>                             // memcpy

MAP_LBUFF  map_lbuff[MAP_LBUFF_NUM];    // bss 0x37fd40
MAP_LBUFF *map_door_vec;                // sbss gp-0x7cc8

// A door leaf in its own space: 18 long on x, z set per bank from
// map_lbuff_half_w before each copy (the ROM writes the z's into this table,
// so it keeps the last bank's).
static sceVu0FVECTOR map_lbuff_leaf[4] =    // data 0x2f14b0
{
    {  0.0f, 0.0f, -0.4f, 1.0f },
    {  0.0f, 0.0f,  0.4f, 1.0f },
    { 18.0f, 0.0f,  0.4f, 1.0f },
    { 18.0f, 0.0f, -0.4f, 1.0f },
};
static float map_lbuff_half_w[2] = { 3.0f, 10.0f };   // sdata 0x36b830: per bank
static float map_lbuff_leaf_len  = 18.0f;             // sdata 0x36b838: kind 2's x shift

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119390.

int MapLBuffRegist(int id, int no, u_int mask, int on)
{
    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = MapHitRegistDoorVec(i);

        if (vec == nullptr || (vec->flags & MAP_LBUFF_REG) != 0)
            continue;
        if (vec->no != no || vec->id == id)
            continue;
        if (on == 0)
            vec->flags &= ~mask;
        else if (on == 1)
            vec->flags |= mask;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119458 (MapDrawRoomObjRegist: door put flag 8).

int MapLBuffIsDoorLoaded(const char *name)
{
    int no = FurnNameGetNum(name, 1, 3);

    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = MapHitRegistDoorVec(i);

        if (vec != nullptr && (vec->flags & MAP_LBUFF_REG) == 0 && vec->no == no)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001194d0, 0x001194f0, 0x00119500. Only called with a door taken
// (map_door_vec set).

MAP_OBJ_MDL *MapDoorVecGetMot(void)
{
    return MapPutGetMdl(map_door_vec->put);
}

MAP_PUT *MapDoorVecGetPut(void)
{
    return map_door_vec->put;
}

void MapDoorVecSetDisp(int disp)
{
    if (disp == 0)
        map_door_vec->flags |= MAP_LBUFF_HIDE;
    else if (disp == 1)
        map_door_vec->flags &= ~MAP_LBUFF_HIDE;
}

// ──────────────────────────────────────────────────────────────────────
// Bank b's corners: z -/+ map_lbuff_half_w[b] (corners 0 and 3 negative),
// x shifted back by the leaf length for kind 2, then through mtx (w 1).
// FF3 0x00119540 (MapLBuffSetDoorCoord).

void MapLBuffSetDoorQuad(sceVu0FVECTOR (*quad)[4], int kind, const sceVu0FMATRIX mtx)
{
    float shift = (kind == 2) ? map_lbuff_leaf_len : 0.0f;

    for (int bank = 0; bank < 2; bank++)
    {
        for (int i = 0; i < 4; i++)
        {
            float w = map_lbuff_half_w[bank];

            map_lbuff_leaf[i][2] = (i == 0 || i == 3) ? -w : w;
            memcpy(quad[bank][i], map_lbuff_leaf[i], sizeof(sceVu0FVECTOR));
            quad[bank][i][0] -= shift;
            sceVu0ApplyMatrix(quad[bank][i], mtx, quad[bank][i]);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The first free entry, now owned by buffer buff; nullptr when all 32 are
// used. FF3 0x00119648.

static MAP_LBUFF *MapLBuffGetFree(int buff)
{
    for (int i = 0; i < MAP_LBUFF_NUM; i++)
    {
        MAP_LBUFF *vec = &map_lbuff[i];

        if (vec->buff < 0)
        {
            vec->buff = buff;
            return vec;
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119680 (MapLBuffLoadDoor, MapLBuffLoadVec).

MAP_LBUFF *MapLBuffLoad(int buff, int id, int no, MAP_PUT *put, int label, u_int flags)
{
    MAP_LBUFF *vec = MapLBuffGetFree(buff);
    if (vec == nullptr)
        return nullptr;

    vec->label = label;
    vec->id    = id;
    vec->flags = flags;
    vec->no    = no;
    vec->put   = put;
    // The door being taken is loaded again by the room behind it: that
    // side's copy starts hidden.
    if (map_door_vec != nullptr && map_door_vec->no == no && map_door_vec->put != put)
        vec->flags = flags | MAP_LBUFF_HIDE;
    return vec;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119720 (MapLightRegOn).

int MapLBuffLoadVec(int buff, int id, int no, MAP_PUT *put, int label,
                    const float (*vec)[4], u_int flags)
{
    MAP_LBUFF *lb = MapLBuffLoad(buff, id, no, put, label, flags);
    if (lb == nullptr)
        return -1;

    for (int i = 0; i < 4; i++)
    {
        memcpy(lb->quad[0][i], vec[i], sizeof(sceVu0FVECTOR));
        memcpy(lb->quad[1][i], vec[i], sizeof(sceVu0FVECTOR));
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119790 (IngameNewGameReset, MapDoorVecInit / Release): only
// buff is reset.

void MapLBuffInit(void)
{
    for (int i = 0; i < MAP_LBUFF_NUM; i++)
        map_lbuff[i].buff = -1;
}
