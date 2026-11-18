/* ==========================================================================
 *  graphics/effect/effect.h
 *
 *  The effect system's core (FF3 0x156e08..0x158ed0 and EffectInPhoto at
 *  0x2afd10; FF2 effect.c): the EFFECT_CONT control banks, the SetEffects
 *  request front door and its per-kind unpackers, the per-frame
 *  EffectControl passes, the depth-sorted draw of the model effects, the
 *  twelve whole-screen looks and the effect allocation helpers.
 *
 *  Two banks of each table exist (eff_wrk.change_bank picks one): 0x18
 *  fixed slots (screen effects: the slot is the effect id) and 0x40 free
 *  slots (model effects, first empty slot). FF3 renumbered FF2's ids:
 *
 *    id    kind   unpacker                drawn by
 *    1     1      SetEffects_Z_DEP        EffectControl 0 (slot 1)
 *    2     2      SetEffects_DITHER       EffectControl 0 SetDither3
 *    3     3      SetEffects_BLUR         EffectControl 0 SetBlur / RunBlur
 *    4     4      SetEffects_DEFORM       EffectControl 0 SetDeform
 *    5     5      SetEffects_FOCUS        EffectControl 0 SetFocus / RunFocus
 *    6     6      SetEffects_OVERLAP      EffectControl 2 SetOverRap
 *    7     7      SetEffects_FADEFRAME    EffectControl 2 SetFadeFrame
 *    8     8      SetEffects_RENZFLARE    EffectInPhoto (slot 8)
 *    9     9      SetEffects_BLACKFILTER  EffectControl 0 SetBlackFilter
 *    10    10     SetEffects_NEGA         EffectControl 0 SetNega
 *    11-13 =id    SetEffects_NCONTRAST    EffectControl 0 SetContrast2 / 3
 *    14/15 0xe/f  SetEffects_ENEDMG       (GetEffectContBuf answers 0: dead)
 *    0x10  0x10   SetEffects_Kind10       EffectControl 0 (slot 16)
 *    0x11  0x11   SetEffects_Kind11       EffectControl 0 (slot 17)
 *    0x12  0x12   SetEffects_Kind12       EffectInPhoto (slot 18)
 *    model bank (EffectZSort):
 *    0x13 HALO, 0x14 FIRE, 0x16 PDEFORM, 0x17 WATERDROP, 0x18 TORCH2,
 *    0x19 SPARK, 0x1a TORCH_FREA, 0x1b HAZE, 0x1c MANY_CANDLE, 0x1d Kind1D,
 *    0x1e Kind1E, 0x1f ENEIN; 0x15 has a slot but no unpacker.
 *
 *  SetEffects' argument tail: every argument is one EE 8-byte slot; ints
 *  and bytes are ints, floats arrive as doubles (the unpackers convert with
 *  dptofp), pointers are pointers. Host callers pass exactly that: int for
 *  byte / int slots, (double) for float slots, a real pointer (nullptr, not
 *  0) for pointer slots.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_H
#define _GRAPHICS_EFFECT_EFFECT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR

// The 16-byte head of an effect record (FF2 Q_WORDDATA): uc8[0] the kind
// (0: free), uc8[1] the request flags (bit 0 one frame, bit 1 requested by
// code, bit 2 arms the in / keep / out envelope, bit 7 has a world
// position in pnt[0], set by every model unpacker).
union EFF_DAT                           /* 0x10 */
{
    u_char uc8[16];
    int    i32[4];
    u_int  ui32[4];
    float  fl32[4];
};
static_assert(sizeof(EFF_DAT) == 0x10, "EFF_DAT");

// One effect record (FF2 EFFECT_CONT; EE 0x70, banks stride 0x70).
// FF3 grew fw to four floats, which moved z..rgba 4 bytes up from FF2.
// layout: host -- pnt holds six EE pointers (0x18 bytes at +0x20); every
// later field follows at its EE order, EE offsets in the comments.
struct EFFECT_CONT
{
    /* 0x00 */ EFF_DAT       dat;
    /* 0x10 */ sceVu0FVECTOR Pos;       // copied position; pnt[0] points here for the copying kinds
    /* 0x20 */ void         *pnt[6];    // per-kind pointers: pnt[0] the world position (bit 7 kinds)
    /* 0x38 */ float         fw[4];
    /* 0x48 */ u_int         z;         // EffectZSort: screen z of pnt[0], 0 without bit 7
    /* 0x4c */ u_int         flow;      // envelope phase 0 in, 1 keep, 2 out, 3 over, 4 reaped (EffInKeepOutFlowCtrl)
    /* 0x50 */ u_int         cnt;
    /* 0x54 */ u_int         in;
    /* 0x58 */ u_int         keep;
    /* 0x5c */ u_int         out;
    /* 0x60 */ u_int         max;
    /* 0x64 */ u_char        r;
    /* 0x65 */ u_char        g;
    /* 0x66 */ u_char        b;
    /* 0x67 */ u_char        a;
};
static_assert(offsetof(EFFECT_CONT, dat) == 0x00, "EFFECT_CONT.dat");
static_assert(offsetof(EFFECT_CONT, Pos) == 0x10, "EFFECT_CONT.Pos");
static_assert(offsetof(EFFECT_CONT, pnt) == 0x20, "EFFECT_CONT.pnt");
static_assert(offsetof(EFFECT_CONT, fw) == offsetof(EFFECT_CONT, pnt) + 6 * sizeof(void *), "EFFECT_CONT.fw (EE 0x38)");
static_assert(offsetof(EFFECT_CONT, z) == offsetof(EFFECT_CONT, fw) + 0x10, "EFFECT_CONT.z (EE 0x48)");
static_assert(offsetof(EFFECT_CONT, flow) == offsetof(EFFECT_CONT, z) + 0x04, "EFFECT_CONT.flow (EE 0x4c)");
static_assert(offsetof(EFFECT_CONT, cnt) == offsetof(EFFECT_CONT, z) + 0x08, "EFFECT_CONT.cnt (EE 0x50)");
static_assert(offsetof(EFFECT_CONT, in) == offsetof(EFFECT_CONT, z) + 0x0c, "EFFECT_CONT.in (EE 0x54)");
static_assert(offsetof(EFFECT_CONT, keep) == offsetof(EFFECT_CONT, z) + 0x10, "EFFECT_CONT.keep (EE 0x58)");
static_assert(offsetof(EFFECT_CONT, out) == offsetof(EFFECT_CONT, z) + 0x14, "EFFECT_CONT.out (EE 0x5c)");
static_assert(offsetof(EFFECT_CONT, max) == offsetof(EFFECT_CONT, z) + 0x18, "EFFECT_CONT.max (EE 0x60)");
static_assert(offsetof(EFFECT_CONT, r) == offsetof(EFFECT_CONT, z) + 0x1c, "EFFECT_CONT.r (EE 0x64)");
static_assert(offsetof(EFFECT_CONT, a) == offsetof(EFFECT_CONT, z) + 0x1f, "EFFECT_CONT.a (EE 0x67)");

#define EFFECT_SCREEN_NUM   0x18        // fixed slots per bank (0xa80 bytes on the EE)
#define EFFECT_MODEL_NUM    0x40        // free slots per bank (0x1c00 bytes on the EE)

// The effect switch block (FF2 EFF_WRK; data 0x2f6a10, InitEffects).
struct EFF_WRK                          /* 0x20 */
{
    /* 0x00 */ int    disp_flg;         // 0: effects hidden (the player's death scene, PlyrDeathHideEffects); read as a byte
    /* 0x04 */ int    monochro_mode;    // FF2's order: 0 in InitEffects
    /* 0x08 */ int    stop_flg;         // 1: effects frozen (EffectControl skips the enemy effect mains, envelopes stop)
    /* 0x0c */ int    change_bank;      // which bank of each table is live (0: 0x3961d0 / 0x396c50)
    /* 0x10 */ int    blur_off;         // 1: SetBlur draws nothing (FF2's; SetBlur reads it); 0 in InitEffects
    /* 0x14 */ int    _14;              // FF2 dith_off, unverified; 0 in InitEffects
    /* 0x18 */ int    _18;              // 1 in InitEffects; 0 while a story movie plays (one_Story_Movie_Main); IgEffectStoryMainScreenEffectReq reads it
    /* 0x1c */ int    _1c;              // 0x55 in InitEffects
};
static_assert(offsetof(EFF_WRK, disp_flg) == 0x00, "EFF_WRK.disp_flg");
static_assert(offsetof(EFF_WRK, stop_flg) == 0x08, "EFF_WRK.stop_flg");
static_assert(offsetof(EFF_WRK, change_bank) == 0x0c, "EFF_WRK.change_bank");
static_assert(offsetof(EFF_WRK, _1c) == 0x1c, "EFF_WRK._1c");
static_assert(sizeof(EFF_WRK) == 0x20, "EFF_WRK: InitEffects writes 0x2f6a10..0x2f6a2c");
extern EFF_WRK eff_wrk;                 // data 0x2f6a10

// One canned whole-screen look (FF2 SCREEN_EFFECT_PARAMETER, 0x60 there):
// twelve at data 0x2f6410 (0x80 each), pointers at 0x2f6a30, picked by
// EffectSetScreenEffectNo and the candle / camera state
// (EffectGetNowScreenEffectParamPtr), handed every frame to
// IgEffectStoryMainScreenEffectReq. The first 24 words keep FF2's order (the
// values fit it: dither 54 / 18 maxima, blur 1000 / 1800, nega 128s); the
// eight words FF3 added (ex) are read by ingame_effect.c.
struct SCREEN_EFFECT_PARAMETER          /* 0x80 */
{
    /* 0x00 */ int Z_Dep;
    /* 0x04 */ int Dither;
    /* 0x08 */ int DitherSpeed;
    /* 0x0c */ int DitherAlpha;
    /* 0x10 */ int DitherAlphaMax;
    /* 0x14 */ int DitherColorMax;
    /* 0x18 */ int Blur;
    /* 0x1c */ int BlurAlpha;
    /* 0x20 */ int BlurScale;
    /* 0x24 */ int BlrrRot;
    /* 0x28 */ int Deform;
    /* 0x2c */ int DeformRate;
    /* 0x30 */ int Focus;
    /* 0x34 */ int ColorFilter;
    /* 0x38 */ int BlackFilter;
    /* 0x3c */ int Contrast;
    /* 0x40 */ int ContrastColor;
    /* 0x44 */ int ContrastAlpha;
    /* 0x48 */ int NegaColor;
    /* 0x4c */ int NegaAlpha;
    /* 0x50 */ int NegaAlpha2;
    /* 0x54 */ int FadeFrame;
    /* 0x58 */ int FadeFrameAlpha;
    /* 0x5c */ int Overlap;
    // FF3-only, read by IgEffectStoryMainScreenEffectReq: kind 0x10 (the
    // room glow, 1 in all twelve) and kind 0x11 (b2, fw0, b3, fw1, fw2) =
    // (Kind11Col, Kind11Rate / 1000, Kind11Col2, Kind11Rate / 1000 again,
    // Kind11Rate2 / 1000); _74 is never read.
    struct
    {
        /* 0x60 */ int RoomGlow;
        /* 0x64 */ int Kind11;
        /* 0x68 */ int Kind11Col;
        /* 0x6c */ int Kind11Rate;
        /* 0x70 */ int Kind11Col2;
        /* 0x74 */ int _74;
        /* 0x78 */ int Kind11Rate2;
        /* 0x7c */ int _7c;
    } ex;
};
static_assert(offsetof(SCREEN_EFFECT_PARAMETER, Overlap) == 0x5c, "SCREEN_EFFECT_PARAMETER.Overlap");
static_assert(offsetof(SCREEN_EFFECT_PARAMETER, ex) == 0x60, "SCREEN_EFFECT_PARAMETER.ex");
static_assert(offsetof(SCREEN_EFFECT_PARAMETER, ex.Kind11Rate2) == 0x78, "SCREEN_EFFECT_PARAMETER.ex.Kind11Rate2");
static_assert(sizeof(SCREEN_EFFECT_PARAMETER) == 0x80, "SCREEN_EFFECT_PARAMETER: table stride 0x80");

#define SCREEN_EFFECT_NUM   12

void InitEffects(void);                 // FF3 0x00156e08: IngameInit
int  CheckSetEffectsOK(void);           // FF3 0x00156f50: effect data ready (IngameNowLoadingMain waits on it)
void InitEffectsEF(void);               // FF3 0x00156fa8: per frame, first thing EffectDrawMain does
void EffectsReleaseAll(void);           // FF3 0x00157000: IngameEnd: every effect cut, both banks reset
void EffectDrawMain(u_int flag);        // FF3 0x00157150: IngameDrawSub / SceneDraw (flag unused)

EFFECT_CONT *EffectGetScreenCont(u_int no);     // FF3 0x001571d0: fixed slot no of the live bank, nullptr for no >= 0x18
EFFECT_CONT *EffectGetModelCont(u_int no);      // FF3 0x00157218: free slot no of the live bank, nullptr for no >= 0x40
int  EffectIsDrawOn(void);              // FF3 0x00157260: disp_flg and not IngameGetDrawFlags 0x10

// The effect request entry (FF2 SetEffects: id, flags, then the per-kind
// arguments; see the table above). Returns the record, nullptr when effects
// are off or no slot is free.
void *SetEffects(int id, int fl, ...);  // FF3 0x00157300
void  CutEffects(int id);               // FF3 0x00157658: ResetEffects on the effect of id (GetEffectContBuf)
void  EffectRelease(void *eff);         // FF3 0x00157678: ends an effect (kind 0x1c: nothing, 0x1d: EffKind1DRelease, else ResetEffects)
void  ResetEffects(void *eff);          // FF3 0x001583a0: clears an effect SetEffects returned (nullptr: nothing)

void  EffectControl(int no);            // FF3 0x00158728: pass 0 (3D + screen filters), 1 (enemy damage), 2 (last 2D)
void  EffectSetScreenEffectNo(u_int no);    // FF3 0x00158988: screen effect 0..11 (others ignored)
int   EffectGetScreenEffectNo(void);    // FF3 0x001589a0
SCREEN_EFFECT_PARAMETER *EffectGetScreenEffectParamPtr(u_int no);   // FF3 0x001589a8: no > 11 -> set 0
SCREEN_EFFECT_PARAMETER *EffectGetNowScreenEffectParamPtr(void);    // FF3 0x001589c8
int   EffectNowScreenEffectIs5Or6(void);    // FF3 0x00158aa0: the candle-out looks
void  EffInKeepOutFlowCtrl(EFFECT_CONT *ec);    // FF3 0x00158b78: steps the in / keep / out envelope
int   EffectIsSlotNo(int no);           // FF3 0x00158c60: fixed slot no holds kind no
int   EffectExecCheck(void *eff, int kind);     // FF3 0x00158c90: eff is a live model slot of that kind
int   EffectModelKindIsSet(int kind);   // FF3 0x00158cf8: any model slot holds kind (PlyrFlashInput: 0x1b HAZE blocks the flash)
void  EffectPhotoPhase(void);           // FF3 0x00158d50: EffectInPhoto on the model bank, no blur deferral
float EffectHazeLevelMax(void);         // FF3 0x00158d78: largest haze level of the kind 0x1b effects

void *EFFECT_MALLOC(int size);          // FF3 0x00158df8: system heap; size <= 0: nullptr
void  EFFECT_FREE(void *p);             // FF3 0x00158e50: nullptr ignored

#endif /* _GRAPHICS_EFFECT_EFFECT_H */
