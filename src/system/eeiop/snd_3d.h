/* ==========================================================================
 *  system/eeiop/snd_3d.h
 *
 *  3D sound placement (FF3 snd_3d.c, 0x26eb88..0x26f340; FF2's snd_3d.c).
 *  Thirty emitter slots and one listener. A voice owner (snd_buffer.c,
 *  snd_stream.c) gets a handle from Snd3DCreateWrk; Snd3DMain turns each live
 *  handle into a stereo volume pair and a doppler pitch once a frame, which
 *  SndCalcValue reads back through Snd3DGetVal.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SND_3D_H
#define _SYSTEM_EEIOP_SND_3D_H

#include <stddef.h>                     // offsetof

#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/eeiop/snd.h"           // VOLSET

#define SND_3D_WRK_NUM  30              // emitter slots (Snd3DInit, Snd3DMain)

// The 3D attenuation model (FF2 SND_3D_ENV); data 0x333a70 holds FF2's
// defaults (0.8, 0.1, 0.7, 0.3, 30, 2), snd3DSetEnvironment replaces them.
typedef struct SND_3D_ENV
{
    /* 0x00 */ float front;             // level kept dead ahead
    /* 0x04 */ float side;              // far ear's share of the off-axis term
    /* 0x08 */ float back;              // level kept dead behind
    /* 0x0c */ float vol_min;           // floor of the distance curve
    /* 0x10 */ float farthest_dist;     // metres; vol_min beyond
    /* 0x14 */ float nearest_dist;      // metres; full volume within
} SND_3D_ENV;                           /* 0x18 */

static_assert(sizeof(SND_3D_ENV) == 0x18, "SND_3D_ENV size");
static_assert(offsetof(SND_3D_ENV, side) == 0x4, "SND_3D_ENV.side");
static_assert(offsetof(SND_3D_ENV, back) == 0x8, "SND_3D_ENV.back");
static_assert(offsetof(SND_3D_ENV, vol_min) == 0xc, "SND_3D_ENV.vol_min");
static_assert(offsetof(SND_3D_ENV, farthest_dist) == 0x10, "SND_3D_ENV.farthest_dist");
static_assert(offsetof(SND_3D_ENV, nearest_dist) == 0x14, "SND_3D_ENV.nearest_dist");

// Where a source (or the listener) is: pointers to its vectors, copied in by
// the callee (FF2 SND_3D_SET). vel nullptr: not moving.
// layout: host -- three pointers (0xc on the EE); only built on the stack.
typedef struct SND_3D_SET
{
    float *pos;
    float *vel;
    float *dir;                         // listener: unit forward axis; sources: unused
} SND_3D_SET;

// One emitter slot (bss 0x4108c0, 30 x 0x30). status bits: SND_3D_ST_*.
typedef struct SND_3D_WRK
{
    /* 0x00 */ sceVu0FVECTOR pos;
    /* 0x10 */ sceVu0FVECTOR velocity;  // the zero vector when the source has none
    /* 0x20 */ int           voll;      // results of Snd3DCalc: 0..16383
    /* 0x24 */ int           volr;
    /* 0x28 */ int           pitch;     // 0x1000 = unchanged
    /* 0x2c */ int           status;
} SND_3D_WRK;                           /* 0x30 */

static_assert(sizeof(SND_3D_WRK) == 0x30, "SND_3D_WRK size");
static_assert(offsetof(SND_3D_WRK, velocity) == 0x10, "SND_3D_WRK.velocity");
static_assert(offsetof(SND_3D_WRK, voll) == 0x20, "SND_3D_WRK.voll");
static_assert(offsetof(SND_3D_WRK, volr) == 0x24, "SND_3D_WRK.volr");
static_assert(offsetof(SND_3D_WRK, pitch) == 0x28, "SND_3D_WRK.pitch");
static_assert(offsetof(SND_3D_WRK, status) == 0x2c, "SND_3D_WRK.status");

#define SND_3D_ST_USE   0x1             // slot taken
#define SND_3D_ST_CALC  0x2             // moved since the last Snd3DCalc

// The listener (bss 0x410e60). defer: moved, so every slot is recomputed.
typedef struct SND_3D_LISTENER
{
    /* 0x00 */ sceVu0FVECTOR pos;
    /* 0x10 */ sceVu0FVECTOR velocity;
    /* 0x20 */ sceVu0FVECTOR dir;
    /* 0x30 */ sceVu0FVECTOR top;
    /* 0x40 */ int           defer;
} SND_3D_LISTENER;                      /* 0x50 (quadword aligned) */

static_assert(sizeof(SND_3D_LISTENER) == 0x50, "SND_3D_LISTENER size");
static_assert(offsetof(SND_3D_LISTENER, velocity) == 0x10, "SND_3D_LISTENER.velocity");
static_assert(offsetof(SND_3D_LISTENER, dir) == 0x20, "SND_3D_LISTENER.dir");
static_assert(offsetof(SND_3D_LISTENER, top) == 0x30, "SND_3D_LISTENER.top");
static_assert(offsetof(SND_3D_LISTENER, defer) == 0x40, "SND_3D_LISTENER.defer");

// A replacement for Snd3DCalc's model (sdata gp-0xcf4, never set in the ROM):
// gets &s3d->voll (voll, volr, pitch), the slot, the listener and 0.
typedef void (*SND_3D_CALC_FUNC)(int *val, SND_3D_WRK *s3d, SND_3D_LISTENER *listener, int arg);

void        Snd3DInit(void);                                    // FF3 0x0026eb88: from SndInit
void        snd3DSet1Meter(float unit);                         // FF3 0x0026ebd8: world units per metre
void        snd3DSetEnvironment(SND_3D_ENV *env);               // FF3 0x0026ec20
void        SndSetListener(const SND_3D_SET *set, float *top);  // FF3 0x0026ec78 (FF2 snd3DSetListner)
void        Snd3DFreeWrk(SND_3D_WRK *s3d);                      // FF3 0x0026ecf8
SND_3D_WRK *Snd3DCreateWrk(const SND_3D_SET *set);              // FF3 0x0026ed10: nullptr when all 30 are taken
void        snd3DSetSET(SND_3D_WRK *s3d, const SND_3D_SET *set);        // FF3 0x0026ed80
void        Snd3DSetPosition(SND_3D_WRK *s3d, float *pos);      // FF3 0x0026ede0
void        Snd3DGetVal(void *s3d, VOLSET *volset, short *pitch);       // FF3 0x0026ee10: a SND_3D_WRK
float       GetLenDirection(float *velocity, float *unit_dir);  // FF3 0x0026ee30: signed speed along unit_dir
void        Snd3DSetCalcFunc(SND_3D_CALC_FUNC func);            // FF3 0x0026f178: no caller in the ROM
void        Snd3DMain(void);                                    // FF3 0x0026f288: per frame, from SndMain

#endif /* _SYSTEM_EEIOP_SND_3D_H */
