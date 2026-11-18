/* ==========================================================================
 *  graphics/effect/effect_torch.h
 *
 *  The torch flame, its sparks and the light shaft (FF2 effect_torch.c;
 *  FF3 0x183d58..0x185908, all 33 functions).
 *
 *  A torch is a ring of 200 PARTICLEs plus a flare at its base, driven by one
 *  of nine parameter sets. On top of the steady flame sit two timed
 *  disturbances: a burst (the flame widens and lengthens and throws a spark
 *  effect, model kind 0x19) and a wind (never fires: the interval getter
 *  answers -1, as in FF2). Sparks are a second ring of 50 particles with
 *  drag and gravity. Model kinds (effect.h): 0x18 SetTorch2, 0x19 SetSpark,
 *  0x1a SetTorch2BigFrea.
 *
 *  FF3 against FF2: the 25x flame scale is gone (particles live in units of
 *  Scale / 100, not 25 * Scale / 100), the flare is EffCandleFreaDraw
 *  instead of DrawFrea, the torch sounds are gone (EffectSetTorch2 /
 *  EffectResetTorch2 start and stop nothing), EffectTorch2GetAlphaRate is
 *  gone, torch 0 is smaller (Scale 124 / StartScale 107 for FF2's 284 / 358)
 *  and burst 0's start scales are 183 / 130 for 666 / 246.
 *
 *  Layouts: every type here is ints, floats and shorts (PARTICLE has no
 *  pointers), so the EE offsets hold on the hosts; the tail padding the EE's
 *  16-byte alignment adds is spelled out so EFFECT_MALLOC(sizeof) asks for
 *  the ROM's sizes.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_TORCH_H
#define _GRAPHICS_EFFECT_EFFECT_TORCH_H

#include <stddef.h>                     // offsetof
#include "graphics/effect/effect.h"     // EFFECT_CONT
#include "graphics/effect/effect_oth.h" // PARTICLE

// One flame preset (data 0x2f9a90.., 0x74 each). Scale / Range / StartScale /
// EndScale are hundredths; the Speed pairs are thousandths biased by 1000
// (1031 is +0.031); R / G / B / alphas are 0..255.
struct TORCH2_PARAMETER                 /* 0x74 */
{
    /* 0x00 */ int LifeTime;
    /* 0x04 */ int AppearNumMax;
    /* 0x08 */ int AppearNumMin;
    /* 0x0c */ int Range;
    /* 0x10 */ int SpeedXMax;
    /* 0x14 */ int SpeedXMin;
    /* 0x18 */ int SpeedYMax;
    /* 0x1c */ int SpeedYMin;
    /* 0x20 */ int SpeedZMax;
    /* 0x24 */ int SpeedZMin;
    /* 0x28 */ int Scale;
    /* 0x2c */ int StartScale;
    /* 0x30 */ int EndScale;
    /* 0x34 */ int StartAlpha;
    /* 0x38 */ int EndAlpha;
    /* 0x3c */ int R;
    /* 0x40 */ int G;
    /* 0x44 */ int B;
    /* 0x48 */ int AlphaBlendA;
    /* 0x4c */ int AlphaBlendB;
    /* 0x50 */ int AlphaBlendC;
    /* 0x54 */ int AlphaBlendD;
    /* 0x58 */ int AlphaBlendFIX;
    /* 0x5c */ int FreaSize;
    /* 0x60 */ int FreaR;
    /* 0x64 */ int FreaG;
    /* 0x68 */ int FreaB;
    /* 0x6c */ int FreaAlpha;
    /* 0x70 */ int ParticleFollowMove;  // % of the life that trails a moved base
};
static_assert(offsetof(TORCH2_PARAMETER, Scale) == 0x28, "TORCH2_PARAMETER.Scale");
static_assert(offsetof(TORCH2_PARAMETER, AlphaBlendA) == 0x48, "TORCH2_PARAMETER.AlphaBlendA");
static_assert(offsetof(TORCH2_PARAMETER, FreaSize) == 0x5c, "TORCH2_PARAMETER.FreaSize");
static_assert(offsetof(TORCH2_PARAMETER, ParticleFollowMove) == 0x70, "TORCH2_PARAMETER.ParticleFollowMove");
static_assert(sizeof(TORCH2_PARAMETER) == 0x74, "TORCH2_PARAMETER: data stride 0x78");

// The burst envelope (data 0x2f9ec8.., 0x24 each): three linear segments over
// Frame1 / Frame2 / EndFrame; IntervalMax = IntervalMin = 0 means never.
struct TORCH2_BURST_PARAMETER           /* 0x24 */
{
    /* 0x00 */ int Range1;
    /* 0x04 */ int Range2;
    /* 0x08 */ int StartScale1;
    /* 0x0c */ int StartScale2;
    /* 0x10 */ int Frame1;
    /* 0x14 */ int Frame2;
    /* 0x18 */ int EndFrame;
    /* 0x1c */ int IntervalMax;
    /* 0x20 */ int IntervalMin;
};
static_assert(offsetof(TORCH2_BURST_PARAMETER, Frame1) == 0x10, "TORCH2_BURST_PARAMETER.Frame1");
static_assert(offsetof(TORCH2_BURST_PARAMETER, IntervalMin) == 0x20, "TORCH2_BURST_PARAMETER.IntervalMin");
static_assert(sizeof(TORCH2_BURST_PARAMETER) == 0x24, "TORCH2_BURST_PARAMETER");

// A spark preset (data 0x2fa030.., 0x50 each). Speed and AccelY pairs are
// biased by 1000; speeds divide by 100, AccelY by 1000; BrakeRate is a
// per-frame velocity factor in thousandths.
struct TORCH2_SPARK_PARAMETER           /* 0x50 */
{
    /* 0x00 */ int LifeTimeMax;
    /* 0x04 */ int LifeTimeMin;
    /* 0x08 */ int AppearNumMax;
    /* 0x0c */ int AppearNumMin;
    /* 0x10 */ int SpeedXMax;
    /* 0x14 */ int SpeedXMin;
    /* 0x18 */ int SpeedYMax;
    /* 0x1c */ int SpeedYMin;
    /* 0x20 */ int SpeedZMax;
    /* 0x24 */ int SpeedZMin;
    /* 0x28 */ int AccelYMax;
    /* 0x2c */ int AccelYMin;
    /* 0x30 */ int BrakeRate;
    /* 0x34 */ int StartScale;
    /* 0x38 */ int EndScale;
    /* 0x3c */ int StartAlpha;
    /* 0x40 */ int EndAlpha;
    /* 0x44 */ int R;
    /* 0x48 */ int G;
    /* 0x4c */ int B;
};
static_assert(offsetof(TORCH2_SPARK_PARAMETER, BrakeRate) == 0x30, "TORCH2_SPARK_PARAMETER.BrakeRate");
static_assert(offsetof(TORCH2_SPARK_PARAMETER, B) == 0x4c, "TORCH2_SPARK_PARAMETER.B");
static_assert(sizeof(TORCH2_SPARK_PARAMETER) == 0x50, "TORCH2_SPARK_PARAMETER");

// One live flame: EFFECT_MALLOC(0x3ed0), ec->pnt[1] of a kind 0x18 record.
// disp is how many particles the last frame drew; it scales the flare.
struct TORCH_CTRL                       /* 0x3ed0 */
{
    /* 0x0000 */ PARTICLE particles[200];
    /* 0x3e80 */ float    BasePos[4];   // last frame's world position
    /* 0x3e90 */ float    WindMax[4];   // the gust blowing
    /* 0x3ea0 */ int      head;         // ring write cursor
    /* 0x3ea4 */ int      BurstCount;   // 0: not bursting, else frame + 1
    /* 0x3ea8 */ int      BurstInterval;    // frames to the next burst (-1 never)
    /* 0x3eac */ int      WindCount;
    /* 0x3eb0 */ int      WindInterval;
    /* 0x3eb4 */ float    AlphaRate;    // master fade, 1.0 full (EffectTorch2SetAlphaRate)
    /* 0x3eb8 */ short    max;          // 2 * LifeTime, never read
    /* 0x3eba */ short    disp;         // particles drawn last frame
    /* 0x3ebc */ short    blife;        // particle lifetime, frames
    /* 0x3ebe */ short    Type;
    /* 0x3ec0 */ short    SeReqFlg;     // kept; FF3 plays no torch sound
    /* 0x3ec2 */ u_char   _pad[0xe];    // the EE's 16-byte tail padding
};
static_assert(offsetof(TORCH_CTRL, BasePos) == 0x3e80, "TORCH_CTRL.BasePos");
static_assert(offsetof(TORCH_CTRL, WindMax) == 0x3e90, "TORCH_CTRL.WindMax");
static_assert(offsetof(TORCH_CTRL, head) == 0x3ea0, "TORCH_CTRL.head");
static_assert(offsetof(TORCH_CTRL, BurstCount) == 0x3ea4, "TORCH_CTRL.BurstCount");
static_assert(offsetof(TORCH_CTRL, WindInterval) == 0x3eb0, "TORCH_CTRL.WindInterval");
static_assert(offsetof(TORCH_CTRL, AlphaRate) == 0x3eb4, "TORCH_CTRL.AlphaRate");
static_assert(offsetof(TORCH_CTRL, disp) == 0x3eba, "TORCH_CTRL.disp");
static_assert(offsetof(TORCH_CTRL, Type) == 0x3ebe, "TORCH_CTRL.Type");
static_assert(offsetof(TORCH_CTRL, SeReqFlg) == 0x3ec0, "TORCH_CTRL.SeReqFlg");
static_assert(sizeof(TORCH_CTRL) == 0x3ed0, "TORCH_CTRL: EFFECT_MALLOC(0x3ed0)");

// One live spark burst: EFFECT_MALLOC(0xfc0), ec->pnt[1] of a kind 0x19 record.
struct SPARK_CTRL                       /* 0xfc0 */
{
    /* 0x000 */ PARTICLE particles[50];
    /* 0xfa0 */ int      head;
    /* 0xfa4 */ int      cnt;
    /* 0xfa8 */ short    max;
    /* 0xfaa */ short    disp;
    /* 0xfac */ short    blife;
    /* 0xfae */ short    Type;
    /* 0xfb0 */ float    BrakeRate;
    /* 0xfb4 */ float    WholeScale;
    /* 0xfb8 */ float    AlphaRate;
    /* 0xfbc */ u_char   _pad[4];       // the EE's 16-byte tail padding
};
static_assert(offsetof(SPARK_CTRL, head) == 0xfa0, "SPARK_CTRL.head");
static_assert(offsetof(SPARK_CTRL, disp) == 0xfaa, "SPARK_CTRL.disp");
static_assert(offsetof(SPARK_CTRL, Type) == 0xfae, "SPARK_CTRL.Type");
static_assert(offsetof(SPARK_CTRL, BrakeRate) == 0xfb0, "SPARK_CTRL.BrakeRate");
static_assert(offsetof(SPARK_CTRL, AlphaRate) == 0xfb8, "SPARK_CTRL.AlphaRate");
static_assert(sizeof(SPARK_CTRL) == 0xfc0, "SPARK_CTRL: EFFECT_MALLOC(0xfc0)");

// The light shaft singleton (data 0x2fa300): a position, a triangular alpha
// ramp of period 2 * Cycle, and the run flag.
struct TORCH2_BIGFREA_CTRL              /* 0x20 */
{
    /* 0x00 */ float Position[4];
    /* 0x10 */ short Count;
    /* 0x12 */ short Cycle;
    /* 0x14 */ short ExecFlg;
    /* 0x16 */ u_char _pad[0xa];
};
static_assert(offsetof(TORCH2_BIGFREA_CTRL, Count) == 0x10, "TORCH2_BIGFREA_CTRL.Count");
static_assert(offsetof(TORCH2_BIGFREA_CTRL, ExecFlg) == 0x14, "TORCH2_BIGFREA_CTRL.ExecFlg");
static_assert(sizeof(TORCH2_BIGFREA_CTRL) == 0x20, "TORCH2_BIGFREA_CTRL");

// Preset lookup: a type > 8 (unsigned) falls back to 0.
TORCH2_PARAMETER       *EffOthTorch2ParameterPtrGet(int Type);         // FF3 0x00183d58
TORCH2_BURST_PARAMETER *EffOthTorch2BurstParameterPtrGet(int Type);    // FF3 0x00183d78
TORCH2_SPARK_PARAMETER *EffOthTorch2SparkParameterPtrGet(int Type);    // FF3 0x00183d98

// Model effect handlers (EffectZSort): each allocates its work on first call.
void SetTorch2(EFFECT_CONT *ec);        // FF3 0x00183e90: kind 0x18
void SetSpark(EFFECT_CONT *ec);         // FF3 0x001848e8: kind 0x19
void SetTorch2BigFrea(EFFECT_CONT *ec); // FF3 0x00185750: kind 0x1a

void TorchPartInit(TORCH_CTRL *pTc, int LifeTime, float *BasePos, int Type, int SeReqFlg);     // FF3 0x00183db8
void ContTorch2(TORCH_CTRL *pTc, float *pos, int Depth,
                TORCH2_PARAMETER *pParam, TORCH2_BURST_PARAMETER *pBurstParam);     // FF3 0x00183f88

// A torch at pos (kept by address: the torch follows it). Returns the
// effect record, the handle for the calls below.
void *EffectSetTorch2(float *pos, int type);            // FF3 0x001847d0: SE flag 1
void *EffectSetTorch2NoSE(float *pos, int type);        // FF3 0x001847e8: SE flag 0 (no caller)
void  EffectResetTorch2(void *pTorch2);                 // FF3 0x00184800: frees the work, ResetEffects
void  EffectTorch2SetAlphaRate(void *pTorch2, float rate);  // FF3 0x00184860: master fade

// The light shaft (FF2's opening "eff_op_hikari"). FF3 never calls the Req,
// so the shaft never runs.
void EffectTorch2BigFreaInit(void);     // FF3 0x001855e8: InitEffects
void EffectTorch2BigFreaMain(void);     // FF3 0x00185600: EffectInPhoto, every frame
void EffectTorch2BigFreaReq(float *Position);   // FF3 0x00185718: no caller
void EffectTorch2BigFreaCut(void);      // FF3 0x00185740: EffectsReleaseAll

#endif /* _GRAPHICS_EFFECT_EFFECT_TORCH_H */
