/* ==========================================================================
 *  ingame/map/MapLight.h
 *
 *  FF3 0x1197c8..0x11afa0. Two parts:
 *  - the hit vec table's rooms and doors (FF3-only; the table is
 *    MapLBuff.h's): a room's door leaves become wall quads, the tests walk
 *    them, MapDoorVec* pick the door the player is taking;
 *  - the map lights (FF2 MapLight.c): the player-only light, a room's real
 *    lights with the player light added (MapLightMakeRoomReal), and the door
 *    blend -- MapLightMakeDual picks the strongest lights of both rooms at
 *    the player into three sets (start, end, current), MapLightDualProc
 *    moves the current set's ranges from start to end over 30 steps.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPLIGHT_H
#define _INGAME_MAP_MAPLIGHT_H

#include "sdk/scetypes.h"

struct GRA3DLIGHTDATA;                  // graphics/graph3d/gra3d.h
struct MAP_LBUFF;                       // ingame/map/MapLBuff.h
struct MAP_PUT;                         // ingame/map/MapPut.h

// ── the hit vec table ────────────────────────────────────────────────
void MapLBuffDeleteRoom(int buff);      // FF3 0x001197c8: buffer buff's vecs freed, the other room's same doors shown (MapDrawDeleteNoDraw)
void MapLBuffDeleteId(int id, u_int mask);  // FF3 0x001198c8: vecs of id with a mask flag freed (MapLightRegOff passes 2)
// FF3 0x00119918: func(quad, arg) over buffer buff's vecs of label (< 0:
// any), quad2 too when used; 1 at the first non-zero result.
int  MapLBuffForEachQuad(int buff, int label, int (*func)(float *quad, void *arg), void *arg);
int  MapLBuffWallPushOut(float r, int label, float *pos);   // FF3 0x00119a20: circle r at pos pushed out of the shown quads (MrecWallPushOut); 1 when pushed
int  MapLBuffLineCheck(int a0, int label, float *p0, float *p1);  // FF3 0x00119b48: 1 when p0-p1 crosses a quad (MapHitLineCheck); a0 unused
int  MapLBuffSetDoorCoord(MAP_LBUFF *vec);  // FF3 0x00119c98: a door vec's quads from its door model; -1 / -2 errors, 0 / 1 = quad2 used
void MapDoorVecCoordSet(void);          // FF3 0x00119d48: the current door vec's quads from the door anime
int  MapDoorVecSelect(int id);          // FF3 0x00119d60: door id's vec becomes current (< 0: none); -1 when not found
void MapDoorVecInit(void);              // FF3 0x00119e18: MapLBuffInit, no current door vec (MapPlayStartReset)
void MapDoorVecRelease(void);           // FF3 0x00119e38: no current door vec, MapLBuffInit (MapReleaseAll)
int  MapLBuffLoadDoor(int room, const char *name, MAP_PUT *put);  // FF3 0x00119e50: the door put's vecs from the room's RegList (MapDrawRoomObjRegist); its door id, -1 / -2

// ── the lights ───────────────────────────────────────────────────────
void  MapLightSetPlayerOnly(void);      // FF3 0x0011a238: all lights off, the player light (slot 38) on unless lock 6
void  MapLightSetPlayerReal(void);      // FF3 0x0011a1f0: MapLightSetPlayerOnly and the non-prelight point / spot lights (no callers)
// FF3 0x0011a2c8: out = src scaled for real lighting, with the player light
// in slot 38 when it is on; MapPutSetLight passes sway 1 for sway puts.
void  MapLightMakeRoomReal(GRA3DLIGHTDATA *out, GRA3DLIGHTDATA *src, int sway);
void  MapDrawPrelightRoom(int buff, int clear);   // FF3 0x0011a3d0: buffer's light set, then its room model prelit (0) or its prelighting cleared (1); MapObjReqMdlRoom
GRA3DLIGHTDATA *MapLightGetDualWork(int no);      // FF3 0x0011a470: the door blend's set no (0 start, 1 end, 2 current)
GRA3DLIGHTDATA *MapLightDualProc(void);           // FF3 0x0011a508: one blend step; the current set
// FF3 0x0011acb0: starts the door blend from the two rooms' sets at pos
// (light1's lights fade in); MapDoorModeWait passes the new room first.
void  MapLightMakeDual(GRA3DLIGHTDATA *light1, GRA3DLIGHTDATA *light2, float *pos);

#endif /* _INGAME_MAP_MAPLIGHT_H */
