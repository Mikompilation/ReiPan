/* ==========================================================================
 *  ingame/map/MapHit.h
 *
 *  Map hit (FF2 MapHit.c; FF3 keeps only the hit vec table's accessors
 *  here, the table itself is MapLBuff.h's). Minimal so far: what
 *  reconstructed code calls.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPHIT_H
#define _INGAME_MAP_MAPHIT_H

#include "sdk/scetypes.h"

struct MAP_LBUFF;                       // ingame/map/MapLBuff.h

struct MAP_LBUFF *MapHitRegistDoorVec(int no);  // FF3 0x00119368: hit vec no (0..31), nullptr when free
int  MapHitRegistCallback(int area_no, int floor, float *pos);  // FF3 0x00119348: returns 0 (MapFogProc skips the frame on non-zero; name from the matcher)

#endif /* _INGAME_MAP_MAPHIT_H */
