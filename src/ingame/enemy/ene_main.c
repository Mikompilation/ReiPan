// ingame/enemy/ene_main.c -- the enemy main loop and the enemy work (FF3
// 0x186a88..0x18a338). FF2 has this in enemy.c (EnemyMain, EnemyWrkOne,
// SearchEneWrkNo, IsActEnemy, ShutterChanceChk, EneBlinkPosSet); FF3 split it
// off and added the paralysis timers, the linked enemies, the attack
// reactions and the photo HUD targets. Reconstructed from FF3; FF2 gave the
// shape only.

#include <math.h>                               // ceil, fabsf, sqrtf
#include <string.h>                             // memset

#include "ingame/enemy/ene_main.h"
#include "ingame/map/MapCtrl.h"              // MapAreaGetHeight
#include "common/vu0math.h"                     // RotLimitChk, RotVectorZXY
#include "graphics/effect/effect_ene.h"         // the aura: EneAuraFrameBegin / End, EneAuraDraw
#include "ingame/enemy/enemy_act.h"            // EneActSet, EneAlgCtrl, EneBlinkCtrl, EneActRule, EneDamageHitHook
#include "graphics/game_model.h"                 // GameModel
#include "graphics/game_anime_model.h"           // GameAnimeModel::SetAnimeSpeed
#include "graphics/graph3d/gra3d.h"             // gra3dcamGetPosition
#include "ingame/char_model.h"               // CharModel::GetRootMove
#include "ingame/camera/finder_camera.h"        // GetFinderModeWrk
#include "ingame/enemy/enemy.h"                 // EnemyWrkClear, SetEneMahiMode, FUN_0019xxxx
#include "ingame/enemy/fly_ctrl.h"              // FlyRule
#include "ingame/ingame_effect.h"               // IgEffectEffectEndParticleReq
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/photo/finder_photo.h"          // GetPhotoModeWrk*
#include "ingame/photo/fndr_parts.h"            // EneGaugeWork, DamageDispWork, Filament*, PhotoGage4bc*
#include "ingame/plyr/player.h"                 // PLYR_WRK, PlyrSetMode, SetPlyrAnime, PlyrSePlay, ...
#include "ingame/plyr/plyr_mdl.h"               // PlyrFinderModeCancel
#include "ingame/plyr/plyr_gauge.h"             // PlyrGaugeDmgRate
#include "ingame/plyr/plyr_sub.h"               // PlyrMdlFxMain, PlyrMdlFxReq
#include "ingame/plyr/unit_ctl.h"               // UnitGetTrgtRot, ConvertRot2Dir, IsPosInView
#include "ingame/enemy/random_ghost.h"    // RandGhostWrkClear, RandGhostMain
#include "ingame/story_menu.h"                  // IsLockTblOn
#include "ingame/tattoo_event.h"                // TattooEvInit
#include "main/gphase.h"                        // GetGPhaseCtrl
#include "sdk/libvu0.h"                         // sceVu0AddVector, sceVu0SubVector
#include "system/os/system.h"                   // GetPALMode, GetSysWrk
#include "graphics/effect/effect_sub.h" // CheckPointDepth

// ──────────────────────────────────────────────────────────────────────
// Callees in runs that are not defined yet. Move each block to its real
// header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@0011cea8 (map hit)

// ──────────────────────────────────────────────────────────────────────

ENE_WRK ene_wrk[ENE_WRK_MAX];                   // FF3 data 0x2fa3a0

static ENE_WRK *ene_gauge_target;               // FF3 bss 0x3df2bc: the enemy the hp gauge follows

static void EnemyWrkOne(ENE_WRK *ene, int no);
static void EnemyActOne(ENE_WRK *ene);
static void EneRule(ENE_WRK *ene);
static void EneMahi0Ctrl(ENE_WRK *ene);
static void EneMahi1Ctrl(ENE_WRK *ene);
static void EneMahi2Ctrl(ENE_WRK *ene);
static void EneMahiModeSelect(ENE_WRK *ene);
static void EneMahiCopy(ENE_WRK *src, ENE_WRK *dst);
static void EneMahiLinkCopy(ENE_WRK *ene);
static void EneMotMoveGet(ENE_WRK *ene);
static void EneMoveCtrl(ENE_WRK *ene);
static void EneJointPosUpdate(ENE_WRK *ene);
static void EneHpCtrl(ENE_WRK *ene);
static void EneDamageApply(ENE_WRK *ene);
static void EneHpRegen(ENE_WRK *ene);
static void EneDamageStart(ENE_WRK *ene);
static void EneMapHitCtrl(ENE_WRK *ene);
static void EneMahiMain(ENE_WRK *ene);
static void EneAlphaCtrl(ENE_WRK *ene);
static void EneAttackHold(ENE_WRK *ene, u_char kind, u_short dmg_type);
static void EneAttackHold6(ENE_WRK *ene, u_char kind, u_short dmg_type);
static void EneAttackHit(ENE_WRK *ene, u_char kind, u_short dmg_type);
static void EnePhotoHudCtrl(void);
static void EneSearchNearVisible(ENE_WRK **out, int *out2);
static void EneFilamentCtrl(void);
static void EneHpGaugeCtrl(void);
static void EneBlinkPosSet(ENE_WRK *ene);

// The VU0 distance FF3 inlines here: x and z only (vsub.xyz, vmul.xyz,
// vaddz.x, vsqrt), so height does not count.
static float EneDistXZ(const float *a, const float *b)
{
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];

    return sqrtf(dx * dx + dz * dz);
}

static void EneCopyVector(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00186a88

void EnemyMainInit(void)
{
    memset(ene_wrk, 0, sizeof(ene_wrk));
    RandGhostWrkClear();
    EnemyWrkClear();
    TattooEvInit();
    EneSndInit();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00186ad0

void EnemyMainLoadRestore(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Nothing while phase 0x22 is requested or lock 0xb is on (the random
// ghosts still run before the lock check).
// FF3 0x00186ae8

void EnemyMain(void)
{
    static GPHASE_CTRL *gphase_ctrl;            // FF3: the shared inline cache sdata 0x37284c
    int i;

    if (gphase_ctrl == nullptr)
        gphase_ctrl = GetGPhaseCtrl();
    if (gphase_ctrl->next == 0x22)
        return;

    RandGhostMain();
    if (IsLockTblOn(0xb))
        return;

    for (i = 0; i < ENE_WRK_MAX; i++)
        EnemyWrkOne(&ene_wrk[i], i);

    EnePhotoHudCtrl();
    EneFilamentCtrl();
    EneHpGaugeCtrl();
    EneSndMain();
    FlyRule();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00186ba8

static void EnemyWrkOne(ENE_WRK *ene, int no)
{
    int next;

    switch (ene->state)
    {
    case ENE_STATE_LOADING:
        if (enemyIsReadyData(ene) == 0)
            return;
        next = ENE_STATE_READY;
        if (ene->_4dd == 1)
        {
            EnemyStart(SearchEneWrkNo(ene->type, ene->id));
            next = ENE_STATE_ACT;
        }
        ene->state = next;
        return;

    case ENE_STATE_ACT:
        EnemyActOne(ene);
        return;

    case ENE_STATE_RELEASE:
        EneRelease(no, 0);
        return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// One acting enemy, one frame. Lock 8 stops all of them, 10 the special
// (type 2) ones, 9 the others. After the rule, the +0x4d0 flags tie it to
// its first linked enemy (dat->link[0]) or carry it with all of them.
// FF3 0x00186c58 (inside EnemyWrkOne's Ghidra body until 2026-10-14)

static void EnemyActOne(ENE_WRK *ene)
{
    PLYR_WRK     *plyr;
    ENE_WRK      *other;
    sceVu0FVECTOR rot;
    sceVu0FVECTOR ofs;
    int           no;
    int           i;
    short         id;

    if (IsLockTblOn(8))
        return;
    if (IsLockTblOn(ene->type == 2 ? 10 : 9))
        return;

    plyr = ingame_wrk.plyr_wrk;
    ene->dist_old = ene->dist;
    ene->dist = EneDistXZ(plyr->pos, ene->pos);
    EneGaugeWork(&ene->gauge);
    DamageDispWork(&ene->dmg_disp);
    ene->flags_old = ene->flags;
    EneEffPosUpdate(ene);
    EneEffAlphaUpdate(ene);
    EneBlinkPosSet(ene);
    EneRule(ene);

    if (ingame_wrk.plyr_wrk->_234 == 0)
    {
        EneStepUpdate(ene);
        EneMotScrMain(ene);
        ene->fx->Main(ene->mdl, ene->step);
        EneMahiMain(ene);
    }

    // Placed at the linked enemy's position plus the common data's offset.
    // FF3 also builds the linked enemy's rotation with its joint 3 angle
    // here and then does not use it; the call is kept.
    if ((ene->flags2 & 0x40000000) != 0 && (id = ene->dat->link[0]) != -1)
    {
        no = (short)SearchEneWrkNo(ene->type, id);
        if (no != -1)
        {
            other = &ene_wrk[no];
            ofs[0] = ene->cmn->pos[0];
            ofs[1] = ene->cmn->pos[1];
            ofs[2] = ene->cmn->pos[2];
            ofs[3] = 0.0f;
            EneCopyVector(rot, other->rot);
            rot[1] = EneGetJointRot(other, 3, 1);
            sceVu0AddVector(ene->pos, other->pos, ofs);
        }
    }

    // ... and turned towards it.
    if ((ene->flags2 & 0x40000000) != 0 && (id = ene->dat->link[0]) != -1)
    {
        no = (short)SearchEneWrkNo(ene->type, id);
        if (no != -1)
        {
            UnitGetTrgtRot(ene->pos, ene_wrk[no].pos, rot, 3);
            ene->rot[1] = rot[1];
        }
    }

    // Carries all its linked enemies: position, rotation, their own offset,
    // and the alpha / eff.rate values.
    if ((ene->flags2 & 0x100000) != 0)
    {
        for (i = 0; i < 8; i++)
        {
            id = ene->dat->link[i];
            if (id < 0)
                continue;
            no = SearchEneWrkNo(ene->type, id);
            if (no < 0)
                continue;
            other = &ene_wrk[no];
            EneCopyVector(other->pos, ene->pos);
            EneCopyVector(other->rot, ene->rot);
            sceVu0AddVector(other->pos, other->pos, other->ofs);
            other->alpha_a = ene->alpha_a;
            other->eff.rate[1] = ene->eff.rate[1];
            other->eff.rate[0] = ene->eff.rate[0];
        }
    }

    // Takes its first linked enemy's position and rotation ...
    if ((ene->flags2 & 0x20000) != 0 && ene->dat->link[0] >= 0)
    {
        no = SearchEneWrkNo(ene->type, ene->dat->link[0]);
        if (no >= 0)
        {
            EneCopyVector(ene->pos, ene_wrk[no].pos);
            EneCopyVector(ene->rot, ene_wrk[no].rot);
        }
    }

    // ... or its alpha and eff.rate values.
    if ((ene->flags2 & 0x40000) != 0 && ene->dat->link[0] >= 0)
    {
        no = SearchEneWrkNo(ene->type, ene->dat->link[0]);
        if (no >= 0)
        {
            other = &ene_wrk[no];
            ene->alpha_a = other->alpha_a;
            ene->alpha = other->alpha;
            ene->eff.rate[0] = other->eff.rate[0];
            ene->eff.rate[1] = other->eff.rate[1];
        }
    }

    if ((ene->flags2 & 0x80000) != 0)
        EneMahiLinkCopy(ene);
}

// ──────────────────────────────────────────────────────────────────────
// Draws an acting enemy with a model (_554 != 0xff, flags 0x200000 off).
// The normal pass (flags without 0x200) skips the +0x4d0 0x800000 enemies
// and steps the motion of the rest; the second pass (0x200) only draws the
// 0xc00000 ones, stepping the 0x800000 ones. A linked enemy of kind 0x1e
// that is alive is drawn after it, in the normal way.
// FF3 0x00187060

void EnemyDrawOne(ENE_WRK *ene, u_int flags)
{
    ENE_AURA_DAT *fx;

    for (;;)
    {
        if (IsLockTblOn(0xb) || ene->state != ENE_STATE_ACT || ene->_554 == 0xff
            || (ene->flags & 0x200000) != 0)
            return;

        if ((flags & 0x200) != 0)
        {
            if ((ene->flags2 & 0xc00000) == 0)
                return;
            if ((ene->flags2 & 0x800000) != 0)
                EnemyMotionWorkOne(ene);
        }
        else
        {
            if ((ene->flags2 & 0x800000) != 0)
                return;
            EnemyMotionWorkOne(ene);
        }

        fx = EneAuraDatGet(ene, 0xff);
        if ((ene->flags2 & 0x40) == 0 && EneAuraDatGet(ene, 0xff)->on != 0)
        {
            if (ene->alpha == 0)
            {
                ene->mdl->SetAlpha(0);
                ene->mdl->DrawMdl();
            }
            else
            {
                ene->mdl->SetAlpha(0x80);
                EneAuraFrameBegin();
                ene->mdl->DrawMdl();
                EneAuraFrameEnd(fx);
                EneAuraDraw(fx, ene->alpha, ene->aura_time);
            }
        }
        else
        {
            ene->mdl->SetAlpha(ene->alpha);
            ene->mdl->DrawMdl();
        }

        if ((flags & 0x200) == 0 && (ene->flags & 0x400000) == 0)
            ene->aura_time += 2.0f;

        if (ene->type == 2 || ene->dat->link[0] < 0)
            return;
        int no = SearchEneWrkNo(ene->type, ene->dat->link[0]);
        if (no < 0)
            return;
        ENE_WRK *other = &ene_wrk[no];
        if (other->cmn->id != 0x1e || other->state != ENE_STATE_ACT || other->hp == 0)
            return;
        ene = other;
        flags = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00187268

void EnemyMotionWorkOne(ENE_WRK *ene)
{
    if (IsLockTblOn(0xb) || ene->state != ENE_STATE_ACT || ene->_554 == 0xff)
        return;

    static_cast<GameAnimeModel *>(ene->mdl)->SetAnimeSpeed(ene->step);
    ene->mdl->UpdateMatrix();
    EneMdlWatchUpdate(ene);
}

// ──────────────────────────────────────────────────────────────────────
// Nothing while paralysis 0 runs (flags bit 0).
// FF3 0x001872e0

static void EneRule(ENE_WRK *ene)
{
    float t;

    if ((ene->flags & 1) != 0)
        return;

    t = ene->_6f0 - ene->step;
    if (0.0f < t)
        ene->_6f0 = t;
    else
        ene->_6f0 = 0.0f;

    if (ingame_wrk.plyr_wrk->_234 == 0)
        EneAnimeFlagUpdate(ene);
    EneHpCtrl(ene);
    EneActRule(ene);
    if (ingame_wrk.plyr_wrk->_234 == 0)
        EneAlgCtrl(ene);
    EneAlphaCtrl(ene);
    EneMoveCtrl(ene);
    EneJointPosUpdate(ene);
}

// ──────────────────────────────────────────────────────────────────────
// The three paralyses ("mahi"): each has a frame count, a start time (the
// system frame counter; the latest running one sets the look) and a flags
// bit. Its end plays particles and SE 9 when the enemy is visible.
// FF3 0x001873b0

static void EneMahi0Ctrl(ENE_WRK *ene)
{
    short t = ene->mahi_time[0];

    if (t != 0 && (ene->mahi_time[0] = t - 1, (short)(t - 1) < 1))
        EneMahi0End(ene, 1);
}

static SYS_WRK *ene_sys_wrk;                    // FF3: the shared inline cache sdata 0x372890

static u_int EneSysCount(void)
{
    if (ene_sys_wrk == nullptr)
        ene_sys_wrk = GetSysWrk();
    return ene_sys_wrk->count;
}

// FF3 0x00187400 (callcheck: GetSysWrk (through EneSysCount))

void EneMahi0Req(ENE_WRK *ene, float frames)
{
    ene->mahi_time[0] = (short)(int)frames;
    ene->mahi_start[0] = EneSysCount();
    ene->flags |= 1;
    SetEneMahiMode(ene, 1.0f, 0.95f, 0.0f);
}

// FF3 0x00187478

void EneMahi0End(ENE_WRK *ene, int fx)
{
    if (ene->mahi_time[0] > 0 || (ene->flags & 1) != 0)
    {
        ene->mahi_start[0] = 0;
        ene->mahi_time[0] = 0;
        if (ene->alpha != 0 && fx != 0)
        {
            IgEffectEffectEndParticleReq(ene->jpos1, 4);
            PlyrSePlay(ingame_wrk.plyr_wrk, 9, 100, 0x1000);
        }
        ene->flags &= ~(u_long)1;
    }
}

// FF3 0x00187508 (was CallAmuletFire, a low FF2 match)

void EneSetStepReq(ENE_WRK *ene, short frames, float rate)
{
    ene->step_time = frames;
    ene->step_req = rate;
}

// FF3 0x00187518

static void EneMahi1Ctrl(ENE_WRK *ene)
{
    short t = ene->mahi_time[1];

    if (t != 0 && (ene->mahi_time[1] = t - 1, (short)(t - 1) < 1))
        EneMahi1End(ene, 1);
}

// FF3 0x00187568 (callcheck: GetSysWrk (through EneSysCount))

void EneMahi1Req(ENE_WRK *ene, short frames, float rate)
{
    ene->mahi_time[1] = frames;
    ene->mahi1_rate = rate;
    ene->mahi_start[1] = EneSysCount();
    ene->flags |= 2;
    SetEneMahiMode(ene, 1.0f, 0.6f, 0.85f);
}

// FF3 0x001875f0

void EneMahi1End(ENE_WRK *ene, int fx)
{
    if (ene->mahi_time[1] > 0 || (ene->flags & 2) != 0)
    {
        ene->mahi_start[1] = 0;
        ene->mahi_time[1] = 0;
        if (ene->alpha != 0 && fx != 0)
        {
            IgEffectEffectEndParticleReq(ene->jpos1, 0);
            PlyrSePlay(ingame_wrk.plyr_wrk, 9, 100, 0x1000);
        }
        ene->flags &= ~(u_long)2;
    }
}

// FF3 0x00187678

static void EneMahi2Ctrl(ENE_WRK *ene)
{
    short t = ene->mahi_time[2];

    if (t != 0 && (ene->mahi_time[2] = t - 1, (short)(t - 1) < 1))
        EneMahi2End(ene, 1);
}

// FF3 0x001876c8 (callcheck: GetSysWrk (through EneSysCount))

void EneMahi2Req(ENE_WRK *ene, float frames)
{
    ene->mahi_time[2] = (short)(int)frames;
    ene->mahi_start[2] = EneSysCount();
    ene->flags |= 4;
    SetEneMahiMode(ene, 0.1f, 0.82f, 1.0f);
}

// FF3 0x00187748

void EneMahi2End(ENE_WRK *ene, int fx)
{
    if (ene->mahi_time[2] > 0 || (ene->flags & 4) != 0)
    {
        ene->mahi_start[2] = 0;
        ene->mahi_time[2] = 0;
        if (ene->alpha != 0 && fx != 0)
        {
            IgEffectEffectEndParticleReq(ene->jpos1, 2);
            PlyrSePlay(ingame_wrk.plyr_wrk, 9, 100, 0x1000);
        }
        ene->flags &= ~(u_long)4;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The running paralysis that began last sets the look and mahi_rate; with
// none, EneMahiModeDefault and rate 1.
// FF3 0x001877d0

static void EneMahiModeSelect(ENE_WRK *ene)
{
    u_int best = 0;
    int   sel = 0;
    int   i;

    for (i = 0; i < 3; i++)
    {
        if (ene->mahi_time[i] > 0 && best < ene->mahi_start[i])
        {
            best = ene->mahi_start[i];
            sel = i;
        }
    }

    if (best != 0)
    {
        if (sel == 1)
        {
            SetEneMahiMode(ene, 1.0f, 0.6f, 0.85f);
            ene->mahi_rate = ene->mahi1_rate;
            return;
        }
        if (sel == 0)
        {
            SetEneMahiMode(ene, 1.0f, 0.95f, 0.0f);
            ene->mahi_rate = 0.0f;
            return;
        }
        if (sel == 2)
        {
            SetEneMahiMode(ene, 0.1f, 0.82f, 1.0f);
            ene->mahi_rate = (ene->flags & 2) != 0 ? ene->mahi1_rate : 1.0f;
            return;
        }
    }

    EneMahiModeDefault(ene);
    ene->mahi_rate = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// dst takes src's paralyses (ending those src has not, with effects) and
// its step fields.
// FF3 0x00187930

static void EneMahiCopy(ENE_WRK *src, ENE_WRK *dst)
{
    if ((src->flags & 1) == 0)
        EneMahi0End(dst, 1);
    else
        dst->flags |= 1;
    if ((src->flags & 2) == 0)
        EneMahi1End(dst, 1);
    else
        dst->flags |= 2;
    if ((src->flags & 4) == 0)
        EneMahi2End(dst, 1);
    else
        dst->flags |= 4;

    dst->mahi_time[0] = src->mahi_time[0];
    dst->mahi_start[0] = src->mahi_start[0];
    dst->mahi_time[1] = src->mahi_time[1];
    dst->mahi_start[1] = src->mahi_start[1];
    dst->mahi_time[2] = src->mahi_time[2];
    dst->_528 = src->_528;
    dst->mahi_start[2] = src->mahi_start[2];
    dst->step_time = src->step_time;
    dst->step = src->step;
    dst->step_req = src->step_req;
    dst->_56c = src->_56c;
    dst->mahi_rate = src->mahi_rate;
    dst->_4e0[0] = src->_4e0[0];
    dst->_4e0[1] = src->_4e0[1];
    dst->_4e0[2] = src->_4e0[2];
    dst->_4e0[3] = src->_4e0[3];
}

// FF3 0x00187a60

static void EneMahiLinkCopy(ENE_WRK *ene)
{
    int i;
    int no;

    if (ene->type == 2)
        return;

    for (i = 0; i < 8; i++)
    {
        if (ene->dat->link[i] < 0)
            continue;
        no = SearchEneWrkNo(ene->type, ene->dat->link[i]);
        if (no >= 0 && ene_wrk[no].state == ENE_STATE_ACT)
            EneMahiCopy(ene, &ene_wrk[no]);
    }
}

// FF3 0x00187b10

void EneMahiAllEnd(ENE_WRK *ene)
{
    ENE_WRK *other;
    int      i;
    int      no;

    if (ene->type == 2)
        return;

    EneMahi0End(ene, 0);
    EneMahi1End(ene, 0);
    EneMahi2End(ene, 0);
    for (i = 0; i < 8; i++)
    {
        if (ene->dat->link[i] < 0)
            continue;
        no = SearchEneWrkNo(ene->type, ene->dat->link[i]);
        if (no >= 0 && ene_wrk[no].state == ENE_STATE_ACT)
        {
            other = &ene_wrk[no];
            EneMahi0End(other, 0);
            EneMahi1End(other, 0);
            EneMahi2End(other, 0);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00187c00

static void EneMotMoveGet(ENE_WRK *ene)
{
    if (ene->mot_kind < 2)
    {
        ene->move[0] = -static_cast<CharModel *>(ene->mdl)->GetRootMove(0, ene->mot_attr);
        ene->move[1] = static_cast<CharModel *>(ene->mdl)->GetRootMove(1, ene->mot_attr);
        ene->move[2] = static_cast<CharModel *>(ene->mdl)->GetRootMove(2, ene->mot_attr);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The turn (a one-frame rot_add, else towards rot_trgt at turn_spd), the
// motion's move turned by rot, then for a follower the height towards the
// player's in proportion to the distance closed. All rates times the step.
// FF3 0x00187c70

static void EneMoveCtrl(ENE_WRK *ene)
{
    PLYR_WRK     *plyr;
    sceVu0FVECTOR tmp;
    float         diff;
    float         spd;
    float         target;
    float         d_new;
    float         d_old;
    float         dy;

    EneMotMoveGet(ene);

    if (ene->rot_add[1] != 0.0f)
    {
        sceVu0AddVector(ene->rot, ene->rot, ene->rot_add);
    }
    else if (ene->turn_spd != 0.0f)
    {
        if (ene->follow == 1)
            UnitGetTrgtRot(ene->pos, ingame_wrk.plyr_wrk->pos, ene->rot_trgt, 3);
        spd = ene->turn_spd;
        target = ene->rot_trgt[1];
        diff = target - ene->rot[1];
        RotLimitChk(&diff);
        spd = spd * ene->step;
        if (0.0f < diff ? spd <= diff : spd <= -diff)
        {
            ene->rot[1] = 0.0f < diff ? ene->rot[1] + spd : ene->rot[1] - spd;
            RotLimitChk(&ene->rot[1]);
        }
        else
        {
            ene->rot[1] = target;
        }
    }

    tmp[0] = ene->turn[0] * ene->step;
    tmp[1] = ene->turn[1] * ene->step;
    tmp[2] = ene->turn[2] * ene->step;
    tmp[3] = ene->turn[3] * ene->step;
    sceVu0AddVector(ene->rot, ene->rot, tmp);
    RotLimitChk(&ene->rot[1]);
    RotVectorZXY(ene->move, ene->move, ene->rot);
    if ((ene->flags & (u_long)0x800000000) != 0)
        ene->move[1] = 0.0f;

    EneCopyVector(ene->pos_old, ene->pos);
    EneCopyVector(ene->move_old, ene->move_last);
    EneCopyVector(ene->move_last, ene->move);
    sceVu0AddVector(ene->pos, ene->pos, ene->move);

    plyr = ingame_wrk.plyr_wrk;
    if (plyr->pos[1] != ene->pos_old[1] && ene->follow == 1 && (ene->flags2 & 8) == 0)
    {
        d_new = EneDistXZ(plyr->pos, ene->pos);
        d_old = EneDistXZ(plyr->pos, ene->pos_old);
        if (d_old < d_new)
        {
            ene->pos[1] = ene->pos_old[1];
        }
        else
        {
            dy = (d_new * (ingame_wrk.plyr_wrk->pos[1] - ene->pos_old[1])) / d_old;
            if (fabsf(d_new - d_old) < fabsf(dy))
                dy = (-dy / fabsf(dy)) * (d_new - d_old);
            ene->pos[1] = dy + ene->pos_old[1];
        }
    }

    EneMapHitCtrl(ene);
    ene->rot_add[0] = ene->rot_add[1] = ene->rot_add[2] = ene->rot_add[3] = 0.0f;
    ene->move[0] = ene->move[1] = ene->move[2] = ene->move[3] = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00187fb8

static void EneJointPosUpdate(ENE_WRK *ene)
{
    sceVu0FVECTOR in = { 0.0f, 0.0f, 0.0f, 1.0f };

    EneGetJointPos(ene, 2, ene->mpos, in);
    EneGetJointPos(ene, 1, ene->jpos1, in);
    EneGetJointPos(ene, 0x13, ene->jpos13, in);
    EneGetJointPos(ene, 7, ene->jpos7, in);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00188040

static void EneHpCtrl(ENE_WRK *ene)
{
    if ((ene->flags & 0x20000) != 0)
        EneDamageApply(ene);
    else if ((ene->flags2 & 0x10) != 0)
        EneHpRegen(ene);
}

// ──────────────────────────────────────────────────────────────────────
// Takes the pending damage. At 0 hp (or with flags bit 42) it dies:
// reaction 4; a hit with dmg_react > 1 knocks it down: reaction 3.
// FF3 0x00188088

static void EneDamageApply(ENE_WRK *ene)
{
    static FINDER_MODE_WRK *finder_wrk;         // FF3: the shared inline cache sdata 0x372a40
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    int       reaction = 0xff;
    int       hp;
    u_long    fl;

    EneDamageStart(ene);
    hp = ene->hp - ene->dmg;
    if (hp < 1 || (ene->flags & (u_long)0x40000000000) != 0)
    {
        ene->hp = 0;
        reaction = 4;
        ene->alg.bwait_time = 0.0f;
        EneStepReset(ene, 7);
        plyr->status_time = 0.0f;
        ene->flags = (ene->flags & ~(u_long)0x400000a0000) | (u_long)0x800000000;
        if (finder_wrk == nullptr)
            finder_wrk = GetFinderModeWrk();
        finder_wrk->AddKill();
    }
    else
    {
        ene->hp = hp;
        if (ene->act_no != 3)
            EneDamageHitHook(ene);
        fl = ene->flags;
        if (ene->dmg_react > 1)
        {
            reaction = 3;
            fl |= (u_long)0x800000000;
            ene->flags = fl;
        }
        ene->flags = fl & ~(u_long)0x20000;
    }

    if (PlyrIsHeldBy(ene))
        ingame_wrk.plyr_wrk->status_time = 0.0f;
    ene->dmg_last = ene->dmg;
    ene->dmg = 0;
    if (reaction != 0xff)
        EneActSet(ene, (u_char)reaction);
}

// ──────────────────────────────────────────────────────────────────────
// After regen_wait seconds below full hp without being down, hp grows by
// dat->regen a frame, at most 5 at once.
// FF3 0x00188220

static void EneHpRegen(ENE_WRK *ene)
{
    float acc;
    float add;
    int   hp;
    u_int cnt;
    int   wait;

    if (ene->hp < 1)
        return;

    if ((ene->flags & (u_long)0x800000000) == 0 && ene->hp < ene->hp_max)
    {
        ene->regen_cnt++;
    }
    else
    {
        ene->regen_cnt = 0;
        ene->regen_acc = 0.0f;
    }

    cnt = ene->regen_cnt;
    wait = ene->dat->regen_wait;
    if (GetPALMode() == 0)
    {
        if ((u_int)(wait * 30) <= cnt)
            ene->regen_acc += ene->dat->regen;
    }
    else
    {
        if ((u_int)(wait * 25) <= cnt)
            ene->regen_acc += ene->dat->regen;
    }

    acc = ene->regen_acc;
    if (0.0f < acc)
    {
        add = 5.0f;
        if (acc <= 5.0f)
            add = (float)(int)acc;
        hp = ene->hp + (int)add;
        if (ene->hp_max < hp)
            hp = ene->hp_max;
        ene->regen_acc = acc - add;
        ene->hp = hp;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00188350 (was SetLeaf, a low FF2 match)

static void EneDamageStart(ENE_WRK *ene)
{
    ene->hold_pow = 0.0f;
    EneStepReset(ene, 1);
    ene->tpos = ene->mpos;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00188380

static void EneMapHitCtrl(ENE_WRK *ene)
{
    s_char area = (s_char)GetPlyrAreaNo();

    ene->area = area;
    if (area != -1 && (ene->flags2 & 4) != 0)
        MapAreaGetHeight(ene->pos, ene->pos, area, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001883e0

int SearchEneWrkNo(u_char type, short id)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state != ENE_STATE_FREE && ene_wrk[i].id == id && ene_wrk[i].type == type)
            return i;
    }
    return -1;
}

// FF3 0x00188430

ENE_WRK *EneGetWrk(u_char type, short id)
{
    s_char no = (s_char)SearchEneWrkNo(type, id);

    if (no < 0)
        return nullptr;
    return &ene_wrk[no];
}

// FF3 0x00188478 (was EffectKusabiHazeReq, a low FF2 match)

int GetEneWrkBuffer(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state == ENE_STATE_FREE)
            return i;
    }
    return -1;
}

// FF3 0x001884b8

void *GetEneDatP(int type, int no)
{
    switch (type)
    {
    case 0: return ene_dat_tbl0 + no * 0xa8;
    case 1: return ene_dat_tbl1 + no * 0xa8;
    case 2: return ene_dat_tbl2 + no * 0x60;
    }
    return nullptr;
}

// FF3 0x00188520

int IsActEnemy(int no)
{
    return ene_wrk[no].state == ENE_STATE_ACT;
}

// ──────────────────────────────────────────────────────────────────────
// The model's vtable +0x34 maps the joint to its coordinate (CharModel's
// 0x145528 for every model class here); FF3 calls them through the vtable.
// FF3 0x001885c8 (was EffectSaeHazeReq, a low FF2 match)

float EneGetJointRot(ENE_WRK *ene, int joint, int axis)
{
    int coord = ene->mdl->GetJointCoord(joint);

    return ene->mdl->GetCoordRot(coord, axis);
}

// As PlyrGetJointPos, for an enemy (vtable +0x2c: CharEnemyModel's 0x144d70).
// FF3 0x00188610 (was HazeCtrlInit, a wrong FF2 match)

void EneGetJointPos(ENE_WRK *ene, int joint, float *out, float *in)
{
    sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };
    int           coord;

    coord = ene->mdl->GetJointCoord(joint);
    if (coord == -1)
        return;
    if (in == nullptr)
        in = origin;
    ene->mdl->GetCoordPos(out, coord, in);
}

// ──────────────────────────────────────────────────────────────────────
// The lowest level among the live enemies: types 0 and 2 count as 1, type 1
// has its row's level; 3 with none. Callers: enemy.c 0x1924c8..0x192640,
// the random ghosts.
// FF3 0x001886b0

s_char EneGetMinLevel(void)
{
    s_char res = 3;
    s_char cand = 3;
    int    i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state == ENE_STATE_ACT && ene->hp != 0)
        {
            if (ene->type == 1)
                cand = (s_char)ene->rg.row->level;
            else if (ene->type == 0 || ene->type == 2)
                cand = 1;
        }
        if (cand < res)
            res = cand;
    }
    return res;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00188750

static void EneMahiMain(ENE_WRK *ene)
{
    if (ene->_528 != 0)
        ene->_528--;
    EneMahi1Ctrl(ene);
    EneMahi2Ctrl(ene);
    EneMahi0Ctrl(ene);
    EneMahiModeSelect(ene);
}

// ──────────────────────────────────────────────────────────────────────
// With +0x4d0 bit 0 the enemy is seen only between the common data's near
// and far distances (x 0.2), fading over 4 (or a tenth of a band under 40)
// at both ends. The result is scaled by the in-finder / outside factor and
// the two other alpha bytes (128 = 1); 0x8000 hides it while _528 is 0.
// FF3 0x001887a8

static void EneAlphaCtrl(ENE_WRK *ene)
{
    float rate = 1.0f;
    float d;
    float near_d;
    float far_d;
    float w;
    u_char b;

    EneBlinkCtrl(ene);
    if ((ene->flags2 & 1) != 0)
    {
        d = EneDistXZ(ingame_wrk.plyr_wrk->pos, ene->pos);
        far_d = ene->cmn->fade_far * 0.2f;
        near_d = ene->cmn->fade_near * 0.2f;
        w = 4.0f;
        if (far_d - near_d < 40.0f)
            w = (far_d - near_d) * 0.1f;

        if (d < near_d)
        {
            rate = 0.0f;
        }
        else if (d < near_d + w)
        {
            w = (near_d + w) - near_d;
            if (0.0f < w)
                rate = (d - near_d) / w;
        }
        else if (d < far_d - w)
        {
            rate = 1.0f;
        }
        else if (far_d <= d)
        {
            rate = 0.0f;
        }
        else
        {
            w = far_d - (far_d - w);
            if (0.0f < w)
                rate = (far_d - d) / w;
        }
    }

    ene->_578 = 1.0f;
    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
        b = ene->alpha_nrm;
    else
        b = ene->alpha_fdr;
    rate = (float)b * 0.0078125f * (float)ene->alpha_a * 0.0078125f * (float)ene->alpha_b * 0.0078125f
           * ene->_578 * rate;
    if ((ene->flags2 & 0x8000) != 0 && ene->_528 < 1)
        rate = 0.0f;
    ene->alpha_rate = rate;
    ene->alpha = (u_char)(int)(rate * 128.0f);
}

// ──────────────────────────────────────────────────────────────────────
// Mode 1 / 2 / 3 set 0x4000000 / 0x200000 / both on the acting slots whose
// bit in mask is clear; mode 0 clears both on every slot with 0x4000000.
// FF3 0x001889e8

void EneFlagReq(u_char mode, u_char mask)
{
    u_long set;
    int    i;

    switch (mode)
    {
    case 0:
        for (i = 0; i < ENE_WRK_MAX; i++)
        {
            if ((ene_wrk[i].flags & 0x4000000) != 0)
                ene_wrk[i].flags &= ~(u_long)0x4200000;
        }
        return;
    case 1: set = 0x4000000; break;
    case 2: set = 0x200000; break;
    case 3: set = 0x4200000; break;
    default:
        return;
    }

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state == ENE_STATE_ACT && ((mask >> i) & 1) == 0)
            ene_wrk[i].flags |= set;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF2 ShutterChanceChk: a live, targetable enemy whose last flags show a
// shutter chance (0x1080: 2, checked over all slots before 0x880: 1).
// FF3 0x00188b80

int ShutterChanceChk(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state == ENE_STATE_ACT && ene->hp != 0 && (ene->flags_old & 0x100000) == 0
            && (ene->flags_old & 0x11080) == 0x1080 && (ene->flags2 & 0x100) == 0)
            return 2;
        if (ene->state == ENE_STATE_ACT && ene->hp != 0 && (ene->flags_old & 0x100000) == 0
            && (ene->flags_old & 0x10880) == 0x880 && (ene->flags2 & 0x100) == 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The enemy's attack lands: on the ladder (mode 5 / flags2 0x2000000) the
// damage is a plain hit at once (returns 0); otherwise the reaction by
// dmg_type: 1, 7, 9 hit, 3..5, 10, 11 hold, 6 the short hold; then the hit
// camera. dmg_type 9 uses dat->atk9, the others hit_dmg.
// FF3 0x00188c70

int EneAttackReq(ENE_WRK *ene, u_short dmg_type, u_char kind)
{
    PLYR_WRK *plyr;

    dmg_type &= 0xff;
    PlyrMode10Cancel(ingame_wrk.plyr_wrk);
    if (PlyrIsMode5Or2000000())
    {
        plyr = ingame_wrk.plyr_wrk;
        if (dmg_type == 1)
            plyr->dmg = ene->hit_dmg;
        else if (dmg_type == 9)
            plyr->dmg = ene->dat->atk9;
        else
            plyr->dmg = ene->hit_dmg;
        plyr->flags |= PLYR_FLG_20000;
        plyr->flags &= ~(u_long)PLYR_FLG_10000;
        ene->flags |= 0x4000;
        plyr->_147 = kind;
        plyr->ene_no = ene->alg.idx;
        PlyrSetEneWait(plyr, 0x78);
        plyr->status_time = 0.0f;
        ene->atk_type = dmg_type;
        plyr->dmg_type = dmg_type;
        ene->hold_pow = 0.0f;
        PlyrHitDamage();
        return 0;
    }

    switch (dmg_type)
    {
    case 1:
    case 7:
    case 9:
        EneAttackHit(ene, kind, dmg_type);
        break;
    case 3:
    case 4:
    case 5:
    case 10:
    case 11:
        EneAttackHold(ene, kind, dmg_type);
        break;
    case 6:
        EneAttackHold6(ene, kind, dmg_type);
        break;
    default:
        return 1;
    }
    PlyrDamageCameraReq(ene);
    return 1;
}

// The player's hold anime of each attacker kind (data 0x32fe90: the first
// row of player.c's plyr_hold_anime, the grab).
static const u_char ene_hold_grab_anime[16] =
{
    0x80, 0x8c, 0x6d, 0x86, 0x89, 0x83, 0x70, 0x72, 0x73, 0x76, 0x7a, 0x7d, 0x00, 0x00, 0x6d, 0x9f,
};

// Puts the player into the hold (mode 1) and the enemy in front of them:
// hold_dist x 0.2 ahead along the facing to the enemy (kind 8 keeps the
// player's), at the hold height ofs[1]. An enemy lying on its side
// (|rot[2]| >= pi/2) only moves in x / z and faces the player.
// FF3 0x00188e08

static void EneAttackHold(ENE_WRK *ene, u_char kind, u_short dmg_type)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR ppos;
    sceVu0FVECTOR off;
    sceVu0FVECTOR prot;
    sceVu0FVECTOR rot_a;
    sceVu0FVECTOR rot_b;
    int           fps;

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0)
        PlyrFinderModeCancel();
    ingame_wrk.plyr_wrk->flags2 &= ~(u_long)0xf;
    PlyrSetMode(PLYR_MODE_DMG1, nullptr);
    plyr->_147 = kind;
    plyr->flags &= ~(u_long)0x30000;
    plyr->ene_no = ene->alg.idx;
    PlyrSetEneWait(plyr, 0x78);
    plyr->status_time = 0.0f;
    plyr->dmg = ene->hit_dmg;
    ene->atk_type = dmg_type & 0xff;
    plyr->dmg_type = dmg_type & 0xff;
    SetPlyrAnime(ene_hold_grab_anime[plyr->_147], 0, 0xff, 0xff, 0, 0.0f);

    fps = GetPALMode() == 0 ? 30 : 25;
    ene->hold_pow = (float)(ene->dat->hold_time * fps);
    ene->pos[1] = plyr->pos[1];
    EneCopyVector(ppos, plyr->pos);
    UnitGetTrgtRot(ppos, ene->pos, rot_a, 3);
    EneCopyVector(prot, plyr->rot);
    if (plyr->_147 != 8)
        prot[1] = rot_a[1];
    off[0] = 0.0f;
    off[1] = ene->ofs[1];
    off[2] = ene->dat->hold_dist * 0.2f;
    off[3] = 0.0f;
    RotVectorZXY(off, off, prot);

    if (1.5707964f <= fabsf(ene->rot[2]))
    {
        ene->pos[0] = plyr->pos[0] + off[0];
        ene->pos[2] = plyr->pos[2] + off[2];
        UnitGetTrgtRot(ene->pos, plyr->pos, rot_b, 2);
        ene->rot[0] = 0.0f;
        ene->rot[1] = rot_b[1];
    }
    else
    {
        sceVu0AddVector(ene->pos, plyr->pos, off);
        if (plyr->_147 == 9 || plyr->_147 == 8)
            EneCopyVector(ene->rot, plyr->rot);
        else
            UnitGetTrgtRot(ene->pos, plyr->pos, ene->rot, 2);
        ene->pos[1] = plyr->pos[1] + ene->ofs[1];
    }
}

// dmg_type 6: as the hold without its timer; the enemy steps back by
// hold_dist x 0.2 along its own facing and the two face each other.
// FF3 0x00189098

static void EneAttackHold6(ENE_WRK *ene, u_char kind, u_short dmg_type)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR off;

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0)
        PlyrFinderModeCancel();
    ingame_wrk.plyr_wrk->flags2 &= ~(u_long)0xf;
    PlyrSetMode(PLYR_MODE_DMG1, nullptr);
    plyr->_147 = kind;
    plyr->flags &= ~(u_long)0x30000;
    plyr->ene_no = ene->alg.idx;
    PlyrSetEneWait(plyr, 0x78);
    plyr->status_time = 0.0f;
    plyr->dmg = ene->hit_dmg;
    ene->atk_type = dmg_type & 0xff;
    plyr->dmg_type = dmg_type & 0xff;
    SetPlyrAnime(ene_hold_grab_anime[plyr->_147], 0, 0xff, 0xff, 0, 0.0f);

    ene->hold_pow = 0.0f;
    ene->pos[1] = plyr->pos[1];
    off[0] = 0.0f;
    off[1] = ene->ofs[1];
    off[2] = ene->dat->hold_dist * 0.2f;
    off[3] = 0.0f;
    RotVectorZXY(off, off, ene->rot);
    sceVu0SubVector(ene->pos, plyr->pos, off);
    UnitGetTrgtRot(ene->pos, plyr->pos, ene->rot, 2);
    UnitGetTrgtRot(plyr->pos, ene->pos, plyr->rot, 2);
}

// A hit (mode 2): types 1 and 9 play the hit from the side the enemy is on
// (0x6b / 0x6c), 7 plays 0x79; then the model's red flash.
// FF3 0x00189268

static void EneAttackHit(ENE_WRK *ene, u_char kind, u_short dmg_type)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR rot;
    u_char        anime;

    dmg_type &= 0xff;
    ingame_wrk.plyr_wrk->flags2 &= ~(u_long)0xf;
    PlyrSetMode(PLYR_MODE_DMG2, nullptr);
    ene->flags = (ene->flags & ~(u_long)0x300000000) | 0x4000;
    plyr->_147 = kind;
    plyr->ene_no = ene->alg.idx;
    PlyrSetEneWait(plyr, 0x78);
    plyr->status_time = 0.0f;
    if (dmg_type == 9)
        plyr->dmg = ene->dat->atk9;
    else
        plyr->dmg = ene->hit_dmg;
    ene->atk_type = dmg_type;
    plyr->dmg_type = dmg_type;

    if (dmg_type == 1 || dmg_type == 9)
    {
        UnitGetTrgtRot(plyr->pos, ene->pos, rot, 2);
        rot[1] = rot[1] - plyr->rot[1];
        RotLimitChk(&rot[1]);
        anime = ConvertRot2Dir(rot[1], 2) != 0 ? 0x6c : 0x6b;
        SetPlyrAnime(anime, 0, 0xff, 0xff, 1, 0.0f);
    }
    else if (dmg_type == 7)
    {
        SetPlyrAnime(0x79, 0, 0xff, 0xff, 1, 0.0f);
    }
    ingame_wrk.plyr_wrk->mdl_fx->Req(0x3c, 0x3c, 0x1e, 0x80, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001893e8 (callcheck: litodp, __divdf3, FUN_002942b8, __fixdfsi (the double
// conversions, ceil 0x2942b8 and the int conversion))

int EneHpRateCeil(ENE_WRK *ene)
{
    return (int)ceil((double)ene->hp / (double)ene->hp_max);
}

// FF3 0x00189440

int EneGetState(u_char type, short id)
{
    int no = SearchEneWrkNo(type, id);

    if (no < 0)
        return 0;
    return ene_wrk[no].state;
}

// FF3 0x00189480

int EneGetPos(float *out, u_char type, short id)
{
    int no = SearchEneWrkNo(type, id);

    if (no != -1)
        EneCopyVector(out, ene_wrk[no].pos);
    return no != -1;
}

// FF3 0x001894e0

int EneIsAnyAlive(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state == ENE_STATE_ACT && ene_wrk[i].hp != 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The camera HUD's target: the nearest acting enemy (one near the finder
// centre, flags 0x400, first; type 2 apart, and only near the centre).
// FF3 0x00189528

static void EnePhotoHudCtrl(void)
{
    ENE_WRK        *near_e = nullptr;
    ENE_WRK        *near_fdr = nullptr;
    ENE_WRK        *special = nullptr;
    PHOTO_WRK_180  *w180 = GetPhotoModeWrk180();
    PHOTO_WRK_1E8  *w1e8 = GetPhotoModeWrk1e8();
    PHOTO_MODE_WRK *photo = GetPhotoModeWrk();
    int             i;
    int             on;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state != ENE_STATE_ACT || (ene->flags2 & 0x1000) != 0)
            continue;
        if (ene->type == 2)
        {
            if ((ene->flags & 0x400) != 0 && (special == nullptr || ene->dist < special->dist))
                special = ene;
        }
        else
        {
            if (near_e == nullptr || ene->dist < near_e->dist)
                near_e = ene;
            if ((ene->flags & 0x400) != 0 && (near_fdr == nullptr || ene->dist < near_fdr->dist))
                near_fdr = ene;
        }
    }
    if (near_fdr != nullptr)
        near_e = near_fdr;

    if (near_e == nullptr)
    {
        PhotoWrk180SetBit2(w180, 0);
        PhotoWrk180SetBit5(w180, 0);
    }
    else
    {
        if ((near_e->flags & 0x10000) == 0 && (near_e->flags2 & 0x100) == 0)
        {
            w1e8->FadeIn();
            on = 0;
        }
        else
        {
            w1e8->FadeOut();
            on = 1;
        }
        PhotoWrk180SetBit5(w180, on);
        PhotoWrk180SetBit2(w180, PhotoChargerIsReady(&photo->charger) != 0);
    }

    PhotoWrk180SetBit1(w180, special != nullptr && (special->flags & 0x80) != 0);
}

// The nearest acting enemy that can be seen (alpha; +0x4d0 bit 1: always
// outside the viewfinder) and is a filament target (not 0x400).
// FF3 0x00189720

static void EneSearchNearVisible(ENE_WRK **out, int *out2)
{
    ENE_WRK *best = nullptr;
    int      i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state != ENE_STATE_ACT)
            continue;
        if ((ene->flags2 & 2) == 0 || (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0)
        {
            if (ene->alpha == 0)
                continue;
        }
        if ((ene->flags2 & 0x400) == 0 && (best == nullptr || ene->dist < best->dist))
            best = ene;
    }
    *out = best;
    *out2 = 0;
}

// The filament (the camera's ghost sensor) by the view-distance rate to that
// enemy: the battle rate with player _348 == 2, else the auto rate.
// FF3 0x001897c0

static void EneFilamentCtrl(void)
{
    ENE_WRK      *ene = nullptr;
    int           n = 0;
    FILAMENT_WRK *w = GetPhotoModeWrk244();
    float         rate;

    EneSearchNearVisible(&ene, &n);
    if (ene == nullptr)
    {
        FilamentSetBattle(w, 0.0f);
        rate = 0.0f;
    }
    else if (ingame_wrk.plyr_wrk->_348 == 2)
    {
        FilamentSetAuto(w, 0.0f);
        FilamentSetBattle(w, PlyrViewDistRate2(10.0f, 200.0f, 180.0f, ene->tpos));
        return;
    }
    else
    {
        FilamentSetBattle(w, 0.0f);
        rate = PlyrViewDistRate2(10.0f, 200.0f, 180.0f, ene->tpos);
    }
    FilamentSetAuto(w, rate);
}

// The hp gauge follows the nearest acting ordinary enemy (one near the
// finder centre first); a new target snaps the gauge.
// FF3 0x001898c8

static void EneHpGaugeCtrl(void)
{
    ENE_WRK       *near_e = nullptr;
    ENE_WRK       *near_fdr = nullptr;
    PHOTO_WRK_4BC *w;
    int            i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state != ENE_STATE_ACT || (ene->flags2 & 0x100) != 0 || ene->type == 2)
            continue;
        if (near_e == nullptr || ene->dist < near_e->dist)
            near_e = ene;
        if ((ene->flags & 0x400) != 0 && (near_fdr == nullptr || ene->dist < near_fdr->dist))
            near_fdr = ene;
    }
    if (near_fdr != nullptr)
        near_e = near_fdr;

    if (near_e == nullptr)
    {
        ene_gauge_target = nullptr;
        return;
    }

    w = GetPhotoModeWrk4bc();
    if (near_e != ene_gauge_target)
    {
        ene_gauge_target = near_e;
        PhotoGage4bcSetNow(w, (float)near_e->hp / (float)near_e->hp_max);
    }
    PhotoGage4bcSet(w, (float)near_e->hp / (float)near_e->hp_max);
}

// FF3 0x00189a10 (was EffectThunderLightGetLightningFlg, a low FF2 match)

ENE_WRK *EneGetGaugeTarget(void)
{
    return ene_gauge_target;
}

// ──────────────────────────────────────────────────────────────────────
// flags 0x100: tpos is in view and the enemy is unoccluded (blink.result[0]) or +0x4d0 0x20
// is set, unless 0x100000. The anchor is tpos moved 8 towards the camera;
// EneBlinkCapture uses it after the draw.
// FF3 0x00189a20

static void EneBlinkPosSet(ENE_WRK *ene)
{
    sceVu0FVECTOR v;
    sceVu0FVECTOR rot;
    u_long        fl = ene->flags;

    ene->flags = fl & ~(u_long)0x100;
    if ((fl & 0x100000) == 0 && IsPosInView(ene->tpos)
        && (ene->blink.result[0] != 0 || (ene->flags2 & 0x20) != 0))
        ene->flags |= 0x100;

    ene->blink.result[0] = 0;
    ene->blink.num = 1;
    ene->blink_req = 1;
    EneCopyVector(ene->blink.p[0], ene->tpos);
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 8.0f;
    v[3] = 0.0f;
    UnitGetTrgtRot(ene->blink.p[0], gra3dcamGetPosition(), rot, 3);
    RotVectorZXY(v, v, rot);
    sceVu0AddVector(ene->blink.p[0], ene->blink.p[0], v);
}

// FF3 0x00189b38 (was EffectSprayParticleUpdate, a low FF2 match)

void EneBlinkCapture(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].blink_req != 0)
        {
            CheckPointDepth(&ene_wrk[i].blink, i * 10);
            ene_wrk[i].blink_req = 0;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// The damage reaction, the anime and the time step (0x189b90..0x18a280;
// the old link-order guess graphics/effect/effect_scr.c held them)
// ══════════════════════════════════════════════════════════════════════

// 1 when any acting enemy with hp is in a strong reaction (dmg_react 3..5);
// the photo HUD holds its pulse and drops the finder glow while it is.
// FF3 0x00189b90 (was CallFocus2, a wrong FF2 match)

int EneAnyDmgReactBig(void)
{
    int ret = 0;
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state == ENE_STATE_ACT && ene->hp != 0 && (u_short)(ene->dmg_react - 3) < 3)
            ret = 1;
    }
    return ret;
}

// FF3 0x00189be0

int EneIsDmgReactBig(ENE_WRK *ene)
{
    return (u_short)(ene->dmg_react - 3) < 3;
}

// The level windows shrink while hits chain: a plain reaction (<= 3) starts
// over at level 1, a shutter-chance one (4, 5) climbs a level.
// FF3 0x00189bf0

void EneLevelByDmgReact(ENE_WRK *ene)
{
    if (ene->dmg_react <= 3)
    {
        ene->level = 1;
        return;
    }
    if (ene->dmg_react < 6)
        ene->level++;
}

// A photo hit of the given shot kind lands: damage pending, then the
// reaction. None (0) while the lens rate is 0 (shots >= 0 only) or the photo
// mode is on. Shot 4 on an enemy without flags2 0x200 and no chain running
// (flags 0x2000000000 / 0x10000000000) opens a chain with reaction 3. In a
// chain a shutter-chance / fatal-frame hit (0x1800) gives 4 and, for shots
// 0 / 1, slows the enemy to 0.7 for 120 frames (100 in PAL); without one the
// chain closes with 5. 0x10000000000 (set elsewhere) asks for a chain start
// (3, the same slow-down for shots 0 / 1); 0x8000000000 or a plain 0x1800
// hit gives 2.
// FF3 0x00189c30

void EneDmgReactSet(ENE_WRK *ene, s_char shot)
{
    u_long fl;

    ene->flags |= 0x20000;
    if (shot >= 0 && GetLensWrk()->GetSelRate() == 0.0f)
    {
        ene->dmg_react = 0;
        return;
    }
    if (GetPhotoModeWrk()->GetMode() != 0)
    {
        ene->dmg_react = 0;
        return;
    }

    fl = ene->flags;
    if (shot == 4 && (ene->flags2 & 0x200) == 0 && (fl & (u_long)0x12000000000) == 0)
    {
        ene->dmg_react = 3;
        ene->flags = (fl | (u_long)0x2000000000) & ~(u_long)0x10000000000;
        return;
    }

    if ((fl & (u_long)0x8000000000) != 0)
    {
        ene->dmg_react = 2;
        return;
    }

    if ((fl & (u_long)0x2000000000) != 0)
    {
        if ((fl & 0x1800) == 0)
        {
            ene->dmg_react = 5;
            ene->flags = fl & ~((u_long)0x2000000000 | 0x800 | 0x1000);
            return;
        }
        if ((u_char)shot < 2)
        {
            EneSetStepReq(ene, (GetPALMode() != 0) ? 100 : 120, 0.7f);
            fl = ene->flags;
        }
        ene->dmg_react = 4;
        ene->flags = (fl | (u_long)0x2000000000) & ~(u_long)0x10000000000;
        return;
    }

    if ((fl & (u_long)0x10000000000) != 0)
    {
        if ((u_char)shot < 2)
        {
            EneSetStepReq(ene, (GetPALMode() != 0) ? 100 : 120, 0.7f);
            fl = ene->flags;
        }
        ene->dmg_react = 3;
        ene->flags = (fl | (u_long)0x2000000000) & ~(u_long)0x10000000000;
        return;
    }

    ene->dmg_react = ((fl & 0x1800) != 0) ? 2 : 0;
}

// Starts anime (anime, sub) on the enemy model: its motion script
// (EneMotScrSet), then GameAnimeModel::StartAnime(anime, start, skip, blend,
// time, 1); attr 0xff stands for 1. The same anime on the same sub only
// restarts for anime >= 2 or with force. Linked enemies (flags2 0x2000000)
// that act with hp start it too.
// FF3 0x00189e28

void EneAnimeSet(ENE_WRK *ene, u_char anime, u_char start, u_char sub, u_char time, u_char skip,
                 u_char force, u_char attr, u_char blend)
{
    int i;

    if (force == 0 && ene->_554 == anime && ene->_555 == sub && anime < 2)
        return;

    ene->flags &= ~(u_long)0x60000000;
    EneMotScrSet(ene, anime, sub);
    // FF3: through the model's vtable +0x40 (CharModel::StartAnime)
    static_cast<GameAnimeModel *>(ene->mdl)->StartAnime(anime, start, skip, blend, (float)time, 1.0f);
    ene->mot_attr = (attr == 0xff) ? 1 : attr;
    for (i = 0; i < 4; i++)
        ene->turn[i] = 0.0f;
    ene->_557 = 0;

    if ((ene->flags2 & 0x2000000) == 0)
        return;
    for (i = 0; i < 8; i++)
    {
        short link = ene->dat->link[i];
        int   no;

        if (link < 0)
            continue;
        no = SearchEneWrkNo(ene->type, link);
        if (no >= 0 && ene_wrk[no].state == ENE_STATE_ACT && ene_wrk[no].hp != 0)
            EneAnimeSet(&ene_wrk[no], anime, start, sub, time, skip, force, attr, blend);
    }
}

// The model's yaw callback (EnemyStart: CharModel::SetYawFunc): a one-frame
// turn, unless EneAnimeSet's _557 is set.
// FF3 0x0018a008

void EneYawTurnFunc(float rot, void *p)
{
    ENE_WRK *ene = (ENE_WRK *)p;

    if (ene == nullptr || ene->_557 != 0)
        return;
    ene->rot_add[1] = rot;
    RotLimitChk(&ene->rot_add[1]);
}

// Per frame (EnemyActOne): EneSetStepReq's frames run out, then its rate
// goes back to 1; the step is the product of the factors and the player's
// gauge rate.
// FF3 0x0018a040

void EneStepUpdate(ENE_WRK *ene)
{
    short t = ene->step_time;

    ene->_56c = 1.0f;
    if (t > 0)
        ene->step_time = t - 1;
    else if (t == 0)
        ene->step_req = 1.0f;
    ene->step = ene->step_req * ene->_56c * ene->mahi_rate * PlyrGaugeDmgRate(GetAlbumSaveDisp());
}

// flags 2: EneSetStepReq's slow-down ends; 4: the three paralyses end (with
// their effects) and their rate goes back to 1. The step is recomputed
// (EneDamageStart passes 1, a death 7).
// FF3 0x0018a0c8

void EneStepReset(ENE_WRK *ene, u_int flags)
{
    float rate = PlyrGaugeDmgRate(GetAlbumSaveDisp());

    if ((flags & 2) != 0)
    {
        ene->step_time = 0;
        ene->step_req = 1.0f;
    }
    if ((flags & 4) != 0)
    {
        EneMahi0End(ene, 1);
        EneMahi1End(ene, 1);
        EneMahi2End(ene, 1);
        ene->mahi_rate = 1.0f;
    }
    ene->step = ene->step_req * ene->_56c * ene->mahi_rate * rate;
}

// Per frame (EneRule): flags 0x40000000 mirrors the anime script's flag 1;
// once the anime has run out flags 0x20000000 is set and mot_kind counts the
// frames since.
// FF3 0x0018a188

void EneAnimeFlagUpdate(ENE_WRK *ene)
{
    GameAnimeModel *mdl = static_cast<GameAnimeModel *>(ene->mdl);

    if ((ene->flags & 0x40000000) != 0)
        ene->flags &= ~(u_long)0x40000000;
    if (mdl->GetAnimeFlag(1) != 0)
        ene->flags |= 0x40000000;
    if (mdl->IsAnimeEnd() == 0)
        ene->mot_kind = 0;
    else
    {
        ene->flags |= 0x20000000;
        ene->mot_kind++;
    }
}

// FF3 0x0018a220

int EneGetAnimeTime(ENE_WRK *ene)
{
    return (int)static_cast<GameAnimeModel *>(ene->mdl)->GetAnimeTime();
}

// FF3 0x0018a248

int EneGetAnimeLength(ENE_WRK *ene)
{
    return static_cast<GameAnimeModel *>(ene->mdl)->GetAnimeLength(ene->_554);
}

// FF3 0x0018a280 (__static_initialization_and_destruction_0, run by its
// _GLOBAL__I_ 0x18a318, the .ctors word 0x2e912c, with (1, 0xffff)): every
// slot's damage number and gauge start cleared. A static initializer does
// the same here. FF3 0x0018a2f8 is compiler output: the atexit thunk of the
// GPhaseCtrl singleton, GPhaseCtrlTerm(0x41e468); implicit for the C++ static.

static void EneMainStaticInit(int initialize, int priority)
{
    ENE_WRK *ene = ene_wrk;
    int      i;

    if (priority != 0xffff || initialize != 1)
        return;
    for (i = ENE_WRK_MAX - 1; i != -1; i--, ene++)
    {
        ene->dmg_disp.pulse.wait.val = 0;
        EneGaugeInit(&ene->gauge);
    }
}

// layout: host -- an empty object whose constructor stands in for the .ctors entry.
static struct EneMainStaticInitRun
{
    EneMainStaticInitRun() { EneMainStaticInit(1, 0xffff); }
} ene_main_static_init;
