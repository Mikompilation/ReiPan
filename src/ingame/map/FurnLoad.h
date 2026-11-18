/* ==========================================================================
 *  ingame/map/FurnLoad.h
 *
 *  Furniture loading (FF2 FurnLoad.c; FF3 0x116fa0..0x117720). FF3 keeps
 *  FF2's key table (an object name's prefix -> its block of model files and
 *  the kind of object it is), and adds a record per loaded furniture model:
 *  48 records (bss 0x379a48) of the two room buffers, by object name, with
 *  the model PK4 and its default motion PK4 (MapDraw.c loads them). Also the
 *  object-name parsers ("f034", "d012_zL-020", "e003_no12").
 * ======================================================================== */
#ifndef _INGAME_MAP_FURNLOAD_H
#define _INGAME_MAP_FURNLOAD_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// FurnLoadGetAttr: what an object name's prefix is (MapDrawRoomObjRegist's switch).
#define FURN_ATTR_FURN0     0           // "f0".."f3": furniture
#define FURN_ATTR_FURN1     1           // "f4"
#define FURN_ATTR_FURN2     2           // "f5"
#define FURN_ATTR_FURN3     3           // "f6"
#define FURN_ATTR_EFFECT    4           // "f7".."f9": furniture with a water-flow effect (801..999)
#define FURN_ATTR_DOOR      5           // "d"
#define FURN_ATTR_EVENT     6           // "e": an effect object (MapEffRegist)
#define FURN_ATTR_AREA_POS  7           // "area_pos": the room's own entry

#define FURN_KEY_NUM 13

// A name prefix (data 0x34dbf0, 12 bytes each on the EE).
// layout: host -- key is an EE pointer to an sdata string.
struct FURN_LOAD_TBL
{
    /* 0x00 */ const char *key;
    /* 0x04 */ int         label;       // first model file of the block (F000_PK4 0xadf, D000 0x1590), -1 none
    /* 0x08 */ int         attr;        // FURN_ATTR_*
};
static_assert(offsetof(FURN_LOAD_TBL, key) == 0, "FURN_LOAD_TBL.key");

// FURN_LOAD_REC.flag
#define FURN_REC_FLAG1      0x1         // cleared by FurnLoadClearFlag1 (MapDrawDeleteNoDraw); no setter traced
#define FURN_REC_PRELIT     0x2         // the object was prelit (MapObjRegist, MapObjReqMdlRoom)

#define FURN_LOAD_REC_NUM 48

// One loaded furniture model (bss 0x379a48, 0x38 bytes each on the EE).
// layout: host -- mdl_pk4 / mot_pk4 are EE pointers (host pointers here).
struct FURN_LOAD_REC
{
    /* 0x00 */ int     room;            // its room buffer (0 / 1), -1 free
    /* 0x04 */ int     size;            // bytes of its model data
    /* 0x08 */ u_short flag;            // FURN_REC_*
    /* 0x0a */ char    name[0x26];      // the object name; lookups compare 4 chars
    /* 0x30 */ void   *mdl_pk4;
    /* 0x34 */ void   *mot_pk4;         // its default motion (MapDrawLoadFurnMot), nullptr
};
static_assert(offsetof(FURN_LOAD_REC, size) == 0x04, "FURN_LOAD_REC.size");
static_assert(offsetof(FURN_LOAD_REC, flag) == 0x08, "FURN_LOAD_REC.flag");
static_assert(offsetof(FURN_LOAD_REC, name) == 0x0a, "FURN_LOAD_REC.name");
static_assert(offsetof(FURN_LOAD_REC, mdl_pk4) >= 0x30, "FURN_LOAD_REC.mdl_pk4 (EE +0x30)");

int  FurnLoadGetKeyLabel(const char *name);     // FF3 0x00117020: the prefix's first file, -1
int  FurnLoadGetAttr(const char *name);         // FF3 0x00117050: FURN_ATTR_*, -1
int  FurnLoadIsRec(int room, const char *name); // FF3 0x001170f8: room has a record of name
int  FurnLoadSetRecFlag(int room, const char *name, u_short flag);  // FF3 0x00117118: record flags |= flag; 0, -1 without a record
int  FurnLoadCallFunc(int room, const char *name);  // FF3 0x00117160: 1 for a record not prelit yet ("f049" never)
void FurnLoadClearPrelit(int room);         // FF3 0x00117208: FURN_REC_PRELIT off on room's records
void FurnLoadClearFlag1(int room);          // FF3 0x00117220: FURN_REC_FLAG1 off on room's records
int  FurnLoadGetSize(int room, const char *name);       // FF3 0x00117238: the record's size, 0
void *FurnLoadGetMdlPk4(int room, const char *name);    // FF3 0x00117268: the record's model PK4, nullptr
void *FurnLoadGetMotPk4(int room, const char *name);    // FF3 0x00117298: its motion PK4, nullptr
u_short *FurnLoadGetFlagPtr(int room, const char *name);    // FF3 0x001172c8: &record->flag, nullptr
void FurnLoadSetMotPk4(int room, const char *name, void *mot);  // FF3 0x001172f0
int  FurnNameGetNum(const char *str, int start, int len);   // FF3 0x00117318: len decimal digits from str[start], -1 at a non-digit
int  FurnNameGetNo(const char *name);       // FF3 0x001173b0: the three digits of a "d..." / "f..." name, -1
int  FurnNameGetFileNo(const char *name);   // FF3 0x001173f8: the name's model file; -1 no key, -2 no number, -3 out of its block
int  FurnNameGetEventNo(const char *name);  // FF3 0x00117498: an "e" name's "_no" number, 0
int  FurnNameGetZ(const char *name, int *kind); // FF3 0x001174e0: the "_z" suffix's number; *kind 0 L, 1 N, 2 F, 3 none
int  FurnLoadAddRec(int room, const char *name, void *mdl, int size);   // FF3 0x00117640: 0, -1 when full
void FurnLoadDeleteRoom(int room);          // FF3 0x001176b0: room's records freed
void FurnLoadInitRec(void);                 // FF3 0x001176e8: every record freed

#endif /* _INGAME_MAP_FURNLOAD_H */
