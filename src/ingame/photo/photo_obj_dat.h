/* ==========================================================================
 *  ingame/photo/photo_obj_dat.h
 *
 *  The photo-object table (FF3-only, FF3 0x22a930..0x22ada0; file name
 *  invented). 66 records at data 0x32e050 name the things the camera can
 *  photograph besides ghosts: their position, room area, finder range and
 *  mode bits, some carried by an enemy (types 4 / 5). Two bit sets of 96
 *  bits follow which have been photographed (0x3f7770) and which may be
 *  photographed now (0x3f777c, SET_PHOTOABLE_); both are save block 29.
 *  photo_obj.c draws their glow and lights the finder's filament.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_OBJ_DAT_H
#define _INGAME_PHOTO_PHOTO_OBJ_DAT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define PHOTO_OBJ_NUM       66          // records 0..65 (loops run 0 / 1 .. 0x41)

// The 2-bit modes in PHOTO_OBJ_DAT.bits (PhotoObjModeIsOn): 1 always on,
// 2 only while the viewfinder is up (PLYR_FLG_800000), 0 / 3 off.
#define PHOTO_OBJ_MODE(bits, shift) (((bits) >> (shift)) & 3)
#define PHOTO_OBJ_SH_16     16          // not read by photo_obj.c (1 in every record that has bits)
#define PHOTO_OBJ_SH_FINDER 18          // the finder test (target, filament hint, alpha fade)
#define PHOTO_OBJ_SH_GLOW   20          // the glow effect's target rate (0 for type 3)
#define PHOTO_OBJ_SH_NEAR   22          // nearest-object search, photoSe's hum
#define PHOTO_OBJ_BITS_SCORE 0xffff     // low half: 1000 / 2000 (type 3), 0 (not read here)

#define PHOTO_OBJ_FLG_ONCE    0x10000   // flags: photographed once -> inactive (glow slot cut)
#define PHOTO_OBJ_FLG_RESET   0x20000   // flags: PhotoObjResetFlg clears its photographed bit

#define PHOTO_OBJ_F34_LINE    0x2       // f34: the glow needs a clear line from the eye (PlyrAreaLineHitCheck)

// One photo object (data 0x32e050 + no * 0x40). Type: 1 / 2 / 6 hum with SE
// 0x1a9b0008, 3 with 0x1a9b000a (the 33 records 33..65, score 1000 / 2000,
// range 2000), 4 / 5 ride on an enemy (PhotoObjCheckEne: type 4 kind 2,
// type 5 kind 0; their position is 0). Fields +0x24.. are not read by
// photo_obj.c / photo_obj_dat.c; their values are noted.
struct PHOTO_OBJ_DAT                    /* 0x40 */
{
    /* 0x00 */ short no;                // its own index
    /* 0x02 */ short type;              // 1..5 (photoSeHdl::Play also knows 6)
    /* 0x04 */ float pos[3];            // world position (0 for types 4 / 5)
    /* 0x10 */ short area;              // room area (PhotoObjAreaSet); -1 for types 4 / 5
    /* 0x12 */ short ene_id;            // types 4 / 5: the enemy id (EneGetWrk)
    /* 0x14 */ short ene_cond;          // types 4 / 5: 1 the enemy flags 0x1800, 2 hp used up
    /* 0x16 */ short _16;               // 0
    /* 0x18 */ float range;             // PlyrFinderTargetCheck's range (40 .. 2000)
    /* 0x1c */ u_int bits;              // PHOTO_OBJ_SH_* modes, PHOTO_OBJ_BITS_SCORE
    /* 0x20 */ u_int flags;             // PHOTO_OBJ_FLG_*; bit 0 set in most; the low half picks the hint draw flags (PhotoHintDraw: 0 -> 2, 1 -> 3)
    /* 0x24 */ short _24[2];            // {4, 4}, {0x73, n}: the hint message (Anim2dMsgSet(_24[0], _24[1]) in photo step 9; _24[0] < 0: none)
    /* 0x28 */ int   _28;               // 0x6d3.. / 0x947..0x967: the hint texture file (photo step 0 reads it as a short; < 0: none)
    /* 0x2c */ int   _2c;               // -1, 0xfc3 / 0xfed / 0x101d: the hint stream file (+1 its header; photo step 7)
    /* 0x30 */ int   _30;               // -1 or 0..0x13: a film developed at the end of a photo (FUN_001b3130, photo step 14)
    /* 0x34 */ int   f34;               // PHOTO_OBJ_F34_LINE; values 0 / 1 / 2
    /* 0x38 */ int   _38;               // -1 or 0x6c..0x9f: a document got by the photo (DocFlagSet, photo step 14)
    /* 0x3c */ int   _3c;               // -1, or 0..0x20 for records 33..65: its soul-list entry (soul_list.c), marked had after a photo (SoulListSetHave)
};
static_assert(sizeof(PHOTO_OBJ_DAT) == 0x40, "PHOTO_OBJ_DAT: PhotoObjGetDat stride");
static_assert(offsetof(PHOTO_OBJ_DAT, type) == 0x02, "PHOTO_OBJ_DAT.type");
static_assert(offsetof(PHOTO_OBJ_DAT, pos) == 0x04, "PHOTO_OBJ_DAT.pos");
static_assert(offsetof(PHOTO_OBJ_DAT, area) == 0x10, "PHOTO_OBJ_DAT.area");
static_assert(offsetof(PHOTO_OBJ_DAT, ene_id) == 0x12, "PHOTO_OBJ_DAT.ene_id");
static_assert(offsetof(PHOTO_OBJ_DAT, ene_cond) == 0x14, "PHOTO_OBJ_DAT.ene_cond");
static_assert(offsetof(PHOTO_OBJ_DAT, range) == 0x18, "PHOTO_OBJ_DAT.range");
static_assert(offsetof(PHOTO_OBJ_DAT, bits) == 0x1c, "PHOTO_OBJ_DAT.bits");
static_assert(offsetof(PHOTO_OBJ_DAT, flags) == 0x20, "PHOTO_OBJ_DAT.flags");
static_assert(offsetof(PHOTO_OBJ_DAT, f34) == 0x34, "PHOTO_OBJ_DAT.f34");
static_assert(offsetof(PHOTO_OBJ_DAT, _3c) == 0x3c, "PHOTO_OBJ_DAT._3c");

// A 96-bit set (bit no in word no >> 5). FF3 constructs the second one with
// FF3 0x002be100 (a shared template instance: three words cleared) and
// clears the first inline.
struct PHOTO_OBJ_BITS                   /* 0xc */
{
    /* 0x0 */ u_int w[3];
};
static_assert(sizeof(PHOTO_OBJ_BITS) == 0xc, "PHOTO_OBJ_BITS is three words");

// Save block 29 (0x18): [0] photographed (bss 0x3f7770), [1] photoable (0x3f777c).
struct PHOTO_OBJ_FLG                    /* 0x18 */
{
    /* 0x00 */ PHOTO_OBJ_BITS photographed;
    /* 0x0c */ PHOTO_OBJ_BITS photoable;
};
static_assert(sizeof(PHOTO_OBJ_FLG) == 0x18, "PHOTO_OBJ_FLG: PhotoObjFlgSaveGet size");
static_assert(offsetof(PHOTO_OBJ_FLG, photoable) == 0xc, "PHOTO_OBJ_FLG.photoable");

PHOTO_OBJ_DAT *PhotoObjGetDat(int no);              // FF3 0x0022a930: record no (never null)
void   PhotoObjResetFlg(void);                      // FF3 0x0022a948: unreferenced
int    PhotoObjFlgSaveGet(void **addr);             // FF3 0x0022a9d0: save block 29 {0x3f7770, 0x18}
void   PhotoObjSetPhotographed(int no);             // FF3 0x0022a9e8: photo_dat.c 0x2282e0
u_int  PhotoObjIsPhotographed(u_int no);            // FF3 0x0022aa58: event conditions
void   PhotoObjSetPhotoable(int no);                // FF3 0x0022aab0: SET_PHOTOABLE_ on
u_int  PhotoObjIsPhotoable(u_int no);               // FF3 0x0022aae0
void   PhotoObjClrPhotoable(int no);                // FF3 0x0022ab08: SET_PHOTOABLE_ off
PHOTO_OBJ_DAT *PhotoObjGetEneSubject(void);         // FF3 0x0022ab38: PlyrFinderShot; null none
int    PhotoObjClrEneSubject(void);                 // FF3 0x0022ab40: returns 0
int    PhotoObjCheckEne(int kind, short id);        // FF3 0x0022ab50: 1 latches the subject
void   PhotoObjDatInit(void);                       // FF3 0x0022aca0: PhotoDatInit

#endif /* _INGAME_PHOTO_PHOTO_OBJ_DAT_H */
