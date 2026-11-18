// graphics/effect/effect_torch.c -- the torch flame, its sparks and the light
// shaft (FF2 effect_torch.c; FF3 0x183d58..0x185908, all 33 functions).
//
// Ported from MioPan's effect_torch.c and checked against FF3 function by
// function; the FF3 changes are listed in effect_torch.h. Callers: the
// flies (fly_ctrl.c, EffectSetTorch2 / EffectTorch2SetAlphaRate /
// EffectResetTorch2) and effect.c (EffectZSort kinds 0x18..0x1a, InitEffects,
// EffectInPhoto, EffectsReleaseAll).

#include "graphics/effect/effect_torch.h"

#include <string.h>                     // memset

#include "graphics/effect/effect.h"     // SetEffects, ResetEffects, EFFECT_MALLOC / FREE, eff_wrk
#include "graphics/effect/effect_obj.h" // EffCandleFreaDraw
#include "graphics/effect/effect_oth.h" // PARTICLE, draw_distortion_particles2
#include "graphics/effect/effect_sub.h" // Get2PosRot, Set3DPosTexure
#include "graphics/graph2d/g2d_draw.h"  // effdat
#include "graphics/graph3d/gra3d.h"     // gra3dGetCamera, gra3dcamGetPosition
#include "graphics/gs_reg.h"            // GS_REG_SET, SetGsRegsCached
#include "reipan/os/reipan_rand.h"      // ReiPan_Rand, REIPAN_RAND_MAXF

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The nine flame presets (data 0x2f9a90, 0x74 each). Types 1..8 are FF2's;
// type 0 is FF3's own smaller flame (Scale 124, StartScale 107).
static TORCH2_PARAMETER TorchType[9] = {
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 124, 107, 0, 25, 0,
        181, 89, 67, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 124, 358, 0, 31, 0,
        55, 29, 21, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   50, 6, 1, 171, 1031, 974, 889, 868, 1011, 981, 84, 358, 0, 25, 0,
        181, 89, 67, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   1000, 3, 0, 96, 1031, 974, 989, 701, 1011, 981, 97, 121, 1000, 38, 0,
        21, 21, 21, 0, 1, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 100, 358, 0, 30, 0,
        55, 29, 21, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 202, 358, 0, 25, 0,
        57, 63, 66, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 284, 358, 0, 25, 0,
        181, 89, 67, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
    {   51, 7, 1, 219, 1031, 974, 889, 868, 1011, 981, 280, 496, 18, 18, 0,
        68, 72, 139, 0, 2, 0, 1, 0, 303, 75, 77, 191, 68, 80 },
    {   50, 6, 1, 140, 1031, 974, 889, 868, 1011, 981, 284, 358, 0, 25, 0,
        181, 89, 67, 0, 2, 0, 1, 0, 140, 50, 50, 50, 89, 80 },
};

// The burst envelopes (data 0x2f9ec8, 0x24 each). Only type 0 is ever used
// for the envelope (SetTorch2 passes it for every torch); the others only
// give their type's interval. Type 0's start scales are FF3's (183 / 130).
static TORCH2_BURST_PARAMETER TorchBurstType[9] = {
    { 419, 149, 183, 130, 3, 33, 50, 300, 150 },
    { 419, 149, 666, 246, 3, 33, 50, 0, 0 },
    { 419, 149, 666, 246, 3, 33, 50, 0, 0 },
    { 419, 149, 666, 246, 3, 33, 50, 0, 0 },
    { 419, 149, 666, 246, 3, 33, 50, 300, 150 },
    { 419, 149, 666, 246, 3, 33, 50, 141, 74 },
    { 419, 149, 666, 246, 3, 33, 50, 300, 150 },
    { 419, 149, 666, 246, 3, 33, 50, 300, 150 },
    { 419, 149, 666, 246, 3, 33, 50, 300, 150 },
};

// The spark presets (data 0x2fa030, 0x50 each); FF2's values.
static TORCH2_SPARK_PARAMETER TorchSparkType[9] = {
    { 87, 3, 18, 13, 1051, 951, 1056, 897, 1053, 950, 1000, 981, 848, 23, 0, 127, 0, 185, 76, 66 },
    { 87, 3, 18, 13, 1051, 951, 1004, 897, 1053, 950, 1000, 981, 848, 48, 0, 100, 0, 75, 35, 21 },
    { 87, 3, 18, 13, 1051, 951, 1004, 897, 1053, 950, 1000, 981, 848, 48, 0, 100, 0, 75, 35, 21 },
    { 87, 3, 18, 13, 1051, 951, 1004, 897, 1053, 950, 1000, 981, 848, 48, 0, 100, 0, 75, 35, 21 },
    { 87, 3, 18, 13, 1051, 951, 1004, 897, 1053, 950, 1000, 981, 848, 48, 0, 100, 0, 75, 35, 21 },
    { 87, 3, 18, 13, 1051, 951, 1056, 897, 1053, 950, 1000, 981, 848, 23, 0, 127, 0, 51, 55, 78 },
    { 87, 3, 18, 13, 1051, 951, 1056, 897, 1053, 950, 1000, 981, 848, 23, 0, 127, 0, 185, 76, 66 },
    { 87, 3, 18, 13, 1051, 951, 1056, 897, 1053, 950, 1000, 981, 848, 23, 0, 127, 0, 185, 76, 66 },
    { 87, 3, 18, 13, 1051, 951, 1056, 897, 1053, 950, 1000, 981, 848, 23, 0, 127, 0, 185, 76, 66 },
};

// The pointer tables the three getters index (data 0x2fa320 / 0x2fa348 / 0x2fa370).
static TORCH2_PARAMETER *pTorchParamPtr[9] = {
    &TorchType[0], &TorchType[1], &TorchType[2], &TorchType[3], &TorchType[4],
    &TorchType[5], &TorchType[6], &TorchType[7], &TorchType[8],
};
static TORCH2_BURST_PARAMETER *pTorchBurstParamPtr[9] = {
    &TorchBurstType[0], &TorchBurstType[1], &TorchBurstType[2], &TorchBurstType[3], &TorchBurstType[4],
    &TorchBurstType[5], &TorchBurstType[6], &TorchBurstType[7], &TorchBurstType[8],
};
static TORCH2_SPARK_PARAMETER *pTorchSparkParamPtr[9] = {
    &TorchSparkType[0], &TorchSparkType[1], &TorchSparkType[2], &TorchSparkType[3], &TorchSparkType[4],
    &TorchSparkType[5], &TorchSparkType[6], &TorchSparkType[7], &TorchSparkType[8],
};

static TORCH2_BIGFREA_CTRL Torch2BigFreaCtrl;  // data 0x2fa300

// The torch's GS state (rdata 0x354b30): additive ALPHA 0x48, bilinear TEX1,
// CLAMP 0, TEST 0x5000d, ZBUF 0x10a000118 (z test, no z write).
static const GS_REG_SET torch_draw_env[5] = {
    { 0x48,           SCE_GS_ALPHA_1 },
    { 0x161,          SCE_GS_TEX1_1 },
    { 0,              SCE_GS_CLAMP_1 },
    { 0x5000d,        SCE_GS_TEST_1 },
    { 0x10a000118ULL, SCE_GS_ZBUF_1 },
};

static int  EffOthTorch2BurstIntervalTimeGet(int Type);
static int  EffOthTorch2WindIntervalTimeGet(void);
static void Torch2BasePosMoveInfluence(float WholeScale, PARTICLE *pPartTop, float *Diff,
                                       int InitLifeTime, int ParticleFollowMove);
static void Torch2AddParticle(float r, float g, float b, float a, float Scale,
                              TORCH_CTRL *pTc, float *pos, float *vel);
static void Torch2UpdateParticles(float StartAlpha, float EndAlpha, float StartScale, float EndScale,
                                  PARTICLE *pPartTop, int InitLifeTime);
static void EffOthTorch2BurstReqCtrl(float WholeScale, float AlphaRate, TORCH_CTRL *pTc);
static int  Torch2BurstRangeAndStartScaleGet(float *pRange, float *pStartScale, int Count,
                                             TORCH2_PARAMETER *pParam, TORCH2_BURST_PARAMETER *pBurstParam);
static void Torch2WindReqCtrl(TORCH_CTRL *pTc);
static int  Torch2WindVectorGet(float *NowWind, float *WindMax, int Count);
static void TorchSparkPartInit(float BrakeRate, float WholeScale, float AlphaRate, SPARK_CTRL *pSc, int Type);
static void AddSparkParticle(float Scale, SPARK_CTRL *pSc, TORCH2_SPARK_PARAMETER *pSparkParam);
static int  UpdateSparkParticles(float BrakeRate, float AlphaRate, PARTICLE *pPtop,
                                 TORCH2_SPARK_PARAMETER *pSparkParam);
static SPARK_CTRL *ContTorchSpark(float size, float sr, SPARK_CTRL *pSc, float *pos,
                                  TORCH2_SPARK_PARAMETER *pSparkParam);
static void Torch2BigFreaDraw(float ScaleX, float ScaleY, float *Position, u_char R, u_char G, u_char B,
                              u_char Alpha);

// rand() / RAND_MAX as the ROM writes it (2147483520.0f, rdata 0x36f328).
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)
static inline float TorchRand01(void)
{
    return (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
}

// FF2's EffectGetRandom, inline in FF3: min + (max - min) * rand01.
static inline float TorchGetRandom(float min, float max)
{
    return (max - min) * TorchRand01() + min;
}

static inline void TorchCopyVector(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// ──────────────────────────────────────────────────────────────────────
// Preset lookup
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00183d58
TORCH2_PARAMETER *EffOthTorch2ParameterPtrGet(int Type)
{
    if ((u_int)Type > 8)
        Type = 0;
    return pTorchParamPtr[Type];
}

// FF3 0x00183d78
TORCH2_BURST_PARAMETER *EffOthTorch2BurstParameterPtrGet(int Type)
{
    if ((u_int)Type > 8)
        Type = 0;
    return pTorchBurstParamPtr[Type];
}

// FF3 0x00183d98
TORCH2_SPARK_PARAMETER *EffOthTorch2SparkParameterPtrGet(int Type)
{
    if ((u_int)Type > 8)
        Type = 0;
    return pTorchSparkParamPtr[Type];
}

// ──────────────────────────────────────────────────────────────────────
// The flame
// ──────────────────────────────────────────────────────────────────────

// Wipes the particle ring and seeds the counters. The first burst interval
// reads pTc->Type before it is written (the previous torch's type, or heap
// garbage on a fresh block); ROM-faithful, as in FF2.
// FF3 0x00183db8
void TorchPartInit(TORCH_CTRL *pTc, int LifeTime, float *BasePos, int Type, int SeReqFlg)
{
    int i;

    for (i = 0; i < 200; i++) {
        pTc->particles[i].position[0] = 0.0f;
        pTc->particles[i].position[1] = 0.0f;
        pTc->particles[i].position[2] = 0.0f;
        pTc->particles[i].position[3] = 1.0f;
        pTc->particles[i].color[3] = 0.0f;
        pTc->particles[i].lifetime = 0;
    }
    TorchCopyVector(pTc->BasePos, BasePos);
    pTc->head = 0;
    pTc->BurstCount = 0;
    pTc->BurstInterval = EffOthTorch2BurstIntervalTimeGet(pTc->Type);
    pTc->WindCount = 0;
    pTc->WindInterval = EffOthTorch2WindIntervalTimeGet();
    pTc->SeReqFlg = (short)SeReqFlg;
    pTc->blife = (short)LifeTime;
    pTc->Type = (short)Type;
    pTc->AlphaRate = 1.0f;
    pTc->max = (short)(LifeTime * 2);
    pTc->disp = 0;
}

// Kind 0x18: ec->pnt[0] the torch's position, pnt[1] its TORCH_CTRL,
// uc8[2] the type, uc8[4] the SE flag. Every torch bursts on type 0's
// envelope (FF2 the same).
// callcheck: EFFECT_MALLOC (read as a macro)
// FF3 0x00183e90
void SetTorch2(EFFECT_CONT *ec)
{
    TORCH2_PARAMETER *pParam;
    float pos[4];

    TorchCopyVector(pos, (float *)ec->pnt[0]);
    pParam = EffOthTorch2ParameterPtrGet(ec->dat.uc8[2]);
    if (ec->pnt[1] == nullptr) {
        ec->pnt[1] = EFFECT_MALLOC(sizeof(TORCH_CTRL));
        if (ec->pnt[1] == nullptr) {
            ResetEffects(ec);
            return;
        }
        TorchPartInit((TORCH_CTRL *)ec->pnt[1], pParam->LifeTime, pos, ec->dat.uc8[2], ec->dat.uc8[4]);
    }
    SetGsRegsCached(torch_draw_env, 5, 1);
    ContTorch2((TORCH_CTRL *)ec->pnt[1], pos, (int)ec->z, pParam, &TorchBurstType[0]);
}

// One frame of a flame: the base-move drag, the burst and wind envelopes,
// the flare (20 below the anchor, sized and lit by last frame's particle
// count), the particles, the new spawns on a flat disc of diameter Range,
// then the ageing. Particles live in units of WholeScale (Scale / 100).
// callcheck: rand (inside TorchGetRandom / TorchRand01)
// FF3 0x00183f88
void ContTorch2(TORCH_CTRL *pTc, float *pos, int Depth,
                TORCH2_PARAMETER *pParam, TORCH2_BURST_PARAMETER *pBurstParam)
{
    sceVu0FMATRIX work;
    sceVu0FMATRIX local_clip;
    sceVu0FMATRIX local_world;
    sceVu0FMATRIX local_screen;
    sceVu0FVECTOR ppos;
    sceVu0FVECTOR pvel;
    sceVu0FVECTOR wpos;
    sceVu0FVECTOR Diff;
    sceVu0FVECTOR Wind;
    GRA3DCAMERA *pCam;
    float Range;
    float StartScale;
    float FreaSize;
    float FreaAlpha;
    float WholeScale;
    u_long AlphaBlend;
    int FreaR, FreaG, FreaB;
    int ParticleNum;

    ppos[0] = ppos[1] = ppos[2] = 0.0f;
    ppos[3] = 1.0f;
    pvel[0] = pvel[1] = pvel[2] = pvel[3] = 0.0f;
    Wind[0] = Wind[1] = Wind[2] = Wind[3] = 0.0f;

    pCam = gra3dGetCamera();
    FreaR = pParam->FreaR;
    FreaG = pParam->FreaG;
    AlphaBlend = (u_long)pParam->AlphaBlendA | ((u_long)pParam->AlphaBlendFIX << 32)
               | ((u_long)pParam->AlphaBlendC << 4) | ((u_long)pParam->AlphaBlendB << 2)
               | ((u_long)pParam->AlphaBlendD << 6);
    FreaB = pParam->FreaB;
    FreaSize = (float)pParam->FreaSize / 1000.0f;
    FreaAlpha = (float)pParam->FreaAlpha;
    WholeScale = (float)pParam->Scale / 100.0f;
    if (pTc == nullptr)
        return;

    sceVu0SubVector(Diff, pos, pTc->BasePos);
    TorchCopyVector(pTc->BasePos, pos);
    Torch2BasePosMoveInfluence(WholeScale, pTc->particles, Diff, pTc->blife, pParam->ParticleFollowMove);

    if (pBurstParam == nullptr) {
        Range = (float)pParam->Range / 100.0f;
        StartScale = (float)pParam->StartScale / 100.0f;
    } else {
        EffOthTorch2BurstReqCtrl(WholeScale, pTc->AlphaRate, pTc);
        if (Torch2BurstRangeAndStartScaleGet(&Range, &StartScale, pTc->BurstCount, pParam, pBurstParam) == 0)
            pTc->BurstCount = 0;
    }

    Torch2WindReqCtrl(pTc);
    if (Torch2WindVectorGet(Wind, pTc->WindMax, pTc->WindCount) == 0)
        pTc->WindCount = 0;

    sceVu0UnitMatrix(work);
    work[0][0] = work[1][1] = work[2][2] = WholeScale;
    sceVu0TransMatrix(local_world, work, pos);
    sceVu0MulMatrix(local_screen, pCam->matWorldScreen, local_world);
    sceVu0MulMatrix(local_clip, pCam->matWorldClipPolygon, local_world);

    TorchCopyVector(wpos, pos);
    wpos[1] -= 20.0f;
    EffCandleFreaDraw((FreaSize * (float)pTc->disp * 3.3f * StartScale) / 200.0f, 0.0f, wpos, Depth,
                      FreaR, FreaG, FreaB, (int)((FreaAlpha * (float)pTc->disp) / 200.0f));

    // 4289.9995f: the ROM's literal (FF2's 4290 one ulp down).
    pTc->disp = (short)draw_distortion_particles2(local_screen, local_clip, 200, pTc->particles,
                                                  (WholeScale * 4289.9995f) / pCam->fFov,
                                                  effdat[0x4c + eff_wrk.monochro_mode].tex0, AlphaBlend);

    ParticleNum = (int)TorchGetRandom((float)pParam->AppearNumMin, (float)pParam->AppearNumMax);
    while (ParticleNum > 0) {
        ParticleNum--;
        ppos[1] = 0.0f;
        ppos[0] = (TorchRand01() - 0.5f) * Range;
        ppos[2] = (TorchRand01() - 0.5f) * Range;
        // Speeds are thousandths biased by 1000.
        pvel[0] = (TorchGetRandom((float)pParam->SpeedXMin, (float)pParam->SpeedXMax) - 1000.0f) / 1000.0f + Wind[0];
        pvel[1] = (TorchGetRandom((float)pParam->SpeedYMin, (float)pParam->SpeedYMax) - 1000.0f) / 1000.0f + Wind[1];
        pvel[2] = (TorchGetRandom((float)pParam->SpeedZMin, (float)pParam->SpeedZMax) - 1000.0f) / 1000.0f + Wind[2];
        Torch2AddParticle((float)pParam->R, (float)pParam->G, (float)pParam->B,
                          (float)pParam->StartAlpha * pTc->AlphaRate, StartScale, pTc, ppos, pvel);
    }

    Torch2UpdateParticles((float)pParam->StartAlpha * pTc->AlphaRate, (float)pParam->EndAlpha * pTc->AlphaRate,
                          StartScale, (float)pParam->EndScale / 100.0f, pTc->particles, pTc->blife);
    if (pTc->BurstCount > 0)
        pTc->BurstCount++;
    if (pTc->WindCount > 0)
        pTc->WindCount++;
}

// Drags the particles whose remaining life is under ParticleFollowMove % of
// the full span back by the anchor's move, so the tip lags a moving torch.
// FF3 0x00184568
static void Torch2BasePosMoveInfluence(float WholeScale, PARTICLE *pPartTop, float *Diff,
                                       int InitLifeTime, int ParticleFollowMove)
{
    sceVu0FVECTOR LocalDiff;
    int i;

    sceVu0ScaleVector(LocalDiff, Diff, 1.0f / WholeScale);
    for (i = 0; i < 200; i++) {
        if (pPartTop[i].lifetime != 0 && pPartTop[i].lifetime <= (InitLifeTime * ParticleFollowMove) / 100)
            sceVu0SubVector(pPartTop[i].position, pPartTop[i].position, LocalDiff);
    }
}

// Pushes one particle into the ring, overwriting the oldest.
// FF3 0x00184610
static void Torch2AddParticle(float r, float g, float b, float a, float Scale,
                              TORCH_CTRL *pTc, float *pos, float *vel)
{
    PARTICLE *p = &pTc->particles[pTc->head];

    p->position[0] = pos[0];
    p->position[1] = pos[1];
    p->position[2] = pos[2];
    p->position[3] = 1.0f;
    p->velocity[0] = vel[0];
    p->velocity[1] = vel[1];
    p->velocity[2] = vel[2];
    p->color[0] = r;
    p->color[1] = g;
    p->color[2] = b;
    p->color[3] = a;
    p->velocity[3] = 0.0f;
    p->alp_step = 0.0f;
    p->Scale = Scale;
    p->lifetime = pTc->blife;
    pTc->head = (pTc->head + 1) % 200;
}

// Ages the ring: alpha and scale follow 1 - life / InitLifeTime (the shared
// lifetime, not the particle's own).
// FF3 0x001846a8
static void Torch2UpdateParticles(float StartAlpha, float EndAlpha, float StartScale, float EndScale,
                                  PARTICLE *pPartTop, int InitLifeTime)
{
    float Progress;
    int i;

    if (pPartTop == nullptr || InitLifeTime == 0)
        return;
    for (i = 0; i < 200; i++) {
        Progress = 1.0f - (float)pPartTop[i].lifetime / (float)InitLifeTime;
        if (pPartTop[i].lifetime == 0) {
            pPartTop[i].color[3] = 0.0f;
        } else {
            sceVu0AddVector(pPartTop[i].position, pPartTop[i].position, pPartTop[i].velocity);
            pPartTop[i].lifetime--;
            pPartTop[i].color[3] = (EndAlpha - StartAlpha) * Progress + StartAlpha;
            pPartTop[i].Scale = (EndScale - StartScale) * Progress + StartScale;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The torch handle (the flies)
// ──────────────────────────────────────────────────────────────────────

// SetEffects(0x18, 2, type, pos, SE flag). FF3 dropped FF2's ignition sound
// and fire loop: the SE flag is only stored.
// FF3 0x001847a8
static void *EffectSetTorch2Sub(float *pos, int type, int SeReqFlg)
{
    return SetEffects(0x18, 2, type, pos, SeReqFlg);
}

// FF3 0x001847d0
void *EffectSetTorch2(float *pos, int type)
{
    return EffectSetTorch2Sub(pos, type, 1);
}

// FF3 0x001847e8
void *EffectSetTorch2NoSE(float *pos, int type)
{
    return EffectSetTorch2Sub(pos, type, 0);
}

// callcheck: EFFECT_FREE (read as a macro)
// FF3 0x00184800
void EffectResetTorch2(void *pTorch2)
{
    if (pTorch2 == nullptr)
        return;
    EFFECT_FREE(((EFFECT_CONT *)pTorch2)->pnt[1]);
    ResetEffects(pTorch2);
}

// FF3 0x00184860
void EffectTorch2SetAlphaRate(void *pTorch2, float rate)
{
    TORCH_CTRL *pTc;

    if (pTorch2 == nullptr)
        return;
    pTc = (TORCH_CTRL *)((EFFECT_CONT *)pTorch2)->pnt[1];
    if (pTc != nullptr)
        pTc->AlphaRate = rate;
}

// ──────────────────────────────────────────────────────────────────────
// The sparks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00184880
static void TorchSparkPartInit(float BrakeRate, float WholeScale, float AlphaRate, SPARK_CTRL *pSc, int Type)
{
    int i;

    if (pSc == nullptr)
        return;
    for (i = 0; i < 50; i++) {
        pSc->particles[i].position[0] = 0.0f;
        pSc->particles[i].position[1] = 0.0f;
        pSc->particles[i].position[2] = 0.0f;
        pSc->particles[i].position[3] = 1.0f;
        pSc->particles[i].color[3] = 0.0f;
        pSc->particles[i].lifetime = 0;
    }
    pSc->AlphaRate = AlphaRate;
    pSc->Type = (short)Type;
    pSc->BrakeRate = BrakeRate;
    pSc->WholeScale = WholeScale;
    pSc->head = 0;
    pSc->cnt = 0;
    pSc->disp = 0;
    pSc->blife = 0;
}

// Kind 0x19 (a burst's sparks): the whole burst is spawned on the first
// frame, then only ages; the effect ends the frame the last spark dies.
// ec->Pos the origin, uc8[2] the type, fw[0] the scale, fw[1] the alpha rate.
// callcheck: EFFECT_MALLOC (read as a macro)
// callcheck: rand (inside TorchGetRandom)
// FF3 0x001848e8
void SetSpark(EFFECT_CONT *ec)
{
    TORCH2_SPARK_PARAMETER *pSparkParam;
    int ReqNum;
    int i;

    pSparkParam = EffOthTorch2SparkParameterPtrGet(ec->dat.uc8[2]);
    ReqNum = (int)TorchGetRandom((float)pSparkParam->AppearNumMin, (float)pSparkParam->AppearNumMax);
    if (ec->pnt[1] == nullptr) {
        ec->pnt[1] = EFFECT_MALLOC(sizeof(SPARK_CTRL));
        if (ec->pnt[1] == nullptr) {
            ResetEffects(ec);
        } else {
            TorchSparkPartInit((float)pSparkParam->BrakeRate / 1000.0f, ec->fw[0], ec->fw[1],
                               (SPARK_CTRL *)ec->pnt[1], ec->dat.uc8[2]);
            for (i = 0; i < ReqNum; i++)
                AddSparkParticle(3.0f, (SPARK_CTRL *)ec->pnt[1], pSparkParam);
        }
    }
    ec->pnt[1] = ContTorchSpark(1300.0f, 3.3f, (SPARK_CTRL *)ec->pnt[1], ec->Pos, pSparkParam);
    if (ec->pnt[1] == nullptr)
        ResetEffects(ec);
}

// Draws and ages one burst; nullptr (block freed) once every spark is dead.
// The stop flag freezes the burst without ending it.
// callcheck: EFFECT_FREE (read as a macro)
// FF3 0x00184a68
static SPARK_CTRL *ContTorchSpark(float size, float sr, SPARK_CTRL *pSc, float *pos,
                                  TORCH2_SPARK_PARAMETER *pSparkParam)
{
    sceVu0FMATRIX work;
    sceVu0FMATRIX local_clip;
    sceVu0FMATRIX local_world;
    sceVu0FMATRIX local_screen;
    GRA3DCAMERA *pCam;

    pCam = gra3dGetCamera();
    if (pSc == nullptr)
        return nullptr;
    sceVu0UnitMatrix(work);
    work[0][0] = work[1][1] = work[2][2] = pSc->WholeScale;
    sceVu0TransMatrix(local_world, work, pos);
    sceVu0MulMatrix(local_screen, pCam->matWorldScreen, local_world);
    sceVu0MulMatrix(local_clip, pCam->matWorldClipPolygon, local_world);
    pSc->disp = (short)draw_distortion_particles2(local_screen, local_clip, 50, pSc->particles,
                                                  (size * sr * pSc->WholeScale) / pCam->fFov,
                                                  effdat[0x4c + eff_wrk.monochro_mode].tex0, 0x48);
    if (eff_wrk.stop_flg == 0
        && UpdateSparkParticles(pSc->BrakeRate, pSc->AlphaRate, pSc->particles, pSparkParam) == 0) {
        EFFECT_FREE(pSc);
        return nullptr;
    }
    return pSc;
}

// One spark at the burst's origin with a random speed, gravity and life.
// Scale is unused; the start scale is StartScale / 100 in integers (0 for
// every preset), as in FF2.
// callcheck: rand (inside TorchGetRandom)
// FF3 0x00184bc8
static void AddSparkParticle(float Scale, SPARK_CTRL *pSc, TORCH2_SPARK_PARAMETER *pSparkParam)
{
    PARTICLE *pPart = &pSc->particles[pSc->head];

    (void)Scale;
    pPart->position[0] = 0.0f;
    pPart->position[1] = 0.0f;
    pPart->position[2] = 0.0f;
    pPart->position[3] = 1.0f;
    pPart->velocity[0] = (TorchGetRandom((float)pSparkParam->SpeedXMin, (float)pSparkParam->SpeedXMax) - 1000.0f) / 100.0f;
    pPart->velocity[1] = (TorchGetRandom((float)pSparkParam->SpeedYMin, (float)pSparkParam->SpeedYMax) - 1000.0f) / 100.0f;
    pPart->acceleration[0] = 0.0f;
    pPart->velocity[2] = (TorchGetRandom((float)pSparkParam->SpeedZMin, (float)pSparkParam->SpeedZMax) - 1000.0f) / 100.0f;
    pPart->acceleration[2] = 0.0f;
    pPart->acceleration[1] = (TorchGetRandom((float)pSparkParam->AccelYMin, (float)pSparkParam->AccelYMax) - 1000.0f) / 1000.0f;
    pPart->color[0] = (float)pSparkParam->R;
    pPart->color[1] = (float)pSparkParam->G;
    pPart->color[2] = (float)pSparkParam->B;
    pPart->alp_step = 0.0f;
    pPart->color[3] = (float)pSparkParam->StartAlpha * pSc->AlphaRate;
    pPart->lifetime = (int)TorchGetRandom((float)pSparkParam->LifeTimeMin, (float)pSparkParam->LifeTimeMax);
    pPart->BaseLifeTime = pPart->lifetime;
    pPart->Scale = (float)(pSparkParam->StartScale / 100);
    pSc->head = (pSc->head + 1) % 50;
}

// Ages every spark (drag, then gravity; progress per particle) and answers
// how many are alive.
// FF3 0x00184e78
static int UpdateSparkParticles(float BrakeRate, float AlphaRate, PARTICLE *pPtop,
                                TORCH2_SPARK_PARAMETER *pSparkParam)
{
    float StartAlpha, EndAlpha, StartScale, EndScale;
    float Progress;
    int LeftNum = 0;
    int i;

    EndScale = (float)pSparkParam->EndScale / 100.0f;
    StartAlpha = (float)pSparkParam->StartAlpha * AlphaRate;
    StartScale = (float)pSparkParam->StartScale / 100.0f;
    EndAlpha = (float)pSparkParam->EndAlpha * AlphaRate;
    if (pPtop == nullptr)
        return LeftNum;
    for (i = 0; i < 50; i++) {
        if (pPtop[i].BaseLifeTime == 0)
            Progress = 1.0f;
        else
            Progress = 1.0f - (float)pPtop[i].lifetime / (float)pPtop[i].BaseLifeTime;
        if (pPtop[i].lifetime == 0) {
            pPtop[i].color[3] = 0.0f;
        } else {
            sceVu0AddVector(pPtop[i].position, pPtop[i].position, pPtop[i].velocity);
            LeftNum++;
            sceVu0ScaleVector(pPtop[i].velocity, pPtop[i].velocity, BrakeRate);
            sceVu0AddVector(pPtop[i].velocity, pPtop[i].velocity, pPtop[i].acceleration);
            pPtop[i].lifetime--;
            pPtop[i].color[3] = (EndAlpha - StartAlpha) * Progress + StartAlpha;
            pPtop[i].Scale = (EndScale - StartScale) * Progress + StartScale;
        }
    }
    return LeftNum;
}

// ──────────────────────────────────────────────────────────────────────
// The burst
// ──────────────────────────────────────────────────────────────────────

// Interval 0: throws a spark burst (kind 0x19) at the base and starts the
// envelope; otherwise counts the interval down while no burst runs. -1
// (never) is never 0 and never counted. FF3 dropped FF2's burst sound.
// callcheck: fptodp (the double arguments)
// FF3 0x00185018
static void EffOthTorch2BurstReqCtrl(float WholeScale, float AlphaRate, TORCH_CTRL *pTc)
{
    if (pTc->BurstInterval == 0) {
        SetEffects(0x19, 2, pTc->BasePos, (int)pTc->Type, (double)WholeScale, (double)AlphaRate);
        pTc->BurstCount = 1;
        pTc->BurstInterval = EffOthTorch2BurstIntervalTimeGet(pTc->Type);
    } else if (pTc->BurstCount == 0 && pTc->BurstInterval > 0) {
        pTc->BurstInterval--;
    }
}

// Frames to the next burst, -1 when the preset never bursts.
// callcheck: rand (inside TorchGetRandom)
// FF3 0x001850d0
static int EffOthTorch2BurstIntervalTimeGet(int Type)
{
    TORCH2_BURST_PARAMETER *pBurstParam = EffOthTorch2BurstParameterPtrGet(Type);

    if (pBurstParam->IntervalMax == 0 && pBurstParam->IntervalMin == 0)
        return -1;
    return (int)TorchGetRandom((float)pBurstParam->IntervalMin, (float)pBurstParam->IntervalMax);
}

// The burst envelope at frame Count: rest -> 1 over Frame1, 1 -> 2 over
// Frame2, back to rest by EndFrame. 0 (and the resting values) outside it.
// FF3 0x00185168
static int Torch2BurstRangeAndStartScaleGet(float *pRange, float *pStartScale, int Count,
                                            TORCH2_PARAMETER *pParam, TORCH2_BURST_PARAMETER *pBurstParam)
{
    int Frame1 = pBurstParam->Frame1;
    int Frame2 = pBurstParam->Frame2;
    int EndFrame = pBurstParam->EndFrame;
    float StartScale2 = (float)pBurstParam->StartScale2 / 100.0f;
    float BaseRange = (float)pParam->Range / 100.0f;
    float Range1 = (float)pBurstParam->Range1 / 100.0f;
    float Range2 = (float)pBurstParam->Range2 / 100.0f;
    float BaseScale = (float)pParam->StartScale / 100.0f;
    float StartScale1 = (float)pBurstParam->StartScale1 / 100.0f;
    float Progress;

    if (Count < 1 || Count >= EndFrame) {
        *pRange = BaseRange;
        *pStartScale = BaseScale;
        return 0;
    }
    if (Count < Frame1) {
        Progress = (Frame1 == 0) ? 1.0f : (float)Count / (float)Frame1;
        *pRange = (Range1 - BaseRange) * Progress + BaseRange;
        *pStartScale = (StartScale1 - BaseScale) * Progress + BaseScale;
    } else if (Count < Frame2) {
        Progress = (Frame2 == Frame1) ? 1.0f : (float)(Count - Frame1) / (float)(Frame2 - Frame1);
        *pRange = (Range2 - Range1) * Progress + Range1;
        *pStartScale = (StartScale2 - StartScale1) * Progress + StartScale1;
    } else {
        Progress = (EndFrame == Frame2) ? 1.0f : (float)(Count - Frame2) / (float)(EndFrame - Frame2);
        *pRange = (BaseRange - Range2) * Progress + Range2;
        *pStartScale = (BaseScale - StartScale2) * Progress + StartScale2;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The wind (dead: EffOthTorch2WindIntervalTimeGet answers -1)
// ──────────────────────────────────────────────────────────────────────

// A 0.15-long gust along +z, pitched 0..10 degrees ((r * 10 + 90) - 90, the
// ROM's way round) and spun to a random heading.
// callcheck: rand (inside TorchRand01)
// FF3 0x00185330
static void Torch2WindVectorMake(float *Wind)
{
    sceVu0FMATRIX TmpMat;
    sceVu0FVECTOR TmpVec;
    float RotX, RotY;

    TmpVec[0] = 0.0f;
    TmpVec[1] = 0.0f;
    TmpVec[3] = 1.0f;
    TmpVec[2] = (TorchRand01() * 0.0f + 15.0f) / 100.0f;      // EffectGetRandom(15, 15) / 100
    RotX = (((TorchRand01() * 10.0f + 90.0f) - 90.0f) * 3.1415927f) / 180.0f;
    RotY = TorchRand01() * 6.2831855f - 3.1415927f;
    sceVu0UnitMatrix(TmpMat);
    sceVu0RotMatrixX(TmpMat, TmpMat, RotX);
    sceVu0RotMatrixY(TmpMat, TmpMat, RotY);
    sceVu0ApplyMatrix(Wind, TmpMat, TmpVec);
}

// FF3 0x001854b0
static void Torch2WindReq(TORCH_CTRL *pTc)
{
    Torch2WindVectorMake(pTc->WindMax);
    pTc->WindCount = 1;
}

// FF3 0x001854e0
static int EffOthTorch2WindIntervalTimeGet(void)
{
    return -1;
}

// FF3 0x001854e8
static void Torch2WindReqCtrl(TORCH_CTRL *pTc)
{
    if (pTc->WindInterval == 0) {
        Torch2WindReq(pTc);
        pTc->WindInterval = EffOthTorch2WindIntervalTimeGet();
    } else if (pTc->WindCount == 0 && pTc->WindInterval > 0) {
        pTc->WindInterval--;
    }
}

// The gust at frame Count: full for 1..5, a linear fade to 35, 0 after.
// FF3 0x00185540
static int Torch2WindVectorGet(float *NowWind, float *WindMax, int Count)
{
    NowWind[0] = 0.0f;
    NowWind[1] = 0.0f;
    NowWind[2] = 0.0f;
    NowWind[3] = 1.0f;
    if (Count < 1)
        return 0;
    if (Count < 6) {
        TorchCopyVector(NowWind, WindMax);
        return 1;
    }
    if (Count > 35)
        return 0;
    sceVu0ScaleVector(NowWind, WindMax, (float)(30 - (Count - 6)) / 30.0f);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The light shaft
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001855e8
void EffectTorch2BigFreaInit(void)
{
    Torch2BigFreaCtrl.ExecFlg = 0;
    Torch2BigFreaCtrl.Count = 0;
    Torch2BigFreaCtrl.Cycle = 0;
}

// Re-posts the shaft (kind 0x1a, one frame) while ExecFlg is set: alpha a
// triangle 18 -> 35 -> 18 over 2 * Cycle frames (35 with Cycle 0).
// callcheck: fptodp (the double arguments)
// FF3 0x00185600
void EffectTorch2BigFreaMain(void)
{
    sceVu0FVECTOR Position;
    sceVu0FVECTOR Offset;
    int Alpha;

    if (Torch2BigFreaCtrl.ExecFlg == 0)
        return;
    Offset[0] = Offset[1] = Offset[2] = Offset[3] = 0.0f;
    sceVu0AddVector(Position, Torch2BigFreaCtrl.Position, Offset);
    Alpha = 35;
    if (Torch2BigFreaCtrl.Cycle != 0) {
        if (Torch2BigFreaCtrl.Count < Torch2BigFreaCtrl.Cycle)
            Alpha = (Torch2BigFreaCtrl.Count * 17) / Torch2BigFreaCtrl.Cycle + 18;
        else
            Alpha = 35 - ((Torch2BigFreaCtrl.Count - Torch2BigFreaCtrl.Cycle) * 17) / Torch2BigFreaCtrl.Cycle;
    }
    SetEffects(0x1a, 1, Position, 0x8f, 0x32, 0x26, 69.5, (double)77.1f, Alpha);
    Torch2BigFreaCtrl.Count++;
    if (Torch2BigFreaCtrl.Count >= Torch2BigFreaCtrl.Cycle * 2)
        Torch2BigFreaCtrl.Count = 0;
}

// No caller in FF3 (FF2's opening used it), so the shaft never runs.
// FF3 0x00185718
void EffectTorch2BigFreaReq(float *Position)
{
    TorchCopyVector(Torch2BigFreaCtrl.Position, Position);
    Torch2BigFreaCtrl.Count = 0;
    Torch2BigFreaCtrl.Cycle = 0x37;
    Torch2BigFreaCtrl.ExecFlg = 1;
}

// FF3 0x00185740
void EffectTorch2BigFreaCut(void)
{
    Torch2BigFreaCtrl.ExecFlg = 0;
}

// Kind 0x1a: the shaft's quad; one-shot records (uc8[1] bit 0) end here.
// FF3 0x00185750
void SetTorch2BigFrea(EFFECT_CONT *ec)
{
    sceVu0FVECTOR Position;

    TorchCopyVector(Position, (float *)ec->pnt[0]);
    Torch2BigFreaDraw(ec->fw[0], ec->fw[1], Position, ec->r, ec->g, ec->b, ec->a);
    if ((ec->dat.uc8[1] & 1) != 0)
        ResetEffects(ec);
}

// A quad of effdat 0x40 turned by the camera's pitch and heading
// (Get2PosRot from the camera to its target), additive.
// FF3 0x001857b8
static void Torch2BigFreaDraw(float ScaleX, float ScaleY, float *Position, u_char R, u_char G, u_char B,
                              u_char Alpha)
{
    u_long DrawEnv[6];
    sceVu0FMATRIX matWorldLocal;
    GRA3DCAMERA *pCam;
    float RotX, RotY;

    DrawEnv[0] = 0x161;                     // TEX1
    DrawEnv[1] = 0x48;                      // ALPHA
    DrawEnv[2] = 0x10a000118ULL;            // ZBUF
    DrawEnv[3] = 0x5000d;                   // TEST
    DrawEnv[4] = 0;                         // CLAMP
    DrawEnv[5] = 0x302a400000008004ULL;     // the strip's GIF tag
    pCam = gra3dGetCamera();
    Get2PosRot(gra3dcamGetPosition(), pCam->vTarget, &RotX, &RotY);
    sceVu0UnitMatrix(matWorldLocal);
    sceVu0RotMatrixX(matWorldLocal, matWorldLocal, RotX);
    sceVu0RotMatrixY(matWorldLocal, matWorldLocal, RotY);
    sceVu0TransMatrix(matWorldLocal, matWorldLocal, Position);
    Set3DPosTexure(ScaleX * 100.0f, ScaleY * 100.0f, matWorldLocal, DrawEnv, 0x40, R, G, B, Alpha);
}
