/* ==========================================================================
 *  ingame/story_door.h
 *
 *  FF3-only module after story_menu.c (FF3 0x1f20b8..0x1f3780; the name
 *  follows the story_*.c phase files, the code itself is unnamed): the
 *  area table (3ddata/room/area_data.ecb), the current area number and
 *  side, the door action check and the door modes of phase 0x22
 *  Story_Door_Open. All of it shares one sdata block (0x370ef0..0x370f1c).
 * ======================================================================== */
#ifndef _INGAME_STORY_DOOR_H
#define _INGAME_STORY_DOOR_H

#include "sdk/scetypes.h"
#include "ingame/map/DoorStat.h"                     // MAP_DOOR_DAT
#include "ingame/map/MapFog.h"                       // MAP_FOG_DAT

#include <stddef.h>                                     // offsetof

// A row of area_data.ecb (0x24 bytes; the ECB is checked for that size).
// layout: EE
typedef struct
{
    /* 0x00 */ s_char area;             // area number (MapAreaDataGet key)
    /* 0x01 */ u_char side;             // obj_pack side (RoomFileGetSide, EvSetHaichiId)
    /* 0x02 */ u_char cam_type;         // camera type (MrecSetCameraSet, EvCamChangeType)
    /* 0x03 */ u_char _03;
    /* 0x04 */ MAP_FOG_DAT fog[2];      // the area's fog outside every fog rectangle, [1] with the camera up (MapFogSetNowDat / MapFogAnimNowDat)
} MAP_AREA_DAT;
static_assert(sizeof(MAP_AREA_DAT) == 0x24, "MAP_AREA_DAT");
static_assert(offsetof(MAP_AREA_DAT, cam_type) == 0x02, "MAP_AREA_DAT.cam_type");
static_assert(offsetof(MAP_AREA_DAT, fog) == 0x04, "MAP_AREA_DAT.fog");

// MAP_DOOR_DAT (a door_stat.ecb row) lives in ingame/map/DoorStat.h.

// Door modes (names from the table 0x3299d0: "MAP_DOOR_MODE_AFT" ..).
enum
{
    MAP_DOOR_MODE_AFT   = 0,            // walking out of the door rectangle
    MAP_DOOR_MODE_NOW   = 1,            // the door motion plays
    MAP_DOOR_MODE_CLOSE = 2,
    MAP_DOOR_MODE_WAIT  = 3,
    MAP_DOOR_MODE_BFO   = 4,
    MAP_DOOR_MODE_NONE  = 5,            // MapDoorModeGet: no door step runs
};

// Area table (area_data.ecb, file 0xad8)
int  MapAreaDataGetSaveData(void **out);    // FF3 0x001f20b8: the table buffer, 0xa00 bytes (save block 47)
MAP_AREA_DAT *MapAreaDataGet(s_char area);  // FF3 0x001f20c8: the area's row, nullptr if none
void MapAreaDataAlloc(void);            // FF3 0x001f2160: 0xa00-byte buffer, loads file 0xad8
void MapAreaDataLoad(void);             // FF3 0x001f21a0: loads file 0xad8 again
void MapAreaDataFree(void);             // FF3 0x001f21c0

// Current area number and side (0 / 1: changing it deletes the other
// side's room).
int  MapAreaGetSaveData(void **out);    // FF3 0x001f2208: &area no, 8 bytes (no + side)
void MapAreaNoSet(int no);              // FF3 0x001f2218
int  MapAreaNoGet(void);                // FF3 0x001f2220
int  MapAreaSideGet(void);              // FF3 0x001f2228
void MapAreaSideSet(u_int side);        // FF3 0x001f2238
int  MapAreaIsSideArea(int area);       // FF3 0x001f2298: areas 0x2f..0x34
int  MapAreaFileNoGet(int area);        // FF3 0x001f22a8: room file index (+6 on side 1)
void MapAreaSideInit(void);             // FF3 0x001f22f0

// Doors
int  MapDoorAreaGet(void);              // FF3 0x001f2300: the area the door action was taken in
int  MapDoorRoomMhGet(int room);        // FF3 0x001f2308: the room's mh while a door mode runs
int  MapDoorModeGet(void);              // FF3 0x001f2320: MAP_DOOR_MODE_*
int  MapDoorModeIsOpen(void);           // FF3 0x001f2360: AFT or NOW
int  MapDoorModeIsMove(void);           // FF3 0x001f2380: AFT, NOW or WAIT
int  MapDoorIdGet(void);                // FF3 0x001f23c8: -1 outside the door phase
MAP_DOOR_DAT *MapDoorDataGet(void);     // FF3 0x001f23e0: the current door's row

// Area changes
void MapAreaMhSet(int area, int mh);    // FF3 0x001f23f8: player mh / floor, MapAreaNoSet
void MapAreaMoveReq(int area, int mh);  // FF3 0x001f2460: MapLoadMoveRoom(area)
int  MapAreaChange(int area, int mh);   // FF3 0x001f2478: 1 while busy, 0 once changed
void MapAreaFloorChange(int mh);        // FF3 0x001f2538: PLYR_FLOOR_CHANGE

void MapDoorLockInc(void);              // FF3 0x001f2568: the 13 door locks + 1
void MapDoorLockDec(void);              // FF3 0x001f25b0: the 13 door locks - 1

int  PlyrCheckAction(float rot, float *pos, short floor);          // FF3 0x001f2778: 1 when a door is taken
int  PlyrCheckActionPhaseReq(float rot, float *pos, short floor);  // FF3 0x001f2950: and requests phase 0x22
int  MapDoorAreaAtPos(short floor, float *pos);    // FF3 0x001f2990: the area on either side of the door holding pos, -1 for neither

int  MapDoorInit(void);                 // FF3 0x001f3728: door state cleared (IngameInit); 0
void MapDoorDelete(void);               // FF3 0x001f3748: the door object deleted (IngameEnd)

// Door mode steps (table 0x3299d0), run by one_Story_Door_Open; each runs
// the game frame and returns IngameMain's result. The path: Bfo (the player
// reaches for the door) -> Wait (SE bank, the other room) -> Now (the door
// opens) -> Aft (walking out of the door rectangle) -> AftEnd; a locked
// door goes Wait -> Close (-> MapDoorModeMsgOpen -> MapDoorModeMsg).
int  MapDoorModeAftEnd(void);           // FF3 0x001f2c38
int  MapDoorModeAft(void);              // FF3 0x001f2c88
int  MapDoorModeNow(void);              // FF3 0x001f2d58
int  MapDoorModeClose(void);            // FF3 0x001f2f70
int  MapDoorModeWait(void);             // FF3 0x001f3150
int  MapDoorModeBfo(void);              // FF3 0x001f3398

// Phase 0x22 (34), Story_Door_Open (no pre / after).
int  init_Story_Door_Open(void);        // FF3 0x001f33f0
int  end_Story_Door_Open(void);         // FF3 0x001f3680
int  one_Story_Door_Open(void);         // FF3 0x001f36e8

#endif /* _INGAME_STORY_DOOR_H */
