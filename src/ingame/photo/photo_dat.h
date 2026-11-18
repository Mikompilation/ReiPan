/* ==========================================================================
 *  ingame/photo/photo_dat.h
 *
 *  The photo sequence and its phase (FF3 0x2274e0..0x228718; the file name
 *  is FF2's link-order slot, the content is FF2 photo.c's phase half): after
 *  a shot PlyrFinderShot builds a PHOTO_WRK_DEF, hands it to PhotoSeqStart
 *  (PlyrShotRecord) and requests phase 33, Story_Photo. Its one runs one step
 *  of the sequence a frame (PhotoMain, step table 0x35fb20 picked by
 *  photo_wrk.mode): white flash and frozen frame, the hint picture of each
 *  photographed subject (texture, SE / stream, message), the seal-door burn
 *  (unlock_ghost), the frame encoded into the album slot, and the end, which
 *  returns to the phase that requested 33. FF2 runs the same beats as a
 *  12-step switch (photo.c PhotoMain); FF3 numbers them 0..14 in a table.
 *  The drawing (frame, hint picture) is photo_make.c; the encode is the
 *  unassigned run 0x228718..0x2289a8, still stubs.
 *  Also PhotoDatInit (the photo tables at a new game).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_DAT_H
#define _INGAME_PHOTO_PHOTO_DAT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/vu0_intrin.h"             // sceVu0FVECTOR

struct PHOTO_OBJ_DAT;                   // ingame/photo/photo_obj_dat.h

#define PHOTO_HINT_MAX      5           // hint_pict[] (FF2 HINT_PHOTO_REQ_MAX)

// What a shot hands to the sequence (FF2 PHOTO_WRK_DEF, same 0x40 layout).
// Built on PlyrFinderShot's stack by PHOTO_WRK_DEF::Init (all 0, msg_name /
// msg_type -1) and PlyrShotRecord (type 1, adr_no from GetSavePhotoNo).
struct PHOTO_WRK_DEF                    /* 0x40 */
{
    // Written in ingame/plyr/plyr_shot.c (the shot code builds the def).
    void Init();                        // FF3 0x0023c690 (PhotoWrkDefInit): all 0, msg_name / msg_type -1
    int  GetSpecial();                  // FF3 0x0023c688 (PhotoWrkDefGetSpecial): -1 always
    void AddHint(int no);               // FF3 0x0023c6d8 (PhotoWrkDefAddHint): hint_pict[hint_cnt++] = no, unchecked

    /* 0x00 */ sceVu0FVECTOR pos;     // FF2: the rare ghost's position; unread in FF3's sequence
    /* 0x10 */ int   hint_pict[PHOTO_HINT_MAX]; // PHOTO_OBJ_DAT numbers of the subjects
    /* 0x24 */ int   hint_cnt;
    /* 0x28 */ int   adr_no;            // album photo slot: FUN_00228968 encodes into it
    /* 0x2c */ int   type;              // PHOTO_TYPE_*
    /* 0x30 */ int   msg_name;          // -1 default; with msg_type >= 0 PhotoZbufRefresh runs
    /* 0x34 */ int   msg_type;          // -1 default
    /* 0x38 */ int   unlock_ghost;      // non-zero: the seal-door burn steps 11 / 12 run
    /* 0x3c */ int   pad;
};
static_assert(sizeof(PHOTO_WRK_DEF) == 0x40, "PHOTO_WRK_DEF: PHOTO_WRK_DEF::Init clears 0x40");
static_assert(offsetof(PHOTO_WRK_DEF, hint_pict) == 0x10, "PHOTO_WRK_DEF.hint_pict");
static_assert(offsetof(PHOTO_WRK_DEF, hint_cnt) == 0x24, "PHOTO_WRK_DEF.hint_cnt");
static_assert(offsetof(PHOTO_WRK_DEF, adr_no) == 0x28, "PHOTO_WRK_DEF.adr_no");
static_assert(offsetof(PHOTO_WRK_DEF, type) == 0x2c, "PHOTO_WRK_DEF.type");
static_assert(offsetof(PHOTO_WRK_DEF, msg_name) == 0x30, "PHOTO_WRK_DEF.msg_name");
static_assert(offsetof(PHOTO_WRK_DEF, msg_type) == 0x34, "PHOTO_WRK_DEF.msg_type");
static_assert(offsetof(PHOTO_WRK_DEF, unlock_ghost) == 0x38, "PHOTO_WRK_DEF.unlock_ghost");

// PHOTO_WRK_DEF.type as PhotoSeqStart reads it. FF2's enum had HINT 0,
// HINT3D 1, RARE 2, MAYU_CURSE 3; FF3 tests only 2 and 3 here.
#define PHOTO_TYPE_FURN     2           // sta |= PHOTO_STA_FURN (FF2 set it for HINT3D)
#define PHOTO_TYPE_SPECIAL  3           // bits |= PHOTO_BIT_SPECIAL: the frame drawn in style 3

// photo_wrk.sta (FF2's names where FF2 has the bit).
#define PHOTO_STA_HINT_LOAD 0x02        // a hint texture was requested (step 0) until step 7 plays its SE
#define PHOTO_STA_FURN      0x10        // a scenery shot: no flash of its own, no hint picture

// photo_wrk.bits (byte +0x09: bits 8..15 of the EE's 64-bit word at +0x08).
#define PHOTO_BIT_FRAME     0x01        // FUN_002275d0: set by step 1 for a scenery shot, cleared by step 14; no reader found
#define PHOTO_BIT_02        0x02        // set by PhotoSeqStart; no reader found
#define PHOTO_BIT_END       0x04        // set by step 14; photo_datObjSealGhostDrawLock clears it; no reader found
#define PHOTO_BIT_SPECIAL   0x08        // PHOTO_TYPE_SPECIAL: frame style 3, no hint message (step 9 -> 13)
#define PHOTO_BIT_SUBJECT   0x10        // step 0 took a subject: its hint picture is drawn

// The sequence work (bss 0x3f6670). FF3 addresses it from one base
// register, so it is one object; only this file touches it.
// layout: host -- cur and hint_tex are pointers (EE 4 bytes at +0x50 /
// +0x58); the offsets before hint_tex match the ROM.
struct PHOTO_WRK                        /* 0x6c on the EE */
{
    /* 0x00 */ int           mode;      // step, index into photo_step_tbl
    /* 0x04 */ u_int         sta;       // PHOTO_STA_*
    /* 0x08 */ s_char        cnt;       // frame counter of the step
    /* 0x09 */ u_char        bits;      // PHOTO_BIT_*
    /* 0x0a */ u_char        _0a[6];    // rest of the EE's 64-bit word; never set
    /* 0x10 */ PHOTO_WRK_DEF def;       // copy of the shot's definition (PhotoSeqStart)
    /* 0x50 */ void          *hint_tex; // the hint texture on the system heap (PhotoHintTexLoad), or null
    /* 0x54 */ int           _54;       // not written here
    /* 0x58 */ PHOTO_OBJ_DAT *cur;      // the subject whose hint is shown
    /* 0x5c */ int           hint_no;   // next index into def.hint_pict
    /* 0x60 */ int           layout;    // LayoutAnimStart slot of the hint message, -1: none
    /* 0x64 */ int           stream;    // the hint's stream (StreamAutoPlayPitch), -1: none
    /* 0x68 */ short         back;      // phase to return to (init_Story_Photo: the requester)
};
static_assert(offsetof(PHOTO_WRK, sta) == 0x04, "PHOTO_WRK.sta");
static_assert(offsetof(PHOTO_WRK, cnt) == 0x08, "PHOTO_WRK.cnt");
static_assert(offsetof(PHOTO_WRK, bits) == 0x09, "PHOTO_WRK.bits");
static_assert(offsetof(PHOTO_WRK, def) == 0x10, "PHOTO_WRK.def");
static_assert(offsetof(PHOTO_WRK, hint_tex) == 0x50, "PHOTO_WRK.hint_tex");

// A row of the step table (data 0x35fb20): mode, handler, and the
// IngameDrawSub skip flags drawn before it (-1: the game is not drawn).
// layout: host -- func is a pointer (EE rows are 0xc bytes); the table is
// rebuilt in photo_dat.c, never read as ROM bytes.
struct PHOTO_STEP                       /* 0xc on the EE */
{
    int mode;                           // -1 ends the table
    int (*func)(void);                  // 1 from step 14 when the sequence is over
    int draw;                           // IngameDrawSub skip flags, -1: none
};

void photo_datObjSealGhostDrawLock(void);  // FF3 0x00227650: clears PHOTO_BIT_END (FF2's name)
void PhotoDatInit(void);                // FF3 0x002284a0
void PhotoSeqReset(void);               // FF3 0x002284f0: PlyrShotRecord, before the definition is built
void PhotoSeqStart(PHOTO_WRK_DEF *def); // FF3 0x00228518: PlyrShotRecord, the shot's definition
int  PhotoMain(void);                   // FF3 0x002285b8: one step; 1 when step 14 is over

// Phase 33, Story_Photo (FF2 GID_STORY_PHOTO 38).
int  init_Story_Photo(void);            // FF3 0x00228638
int  end_Story_Photo(void);             // FF3 0x00228688
int  one_Story_Photo(void);             // FF3 0x00228690

#endif /* _INGAME_PHOTO_PHOTO_DAT_H */
