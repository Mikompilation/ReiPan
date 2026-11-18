/* ==========================================================================
 *  ingame/map/MapCtrl.h
 *
 *  Map control: the map side of the in-game loop. Starting a map
 *  (MapPlayStartReset: resets, a new CLightCtl, the start room load), the
 *  frame step, the draw of the rooms and the characters, the release, and
 *  the area-level height / wall / line helpers (FF2 kept those in MhCtl.c).
 *  FF3-only file 0x0011cea8..0x0011d648 (after MapPut.c, before MapPhf.c);
 *  the name is invented. It ends with the static CClassPtr<CLightCtl> at
 *  0x38e6d0 and its _GLOBAL__I_ / _GLOBAL__D_ pair.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPCTRL_H
#define _INGAME_MAP_MAPCTRL_H

#include "sdk/scetypes.h"

void MapStartNoSet(int no);             // FF3 0x0011cea8: the area MapPlayStartReset loads (< 0: area 1)

// FF3 0x0011ceb0: out = pos on area's ground (MhGetMapHeight; a miss outside
// a door mode tries the other buffer). 1 when the room has no height data
// (out = pos), < 0 when area is not loaded, 0 when no ground.
int  MapAreaGetHeight(float *out, float *pos, int area, int limit);
int  MapAreaHitLineCheck(float *from, float *to, int area);    // FF3 0x0011cf88: 0 when the line is free, -1 when area is not loaded
// FF3 0x0011cfe8: the circle r at pos pushed out of the walls (MrecWallPushOut;
// door modes take the area / floor / mh from the door). 1 when pushed.
int  MapAreaHitCircle(float r, float *pos, float *pos_old, int area, short floor, int mh_no);
int  MapAreaAtPos(short floor, float *pos);     // FF3 0x0011d0d8: the area holding pos, -1: none

int  MapFrameUpdate(void);              // FF3 0x0011d160: IngameMain's map step; returns 0
void MapDrawChars(u_int skip);          // FF3 0x0011d2e0: player shadow / model / camera models, once a frame (IngameDrawSub)
int  MapDrawAll(void);                  // FF3 0x0011d398: the rooms; returns 0
int  MapPlayStartIsReady(void);         // FF3 0x0011d3e8: the map start's CLightCtl has loaded (and is started)
void MapPlayStartReset(void);           // FF3 0x0011d430
void MapReleaseAll(void);               // FF3 0x0011d4d8

#endif /* _INGAME_MAP_MAPCTRL_H */
