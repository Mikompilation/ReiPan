/* ==========================================================================
 *  ingame/map/MapDoor.h
 *
 *  The door data table and the player's door motion pack (FF3 0x1f4448..
 *  0x1f4788). FF2's MapDoor.c held the resident-door registry and the whole
 *  door transition; in FF3 those moved to story_door.c / MapDBuff.c and what
 *  is left here is FF2's MapDoorAnimInit / MapDoorLoadReq / MapDoorCheckLoad
 *  plus the door table: 18 rows of 0xb0 bytes at data 0x329a20, one per kind
 *  of door (door_stat.ecb's dat_id picks the row).
 *
 *  The door motion pack is per player: DCH000 / DCH001 / DCH002_DEFAULT_PK4
 *  (files 0xadb..0xadd, 3ddata/door/motion/) for Rei, Miku and Kei, loaded
 *  into one buffer sized for the largest (MapDoorAnimInit). A player model
 *  change reloads it (EvPlyrMdlChangeStart / EvPlyrMdlChangeWait).
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPDOOR_H
#define _INGAME_MAP_MAPDOOR_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define DOOR_DAT_NUM    18              // rows at 0x329a20..0x32a680
#define DOOR_DAT_PLYR   3               // per-player rows: Rei, Miku, Kei (PlyrGetCurNo)

// One player's start position at a door, in the door model's space before
// DoorDatGetPlyrPos flips z (0x30 bytes). Side 0 and side 1 hold the same
// values in every retail row.
// layout: EE (floats only)
struct DOOR_DAT_POS                     /* 0x30 */
{
    /* 0x00 */ float pos[2][4];         // [0] side != 0, [1] side == 0 (DoorDatGetPlyrPos); w 1
    /* 0x20 */ float _20[4];            // 0 in every row, no reader
};
static_assert(sizeof(DOOR_DAT_POS) == 0x30, "DOOR_DAT_POS is 0x30 bytes");

// One kind of door (0xb0 bytes), found by id (DoorDatGet).
// layout: EE (ints and floats only)
struct DOOR_DAT                         /* 0xb0 */
{
    /* 0x00 */ int  id;                 // == the row number
    /* 0x04 */ int  leaf[2];            // the hit vec kind of door leaf 0 / 1 (MapLBuffSetDoorCoord -> MapLBuffSetDoorQuad; leaf 1 < 0: one leaf)
    /* 0x0c */ int  turn;               // 1: the player's offsets are turned by pi about Y (DoorDatGetPlyrPos)
    /* 0x10 */ int  plyr_anime[2];      // the player's anime from side 0 / 1 (PlyrDoorStart; <= 0: none)
    /* 0x18 */ int  _18[2];             // 0 in every row, no reader
    /* 0x20 */ DOOR_DAT_POS plyr[DOOR_DAT_PLYR];
};
static_assert(sizeof(DOOR_DAT) == 0xb0, "DOOR_DAT is 0xb0 bytes");
static_assert(offsetof(DOOR_DAT, leaf) == 0x04, "DOOR_DAT.leaf");
static_assert(offsetof(DOOR_DAT, turn) == 0x0c, "DOOR_DAT.turn");
static_assert(offsetof(DOOR_DAT, plyr_anime) == 0x10, "DOOR_DAT.plyr_anime");
static_assert(offsetof(DOOR_DAT, plyr) == 0x20, "DOOR_DAT.plyr");

u_int MapDoorAnimGetAddr(void);         // FF3 0x001f4448: the door motion buffer (EE address, sdata 0x370f4c)
DOOR_DAT *DoorDatGet(int id);           // FF3 0x001f4450; nullptr: no such door
int   DoorDatGetMotNo(int id);          // FF3 0x001f4488: the door model's motion (doors share them: 0xb / 0xc read rows 9 / 10, then a remap)
// FF3 0x001f4508: the player's start pos at door `id` into pos (4 floats):
// the player's row offset as (x, y, -z, 1), turned by pi when turn is 1,
// through mtx (the door put's 4x4). side != 0 reads pos[0], 0 pos[1].
void  DoorDatGetPlyrPos(int plyr, float *pos, int id, float *mtx, int side);
int   MapDoorCheckLoad(void);           // FF3 0x001f4668: 1 when the door motion pack is in (IsLoadEnd)
void  MapDoorLoadReq(int plyr);         // FF3 0x001f4680: loads player plyr's door motion pack (0xadb + plyr; another value loads file 0)
u_int MapDoorAnimInit(u_int addr);      // FF3 0x001f46d8: the door motion buffer at EE addr; addr + the largest of files 0xadb..0xadd

#endif /* _INGAME_MAP_MAPDOOR_H */
