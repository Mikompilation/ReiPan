/* ==========================================================================
 *  graphics/effect/effect_ene.h
 *
 *  The ghost-damage effects (FF2 effect_ene.c; FF3 0x15b658..0x1622d0): the
 *  damage overlay over a photographed ghost (ENDMG1, one record per enemy
 *  slot), the spirit strands it raises (ENDMG2 + NEW_PERTICLE), the spirit
 *  particle bursts (two SingleLinkLists of ENEDMG_P_WRK), the large-hit flash
 *  (ENE_DMG_LARGE_HIT_CTRL) and the hit-effect controller (ENE_HIT_EFFECT_CTRL),
 *  plus the CEffectFinder singleton at the end of the file.
 *
 *  FF3 differences from FF2 so far: 16 ENDMG1 records (one per ENE_WRK slot;
 *  FF2 10), no SWORD_LINE / eneseal_status, the burst cell is 0x50 bytes with
 *  its particles and tail on the effect heap (FF2 inlined 48 particles in
 *  0xf50), the strand colour tables are .rodata, and the rotations come from
 *  GetTrgtRotType2Neg.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_ENE_H
#define _GRAPHICS_EFFECT_EFFECT_ENE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR, sceVu0FMATRIX
#include "common/SingleLinkList.h"      // SINGLE_LINK_LIST

// The flash EneHitEffectReq plays (FF3 0..0x19, larger ones become 0). As in
// FF2's ENE_HIT_EFFECT_LABEL, the camera-function labels come in threes:
// plain, shutter chance (+1), fatal frame (+2); LensHitEffectReq picks them.
#define ENE_HIT_EFFECT_NUM  0x1a

// ---- the damage overlay -------------------------------------------------

#define ENDMG1_NUM 16                   // FF2 10: one per ENE_WRK slot

// One ghost's overlay: two camera-facing quads (slots 0 / 1; the arrays hold
// four as in FF2), each with its own flow / cnt ramp. Quad 0 holds 15 frames,
// then blooms over 40 while fading from 80 and raises the strands
// (SetEneDmgEffect2) as it starts; quad 1 holds 15, then spins and fades from
// 64 over 40, and its end clears the record. Only quad 1 is drawn.
struct ENDMG1                           /* 0x90 */
{
    /* 0x00 */ sceVu0FVECTOR wbpos[2];  // where each quad sits: the ghost's EfGetMpos(no, 0) when armed
    /* 0x20 */ float scw[4];            // quad width scale
    /* 0x30 */ float sch[4];            // quad height scale
    /* 0x40 */ float rot_z[4];          // spin (quad 1: 2 degrees a frame)
    /* 0x50 */ float alp[4];            // alpha
    /* 0x60 */ int   cnt[4];            // frames in the flow state
    /* 0x70 */ int   flow[4];           // 0 hold, 1 ramp, 2 done
    /* 0x80 */ int   enedmg_no;         // the ene_wrk slot
    /* 0x84 */ int   enedmg1_flg;       // 0 off, 1 armed, 2 running
    /* 0x88 */ int   enedmg_chance;     // 0 plain, 1 shutter chance, 2 fatal frame: colour row
    /* 0x8c */ int   dummy;
};
static_assert(sizeof(ENDMG1) == 0x90, "ENDMG1 is 0x90 bytes");
static_assert(offsetof(ENDMG1, scw) == 0x20, "ENDMG1.scw");
static_assert(offsetof(ENDMG1, rot_z) == 0x40, "ENDMG1.rot_z");
static_assert(offsetof(ENDMG1, alp) == 0x50, "ENDMG1.alp");
static_assert(offsetof(ENDMG1, cnt) == 0x60, "ENDMG1.cnt");
static_assert(offsetof(ENDMG1, flow) == 0x70, "ENDMG1.flow");
static_assert(offsetof(ENDMG1, enedmg_no) == 0x80, "ENDMG1.enedmg_no");
static_assert(offsetof(ENDMG1, enedmg1_flg) == 0x84, "ENDMG1.enedmg1_flg");
static_assert(offsetof(ENDMG1, enedmg_chance) == 0x88, "ENDMG1.enedmg_chance");

// The strand pass: one record for the scene.
struct ENDMG2                           /* 0xc */
{
    /* 0x0 */ int enedmg_no;
    /* 0x4 */ int enedmg2_flg;          // 0 off, 1 armed (SetEneDmgEffect2), 2 running
    /* 0x8 */ int enedmg_chance;
};
static_assert(sizeof(ENDMG2) == 0xc, "ENDMG2 is 0xc bytes");
static_assert(offsetof(ENDMG2, enedmg2_flg) == 0x4, "ENDMG2.enedmg2_flg");

// One strand's trail: a ring of the last twelve world matrices and positions.
struct TAIL_DMG2_DAT                    /* 0x3c0 */
{
    /* 0x000 */ sceVu0FMATRIX wmtx[12];
    /* 0x300 */ sceVu0FVECTOR opos[12];
};
static_assert(sizeof(TAIL_DMG2_DAT) == 0x3c0, "TAIL_DMG2_DAT is 0x3c0 bytes");
static_assert(offsetof(TAIL_DMG2_DAT, opos) == 0x300, "TAIL_DMG2_DAT.opos");

// One spirit strand: flies from the ghost to a point before the player (or
// the lens) along a spiral; cnt is its clock, 3 .. 180.
// layout: host -- oposp / wmtxp are EE pointers (+0x10 / +0x14); the fields
// after them follow at EE +0x18.. (the EE record is 0x40).
struct NEW_PERTICLE
{
    /* 0x00 */ sceVu0FVECTOR npos;      // head position
    /* 0x10 */ sceVu0FVECTOR *oposp;    // its TAIL_DMG2_DAT.opos
    /* 0x14 */ sceVu0FMATRIX *wmtxp;    // its TAIL_DMG2_DAT.wmtx
    /* 0x18 */ int   time;              // 1 while live (compared as a float)
    /* 0x1c */ short top;               // newest ring slot
    /* 0x1e */ short num;               // ring slots in use
    /* 0x20 */ float xp;                // clock speed, 0.9..1.2
    /* 0x24 */ float xm;
    /* 0x28 */ float rot;               // roll about the line, -pi..pi
    /* 0x2c */ float rotp;              // extra roll over the flight, -0.5..1.5 degrees
    /* 0x30 */ float x;                 // distance along the line
    /* 0x34 */ float y;                 // sideways swing
    /* 0x38 */ float n;                 // swing amplitude
    /* 0x3c */ float cnt;               // the clock
};
static_assert(offsetof(NEW_PERTICLE, oposp) == 0x10, "NEW_PERTICLE.oposp");

#define NEW_PERTICLE_NUM 48             // at most (80 * 40 - 400) / 70 + 4 = 44 are used

// ---- the particle bursts ---------------------------------------------------

#define ENEDMG_PARTICLE_MAX 48

// One particle (FF2's). It spirals: rrad is its radius about rpos at angle
// rrot; racc / rbrk grow and brake the radius so a burst blooms; on the
// flight home cnt_f is the clock (0.556 .. 90) and rrad winds back from
// rrad_max. Center is where it was last drawn.
struct ENEDMG_PARTICLE_ONE              /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR Center;
    /* 0x10 */ sceVu0FVECTOR rpos;
    /* 0x20 */ float rrad;
    /* 0x24 */ float rrot;
    /* 0x28 */ float racc;
    /* 0x2c */ float rbrk;
    /* 0x30 */ float ralp;
    /* 0x34 */ float rrad_max;
    /* 0x38 */ float cnt_f;
    /* 0x3c */ float cnt_spd;
    /* 0x40 */ float rot_spd;
    /* 0x44 */ int   anm_count;         // sprite phase, clamped at 100
    /* 0x48 */ float AlphaRate;
    /* 0x4c */ int   _4c;
};
static_assert(sizeof(ENEDMG_PARTICLE_ONE) == 0x50, "ENEDMG_PARTICLE_ONE is 0x50 bytes");
static_assert(offsetof(ENEDMG_PARTICLE_ONE, rrad) == 0x20, "ENEDMG_PARTICLE_ONE.rrad");
static_assert(offsetof(ENEDMG_PARTICLE_ONE, ralp) == 0x30, "ENEDMG_PARTICLE_ONE.ralp");
static_assert(offsetof(ENEDMG_PARTICLE_ONE, cnt_f) == 0x38, "ENEDMG_PARTICLE_ONE.cnt_f");
static_assert(offsetof(ENEDMG_PARTICLE_ONE, anm_count) == 0x44, "ENEDMG_PARTICLE_ONE.anm_count");
static_assert(offsetof(ENEDMG_PARTICLE_ONE, AlphaRate) == 0x48, "ENEDMG_PARTICLE_ONE.AlphaRate");

// The comet tail behind a particle flying home (FF2's): a ring of the last
// ten world matrices and positions, Top the newest slot.
struct ENEDMG_P_TAIL_WRK                /* 0x330 */
{
    /* 0x000 */ sceVu0FMATRIX LwMatrix[10];
    /* 0x280 */ sceVu0FVECTOR OldPos[10];
    /* 0x320 */ int NumMax;
    /* 0x324 */ int Num;
    /* 0x328 */ int Top;
    /* 0x32c */ int _32c;
};
static_assert(sizeof(ENEDMG_P_TAIL_WRK) == 0x330, "ENEDMG_P_TAIL_WRK is 0x330 bytes");
static_assert(offsetof(ENEDMG_P_TAIL_WRK, OldPos) == 0x280, "ENEDMG_P_TAIL_WRK.OldPos");
static_assert(offsetof(ENEDMG_P_TAIL_WRK, NumMax) == 0x320, "ENEDMG_P_TAIL_WRK.NumMax");
static_assert(offsetof(ENEDMG_P_TAIL_WRK, Top) == 0x328, "ENEDMG_P_TAIL_WRK.Top");

// One burst, a SingleLinkList cell body. FF3 keeps the particles and tails on
// the effect heap (EFFECT_MALLOC(Num * 0x50 / Num * 0x330), EneDmgParticleReq
// 0x15dcc8; the effect-end burst 0x15de48 has no tails).
// layout: host -- pEndPos, pTailWrk and particle are EE pointers (+0x20..+0x28).
struct ENEDMG_P_WRK                     /* 0x50 on the EE */
{
    /* 0x00 */ sceVu0FVECTOR StartPos;  // the ghost
    /* 0x10 */ sceVu0FVECTOR EndPos;
    /* 0x20 */ float (*pEndPos)[4];     // where the particles fly to (EneDmgParticleEndPosSet while Type 0)
    /* 0x24 */ ENEDMG_P_TAIL_WRK *pTailWrk;     // EFFECT_MALLOC'd, Num of them
    /* 0x28 */ ENEDMG_PARTICLE_ONE *particle;   // EFFECT_MALLOC'd, Num of them
    /* 0x2c */ int   Num;               // 0..48
    /* 0x30 */ int   flow;              // 1 at the request
    /* 0x34 */ int   cnt;
    /* 0x38 */ float SpeedRate;
    /* 0x3c */ int   SuctionFlg;
    /* 0x40 */ int   Type;              // 0 damage burst (EneParticleList), 1 effect-end burst (EffectEndParticleList)
    /* 0x44 */ int   EffectType;
    /* 0x48 */ u_char _48[8];
};
static_assert(offsetof(ENEDMG_P_WRK, pEndPos) == 0x20, "ENEDMG_P_WRK.pEndPos");

// A burst request (FF2's ENE_DMG_PARTICLE_REQ; 0x30 on the EE, a caller's
// stack local). FF3's distances are FF2's / 25: DistPE 20..80 maps the
// flight speed (FF2 500..2000).
// layout: host -- pEndPos is an EE pointer (+0x10); the fields after it
// follow at EE +0x14..
struct ENE_DMG_PARTICLE_REQ
{
    /* 0x00 */ sceVu0FVECTOR StartPos;  // the ghost
    /* 0x10 */ float (*pEndPos)[4];     // where a suction burst flies (the lens point)
    /* 0x14 */ int   ParticleNum;       // clamped to 0..48
    /* 0x18 */ float DistPE;            // player to ghost
    /* 0x1c */ int   SuctionFlg;        // 1: fly home after the bloom
    /* 0x20 */ int   EffectType;        // the effect-end burst's colour, 0..4
};
static_assert(offsetof(ENE_DMG_PARTICLE_REQ, pEndPos) == 0x10, "ENE_DMG_PARTICLE_REQ.pEndPos");

// ---- the large-hit flash and the hit-effect controller ---------------------

#define ENE_DMG_LARGE_HIT_PRESET_NUM    50      // FF2 38: FF3 adds two three-grade sets (38..49)
#define ENE_DMG_BLUR_PRESET_NUM         26      // FF2 20: one per hit-effect label

// One authored flash (FF2's; .data 0x2f6b68.., one per 0x60, table 0x2f8648).
// The geometry fields are fixed point and EneDmgLargeHitInit divides each
// down: Size and LastScale by 100, MoveDist and Distance by 250 (FF2 10),
// RotVal is degrees + 3600. The five AlphaBlend fields are the GS ALPHA
// register's A / B / C / D selectors and FIX.
struct ENE_DMG_LARGE_HIT_PARAMETER      /* 0x5c (0x60 apart in the ROM) */
{
    /* 0x00 */ int CenterRgba[4];       // the fan's hub colour
    /* 0x10 */ int OutsideRgba[4];      // its rim colour
    /* 0x20 */ int VertexNumW;          // the rim's vertices across
    /* 0x24 */ int VertexNumH;          // and down
    /* 0x28 */ int Size;                // / 100: the sampled size
    /* 0x2c */ int LastScale;           // / 100: the drawn scale at AllFrame
    /* 0x30 */ int AllFrame;            // lifetime in frames
    /* 0x34 */ int MoveDist;            // / 250: drift along the view
    /* 0x38 */ int Distance;            // / 250: distance before the camera
    /* 0x3c */ int RotVal;              // degrees + 3600 over the lifetime
    /* 0x40 */ int CaptureInterval;     // slot 0: frames between frame grabs
    /* 0x44 */ int CaptureNumber;       // slot 0: grabs after the first
    /* 0x48 */ int AlphaBlendA;
    /* 0x4c */ int AlphaBlendB;
    /* 0x50 */ int AlphaBlendC;
    /* 0x54 */ int AlphaBlendD;
    /* 0x58 */ int AlphaBlendFIX;
};
static_assert(sizeof(ENE_DMG_LARGE_HIT_PARAMETER) == 0x5c, "ENE_DMG_LARGE_HIT_PARAMETER is 0x5c bytes");
static_assert(offsetof(ENE_DMG_LARGE_HIT_PARAMETER, VertexNumW) == 0x20, "ENE_DMG_LARGE_HIT_PARAMETER.VertexNumW");
static_assert(offsetof(ENE_DMG_LARGE_HIT_PARAMETER, RotVal) == 0x3c, "ENE_DMG_LARGE_HIT_PARAMETER.RotVal");
static_assert(offsetof(ENE_DMG_LARGE_HIT_PARAMETER, AlphaBlendA) == 0x48, "ENE_DMG_LARGE_HIT_PARAMETER.AlphaBlendA");
static_assert(offsetof(ENE_DMG_LARGE_HIT_PARAMETER, AlphaBlendFIX) == 0x58, "ENE_DMG_LARGE_HIT_PARAMETER.AlphaBlendFIX");

// The screen half of a hit (FF2's; .data 0x2f7e28.., table 0x2f8710): one
// in / keep / out ramp from StartFrame driving the blur and the contrast,
// a camera shake and a rumble at their frames. Each part has its own switch.
struct ENE_DMG_BLUR_CONTRAST_PARAMETER  /* 0x50 */
{
    /* 0x00 */ int StartFrame;
    /* 0x04 */ int InTime;
    /* 0x08 */ int KeepTime;
    /* 0x0c */ int OutTime;
    /* 0x10 */ int MinBlurScale;        // thousandths (1000: no zoom)
    /* 0x14 */ int MaxBlurScale;
    /* 0x18 */ int MinBlurAlpha;
    /* 0x1c */ int MaxBlurAlpha;
    /* 0x20 */ int MinBlurRot;          // tenths of a degree about 1800
    /* 0x24 */ int MaxBlurRot;
    /* 0x28 */ int BlurOnFlg;
    /* 0x2c */ int MinContrastColor;
    /* 0x30 */ int MaxContrastColor;
    /* 0x34 */ int MinContrastAlpha;
    /* 0x38 */ int MaxContrastAlpha;
    /* 0x3c */ int ContrastOnFlg;
    /* 0x40 */ int CameraShakeOnFlg;
    /* 0x44 */ int CameraShakeFrame;
    /* 0x48 */ int PadVibrateOnFlg;
    /* 0x4c */ int PadVibrateFrame;
};
static_assert(sizeof(ENE_DMG_BLUR_CONTRAST_PARAMETER) == 0x50, "ENE_DMG_BLUR_CONTRAST_PARAMETER is 0x50 bytes");
static_assert(offsetof(ENE_DMG_BLUR_CONTRAST_PARAMETER, BlurOnFlg) == 0x28, "ENE_DMG_BLUR_CONTRAST_PARAMETER.BlurOnFlg");
static_assert(offsetof(ENE_DMG_BLUR_CONTRAST_PARAMETER, ContrastOnFlg) == 0x3c, "ENE_DMG_BLUR_CONTRAST_PARAMETER.ContrastOnFlg");
static_assert(offsetof(ENE_DMG_BLUR_CONTRAST_PARAMETER, PadVibrateFrame) == 0x4c, "ENE_DMG_BLUR_CONTRAST_PARAMETER.PadVibrateFrame");

// A live flash, unpacked from a preset by EneDmgLargeHitInit (FF2's).
// AlphaBlend is the GS ALPHA register (u_long: 64 bits on both sides).
struct ENE_DMG_LARGE_HIT                /* 0x60 */
{
    /* 0x00 */ sceVu0FVECTOR CenterPos; // the fan's hub in the billboard's space (z drifts)
    /* 0x10 */ u_long AlphaBlend;
    /* 0x18 */ float  Scale;            // the sampled size
    /* 0x1c */ float  LastScale;
    /* 0x20 */ float  MoveDist;
    /* 0x24 */ float  Distance;
    /* 0x28 */ float  RotVal;           // radians over the lifetime
    /* 0x2c */ u_int  VertexNumW;
    /* 0x30 */ u_int  VertexNumH;
    /* 0x34 */ u_int  NowFrame;
    /* 0x38 */ u_int  AllFrame;
    /* 0x3c */ u_int  CaptureInterval;
    /* 0x40 */ u_int  CaptureNumber;
    /* 0x44 */ u_char CenterRgba[4];
    /* 0x48 */ u_char OutsideRgba[4];
    /* 0x4c */ u_int  InitCenterAlpha;
    /* 0x50 */ u_int  InitOutsideAlpha;
    /* 0x54 */ int    Delay;            // 1 at the request: the flash sits out its first frame
};
static_assert(sizeof(ENE_DMG_LARGE_HIT) == 0x60, "ENE_DMG_LARGE_HIT is 0x60 bytes");
static_assert(offsetof(ENE_DMG_LARGE_HIT, AlphaBlend) == 0x10, "ENE_DMG_LARGE_HIT.AlphaBlend");
static_assert(offsetof(ENE_DMG_LARGE_HIT, RotVal) == 0x28, "ENE_DMG_LARGE_HIT.RotVal");
static_assert(offsetof(ENE_DMG_LARGE_HIT, NowFrame) == 0x34, "ENE_DMG_LARGE_HIT.NowFrame");
static_assert(offsetof(ENE_DMG_LARGE_HIT, CaptureNumber) == 0x40, "ENE_DMG_LARGE_HIT.CaptureNumber");
static_assert(offsetof(ENE_DMG_LARGE_HIT, CenterRgba) == 0x44, "ENE_DMG_LARGE_HIT.CenterRgba");
static_assert(offsetof(ENE_DMG_LARGE_HIT, InitCenterAlpha) == 0x4c, "ENE_DMG_LARGE_HIT.InitCenterAlpha");
static_assert(offsetof(ENE_DMG_LARGE_HIT, Delay) == 0x54, "ENE_DMG_LARGE_HIT.Delay");

// Two flash slots, so an A / B preset pair runs at once.
struct ENE_DMG_LARGE_HIT_CTRL           /* 0xd0 */
{
    /* 0x00 */ ENE_DMG_LARGE_HIT Work[2];
    /* 0xc0 */ int    Status[2];        // 0 off, 1 running
    /* 0xc8 */ u_char _c8[8];
};
static_assert(sizeof(ENE_DMG_LARGE_HIT_CTRL) == 0xd0, "ENE_DMG_LARGE_HIT_CTRL is 0xd0 bytes");
static_assert(offsetof(ENE_DMG_LARGE_HIT_CTRL, Status) == 0xc0, "ENE_DMG_LARGE_HIT_CTRL.Status");

// The camera hit's screen half (FF2 ENE_HIT_EFFECT_CTRL; 0x40 in FF3, set by
// EneHitEffectReq, stepped by EneHitEffectMain): Flow 0 idle, 1 the SEC0
// arming delay, 2 the ramp, 3 the reset.
// layout: host -- pBlurContrast is an EE pointer (+0x10); the fields after it
// follow at EE +0x14..
struct ENE_HIT_EFFECT_CTRL              /* 0x40 on the EE */
{
    /* 0x00 */ sceVu0FVECTOR EneMposP0; // the ghost: the blur's centre
    /* 0x10 */ ENE_DMG_BLUR_CONTRAST_PARAMETER *pBlurContrast;
    /* 0x14 */ int    Flow;
    /* 0x18 */ int    EneWrkNo;
    /* 0x1c */ int    HitEffecType;     // the label, 0..0x19 (FF2's spelling)
    /* 0x20 */ int    Counter;
    /* 0x24 */ int    ContrastColor;
    /* 0x28 */ int    ContrastAlpha;
    /* 0x2c */ int    BlurScale;
    /* 0x30 */ int    BlurRot;
    /* 0x34 */ u_char BlurAlpha;
};
static_assert(offsetof(ENE_HIT_EFFECT_CTRL, pBlurContrast) == 0x10, "ENE_HIT_EFFECT_CTRL.pBlurContrast");

// The damage sprite's ramp (FF2 ENE_DMG_EFF, EneDmgMain). FF3 has no
// EneDmgScreenEffectReq: nothing but EneDmgMain writes flow, so it idles.
struct ENE_DMG_EFF                      /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR MposP0;
    /* 0x10 */ u_char flow;             // 0 / 8 idle, 1 arming, 2 maxima, 3..7 the ramp legs
    /* 0x11 */ u_char alp;
    /* 0x12 */ u_char chance;
    /* 0x14 */ int    scl;              // thousandths
    /* 0x18 */ int    rot;              // tenths of a degree
    /* 0x1c */ int    cntcol;
    /* 0x20 */ int    cntalp;
    /* 0x24 */ u_int  cnt;
    /* 0x28 */ float  x;
    /* 0x2c */ float  y;
    /* 0x30 */ float  dist;             // 0..120 in FF3's ramp (FF2 0..3000)
    /* 0x34 */ int    almx;
    /* 0x38 */ int    scmx;
    /* 0x3c */ int    rtmx;
    /* 0x40 */ int    ccmx;
    /* 0x44 */ int    camx;
    /* 0x48 */ int    DmgType;          // 3 / 4 raise the large hits and shake
    /* 0x4c */ int    _4c;
};
static_assert(sizeof(ENE_DMG_EFF) == 0x50, "ENE_DMG_EFF is 0x50 bytes");
static_assert(offsetof(ENE_DMG_EFF, flow) == 0x10, "ENE_DMG_EFF.flow");
static_assert(offsetof(ENE_DMG_EFF, scl) == 0x14, "ENE_DMG_EFF.scl");
static_assert(offsetof(ENE_DMG_EFF, cnt) == 0x24, "ENE_DMG_EFF.cnt");
static_assert(offsetof(ENE_DMG_EFF, dist) == 0x30, "ENE_DMG_EFF.dist");
static_assert(offsetof(ENE_DMG_EFF, camx) == 0x44, "ENE_DMG_EFF.camx");
static_assert(offsetof(ENE_DMG_EFF, DmgType) == 0x48, "ENE_DMG_EFF.DmgType");

// ---- globals -----------------------------------------------------------------

extern ENDMG1 enedmg1[ENDMG1_NUM];                  // bss 0x399fa0
extern ENDMG2 enedmg2;                              // bss 0x39a8a0
extern ENE_DMG_LARGE_HIT_CTRL EneDmgLargeHitCtrl;   // bss 0x39a8b0
extern ENE_HIT_EFFECT_CTRL EneHitEffectCtrl;        // bss 0x39a980
extern ENE_DMG_EFF ene_dmg_eff;                     // bss 0x3a69d0
extern SINGLE_LINK_LIST EneParticleList;            // bss 0x3a6a20
extern SINGLE_LINK_LIST EffectEndParticleList;      // bss 0x3a6a30
extern int enedmg_status;               // sbss 0x372f28: 1 strands in flight, 2 the last one landed
extern int EneDmgParticleSuctionNum;    // sbss 0x372f2c: particles that reached the tray this frame

// ---- the random timers (FF3-only, unreferenced) --------------------------------

// The block at +0x240 of the object EffEneTimerRearm takes. Nothing in FF3
// calls it, so the owner is unknown; only these fields are known.
struct EFF_ENE_TIMER                    /* EE: from +0x240 of its owner */
{
    /* 0x00 */ u_char _00[0x50];
    /* 0x50 */ int    time0;            // +0x290: the period (4 + rand 0)
    /* 0x54 */ int    time1;            // +0x294: 60 + rand 0
    /* 0x58 */ int    cnt;              // +0x298: re-armed when <= 0, to time0
    /* 0x5c */ int    time3;            // +0x29c: 80 + rand 20
    /* 0x60 */ float  a;                // +0x2a0: -0.2 (mode 0) or 0.13 (mode 1)
    /* 0x64 */ float  b;                // +0x2a4: the other one
};
static_assert(sizeof(EFF_ENE_TIMER) == 0x68, "EFF_ENE_TIMER size");
static_assert(offsetof(EFF_ENE_TIMER, time0) == 0x50, "EFF_ENE_TIMER.time0");
static_assert(offsetof(EFF_ENE_TIMER, cnt) == 0x58, "EFF_ENE_TIMER.cnt");
static_assert(offsetof(EFF_ENE_TIMER, a) == 0x60, "EFF_ENE_TIMER.a");
static_assert(offsetof(EFF_ENE_TIMER, b) == 0x64, "EFF_ENE_TIMER.b");

struct EFF_ENE_TIMER_OWNER              /* EE: at least 0x2a8 */
{
    /* 0x000 */ u_char        _000[0x240];
    /* 0x240 */ EFF_ENE_TIMER tm;
};
static_assert(offsetof(EFF_ENE_TIMER_OWNER, tm) == 0x240, "EFF_ENE_TIMER_OWNER.tm");

// ---- the finder sparks (FF3-only, unreferenced) --------------------------------

// A CEffectFinder is a set of 2D sprite particles at a screen point; the
// manager holds 20 of them and drops each once it reports its end. FF3
// keeps the manager as a singleton (EffectFinderCreate / Release / Main /
// Req / DeleteAll) but nothing calls those, so none of this ever runs.

// One particle (0x28, operator_new'd by CEffectFinderSpark's constructor,
// which fills it; no constructor code of its own).
struct EFF_FINDER_PTCL                  /* 0x28 */
{
    /* 0x00 */ float  x;                // screen position, the sprite's centre
    /* 0x04 */ float  y;
    /* 0x08 */ float  rot;              // DISP_SPRT.rot
    /* 0x0c */ float  vx;
    /* 0x10 */ float  vy;
    /* 0x14 */ float  vrot;
    /* 0x18 */ float  scw;              // DISP_SPRT.scw / sch
    /* 0x1c */ float  sch;
    /* 0x20 */ short  sprite;           // effdat index, < 0: not drawn
    /* 0x22 */ short  alpha;            // <= 0: not drawn (and over for the spark)
    /* 0x24 */ u_char r;
    /* 0x25 */ u_char g;
    /* 0x26 */ u_char b;
    /* 0x27 */ u_char blend;            // index into the ALPHA table (rdata 0x354ae0)
};
static_assert(sizeof(EFF_FINDER_PTCL) == 0x28, "EFF_FINDER_PTCL size");
static_assert(offsetof(EFF_FINDER_PTCL, vx) == 0x0c, "EFF_FINDER_PTCL.vx");
static_assert(offsetof(EFF_FINDER_PTCL, scw) == 0x18, "EFF_FINDER_PTCL.scw");
static_assert(offsetof(EFF_FINDER_PTCL, sprite) == 0x20, "EFF_FINDER_PTCL.sprite");
static_assert(offsetof(EFF_FINDER_PTCL, r) == 0x24, "EFF_FINDER_PTCL.r");
static_assert(offsetof(EFF_FINDER_PTCL, blend) == 0x27, "EFF_FINDER_PTCL.blend");

// The particle pointers (operator_new(8); the array new[]).
// layout: host -- EE: ptcl +0x00, num +0x04 (8 bytes).
struct EFF_FINDER_PTCL_LIST
{
    EFF_FINDER_PTCL **ptcl;
    int               num;
};

// The base effect: moves its particles and draws them; ends at once.
// layout: host -- the vptr and list are 4 bytes on the EE (0x18 in all:
// x +0x04, y +0x08, z +0x0c, list +0x10, num +0x14); nothing reads them by
// offset.
class CEffectFinder
{
public:
    // Inline in EffectFinderMgrReq (kind 1) and CEffectFinderSpark's
    // constructor: the position, no particles.
    CEffectFinder(float x, float y, float z) : x(x), y(y), z(z), list(nullptr), num(0) {}
    virtual ~CEffectFinder();           // FF3 0x002b0b98 (0x002b0ba8 EffectFinderDtorDelete the deleting one)
    virtual void Update();              // FF3 0x002b0bc8
    virtual void Draw();                // FF3 0x002b0c40
    virtual int  IsEnd();               // FF3 0x002b0ca8

protected:
    float                 x;
    float                 y;
    float                 z;            // stored, never read
    EFF_FINDER_PTCL_LIST *list;
    int                   num;          // the spark's particle count (the base uses list->num)
};

// A spark: 20..29 particles flying out of (x, y), slowing, shrinking and
// fading by 0.9 a frame.
// layout: host -- as CEffectFinder.
class CEffectFinderSpark : public CEffectFinder
{
public:
    CEffectFinderSpark(float x, float y, float z);  // FF3 0x002b0cb0
    ~CEffectFinderSpark();              // FF3 0x002b0ff0 (0x002b10a8 EffectFinderSparkDtorDelete the deleting one)
    void Update();                      // FF3 0x002b1160
    int  IsEnd();                       // FF3 0x002b1238
};

#define EFFECT_FINDER_NUM 20

// The 20 slots (operator_new(0x50)).
// layout: host -- 20 pointers, 0x50 bytes on the EE.
class CEffectFinderMgr
{
public:
    CEffectFinderMgr();                 // FF3 0x00161f38
    ~CEffectFinderMgr();                // FF3 0x00161f78 (0x00161f60 EffectFinderMgrDtorBase the base-object twin)
    void Update();                      // FF3 0x00161f90
    void Draw();                        // FF3 0x00162020
    void Main();                        // FF3 0x00162078
    int  Req(float x, float y, float z, int kind);  // FF3 0x001620a0
    void DeleteAll();                   // FF3 0x00162180

    CEffectFinder *slot[EFFECT_FINDER_NUM];
};

// ---- functions -----------------------------------------------------------------

void EneHitEffectReq(int ene_no, float *mpos, int label);    // FF3 0x0015d9a8
void EneDmgLargeHitReq(int preset);                          // FF3 0x00160108: one flash, preset 0..49
ENE_DMG_LARGE_HIT_PARAMETER *EffEneDmgLargeHitParameterPtrGet(int label);          // FF3 0x00161838
ENE_DMG_BLUR_CONTRAST_PARAMETER *EffEneDmgLargeHitBlurParameterPtrGet(int label);  // FF3 0x00161850

// The enemy aura (EnemyDrawOne): EneAuraFrameBegin points the GS at the
// aura work frame before the model draws, EneAuraFrameEnd restores the
// display frame, EneAuraDraw copies the work frame back over the screen per
// layer of fx (EneAuraDatGet's) at alpha, wobbling with time.
void EneAuraFrameBegin(void);                                       // FF3 0x00161868
struct ENE_AURA_DAT;                    // ingame/enemy/enemy.h
void EneAuraFrameEnd(ENE_AURA_DAT *fx);                             // FF3 0x001619e8
void EneAuraDraw(ENE_AURA_DAT *fx, u_char alpha, float time);       // FF3 0x00161bb8

// Unreferenced in FF3 (see above).
int  EffEneTimerRearm(EFF_ENE_TIMER_OWNER *obj, int mode);          // FF3 0x00161af0
void EffectFinderCreate(void);                                      // FF3 0x001621d8
void EffectFinderRelease(void);                                     // FF3 0x00162210
void EffectFinderMain(void);                                        // FF3 0x00162250
int  EffectFinderReq(float x, float y, float z, int kind);          // FF3 0x00162268: kind 0 a spark, 1 a bare base
void EffectFinderDeleteAll(void);                                   // FF3 0x001622a0

// Called by effect.c.
void InitEffectEne(void);                       // FF3 0x0015b658: InitEffects
void InitEffectEneEF(void);                     // FF3 0x0015b6d0: InitEffectsEF (empty)
void EffectEneReleaseAll(void);                 // FF3 0x0015b6d8: EffectsReleaseAll
void SetEneDmgEffect1_Sub(void);                // FF3 0x0015c528: EffectControl 1, the overlays
void SetEneDmgEffect2_Sub(void);                // FF3 0x0015ce80: EffectControl 1, the strands
void SetEneDmgEffect2(void);                    // FF3 0x0015d8c8: arms the strands
void EneHitEffectMain(void);                    // FF3 0x0015da20: EffectControl 0
void EneDmgMain(void);                          // FF3 0x0015e080: EffectControl 0
void EffectEndParticleMain(void);               // FF3 0x0015e780: EffectControl 0

// The burst list's own (called by the list walkers).
void EneDmgParticleEffectCut(SINGLE_LINK_LIST *pSLL, SLL_CELL *pCell);    // FF3 0x0015df18
void EneDmgParticleListCutAll(SINGLE_LINK_LIST *pSLL);                   // FF3 0x0015dfb0
void EneDmgParticle(SINGLE_LINK_LIST *pSLL, SLL_CELL *pCell);            // FF3 0x0015e878

// The burst requests: 1 when the burst is queued, 0 when the heap or the
// list is full.
int  EneDmgParticleEffectReq(const ENE_DMG_PARTICLE_REQ *pReq);     // FF3 0x0015dcc8: a camera hit's (roku_pzl.c)
int  EffectEndParticleEffectReq(const ENE_DMG_PARTICLE_REQ *pReq);  // FF3 0x0015de48: a ghost's end (ingame_effect.c)
// Retargets every damage burst (Type 0) at pEndPos (ingame_effect.c, per frame).
void EneDmgParticleEndPosSet(float (*pEndPos)[4]);                  // FF3 0x0015e008
int  IsActiveEneDmgParticle(void);      // FF3 0x0015e7f0: EneParticleList holds a cell (IgEffectIsEndParticleSuck)

// A gouraud ribbon of num groups (left, centre, right) of world points: two
// strips, centre r1 g1 b1 with alpha a tapering to the tail, edges r2 g2 b2
// at alpha 0.
void DrawNewPerticleSub(int num, sceVu0FVECTOR *pos, u_char r1, u_char g1, u_char b1,
                        u_char r2, u_char g2, u_char b2, u_char a);       // FF3 0x0015f948

// A triangle fan (vertex 0 the centre) through LocalWorldMat, centre colour
// at vertex 0 and the rim colour elsewhere, textured by pStq (FF2's large
// hit flash; FF3's haze draws the frame snapshot with it).
void EneDmgLargeHitMakePacket(sceVu0FVECTOR *pVertexBuf, int VertexNum, sceVu0FMATRIX LocalWorldMat,
                              u_char *pCenterRgba, u_char *pOutsideRgba, sceVu0FVECTOR *pStq,
                              u_long tex0);    // FF3 0x00161548

#endif /* _GRAPHICS_EFFECT_EFFECT_ENE_H */
