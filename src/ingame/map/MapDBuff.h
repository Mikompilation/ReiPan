/* ==========================================================================
 *  ingame/map/MapDBuff.h
 *
 *  CMapDBuff (FF3 RTTI, vtable 0x341028): a door model that moves on a
 *  MAP_PUT with a second model (file D000_PK4 + no) drawn in turn with the
 *  put's own one (init_Story_Door_Open creates it for an unlocked door).
 *  Then the special-event put pair (SET_SP_EVENT) that shares the file
 *  (FF3 0x0010dbe8..0x0010e230; FF3-only, no FF2 file).
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPDBUFF_H
#define _INGAME_MAP_MAPDBUFF_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#include "common/Load.h"                // CLoad
#include "ingame/map/MapObjWrk.h"       // MAP_OBJ_MDL

struct MAP_PUT;                         // ingame/map/MapPut.h

#define MAP_DBUFF_IDLE    0
#define MAP_DBUFF_LOADING 1             // CMapDBuff::LoadReq; CMapDBuff::IsReady builds the model when the file is in
#define MAP_DBUFF_READY   2

// 0x44 bytes on the EE.
// layout: host -- the vptr is 4 bytes on the EE and the model pointers,
// the matrix and the put are EE pointers; EE offsets in the comments.
class CMapDBuff
{
public:
    CMapDBuff();                        // FF3 0x0010dc78 MapDBuffCtor (twin 0x0010dc20)
    virtual ~CMapDBuff();               // EE vtable +0x00 0x0010dd10 (base 0x0010dcd0), deleting +0x04 0x0010dd50

    void Set(MAP_PUT *put, int anime);  // FF3 0x0010dd90: onto put, anime started on both models (anime < 0: nothing)
    void PutChange();                   // FF3 0x0010de98: the put draws its own model again
    void LoadReq(int no);               // FF3 0x0010deb0: file D000_PK4 + no
    int IsReady();                      // FF3 0x0010dee8: 0 while the file loads (builds the model when it is in)
    void ReqMdl(float *matrix, int i);  // FF3 0x0010df70
    void DrawMdl(int i);                // FF3 0x0010dff8
    void SetMatrix(float *matrix);      // FF3 0x0010e020
    void Draw(int step);                // FF3 0x0010e028

    int          state;                 // EE +0x04: MAP_DBUFF_*
    MAP_OBJ_MDL *mdl[2];                // EE +0x08: [0] the put's model, [1] &own
    MAP_OBJ_MDL  own;                   // EE +0x10: built from load's file
    int          cur;                   // EE +0x20: the model drawn next (flips each stepped frame)
    CLoad        load;                  // EE +0x24: D000_PK4 + no
    float       *matrix;                // EE +0x3c: the put's matrix
    MAP_PUT     *put;                   // EE +0x40
};
static_assert(offsetof(CMapDBuff, mdl) >= offsetof(CMapDBuff, state) + sizeof(int), "CMapDBuff.mdl follows state (padded on 64-bit hosts)");
static_assert(offsetof(CMapDBuff, own) == offsetof(CMapDBuff, mdl) + 2 * sizeof(MAP_OBJ_MDL *), "CMapDBuff.own follows mdl");
static_assert(offsetof(CMapDBuff, cur) == offsetof(CMapDBuff, own) + sizeof(MAP_OBJ_MDL), "CMapDBuff.cur follows own");

// The special event (dream player block word 1, SET_SP_EVENT 0): two puts
// MapDraw registers, swapped in area 0x21 when the event runs.
void MapSpEventPutAppear(MAP_PUT *put); // FF3 0x0010e120: hidden until the event
void MapSpEventPutVanish(MAP_PUT *put); // FF3 0x0010e168: hidden after the event
void MapSpEventSet(void);               // FF3 0x0010e1b8: SET_SP_EVENT 0

#endif /* _INGAME_MAP_MAPDBUFF_H */
