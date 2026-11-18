/* ==========================================================================
 *  graphics/effect/effect_oth.h
 *
 *  FF3 0x1766c8..0x17f8b8 and 0x185908..0x186a88 (file name kept from the
 *  link-order guess). Several FF3 modules share the range:
 *    - 0x1766c8..0x176868  FF3's room glow pairs (effect_obj.c reads them);
 *    - 0x176868..0x178368  the head of FF2 effect_scr.c: InitEffectScr / EF,
 *      the blur (SubBlur, FF3's fan blur, SetBlur / RunBlur / CallBlur2),
 *      the focus, the contrast and the nega filters;
 *    - 0x178368..0x17c750  the screen filters of FF2 effect_scr.c (deform,
 *      fade frame, black filter, dither, overlap), the request wrappers, the
 *      monochrome switch, the brightness filter and the screen flash;
 *    - 0x17c750..0x17ca48  the sky (SORA_PK4);
 *    - 0x17ca48..0x17d830  a sound-bank list on SingleLinkList;
 *    - 0x17d830..0x17f8b8  the map particles of map effects 200 / 201 (list
 *      0x3deda0, the room records of effect_obj.c's EffObjBufTbl; the
 *      per-type callbacks 0x17ee20..0x17f8b8 and the out-of-line helpers
 *      0x2b6678 / 0x2b6c10 / 0x2b6cc8) and a camera shake at
 *      0x17d978..0x17db58;
 *    - 0x185908..0x186a88  FF2 effect_oth.c's distortion particles and
 *      FF3's drips (EffDrip / EffDripFloor / EffDripMgr, map effects
 *      300..303 / 400..403; Ghidra had the FF2 names GetEnePartAddr /
 *      GetAmuPartAddr / SetPartInit / update_particles on them).
 *  The enemy work that used to be described here is ENE_WRK in
 *  ingame/enemy/ene_main.h (FF3 0x186a88.. is the enemy main loop).
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_OTH_H
#define _GRAPHICS_EFFECT_EFFECT_OTH_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR

struct EFFECT_CONT;                     // graphics/effect/effect.h
struct ROOM_GLOW_COL;                   // graphics/effect/effect_obj.h

// ──────────────────────────────────────────────────────────────────────
// The room glow pairs (FF3-only; effect_obj.c's room glow reads them)
// ──────────────────────────────────────────────────────────────────────

// One pair per map buffer (bss 0x3d46f0, 0x20 each: ROOM_GLOW_COL [0]
// normally, [1] while the camera is up) and a third, the default
// (0x3d4730), which nothing but the two memsets writes: all zero, no glow.
// MapRoomFile.c fills a buffer's pair from member 5 of the room file.
ROOM_GLOW_COL *RoomGlowDatDefault(void);    // FF3 0x001766c8: the default pair
void  RoomGlowInit(void);               // FF3 0x001766d8: InitEffects
void  RoomGlowRelease(void);            // FF3 0x00176700: EffectsReleaseAll
int   RoomGlowDatSize(void);            // FF3 0x00176728: 0x20; unreferenced
void  RoomGlowDatClear(int buff);       // FF3 0x00176730: buff 0 / 1 gets the default pair
void  RoomGlowDatSet(int buff, const void *dat);    // FF3 0x001767a8: 8 words of dat, nullptr: cleared
ROOM_GLOW_COL *RoomGlowDatGet(int buff);    // FF3 0x00176830: buff's pair, the default for buff > 1

// ──────────────────────────────────────────────────────────────────────
// The screen filters (EffectControl's slots; FF2 effect_scr.c)
// ──────────────────────────────────────────────────────────────────────

// The blur / focus envelopes (FF2's EFF_BLUR / EFF_FOCUS). Only CallBlur2
// arms eff_blur and nothing arms eff_focus in FF3 (CallBlur2 is
// unreferenced, FF2's CallBlur / CallFocus are gone), so both stay at flow
// 3 and RunBlur / RunFocus only keep the half copy at 0x38e0 fresh.
struct EFF_BLUR                         /* 0x28 */
{
    /* 0x00 */ u_int flow;              // 0 in, 1 keep, 2 out, 3 over
    /* 0x04 */ u_int cnt;
    /* 0x08 */ u_int in;
    /* 0x0c */ u_int keep;
    /* 0x10 */ u_int out;
    /* 0x14 */ u_int alp;
    /* 0x18 */ float scl;
    /* 0x1c */ float rot;               // degrees, 180 = upright
    /* 0x20 */ float cx;                // the centre SubBlur scales and turns round
    /* 0x24 */ float cy;
};
static_assert(sizeof(EFF_BLUR) == 0x28, "EFF_BLUR: bss 0x3d4750..0x3d4778");
static_assert(offsetof(EFF_BLUR, alp) == 0x14, "EFF_BLUR.alp (0x3d4764)");
static_assert(offsetof(EFF_BLUR, cy) == 0x24, "EFF_BLUR.cy (0x3d4774)");
extern EFF_BLUR eff_blur;               // bss 0x3d4750

struct EFF_FOCUS                        /* 0x18 */
{
    /* 0x00 */ u_int flow;
    /* 0x04 */ u_int cnt;
    /* 0x08 */ u_int in;
    /* 0x0c */ u_int keep;
    /* 0x10 */ u_int out;
    /* 0x14 */ u_int max;               // added to the record's ef at the envelope's peak
};
static_assert(sizeof(EFF_FOCUS) == 0x18, "EFF_FOCUS: bss 0x3d4778..0x3d4790");
static_assert(offsetof(EFF_FOCUS, max) == 0x14, "EFF_FOCUS.max (0x3d478c)");
extern EFF_FOCUS eff_focus;             // bss 0x3d4778

void  InitEffectScr(void);              // FF3 0x00176868: InitEffects
void  InitEffectScrEF(void);            // FF3 0x00176978: InitEffectsEF, first
void  EffectScrRelease(void);           // FF3 0x001769b8: EffectsReleaseAll; empty

// The frame laid back over itself, scaled by (scw, sch) and turned by rot
// - 180 degrees round (cx, cy): src 0 samples the 320 x 224 copy at 0x38e0
// (restored from EE 0x117b000 first), 1 the frame shown last; type 2
// darkens (0x78), 3 brightens (0x88), others neutral. SetBlur / RunBlur
// pass src 0; 0x17fdb0 and 0x22b888 are the other callers.
void  SubBlur(float scw, float sch, float rot, float cx, float cy, int type, u_char alp, int src);  // FF3 0x00176fd8
void  SetBlur(EFFECT_CONT *ec);         // FF3 0x001771f8: kind 3
void  RunBlur(EFFECT_CONT *ec);         // FF3 0x00177380: slot 3, every pass 0
// IngameDrawSub / SceneDraw: the half copy at 0x38e0 into EE 0x117b000.
void  EffectOthBufCopy(void);           // FF3 0x00177780
void  CallBlur2(int in, int keep, int out, u_char alp, float scl, float rot, float cx, float cy);  // FF3 0x001777a8: unreferenced
// Four copies of the shown frame, ef / 40 pixels off (x, y) diagonally,
// alpha ef * 4 (40 from ef 10 on); the sprite's UV rectangle and size are
// the caller's (SetFocus / RunFocus: the whole frame). Also MenuBgDraw and 0x1b45d8.
void  SubFocus(float u1, float v1, float u2, float v2, float w, float h, float x, float y, int ef);   // FF3 0x00177800
void  SetFocus(EFFECT_CONT *ec);        // FF3 0x00177998: kind 5
void  RunFocus(EFFECT_CONT *ec);        // FF3 0x00177a48: slot 5, every pass 0
// FF2 SubContrast2 (type 0: the frame multiplied over itself, brightening
// the mids) and SubContrast3 (else: a flat colour subtracted twice).
void  SubContrast(int type, u_char col, u_char alp);    // FF3 0x00177ee0
void  SetContrast2(EFFECT_CONT *ec);    // FF3 0x00178010: kinds 0xb / 0xc
void  SetContrast3(EFFECT_CONT *ec);    // FF3 0x00178068: kind 0xd
// The tint (r, g, b, alp) subtracted, then the half copy at 0x2bc0 blended
// back by 0x80 - alp2 (alp2 >= 0x80: not at all).
void  SubNega(u_char r, u_char g, u_char b, u_char alp, u_char alp2);  // FF3 0x001780c0
void  SetNega(EFFECT_CONT *ec);         // FF3 0x00178228: kind 10

// The deform state (sdata 0x372f70; also read by 0x176868..0x1769a4,
// unassigned@001766c8, and the deform kernels).
struct EFF_DEFORM_WRK                   /* 4 */
{
    /* 0x0 */ u_char type;              // the type the kernels (and SetDeformType3) last drew
    /* 0x1 */ u_char otype;             // the type the kernels last ran (SetDeform compares)
    /* 0x2 */ u_char init;              // 1: the kernel restarts its own state (a type change)
    /* 0x3 */ u_char pass;              // 1: a kernel drew this frame (FF2's pass)
};
static_assert(sizeof(EFF_DEFORM_WRK) == 4, "EFF_DEFORM_WRK: sdata 0x372f70..0x372f73");
extern EFF_DEFORM_WRK eff_deform;       // sdata 0x372f70

// The dither pattern state (sdata 0x372f78; DitherPatternUpdate, also
// 0x176868 and the two dither draws).
struct EFF_DITHER_WRK                   /* 3 */
{
    /* 0x0 */ u_char alpmx;             // the last (alpmx, colmx) asked for
    /* 0x1 */ u_char colmx;
    /* 0x2 */ u_char remake;            // 1: the pair changed, remake once it holds
};
static_assert(sizeof(EFF_DITHER_WRK) == 3, "EFF_DITHER_WRK: sdata 0x372f78..0x372f7a");
extern EFF_DITHER_WRK eff_dither;       // sdata 0x372f78

// The overlap's pass flags (sdata 0x36eb98): SetOverRap sets [0] every frame
// it runs; 0x176868 / 0x176978 (unassigned@001766c8) keep [1], so a first
// frame after a pause grabs the screen again.
extern short overlap_passflg[2];        // sdata 0x36eb98

void *CallNega2(int in, int keep, int out);     // FF3 0x00178368: SetEffects(10, 4, 0x40, 0xc4, ...): PlyrDamageCtrl (0, 10, 5) / (0, 15, 8) on a hit
void *CallDeform2(int in, int keep, int out, int type, int max);    // FF3 0x00178698: SetEffects(4, 4, type, max, ...)

void  SetForcusDepth(EFFECT_CONT *ec);  // FF3 0x00178398: kind 1 (slot 1); FF3 kept only the one-frame reset
void  SubDeform(int type, float rate, u_char alp);  // FF3 0x001783c8: the deform kernel of type 0..9
void  SetDeform(EFFECT_CONT *ec);       // FF3 0x00178468: kind 4
void  SubFadeFrame(u_char alp, u_int pri);  // FF3 0x0017a840: the full-screen vignette
void  SetFadeFrame(EFFECT_CONT *ec);    // FF3 0x0017a928: kind 7
void  SetBlackFilter(EFFECT_CONT *ec);  // FF3 0x0017a978: kind 9
void  DitherPatternUpdate(u_int alpmx, u_int colmx);    // FF3 0x0017a9f8
// The screen dither (FF2 SubDither3, effect_scr.c; FF2's argument order, the
// same registers): SetDither3 and the seal door burn (EffSealDoorBurnDither:
// (5, rate * 95, 24, 0x40, 0x40)).
void  SubDither3(int type, float alp, float spd, u_char alpmx, u_char colmx);  // FF3 0x0017aaa0
void  SubDither4(float alp, float spd, u_char alpmx, u_char colmx);    // FF3 0x0017b1c8: dither types >= 8
void  SetDither3(EFFECT_CONT *ec);      // FF3 0x0017b778: kind 2
void  FUN_0017b970(EFFECT_CONT *ec);    // FF3 0x0017b970: kind 0x11 (slot 17)
void  SetOverRap(EFFECT_CONT *ec);      // FF3 0x0017b9f0: kind 6

// on: eff_wrk.monochro_mode and the 3D monotone draw; prelight: re-prelight
// the rooms when the state changes (EvMonoDisp).
void  EffectMonochromeSet(int on, int prelight);    // FF3 0x0017bdc8
// The brightness option as a full-screen filter: EffectDrawMain (1.0, 0, 0, 640, 448).
void  BrightnessAdjustmentFilterDraw(float scale, int x, int y, int w, int h);  // FF3 0x0017be50

// The screen flash / fade (data 0x2f99e0): mode 1 fades from alpha to 0,
// mode 2 from 0 to alpha and then holds it; time in NTSC frames.
void  ScreenFlashReq(u_char alpha, int time, u_char r, u_char g, u_char b, int mode);  // FF3 0x0017c028
int   ScreenFlashDraw(void);            // FF3 0x0017c0f0: EffectControl 2, last; the mode, 0 when over
void  ScreenFadeToWhite(int time);      // FF3 0x0017c218
void  ScreenFlashWhite(int time);       // FF3 0x0017c248
void  ScreenFadeToBlack(int time);      // FF3 0x0017c278
void  ScreenFlashBlack(int time);       // FF3 0x0017c2a8: SceneEffectFadeInOut
void  ScreenFlashWhiteShort(void);      // FF3 0x0017c2d8: an 18-frame white flash; no references

// Unreferenced in FF3: a screen-space polar ripple over the copy at 0x2bc0
// (12 crests, swung 18 times round the centre) and three small functions.
void  SetDeformScrRipple(float rate, u_char alp);   // FF3 0x0017c308
void  EffScrNop0(void);                 // FF3 0x0017c738: empty
void  EffScrNop1(void);                 // FF3 0x0017c740: empty
void  EffScrUnusedSet(int v);           // FF3 0x0017c748: sdata 0x36efc4 = v, read nowhere

// ──────────────────────────────────────────────────────────────────────
// The sky (SORA_PK4)
// ──────────────────────────────────────────────────────────────────────

// The sky's look (data 0x2f99f0, only EffSoraDraw reads it).
struct EFF_SORA_DAT                     /* 0x50 */
{
    /* 0x00 */ u_int _00[10];           // not read by the draw (kept as the ROM's words)
    /* 0x28 */ float dist;              // the grid's plane (-3000)
    /* 0x2c */ float scale;             // the grid's extent, x 128 (4.0)
    /* 0x30 */ float _30[3];
    /* 0x3c */ int   period;            // frames per U scroll (3500)
    /* 0x40 */ int   r, g, b;           // vertex colour (grey in monochrome)
    /* 0x4c */ int   alpha;             // the grid's alpha (0x5a)
};
static_assert(sizeof(EFF_SORA_DAT) == 0x50, "EFF_SORA_DAT is 0x50 bytes (0x2f99f0..0x2f9a40)");
static_assert(offsetof(EFF_SORA_DAT, dist) == 0x28, "EFF_SORA_DAT.dist");
static_assert(offsetof(EFF_SORA_DAT, period) == 0x3c, "EFF_SORA_DAT.period");
static_assert(offsetof(EFF_SORA_DAT, r) == 0x40, "EFF_SORA_DAT.r");
static_assert(offsetof(EFF_SORA_DAT, alpha) == 0x4c, "EFF_SORA_DAT.alpha");

void  EffSoraTexSend(u_int n);          // FF3 0x0017c750
int   EffSoraLoadReq(void);             // FF3 0x0017c7b0: InitEffects
void  EffSoraRelease(void);             // FF3 0x0017c808: EffectsReleaseAll
int   EffSoraIsLoadEnd(void);           // FF3 0x0017c840
void  EffSoraDraw(void);                // FF3 0x0017c890: IngameDrawSub
// The sky grid's packet (out-of-line, 0x2b5f10): rot the camera's (pitch, yaw).
// FF3 passes &dat->dist and reads the record from there.
void  EffSoraDrawSub(EFF_SORA_DAT *dat, float *rot);    // FF3 0x002b5f10

// ──────────────────────────────────────────────────────────────────────
// The sound-bank list (0x3ded70 / 0x3ded80 / 0x3ded90)
// ──────────────────────────────────────────────────────────────────────

// Banks are loaded on the first play request of their id (SndBankNew(id,
// id - 1, -1)), plays wait in list 0x3ded90 until the bank is ready, and a
// release request drops a bank once none of its plays is left. Dead in FF3:
// only SndBankListInit (InitEffects) and SndBankListBufAllStop have callers
// (no jal / j / data word / lui-addiu pair reaches the rest).
void  SndBankListInit(void);            // FF3 0x0017ca48
void  SndBankListRelease(void);         // FF3 0x0017ca90: unreferenced
void  SndBankListMain(void);            // FF3 0x0017cab8: the per-frame step (unreferenced)
int   SndBankListIsBank(int id);        // FF3 0x0017cae0: 1 when id is listed
void  SndBankListLoadReq(int id);       // FF3 0x0017cb70: cancels id's release, lists and loads it once
void  SndBankListRemoveBank(int id);    // FF3 0x0017cbf0: drops id's cells (no release)
int   SndBankListGetBank(int id);       // FF3 0x0017cc88: the id's bank handle, -1 none
int   SndBankListIsReady(int id);       // FF3 0x0017cd18: 1 once id's bank is ready
void  SndBankListReadyUpdate(void);     // FF3 0x0017cda8: marks the banks SndBankIsReady passes
void  SndBankListAllRelease(void);      // FF3 0x0017ce38: releases and drops list 0x3ded70's banks
void  SndBankListReleaseReq(int id);    // FF3 0x0017ced8: asks to release a listed bank (once)
int   SndBankListIsReleaseReq(int id);  // FF3 0x0017cf28
void  SndBankListReleaseReqCancel(int id);  // FF3 0x0017cfb8
void  SndBankListReleaseReqClear(void); // FF3 0x0017d050: empties the release requests 0x3ded80
void  SndBankListReleaseStep(void);     // FF3 0x0017d0b0: releases the requested banks with no play left
// A play of sound no of bank id (effect as SndBankPlay's; arg is kept in the
// record and read nowhere); pos nullptr: not placed, else a 3D source at pos
// (copied into the record); key for SndBankListStopKey.
void  SndBankListPlay(int id, int no, int effect, int arg, float *pos, int key);   // FF3 0x0017d170
void  SndBankListPlayNoKey(int id, int no, int effect, int arg, float *pos);       // FF3 0x0017d2d0: key 0
void  SndBankListPlayKey(int id, int no, int effect, int arg, float *pos, int key); // FF3 0x0017d2e8
void  SndBankListStop(int id, int no, int fade);    // FF3 0x0017d300: fade: SndBufFadeStop over 60 frames
void  SndBankListStopKey(int key, int fade);        // FF3 0x0017d3f8
void  SndBankListBufAllStop(void);      // FF3 0x0017d4d8
void  SndBankListPauseAll(void);        // FF3 0x0017d578
void  SndBankListRestartAll(void);      // FF3 0x0017d5e8
int   SndBankListIsPlayReq(int id);     // FF3 0x0017d658: 1 while a play of id is listed
void  SndBankListPlayStep(void);        // FF3 0x0017d6e8: starts the waiting plays, drops the finished

// ──────────────────────────────────────────────────────────────────────
// The map particles (map effects 200 / 201)
// ──────────────────────────────────────────────────────────────────────

// One particle (0xa0, EFFECT_MALLOC'd in blocks of Num). No pointers: the
// EE layout holds on the hosts.
struct EFF_MAP_PARTICLE                 /* 0xa0 */
{
    /* 0x00 */ sceVu0IVECTOR Scr[2];    // the sprite's corners in GS units (ftoi4: x, y, z, w * 16)
    /* 0x20 */ sceVu0FVECTOR Pos;       // w 1
    /* 0x30 */ sceVu0FVECTOR Vel;
    /* 0x40 */ sceVu0FVECTOR Acc;       // random kicks (the record's Kick / KickRate)
    /* 0x50 */ int           R, G, B;   // the draw takes the first particle's for all
    /* 0x5c */ int           Alpha;     // base alpha
    /* 0x60 */ float         Rise;      // added to Vel.y every step
    /* 0x64 */ float         SizeW;     // half size (x 512 / fov / w on screen)
    /* 0x68 */ float         SizeH;
    /* 0x6c */ float         Shrink;    // size factor per step
    /* 0x70 */ short         Cnt;       // life counter (Timed)
    /* 0x72 */ short         CntMax;
    /* 0x74 */ short         Tex;       // effdat index (the draw takes the first particle's)
    /* 0x76 */ short         DrawAlpha; // this frame's alpha
    /* 0x78 */ short         _78;
    /* 0x7a */ short         FadeIn;    // frames of fade-in left
    /* 0x7c */ int           Timed;     // 1: dies when Cnt reaches CntMax
    /* 0x80 */ int           FarFade;   // 1: fades with depth and with the distance from the screen centre
    /* 0x84 */ int           NearFade;  // 1: fades in front of the camera (w * 16 < 600)
    /* 0x88 */ int           Move;      // 0: frozen in place (landed)
    /* 0x8c */ int           Alive;
    /* 0x90 */ int           Visible;   // set by the draw's guard band / alpha test
    /* 0x94 */ int           _94[3];
};
static_assert(sizeof(EFF_MAP_PARTICLE) == 0xa0, "EFF_MAP_PARTICLE: EffMapPartCreate allocates num * 0xa0");
static_assert(offsetof(EFF_MAP_PARTICLE, Pos) == 0x20, "EFF_MAP_PARTICLE.Pos");
static_assert(offsetof(EFF_MAP_PARTICLE, R) == 0x50, "EFF_MAP_PARTICLE.R");
static_assert(offsetof(EFF_MAP_PARTICLE, Rise) == 0x60, "EFF_MAP_PARTICLE.Rise");
static_assert(offsetof(EFF_MAP_PARTICLE, Cnt) == 0x70, "EFF_MAP_PARTICLE.Cnt");
static_assert(offsetof(EFF_MAP_PARTICLE, FadeIn) == 0x7a, "EFF_MAP_PARTICLE.FadeIn");
static_assert(offsetof(EFF_MAP_PARTICLE, Timed) == 0x7c, "EFF_MAP_PARTICLE.Timed");
static_assert(offsetof(EFF_MAP_PARTICLE, Alive) == 0x8c, "EFF_MAP_PARTICLE.Alive");
static_assert(offsetof(EFF_MAP_PARTICLE, Visible) == 0x90, "EFF_MAP_PARTICLE.Visible");

// A type's parameter block in the room's EffObjBufTbl record (EFF_OBJ_BUF
// +0x10 type 0, +0xa0 type 1; 0x90 bytes each). Type 2 uses constants.
struct EFF_MAP_PART_PRM                 /* 0x90 */
{
    /* 0x00 */ float Ext[3];            // the spawn volume's extent
    /* 0x0c */ u_int _0c;
    /* 0x10 */ float Floor;             // negated into EFF_MAP_PART.Floor
    /* 0x14 */ int   Num;               // particle count (EffMapPartReq)
    /* 0x18 */ u_int _18[2];
    /* 0x20 */ float Kick[3];           // x 0.01
    /* 0x2c */ u_int _2c;
    /* 0x30 */ float KickRate;
    /* 0x34 */ float Damp;
    /* 0x38 */ u_int _38[2];
    /* 0x40 */ float VelMin[3];
    /* 0x4c */ u_int _4c;
    /* 0x50 */ float VelRange[3];
    /* 0x5c */ u_int _5c;
    /* 0x60 */ float SizeMin;
    /* 0x64 */ float SizeRange;
    /* 0x68 */ float Shrink;
    /* 0x6c */ float RiseMin;           // x 0.1
    /* 0x70 */ float RiseRange;
    /* 0x74 */ int   R, G, B;
    /* 0x80 */ int   AlphaMin;
    /* 0x84 */ int   AlphaRange;        // the random part is masked to 8 bits
    /* 0x88 */ u_int _88[2];
};
static_assert(sizeof(EFF_MAP_PART_PRM) == 0x90, "EFF_MAP_PART_PRM: type 0 at record +0x10, type 1 at +0xa0");
static_assert(offsetof(EFF_MAP_PART_PRM, Num) == 0x14, "EFF_MAP_PART_PRM.Num (record +0x24 / +0xb4)");
static_assert(offsetof(EFF_MAP_PART_PRM, VelMin) == 0x40, "EFF_MAP_PART_PRM.VelMin");
static_assert(offsetof(EFF_MAP_PART_PRM, SizeMin) == 0x60, "EFF_MAP_PART_PRM.SizeMin");
static_assert(offsetof(EFF_MAP_PART_PRM, R) == 0x74, "EFF_MAP_PART_PRM.R");
static_assert(offsetof(EFF_MAP_PART_PRM, AlphaRange) == 0x84, "EFF_MAP_PART_PRM.AlphaRange");

// The room's SCL file (EFF_OBJ_BUF.pBuf): boxes that keep particles out.
// EffMapPartCreate moves them by the room's offset once (Moved).
struct EFF_MAP_SCL_BOX                  /* 0x20 */
{
    /* 0x00 */ sceVu0FVECTOR Min;
    /* 0x10 */ sceVu0FVECTOR Max;
};
struct EFF_MAP_SCL                      /* 0x10 + Num * 0x20 */
{
    /* 0x00 */ int             Num;
    /* 0x04 */ int             KeepNum; // the first KeepNum boxes keep a particle, the rest throw it out
    /* 0x08 */ int             Moved;
    /* 0x0c */ int             _0c;
    /* 0x10 */ EFF_MAP_SCL_BOX Box[1];  // Num of them
};
static_assert(sizeof(EFF_MAP_SCL_BOX) == 0x20, "EFF_MAP_SCL_BOX: stride 0x20");
static_assert(offsetof(EFF_MAP_SCL, Box) == 0x10, "EFF_MAP_SCL.Box");

// One record of the list 0x3deda0 (0x70 on the EE). Built by
// EffMapPartCreate from the area's EffObjBufTbl record and the type's setup
// callback (table 0x2f9a40).
// layout: host -- pPart / pPrm / pScl are pointers (EE 4 bytes at 0 / 4 / 8);
// the fields from Pos on follow at the EE order, EE offsets in the comments.
// PC: the list's cells hold sizeof(EFF_MAP_PART), not the EE's 0x70.
struct EFF_MAP_PART
{
    /* 0x00 */ EFF_MAP_PARTICLE *pPart; // Num particles, EFFECT_MALLOC'd
    /* 0x04 */ EFF_MAP_PART_PRM *pPrm;  // the type's parameter block (not set for type 2)
    /* 0x08 */ EFF_MAP_SCL   *pScl;     // the room record's SCL file, nullptr: none
    /* 0x0c */ int            _0c;
    /* 0x10 */ sceVu0FVECTOR  Pos;      // the spawn volume's centre
    /* 0x20 */ sceVu0FVECTOR  Ext;      // its extent
    /* 0x30 */ sceVu0FVECTOR  Kick;     // the random acceleration's size
    /* 0x40 */ u_long         Alpha;    // GS ALPHA_1 (0x44 normal, 0x42 subtractive)
    /* 0x48 */ u_long         Test;     // GS TEST_1
    /* 0x50 */ float          KickRate; // the chance of a kick per step
    /* 0x54 */ float          Damp;     // velocity / acceleration damping per step
    /* 0x58 */ float          Floor;    // the y where the type's land callback runs
    /* 0x5c */ float          FloorOfs; // the level EffMapPartFloorLevelUpdate picks (type 1, area 6)
    /* 0x60 */ short          Type;     // 0 / 1 (map effect 200 / 201), 2
    /* 0x62 */ short          Num;
    /* 0x64 */ int            Ready;    // 1 once the particles are set up
    /* 0x68 */ int            Area;
    /* 0x6c */ int            No;       // the map effect's number
};
static_assert(offsetof(EFF_MAP_PART, Ext) == offsetof(EFF_MAP_PART, Pos) + 0x10, "EFF_MAP_PART.Ext (EE 0x20)");
static_assert(offsetof(EFF_MAP_PART, Alpha) == offsetof(EFF_MAP_PART, Pos) + 0x30, "EFF_MAP_PART.Alpha (EE 0x40)");
static_assert(offsetof(EFF_MAP_PART, KickRate) == offsetof(EFF_MAP_PART, Pos) + 0x40, "EFF_MAP_PART.KickRate (EE 0x50)");
static_assert(offsetof(EFF_MAP_PART, FloorOfs) == offsetof(EFF_MAP_PART, Pos) + 0x4c, "EFF_MAP_PART.FloorOfs (EE 0x5c)");
static_assert(offsetof(EFF_MAP_PART, Type) == offsetof(EFF_MAP_PART, Pos) + 0x50, "EFF_MAP_PART.Type (EE 0x60)");
static_assert(offsetof(EFF_MAP_PART, No) == offsetof(EFF_MAP_PART, Type) + 0x0c, "EFF_MAP_PART.No (EE 0x6c)");

void  EffMapPartInit(void);             // FF3 0x0017d830: InitEffects
void  EffMapPartInPhoto(void);          // FF3 0x0017d860: EffectInPhoto
void  EffMapPartRelease(void);          // FF3 0x0017d8a0: EffectsReleaseAll
void  EffMapPart0Req(float *pos, int no, int area);     // FF3 0x0017d8c0: map effect 200
void  EffMapPart0Cut(int no);           // FF3 0x0017d8e8
int   EffObjBufTblIsSet(int area);      // FF3 0x0017d908: area has a room record
void  EffMapPart1Req(float *pos, int no, int area);     // FF3 0x0017d930: map effect 201
void  EffMapPart1Cut(int no);           // FF3 0x0017d958
void  EffMapPartListInit(void);         // FF3 0x0017de30
int   EffMapPartReq(int type, float *pos, int no, int area);    // FF3 0x0017eb70: 1 when asked (also when the list is full)
void  EffMapPartCut(int type, int no);  // FF3 0x0017ec70: no < 0: all
void  EffMapPartDrawType(float rate, int type);         // FF3 0x0017ed50

int   EffMapPartCreate(EFF_MAP_PART *mp, float *pos, int num, int type, int area);  // FF3 0x0017de50: 0 when num < 1 or no memory
void  EffMapPartDraw(float rate, EFF_MAP_PART *mp);     // FF3 0x0017e328: step and draw, types 0..2 (table 0x2f9a80)

// The menu backdrop (menu_file.c 0x208420 / 0x2083d0): map particle type 2
// under a fixed camera with three scrolling fog layers; Start saves the
// game camera and End puts it back.
void  EffMenuBgStart(int no);           // FF3 0x0017d978
void  EffMenuBgEnd(int no);             // FF3 0x0017da60
void  EffMenuBgDraw(float rate);        // FF3 0x0017dac8

void  FUN_0017de28(void);               // FF3 0x0017de28: empty (FUN_002083d0)

// ──────────────────────────────────────────────────────────────────────
// The distortion particles (FF2 effect_oth.c)
// ──────────────────────────────────────────────────────────────────────

// FF2's PARTICLE (0x50; no pointers, the EE layout holds on the hosts).
struct PARTICLE                         /* 0x50 */
{
    /* 0x00 */ float position[4];
    /* 0x10 */ float color[4];          // rgb 0..255, a the live alpha
    /* 0x20 */ float velocity[4];
    /* 0x30 */ float acceleration[4];
    /* 0x40 */ float alp_step;
    /* 0x44 */ int   lifetime;          // counts down; 0 dead
    /* 0x48 */ int   BaseLifeTime;
    /* 0x4c */ float Scale;
};
static_assert(sizeof(PARTICLE) == 0x50, "PARTICLE: draw_distortion_particles2 steps 0x50");
static_assert(offsetof(PARTICLE, color) == 0x10, "PARTICLE.color");
static_assert(offsetof(PARTICLE, Scale) == 0x4c, "PARTICLE.Scale");

// One textured fan per live particle (alpha > 0) inside the clip volume:
// full alpha at the centre, 0 at the corners. The number of live particles.
int   draw_distortion_particles2(float (*matLocalScreen)[4], float (*matLocalClip)[4],
                                 int t_particles, PARTICLE *pPartTop, float psize,
                                 u_long tex0, u_long alpha);    // FF3 0x00185908

// ──────────────────────────────────────────────────────────────────────
// The drips (map effects 300..303 / 400..403; FF3-only, no RTTI)
// ──────────────────────────────────────────────────────────────────────
// A drip (map effect 300 + kind) hangs at its point and waits until a floor
// point (map effect 400 + type) lies under it (XZ within 1.0); from then on it
// forms, falls, splashes (EffParticleReq 7 + type) and forms again, for
// ever. The floor point is used up by the drip that takes it.

// The colours (data 0x354c20): kinds 0 / 1 and 2 / 3; a is the alpha the
// drip grows to. EffParticleReq's splashes 7 / 8 take row 0 (EffSparkColGet).
extern int eff_drip_col[2][4];          // data 0x354c20

class EffDrip                           /* 0x60 */
{
public:
    EffDrip(const float *pos, int kind, int id);    // FF3 0x00185d70 (EffDripCtor; base-object twin 0x00185d20 EffDripCtorTwin)
    ~EffDrip();                         // FF3 0x00185dc8 (EffDripDtor): empty
    void SetAlphaMax();                 // FF3 0x00185dd0 (EffDripSetAlphaMax)
    void Reset();                       // FF3 0x00185e18 (EffDripReset)
    void Start(float floor, int type);  // FF3 0x00185e80 (EffDripStart)
    void Splash();                      // FF3 0x00185e98 (EffDripSplash)
    void Step();                        // FF3 0x00185f28 (EffDripStep)
    void Draw();                        // FF3 0x001861f0 (EffDripDraw)

    /* 0x00 */ int    state;            // 0 wait, 1 grow, 2 hang, 3 fall, 4 fade
    /* 0x04 */ int    _04[3];
    /* 0x10 */ sceVu0FVECTOR pos;
    /* 0x20 */ float  vel;              // y per frame
    /* 0x24 */ float  acc;              // added to vel through the damping
    /* 0x28 */ float  gravity;          // > 0: falling
    /* 0x2c */ float  size;             // the sprite's half size (0.2 grown)
    /* 0x30 */ short  alpha;
    /* 0x32 */ short  timer;
    /* 0x34 */ int    active;           // 1 once a floor point started it
    /* 0x38 */ float  floor;            // the floor point's y
    /* 0x3c */ int    type;             // the floor point's splash type 0..3
    /* 0x40 */ sceVu0FVECTOR base;      // where it forms
    /* 0x50 */ int    kind;             // 0..3: colour row kind / 2, double size for odd kinds
    /* 0x54 */ int    id;               // the map effect's number
    /* 0x58 */ float  alpha_max;
    /* 0x5c */ int    _5c;
};
static_assert(sizeof(EffDrip) == 0x60, "EffDrip: EffDripMgrAddDrip news 0x60");
static_assert(offsetof(EffDrip, pos) == 0x10, "EffDrip.pos");
static_assert(offsetof(EffDrip, alpha) == 0x30, "EffDrip.alpha");
static_assert(offsetof(EffDrip, active) == 0x34, "EffDrip.active");
static_assert(offsetof(EffDrip, base) == 0x40, "EffDrip.base");
static_assert(offsetof(EffDrip, alpha_max) == 0x58, "EffDrip.alpha_max");

class EffDripFloor                      /* 0x20 */
{
public:
    EffDripFloor(const float *pos, int type, int id);   // FF3 0x00186360 (EffDripFloorCtor)
    ~EffDripFloor();                    // FF3 0x00186380 (EffDripFloorDtor): empty
    int  IsNear(const EffDrip *drip) const;     // FF3 0x00186388 (EffDripFloorIsNear): XZ distance < 1.0

    /* 0x00 */ sceVu0FVECTOR pos;
    /* 0x10 */ int    type;             // the splash type 0..3
    /* 0x14 */ int    id;
    /* 0x18 */ int    _18[2];
};
static_assert(sizeof(EffDripFloor) == 0x20, "EffDripFloor: EffDripMgrAddFloor news 0x20");
static_assert(offsetof(EffDripFloor, type) == 0x10, "EffDripFloor.type");
static_assert(offsetof(EffDripFloor, id) == 0x14, "EffDripFloor.id");

#define EFF_DRIP_MAX 50

// layout: host -- two arrays of 50 pointers (EE 400 bytes: drips at 0,
// floor points at 0xc8).
class EffDripMgr
{
public:
    EffDripMgr();                       // FF3 0x00186430 (EffDripMgrCtor)
    ~EffDripMgr();                      // FF3 0x001864b0 (EffDripMgrDtor; twin 0x00186480 EffDripMgrDtorTwin)
    void Activate(EffDrip *drip);       // FF3 0x001864e0 (EffDripMgrActivate)
    void Step();                        // FF3 0x00186580 (EffDripMgrStep)
    void Draw();                        // FF3 0x001865f0 (EffDripMgrDraw)
    void Main();                        // FF3 0x001866b0 (EffDripMgrMain)
    int  AddDrip(const float *pos, int kind, int id);   // FF3 0x001866d8 (EffDripMgrAddDrip): 0 when full
    void CutDrip(int id);               // FF3 0x00186770 (EffDripMgrCutDrip): id < 0 all
    int  AddFloor(const float *pos, int type, int id);  // FF3 0x001867f0 (EffDripMgrAddFloor): 0 when full
    void CutFloor(int id);              // FF3 0x00186888 (EffDripMgrCutFloor): id < 0 all

    EffDrip      *drip[EFF_DRIP_MAX];   // EE +0x00
    EffDripFloor *floor[EFF_DRIP_MAX];  // EE +0xc8
};

void  EffDripInit(void);                // FF3 0x00186908: InitEffects
void  EffDripRelease(void);             // FF3 0x00186940: EffectsReleaseAll
void  EffDripMain(void);                // FF3 0x00186980: EffectInPhoto
void  EffDripReq(float *pos, int kind, int no);     // FF3 0x00186998: map effects 300..303
void  EffDripCut(int no);               // FF3 0x001869d8
void  EffDripFloorReq(float *pos, int type, int no);    // FF3 0x00186a08: map effects 400..403
void  EffDripFloorCut(int no);          // FF3 0x00186a48
int  *EffSparkColGet(void);             // FF3 0x00186a78: eff_drip_col[0]

#endif /* _GRAPHICS_EFFECT_EFFECT_OTH_H */
