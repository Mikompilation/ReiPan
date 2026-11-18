/* ==========================================================================
 *  graphics/effect/effect_butterfly.h
 *
 *  The crimson butterflies (FF2 effect_butterfly.c; FF3 0x159188..0x15b348).
 *
 *  Two kinds, told apart by BUTTERFLY_DISP::MoveType. Type 0 is the ambient
 *  butterfly (IgEffectButterflyReq): it wanders on a randomised velocity in
 *  its own frame, lives 450 frames and fades in and out at the ends. Type 1
 *  is the guide (SET_BUTTERFLY / MOVE_BUTTERFLY / RELEASE_BUTTERFLY): it
 *  homes on a target, is addressable by id and lives until it is faded out.
 *  Both throw trail motes, a separate list (BUTTERFLY_PARTICLE).
 *
 *  FF3 against FF2: each butterfly owns a GameMotionModel (BUTTERFLY_MDL_PK4
 *  with BUTTERFLY_MOT_PK4) instead of an embedded ANI_CTRL, so a cell is 0x80
 *  bytes, not 0x2c0; distances and speeds are 1/25 of FF2's (FF3's world
 *  scale); the guide presets grew a word (0x30); the drawn alpha also fades
 *  with the XZ distance to the player (invisible closer than 10 units).
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_BUTTERFLY_H
#define _GRAPHICS_EFFECT_EFFECT_BUTTERFLY_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "common/SingleLinkList.h"      // SINGLE_LINK_LIST

class GameMotionModel;                  // graphics/game_model.h

// A guide-butterfly preset (data 0x2f6a60.., five of them, stride 0x30).
// Speed is 1/2500 unit per frame, TurnMax / BureLeftRight / BureUpDown are
// degrees (the wobble added to the heading when not locked on), the three
// LockOnInterval values the odds of locking on by distance (x 25) against
// NearDistance / FarDistance.
struct BUTTERFLY_TARGET_PARAMETER       /* 0x30 */
{
    /* 0x00 */ int Speed;
    /* 0x04 */ int TurnMax;
    /* 0x08 */ int BureLeftRight;
    /* 0x0c */ int BureUpDown;
    /* 0x10 */ int InTime;              // fade-in frames
    /* 0x14 */ int OutTime;             // fade-out frames
    /* 0x18 */ int LockOnIntervalNear;
    /* 0x1c */ int LockOnIntervalMiddle;
    /* 0x20 */ int LockOnIntervalFar;
    /* 0x24 */ int NearDistance;
    /* 0x28 */ int FarDistance;
    /* 0x2c */ int _2c;                 // FF3-only, 0 in all five, never read
};
static_assert(sizeof(BUTTERFLY_TARGET_PARAMETER) == 0x30, "BUTTERFLY_TARGET_PARAMETER is 0x30 bytes in FF3");
static_assert(offsetof(BUTTERFLY_TARGET_PARAMETER, InTime) == 0x10, "BUTTERFLY_TARGET_PARAMETER.InTime");
static_assert(offsetof(BUTTERFLY_TARGET_PARAMETER, OutTime) == 0x14, "BUTTERFLY_TARGET_PARAMETER.OutTime");
static_assert(offsetof(BUTTERFLY_TARGET_PARAMETER, LockOnIntervalNear) == 0x18, "BUTTERFLY_TARGET_PARAMETER.LockOnIntervalNear");
static_assert(offsetof(BUTTERFLY_TARGET_PARAMETER, NearDistance) == 0x24, "BUTTERFLY_TARGET_PARAMETER.NearDistance");
static_assert(offsetof(BUTTERFLY_TARGET_PARAMETER, FarDistance) == 0x28, "BUTTERFLY_TARGET_PARAMETER.FarDistance");

// One live butterfly (a list cell body; FF3 SingleLinkListInit(.., 0x80)).
// layout: host -- mdl is a pointer (EE 4 bytes at +0x50); the vectors
// before it keep the ROM's offsets, the words after it follow it.
struct BUTTERFLY_DISP                   /* 0x80 on the EE */
{
    /* 0x00 */ sceVu0FVECTOR    Position;
    /* 0x10 */ sceVu0FVECTOR    OldPosition;    // last frame's, for the facing
    /* 0x20 */ sceVu0FVECTOR    Target;         // MoveType 1 only
    /* 0x30 */ sceVu0FVECTOR    Rot;
    /* 0x40 */ sceVu0FVECTOR    Velocity;       // MoveType 0: own frame; 1: world
    /* 0x50 */ GameMotionModel *mdl;            // EffectButterflyMdlCreate's, deleted by ButterflyDelete
    /* 0x54 */ int              LifeTime;
    /* 0x58 */ int              MoveInterval;   // frames until the next steer
    /* 0x5c */ float            AlphaRate;
    /* 0x60 */ int              MoveType;       // 0 ambient, 1 guide
    /* 0x64 */ int              InTime;         // fade-in length, 0: done
    /* 0x68 */ int              OutTime;        // fade-out length, 0: not fading
    /* 0x6c */ int              Count;
    /* 0x70 */ int              Type;           // preset, MoveType 1 only
    /* 0x74 */ int              Id;             // MoveType 1 only
};
static_assert(offsetof(BUTTERFLY_DISP, Target) == 0x20, "BUTTERFLY_DISP.Target");
static_assert(offsetof(BUTTERFLY_DISP, Rot) == 0x30, "BUTTERFLY_DISP.Rot");
static_assert(offsetof(BUTTERFLY_DISP, Velocity) == 0x40, "BUTTERFLY_DISP.Velocity");
static_assert(offsetof(BUTTERFLY_DISP, mdl) == 0x50, "BUTTERFLY_DISP.mdl (EE +0x50)");
static_assert(offsetof(BUTTERFLY_DISP, LifeTime) == 0x50 + sizeof(GameMotionModel *), "BUTTERFLY_DISP.LifeTime (EE +0x54)");
static_assert(offsetof(BUTTERFLY_DISP, Id) == 0x70 + sizeof(GameMotionModel *), "BUTTERFLY_DISP.Id (EE +0x74)");

// One trail mote (a list cell body; FF3 SingleLinkListInit(.., 0x40)).
struct BUTTERFLY_PARTICLE               /* 0x40 */
{
    /* 0x00 */ sceVu0FVECTOR Position;
    /* 0x10 */ sceVu0FVECTOR Velocity;
    /* 0x20 */ sceVu0FVECTOR Acceleration;
    /* 0x30 */ int           LifeTime;      // 30 frames
    /* 0x34 */ float         AlphaRate;     // AlphaScale * LifeTime / 30
    /* 0x38 */ float         AlphaScale;    // FF3: the butterfly's drawn alpha when thrown
    /* 0x3c */ int           _3c;
};
static_assert(sizeof(BUTTERFLY_PARTICLE) == 0x40, "BUTTERFLY_PARTICLE is 0x40 bytes in FF3");
static_assert(offsetof(BUTTERFLY_PARTICLE, Acceleration) == 0x20, "BUTTERFLY_PARTICLE.Acceleration");
static_assert(offsetof(BUTTERFLY_PARTICLE, LifeTime) == 0x30, "BUTTERFLY_PARTICLE.LifeTime");
static_assert(offsetof(BUTTERFLY_PARTICLE, AlphaRate) == 0x34, "BUTTERFLY_PARTICLE.AlphaRate");
static_assert(offsetof(BUTTERFLY_PARTICLE, AlphaScale) == 0x38, "BUTTERFLY_PARTICLE.AlphaScale");

// layout: host -- a SINGLE_LINK_LIST (two pointers) each.
struct BUTTERFLY_DISP_CTRL              /* 0x10 on the EE */
{
    /* 0x0 */ SINGLE_LINK_LIST ButterflyList;
};

// layout: host -- as BUTTERFLY_DISP_CTRL.
struct BUTTERFLY_PARTICLE_CTRL          /* 0x10 on the EE */
{
    /* 0x0 */ SINGLE_LINK_LIST ParticleList;
};

extern BUTTERFLY_DISP_CTRL     ButterFlyDispCtrl;       // bss 0x399f80
extern BUTTERFLY_PARTICLE_CTRL ButterFlyParticleCtrl;   // bss 0x399f90

// Called by effect.c (InitEffects / EffectsReleaseAll / EffectInPhoto).
void EffectButterflyInit(void);                 // FF3 0x00159188
void EffectButterflyMain(void);                 // FF3 0x001594a8
void EffectButterflyAllCut(void);               // FF3 0x0015a980

// Ambient butterfly at pos: no id, no target, dies after 450 frames.
void EffectButterflyReq(float *pos);            // FF3 0x00159ff0: IgEffectButterflyReq
// Guide butterfly: homes on target with preset type, addressable by id.
void EffectButterflyReqTarget(int id, int type, float *pos, float *target);  // FF3 0x0015a1f8
void EffectButterflyFadeOut(int id);            // FF3 0x0015aa78
void EffectButterflyAllFadeOut(void);           // FF3 0x0015aab8: no caller in FF3
void EffectButterflyChangeTarget(int id, float *target);    // FF3 0x0015ab38
void EffectButterflyChangeType(int id, int type);           // FF3 0x0015ab70: no caller in FF3
void EffectButterflyAllChangeTarget(float *target);         // FF3 0x0015aba0: no caller in FF3
int  EffectButterflyNumGet(void);               // FF3 0x0015ac20: butterflies alive, both kinds
BUTTERFLY_TARGET_PARAMETER *EffectButterflyTargetParameterPtrGetPublic(int type);  // FF3 0x0015ac50: no clamp

// The trail motes: their own list and per-frame pass.
void EffectButterflyParticleInit(void);         // FF3 0x0015ac68
void EffectButterflyParticleMain(void);         // FF3 0x0015ac88
void EffectButterflyParticleAllCut(void);       // FF3 0x0015b2b8

#endif /* _GRAPHICS_EFFECT_EFFECT_BUTTERFLY_H */
