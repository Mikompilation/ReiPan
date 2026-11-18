// FILE: ingame/map/MapHit.c
//
// Map hit (FF2 MapHit.c's slot, FF3 0x119348..0x119390). FF2's hit record
// registry is gone; what is left are the hit vec table's accessors (the
// table is MapLBuff.c's) and a callback hook that does nothing.

#include "ingame/map/MapHit.h"

#include "ingame/map/MapLBuff.h"                // MAP_LBUFF, map_lbuff
#include "system/mc/dat/save_data.h"            // MapLBuffSaveGet

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119348 (MapFogProc skips its frame on non-zero): returns 0.

int MapHitRegistCallback(int area_no, int floor, float *pos)
{
    (void)area_no;
    (void)floor;
    (void)pos;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Save block 51. FF3 0x00119350.
// PC: the block is the host table (MAP_LBUFF.put is a host pointer, so the
// host size differs from the ROM's 0x2600; open-items.md).

int MapLBuffSaveGet(void **addr)
{
    *addr = map_lbuff;
    return sizeof(map_lbuff);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00119368.

MAP_LBUFF *MapHitRegistDoorVec(int no)
{
    MAP_LBUFF *vec = &map_lbuff[no];

    return (vec->buff < 0) ? nullptr : vec;
}
