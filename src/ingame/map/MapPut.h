/* ==========================================================================
 *  ingame/map/MapPut.h
 *
 *  Placed map objects (FF2 MapPut.c, FF3 0x11bf28..0x11cea8): a pool of 150
 *  puts (bss 0x389260), a list of the used ones (sbss gp-0x92c) and the draw
 *  list MapPutSort builds every frame (culling, depth, first / last keys).
 *  A put draws a model (MAP_OBJ_MDL) or calls a callback.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPPUT_H
#define _INGAME_MAP_MAPPUT_H

#include <stddef.h>                     // offsetof
#include <libvu0.h>                     // sceVu0FVECTOR / sceVu0FMATRIX
#include "sdk/scetypes.h"

struct MAP_PUT;
struct MAP_OBJ_MDL;                     // ingame/map/MapObjWrk.h
struct GRA3DLIGHTDATA;                  // graphics/graph3d/gra3d.h

// A put drawn by a callback (MAP_PUT.func) gets MapPutDraw's flags.
typedef void (*MAP_PUT_DRAW_FUNC)(MAP_PUT *put, u_int flags);

// MAP_PUT.flg. FF2's bits where FF3 still uses them; MapPutSort and
// MapPutSetLight look at the "kind" bits (MAP_PUT_KIND_MASK) as one value.
enum
{
    MAP_PUT_FUNC_OBJECT  = 0x00001,     // func instead of mdl
    MAP_PUT_NODRAW       = 0x00008,     // hidden (MapPutSetFlg 8: the room is hidden)
    MAP_PUT_MATRIX_PTR   = 0x00010,     // mp instead of mat (no matrix built)
    MAP_PUT_PLAYER_LIGHT = 0x00020,     // kind: only the player light
    MAP_PUT_FIRST        = 0x00400,     // drawn before ordinary puts; first_id orders them
    MAP_PUT_REAL_LIGHT   = 0x00800,     // kind: the room light emulated at the put; culled by its box
    MAP_PUT_PRELIGHT     = 0x01000,     // kind: its light data set, then the player light only
    MAP_PUT_POS_PTR      = 0x02000,     // pos points at its position (MapPutSetFuncObj)
    MAP_PUT_SWAY         = 0x04000,     // kind: swaying furniture, real light, far ones skipped
    MAP_PUT_LAST         = 0x08000,     // drawn after ordinary puts; first_id orders them
    MAP_PUT_GS           = 0x10000,     // passed on to MapObjMdlDraw
    MAP_PUT_ROOM_LIGHT   = 0x20000,     // kind: the room's registered light (MapDrawGetLightPtr4BuffID(buff, 1): bank 1)
    MAP_PUT_REQ_MDL      = 0x40000,     // kind: once, ModelManager::ExecPrelightMatrix with its light, then the flag and lit cleared
    MAP_PUT_KIND_MASK    = 0x65820,
};

// One put (FF2 MAPPUT_OBJ, 0x90 on the EE; FF3 added drawn / visible and
// pos, and dropped FF2's model address for mdl at the end). Pool bss
// 0x389260, MapPutResetAll clears 0x5460 = 150 x 0x90.
// layout: host -- func, work, mp, pos, lit, mdl and the links are EE
// pointers (4 bytes); EE offsets in the comments.
struct MAP_PUT                          /* 0x90 on the EE */
{
    /* 0x00 */ short             buff;      // the room buffer / effect group, -1 free
    /* 0x02 */ short             first_id;  // order among MAP_PUT_FIRST / LAST puts (bigger first)
    /* 0x04 */ u_int             flg;       // MAP_PUT_*
    /* 0x08 */ float             z_num;     // -(screen z + z_offset), MapPutSort
    /* 0x0c */ float             z_offset;  // MapPutSetZoffset, MapPutSetDrawFunc
    /* 0x10 */ int               drawn;     // its model's matrix was set (MapPutDrawFlagClear)
    /* 0x14 */ int               visible;   // MapPutSort: in view, light it and draw it
    /* 0x18 */ MAP_PUT_DRAW_FUNC func;      // MAP_PUT_FUNC_OBJECT
    /* 0x1c */ void             *work;      // the owner's (MapPutSetWork)
    /* 0x20 */ sceVu0FMATRIX     mat;       // world matrix: scale, rot x, y, z, then pos
    /* 0x60 */ float            *mp;        // MAP_PUT_MATRIX_PTR: a 4x4 matrix
    /* 0x64 */ float            *pos;       // MAP_PUT_POS_PTR: its position
    /* 0x68 */ GRA3DLIGHTDATA   *lit;       // its light data (MapPutSetLitPtr)
    /* 0x70 */ sceVu0FVECTOR     c_pos;     // bounding-box centre in the world
    /* 0x80 */ MAP_OBJ_MDL      *mdl;       // the model drawn
    /* 0x84 */ MAP_PUT          *next;      // put list
    /* 0x88 */ MAP_PUT          *befo;
    /* 0x8c */ MAP_PUT          *next_draw; // draw list (MapPutSort)
};
static_assert(offsetof(MAP_PUT, first_id) == 0x02, "MAP_PUT.first_id");
static_assert(offsetof(MAP_PUT, flg) == 0x04, "MAP_PUT.flg");
static_assert(offsetof(MAP_PUT, z_num) == 0x08, "MAP_PUT.z_num");
static_assert(offsetof(MAP_PUT, z_offset) == 0x0c, "MAP_PUT.z_offset");
static_assert(offsetof(MAP_PUT, drawn) == 0x10, "MAP_PUT.drawn");
static_assert(offsetof(MAP_PUT, visible) == 0x14, "MAP_PUT.visible");
static_assert(offsetof(MAP_PUT, func) == 0x18, "MAP_PUT.func");
static_assert(offsetof(MAP_PUT, mat) % 16 == 0 && offsetof(MAP_PUT, c_pos) % 16 == 0,
              "MAP_PUT.mat / c_pos are quadwords (EE +0x20 / +0x70)");
static_assert(offsetof(MAP_PUT, mp) == offsetof(MAP_PUT, mat) + sizeof(sceVu0FMATRIX),
              "MAP_PUT.mp follows mat (EE +0x60)");
static_assert(offsetof(MAP_PUT, next_draw) == offsetof(MAP_PUT, mdl) + 3 * sizeof(void *),
              "MAP_PUT.next / befo / next_draw follow mdl (EE +0x84..+0x8c)");

#define MAP_PUT_MAX 150

// Pool and lists.
void     MapPutResetAll(void);                  // FF3 0x0011ce38: MapPlayStartReset
MAP_PUT *MapPutSetObj(short room, MAP_OBJ_MDL *mdl, float *pos, float *rot, float *scale,
                      void *lit, u_int flags);  // FF3 0x0011c360: nullptr when the pool is full
MAP_PUT *MapPutSetFunc(int buff, MAP_PUT_DRAW_FUNC func, u_int flags);  // FF3 0x0011c520: callback put on a matrix pointer (flags | 0x11)
MAP_PUT *MapPutSetFuncPos(int buff, MAP_PUT_DRAW_FUNC func, float *pos, float *rot, float *scale,
                          u_int flags);         // FF3 0x0011c558: callback put with its own matrix (no callers)
MAP_PUT *MapPutSetFuncObj(int buff, MAP_PUT_DRAW_FUNC func, float *pos, u_int flags);   // FF3 0x0011c578: flags | 0x2011, pos kept as a pointer
void     MpaPutDeleteOneObj(MAP_PUT *put);      // FF3 0x0011c670
void     MapPutDelete(short buff);              // FF3 0x0011c688: every put of buff
void     MapPutSetFlg(int room, u_int flg);     // FF3 0x0011bfe0: flg into the room's puts (8: hidden)

// Fields.
void     MapPutSetFirst(MAP_PUT *put, short first_id);  // FF3 0x0011c058
void     MapPutSetZoffset(float dist, MAP_PUT *put);    // FF3 0x0011c070 (3000 / 200 for the map effects)
void     MapPutSetMatrixPtr(MAP_PUT *put, float *mtx);  // FF3 0x0011c0b0
void     MapPutSetLitPtr(MAP_PUT *put, void *lit);      // FF3 0x0011c0d0: GRA3DLIGHTDATA
void     MapPutSetWork(MAP_PUT *put, void *work);       // FF3 0x0011c0e0
float   *MapPutGetMatrix(MAP_PUT *put);         // FF3 0x0011c110: 4x4, row 3 the position; nullptr for nullptr
u_int   *MapPutGetFlag(MAP_PUT *put);           // FF3 0x0011c120: nullptr for nullptr
void    *MapPutGetLitPtr(MAP_PUT *put);         // FF3 0x0011c130: nullptr for nullptr
MAP_OBJ_MDL *MapPutGetMdl(MAP_PUT *put);        // FF3 0x0011c148: nullptr for nullptr
void    *MapPutGetWork(MAP_PUT *put);           // FF3 0x0011c160: nullptr for nullptr
int      MapPutGetPos(void *out, MAP_PUT *put); // FF3 0x0011c180: out = its position (4 floats); 0
void     MapPutSetDrawFunc(MAP_PUT *put, MAP_PUT_DRAW_FUNC func, u_int flags, int dist, short first_id);  // FF3 0x0011c5d8: flg |= 1 | flags, z_offset += dist
void     MapPutChangeObj(MAP_PUT *put);         // FF3 0x0011c5c0: no callback, draws its model again
MAP_PUT *MapPutGetNowHdl(void);                 // FF3 0x0011ca68: the put MapPutDraw is drawing

// Drawing.
void     MapPutDrawFlagClear(void);             // FF3 0x0011ca30: every put's drawn flag
void     MapPutDraw(u_int flags);               // FF3 0x0011cd00: flags 8: matrices only
void     MapPutSetDrawStop(int on);             // FF3 0x0011bfd8: MapPutDraw draws nothing while on (player model change)
int      MapPutGetDrawStop(void);               // FF3 0x0011bfd0 (no callers)
void     MapPutSetSceneLoad(int on);            // FF3 0x0011bf28: 1 around SceneInitAfterModelLoad's room set-up; never read
void     MapPutClearSceneLoad(void);            // FF3 0x0011bf30 (no callers)
void     MapPutNop(void);                       // FF3 0x0011ccf8: empty (no callers)

#endif /* _INGAME_MAP_MAPPUT_H */
