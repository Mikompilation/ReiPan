/* ==========================================================================
 *  ingame/map/MapDoor.c
 *
 *  The door data table and the player's door motion pack (see MapDoor.h).
 *  FF3 0x1f4448..0x1f4788: FF2's MapDoorAnimInit / MapDoorLoadReq /
 *  MapDoorCheckLoad rewritten for FF3's per-player packs, plus the FF3-only
 *  door table readers; written from the FF3 decompilation.
 * ======================================================================== */
#include "ingame/map/MapDoor.h"

#include "common/vu0math.h"                     // NormalizeAngle, sceVu0*
#include "ingame/plyr/player.h"                 // PlyrGetCurNo
#include "reipan/reipan_memory.h"               // ReiPan_GetHostPointer
#include "system/eeiop/cd_file_dat.h"           // DCH000_DEFAULT_PK4 ..
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/os/eecdvd.h"                   // LoadReqGetAddr / IsLoadEnd

static u_int map_door_anim_addr;                                        /* sdata 370f4c (gp-0x25a4) */
static int   map_door_req_id;                                           /* sdata 370f50 (gp-0x25a0) */

// The door table, data 0x329a20 (dumped from the ELF). Rows 11 / 12 repeat
// 9 / 10 with other player animes; 13..17 are variants of 8 / 9 / 10.
alignas(16) static DOOR_DAT door_dat[DOOR_DAT_NUM] =
{
#define DOOR_POS(x, y, z)   {{{ x, y, z, 1.0f }, { x, y, z, 1.0f }}, {}}
    { 0,  { 2, -1}, 0, {62, 63}, {}, { DOOR_POS(-11.5f, 0.0f,  14.0f), DOOR_POS(-11.0f, 0.0f,  10.0f), DOOR_POS(-11.0f, 0.0f,  14.0f) } },
    { 1,  { 1, -1}, 1, {64, 65}, {}, { DOOR_POS(-11.5f, 0.0f,  14.0f), DOOR_POS(-12.5f, 0.0f,  11.5f), DOOR_POS(-10.5f, 0.0f,  13.5f) } },
    { 2,  { 1, -1}, 0, {66, 67}, {}, { DOOR_POS( 11.5f, 0.0f,  14.0f), DOOR_POS( 12.0f, 0.0f,  10.0f), DOOR_POS( 11.0f, 0.0f,  14.0f) } },
    { 3,  { 2, -1}, 1, {68, 69}, {}, { DOOR_POS( 11.5f, 0.0f,  14.0f), DOOR_POS( 12.0f, 0.0f,  11.5f), DOOR_POS( 10.5f, 0.0f,  13.5f) } },
    { 4,  { 1,  2}, 0, {70, 71}, {}, { DOOR_POS(  0.0f, 0.0f,  16.0f), DOOR_POS(  0.0f, 0.0f,  10.0f), DOOR_POS(  0.0f, 0.0f,  15.5f) } },
    { 5,  { 1,  2}, 1, {72, 73}, {}, { DOOR_POS(  0.0f, 0.0f,  16.0f), DOOR_POS(  0.0f, 0.0f,   8.0f), DOOR_POS(  0.0f, 0.0f,  16.0f) } },
    { 6,  { 1, -1}, 0, {74, 75}, {}, { DOOR_POS(  8.0f, 0.0f,  11.5f), DOOR_POS(  7.0f, 0.0f,  11.5f), DOOR_POS(  7.5f, 0.0f,  14.0f) } },
    { 7,  { 1, -1}, 1, {76, 77}, {}, { DOOR_POS( -9.0f, 0.0f,  11.5f), DOOR_POS( -7.0f, 0.0f,  11.5f), DOOR_POS( -6.5f, 0.0f,  14.0f) } },
    { 8,  { 1,  1}, 0, {78, 79}, {}, { DOOR_POS(  0.0f, 0.0f,  15.0f), DOOR_POS(  0.0f, 0.0f,  13.0f), DOOR_POS(  0.0f, 0.0f,  16.0f) } },
    { 9,  { 0, -1}, 1, {80, 81}, {}, { DOOR_POS(-10.0f, 0.0f, -16.0f), DOOR_POS(-10.0f, 0.0f, -16.5f), DOOR_POS( -9.5f, 0.0f, -17.0f) } },
    { 10, { 0, -1}, 0, {86, 87}, {}, { DOOR_POS( 11.0f, 0.0f, -17.5f), DOOR_POS( 12.0f, 0.0f, -16.0f), DOOR_POS( 10.2f, 0.0f, -19.5f) } },
    { 11, { 0, -1}, 1, {66, 67}, {}, { DOOR_POS(-10.0f, 0.0f, -16.0f), DOOR_POS(-10.0f, 0.0f, -16.5f), DOOR_POS( -9.5f, 0.0f, -17.0f) } },
    { 12, { 0, -1}, 0, {68, 69}, {}, { DOOR_POS( 10.0f, 0.0f, -17.5f), DOOR_POS( 10.0f, 0.0f, -16.0f), DOOR_POS( 10.2f, 0.0f, -19.5f) } },
    { 13, { 1,  1}, 1, {78, 79}, {}, { DOOR_POS(  0.0f, 0.0f,  15.0f), DOOR_POS(  0.0f, 0.0f,  15.0f), DOOR_POS(  0.0f, 0.0f,  15.0f) } },
    { 14, { 0, -1}, 1, {84, 85}, {}, { DOOR_POS(-10.0f, 0.0f, -16.0f), DOOR_POS(-10.0f, 0.0f, -16.5f), DOOR_POS( -9.5f, 0.0f, -17.0f) } },
    { 15, { 0, -1}, 1, {82, 83}, {}, { DOOR_POS(-10.0f, 0.0f, -16.0f), DOOR_POS(-10.0f, 0.0f, -16.5f), DOOR_POS( -9.5f, 0.0f, -17.0f) } },
    { 16, { 0, -1}, 0, {90, 91}, {}, { DOOR_POS( 11.0f, 0.0f, -17.5f), DOOR_POS( 12.0f, 0.0f, -16.0f), DOOR_POS( 10.2f, 0.0f, -19.5f) } },
    { 17, { 0, -1}, 0, {88, 89}, {}, { DOOR_POS( 11.0f, 0.0f, -17.5f), DOOR_POS( 12.0f, 0.0f, -16.0f), DOOR_POS( 10.2f, 0.0f, -19.5f) } },
#undef DOOR_POS
};
static_assert(sizeof(door_dat) == 0x32a680 - 0x329a20, "door_dat spans 0x329a20..0x32a680");

// The door motion packs by player, -1 terminated (rodata 0x35b7d0).
static const int map_door_anim_file[] =
{
    DCH000_DEFAULT_PK4, DCH001_DEFAULT_PK4, DCH002_DEFAULT_PK4, -1,
};

// ──────────────────────────────────────────────────────────────────────
// The door motion buffer, an EE address (MapDBuffIsReady, MapDrawRoomObjRegist
// hand it to the door model builder).
// FF3 0x001f4448.

u_int MapDoorAnimGetAddr(void)
{
    return map_door_anim_addr;
}

// ──────────────────────────────────────────────────────────────────────
// The row whose id is `id`, nullptr when there is none.
// FF3 0x001f4450.

DOOR_DAT *DoorDatGet(int id)
{
    for (DOOR_DAT *dat = door_dat; dat < door_dat + DOOR_DAT_NUM; dat++)
    {
        if (dat->id == id)
            return dat;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The door model's motion in the door motion pack. Doors 0xb / 0xc read
// rows 9 / 10; the row's id then maps through a jump table (rodata
// 0x35b7a0, ids 6..0x11) so look-alike doors share a motion.
// FF3 0x001f4488.

int DoorDatGetMotNo(int id)
{
    short no = (short)id;

    if (no == 0xb)
        no = 9;
    if (no == 0xc)
        no = 10;

    DOOR_DAT *dat = DoorDatGet(no);
    if (dat == nullptr)                 // PC: FF3 reads word 0 of address 0
        return 0;

    switch (dat->id)
    {
    case 6:
        return 7;
    case 0xd:
        return 8;
    case 0xe:
    case 0xf:
        return 9;
    case 0x10:
    case 0x11:
        return 10;
    default:
        return dat->id;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The player's start position at a door (init_Story_Door_Open): player
// plyr's offset (side != 0: pos[0], else pos[1]) as (x, y, -z, 1) -- FF3
// computes 0 - pos and negates x and y back, w is the constant 1.0 at
// 0x370f54 -- turned by pi about Y for turn == 1 rows, then through mtx.
// pos is untouched when the door is unknown.
// FF3 0x001f4508.

void DoorDatGetPlyrPos(int plyr, float *pos, int id, float *mtx, int side)
{
    DOOR_DAT *dat = DoorDatGet(id);

    if (dat == nullptr)
        return;

    const float *src = dat->plyr[plyr].pos[side != 0 ? 0 : 1];
    sceVu0FVECTOR v;

    v[0] = src[0];
    v[1] = src[1];
    v[2] = -src[2];
    v[3] = 1.0f;

    if (dat->turn == 1)
    {
        sceVu0FMATRIX m;

        sceVu0UnitMatrix(m);
        sceVu0RotMatrixY(m, m, NormalizeAngle(3.1415927f));        // lit4 0x370f58
        sceVu0ApplyMatrix(v, m, v);
    }
    sceVu0ApplyMatrix(pos, (float (*)[4])mtx, v);
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the door motion pack MapDoorLoadReq asked for is in. FF2 also ran
// sgdRemap on the loaded door here; FF3 does not.
// FF3 0x001f4668.

int MapDoorCheckLoad(void)
{
    return IsLoadEnd(map_door_req_id);
}

// ──────────────────────────────────────────────────────────────────────
// Loads player plyr's door motion pack into the door motion buffer
// (MapDoorAnimInit, and after a player model change). Any other plyr loads
// file 0, as FF3 does.
// FF3 0x001f4680.

void MapDoorLoadReq(int plyr)
{
    int file = 0;

    if (plyr == 1)
        file = DCH001_DEFAULT_PK4;
    else if (plyr == 0)
        file = DCH000_DEFAULT_PK4;
    else if (plyr == 2)
        file = DCH002_DEFAULT_PK4;

    LoadReqGetAddr(file, ReiPan_GetHostPointer(map_door_anim_addr), &map_door_req_id);
}

// ──────────────────────────────────────────────────────────────────────
// Places the door motion buffer at EE addr (MapLoadInit), sized for the
// largest of the three packs (unsigned compare, as FF3), and loads the
// current player's. Returns the end of the buffer.
// FF3 0x001f46d8.

u_int MapDoorAnimInit(u_int addr)
{
    u_int size = 0;

    map_door_anim_addr = addr;
    for (int i = 0; map_door_anim_file[i] >= 0; i++)
    {
        u_int file_size = GetFileSize(map_door_anim_file[i]);

        if (size < file_size)
            size = file_size;
    }
    MapDoorLoadReq(PlyrGetCurNo());
    return addr + size;
}
