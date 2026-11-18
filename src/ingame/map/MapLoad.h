/* ==========================================================================
 *  ingame/map/MapLoad.h
 *
 *  Room streaming (FF2 MapLoad.c, FF3 0x11afa0..0x11bf28). Two room buffers
 *  (MapLoadBuff, FF2 MLOAD_HEAD): the player stands in one while the next
 *  area loads into the other. FF3 keys a buffer by its area number (FF2: by
 *  a file label) and loads an area as its model PK4 plus one obj_pack PHF
 *  (MapRoomFile.c).
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPLOAD_H
#define _INGAME_MAP_MAPLOAD_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"
#include "graphics/graph3d/gra3d.h"         // GRA3DLIGHTDATA
#include "ingame/map/MapRoomFile.h"         // REG_LIST_NUM
#include "ingame/map/MapObjWrk.h"           // MAP_OBJ_MDL

// MAP_ROOM_DAT.stat
#define MAP_ROOM_HIDE       0x1     // not drawn (MapLoadCheckDrawFlg == 0)
#define MAP_ROOM_LOADING    0x2     // files requested, MapLoadInitRoom not run yet
#define MAP_ROOM_FURN       0x8     // MapLoadCallFurn done

// MAP_ROOM_DAT.file[]: EE addresses of the area's files in the buffer.
#define ROOM_FILE_MDL_PK4   0       // model PK4 (RoomFileMdlLoadReq)
#define ROOM_FILE_DATA_PK4  1       // data PK4: members 3 hit, 4 area_pos PHF, 5 per-room words (MapLoadGetRoomData)
#define ROOM_FILE_OBJ_PACK  2       // the side's obj_pack PHF
#define ROOM_FILE_NUM       7       // MapLoadDeleteRoom clears seven words

// A room buffer (FF2 MLOAD_HEAD), bss 0x386580[2]. The addresses are EE
// addresses inside the loaded data (resolve with ReiPan_GetHostPointer).
// layout: host -- mdl holds host pointers (0x10 bytes on the EE); the
// offsets up to it match the ROM, the rest are the EE offsets.
struct MAP_ROOM_DAT                 /* 0x1670 on the EE */
{
    MAP_ROOM_DAT();                 // FF3 0x002ac330 (out of line; MapLoad_static_init runs it on MapLoadBuff)

    /* 0x0000 */ int    area;     // MapLoadGetRoomArea; -1 when empty
    /* 0x0004 */ int    room;       // its buffer number (0 / 1): the furniture and load slot it uses
    /* 0x0008 */ u_int  stat;       // MAP_ROOM_*
    /* 0x000c */ u_int  load_addr;  // EE address the room's files load to (0x8f8000 / 0xc3d000)
    /* 0x0010 */ float  origin[4];  // the area_pos entry (RoomFileAreaPosHook; MapLoadGetOffset)
    /* 0x0020 */ u_int  file[ROOM_FILE_NUM];   // ROOM_FILE_*
    /* 0x003c */ MAP_OBJ_MDL mdl;   // the room model (MapDrawInitRoom builds it, MAP_OBJ_MDL::Build)
    /* 0x004c */ u_char _4c[4];
    /* 0x0050 */ GRA3DLIGHTDATA light;          // the room's lights (MapDrawInitRoom)
    /* 0x1660 */ s_char reg_list[REG_LIST_NUM]; // its RegDat slot ids (RegList*)
};
static_assert(offsetof(MAP_ROOM_DAT, stat) == 0x08, "MAP_ROOM_DAT.stat");
static_assert(offsetof(MAP_ROOM_DAT, load_addr) == 0x0c, "MAP_ROOM_DAT.load_addr");
static_assert(offsetof(MAP_ROOM_DAT, origin) == 0x10, "MAP_ROOM_DAT.origin");
static_assert(offsetof(MAP_ROOM_DAT, file) == 0x20, "MAP_ROOM_DAT.file");
static_assert(offsetof(MAP_ROOM_DAT, mdl) >= 0x3c, "MAP_ROOM_DAT.mdl (EE +0x3c)");
static_assert(offsetof(MAP_ROOM_DAT, reg_list) == offsetof(MAP_ROOM_DAT, light) + sizeof(GRA3DLIGHTDATA), "MAP_ROOM_DAT.reg_list follows light (EE +0x1660)");


// MapLoadStat bits
#define MAP_LOAD_NOW        0x1     // a room load is running (MapLoadCheckLoadNow)
#define MAP_LOAD_NO_INIT    0x4     // MapLoadWaitMain skips MapLoadInitRoom once (nothing sets it in FF3)

extern int   MapLoadDrawID;         // sdata 0x36b858: the buffer being loaded / last loaded
extern u_int MapLoadStat;           // sdata 0x36b85c: MAP_LOAD_* (MapLoadChangeArea reads it)
extern const float map_room_rot[4];  // data 0x329530: the room model's rotation, none (MapLoadSetOffSet, MapDrawInitRoom, MapDrawRoom)

int   MapLoadGetArea4File(int file);        // FF3 0x0011afa0: (file - ROS00_PK4) / 7 (no callers)
int   MapLoadCheckDrawFlg(int room);        // FF3 0x0011afc0: 1 when buffer room is drawn
int   MapLoadCheckLoadNow(void);            // FF3 0x0011b000: map-load flag 1: a room load is running
int   MapLoadCheckLoadEnd(int room);        // FF3 0x0011b010: holds an area and its files are in
int   MapLoadCheckDisp(int room);           // FF3 0x0011b058: holds an area and is drawn
int   MapLoadCheckFreeAreaID(int area);     // FF3 0x0011b098: the buffer holding area, -1
MAP_ROOM_DAT *MapLoadGetFreeMemAddr(int room);      // FF3 0x0011b0d0: buffer 0 / 1, nullptr
int   MapLoadGetBuffID4Label(int label);    // FF3 0x0011b0f8: the buffer whose RegList has label, -1
void  MapLoadSetNoRegList(void);            // FF3 0x0011b168: hidden buffers' RegDat slots not registered
int   MapLoadGetRoomNoNow(void);            // FF3 0x0011b1b8: the current area when loaded, -1
MAP_ROOM_DAT *MapLoadGetCurRoomAddr(void);  // FF3 0x0011b1e0: nullptr when the current area is not loaded
void  MapLoadSetDrawFlg2(int room, int sw); // FF3 0x0011b220: sw 1 show, 2 hide buffer room
void  MapLoadSetDrawFlg(u_int flg, int sw); // FF3 0x0011b320: flg 2 the other buffer, 1 the current area's
void  MapLoadSetDrawFlg3(int area, int sw); // FF3 0x0011b3b0
void  MapLoadSetDrawOnly(int area);         // FF3 0x0011b3e0: only area's buffer drawn
void  MapLoadSetOffSet(int area, float x, float y, float z);    // FF3 0x0011b440: moves a loaded area
float *MapLoadGetOffset(int room);          // FF3 0x0011b510: buffer room's origin, nullptr
void  MapLoadGetOffsetVector(float *v, int room);   // FF3 0x0011b540: v = origin (0 when not loaded)
int   MapLoadGetRegBuffID(int area, int kai);       // FF3 0x0011b5a0: the RegDat slot of area on floor kai, -1
int   MapLoadCheckBuffPos(int room, int floor, float *pos);     // FF3 0x0011b608: drawn buffer room's RegList holds pos (floor)
int   MapLoadGetBuffID4Pos(int floor, float *pos);  // FF3 0x0011b678: the first such buffer, -1
int   MapLoadGetBuffID(int area);           // FF3 0x0011b6e0: area's buffer, -1; -2 for area < 0
int   MapLoadGetRoomArea(int room);         // FF3 0x0011b708: buffer room's area, -1
int   MapLoadInitRoom(void);                // FF3 0x0011b738: sets up the loaded buffer; MapLoadDrawID
void  MapLoadDeleteRoom(int room);          // FF3 0x0011b7c0: buffer 0 / 1 emptied
void  MapLoadDeleteRoomAll(void);           // FF3 0x0011b838
u_int MapLoadGetFileAddr(MAP_ROOM_DAT *dat, int no);    // FF3 0x0011b860: file[no] (EE address)
void *MapLoadGetRoomData(MAP_ROOM_DAT *dat, int no);    // FF3 0x0011b870: member no of the data PK4, nullptr
void *MapLoadGetAreaData(int area, int no); // FF3 0x0011b8b8: the same for area's buffer (no 3: the rcb file, MapRcb.h)
int   MapLoadCheckRoomDataNum(MAP_ROOM_DAT *dat);       // FF3 0x0011b8f0: the data PK4 has 7 members
void  MapLoadSetNowRoom(int area);          // FF3 0x0011b928: entering area (event SE, enemy set, play data)
void  MapLoadRegistReq(int area, int room); // FF3 0x0011b958: load area into buffer room
int   MapLoadMoveRoom(int area);            // FF3 0x0011b9c8: the buffer of area (loads it into the other one)
int   MapLoadReload(int area);              // FF3 0x0011ba70: reloads area into its buffer; 0, -1
int   MapLoadCallFurn(void);                // FF3 0x0011bac8: 1 once the obj_pack is in, -1 while loading, 0 done before
int   MapLoadWaitMain(void);                // FF3 0x0011bb60: -1 while a room loads, then finishes it; 0
void  MapLoadIngameInit(void);              // FF3 0x0011bbf8
int   MapLoadInit(int area);                // FF3 0x0011bc10: both buffers reset, area requested (< 0: none)
u_int MapLoadGetFreeArea(void);             // FF3 0x0011bd38: empties the other buffer; its load address (EE), 0
int   MapLoadChangeArea(int area, s_char side);         // FF3 0x0011bdc8: area (side, -1 keeps it) into the current area's buffer; it, -1
void  MapLoadRoomMission(MAP_ROOM_DAT *dat, int on);    // FF3 0x002ac3a8: the room model's mission entry

#endif /* _INGAME_MAP_MAPLOAD_H */
