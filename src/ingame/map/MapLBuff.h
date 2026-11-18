/* ==========================================================================
 *  ingame/map/MapLBuff.h
 *
 *  Map load buffers (FF2 MapLBuff.c). In FF3 the module keeps the hit vec
 *  table: 32 wall quads that the player is pushed out of and lines of sight
 *  are tested against -- the door leaves of a room's doors (MapLBuffLoadDoor,
 *  their quads from the door model) and the walls of event labels
 *  (MapLightRegOn, from RegDat vectors). Minimal so far: what reconstructed
 *  code calls. The functions on the table after MapLBuffInit 0x119790 are
 *  in ingame/map/MapLight.c (MapLBuff*, MapDoorVec*), the ones before
 *  MapLBuffRegist 0x119390 in ingame/map/MapHit.c. FF2's MapLBuff.c (load
 *  buffers per map) is gone; MapLBuffRegist / Load / Init keep the
 *  matcher's FF2 names but not FF2's bodies.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPLBUFF_H
#define _INGAME_MAP_MAPLBUFF_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

struct MAP_PUT;
struct MAP_OBJ_MDL;

// MAP_LBUFF.flags. The upper half is the door's DOOR_DAT id (its anime) for
// door vecs (MapLBuffLoadDoor: anime << 16).
#define MAP_LBUFF_HIDE      0x01        // not hit (MapDoorVecSetDisp 0; MapLBuffRegist hides the other side of the door being taken)
#define MAP_LBUFF_REG       0x02        // an event label's wall (MapLightRegOn); door lookups skip it
#define MAP_LBUFF_QUAD2     0x04        // quad2 is used too (the door's second leaf)
#define MAP_LBUFF_NO_LINE   0x08        // no line-of-sight test (door row _10 == 0; event flag 1)
#define MAP_LBUFF_LINE_HOLD 0x10        // line-of-sight test only with player flags 0x200000 (event flag 2)
#define MAP_LBUFF_NUM       32

// One hit vec: 0x130 bytes at bss 0x37fd40 (save block 51, 0x2600 bytes).
// quad / quad2 hold 4 corners in two banks; player flags2 bit 25 picks the
// bank the tests read (MapLBuffForEachQuad, MapLBuffWallPushOut,
// MapLBuffLineCheck); MapLBuffSetDoorQuad writes both (bank 0 a leaf 6
// units thick, bank 1 20).
// layout: host -- put is an EE pointer (4 bytes); the offsets up to it
// match the ROM (asserted), the host struct is longer.
struct MAP_LBUFF                        /* 0x130 on the EE */
{
    /* 0x000 */ int            buff;        // room buffer 0 / 1, -1 free
    /* 0x004 */ u_int          flags;       // MAP_LBUFF_*
    /* 0x008 */ int            id;          // door id (door_stat row) or event label
    /* 0x00c */ int            no;          // door number (the put's name digits) or event label
    /* 0x010 */ int            label;       // the RegDat file's label (MB_OUT_HEAD.kai): the tests' filter
    /* 0x014 */ int            _014[3];
    /* 0x020 */ sceVu0FVECTOR  quad[2][4];  // [bank][corner]
    /* 0x0a0 */ sceVu0FVECTOR  quad2[2][4]; // MAP_LBUFF_QUAD2
    /* 0x120 */ struct MAP_PUT *put;        // the door's put (nullptr for event walls)
    /* 0x124 */ int            _124[3];
};
static_assert(offsetof(MAP_LBUFF, flags) == 0x004, "MAP_LBUFF.flags");
static_assert(offsetof(MAP_LBUFF, id) == 0x008, "MAP_LBUFF.id");
static_assert(offsetof(MAP_LBUFF, no) == 0x00c, "MAP_LBUFF.no");
static_assert(offsetof(MAP_LBUFF, label) == 0x010, "MAP_LBUFF.label");
static_assert(offsetof(MAP_LBUFF, quad) == 0x020, "MAP_LBUFF.quad");
static_assert(offsetof(MAP_LBUFF, quad2) == 0x0a0, "MAP_LBUFF.quad2");
static_assert(offsetof(MAP_LBUFF, put) == 0x120, "MAP_LBUFF.put");

extern MAP_LBUFF  map_lbuff[MAP_LBUFF_NUM];     // bss 0x37fd40
// The current door vec (sbss gp-0x7cc8, set by MapDoorVecSelect in
// MapLight.c): the door the player is taking.
extern MAP_LBUFF *map_door_vec;

void MapLBuffInit(void);                // FF3 0x00119790: every entry free
// FF3 0x00119390: mask set (on 1) or cleared (on 0) in the flags of the
// door vecs with door number no but another id (the other side of a door).
int  MapLBuffRegist(int id, int no, u_int mask, int on);
// FF3 0x00119680: a free entry for buffer buff with these fields, hidden
// when the current door vec is the same door at another put; nullptr when full.
MAP_LBUFF *MapLBuffLoad(int buff, int id, int no, struct MAP_PUT *put, int label, u_int flags);
// FF3 0x00119720: MapLBuffLoad with the 4 corners vec (a RegDat vector
// record's) in both banks of quad: an event label's wall. 0, -1 when full.
int  MapLBuffLoadVec(int buff, int id, int no, struct MAP_PUT *put, int label,
                     const float (*vec)[4], u_int flags);
// FF3 0x00119540: the 4 corners of a door leaf of kind kind (2: hinged on
// the other side, x -18..0) through mtx into both banks of quad.
void MapLBuffSetDoorQuad(sceVu0FVECTOR (*quad)[4], int kind, const sceVu0FMATRIX mtx);
// FF3 0x00119458: 1 when one of the 32 door vecs (MapHitRegistDoorVec) has
// the door number of put name name (the other room loaded the door).
int  MapLBuffIsDoorLoaded(const char *name);

struct MAP_OBJ_MDL *MapDoorVecGetMot(void);     // FF3 0x001194d0: its MAP_PUT's model (MapPutGetMdl)
struct MAP_PUT *MapDoorVecGetPut(void);         // FF3 0x001194f0: its MAP_PUT (+0x120)
void            MapDoorVecSetDisp(int disp);    // FF3 0x00119500: 0 hides it (flag bit 0), 1 shows it

#endif /* _INGAME_MAP_MAPLBUFF_H */
