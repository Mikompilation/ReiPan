/* ==========================================================================
 *  ingame/map/FurnSave.h
 *
 *  The furniture save table (FF3-only, 0x1fa310..0x1fa768): file 0xad7
 *  FURN_SAVE_ECB (3ddata/room/furn_save.ecb, 340 rows) loaded into a
 *  0x3000-byte system-heap buffer that goes into the save as it is (block
 *  48). One row per placed effect, furniture or door of every area: its
 *  visibility, look flag, motion request, hit check, alpha and wind. The
 *  event commands (SET_OBJ_*, SET_EFF_VISIBLE, SET_FURN_WIND) edit rows;
 *  the map object works (MapObjWrk.c) apply them.
 * ======================================================================== */
#ifndef _INGAME_MAP_FURNSAVE_H
#define _INGAME_MAP_FURNSAVE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// Row kinds, from the object name's letter.
#define FURN_SAVE_KIND_EFF  0           // "e..."
#define FURN_SAVE_KIND_FURN 1           // "f..."
#define FURN_SAVE_KIND_DOOR 2           // "d..."

#define FURN_SAVE_BUF_SIZE  0x3000

// One row of furn_save.ecb (0x24 bytes; ECB rows from +0x20 of the file).
// Fields without a known reader keep their offset as name.
struct FURN_SAVE_ROW                    /* 0x24 */
{
    /* 0x00 */ short  area;
    /* 0x02 */ short  kind;             // FURN_SAVE_KIND_*
    /* 0x04 */ short  id;               // the three digits of the object name
    /* 0x06 */ u_char vis[4];           // shown, one byte per visibility mask bit 0..3 (FurnSaveGetVisMask: 1 normal, 2 in the viewfinder)
    /* 0x0a */ u_char look;             // SET_PLYR_LOOK_FURN: the player may look at it (MapObjLookSearch)
    /* 0x0b */ u_char action_type;      // SET_OBJ_ACTIONTYPE; MapObjSetAction runs 1..3 (1 becomes 2)
    /* 0x0c */ short  req_action;       // SET_OBJ_REQ_ACTION: the motion MapObjSetAction plays
    /* 0x0e */ u_char _0e;
    /* 0x0f */ u_char hit_check;        // SET_OBJ_HITCHECK
    /* 0x10 */ u_char alpha;            // SET_OBJ_ALPHA: the target
    /* 0x11 */ u_char alpha_cur;        // moves 2 a frame towards alpha (FurnSaveMain); MapObjMain puts it on the model
    /* 0x12 */ short  label;            // FurnSaveFind's label
    /* 0x14 */ float  pos[3];           // where it stands (MapEffRoomRegist places its effect there)
    /* 0x20 */ short  wind[2];          // SET_FURN_WIND: [0] strength 0..3, [1] direction 0..3 (MapObjSetWind)
};
static_assert(sizeof(FURN_SAVE_ROW) == 0x24, "FURN_SAVE_ROW is 0x24 bytes");
static_assert(offsetof(FURN_SAVE_ROW, vis) == 0x06, "FURN_SAVE_ROW.vis");
static_assert(offsetof(FURN_SAVE_ROW, look) == 0x0a, "FURN_SAVE_ROW.look");
static_assert(offsetof(FURN_SAVE_ROW, action_type) == 0x0b, "FURN_SAVE_ROW.action_type");
static_assert(offsetof(FURN_SAVE_ROW, req_action) == 0x0c, "FURN_SAVE_ROW.req_action");
static_assert(offsetof(FURN_SAVE_ROW, hit_check) == 0x0f, "FURN_SAVE_ROW.hit_check");
static_assert(offsetof(FURN_SAVE_ROW, alpha) == 0x10, "FURN_SAVE_ROW.alpha");
static_assert(offsetof(FURN_SAVE_ROW, alpha_cur) == 0x11, "FURN_SAVE_ROW.alpha_cur");
static_assert(offsetof(FURN_SAVE_ROW, label) == 0x12, "FURN_SAVE_ROW.label");
static_assert(offsetof(FURN_SAVE_ROW, pos) == 0x14, "FURN_SAVE_ROW.pos");
static_assert(offsetof(FURN_SAVE_ROW, wind) == 0x20, "FURN_SAVE_ROW.wind");

int   FurnSaveSaveGet(void **addr);     // FF3 0x001fa310: {the buffer, 0x3000} (save block 48)
int   FurnSaveLoad(void);               // FF3 0x001fa320: the default rows from the disc; 0
void *FurnSaveGetBuf(void);             // FF3 0x001fa348
FURN_SAVE_ROW *FurnSaveGetRow(int no);  // FF3 0x001fa350
int   FurnSaveGetNum(void);             // FF3 0x001fa380: rows in the buffer
int   FurnSaveGetKind(const char *name);// FF3 0x001fa398: FURN_SAVE_KIND_* of the name's letter, -1
FURN_SAVE_ROW *FurnSaveFind(int area, short kind, short id, int label);    // FF3 0x001fa3d8: area / label -1: any; nullptr if none
FURN_SAVE_ROW *FurnSaveFindId(int area, short kind, short id);             // FF3 0x001fa4d8: label -1
FURN_SAVE_ROW *FurnSaveFindName(int area, const char *name);               // FF3 0x001fa4f8: kind and id from the name
void  FurnSaveSetVisible(FURN_SAVE_ROW *row, u_int mask, u_char val);      // FF3 0x001fa580: vis[i] for mask bits 0..3
u_char FurnSaveGetVisible(FURN_SAVE_ROW *row, u_int mask);                 // FF3 0x001fa5c0: vis[] of the lowest mask bit; 0 for nullptr
int   FurnSaveGetVisMask(void);         // FF3 0x001fa618: 2 in the viewfinder, else 1
void  FurnSaveMain(void);               // FF3 0x001fa640: MapFrameUpdate (alpha fades)
int   FurnSaveAlloc(void);              // FF3 0x001fa6d0: IngameInit; 0
void  FurnSaveFree(void);               // FF3 0x001fa720: GameSystemEnd, MapReleaseAll

#endif /* _INGAME_MAP_FURNSAVE_H */
