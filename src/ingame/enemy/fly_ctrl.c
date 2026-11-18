// ingame/enemy/fly_ctrl.c -- the flies an enemy sends out (FF2 fly_ctrl.c;
// FF3 0x1bf108..0x1c1418). Forty fly works (0xc0 each, FF2 0x1460) serve
// every enemy: an ordinary enemy takes five per fly type its dat lists
// (FlyAct, FF2's EneFlyInit) and its models load before the enemy counts as
// ready (EneFlyModelInitWait); its action script launches them one at a
// time (EneFlyActByKind -> EneFlyAct -> SetFlyOne). FlyRule (end of
// EnemyMain) moves them: speed and turn rate come from a curve keyed by the
// distance to the target or by the frames flown (FF3 replaced FF2's three
// distance bands), a second curve drives a figure-eight wobble around the
// path (the drawn position). A fly that reaches the player's head hurts
// them and is spent; the camera (PhotoFlyChk) and the flash (FlyFadeAll)
// fade them out. A fly is a FlyModel (FLY000_PK4 + mdl_no, drawn through
// CharDrawSetFly) or, with mdl_no bit 0x8000, an effect only.
// Ported from MioPan's fly_ctrl.c, every function checked against FF3.

#include <math.h>                       // fabsf, sqrtf
#include <string.h>                     // memcpy

#include <new>                          // placement new on FF3's C++ heap (operator_new)

#include "ingame/enemy/fly_ctrl.h"
#include "common/cpp_heap.h"            // operator_new, operator_delete
#include "common/vu0math.h"             // GetRandValIByF, RotLimitChk, NormalizeAngle, GetTrgtRot, Vu0Sinf
#include "graphics/effect/effect_obj.h" // EffKind1DReq, EffKind1DCut
#include "graphics/effect/effect_torch.h"   // EffectSetTorch2, EffectResetTorch2, EffectTorch2SetAlphaRate
#include "graphics/graph3d/gra3d.h"     // GRA3DLIGHTDATA, gra3dEmulateLightData, gra3dSetLightData, gra3dGetCamera, gra3dcamGetPosition
#include "graphics/graph3d/gra3dMisc.h" // GRA3DEMULATIONLIGHTDATACREATIONDATA, gra3dGenerateLightDataToChar
#include "ingame/char_model_sub.h"      // CharDrawSetFly, CharDrawDelete
#include "ingame/enemy/ene_main.h"      // ENE_WRK, ENE_DAT, ENE_STATE_ACT
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapDraw.h"         // MapDrawGetLightPtr
#include "ingame/photo/finder_photo.h"  // GetAlbumSaveDisp
#include "ingame/plyr/player.h"         // PLYR_WRK, SetPlyrAnime, PlyrSetMode, PlyrSetEneWait, FrameInsideChk, GetPlyrAreaNo
#include "ingame/plyr/plyr_gauge.h"     // PlyrGaugeDmgRate
#include "ingame/plyr/plyr_mdl.h"       // PlyrFinderModeCancel
#include "ingame/plyr/unit_ctl.h"       // ConvertRot2Dir, OutSightChk
#include "ingame/story_menu.h"          // IsLockTblOn
#include "system/eeiop/cd_file_dat.h"   // FLY000_PK4
#include "system/os/system.h"           // GetFrameTimeScale, GetPALMode

#define FLY_PI          3.1415927f      // sdata 0x3706c8 .. 0x370720

// data 0x327318: the fly types (FLY_DAT).
static const FLY_DAT fly_dat[FLY_DAT_NUM] =
{
    { 0x00, 0x40, 0x0000,  500, 0,  4, 120, 0, -1 },
    { 0x00, 0x40, 0x0001,  500, 0,  4, 120, 1,  0 },
    { 0x00, 0x40, 0x0001, 1500, 0,  4, 180, 2,  1 },
    { 0x0d, 0x40, 0x8002, 1500, 0, 12, 180, 3, -1 },
};

// ──────────────────────────────────────────────────────────────────────
// The curves (data 0x327200..0x327318). A curve is a list of points
// {key, v[0], v[1]} with keys ascending or descending; between two points
// the values are interpolated, outside them the end point's are kept.
// Speed curves (FLY_DAT.mv_curve): kind 0 keyed by the distance to the
// target (floor plane), kind 1 by the frames flown; v[0] the speed (times
// the slow-mode rate), v[1] the turn rate in degrees a frame. Wobble curves
// (FLY_DAT.wob_curve): keyed by the frames flown; v[0] the size, v[1] the
// speed (degrees a frame) of the figure-eight; kind 0 (the only one).
// ──────────────────────────────────────────────────────────────────────

struct FLY_KEYF                         /* 0xc: a point keyed by a float */
{
    float key;
    float v[2];
};
static_assert(sizeof(FLY_KEYF) == 0xc, "FLY_KEYF is 0xc bytes");

struct FLY_KEYI                         /* 0xc: a point keyed by a frame count */
{
    int   key;
    float v[2];
};
static_assert(sizeof(FLY_KEYI) == 0xc, "FLY_KEYI is 0xc bytes");

// layout: host -- pt is an EE pointer (0xc rows on the EE).
struct FLY_CURVE
{
    int         kind;                   // speed: 0 by distance (FLY_KEYF), 1 by frames (FLY_KEYI); wobble: 0
    const void *pt;                     // its points
    int         num;
};

static const FLY_KEYF fly_mv_key0[2] = { { 100.0f, { 2.4f, 0.4f } }, { 150.0f, { 6.4f, 0.8f } } };  // 0x327208
static const FLY_KEYI fly_mv_key1[4] =                                                              // 0x327220
{
    {  0, { 0.5f, 8.0f } }, { 20, { 0.0f, 2.0f } }, { 30, { 0.1f, 2.0f } }, { 60, { 2.0f, 1.0f } },
};
static const FLY_KEYF fly_mv_key2[2] = { { 140.0f, { 1.0f, 3.0f } }, { 70.0f, { 1.8f, 1.4f } } };   // 0x327250
static const FLY_KEYI fly_mv_key3[3] =                                                              // 0x327268
{
    {  0, { 0.6f, 0.0f } }, { 30, { 1.2f, 1.0f } }, { 60, { 1.2f, 1.0f } },
};
static const FLY_KEYI fly_wob_key0[2] = { { 30, { 4.0f, 3.0f } }, { 60, { 0.0f, 0.0f } } };         // 0x3272c0
static const FLY_KEYI fly_wob_key1[3] =                                                             // 0x3272d8
{
    {  0, { 2.0f, 3.0f } }, { 30, { 0.0f, 2.0f } }, { 60, { 3.0f, 1.0f } },
};

static const FLY_CURVE fly_mv_curve[4] =        // data 0x327290
{
    { 0, fly_mv_key0, 2 }, { 1, fly_mv_key1, 4 }, { 0, fly_mv_key2, 2 }, { 1, fly_mv_key3, 3 },
};
static const FLY_CURVE fly_wob_curve[2] =       // data 0x327300
{
    { 0, fly_wob_key0, 2 }, { 0, fly_wob_key1, 3 },
};

static FLY_MDL_WRK fly_wrk[FLY_WRK_NUM];    // bss 0x3e7e30

static void SetFlyOne(FLY_MDL_WRK *fly, ENE_WRK *ene, u_char kind, float *pos, float *rot,
                      PLYR_WRK *target, float *fix);

// The distance of a and b on the floor plane (FF3: VU0 vsub / vmul / vaddz.x
// / vsqrt, inline).
static float FlyDistXZ(const float *a, const float *b)
{
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];

    return sqrtf(dx * dx + dz * dz);
}

static void FlyCopyVector(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// ──────────────────────────────────────────────────────────────────────
// FlyModel (RTTI; vtable 0x341d68).
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001bf148 (base-object twin 0x001bf108).
// callcheck: GameModelCtorBase (the base constructor GameModel::GameModel)

FlyModel::FlyModel(FLY_MDL_WRK *fly)
{
    this->fly = fly;
}

// The model's world matrix: the fly's heading (pitch negated, yaw turned
// half round, so the model's front leads) at its drawn position.
// FF3 0x001bf188 (vtable +0x1c; the translation is a VU0 multiply with the
// unit matrix whose row 3 is dpos).

void FlyModel::GetWorldMatrix(sceVu0FMATRIX out)
{
    sceVu0FMATRIX unit;
    sceVu0FMATRIX m;
    sceVu0FVECTOR rot;
    float         ry;

    ry = fly->nrot[1] - FLY_PI;
    RotLimitChk(&ry);
    rot[0] = NormalizeAngle(-fly->nrot[0]);
    rot[1] = NormalizeAngle(ry);
    rot[2] = NormalizeAngle(fly->nrot[2]);
    rot[3] = 0.0f;                      // FF3: not set (sceVu0RotMatrix reads x, y, z)
    sceVu0UnitMatrix(unit);             // FF3: the unit matrix 0x3293c0
    sceVu0RotMatrix(m, unit, rot);
    sceVu0TransMatrix(out, m, fly->dpos);
}

// The light set: the player's area light made for a character at the fly,
// plus a grey (0.8) light from the camera in slot 2 (FF2's SetFlyLight
// lit it from the camera alone).
// FF3 0x001bf300 (vtable +0x20).

void FlyModel::DrawMdlPre()
{
    GRA3DLIGHTDATA                      ld;     // 0x1610 bytes
    GRA3DEMULATIONLIGHTDATACREATIONDATA cd;
    GRA3DLIGHTDATA *src;
    float          *dir;
    G3DLIGHT       *l;
    int             i;

    src = MapDrawGetLightPtr(GetPlyrAreaNo(), 0);
    if (src == nullptr)                 // PC: the area's room is not loaded (FF3 reads EE address 0)
        return;
    memcpy(&ld, src, sizeof(ld));

    memset(&cd, 0, sizeof(cd));
    cd.fAngleScale         = 1.0f;
    cd.fDiffuseScale       = 0.5f;
    cd.fMaplightScale      = 1.0f;
    cd.bEnableFlashlight   = 1;
    cd.bEmulateFlashlight  = 1;
    cd.bEnableFlashlight2  = 1;
    cd.bEmulateFlashlight2 = 1;
    gra3dGenerateLightDataToChar(&ld, src, &cd);
    gra3dEmulateLightData(&ld, &ld, fly->dpos, cd.fMaplightScale);

    l = &ld.aLight[2];
    l->vDiffuse[0] = 0.8f;              // sdata 0x3706cc
    l->vDiffuse[1] = 0.8f;
    l->vDiffuse[2] = 0.8f;
    l->vDiffuse[3] = 0.0f;
    dir = gra3dGetCamera()->matCoord[2];
    for (i = 0; i < 4; i++)
        l->vDirection[i] = dir[i] * -1.0f;
    ld.aStatus[2].bEnable = 1;
    gra3dSetLightData(&ld, nullptr);
}

// Fly model no.
// FF3 0x001bf2d8.

int FlyModel::Load(short no)
{
    return GameModel::Load((short)(no + FLY000_PK4));
}

// ──────────────────────────────────────────────────────────────────────
// Slots and models.
// ──────────────────────────────────────────────────────────────────────

// A free fly slot (sta without FLY_STA_USE), or null when all 40 are taken.
// FF3 0x001bf410; FF2 GetFlyWork (static there).

static FLY_MDL_WRK *GetFlyWork(void)
{
    for (int i = 0; i < FLY_WRK_NUM; i++)
    {
        if ((fly_wrk[i].sta & FLY_STA_USE) == 0)
            return &fly_wrk[i];
    }
    return nullptr;
}

// 1 for an effect fly (mdl_no bit 0x8000): no model to load.
// FF3 0x001bf448; FF2 CheckEffectFly.

int CheckEffectFly(FLY_MDL_WRK *fly)
{
    return (fly->mdl_no & 0x8000) != 0;
}

// FF3 0x001bf458 (vtable +0x10).

static int FlyModelIsLoaded(FLY_MDL_WRK *fly)
{
    return fly->mdl->IsLoaded();
}

// The fly's model one step further: 0 a FlyModel made and its file asked
// for (the slot taken), 1 waits for it, then registers the draw
// (CharDrawSetFly); 2 is ready (returns 1); 3 gives the slot back.
// FF3 0x001bf480; FF2 FlyModelDataInit (FF2 loaded a model and an
// animation through mmanage).

int FlyModelDataInit(FLY_MDL_WRK *fly)
{
    switch (fly->init_flow)
    {
    case 0:
        fly->mdl = new (operator_new(sizeof(FlyModel))) FlyModel(fly);
        fly->mdl->Load((short)fly->mdl_no);
        fly->init_flow = 1;
        fly->sta |= FLY_STA_USE;
        break;
    case 1:
        if (FlyModelIsLoaded(fly) == 0)
            break;
        CharDrawSetFly(fly);
        fly->init_flow = 2;
        break;
    case 2:
        return 1;
    case 3:
        fly->sta &= ~FLY_STA_USE;
        break;
    }
    return 0;
}

// An effect fly's effect at its drawn position: torch 5 (type 0), torch 2
// (type 1), an EffKind1D 2 that follows its heading and alpha (type 2).
// FF3 0x001bf568; FF2 FlyEffectDataInit.

static int FlyEffectDataInit(FLY_MDL_WRK *fly)
{
    switch (fly->mdl_no & 0x7fff)
    {
    case 0:
        fly->efpw = EffectSetTorch2(fly->dpos, 5);
        break;
    case 1:
        fly->efpw = EffectSetTorch2(fly->dpos, 2);
        break;
    case 2:
        fly->efpw = EffKind1DReq(fly->dpos, fly->nrot, &fly->alp, 2, 0);
        break;
    }
    return 1;
}

// The enemy's flies: for each fly type its dat lists, five slots taken and
// their models started (an effect fly is only marked used). Stops at the
// first slot that cannot be had. Nothing for a special enemy (type 2).
// FF3 0x001bfbb0; FF2 EneFlyInit (FF3 sets no owner or wobble here).

void FlyAct(ENE_WRK *ene)
{
    if (ene->type == 2)
        return;

    for (int i = 0; i < 3; i++)
    {
        short kind = ene->dat->fly[i];

        if (kind < 0)
            continue;

        for (int j = 0; j < 5; j++)
        {
            FLY_MDL_WRK *fly = GetFlyWork();

            if (fly == nullptr)
                return;

            fly->init_flow = 0;
            fly->dat = &fly_dat[kind];
            fly->mdl_no = fly_dat[kind].mdl_no;
            ene->fly[i][j] = fly;
            if (CheckEffectFly(fly) == 0)
                FlyModelDataInit(fly);
            else
                fly->sta |= FLY_STA_USE;
        }
    }
}

// 1 once every model fly of the enemy is ready (FlyModelDataInit stepped for
// the slots still marked used); a special enemy (type 2) and effect-fly
// types count as ready.
// FF3 0x001bfcc0; FF2 EneFlyModelInitWait (same).

int EneFlyModelInitWait(ENE_WRK *ene)
{
    int ret = 1;

    if (ene->type == 2)
        return 1;

    for (int i = 0; i < 3; i++)
    {
        if (ene->dat->fly[i] < 0 || CheckEffectFly(ene->fly[i][0]) != 0)
            continue;

        for (int j = 0; j < 5; j++)
        {
            if ((ene->fly[i][j]->sta & FLY_STA_USE) != 0)
                ret &= FlyModelDataInit(ene->fly[i][j]);
        }
    }
    return ret;
}

// Launch the enemy's next idle fly of type kind (taken, not moving) from pos
// along rot at target (or at fix); an effect fly gets its effect first.
// FF3 0x001bfda8; FF2 EneFlyAct (fix is FF3's).

void EneFlyAct(ENE_WRK *ene, u_char kind, float *pos, float *rot, PLYR_WRK *target, float *fix)
{
    FLY_MDL_WRK *fly;
    int          n = -1;
    int          m = -1;

    if (ene->type == 2)
        return;

    for (int i = 0; i < 3; i++)
    {
        if ((u_short)ene->dat->fly[i] == kind)
        {
            n = i;
            break;
        }
    }
    if (n == -1)
        return;

    for (int j = 0; j < 5; j++)
    {
        u_short sta = ene->fly[n][j]->sta;

        if ((sta & FLY_STA_USE) != 0 && (sta & FLY_STA_MOVE) == 0)
        {
            m = j;
            break;
        }
    }
    if (m < 0)
        return;

    fly = ene->fly[n][m];
    if (CheckEffectFly(fly) != 0)
        FlyEffectDataInit(fly);
    SetFlyOne(fly, ene, kind, pos, rot, target, fix);
}

// The enemy's action (EJobC18) sends a fly of type kind: 0 from joint 0x13,
// 1 and 2 from joint 1 at the target's head; 3 from joint 1 at a point
// 100..129 ahead of the enemy, turned 15..30 (close) to 30..59 degrees
// (130 away or more) to either side, 10 above it.
// FF3 0x001bff20 (FF3-only).

void EneFlyActByKind(ENE_WRK *ene, u_char kind)
{
    sceVu0FMATRIX unit;
    sceVu0FMATRIX m;
    sceVu0FVECTOR ofs;
    sceVu0FVECTOR fix;
    float        *pos;
    float        *fixp = nullptr;
    float         dist;
    float         ang;

    switch (kind)
    {
    case 0:
        pos = ene->jpos13;
        break;
    case 1:
    case 2:
        pos = ene->jpos1;
        break;
    case 3:
        ofs[0] = 0.0f;
        ofs[1] = 0.0f;
        ofs[2] = (float)(GetRandValIByF(30) + 100);
        ofs[3] = 1.0f;
        // FF3 measures to the target's first 16 bytes (FF2's PLCMN_WRK began
        // with mbox.pos; PLYR_WRK +0x00 holds the turn timers).
        dist = FlyDistXZ(ene->pos, (const float *)ene->target);
        if (130.0f < dist)
            dist = 130.0f;
        ang = (2.0f - dist / 130.0f) * 15.0f;
        ang = (float)GetRandValIByF(30) + ang;
        if (GetRandValIByF(2) != 0)
            ang = -ang;
        ang = (ang * FLY_PI) / 180.0f + ene->rot[1];
        RotLimitChk(&ang);
        sceVu0UnitMatrix(unit);         // FF3: the unit matrix 0x3293c0
        sceVu0RotMatrixY(m, unit, NormalizeAngle(ang));
        sceVu0ApplyMatrix(fix, m, ofs);
        for (int i = 0; i < 4; i++)
            fix[i] += ene->pos[i];
        fix[1] += -10.0f;
        pos = ene->jpos1;
        fixp = fix;
        break;
    default:
        return;
    }
    EneFlyAct(ene, kind, pos, ene->rot, ene->target, fixp);
}

// Place a taken fly at pos and set it flying: owner, type, life and homing
// time (life + rand(life) each), alpha, the target (its head, or fix), and
// a first step of 10 along rot turned by a random launch spread (type 3: by
// the heading to the target, read as degrees).
// FF3 0x001bf600; FF2 SetFlyOne.
// callcheck: GetFlyWork, FlyModelDataInit, FlyEffectDataInit, SetFlyOne (Ghidra's call list for 0x1bf600 runs on into FlyAct / EneFlyModelInitWait / EneFlyAct)

static void SetFlyOne(FLY_MDL_WRK *fly, ENE_WRK *ene, u_char kind, float *pos, float *rot,
                      PLYR_WRK *target, float *fix)
{
    sceVu0FVECTOR mvv;
    sceVu0FVECTOR wvec;
    sceVu0FVECTOR trot;
    sceVu0FMATRIX m;
    float         rx = 0.0f;            // FF3: left unset for kind > 3 (never launched)
    float         ry = 0.0f;

    fly->ew = ene;
    fly->target = target;
    fly->dat = &fly_dat[kind];
    fly->life_time = GetRandValIByF((int)fly_dat[kind].life) + fly->dat->life;
    fly->trace_time = GetRandValIByF((int)fly->dat->life) + fly->dat->life;
    fly->now_cnt = 0;
    fly->wob_cnt = 0;
    fly->wob_ang = GetRandValIByF(360);
    fly->alp = (float)fly->dat->alp * 0.0078125f;
    if (CheckEffectFly(fly) != 0 && (fly->dat->mdl_no & 0x7fff) < 2)
        EffectTorch2SetAlphaRate(fly->efpw, fly->alp);

    FlyCopyVector(fly->npos, pos);
    fly->npos[3] = 1.0f;
    FlyCopyVector(fly->dpos, pos);
    fly->dpos[3] = 1.0f;
    if (fix == nullptr)
    {
        fly->fixed = 0;
        FlyCopyVector(fly->tpos, fly->target->listen_pos);
    }
    else
    {
        fly->fixed = 1;
        FlyCopyVector(fly->fpos, fix);
        FlyCopyVector(fly->tpos, fix);
    }
    fly->tpos[3] = 1.0f;

    // The launch spread, in tenths of a degree.
    switch (kind)
    {
    case 0:
        rx = (float)(GetRandValIByF(60) + 60) / 10.0f;
        ry = (float)(GetRandValIByF(60) + 60) / 10.0f;
        if (GetRandValIByF(2) != 0)
            rx = -rx;
        if (GetRandValIByF(2) != 0)
            ry = -ry;
        break;
    case 1:
        rx = (float)(GetRandValIByF(700) + 200) / 10.0f;
        ry = (float)(GetRandValIByF(450) + 900) / 10.0f;
        if (GetRandValIByF(2) != 0)
            ry = -ry;
        break;
    case 2:
        rx = (float)(GetRandValIByF(250) + 200) / 10.0f;
        ry = (float)(GetRandValIByF(400) + 500) / 10.0f;
        if (GetRandValIByF(2) != 0)
            rx = -rx;
        if (GetRandValIByF(2) != 0)
            ry = -ry;
        break;
    case 3:
        GetTrgtRot(fly->npos, fly->tpos, trot, 3);
        rx = trot[0];
        ry = trot[1];
        break;
    }
    ry = (ry * FLY_PI) / 180.0f + rot[1];
    rx = (rx * FLY_PI) / 180.0f + rot[0];
    RotLimitChk(&rx);
    RotLimitChk(&ry);

    mvv[0] = 0.0f;
    mvv[1] = 0.0f;
    mvv[2] = 10.0f;
    mvv[3] = 1.0f;
    sceVu0UnitMatrix(m);
    if ((fly->dat->attr & FLY_ATTR_LEVEL) == 0)
        sceVu0RotMatrixX(m, m, rx);
    sceVu0RotMatrixY(m, m, ry);
    sceVu0TransMatrix(m, m, fly->npos);
    sceVu0ApplyMatrix(wvec, m, mvv);

    FlyCopyVector(fly->opos, fly->npos);
    FlyCopyVector(fly->npos, wvec);
    GetTrgtRot(fly->opos, fly->npos, fly->nrot, 3);
    fly->sta |= FLY_STA_MOVE;
}

// ──────────────────────────────────────────────────────────────────────
// Release.
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001c0160; FF2 FlyEffectRelease (type 2's EffKind1D is FF3's).

static void FlyEffectRelease(FLY_MDL_WRK *fly)
{
    u_short type;

    if (fly->efpw == nullptr)
        return;

    type = fly->mdl_no & 0x7fff;
    if (type < 2)
    {
        EffectResetTorch2(fly->efpw);
        fly->efpw = nullptr;
    }
    else if (type == 2)
    {
        EffKind1DCut(fly->efpw, 0);
        fly->efpw = nullptr;
    }
}

// The model deleted and its draw entry dropped.
// FF3 0x001c01d0; FF2 FlyModelRelease. FF3: delete through the vtable (+0x0c).

static void FlyModelRelease(FLY_MDL_WRK *fly)
{
    if (fly->mdl != nullptr)
    {
        fly->mdl->~FlyModel();
        operator_delete(fly->mdl);
    }
    fly->mdl = nullptr;
    CharDrawDelete(fly);
}

// FF3 0x001c0218; FF2 FlyRelease.

static void FlyRelease(FLY_MDL_WRK *fly)
{
    if (CheckEffectFly(fly) == 0)
        FlyModelRelease(fly);
    else
        FlyEffectRelease(fly);
    fly->sta = 0;
}

// Retire what was spent last frame (the slot stays the enemy's; an ownerless
// fly is released), fade what fades (1/16 a frame, then spent), and fade the
// flies of an enemy no longer acting.
// FF3 0x001c0260; FF2 ClearCheckFlyWork (FF2 cleared FLY_STA_USE too).

static void ClearCheckFlyWork(void)
{
    for (int i = 0; i < FLY_WRK_NUM; i++)
    {
        FLY_MDL_WRK *fly = &fly_wrk[i];
        u_short      sta = fly->sta;

        if ((sta & FLY_STA_KILL) != 0)
        {
            fly->alp = 0.0f;
            fly->sta = sta & 0xff30;
            if (fly->ew == nullptr)
                FlyRelease(fly);
            else if (CheckEffectFly(fly) != 0)
                FlyEffectRelease(fly);
        }
        else if ((sta & FLY_STA_FADE) != 0)
        {
            if (0.0f < fly->alp - 0.0625f)
                fly->alp = fly->alp - 0.0625f;
            else
            {
                fly->alp = 0.0f;
                fly->sta = sta | FLY_STA_KILL;
            }
            if (CheckEffectFly(fly) != 0 && (fly->dat->mdl_no & 0x7fff) < 2)
                EffectTorch2SetAlphaRate(fly->efpw, fly->alp);
        }
        else if (fly->ew != nullptr && fly->ew->state != ENE_STATE_ACT)
        {
            fly->sta = sta | FLY_STA_FADE;
        }
    }
}

// The enemy's flies released (models deleted, effects reset, slots freed).
// FF3 0x001c03b8; FF2 EneFlyAnmctrlRelease.

void EneFlyAnmctrlRelease(ENE_WRK *ene)
{
    if (ene->type == 2)
        return;

    for (int i = 0; i < 3; i++)
    {
        if (ene->dat->fly[i] < 0)
            continue;
        for (int j = 0; j < 5; j++)
            FlyRelease(ene->fly[i][j]);
    }
}

// The enemy's flies fade.
// FF3 0x001c0450; FF2 EraseEneFlyWork.

void EraseEneFlyWork(ENE_WRK *ene)
{
    for (int i = 0; i < FLY_WRK_NUM; i++)
    {
        if (fly_wrk[i].ew == ene)
            fly_wrk[i].sta |= FLY_STA_FADE;
    }
}

// FF3 0x001c0490.

static int FlyIsMoving(FLY_MDL_WRK *fly)
{
    return (fly->sta & FLY_STA_MOVE) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Movement.
// ──────────────────────────────────────────────────────────────────────

// The values at out->key on a float-keyed curve (FLY_CURVE kind 0).
// FF3 0x001c04a0.

static void FlyCurveGetF(FLY_KEYF *out, const FLY_KEYF *pt, int num)
{
    float first = pt[0].key;
    float last  = pt[num - 1].key;
    float k     = out->key;

    if (first < last)
    {
        if (last <= k)
        {
            out->v[0] = pt[num - 1].v[0];
            out->v[1] = pt[num - 1].v[1];
            return;
        }
        if (k < first)
        {
            out->v[0] = pt[0].v[0];
            out->v[1] = pt[0].v[1];
            return;
        }
    }
    else if (last < first)
    {
        if (first <= k)
        {
            out->v[0] = pt[0].v[0];
            out->v[1] = pt[0].v[1];
            return;
        }
        if (k < last)
        {
            out->v[0] = pt[num - 1].v[0];
            out->v[1] = pt[num - 1].v[1];
            return;
        }
    }

    for (int i = 1; i < num; i++, pt++)
    {
        const FLY_KEYF *a = &pt[0];
        const FLY_KEYF *b = &pt[1];
        float           r;

        if (a->key < b->key)
        {
            if (a->key <= k && k < b->key)
            {
                r = (k - a->key) / (b->key - a->key);
                out->v[1] = r * (b->v[1] - a->v[1]) + a->v[1];
                out->v[0] = r * (b->v[0] - a->v[0]) + a->v[0];
                return;
            }
        }
        else if (b->key < a->key)
        {
            if (b->key <= k && k < a->key)
            {
                r = (k - b->key) / (a->key - b->key);
                out->v[1] = r * (a->v[1] - b->v[1]) + b->v[1];
                out->v[0] = r * (a->v[0] - b->v[0]) + b->v[0];
                return;
            }
        }
        else if (a->key == k)
        {
            out->v[0] = (a->v[0] + b->v[0]) * 0.5f;
            out->v[1] = (a->v[1] + b->v[1]) * 0.5f;
            return;
        }
    }
}

// The values at out->key on a frame-keyed curve.
// FF3 0x001c0640 (FlyCurveGetF with an int key).

static void FlyCurveGetI(FLY_KEYI *out, const FLY_KEYI *pt, int num)
{
    int first = pt[0].key;
    int last  = pt[num - 1].key;
    int k     = out->key;

    if (first < last)
    {
        if (last <= k)
        {
            out->v[0] = pt[num - 1].v[0];
            out->v[1] = pt[num - 1].v[1];
            return;
        }
        if (k < first)
        {
            out->v[0] = pt[0].v[0];
            out->v[1] = pt[0].v[1];
            return;
        }
    }
    else if (last < first)
    {
        if (first <= k)
        {
            out->v[0] = pt[0].v[0];
            out->v[1] = pt[0].v[1];
            return;
        }
        if (k < last)
        {
            out->v[0] = pt[num - 1].v[0];
            out->v[1] = pt[num - 1].v[1];
            return;
        }
    }

    for (int i = 1; i < num; i++, pt++)
    {
        const FLY_KEYI *a = &pt[0];
        const FLY_KEYI *b = &pt[1];
        float           r;

        if (a->key < b->key)
        {
            if (a->key <= k && k < b->key)
            {
                r = (float)(k - a->key) / (float)(b->key - a->key);
                out->v[0] = r * (b->v[0] - a->v[0]) + a->v[0];
                out->v[1] = r * (b->v[1] - a->v[1]) + a->v[1];
                return;
            }
        }
        else if (b->key < a->key)
        {
            if (b->key <= k && k < a->key)
            {
                r = (float)(k - b->key) / (float)(a->key - b->key);
                out->v[0] = r * (a->v[0] - b->v[0]) + b->v[0];
                out->v[1] = r * (a->v[1] - b->v[1]) + b->v[1];
                return;
            }
        }
        else if (a->key == k)
        {
            out->v[1] = (a->v[1] + b->v[1]) * 0.5f;
            out->v[0] = (a->v[0] + b->v[0]) * 0.5f;
            return;
        }
    }
}

// A figure-eight of size amp: out[0] = 2 amp sin t, out[1] = amp sin 2t,
// t = *ang / 100 degrees; *ang advances by add (1/100 degree), mod 360.
// FF3 0x001c0808.

static void FlyWobbleEight(float amp, int add, int *ang, float *out)
{
    float t = (((float)*ang / 100.0f) * FLY_PI) / 180.0f;

    out[0] = (amp + amp) * Vu0Sinf(t);
    out[1] = amp * Vu0Sinf(t + t);
    *ang = (*ang + add) % 36000;
}

// The wobble of kind kind (0: the figure-eight, speed spd degrees a frame).
// FF3 0x001c08f0.

static void FlyWobble(int kind, float amp, float spd, int *ang, float *out)
{
    if (kind == 0)
        FlyWobbleEight(amp, (int)(spd * 100.0f), ang, out);
}

// One frame of a moving fly: its life counted down (then it fades); unless
// it landed, speed and turn rate from its speed curve (speed 0: it stays),
// homing on the target for trace_time frames (turning at most the turn
// rate, yaw at once with FLY_ATTR_YAW_FREE), a step along the new heading,
// the drawn position with the wobble; a fly at a fixed target with
// FLY_ATTR_LAND stops at its height.
// FF3 0x001c0928; FF2 FlyMove (FF2: distance bands, PAL scaling, sin wobble).

static void FlyMove(FLY_MDL_WRK *fly)
{
    const FLY_DAT *dat;
    sceVu0FVECTOR  mvv;
    sceVu0FVECTOR  wvec;
    sceVu0FMATRIX  m;
    float         *tp;
    float          tr_ang = 0.0f;
    float          turn = 0.0f;
    float          rx;
    float          ry;
    float          d;

    if ((fly->sta & FLY_STA_HOLD) != 0)
        return;
    if (--fly->life_time == 0)
        fly->sta |= FLY_STA_FADE;
    if ((fly->sta & FLY_STA_LANDED) != 0)
        return;

    mvv[0] = 0.0f;
    mvv[1] = 0.0f;
    mvv[2] = 0.0f;
    mvv[3] = 1.0f;
    dat = fly->dat;
    if (dat->mv_curve >= 0)
    {
        const FLY_CURVE *c = &fly_mv_curve[dat->mv_curve];
        float            rate = PlyrGaugeDmgRate(GetAlbumSaveDisp());
        float            spd;

        if (c->kind == 0)
        {
            FLY_KEYF k = { FlyDistXZ(fly->npos, fly->tpos), { 0.0f, 0.0f } };

            FlyCurveGetF(&k, (const FLY_KEYF *)c->pt, c->num);
            spd = k.v[0];
            tr_ang = k.v[1] * FLY_PI;
        }
        else
        {
            FLY_KEYI k = { fly->now_cnt, { 0.0f, 0.0f } };

            FlyCurveGetI(&k, (const FLY_KEYI *)c->pt, c->num);
            spd = k.v[0];
            tr_ang = k.v[1] * FLY_PI;
            fly->now_cnt++;
        }
        mvv[2] = spd * rate;
        tr_ang = tr_ang / 180.0f;
    }

    if (mvv[2] == 0.0f)
        fly->sta |= FLY_STA_STOP;
    if (fly->trace_time != 0)
    {
        fly->trace_time--;
        turn = tr_ang;
    }

    tp = (fly->fixed == 0) ? fly->target->listen_pos : fly->fpos;
    FlyCopyVector(fly->tpos, tp);
    GetTrgtRot(fly->npos, fly->tpos, fly->trot, 3);
    GetTrgtRot(fly->opos, fly->npos, fly->nrot, 3);

    rx = fly->trot[0];
    if (turn < fabsf(fly->nrot[0] - rx))
    {
        d = fly->nrot[0] - rx;
        RotLimitChk(&d);
        rx = (0.0f < d) ? fly->nrot[0] - turn : fly->nrot[0] + turn;
    }
    RotLimitChk(&rx);

    ry = fly->trot[1];
    if ((dat->attr & FLY_ATTR_YAW_FREE) == 0 && turn < fabsf(fly->nrot[1] - ry))
    {
        d = fly->nrot[1] - ry;
        RotLimitChk(&d);
        ry = (0.0f < d) ? fly->nrot[1] - turn : fly->nrot[1] + turn;
    }
    RotLimitChk(&ry);

    sceVu0UnitMatrix(m);
    sceVu0RotMatrixX(m, m, rx);
    sceVu0RotMatrixY(m, m, ry);
    sceVu0TransMatrix(m, m, fly->npos);
    sceVu0ApplyMatrix(wvec, m, mvv);

    if (dat->wob_curve >= 0)
    {
        const FLY_CURVE *c = &fly_wob_curve[dat->wob_curve];
        FLY_KEYI         k = { fly->wob_cnt, { 0.0f, 0.0f } };

        FlyCurveGetI(&k, (const FLY_KEYI *)c->pt, c->num);
        fly->wob_cnt++;
        FlyWobble(c->kind, k.v[0], k.v[1], &fly->wob_ang, mvv);
    }
    sceVu0ApplyMatrix(fly->dpos, m, mvv);

    if (fly->fixed != 0 && (dat->attr & FLY_ATTR_LAND) != 0 && fabsf(wvec[1] - fly->fpos[1]) < 1.0f)
    {
        wvec[1] = fly->fpos[1];
        fly->sta |= FLY_STA_LANDED;
    }

    if ((fly->sta & FLY_STA_STOP) == 0)
    {
        FlyCopyVector(fly->opos, fly->npos);
        FlyCopyVector(fly->npos, wvec);
    }
}

// The current player when the fly's drawn position is inside the box
// around their head: hit_rng on the floor plane, hit_rng above, hit_rng + 24
// below (the y axis points down). FF3 only checks the current player.
// FF3 0x001c0d68; FF2 FlyPlyrHitChk (FF2: the target and the sister).

static PLYR_WRK *FlyPlyrHitChk(FLY_MDL_WRK *fly)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR fp;
    sceVu0FVECTOR pp;
    float         rng;
    int           hit;

    FlyCopyVector(fp, fly->dpos);
    FlyCopyVector(pp, plyr->listen_pos);
    rng = (float)fly->dat->hit_rng;

    if (rng < fabsf(fp[0] - pp[0]))
        return nullptr;
    if (rng < fabsf(fp[2] - pp[2]))
        return nullptr;
    if (fp[1] < pp[1])
        hit = fabsf(fp[1] - pp[1]) <= rng;
    else
        hit = fabsf(fp[1] - pp[1]) <= rng + 24.0f;
    return hit ? plyr : nullptr;
}

// A hit: the fly is spent; unless the player is out of reach (ene_wait),
// the damage, the hit reaction (anime 0x6c from behind, 0x6b from the front;
// not in the door mode), 120 frames of no touches, and the fly type's
// status for 240 frames (200 in PAL).
// FF3 0x001c0f00; FF2 FlyAtkHit.
// callcheck: fptodp, fabs, dptofp (FF3's fabs through double; fabsf here)

static void FlyAtkHit(FLY_MDL_WRK *fly, PLYR_WRK *plyr)
{
    u_char anime;
    float  d;

    fly->sta |= FLY_STA_KILL;
    if (plyr->ene_wait != 0)
        return;

    if (plyr == ingame_wrk.plyr_wrk && (plyr->flags & PLYR_FLG_800000) != 0)
        PlyrFinderModeCancel();
    plyr->status_time = 0.0f;
    plyr->dmg_type = 8;
    plyr->_147 = 0xc;
    plyr->dmg += fly->dat->dmg;
    if (ingame_wrk.plyr_wrk->mode == PLYR_MODE_DOOR)
        return;

    if (CheckEffectFly(fly) == 0)
    {
        d = fly->nrot[1] - plyr->rot[1];
        RotLimitChk(&d);
        d = fabsf(d);
        anime = (d < FLY_PI / 2.0f) ? 0x6c : 0x6b;      // sdata 0x37072c
    }
    else
    {
        float rot[4];

        GetTrgtRot(plyr->pos, fly->dpos, rot, 2);
        rot[1] = rot[1] - plyr->rot[1];
        RotLimitChk(&rot[1]);
        anime = (ConvertRot2Dir(rot[1], 2) != 0) ? 0x6c : 0x6b;
    }
    SetPlyrAnime(anime, 1, 0xff, 0xff, 0, 0.0f);
    ingame_wrk.plyr_wrk->flags2 &= ~(u_long)0xf;
    PlyrSetMode(2, nullptr);
    plyr->dmg_type = 8;
    plyr->_147 = 0xc;
    PlyrSetEneWait(plyr, 120);

    if (fly->dat->cond != 0)
    {
        float scale;

        plyr->status = fly->dat->cond;
        scale = GetFrameTimeScale();
        plyr->status_time = ((GetPALMode() == 0) ? 240.0f : 200.0f) / scale;  // sdata 0x370734 / 0x370730
    }
}

// Every moving fly a frame on, then its hit on the player (not while it
// fades); while every moving fly stands still, they all go again. Nothing
// while the player work is busy (_234) or lock 8 / 9 is on.
// FF3 0x001c1110; FF2 FlyRule.

void FlyRule(void)
{
    s_char moving = 0;
    s_char stopped = 0;

    if (ingame_wrk.plyr_wrk->_234 != 0 || IsLockTblOn(8) != 0 || IsLockTblOn(9) != 0)
        return;

    ClearCheckFlyWork();
    for (int i = 0; i < FLY_WRK_NUM; i++)
    {
        FLY_MDL_WRK *fly = &fly_wrk[i];
        PLYR_WRK    *plyr;

        if ((fly->sta & FLY_STA_MOVE) == 0)
            continue;
        FlyMove(fly);
        moving++;
        if ((fly->sta & FLY_STA_STOP) != 0)
            stopped++;
        if ((fly->sta & FLY_STA_FADE) == 0 && (plyr = FlyPlyrHitChk(fly)) != nullptr)
            FlyAtkHit(fly, plyr);
    }

    if (0 < moving && moving == stopped)
    {
        for (int i = 0; i < FLY_WRK_NUM; i++)
        {
            if ((fly_wrk[i].sta & FLY_STA_MOVE) != 0)
                fly_wrk[i].sta &= ~FLY_STA_STOP;
        }
    }
}

// The draw callback's body: the model's matrix and draw while the fly moves.
// FF3 0x001c1248 (CharDrawFlyFunc).

int FlyDraw(FLY_MDL_WRK *fly)
{
    int moving = FlyIsMoving(fly);

    if (moving != 0)
    {
        fly->mdl->UpdateMatrix();
        fly->mdl->DrawMdl();
    }
    return moving != 0;
}

// The shutter: every moving fly (not FLY_ATTR_NO_PHOTO) within 200 of the
// player, in their sight (120 degrees) and inside the viewfinder frame
// fades.
// FF3 0x001c1290; FF2 PhotoFlyChk (FF2: 5000, the camera's position).

void PhotoFlyChk(void)
{
    float tx;
    float ty;

    for (u_char i = 0; i < FLY_WRK_NUM; i++)
    {
        FLY_MDL_WRK *fly = &fly_wrk[i];
        PLYR_WRK    *plyr;

        if ((fly->sta & FLY_STA_MOVE) == 0 || (fly->dat->attr & FLY_ATTR_NO_PHOTO) != 0)
            continue;
        plyr = ingame_wrk.plyr_wrk;
        if (200.0f < FlyDistXZ(plyr->pos, fly->dpos))  // sdata 0x370738
            continue;
        if (OutSightChk(fly->dpos, gra3dcamGetPosition(), plyr->rot[1], 2.0943952f, 200.0f) == 0
            && FrameInsideChk(fly->dpos, &tx, &ty) != 0)
            fly->sta |= FLY_STA_FADE;
    }
}

// The flash: every moving fly fades.
// FF3 0x001c13d0 (PlyrFlashRelease).

void FlyFadeAll(void)
{
    for (u_char i = 0; i < FLY_WRK_NUM; i++)
    {
        if ((fly_wrk[i].sta & FLY_STA_MOVE) != 0)
            fly_wrk[i].sta |= FLY_STA_FADE;
    }
}
