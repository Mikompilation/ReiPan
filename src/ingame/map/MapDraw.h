/* ==========================================================================
 *  ingame/map/MapDraw.h
 *
 *  Room drawing (FF2 MapDraw.c; FF3 0x117720..0x118900). Each room buffer
 *  has two light sets: bank 0 (bss 0x37a4d0) is the room's own lights at
 *  its origin (MapDrawInitRoom), bank 1 (0x37d0f0) what the room was last
 *  drawn with -- bank 0 after the light control and the lantern, plus the
 *  player's flashlight in light 38 (MapDrawSetRoomLight); the map objects
 *  of the room light from it (MapPutSetLight). MapDrawPreLight can replace
 *  both with one set. Also the furniture model loads of a room (the records
 *  in FurnLoad.c) and its objects' registration from area_pos.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPDRAW_H
#define _INGAME_MAP_MAPDRAW_H

#include "sdk/scetypes.h"

struct GRA3DLIGHTDATA;                  // graphics/graph3d/gra3d.h
struct MAP_ROOM_DAT;                    // ingame/map/MapLoad.h

#define MAP_DRAW_LIGHT_PLAYER 38        // the light the player's flashlight goes into

void  MapDrawInit(void);                // FF3 0x00117720: the furniture records freed (MapLoadInit)
void *MapDrawLoadFurnAll(int room, void *pos_data, void *addr);  // FF3 0x00117940: the area_pos objects' models not loaded yet, from addr; the address after them
void *MapDrawInitFurn(int room, void *pack, void *addr);    // FF3 0x00117a00: the obj_pack's furniture recorded, their motions loaded from addr; the address after them
void  MapDrawPreLight(GRA3DLIGHTDATA *light);   // FF3 0x00117af8: light replaces both banks of the loaded buffers (nullptr: back to them)
GRA3DLIGHTDATA *MapDrawGetLightPtr4BuffBank(int buff, int bank);    // FF3 0x00117b58: bank 0 / 1 of buffer (buff & 1)
GRA3DLIGHTDATA *MapDrawGetLightPtr4BuffID(int buff, int bank);      // FF3 0x00117b90: the MapDrawPreLight set, else bank of buffer buff when it holds an area; nullptr
GRA3DLIGHTDATA *MapDrawGetLightPtr(int area, int bank);    // FF3 0x00117c28: the same for area's buffer; nullptr when not loaded
GRA3DLIGHTDATA *MapDrawGetLightPtrDoor(int area, int bank);    // FF3 0x00117c70: the door's area while a door opens
int   MapDrawInitRoom(MAP_ROOM_DAT *dat);       // FF3 0x00117cc0: room model, lights and map height of a loaded buffer; 0
void  MapDrawRoomObjRegist(MAP_ROOM_DAT *dat);  // FF3 0x00117de8: the area_pos objects of a buffer being shown
void  MapDrawDeleteNoDraw(MAP_ROOM_DAT *dat);   // FF3 0x00118318: objects of a buffer being hidden
void  MapDrawDeleteRoom(MAP_ROOM_DAT *dat);     // FF3 0x00118370: everything of a buffer being emptied
void  MapDrawDeleteRoomAll(void);               // FF3 0x001183b0
void  MapDrawRooms(u_int flags);        // FF3 0x00118650: both loaded rooms (flags 0x200: the alpha pass)
void  MapDrawEnableFlashlightOnly(int b);   // FF3 0x00118758
int   MapDrawIsEnableFlashlightOnly(void);  // FF3 0x00118760: 1 (the default) lets MapDrawSetRoomLight / MapPutSetLight set the room's lights; 0 leaves them
void  MapDrawLightReset(void);          // FF3 0x00118768: bank 0 cleared, no MapDrawPreLight set, MapDrawEnableFlashlightOnly(1)
void  MapDrawRelease(void);             // FF3 0x001187d0: MapDrawDeleteRoomAll

#endif /* _INGAME_MAP_MAPDRAW_H */
