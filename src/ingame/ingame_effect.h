/* ==========================================================================
 *  ingame/ingame_effect.h
 *
 *  The in-game side of the effect system (FF3 0x1e8738..0x1ea2b0 and the
 *  out-of-line 0x2bcc98; FF2 ingame_effect.c, MioPan's port of it): the
 *  spirit-particle suction point, the camera sub-function hits and their
 *  parts-deform job table, the screen look of the frame, the zero-shot
 *  delay, the butterflies, the lantern / water-flow / light-come-in map-put
 *  draws, and FF3's additions -- the flashlight's lens flare (the player's
 *  own map-put 0x1194), the high-contrast level FADE_HIGHCONTRAST fades and
 *  the candle flicker near a moving player.
 *
 *  FF3 dropped the parts-deform itself: the eleven *PDeformReq helpers
 *  return nullptr, so a job slot frees itself the frame after it fires.
 * ======================================================================== */
#ifndef _INGAME_INGAME_EFFECT_H
#define _INGAME_INGAME_EFFECT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct SCREEN_EFFECT_PARAMETER;         // graphics/effect/effect.h
struct MAP_PUT;                         // ingame/map/MapPut.h
class GameModel;                        // graphics/game_model.h

// ---- the parts-deform job table ------------------------------------------

// Which camera sub-function armed a job (SUBFUNC_PDEFORM_CTRL.Type). FF2's
// names; FF3 added the two extra functions and moved FREQ to 11 (still
// unreachable: no request passes it).
enum SUBFUNC_PDEFORM_TYPE
{
    PDEFORM_TYPE_ZERO     = 0,
    PDEFORM_TYPE_SLOW     = 1,
    PDEFORM_TYPE_KOKU     = 2,
    PDEFORM_TYPE_PARALYZE = 3,
    PDEFORM_TYPE_VIEW     = 4,
    PDEFORM_TYPE_METSU    = 5,
    PDEFORM_TYPE_REN      = 6,
    PDEFORM_TYPE_TSUI     = 7,
    PDEFORM_TYPE_FUU      = 8,
    PDEFORM_TYPE_EX1      = 9,          // FF3-only
    PDEFORM_TYPE_EX2      = 10,         // FF3-only
    PDEFORM_TYPE_FREQ     = 11,
};

// NOT_USE free, USE counting to StartFrame, WAIT holding the effect until
// EffectExecCheck says it is over.
enum SUBFUNC_PDEFORM_STATUS
{
    SUBFUNC_PDEFORM_STATUS_NOT_USE = 0,
    SUBFUNC_PDEFORM_STATUS_USE     = 1,
    SUBFUNC_PDEFORM_STATUS_WAIT    = 2,
};

// One job (bss 0x3ef190, ten of them).
// layout: host -- pEffRet is a pointer (EE 4 bytes at +0x10); Status follows it.
struct SUBFUNC_PDEFORM_CTRL             /* 0x18 on the EE */
{
    /* 0x00 */ int   EneWrkNo;          // the ghost
    /* 0x04 */ int   Count;             // frames since the request
    /* 0x08 */ int   Type;              // SUBFUNC_PDEFORM_TYPE
    /* 0x0c */ int   StartFrame;        // Count the deform fires at
    /* 0x10 */ void *pEffRet;           // the effect it made (nullptr in FF3)
    /* 0x14 */ int   Status;            // SUBFUNC_PDEFORM_STATUS
};
static_assert(offsetof(SUBFUNC_PDEFORM_CTRL, Count) == 0x04, "SUBFUNC_PDEFORM_CTRL.Count");
static_assert(offsetof(SUBFUNC_PDEFORM_CTRL, StartFrame) == 0x0c, "SUBFUNC_PDEFORM_CTRL.StartFrame");
static_assert(offsetof(SUBFUNC_PDEFORM_CTRL, pEffRet) == 0x10, "SUBFUNC_PDEFORM_CTRL.pEffRet");
static_assert(offsetof(SUBFUNC_PDEFORM_CTRL, Status) == 0x10 + sizeof(void *), "SUBFUNC_PDEFORM_CTRL.Status (EE 0x14)");

#define SUBFUNC_PDEFORM_NUM 10

// The zero shot's particle delay (sbss 0x3733e8). Count -1: idle.
struct ZERO_PARTICLE_CTRL               /* 0x8 */
{
    /* 0x0 */ int EneWrkNo;
    /* 0x4 */ int Count;
};
static_assert(sizeof(ZERO_PARTICLE_CTRL) == 0x8, "ZERO_PARTICLE_CTRL is 8 bytes");

// ---- lifecycle -------------------------------------------------------------

void IgEffectInit(void);                // FF3 0x001e8738: InitEffects' tail call
void IgEffectMain(void);                // FF3 0x001e8790: EffectDrawMain, except in the movie phases
void IgEffectRelease(void);             // FF3 0x001e8838: EffectsReleaseAll (last): high contrast reset, the player's put deleted

// ---- the spirit-particle suction point ---------------------------------------

float (*IgEffectParticleEndPosFinderGet(void))[4];     // FF3 0x001e8c08 (also the player's FUN_0023db98)
float (*IgEffectParticleEndPosNoFinderGet(void))[4];   // FF3 0x001e8c18
void   IgEffectParticleEndPosCalcFinder(float *EndPos);     // FF3 0x001e8c28
void   IgEffectParticleEndPosCalcNoFinder(float *EndPos);   // FF3 0x001e8d50
int    IgEffectIsEndParticleSuck(void); // FF3 0x001e95f0: no damage particle live (no caller in FF3)

// ---- the camera sub-function hits (EneHitEffectSubFuncReq) ------------------
// Each raises its A / B pair of EneDmgLargeHitReq presets (grade 0 plain,
// 1 shutter chance, 2 special) and a parts-deform job.
void IgEffectSubFuncZeroReq(int num, int type);         // FF3 0x001e8e08: presets 6..11
void IgEffectSubFuncSlowReq(int num);                   // FF3 0x001e8e78: presets 4, 5
void IgEffectSubFuncKokuReq(int num, int type);         // FF3 0x001e8eb8: presets 12..17
void IgEffectSubFuncParalyzeReq(int num);               // FF3 0x001e8f28: presets 18, 19
void IgEffectSubFuncViewReq(int num);                   // FF3 0x001e8f68: presets 20, 21
void IgEffectSubFuncMetsuReq(int num, int type);        // FF3 0x001e8fa8: presets 22..27
void IgEffectSubFuncRenReq(int num, int type);          // FF3 0x001e9018: presets 28..33
void IgEffectSubFuncTsuiReq(int num);                   // FF3 0x001e9088: presets 34, 35
void IgEffectSubFuncFuuReq(int num);                    // FF3 0x001e90c8: presets 36, 37
void IgEffectSubFuncEx1Req(int num, int type);          // FF3 0x001e9108: presets 38..43 (FF3-only)
void IgEffectSubFuncEx2Req(int num, int type);          // FF3 0x001e9178: presets 44..49 (FF3-only)
void IgEffectSubFuncPDeformClear(int EneWrkNo);         // FF3 0x001e9290: every job on one ghost (also the player's FUN_0023dad0)
void IgEffectZeroCamFreqReq(void);                      // FF3 0x001e95d8: FF2 ReqFreqCamera, now PhotoShakeReq

// ---- screen effects -----------------------------------------------------------

void IgEffectStoryMainScreenEffectReq(SCREEN_EFFECT_PARAMETER *prm);    // FF3 0x001e96a8: InitEffectsEF, this frame's screen look
int  IgEffectStoryMainContrastTypeGet(SCREEN_EFFECT_PARAMETER *prm);    // FF3 0x002bcc98: Contrast 2..5 -> effect 0xb / 0xc / 0xd / 10, else 0

// The high-contrast level (sbss 0x3733f8, CFadeVariable<short>, 0..100):
// how far the candle-out looks' contrast colour / alpha move to 84 / 124.
void IgEffectHighContrastInit(void);    // FF3 0x001e9610
void IgEffectHighContrastWork(void);    // FF3 0x001e9628: one fade step (IgEffectMain)
void IgEffectHighContrastReset(void);   // FF3 0x001e9640
void IgEffectHighContrastFade(short level, u_short time);  // FF3 0x001e9658: FADE_HIGHCONTRAST, unless already aimed at level

// ---- the flashlight's lens flare (FF3-only) -----------------------------------

void IgEffectHandLightFlareInit(void);  // FF3 0x001e8920
void IgEffectHandLightFlareCtrl(void);  // FF3 0x001e8928: the level fades in / out by 0.125 a frame
void IgEffectHandLightFlareDraw(u_int flags);   // FF3 0x001e89f8: the put's draw flags (0x200: no flare core)
// A model's coord-0 lens point and its aim (also FUN_00256040).
void IgEffectHandLightFlarePosRot(GameModel *mdl, float *pos, float *rx, float *ry);  // FF3 0x001e8ae8
void IgEffectCandleYuramekiCtrl(float *pos, float *pos_old);   // FF3 0x001e8858: candles near a moving player flicker

// The player's map-put 0x1194 (draw func IgEffectPlyrPutDraw).
void IgEffectPlyrPutDraw(MAP_PUT *put, u_int flags);    // FF3 0x001ea178
void IgEffectPlyrPutSet(void);          // FF3 0x001ea1d8: PlyrMdlIsReady, once the models are ready
void IgEffectPlyrPutDelete(void);       // FF3 0x001ea280: PlyrMdlRelease

// ---- particles, butterflies, footsteps -----------------------------------------

void IgEffectEffectEndParticleReq(float *pos, int no);  // FF3 0x001e9eb8: a 48-particle end burst (EneMahi*End: 4, 0, 2)
void IgEffectButterflyReq(float *pos);  // FF3 0x001e9a30: one butterfly, 1 in 400 under 10 (no caller in FF3)
void IgEffectButterflyAllCut(void);     // FF3 0x001e9ac8 (no caller in FF3)
void IgEffectFootStepReq(float *pos, int file_no);      // FF3 0x001e9ae8: a FOOT_44_0_SUIMEN_BD step raises particle 0x309 at pos (unless the camera is up)

// ---- the model-backed room effects (map-put draws) -------------------------------

void IgEffectTourouFreaModelDraw(void);                 // FF3 0x001e9d50 (no caller in FF3)
void IgEffectTourouFreaModelDrawSub(void *pHandle, float (*CoordMat)[4]);   // FF3 0x001e9b38
void IgEffectTourouBaseModelDraw(void);                 // FF3 0x001e9e70 (no caller in FF3)
void IgEffectTourouBaseModelDrawSub(void *pSgdTop, float (*CoordMat)[4]);   // FF3 0x001e9d88
// MapDrawRoomObjRegist's callbacks; the put's work is the water-flow move
// type / the light's SGD.
void IgEffectWaterFlowModelDraw(MAP_PUT *put, u_int flags);     // FF3 0x001e9ef8: furniture 801..899
void IgEffectLightComeInModelDraw(MAP_PUT *put, u_int flags);   // FF3 0x001e9fe8: furniture 900..999

#endif /* _INGAME_INGAME_EFFECT_H */
