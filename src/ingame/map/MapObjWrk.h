/* ==========================================================================
 *  ingame/map/MapObjWrk.h
 *
 *  The map object works (FF3-only, 0x1f8af8..0x1f9b48): 128 works (bss
 *  0x3f1a38, 0x2c bytes each) for the doors ("d012") and furniture
 *  ("f034") of the two room buffers. MapObjRegist builds each one's model
 *  and puts it in the put list; the works then carry the FurnSave row's
 *  state over to the model: visibility (put flag 8), alpha, motion requests
 *  and wind sway, and the furniture the player may look at.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPOBJWRK_H
#define _INGAME_MAP_MAPOBJWRK_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/map/MapObjMdl.h"     // MAP_OBJ_MDL

struct MAP_PUT;                         // ingame/map/MapPut.h
struct FURN_SAVE_ROW;                   // ingame/map/FurnSave.h

#define MAP_OBJ_WRK_NUM 128

#define MAP_OBJ_TYPE_DOOR 0             // "d..."
#define MAP_OBJ_TYPE_FURN 1             // "f..."

#define MAP_OBJ_FLG_LOOP 0x1            // the row's motion restarts when it ends (action type 3)

// layout: host -- put, row and the handle are EE pointers; EE offsets in the
// comments (0x2c bytes per work there).
struct MAP_OBJ_WRK                      /* 0x2c on the EE */
{
    /* 0x00 */ int            list;     // its room buffer (0 / 1), -1 free
    /* 0x04 */ int            area;     // MapLoadGetRoomArea(list)
    /* 0x08 */ int            no;       // the name's three digits
    /* 0x0c */ int            type;     // MAP_OBJ_TYPE_*
    /* 0x10 */ MAP_PUT       *put;
    /* 0x14 */ u_int          flag;     // MAP_OBJ_FLG_*
    /* 0x18 */ FURN_SAVE_ROW *row;      // its save row, nullptr if none
    /* 0x1c */ MAP_OBJ_MDL    mdl;
};
static_assert(offsetof(MAP_OBJ_WRK, type) == 0x0c, "MAP_OBJ_WRK.type");
static_assert(offsetof(MAP_OBJ_WRK, put) == 0x10, "MAP_OBJ_WRK.put");
static_assert(offsetof(MAP_OBJ_WRK, mdl) == offsetof(MAP_OBJ_WRK, row) + sizeof(FURN_SAVE_ROW *),
              "MAP_OBJ_WRK.mdl follows row (EE +0x1c)");

int          MapObjGetType(const char *name);                       // FF3 0x001f8af8: MAP_OBJ_TYPE_* of the name's letter, -1
MAP_OBJ_WRK *MapObjFindWrk(int area, int no, int type);             // FF3 0x001f8b20
MAP_OBJ_MDL *MapObjGetMdlHide(int room, const char *name);          // FF3 0x001f8b78 (no callers)
int          MapObjPutCheck(void *out, int area, const char *name); // FF3 0x001f8c38: 0, or -1 / -4 / -2 / -3
float       *MapObjGetMatrix(int area, const char *name);           // FF3 0x001f8cd8: the object's world matrix, nullptr
int          MapObjGetNoByPut(MAP_PUT *put);                        // FF3 0x001f8d68: a furniture's number, -1
void         MapObjMain(void);                                      // FF3 0x001f8db8: MapFrameUpdate
void         MapObjSetAction(FURN_SAVE_ROW *row);                   // FF3 0x001f8e68: SET_OBJ_ACTIONTYPE
void         MapObjSetActionRoom(int room);                         // FF3 0x001f8f48
MAP_OBJ_WRK *MapObjFindWrkByRow(FURN_SAVE_ROW *row);                // FF3 0x001f9080: nullptr if no work shows it
MAP_PUT     *MapObjRegist(int room, const char *name, float *pos, float *rot, float *scale,
                          void *mot_pk4);                           // FF3 0x001f90c0: the put object, nullptr
void         MapObjDoorMdlRebuildAll(void);                         // FF3 0x001f92c8: EvPlyrMdlChangeWait
void         MapObjReqMdlRoom(int room, int keep);                  // FF3 0x001f9328
void         MapObjReleaseRoom(int room);                           // FF3 0x001f9428: MapDrawDeleteNoDraw
void         MapObjSetVisible(int room, short kind, int no, u_int mask, u_char val);   // FF3 0x001f9488: kind / no -1 any
void         MapObjUpdateRoom(int room, int vis_mask);              // FF3 0x001f9550
void         MapObjUpdate(int vis_mask);                            // FF3 0x001f9688: -1 FurnSaveGetVisMask
void         MapObjInit(void);                                      // FF3 0x001f96d0: MapPlayStartReset
void         MapObjReleaseAll(void);                                // FF3 0x001f9730: GameSystemEnd, MapReleaseAll
int          MapObjLookSearch(void);                                // FF3 0x001f9750: 1 when look 9 takes a target
int          MapObjSetWind(MAP_OBJ_WRK *wrk);                       // FF3 0x001f9948
int          MapObjSetWindCloth(MAP_OBJ_WRK *wrk);                  // FF3 0x001f99e0
void         MapObjSetHide(int room, int type, int no, int show);   // FF3 0x001f9a50: -1 any; show 1 shows

#endif /* _INGAME_MAP_MAPOBJWRK_H */
