// ingame/ingame_effect.c -- the in-game side of the effect system (FF2
// ingame_effect.c, ported from MioPan and checked against FF3 0x1e8738..
// 0x1ea2b0 + 0x2bcc98; see ingame_effect.h for the map of the file).
//
// FF3 differences from FF2 worth knowing:
//   - The parts-deform is gone: the job table still counts and fires, but
//     each *PDeformReq returns nullptr (no CallPartsDeform6, no cue), so a
//     fired slot is freed on the next frame. The zero shot's particle burst
//     (IgEffectZeroParticleReq) is empty too.
//   - The suction point follows the finder flag / first-person view, not
//     plyr mode 6, and the no-finder point is the model's joint-2 coord.
//   - The lens flare off the player's spotlight became the flashlight's own
//     flare, drawn from the player's map-put 0x1194 (IgEffectPlyrPut*).
//   - New: the high-contrast fade level (FADE_HIGHCONTRAST) that mixes the
//     candle-out looks' contrast, and the candle flicker near a moving
//     player (FF2's EffectCandleFlameYuramekiCtrl call, moved here).
//   - The lantern base no longer draws its model (the register save /
//     restore stays), and the lantern flame sits 9599 units off (see there).

#include "ingame/ingame_effect.h"

#include "common/vu0math.h"                     // RotLimitChk, NormalizeAngle
#include "graphics/effect/effect.h"             // SetEffects, EffectExecCheck, eff_wrk, ...
#include "graphics/effect/effect_butterfly.h"
#include "graphics/effect/effect_data.h"        // HandLightEffectIsDisp
#include "graphics/effect/effect_ene.h"         // EneDmgLargeHitReq, ENE_DMG_PARTICLE_REQ
#include "graphics/effect/effect_obj.h"         // TOUROU_*, LIGHT_COME_IN_DATA, EffRenzFlareDraw, ...
#include "graphics/effect/effect_sub.h"         // Vector2Rot, Get2PosRot, Set3DPosTexure, WorldToScreenPos
#include "graphics/game_model.h"                // GameModel
#include "graphics/graph3d/gra3d.h"             // gra3dcamGetDirection, fog, light
#include "graphics/graph3d/gra3dSGD.h"          // gra3dSetVertexColorPreset
#include "graphics/graph3d/sgd_types.h"         // SGDCOORDINATE, SGDFILEHEADER
#include "graphics/gs_reg.h"                    // GetGsRegCached / SetGsRegCached
#include "graphics/mmanage.h"                   // ModelManager::Draw
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/map/MapObjMdl.h"               // MAP_OBJ_MDL
#include "ingame/map/MapPut.h"
#include "ingame/photo/fndr_parts.h"            // CFadeVariable, PhotoShakeReq
#include "ingame/plyr/acs_model.h"              // AcsModel.alpha
#include "ingame/plyr/player.h"                 // PLYR_WRK, PLYR_FLG_*
#include "ingame/story_menu.h"                  // IsLockTblOn
#include "reipan/os/reipan_rand.h"              // ReiPan_Rand, REIPAN_RAND_MAXF
#include "sdk/libgraph.h"                       // SCE_GS_*
#include "system/eeiop/cd_file_dat.h"           // FOOT_44_0_SUIMEN_BD_BD
#include <libvu0.h>
#include <stdint.h>                             // intptr_t

// The light motion (the flashlight in hand; plyr_mdl.c / plyr_sub.c read the
// same bit).
#define IG_PLYR_FLG_LIGHT   0x8000

// EffectExecCheck's kind for a parts-deform (FF2 0x18).
#define IG_EFFECT_KIND_PDEFORM  0x16

// The player's map-put buffer.
#define IG_PLYR_PUT_BUFF    0x1194

// ---- file statics ----------------------------------------------------------

static sceVu0FVECTOR ParticleEndPosFinder;                      // bss 0x3ef170
static sceVu0FVECTOR ParticleEndPosNoFinder;                    // bss 0x3ef180
static SUBFUNC_PDEFORM_CTRL SubFuncPDeformCtrl[SUBFUNC_PDEFORM_NUM];    // bss 0x3ef190
static sceVu0FVECTOR HandLightFlarePos;                         // bss 0x3ef280
static sceVu0FVECTOR HandLightFlareRot;                         // bss 0x3ef290: (-x, y, 0, 0) of the aim
static ZERO_PARTICLE_CTRL ZeroParticleCtrl;                     // sbss 0x3733e8
static float HandLightFlareLevel;                               // sbss 0x3733f0: 0..1
static CFadeVariable<short> HighContrast;                       // sbss 0x3733f8
static MAP_PUT *PlyrPut;                                        // sbss 0x373400

// File-local functions defined below their first caller.
static void IgEffectSubFuncPDeformReqSet(int EneWrkNo, int Type, int StartFrame);
static void IgEffectSubFuncPDeformMain(void);
static void IgEffectZeroParticleMain(void);
static void IgEffectZeroParticleReq(int num);

// ---- lifecycle -------------------------------------------------------------

// FF3 0x001e8738; FF2 IgEffectInit. FF3 clears the flare level and the high
// contrast and forgets the player's put instead of RenzFlareDispFlg.
// callcheck: IgEffectHighContrastInit (the call the stage lists as IgEffectRenzFlareDispFlgSet, 0x1e9610)

void IgEffectInit(void)
{
    int i;

    for (i = 0; i < SUBFUNC_PDEFORM_NUM; i++)
    {
        SubFuncPDeformCtrl[i].Status = SUBFUNC_PDEFORM_STATUS_NOT_USE;
        SubFuncPDeformCtrl[i].Count  = 0;
    }

    ZeroParticleCtrl.Count = -1;
    IgEffectHandLightFlareInit();
    IgEffectHighContrastInit();
    PlyrPut = nullptr;
}

// FF3 0x001e8790; FF2 IgEffectMain. The finder point is used while the camera
// is up or the view is first-person (_0e0 bit 0); the lens flare request is
// gone (the flashlight's flare is drawn by the player's put), the candle
// flicker gets the player's position and last position, and the high
// contrast steps last.

void IgEffectMain(void)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;

    if ((plyr->flags & PLYR_FLG_800000) == 0 && (plyr->_0e0 & 1) == 0)
    {
        IgEffectParticleEndPosCalcNoFinder(ParticleEndPosNoFinder);
        EneDmgParticleEndPosSet(IgEffectParticleEndPosNoFinderGet());
    }
    else
    {
        IgEffectParticleEndPosCalcFinder(ParticleEndPosFinder);
        EneDmgParticleEndPosSet(IgEffectParticleEndPosFinderGet());
    }

    IgEffectSubFuncPDeformMain();
    IgEffectZeroParticleMain();
    IgEffectHandLightFlareCtrl();
    IgEffectCandleYuramekiCtrl(ingame_wrk.plyr_wrk->pos, ingame_wrk.plyr_wrk->pos_old);
    IgEffectHighContrastWork();
}

// FF3 0x001e8838 (FF3-only): EffectsReleaseAll's last call.

void IgEffectRelease(void)
{
    IgEffectHighContrastReset();
    IgEffectPlyrPutDelete();
}

// ---- the candle flicker near a moving player -------------------------------------

// FF3 0x001e8858. Once the player moved 0.8 units or more since last frame,
// every candle fire (model kind 0x14) is offered the position
// (EffCandleFireNearCheck flickers those close by). The square length sums
// x, y and z only.

void IgEffectCandleYuramekiCtrl(float *pos, float *pos_old)
{
    EFFECT_CONT *ec = EffectGetModelCont(0);
    sceVu0FVECTOR d;
    int i;

    d[0] = (pos[0] - pos_old[0]) * (pos[0] - pos_old[0]);
    d[1] = (pos[1] - pos_old[1]) * (pos[1] - pos_old[1]);
    d[2] = (pos[2] - pos_old[2]) * (pos[2] - pos_old[2]);

    if (d[0] + d[1] + d[2] < 0.64f)
        return;

    for (i = 0; i < EFFECT_MODEL_NUM; i++, ec++)
    {
        if (ec->dat.uc8[0] == 0x14)
            EffCandleFireNearCheck(ec, pos);
    }
}

// ---- the flashlight's lens flare --------------------------------------------------

// FF3 0x001e8920.

void IgEffectHandLightFlareInit(void)
{
    HandLightFlareLevel = 0.0f;
}

// FF3 0x001e8928. The flare fades in (0.125 a frame, to 1) while the light
// motion is on outside the viewfinder (neither the camera up nor going up),
// the player is not in mode 3 (held), not in the mode-10 stance, and the
// hand light shows (HandLightEffectIsDisp); otherwise it fades out to 0.

void IgEffectHandLightFlareCtrl(void)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    int fade_out = 0;
    float level;

    if ((plyr->flags & (PLYR_FLG_800000 | PLYR_FLG_400000 | IG_PLYR_FLG_LIGHT)) == IG_PLYR_FLG_LIGHT
        && plyr->mode != PLYR_MODE_DMG3)
    {
        if ((plyr->flags & PLYR_FLG_MODE10) != 0)
            fade_out = 1;
        else if (HandLightEffectIsDisp() == 0)
            fade_out = 1;
    }
    else
    {
        fade_out = 1;
    }

    if (fade_out)
    {
        if (HandLightFlareLevel <= 0.0f)
            return;
        level = HandLightFlareLevel - 0.125f;
        HandLightFlareLevel = (level < 0.0f) ? 0.0f : level;
    }
    else
    {
        if (1.0f <= HandLightFlareLevel)
            return;
        level = HandLightFlareLevel + 0.125f;
        HandLightFlareLevel = (1.0f < level) ? 1.0f : level;
    }
}

// FF3 0x001e89f8. With the light motion on, not crawling, not in the
// viewfinder or the mode-10 stance, not first-person and the model in use:
// the flare at the flashlight's lens, aimed where it points. Its core is the
// flashlight model's alpha / 128 (0 when the put draws with flag 0x200), the
// ghosts that times the fade level. The ROM tests the work pointer only
// after reading its flags.

void IgEffectHandLightFlareDraw(u_int flags)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    AcsModel *light;
    float rx;
    float ry;
    float flare;
    float other;

    if ((plyr->flags & IG_PLYR_FLG_LIGHT) == 0)
        return;
    if ((plyr->flags & (PLYR_FLG_10000000 | PLYR_FLG_8000000 | PLYR_FLG_800000 | PLYR_FLG_MODE10)) != 0)
        return;
    if ((plyr->_0e0 & 1) != 0)
        return;
    if (plyr == nullptr || plyr->_2e4 == 0)
        return;

    light = (AcsModel *)plyr->acs_mdl[0];
    IgEffectHandLightFlarePosRot(light, HandLightFlarePos, &rx, &ry);
    HandLightFlareRot[2] = 0.0f;
    HandLightFlareRot[0] = -rx;
    HandLightFlareRot[1] = ry;
    HandLightFlareRot[3] = 0.0f;

    flare = light->alpha * (1.0f / 128.0f);
    other = flare * HandLightFlareLevel;
    if ((flags & 0x200) != 0)
        flare = 0.0f;
    EffRenzFlareDraw(flare, other, HandLightFlarePos, HandLightFlareRot, 4);
}

// FF3 0x001e8ae8. The lens: a point in the model's coordinate 0; the aim:
// from it towards the coordinate's origin, trimmed by two small constant
// angles. Also FUN_00256040.

void IgEffectHandLightFlarePosRot(GameModel *mdl, float *pos, float *rx, float *ry)
{
    sceVu0FVECTOR lens = { 2.08999896f, -1.00001192f, 0.0899999961f, 1.0f };
    sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };         // data 0x329500
    sceVu0FVECTOR base;

    if (mdl == nullptr)
        return;

    mdl->GetCoordPos(pos, 0, lens);
    mdl->GetCoordPos(base, 0, origin);
    Get2PosRot(pos, base, rx, ry);
    *rx += -0.298404992f;
    *ry += 0.00159499992f;
    RotLimitChk(rx);
    RotLimitChk(ry);
}

// ---- the spirit-particle suction point ----------------------------------------------

// FF3 0x001e8c08.

float (*IgEffectParticleEndPosFinderGet(void))[4]
{
    return &ParticleEndPosFinder;
}

// FF3 0x001e8c18.

float (*IgEffectParticleEndPosNoFinderGet(void))[4]
{
    return &ParticleEndPosNoFinder;
}

// FF3 0x001e8c28; FF2's. FF3 pitches by the viewfinder's pitch, turns by the
// facing and stands on the view's height over the player (crawl_h).
// callcheck: sceVu0UnitMatrix, sceVu0ApplyMatrix, sceVu0TransMatrix (inline VU0 code in the ROM)

void IgEffectParticleEndPosCalcFinder(float *EndPos)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR ppp = { -8.81199932f, 9.10399914f, 12.7919998f, 1.0f };
    sceVu0FVECTOR trans;
    sceVu0FMATRIX wlm;

    trans[0] = plyr->pos[0];
    trans[1] = plyr->pos[1] + plyr->crawl_h;
    trans[2] = plyr->pos[2];
    trans[3] = plyr->pos[3];

    sceVu0UnitMatrix(wlm);
    sceVu0RotMatrixX(wlm, wlm, plyr->finder_pitch);
    sceVu0RotMatrixY(wlm, wlm, ingame_wrk.plyr_wrk->rot[1]);
    sceVu0TransMatrix(wlm, wlm, trans);
    sceVu0ApplyMatrix(EndPos, wlm, ppp);
}

// FF3 0x001e8d50; FF2's twin. FF3: a point in the player model's joint-2
// coordinate, or the player's position while no model is in use.

void IgEffectParticleEndPosCalcNoFinder(float *EndPos)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR ppp = { 2.0f, 1.6f, 0.0f, 1.0f };
    GameModel *mdl;

    if (plyr->_2e4 == 0)
    {
        sceVu0CopyVector(EndPos, plyr->pos);
    }
    else
    {
        mdl = plyr->mdl;
        mdl->GetCoordPos(EndPos, mdl->GetJointCoord(2), ppp);
    }
}

// ---- the camera sub-function hits ------------------------------------------------------
// FF2's, without the cues (FF2's FinderBankPlay after the job). The job's
// delay in frames is the last argument of IgEffectSubFuncPDeformReqSet.

// FF3 0x001e8e08; FF2's.

void IgEffectSubFuncZeroReq(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 6;
        EffectTypeB = 7;
    }
    else if (type == 1)
    {
        EffectTypeA = 8;
        EffectTypeB = 9;
    }
    else
    {
        EffectTypeA = 10;
        EffectTypeB = 11;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_ZERO, 3);
}

// FF3 0x001e8e78; FF2's.

void IgEffectSubFuncSlowReq(int num)
{
    EneDmgLargeHitReq(4);
    EneDmgLargeHitReq(5);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_SLOW, 32);
}

// FF3 0x001e8eb8; FF2's.

void IgEffectSubFuncKokuReq(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 12;
        EffectTypeB = 13;
    }
    else if (type == 1)
    {
        EffectTypeA = 14;
        EffectTypeB = 15;
    }
    else
    {
        EffectTypeA = 16;
        EffectTypeB = 17;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_KOKU, 5);
}

// FF3 0x001e8f28; FF2's.

void IgEffectSubFuncParalyzeReq(int num)
{
    EneDmgLargeHitReq(18);
    EneDmgLargeHitReq(19);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_PARALYZE, 22);
}

// FF3 0x001e8f68; FF2's.

void IgEffectSubFuncViewReq(int num)
{
    EneDmgLargeHitReq(20);
    EneDmgLargeHitReq(21);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_VIEW, 46);
}

// FF3 0x001e8fa8; FF2's.

void IgEffectSubFuncMetsuReq(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 22;
        EffectTypeB = 23;
    }
    else if (type == 1)
    {
        EffectTypeA = 24;
        EffectTypeB = 25;
    }
    else
    {
        EffectTypeA = 26;
        EffectTypeB = 27;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_METSU, 5);
}

// FF3 0x001e9018; FF2's.

void IgEffectSubFuncRenReq(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 28;
        EffectTypeB = 29;
    }
    else if (type == 1)
    {
        EffectTypeA = 30;
        EffectTypeB = 31;
    }
    else
    {
        EffectTypeA = 32;
        EffectTypeB = 33;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_REN, 0);
}

// FF3 0x001e9088; FF2's.

void IgEffectSubFuncTsuiReq(int num)
{
    EneDmgLargeHitReq(34);
    EneDmgLargeHitReq(35);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_TSUI, 25);
}

// FF3 0x001e90c8; FF2's.

void IgEffectSubFuncFuuReq(int num)
{
    EneDmgLargeHitReq(36);
    EneDmgLargeHitReq(37);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_FUU, 25);
}

// FF3 0x001e9108 (FF3-only, the graded shape of Zero).

void IgEffectSubFuncEx1Req(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 38;
        EffectTypeB = 39;
    }
    else if (type == 1)
    {
        EffectTypeA = 40;
        EffectTypeB = 41;
    }
    else
    {
        EffectTypeA = 42;
        EffectTypeB = 43;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_EX1, 3);
}

// FF3 0x001e9178 (FF3-only).

void IgEffectSubFuncEx2Req(int num, int type)
{
    int EffectTypeA;
    int EffectTypeB;

    if (type == 0)
    {
        EffectTypeA = 44;
        EffectTypeB = 45;
    }
    else if (type == 1)
    {
        EffectTypeA = 46;
        EffectTypeB = 47;
    }
    else
    {
        EffectTypeA = 48;
        EffectTypeB = 49;
    }

    EneDmgLargeHitReq(EffectTypeA);
    EneDmgLargeHitReq(EffectTypeB);

    IgEffectSubFuncPDeformReqSet(num, PDEFORM_TYPE_EX2, 3);
}

// ---- the parts-deform job table -----------------------------------------------------------

// FF3 0x001e91e8; FF2's: the first free slot, or the request is dropped.

static void IgEffectSubFuncPDeformReqSet(int EneWrkNo, int Type, int StartFrame)
{
    int i;

    for (i = 0; i < SUBFUNC_PDEFORM_NUM; i++)
    {
        if (SubFuncPDeformCtrl[i].Status == SUBFUNC_PDEFORM_STATUS_NOT_USE)
        {
            SubFuncPDeformCtrl[i].EneWrkNo   = EneWrkNo;
            SubFuncPDeformCtrl[i].Status     = SUBFUNC_PDEFORM_STATUS_USE;
            SubFuncPDeformCtrl[i].Type       = Type;
            SubFuncPDeformCtrl[i].StartFrame = StartFrame;
            SubFuncPDeformCtrl[i].Count      = 0;
            SubFuncPDeformCtrl[i].pEffRet    = nullptr;
            return;
        }
    }
}

// The per-type deform requests. FF2 raised a CallPartsDeform6 and a cue in
// each; FF3 compiled them to "return 0".

static void *IgEffectZeroPDeformReq(int EneWrkNo)       // FF3 0x001e9238
{
    return nullptr;
}

static void *IgEffectSlowPDeformReq(int EneWrkNo)       // FF3 0x001e9240
{
    return nullptr;
}

static void *IgEffectKokuPDeformReq(int EneWrkNo)       // FF3 0x001e9248
{
    return nullptr;
}

static void *IgEffectParalyzePDeformReq(int EneWrkNo)   // FF3 0x001e9250
{
    return nullptr;
}

static void *IgEffectViewPDeformReq(int EneWrkNo)       // FF3 0x001e9258
{
    return nullptr;
}

static void *IgEffectMetsuPDeformReq(int EneWrkNo)      // FF3 0x001e9260
{
    return nullptr;
}

static void *IgEffectRenPDeformReq(int EneWrkNo)        // FF3 0x001e9268
{
    return nullptr;
}

static void *IgEffectTsuiPDeformReq(int EneWrkNo)       // FF3 0x001e9270
{
    return nullptr;
}

static void *IgEffectFuuPDeformReq(int EneWrkNo)        // FF3 0x001e9278
{
    return nullptr;
}

static void *IgEffectEx1PDeformReq(int EneWrkNo)        // FF3 0x001e9280
{
    return nullptr;
}

static void *IgEffectEx2PDeformReq(int EneWrkNo)        // FF3 0x001e9288
{
    return nullptr;
}

// FF3 0x001e9290; FF2's.

void IgEffectSubFuncPDeformClear(int EneWrkNo)
{
    int i;

    for (i = 0; i < SUBFUNC_PDEFORM_NUM; i++)
    {
        if (SubFuncPDeformCtrl[i].Status != SUBFUNC_PDEFORM_STATUS_NOT_USE
            && SubFuncPDeformCtrl[i].EneWrkNo == EneWrkNo)
        {
            ResetEffects(SubFuncPDeformCtrl[i].pEffRet);
            SubFuncPDeformCtrl[i].pEffRet = nullptr;
            SubFuncPDeformCtrl[i].Status  = SUBFUNC_PDEFORM_STATUS_NOT_USE;
        }
    }
}

// FF3 0x001e9308; FF2's: retire the finished jobs, then count the waiting
// ones up to their start frame and fire them (each clears the ghost's other
// jobs first). FREQ (11, never requested) shakes the camera every frame
// without leaving USE, as in FF2.

static void IgEffectSubFuncPDeformMain(void)
{
    SUBFUNC_PDEFORM_CTRL *pPDeformCtrl;
    int i;

    for (i = 0; i < SUBFUNC_PDEFORM_NUM; i++)
    {
        pPDeformCtrl = &SubFuncPDeformCtrl[i];

        if (pPDeformCtrl->Status == SUBFUNC_PDEFORM_STATUS_WAIT)
        {
            if (EffectExecCheck(pPDeformCtrl->pEffRet, IG_EFFECT_KIND_PDEFORM) == 0)
                pPDeformCtrl->pEffRet = nullptr;

            if (pPDeformCtrl->pEffRet == nullptr)
                pPDeformCtrl->Status = SUBFUNC_PDEFORM_STATUS_NOT_USE;
        }
    }

    for (i = 0; i < SUBFUNC_PDEFORM_NUM; i++)
    {
        pPDeformCtrl = &SubFuncPDeformCtrl[i];

        if (pPDeformCtrl->Status != SUBFUNC_PDEFORM_STATUS_USE)
            continue;

        if (pPDeformCtrl->Count != pPDeformCtrl->StartFrame)
        {
            pPDeformCtrl->Count++;
            continue;
        }

        switch (pPDeformCtrl->Type)
        {
        case PDEFORM_TYPE_ZERO:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectZeroPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_SLOW:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectSlowPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_KOKU:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectKokuPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_PARALYZE:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectParalyzePDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_VIEW:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectViewPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_METSU:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectMetsuPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_REN:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectRenPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_TSUI:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectTsuiPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_FUU:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectFuuPDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_EX1:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectEx1PDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_EX2:
            IgEffectSubFuncPDeformClear(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->pEffRet = IgEffectEx2PDeformReq(pPDeformCtrl->EneWrkNo);
            pPDeformCtrl->Status  = SUBFUNC_PDEFORM_STATUS_WAIT;
            break;
        case PDEFORM_TYPE_FREQ:
            IgEffectZeroCamFreqReq();
            break;
        }
    }
}

// ---- the zero shot ------------------------------------------------------------------------

// FF3 0x001e9578; FF2's: the burst eight frames after the request. Nothing in
// FF3 arms it (FF2's IgEffectEneDmgReqZero is gone), and the burst is empty.

static void IgEffectZeroParticleMain(void)
{
    if (ZeroParticleCtrl.Count == -1)
        return;

    if (ZeroParticleCtrl.Count == 8)
    {
        IgEffectZeroParticleReq(ZeroParticleCtrl.EneWrkNo);
        ZeroParticleCtrl.Count = -1;
    }
    else
    {
        ZeroParticleCtrl.Count++;
    }
}

// FF3 0x001e95d0: FF2's 48-particle burst, empty in FF3.

static void IgEffectZeroParticleReq(int num)
{
}

// FF3 0x001e95d8; FF2 called ReqFreqCamera.

void IgEffectZeroCamFreqReq(void)
{
    PhotoShakeReq();
}

// FF3 0x001e95f0; FF2's without the equip tray test. No caller in FF3.

int IgEffectIsEndParticleSuck(void)
{
    return IsActiveEneDmgParticle() == 0;
}

// ---- the high-contrast level ------------------------------------------------------------

// FF3 0x001e9610.

void IgEffectHighContrastInit(void)
{
    HighContrast.val    = 0;
    HighContrast.speed  = 0;
    HighContrast.target = 0;
}

// FF3 0x001e9628.

void IgEffectHighContrastWork(void)
{
    HighContrast.Work();
}

// FF3 0x001e9640.

void IgEffectHighContrastReset(void)
{
    IgEffectHighContrastInit();
}

// FF3 0x001e9658 (EvFadeHighcontrast).

void IgEffectHighContrastFade(short level, u_short time)
{
    if (HighContrast.target != level)
        HighContrast.Fade(level, (short)time);
}

// ---- this frame's screen look --------------------------------------------------------------

// FF3 0x001e96a8; FF2's, reworked. Gated by eff_wrk._18 (FF2: the debug
// menu switch; 0 while a story movie plays). FF3 adds the room glow (kind
// 0x10) and kind 0x11, passes the blur alpha by value, takes the blur centre
// from the player's screen point (the centre while the camera is up or the
// view is first-person), replaces blur and deform by a fixed pair in the
// slow mode (PLYR_FLG_100000), and blends the contrast of the candle-out
// looks towards (84, 124) by the high-contrast level. The black filter's
// argument is the filter value itself (FF2 left it in a2 by accident).
// callcheck: fptodp (the double slots of SetEffects)

void IgEffectStoryMainScreenEffectReq(SCREEN_EFFECT_PARAMETER *prm)
{
    PLYR_WRK *plyr;
    float tx;
    float ty;
    int type;
    int col;
    int alp;

    if ((u_char)eff_wrk._18 == 0)
        return;

    if (prm->Z_Dep == 1)
        SetEffects(1, 1);

    if (prm->ex.RoomGlow == 1)
        SetEffects(0x10, 1);

    if (prm->ex.Kind11 == 1)
    {
        double rate  = (float)prm->ex.Kind11Rate / 1000.0f;
        double rate2 = (float)prm->ex.Kind11Rate2 / 1000.0f;

        SetEffects(0x11, 1, prm->ex.Kind11Col, rate, prm->ex.Kind11Col2, rate, rate2);
    }

    if ((u_int)(prm->Dither - 1) < 8 && EffectIsSlotNo(2) == 0)
    {
        SetEffects(2, 1, prm->Dither, (double)(float)prm->DitherAlpha, (double)(float)prm->DitherSpeed,
                   prm->DitherAlphaMax, prm->DitherColorMax);
    }

    plyr = ingame_wrk.plyr_wrk;
    if ((plyr->flags & PLYR_FLG_100000) != 0)
    {
        SetEffects(3, 1, 4, 0x80, 0x3f7, 0x708, 320.0, 224.0);
        SetEffects(4, 1, 8, 0x15);
    }
    else
    {
        tx = 320.0f;
        ty = 224.0f;
        if ((plyr->flags & PLYR_FLG_800000) == 0 && (plyr->_0e0 & 1) == 0)
            WorldToScreenPos(plyr->pos, &tx, &ty);

        if ((u_int)(prm->Blur - 1) < 4)
            SetEffects(3, 1, prm->Blur, prm->BlurAlpha, prm->BlurScale, prm->BlrrRot, (double)tx, (double)ty);

        if (prm->Deform != 0 && EffectIsSlotNo(4) == 0)
            SetEffects(4, 1, (u_char)prm->Deform, (u_char)prm->DeformRate);
    }

    if (prm->Focus != 0)
        SetEffects(5, 1, (u_char)prm->Focus);

    if (prm->Overlap == 1)
        SetEffects(6, 1, 0x10);

    if (prm->FadeFrame != 0)
        SetEffects(7, 1, (u_char)prm->FadeFrameAlpha, 0);

    type = IgEffectStoryMainContrastTypeGet(prm);
    if (type != 0)
    {
        if (type == 10)
        {
            SetEffects(10, 1, prm->NegaColor, prm->NegaAlpha, &prm->NegaAlpha2);
        }
        else
        {
            col = prm->ContrastColor;
            alp = prm->ContrastAlpha;
            if (EffectNowScreenEffectIs5Or6() != 0)
            {
                col = ((0x54 - col) * HighContrast.val) / 100 + col;
                alp = ((0x7c - alp) * HighContrast.val) / 100 + alp;
            }
            SetEffects(type, 1, col, alp);
        }
    }

    if (prm->BlackFilter != 0)
        SetEffects(9, 1, prm->BlackFilter);
}

// FF3 0x002bcc98 (out of line); FF2's, on the block passed in: Contrast 2..4
// are the contrast kinds 0xb..0xd, 5 the nega (10).

int IgEffectStoryMainContrastTypeGet(SCREEN_EFFECT_PARAMETER *prm)
{
    switch (prm->Contrast)
    {
    case 2:  return 0xb;
    case 3:  return 0xc;
    case 4:  return 0xd;
    case 5:  return 10;
    default: return 0;
    }
}

// ---- butterflies, footsteps, the end burst -------------------------------------------------

// FF3 0x001e9a30; FF2's (rand / RAND_MAX * 400 * 10 < 10). No caller in FF3.
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)

void IgEffectButterflyReq(float *pos)
{
    if (EffectButterflyNumGet() < 10
        && (float)ReiPan_Rand() / REIPAN_RAND_MAXF * 400.0f * 10.0f < 10.0f)
    {
        EffectButterflyReq(pos);
    }
}

// FF3 0x001e9ac8; FF2's. No caller in FF3.

void IgEffectButterflyAllCut(void)
{
    EffectButterflyAllCut();
    EffectButterflyParticleAllCut();
}

// FF3 0x001e9ae8 (FF3-only): the shallow-water step's ripple (particle type 4,
// id 0x309), not while the camera is up.

void IgEffectFootStepReq(float *pos, int file_no)
{
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0 && file_no == FOOT_44_0_SUIMEN_BD_BD)
        EffParticleReq(4, pos, 0x309);
}

// ---- the lanterns -----------------------------------------------------------------------------

// FF3 0x001e9b38; FF2's. FF3: the billboard is 4 x the scale (FF2 100) of
// effdat 0x46 (FF2 0x4c), drawn with its own registers before the ALPHA /
// ZBUF change, and the candle flame is EffCandleFlameOneDraw at the lantern's
// origin 9599 units off on y (rdata 0x370da0 holds -9599.0; FF2 +25): the
// ROM's constant, kept -- the flame lands far from the lantern, so FF3 shows
// none there.

void IgEffectTourouFreaModelDrawSub(void *pHandle, float (*CoordMat)[4])
{
    float *dir = gra3dcamGetDirection();
    int FlameColor[4] = { 0x80, 0x80, 0x80, 0x12 };
    sceVu0FVECTOR FlamePos;
    sceVu0FMATRIX NewCoord;
    TOUROU_FREA_DATA *pTourouFrea;
    u_long DrawEnv[6];
    u_long BackupAlpha;
    u_long BackupZBuf;
    float RotX;
    float RotY;

    if (pHandle == nullptr)
        return;

    sceVu0CopyVector(FlamePos, CoordMat[3]);
    FlamePos[1] += -9599.0f;

    pTourouFrea = EffectTourouFreaGetDataPtr(pHandle);
    if (pTourouFrea == nullptr)
        return;

    Vector2Rot(dir, &RotX, &RotY);
    EffectTourouFreaExecOne(pTourouFrea);

    sceVu0UnitMatrix(NewCoord);
    sceVu0RotMatrixY(NewCoord, NewCoord, NormalizeAngle(RotY));
    sceVu0TransMatrix(NewCoord, NewCoord, CoordMat[3]);

    BackupAlpha = *GetGsRegCached(SCE_GS_ALPHA_1);
    BackupZBuf  = *GetGsRegCached(SCE_GS_ZBUF_1);

    // data 0x35ac00: TEX1, ALPHA, ZBUF, TEST, CLAMP, the strip's GIF tag.
    DrawEnv[0] = 0x161;
    DrawEnv[1] = 0x44;
    DrawEnv[2] = 0x10a000118ULL;
    DrawEnv[3] = 0x5000d;
    DrawEnv[4] = 0;
    DrawEnv[5] = 0x302a400000008004ULL;
    Set3DPosTexure(pTourouFrea->ScaleX * 4.0f, pTourouFrea->ScaleY * 4.0f, NewCoord, DrawEnv, 0x46,
                   0x89, 100, 0x5a, (u_char)pTourouFrea->Alpha);

    SetGsRegCached(0x48, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(0x10a000118ULL, SCE_GS_ZBUF_1, 1);
    EffCandleFlameOneDraw(1.0f, FlamePos, FlameColor[0], FlameColor[1], FlameColor[2], FlameColor[3],
                          pTourouFrea->FlameCounter);
    SetGsRegCached(BackupAlpha, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(BackupZBuf, SCE_GS_ZBUF_1, 1);
}

// FF3 0x001e9d50; FF2's. No caller in FF3 (MapObjReg does not install it).

void IgEffectTourouFreaModelDraw(void)
{
    MAP_PUT *hdl = MapPutGetNowHdl();

    IgEffectTourouFreaModelDrawSub(hdl, (float (*)[4])MapPutGetMatrix(hdl));
}

// FF3 0x001e9d88; FF2's without the draw: FF3 steps the colour, sets the
// material alpha and the vertex colour preset, then sets ALPHA 0x48 / ZBUF
// and puts them straight back -- the MapDrawObj between is gone.

void IgEffectTourouBaseModelDrawSub(void *pSgdTop, float (*CoordMat)[4])
{
    TOUROU_BASE_DATA *pTourouBase;
    float Color[3];
    u_long BackupAlpha;
    u_long BackupZBuf;

    if (pSgdTop == nullptr)
        return;

    pTourouBase = EffectTourouBaseGetDataPtr(pSgdTop);
    if (pTourouBase == nullptr)
        return;

    EffectTourouBaseExecOne(pTourouBase);
    EffSgdSetMaterialAlpha((SGDFILEHEADER *)pSgdTop, (u_char)pTourouBase->Color[3]);

    Color[0] = (float)pTourouBase->Color[0];
    Color[1] = (float)pTourouBase->Color[1];
    Color[2] = (float)pTourouBase->Color[2];
    gra3dSetVertexColorPreset((SGDFILEHEADER *)pSgdTop, -1, Color);

    BackupAlpha = *GetGsRegCached(SCE_GS_ALPHA_1);
    BackupZBuf  = *GetGsRegCached(SCE_GS_ZBUF_1);
    SetGsRegCached(0x48, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(0x10a000118ULL, SCE_GS_ZBUF_1, 1);
    SetGsRegCached(BackupAlpha, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(BackupZBuf, SCE_GS_ZBUF_1, 1);
}

// FF3 0x001e9e70; FF2's. No caller in FF3.

void IgEffectTourouBaseModelDraw(void)
{
    MAP_PUT *hdl = MapPutGetNowHdl();
    void *pSgdTop = MapPutGetWork(hdl);

    IgEffectTourouBaseModelDrawSub(pSgdTop, (float (*)[4])MapPutGetMatrix(hdl));
}

// ---- the end burst ---------------------------------------------------------------------------

// FF3 0x001e9eb8; FF2's: 48 particles at pos, no end point, no suction.

void IgEffectEffectEndParticleReq(float *pos, int no)
{
    ENE_DMG_PARTICLE_REQ EneDmgParticleReq;

    sceVu0CopyVector(EneDmgParticleReq.StartPos, pos);
    EneDmgParticleReq.pEndPos     = nullptr;
    EneDmgParticleReq.ParticleNum = 48;
    EneDmgParticleReq.DistPE      = 0.0f;
    EneDmgParticleReq.SuctionFlg  = 0;
    EneDmgParticleReq.EffectType  = no;

    EffectEndParticleEffectReq(&EneDmgParticleReq);
}

// ---- the map-put draws -----------------------------------------------------------------------

// FF3 0x001e9ef8 (FF3-only; MapDrawRoomObjRegist, water-flow furniture): the
// model drawn with ALPHA 0x40 | Kind << 2 of its move type (Kind 1: blend,
// 2: add) and a ZBUF without the write mask. Skipped in the put's shadow
// pass (flag 8).

void IgEffectWaterFlowModelDraw(MAP_PUT *put, u_int flags)
{
    MAP_OBJ_MDL *mdl;
    u_long BackupAlpha;
    u_long BackupZBuf;
    int kind;

    if ((flags & 8) != 0)
        return;

    kind = EffectWaterFlowGetMoveDat((int)(intptr_t)MapPutGetWork(put))->Kind;
    mdl  = MapPutGetMdl(put);
    if (mdl == nullptr)
        return;

    BackupAlpha = *GetGsRegCached(SCE_GS_ALPHA_1);
    BackupZBuf  = *GetGsRegCached(SCE_GS_ZBUF_1);
    SetGsRegCached((u_long)kind << 2 | 0x40, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(0xa000118, SCE_GS_ZBUF_1, 1);
    mdl->mot->Draw();
    SetGsRegCached(BackupAlpha, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(BackupZBuf, SCE_GS_ZBUF_1, 1);
}

// FF3 0x001e9fe8; FF2 IgEffectLightComeInModelDrawSub's body as the put's
// callback (light-come-in furniture; the work is the SGD). Additive, CLAMP 0,
// no lights, fog pushed to 1000..2000 at full white; the alpha mode is 1
// with the camera up or player flag 1 (FF2: finder mode).

void IgEffectLightComeInModelDraw(MAP_PUT *put, u_int flags)
{
    static sceVu0FVECTOR Ambient;                               // data 0x329530 (zero)
    PLYR_WRK *plyr;
    MAP_OBJ_MDL *mdl;
    void *pSgdTop;
    G3DFOG FogBak;
    u_long BackupAlpha;
    u_long BackupClamp;
    int mode;

    if ((flags & 8) != 0 || put == nullptr)
        return;
    pSgdTop = MapPutGetWork(put);
    if (pSgdTop == nullptr)
        return;
    mdl = MapPutGetMdl(put);
    if (mdl == nullptr)
        return;

    mode = 0;
    BackupAlpha = *GetGsRegCached(SCE_GS_ALPHA_1);
    BackupClamp = *GetGsRegCached(SCE_GS_CLAMP_1);
    FogBak = *gra3dGetFogRef();

    plyr = ingame_wrk.plyr_wrk;
    if ((plyr->flags & PLYR_FLG_800000) != 0 || (plyr->flags & 1) != 0)
        mode = 1;
    EffectLightComeInExecOne(EffectLightComeInGetDataPtr(pSgdTop), mode);

    SetGsRegCached(0x48, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(0, SCE_GS_CLAMP_1, 1);
    gra3dSetFog(255.0f, 255.0f, 1000.0f, 2000.0f);
    gra3dApplyFog();
    gra3dLightEnableAll(0);
    gra3dSetAmbient(Ambient);
    gra3dApplyLight();

    mdl->mot->Draw();

    SetGsRegCached(BackupAlpha, SCE_GS_ALPHA_1, 1);
    SetGsRegCached(BackupClamp, SCE_GS_CLAMP_1, 1);
    gra3dSetFog(&FogBak);
    gra3dApplyFog();
}

// ---- the player's map-put --------------------------------------------------------------------

// FF3 0x001ea178: the flashlight's flare in the player's draw slot, not in
// the put's flag-1 pass or under lock 6.

void IgEffectPlyrPutDraw(MAP_PUT *put, u_int flags)
{
    if ((flags & 1) != 0 || IsLockTblOn(6) != 0)
        return;

    IgEffectHandLightFlareDraw(flags);
    SetDefaultGsRegs();
}

// FF3 0x001ea1d8 (PlyrMdlIsReady, before it marks the model in use): the put
// rides on the player model's coordinate 0, 200 units of z offset, drawn
// first.

void IgEffectPlyrPutSet(void)
{
    MAP_PUT *put;

    if (ingame_wrk.plyr_wrk->_2e4 != 0 || PlyrPut != nullptr)
        return;

    put = MapPutSetFunc(IG_PLYR_PUT_BUFF, IgEffectPlyrPutDraw, 0x8000);
    if (put == nullptr)
        return;

    MapPutSetMatrixPtr(put, ingame_wrk.plyr_wrk->mdl->GetCoord(0)->matLocalWorld[0]);
    MapPutSetZoffset(200.0f, put);
    MapPutSetFirst(put, -1);
    PlyrPut = put;
}

// FF3 0x001ea280 (PlyrMdlRelease).

void IgEffectPlyrPutDelete(void)
{
    if (PlyrPut == nullptr)
        return;

    MapPutDelete(IG_PLYR_PUT_BUFF);
    PlyrPut = nullptr;
}
