/* ==========================================================================
 *  ingame/map/DoorStat.h
 *
 *  The door table and the door states (FF3-only, 0x154720..0x154cd0; name
 *  invented after FurnSave.c). File 0xad6 DOOR_STAT_ECB
 *  (3ddata/room/door_stat.ecb, 161 rows of 0x40 bytes) is loaded into a
 *  system-heap buffer for the in-game run; the door state work (0x393748,
 *  0x404 bytes, save block 49) holds a flags word and one state word per
 *  ECB row: the lock state, whether the player has tried the door (the map
 *  mark's colour) and a no-use flag the door action check honours.
 * ======================================================================== */
#ifndef _INGAME_MAP_DOORSTAT_H
#define _INGAME_MAP_DOORSTAT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One row of door_stat.ecb (0x40 bytes; ECB rows from +0x20 of the file).
// Door ids are room * 1000 + n (101140 ..); 1000..1002 are rows without an
// area (DoorStatSetKnownSys). Fields without a known reader keep their
// offset as name.
// layout: EE
typedef struct
{
    /* 0x00 */ int    id;               // door id
    /* 0x04 */ int    area;             // the area behind the door (KaiPzlTerm 0x1f2990)
    /* 0x08 */ int    mh;               // map hit object behind the door
    /* 0x0c */ short  dat_id;           // its DOOR_DAT (DoorDatGet: PlyrDoorStart, the door anime)
    /* 0x0e */ short  dbuff;            // the opened door's model, file 0x1590 + dbuff (CMapDBuff::LoadReq); the cat's door no; the map mark (DoorGetMarkDoors)
    /* 0x10 */ int    _10;              // 1 in every row but 101143 / 101144
    /* 0x14 */ u_short rot;             // facing angle in degrees (EvRotInRange)
    /* 0x16 */ short  se;               // door SE pair: 0x1c63 + se * 2
    /* 0x18 */ float  _18[3];           // a position in the first rooms' rows, 0 elsewhere
    /* 0x24 */ float  plyr_rot;         // the player's facing in degrees while the door opens
    /* 0x28 */ float  cam_pos[3];       // the door camera, from the room's origin
    /* 0x34 */ float  cam_tgt[3];
} MAP_DOOR_DAT;
static_assert(sizeof(MAP_DOOR_DAT) == 0x40, "MAP_DOOR_DAT is 0x40 bytes");
static_assert(offsetof(MAP_DOOR_DAT, dat_id) == 0x0c, "MAP_DOOR_DAT.dat_id");
static_assert(offsetof(MAP_DOOR_DAT, dbuff) == 0x0e, "MAP_DOOR_DAT.dbuff");
static_assert(offsetof(MAP_DOOR_DAT, rot) == 0x14, "MAP_DOOR_DAT.rot");
static_assert(offsetof(MAP_DOOR_DAT, se) == 0x16, "MAP_DOOR_DAT.se");
static_assert(offsetof(MAP_DOOR_DAT, plyr_rot) == 0x24, "MAP_DOOR_DAT.plyr_rot");
static_assert(offsetof(MAP_DOOR_DAT, cam_pos) == 0x28, "MAP_DOOR_DAT.cam_pos");
static_assert(offsetof(MAP_DOOR_DAT, cam_tgt) == 0x34, "MAP_DOOR_DAT.cam_tgt");

// A door's state word (one u_int per ECB row; the EE reads them as
// bitfields, the port with these masks).
#define DOOR_STAT_LOCK_MASK 0x07        // bits 0..2: lock state (0 open, 2 locked, 3 / 4 locked with a message)
#define DOOR_STAT_KNOWN     0x08        // bit 3: the player has tried the door
#define DOOR_STAT_NOUSE     0x10        // bit 4: the door action check ignores the door

#define DOOR_STAT_DOOR_MAX  256
#define DOOR_STAT_WRK_SIZE  0x404       // flags word + DOOR_STAT_DOOR_MAX words (save block 49)

// The door table (door_stat.ecb)
int  DoorGetMarkDoors(short mark, int *ids);    // FF3 0x00154720: ids of the rows with dbuff == mark; their count
int  DoorGetMarkDoorNos(short mark, int *nos);  // FF3 0x001547c8: their row numbers; the count (no callers)
MAP_DOOR_DAT *DoorStatGetData(int id);  // FF3 0x00154870: the door's row, nullptr if none
int  DoorStatGetNo(int id);             // FF3 0x001548e0: the door's row number, -1 if none
int  DoorStatLoad(void);                // FF3 0x00154950: IngameInit; 0
void DoorStatSetKnownSys(void);         // FF3 0x001549a8: doors 1000..1999 known
void DoorStatApply(void);               // FF3 0x00154a18: IngameNowLoadingMain
void DoorStatFree(void);                // FF3 0x00154a30: IngameEnd

// The door states
int  DoorStatSaveGet(void **addr);      // FF3 0x00154a78: {the state work, 0x404} (save block 49)
u_int *DoorStatGetWrk(int id);          // FF3 0x00154a90: the door's state word
void DoorStatSetNoUse(int id, u_int on);    // FF3 0x00154ac0
int  DoorStatIsNoUse(int id);           // FF3 0x00154b00: -1 / 0
void DoorStatSetLock(int id, u_int lock);   // FF3 0x00154b38
int  DoorStatGetLock(int id);           // FF3 0x00154b78
void DoorStatSetKnown(int id, int on);  // FF3 0x00154bb0
int  DoorStatIsKnown(int id);           // FF3 0x00154bf0: -1 / 0
void DoorStatClearKnownAll(void);       // FF3 0x00154c28: EvGoDreamWorld / EvGoRealWorld
u_int DoorStatGetFlag(u_int mask);      // FF3 0x00154c90: the flags word & mask (bit 0: no door actions)
void DoorStatClear(void);               // FF3 0x00154ca0: IngameInit, a new game
void DoorStatEnd(void);                 // FF3 0x00154cc8: empty

#endif /* _INGAME_MAP_DOORSTAT_H */
