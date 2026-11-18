// ingame/enemy/enemy.c -- enemy setup and release (FF3 0x1931e0..0x1985c0).
// FF2's enemy.c has the shape of SetCommonDat, EnemyStart, JibakuEnemyStart /
// AutoEnemyStart, EneRelease and EneReleaseReq; FF3 rewrote them around its
// own work (ingame/enemy/ene_main.h) and added the per-id flag tables, the
// enemy effects, the motion script and the enemy sound work. Reconstructed
// from FF3; FF2 gave names and shape only.
// The motion script's ROM tables are generated into enemy_mot_dat.inc
// (tools/reverse/enemotdat.py).

#include <math.h>                       // fabsf, sqrtf, tanf
#include <new>                          // placement new on FF3's C++ heap (operator_new)
#include <string.h>                     // memset

#include "ingame/enemy/enemy.h"
#include "common/cpp_heap.h"            // operator_delete
#include "common/reipan_stub.h"
#include "common/vu0math.h"             // RotLimitChk, RotVectorZXY, GetRandValIByF
#include "graphics/effect/effect.h"     // SetEffects, ResetEffects
#include "graphics/effect/effect_obj.h" // CallPartsDeform5_2, EffKind1DReq, EffKind1DCut
#include "ingame/enemy/enemy_act.h"     // EneActSet, EneBlinkSet
#include "graphics/graph3d/gra3d.h"     // gra3dcamGetPosition
#include "ingame/char_model.h"          // CharEnemyModel
#include "ingame/char_model_sub.h"     // CharDrawFind, CharDrawDelete, CharDrawSetEnemy
#include "ingame/enemy/fly_ctrl.h"      // EneFlyModelInitWait, EneFlyAnmctrlRelease, FlyAct
#include "ingame/event/prg/ev_chapter.h"  // EvGhostTblExist
#include "ingame/ingame.h"              // GameModel, CharEnemyModel
#include "ingame/ingame_wrk.h"          // ingame_wrk, title_start_flg, IngameWrkGetDifficulty
#include "ingame/map/map_bgm.h"         // map_bgmFadeOut / map_bgmFadeIn
#include "ingame/photo/finder_photo.h"  // GetPhotoModeWrk, PhotoModeTegata*, GetPhotoEneMark, GetLensWrk
#include "ingame/photo/fndr_parts.h"    // EneGaugeInit, DamageDispInit, EneMarkSlotInit, LensGetFunc
#include "ingame/plyr/player.h"         // PLYR_WRK, PlyrGetCurWrk, GetPlyrAreaNo, PLYR_FLG_800000
#include "ingame/plyr/plyr_param.h"     // PlyrGetEneWinRate
#include "ingame/plyr/plyr_sub.h"       // PlyrMdlFxReq, PlyrMdlFxInit, PlyrGetJointPos
#include "ingame/plyr/unit_ctl.h"       // UnitGetTrgtRot, GetTrgtRotY, GetMidPos
#include "ingame/enemy/random_ghost.h" // the random ghosts (type 1)
#include "ingame/sound/snd_ctrl.h"      // SndSe*, SND_3D_SET
#include "ingame/tattoo_event.h"        // TattooEvGetToggle, TattooEvEneRelease, TattooScrEffect*
#include "reipan/os/reipan_rand.h"      // PC: ReiPan_Rand
#include "sdk/libvu0.h"                 // sceVu0AddVector, sceVu0UnitMatrix, sceVu0RotMatrixY / Z, sceVu0ApplyMatrix
#include "ingame/subtitle/subtitle.h"   // SubTitleGetStreamFile, SubTitleReqEne
#include "system/eeiop/cd_file_dat.h"   // YKN_*_IREZUMI*
#include "system/eeiop/snd_buffer.h"    // SndBufSetPosition
#include "system/eeiop/stream_auto.h"   // StreamAuto*
#include "system/os/system.h"           // GetFrameTimeScale

// ──────────────────────────────────────────────────────────────────────
// Callees in runs that are not defined yet. Move each block to its real
// header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────

u_short ene_id_flg0[ENE_ID_FLG0_NUM];   // FF3 bss 0x3df7d0
u_short ene_id_flg1[ENE_ID_FLG1_NUM];   // FF3 bss 0x3dfae0
u_short ene_id_flg2[ENE_ID_FLG2_NUM];   // FF3 bss 0x3dfbc0
ENE_MOT_SCR_WRK ene_mot_scr_wrk[ENE_WRK_MAX];   // FF3 bss 0x3dfd90
ENE_SND_WRK ene_snd_wrk;                // FF3 bss 0x3dfe30

// One SetEffects request of an enemy (ENE_EFF_DAT.eff; EneEffStart).
struct ENE_EFF_PRM
{
    /* 0x00 */ u_char  no;
    /* 0x02 */ u_short v[7];            // [0], [1] go as floats
};
static_assert(sizeof(ENE_EFF_PRM) == 0x10, "ENE_EFF_PRM size");

// One deform effect of an enemy (ENE_EFF_DAT.deform; EneDeformInit).
struct ENE_DEFORM_DAT
{
    /* 0x00 */ u_char  no;              // CallPartsDeform5_2's effect number
    /* 0x01 */ u_char  b1;              // kept as ENE_EFF.deform_b1
    /* 0x04 */ float   prm[2];          // passed by address
};
static_assert(sizeof(ENE_DEFORM_DAT) == 0xc, "ENE_DEFORM_DAT size");
static_assert(offsetof(ENE_DEFORM_DAT, prm) == 4, "ENE_DEFORM_DAT.prm");

// The difficulty's rates (IngameWrkGetDifficulty): the bound enemies' hp
// (rodata 0x3553c8) and the motion script's window lengths (rodata 0x3554c0).
static const float ene_hp_rate[4]  = { 0.5f, 1.0f, 2.0f, 3.0f };
static const float ene_scr_rate[4] = { 2.0f, 1.0f, 0.699999988f, 0.467000008f };

// rodata 0x354d70 (EneAuraDatGet)
static ENE_AURA_DAT ene_aura_dat[ENE_AURA_DAT_NUM] =
{
    { 0, 1, 0, 16, {
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 1, 0, 1, 0, 128, 1.0f, 1.5f, 1.0f, 5.9f, 4.5f },
        { 0, 1, 0, 1, 0, 128, 1.0f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 3, 0, 16, {
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 1, 0, 1, 0, 128, 0.8f, 1.5f, 1.0f, 5.9f, 4.5f },
        { 0, 1, 0, 1, 0, 128, 0.6f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 3, 0, 16, {
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 2, 0, 1, 0, 128, 0.8f, 1.5f, 1.0f, 5.9f, 4.5f },
        { 0, 2, 0, 1, 0, 128, 0.6f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 3, 1, 16, {
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 2, 0, 0, 1, 0, 128, 0.75f, 0.1f, 0.1f, 0.9f, 0.5f },
        { 0, 2, 0, 1, 0, 128, 0.65f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 2, 1, 16, {
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 2, 0, 0, 1, 0, 128, 0.6f, 1.5f, 1.0f, 5.9f, 4.5f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 2, 1, 16, {
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 2, 0, 0, 1, 0, 128, 1.0f, 1.5f, 1.0f, 2.0f, 2.2f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 3.2f, 0.6f, 2.7f, 1.6f },
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
    { 1, 2, 1, 16, {
        { 0, 2, 0, 1, 0, 128, 1.0f, 0.5f, 0.2f, 0.6f, 0.4f },
        { 2, 0, 0, 1, 0, 128, 0.7f, 0.3f, 0.2f, 2.5f, 2.5f },
        { 0, 2, 0, 1, 0, 128, 0.9f, 0.7f, 0.3f, 2.7f, 1.6f },
        { 2, 0, 0, 1, 0, 128, 0.8f, 0.5f, 0.2f, 0.4f, 0.2f },
    } },
    { 1, 4, 1, 16, {
        { 2, 0, 0, 1, 0, 128, 1.0f, 0.5f, 0.2f, 0.6f, 0.4f },
        { 0, 2, 0, 1, 0, 128, 0.7f, 0.3f, 0.2f, 2.5f, 2.5f },
        { 0, 2, 0, 1, 0, 128, 0.9f, 0.7f, 0.3f, 2.7f, 1.6f },
        { 2, 0, 0, 1, 0, 128, 0.8f, 0.5f, 0.2f, 0.4f, 0.2f },
    } },
    { 1, 3, 0, 16, {
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 1, 0, 1, 0, 128, 0.8f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 1, 0, 1, 0, 128, 0.6f, 0.5f, 0.3f, 0.6f, 0.4f },
        { 0, 1, 0, 1, 0, 128, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    } },
};

// data 0x31b620
static ENE_EFF_DAT ene_eff_dat[7] =
{
    { 0, { 10, 10, 10 }, 1, 0, 1, { 0, 0 } },
    { 1, { 3, 3, 3 }, 0, 0, 0, { 0, 0 } },
    { 2, { 5, 5, 5 }, 7, 0, 0, { 4, 3 } },
    { 3, { 0, 0, 0 }, 7, 0, 0, { 3, 1 } },
    { 4, { 10, 10, 10 }, 5, 0, 0, { 1, 1 } },
    { 5, { 4, 4, 4 }, 6, 0, 0, { 1, 1 } },
    { 5, { 4, 4, 4 }, 7, 0, 0, { 1, 1 } },
};

// data 0x31b660
static const u_char ene_alpha_dat[13][3] =
{
    { 0x80, 0x00, 0x00 },
    { 0x5c, 0x24, 0x03 },
    { 0x23, 0x0f, 0x02 },
    { 0x3c, 0x24, 0x01 },
    { 0x46, 0x14, 0x02 },
    { 0x58, 0x28, 0x04 },
    { 0x36, 0x0f, 0x02 },
    { 0x76, 0x0a, 0x04 },
    { 0x3a, 0x32, 0x05 },
    { 0x4b, 0x19, 0x03 },
    { 0x30, 0x19, 0x05 },
    { 0x14, 0x00, 0x00 },
    { 0x00, 0x00, 0x00 },
};

// data 0x31b688
static const ENE_EFF_PRM ene_eff_prm[10] =
{
    { 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { 8, { 40, 80, 63, 58, 50, 0, 80 } },
    { 8, { 47, 80, 68, 48, 50, 0, 80 } },
    { 8, { 55, 80, 68, 87, 50, 0, 80 } },
    { 1, { 40, 10, 68, 88, 50, 0, 80 } },
    { 1, { 55, 16, 68, 88, 50, 0, 80 } },
    { 1, { 55, 31, 68, 88, 50, 0, 80 } },
    { 8, { 65, 16, 68, 88, 50, 60, 80 } },
    { 8, { 65, 16, 68, 88, 50, 30, 80 } },
    { 3, { 60, 28, 98, 45, 50, 30, 80 } },
};

// data 0x31b728 / 0x31b768
static ENE_DEFORM_DAT ene_deform_dat[2][5] =
{
    {
        { 0x00, 0, 0.0f, 0.0f },
        { 0x13, 42, 0.0f, 0.0f },
        { 0x13, 40, 0.0f, 0.0f },
        { 0x13, 64, 0.0f, 0.0f },
        { 0x13, 74, 0.0f, 0.0f },
    },
    {
        { 0x00, 0, 0.0f, 0.0f },
        { 0x18, 25, 1.20000005f, 0.200000003f },
        { 0x18, 35, 0.200000003f, 0.300000012f },
        { 0x18, 30, 1.5f, 0.400000006f },
        { 0x18, 40, 1.5f, 0.400000006f },
    },
};


// The motion scripts, the watch parameters and the route actions (data
// 0x31b7b0..0x321c70).
#include "ingame/enemy/enemy_mot_dat.inc"

// The enemy rows (data 0x301610 / 0x311600 / 0x315f80) and the route lists
// (data 0x321c70..0x324054), generated by tools/reverse/enedat.py.
#include "ingame/enemy/ene_dat.inc"

// ──────────────────────────────────────────────────────────────────────
// The per-id flag tables
// ──────────────────────────────────────────────────────────────────────

// Empty in retail: every per-id accessor calls it first with (type, id),
// a range check compiled out.
// FF3 0x00193308

static void EneIdCheck(u_char type, int id)
{
    (void)type;
    (void)id;
}

// FF3 inlines this switch in each accessor: the type's table, nullptr for
// other types.
static u_short *EneIdFlgTbl(u_char type)
{
    switch (type)
    {
    case 0: return ene_id_flg0;
    case 1: return ene_id_flg1;
    case 2: return ene_id_flg2;
    }
    return nullptr;
}

// The same switch over the data rows: types 0 and 1 have 0xa8-byte rows,
// type 2 0x60-byte ones; nullptr for other types.
static ENE_CMN *EneDatRow(u_char type, int no)
{
    switch (type)
    {
    case 0: return (ENE_CMN *)(ene_dat_tbl0 + no * sizeof(ENE_DAT));
    case 1: return (ENE_CMN *)(ene_dat_tbl1 + no * sizeof(ENE_DAT));
    case 2: return (ENE_CMN *)(ene_dat_tbl2 + no * sizeof(AENE_DAT));
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193310

int EneDatGetAdpcmNo(u_char type, int no)
{
    ENE_CMN *row;

    EneIdCheck(type, no);
    row = EneDatRow(type, no);
    if (row == nullptr)
        return -1;
    return row->adpcm_no;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001933c0

short EneDatGetMdlId(u_char type, int no)
{
    ENE_CMN *row;

    EneIdCheck(type, no);
    row = EneDatRow(type, no);
    if (row == nullptr)
        return -1;
    return row->id;
}

// ──────────────────────────────────────────────────────────────────────
// A new game (IngameNewGameReset) and EnemyMainInit clear all three tables.
// FF3 0x00193470

void EnemyWrkClear(void)
{
    memset(ene_id_flg0, 0, sizeof(ene_id_flg0));
    memset(ene_id_flg1, 0, sizeof(ene_id_flg1));
    memset(ene_id_flg2, 0, sizeof(ene_id_flg2));
}

// ──────────────────────────────────────────────────────────────────────
// SetCommonDat: only bits 0xf000 of the id's word outlast its start.
// FF3 0x001934c0

void EneIdFlgReset(u_char type, u_short id)
{
    u_short *tbl;

    EneIdCheck(type, id);
    tbl = EneIdFlgTbl(type);
    if (tbl == nullptr)
        return;
    tbl[id] &= 0xf000;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193568

u_short EneIdFlgCheck(u_char type, u_short id, u_short mask)
{
    u_short *tbl;

    EneIdCheck(type, id);
    tbl = EneIdFlgTbl(type);
    if (tbl == nullptr)
        return 0;
    return mask & tbl[id];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193610

void EneIdFlgSet(u_char type, u_short id, u_short mask)
{
    u_short *tbl;

    EneIdCheck(type, id);
    tbl = EneIdFlgTbl(type);
    if (tbl == nullptr)
        return;
    tbl[id] |= mask;
}

// ──────────────────────────────────────────────────────────────────────
// The save blocks (save_game_data, 31..33).
// FF3 0x001936c8

int EneIdFlgSaveGet0(void **addr)
{
    *addr = ene_id_flg0;
    return sizeof(ene_id_flg0);
}

// FF3 0x001936e0

int EneIdFlgSaveGet1(void **addr)
{
    *addr = ene_id_flg1;
    return sizeof(ene_id_flg1);
}

// FF3 0x001936f8

int EneIdFlgSaveGet2(void **addr)
{
    *addr = ene_id_flg2;
    return sizeof(ene_id_flg2);
}

// ──────────────────────────────────────────────────────────────────────
// The effects an enemy carries
// ──────────────────────────────────────────────────────────────────────

static void EneAuraInit(ENE_AURA_DAT *aura);
static void EneAlphaInit(ENE_WRK *ene);
static void EneEffStart(ENE_WRK *ene);
static void EneLightInit(ENE_WRK *ene);
static void EneLightRelease(ENE_WRK *ene);
static void EneDeformInit(ENE_WRK *ene);
static void EneDeformRelease(ENE_WRK *ene);
static int  EneIsKind1f(ENE_WRK *ene);
static void EneObjEffInit(ENE_WRK *ene);
static void EneObjEffRelease(ENE_WRK *ene);

// ──────────────────────────────────────────────────────────────────────
// At the start (SetCommonDat): the effect row (cmn->eff names one of
// ene_eff_dat, or 0xff: the 9 bytes are the row), then everything it
// selects. Kinds 0x1f / 0x20 keep the screen effect and switch to 9.
// FF3 0x00193710

void EneEffInit(ENE_WRK *ene)
{
    s_char no = (s_char)ene->cmn->eff[0];

    if (no == -1)
        ene->eff_dat = (ENE_EFF_DAT *)ene->cmn->eff;
    else
        ene->eff_dat = &ene_eff_dat[no];

    EneAuraInit(EneAuraDatGet(ene, 0xff));
    EneMahiModeDefault(ene);
    EneAlphaInit(ene);
    EneEffStart(ene);
    EneLightInit(ene);
    EneDeformInit(ene);
    EneObjEffInit(ene);

    // FF3 tests the kind here directly, not through EneIsKind1f.
    if (ene->type != 2 && (u_int)(ene->cmn->kind - 0x1f) < 2)
    {
        TattooScrEffectSave();
        TattooScrEffectAfter();
    }
}

// ──────────────────────────────────────────────────────────────────────
// At release (EneRelease): the light, the deforms and the object effects
// end; kinds 0x1f / 0x20 restore the screen effect.
// FF3 0x001937e8

void EneEffRelease(ENE_WRK *ene)
{
    EneLightRelease(ene);
    EneDeformRelease(ene);
    EneObjEffRelease(ene);

    if (ene->type != 2 && (u_int)(ene->cmn->kind - 0x1f) < 2)
        TattooScrEffectRestore();
}

// ──────────────────────────────────────────────────────────────────────
// Alpha bytes from the row's ene_alpha_dat entry and a random 0..359.
// FF3 0x00193850
// callcheck: rand (PC: ReiPan_Rand, the ROM's generator)

static void EneAlphaInit(ENE_WRK *ene)
{
    const u_char *a = ene_alpha_dat[ene->eff_dat->alpha];
    float         r;

    ene->_581 = ene->alpha_b = a[0];
    ene->_582 = a[1];
    ene->_583 = a[2];

    // PC: ReiPan_Rand is the ROM's 31-bit rand(); the host's is 15-bit.
    r = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
    ene->flags |= 0x800000;
    ene->_584 = (short)(int)(r * 360.0f + 0.0f);
}

// ──────────────────────────────────────────────────────────────────────
// SetEffects 2 with the row's ene_eff_prm entry (0: none); the effect it
// returns is not kept.
// FF3 0x00193900
// callcheck: fptodp (the float arguments' promotion to double for the varargs)

static void EneEffStart(ENE_WRK *ene)
{
    u_char             no = ene->eff_dat->eff;
    const ENE_EFF_PRM *p;

    if (no == 0)
        return;

    p = &ene_eff_prm[no];
    SetEffects(2, 4, p->no, (float)p->v[0], (float)p->v[1], p->v[2], p->v[3], p->v[4], p->v[5], p->v[6]);
}

// ──────────────────────────────────────────────────────────────────────
// The light's values, when the row has a colour and no light runs.
// FF3 0x001939a8

static void EneLightInit(ENE_WRK *ene)
{
    u_char   light = ene->eff_dat->light;
    ENE_EFF *eff   = &ene->eff;

    if (eff->light != nullptr || light == 0)
        return;

    eff->light_col   = light | 0x40404500;
    eff->light_pow   = 1.0f;
    eff->light_range = 10000.0f;
}

// FF3 resets only when no light runs, so it passes nullptr and never ends
// one (the deforms below test != 0); kept as it is. The null test of
// &ene->eff is FF3's (an inlined helper on the effect block).
// FF3 0x001939f8

static void EneLightRelease(ENE_WRK *ene)
{
    ENE_EFF *eff = &ene->eff;

    if (eff != nullptr && eff->light == nullptr)
    {
        ResetEffects(nullptr);
        eff->light = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The two deform effects at eff.pos, scaled by cmn->deform_scale (per cent;
// the second one at 90 per cent of it), their alphas cleared, rates 0,
// bases 1.
// FF3 0x00193a38

static void EneDeformInit(ENE_WRK *ene)
{
    ENE_EFF        *eff = &ene->eff;
    u_int           s1  = ene->cmn->deform_scale[1];
    u_char          d1  = ene->eff_dat->deform[1];
    u_int           s0  = ene->cmn->deform_scale[0];
    u_char          d0  = ene->eff_dat->deform[0];
    ENE_DEFORM_DAT *d;

    eff->deform_alpha[0] = 0.0f;
    eff->base[1]         = 1.0f;
    eff->rate[0]         = 0.0f;
    eff->base[0]         = 1.0f;
    eff->deform_alpha[1] = 0.0f;
    eff->rate[1]         = 0.0f;

    if (d0 != 0)
    {
        d = &ene_deform_dat[0][d0];
        eff->deform_b1[0] = (float)d->b1;
        eff->deform[0] = CallPartsDeform5_2((float)s0 / 100.0f, (float)s1 / 100.0f, d->no,
                                            eff->pos, &eff->deform_alpha[0], &d->prm[0], &d->prm[1]);
    }

    if (d1 != 0)
    {
        d = &ene_deform_dat[1][d1];
        eff->deform_b1[1] = (float)d->b1;
        eff->deform[1] = CallPartsDeform5_2((float)((s0 * 90) / 100) / 100.0f, (float)((s1 * 90) / 100) / 100.0f, d->no,
                                            eff->pos, &eff->deform_alpha[1], &d->prm[0], &d->prm[1]);
    }
}

// FF3 0x00193bd8

static void EneDeformRelease(ENE_WRK *ene)
{
    ENE_EFF *eff = &ene->eff;

    if (eff == nullptr)
        return;

    if (eff->deform[0] != nullptr)
    {
        ResetEffects(eff->deform[0]);
        eff->deform[0] = nullptr;
    }
    if (eff->deform[1] != nullptr)
    {
        ResetEffects(eff->deform[1]);
        eff->deform[1] = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193c38

static int EneIsKind1f(ENE_WRK *ene)
{
    return (u_int)(ene->cmn->kind - 0x1f) < 2;
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0x1f / 0x20: two object effects on joint 1.
// FF3 0x00193c50

static void EneObjEffInit(ENE_WRK *ene)
{
    if (EneIsKind1f(ene) == 0)
        return;

    ene->eff.obj[0] = EffKind1DReq(ene->jpos1, ene->rot, &ene->alpha_rate, 0, 1);
    ene->eff.obj[1] = EffKind1DReq(ene->jpos1, ene->rot, &ene->alpha_rate, 1, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0x1f / 0x20: both object effects get v at +0x38 (the job handlers
// pass 1 and 0; TattooEvCameraMain too).
// FF3 0x00193cd8

void EneObjEffSetRate(float v, ENE_WRK *ene)
{
    if (EneIsKind1f(ene) == 0)
        return;

    EffKind1DSetRate(v, ene->eff.obj[0]);
    EffKind1DSetRate(v, ene->eff.obj[1]);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193d38

static void EneObjEffRelease(ENE_WRK *ene)
{
    if (EneIsKind1f(ene) == 0)
        return;

    if (ene->eff.obj[0] != nullptr)
    {
        EffKind1DCut(ene->eff.obj[0], 0);
        ene->eff.obj[0] = nullptr;
    }
    if (ene->eff.obj[1] != nullptr)
    {
        EffKind1DCut(ene->eff.obj[1], 1);
        ene->eff.obj[1] = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The paralysis' look (EneMahi*Req through EneMahiModeSelect); FF2's
// version set the paralysis times instead.
// FF3 0x00193d98; FF2 SetEneMahiMode 0x001712b8

void SetEneMahiMode(ENE_WRK *ene, float a, float b, float c)
{
    ene->_4e0[3] = 0.0f;
    ene->_4e0[0] = a;
    ene->_4e0[1] = b;
    ene->_4e0[2] = c;
}

// ──────────────────────────────────────────────────────────────────────
// The look without a paralysis: the effect row's tenths.
// FF3 0x00193db0

void EneMahiModeDefault(ENE_WRK *ene)
{
    ENE_EFF_DAT *d = ene->eff_dat;

    ene->_4e0[0] = (float)d->mahi[0] / 10.0f;
    ene->_4e0[1] = (float)d->mahi[1] / 10.0f;
    ene->_4e0[3] = 0.0f;
    ene->_4e0[2] = (float)d->mahi[2] / 10.0f;
}

// ──────────────────────────────────────────────────────────────────────
// The deforms' anchor: just under 14 from joint 1 towards the camera.
// FF3 0x00193e20

void EneEffPosUpdate(ENE_WRK *ene)
{
    sceVu0FVECTOR v = { 0.0f, 0.0f, 13.999999f, 0.0f };    // FF3 0x415fffff
    sceVu0FVECTOR rot;

    UnitGetTrgtRot(ene->jpos1, gra3dcamGetPosition(), rot, 3);
    RotVectorZXY(v, v, rot);
    sceVu0AddVector(ene->eff.pos, ene->jpos1, v);
}

// ──────────────────────────────────────────────────────────────────────
// The deforms' alphas follow rate * base * _578; an enemy that only shows
// in the finder (flags2 2) has none while the camera is down. The light's
// strength follows deform 0's alpha, 0.025 up with alpha_a (at most 1) or
// 0.025 down (at least 0).
// FF3 0x00193eb0

void EneEffAlphaUpdate(ENE_WRK *ene)
{
    ENE_EFF *eff = &ene->eff;
    float    v;

    if (eff->deform[0] != nullptr)
    {
        eff->deform_alpha[0] = eff->rate[0] * eff->base[0] * ene->_578;
        if ((ene->flags2 & 2) != 0 && (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
            eff->deform_alpha[0] = 0.0f;
    }

    if (eff->deform[1] != nullptr)
    {
        eff->deform_alpha[1] = eff->rate[1] * eff->base[1] * ene->_578;
        if ((ene->flags2 & 2) != 0 && (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
            eff->deform_alpha[1] = 0.0f;
    }

    if (eff->light == nullptr)
        return;

    v = eff->deform_alpha[0] * 0.0078125f;
    eff->light_pow = v;
    if (ene->alpha_a != 0)
    {
        v = v + 0.025f;
        eff->light_pow = (v < 1.0f) ? v : 1.0f;
        return;
    }
    eff->light_pow = (0.025f < v) ? v - 0.025f : 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00193fc8

ENE_AURA_DAT *EneAuraDatGet(ENE_WRK *ene, u_char no)
{
    if (no == 0xff)
        no = ene->cmn->aura;
    return &ene_aura_dat[no];
}

// ──────────────────────────────────────────────────────────────────────
// Empty in retail.
// FF3 0x00194000

static void EneAuraInit(ENE_AURA_DAT *aura)
{
    (void)aura;
}

// ──────────────────────────────────────────────────────────────────────
// Setup and release
// ──────────────────────────────────────────────────────────────────────

static void EneSndStart(ENE_WRK *ene);
static void EneSndRelease(ENE_WRK *ene);
static void EneSeStopCheck(ENE_WRK *ene);
static void EneStreamReset(ENE_WRK *ene);
static void EneAlgTblSet(ENE_WRK *ene, s_char no);
static void EneLinkRelease(ENE_WRK *ene);
static void EneWrkRelease(ENE_WRK *ene);
static int  EneTegataIsReady(ENE_WRK *ene);
static void EneWrkInit(ENE_WRK *ene);
static void EneTegataLoad(ENE_WRK *ene);
static void EneTegataRelease(ENE_WRK *ene);
static int  EneWrkCreate(u_char type, short id, int *no);
static int  EneMdlCreate(ENE_WRK *ene);

// ──────────────────────────────────────────────────────────────────────
// A slot before its data is set (0x194798).
// FF3 0x00194008

static void EneWrkInit(ENE_WRK *ene)
{
    memset(ene, 0, sizeof(*ene));
    ene->stream_id = 0;
    ene->level = -1;
    ene->_4dd = 0;
}

// ──────────────────────────────────────────────────────────────────────
// An auto enemy (type 2) starts with 100 hp.
// FF3 0x00194048; FF2 AutoEnemyStart

static void AutoEnemyStart(ENE_WRK *ene)
{
    SetCommonDat(ene, ene->cmn, 100);
    EneSndStart(ene);
    EneMdlCtrlSet(0.0f, 0.0f, ene, ene->cmn->mdl_ctrl);
    EneActSet(ene, 1);
}

// ──────────────────────────────────────────────────────────────────────
// A bound enemy (types 0, 1): hp_max is its data's hp times the
// difficulty's rate, and it starts with hp_max * hp_mul.
// FF3 0x001940a0; FF2 JibakuEnemyStart

static void JibakuEnemyStart(ENE_WRK *ene)
{
    u_char diff = (u_char)IngameWrkGetDifficulty(title_start_flg.mode);
    int    hp_max;

    EneAlgTblSet(ene, ene->dat->alg_no);
    hp_max = (int)((float)ene->dat->hp * ene_hp_rate[diff]);
    ene->hp_max = hp_max;
    SetCommonDat(ene, ene->cmn, hp_max * ene->dat->hp_mul);
    EneSndStart(ene);
    EneActSet(ene, 1);
}

// ──────────────────────────────────────────────────────────────────────
// Everything an enemy starts with: hp, position and facing (a save-point
// ghost's from savepoint_top.c; else the data's, from the linked enemy
// with flags2 0x20000000, or 10 behind the player with 0x8000000), the
// player as its target, alphas, effects, paralysis timers, gauge, damage
// number, camera marker, and motion 0.
// FF3 0x00194140; FF2 SetCommonDat

void SetCommonDat(ENE_WRK *ene, ENE_CMN *cmn, int hp)
{
    PLYR_WRK     *plyr;
    sceVu0FVECTOR v;
    sceVu0FVECTOR r;
    float         y   = cmn->pos[1];
    float         rot = 0.0f;
    int           placed = 0;
    short         no;
    short         id;
    int           i;

    ene->hp   = hp;
    ene->_1d0 = 1.0f;

    if (ene->type == 1)
    {
        RandGhostSetPos(ene);
    }
    else
    {
        if (ene->id == 0x10a && ene->type == 0)
            y = ingame_wrk.plyr_wrk->pos[1];

        // At the data's offset from the linked enemy, turned by its rot.
        if ((ene->flags2 & 0x20000000) != 0 && ene->dat->link[0] != -1)
        {
            no = (short)SearchEneWrkNo(ene->type, ene->dat->link[0]);
            placed = (no != -1);
            if (placed)
            {
                v[0] = cmn->pos[0];
                v[1] = y;
                v[2] = cmn->pos[2];
                v[3] = 0.0f;
                RotVectorZXY(v, v, ene_wrk[no].rot);
                sceVu0AddVector(ene->pos, ene_wrk[no].pos, v);
            }
        }

        // 10 behind the player.
        if ((ene->flags2 & 0x8000000) != 0)
        {
            plyr = ingame_wrk.plyr_wrk;
            v[0] = 0.0f;
            v[1] = 0.0f;
            v[2] = 10.0f;
            v[3] = 0.0f;
            r[0] = 0.0f;
            r[1] = plyr->rot[1] + 3.14159274f;
            r[2] = 0.0f;
            r[3] = 0.0f;
            RotLimitChk(&r[1]);
            RotVectorZXY(v, v, r);
            sceVu0AddVector(v, ingame_wrk.plyr_wrk->pos, v);
            for (i = 0; i < 4; i++)
                ene->pos[i] = v[i];
            placed = 1;
        }

        if (!placed)
        {
            ene->pos[0] = cmn->pos[0];
            ene->pos[1] = y;
            ene->pos[2] = cmn->pos[2];
            ene->pos[3] = 0.0f;
        }
    }

    if (ene->type == 1)
    {
        RandGhostSetRot(ene);
    }
    else
    {
        // Towards the linked enemy; else the data's facing (1000: the player).
        if ((ene->flags2 & 0x20000000) != 0 && (id = ene->dat->link[0]) != -1)
        {
            no = (short)SearchEneWrkNo(ene->type, id);
            if (no != -1)
            {
                UnitGetTrgtRot(ene->pos, ene_wrk[no].pos, v, 3);
                rot = v[1];
            }
        }
        if (rot == 0.0f)
        {
            if (cmn->rot == 1000 || (ene->flags2 & 0x8000000) != 0)
            {
                rot = GetTrgtRotY(ene->pos, ingame_wrk.plyr_wrk->pos);
            }
            else
            {
                rot = (float)cmn->rot * 3.14159274f / 180.0f;
                RotLimitChk(&rot);
            }
        }
        ene->rot[1] = rot;
        ene->rot[0] = 0.0f;
        ene->rot[2] = 0.0f;
        ene->rot[3] = 0.0f;
    }

    EneBlinkSet(ene);
    for (i = 0; i < 4; i++)
        ene->_2e0[i] = 0.0f;
    ene->act_req = 0;
    ene->alg.stack_p = ene->alg.stack_b;
    ene->target = ingame_wrk.plyr_wrk;
    ene->alpha_a = 0;
    ene->alpha_nrm = ((ene->flags2 & 2) != 0) ? 0 : 0x80;
    ene->_578 = 1.0f;
    ene->alpha_fdr = 0x80;
    ene->alpha_rate = 1.0f;
    ene->_589 = 0xff;
    EneEffInit(ene);

    for (i = 0; i < 3; i++)
    {
        ene->mahi_time[i] = 0;
        ene->mahi_start[i] = 0;
    }
    ene->_528 = 0;
    EneStepReset(ene, 7);
    ene->_6a8 = 0.0f;
    ene->_554 = 0xff;
    ene->_6ac = 0.0f;
    ene->tpos = ene->mpos;
    ene->_548 = 1.0f;
    EneGaugeInit(&ene->gauge);
    DamageDispInit(&ene->dmg_disp);
    EneMdlCtrlSet(0.0f, 0.0f, ene, 1);
    EneMarkSlotInit(GetPhotoEneMark(), ene);
    EneIdFlgReset(ene->type, (u_short)ene->id);
    EneMotScrClear(ene);
    EneAnimeSet(ene, 0, 0, 0, 0, 0, 0, 0xff, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The random ghosts' candle-out enemy (random_ghost.c) and the enemy script's op 0x18c0f0:
// the enemy of type and id gets a slot and its data unless it has one, is
// marked to start once ready (_4dd), and a ready one starts now. 0: no
// player, or no slot or model for it.
// FF3 0x00194598

int EneLoadStart(u_char type, short id)
{
    int no;

    EneIdCheck(type, id);
    if (PlyrGetCurWrk() == nullptr)
        return 0;

    no = SearchEneWrkNo(type, id);
    if (no < 0 && EneWrkCreate(type, id, &no) == 0)
        return 0;

    ene_wrk[no]._4dd = 1;
    if (ene_wrk[no].state == ENE_STATE_READY)
        EnemyStart(no);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Events (ev_macro.c), the finder and 0x24e478: the same without the start.
// FF3 0x00194650

int EneLoadReq(u_char type, short id)
{
    int no;

    if (PlyrGetCurWrk() == nullptr)
        return 0;

    no = SearchEneWrkNo(type, id);
    if (no < 0 && EneWrkCreate(type, id, &no) == 0)
        return 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Starts slot no: not ready yet, it starts once it is (state 1 and _4dd,
// EnemyWrkOne); else, with a player and a model, the start by type, the
// model's motion callback, and state 3.
// FF3 0x001946c8; FF2 EnemyStart (inline there)

void EnemyStart(int no)
{
    ENE_WRK *ene = &ene_wrk[no];

    if (enemyIsReadyData(ene) == 0)
    {
        ene->state = ENE_STATE_LOADING;
        ene->_4dd = 1;
        return;
    }

    if (PlyrGetCurWrk() == nullptr || ene == nullptr || ene->mdl == nullptr)
        return;

    if (ene->type < 2)
        JibakuEnemyStart(ene);
    else if (ene->type == 2)
        AutoEnemyStart(ene);

    static_cast<CharModel *>(ene->mdl)->SetYawFunc(EneYawTurnFunc, ene);
    ene->state = ENE_STATE_ACT;
}

// ──────────────────────────────────────────────────────────────────────
// A free slot (*no) for the enemy of type and id: its data row (types 0 / 1
// as dat, 2 as adat), flags2 from the row, and for model ids below 0x2f its
// model; state 2 when everything is loaded already, else 1. 0: no free
// slot, no model (*no -1) or a model id from 0x2f on (the slot keeps the
// row, state 0).
// FF3 0x00194798

static int EneWrkCreate(u_char type, short id, int *no)
{
    void    *dat = GetEneDatP(type, id);
    ENE_WRK *ene;
    int      free = GetEneWrkBuffer();

    if (free < 0)
        return 0;

    *no = free;
    ene = &ene_wrk[free];
    EneWrkInit(ene);
    if (type < 2)
    {
        ene->cmn  = (ENE_CMN *)dat;
        ene->dat  = (ENE_DAT *)dat;
        ene->adat = nullptr;
    }
    else if (type == 2)
    {
        ene->cmn  = (ENE_CMN *)dat;
        ene->dat  = nullptr;
        ene->adat = (AENE_DAT *)dat;
    }

    ene->id      = id;
    ene->flags2  = ene->cmn->flags2;
    ene->type    = type;
    ene->alg.idx = (u_char)free;

    if (ene->cmn->id >= 0x2f)
        return 0;
    if (EneMdlCreate(ene) == 0)
    {
        *no = -1;
        return 0;
    }
    ene->state = (enemyIsReadyData(ene) != 0) ? ENE_STATE_READY : ENE_STATE_LOADING;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Its model: a CharEnemyModel (type 1, and the type-0 enemy 0x10a, clear
// the model's hook words), the fade envelope, then the model's files from
// its data row. Loading: the tegata and the fly models start; failed: the
// slot is freed.
// FF3 0x001948b8

// callcheck: PlyrMdlFxInit (PLYR_MDL_FX's constructor on operator_new memory)
static int EneMdlCreate(ENE_WRK *ene)
{
    ENE_CMN *cmn = ene->cmn;
    int      ok;

    ene->mdl = new (operator_new(sizeof(CharEnemyModel))) CharEnemyModel(ene);   // FF3: operator new(0x48)
    if (ene->type == 1)
    {
        ene->mdl->SetFreeHook(0, nullptr);
        ene->mdl->SetFreeHook(1, nullptr);
    }
    if (ene->type == 0 && ene->id == 0x10a)
        ene->mdl->SetFreeHook(1, nullptr);

    ene->fx = new (operator_new(sizeof(PLYR_MDL_FX))) PLYR_MDL_FX;

    ok = static_cast<CharModel *>(ene->mdl)->Load(cmn->id, cmn->mdl_type, cmn->mdl_file, cmn->mot_file, cmn->shp_file);
    if (ok != 0)
    {
        EneTegataLoad(ene);
        FlyAct(ene);
    }
    else
    {
        EneWrkRelease(ene);
    }
    return ok != 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the model (vtable +0x10), the tegata and the fly models are ready;
// the map hit entry is added then (not for model id 0x1e).
// FF3 0x001949c8; FF2 enemyIsReadyData

int enemyIsReadyData(ENE_WRK *ene)
{
    int ready;

    ready = ene->mdl->IsLoaded();     // FF3: through vtable +0x10 (CharEnemyModel's is 0x1d35b8)
    if (EneTegataIsReady(ene) == 0)
        ready = 0;
    if (EneFlyModelInitWait(ene) == 0)
        ready = 0;

    if (ready == 1 && ene->cmn->id != 0x1e)
        CharDrawSetEnemy(ene);
    return ready;
}

// ──────────────────────────────────────────────────────────────────────
// The tegata (model id 0x1c, bound enemies) is drawn by the photo mode work.
// FF3 0x00194a50

static void EneTegataLoad(ENE_WRK *ene)
{
    if (ene->cmn->id == 0x1c && ene->type < 2)
        GetPhotoModeWrk()->TegataLoad();
}

// FF3 0x00194aa0

static int EneTegataIsReady(ENE_WRK *ene)
{
    if (ene->cmn->id == 0x1c && ene->type < 2)
        return GetPhotoModeWrk()->TegataIsReady();
    return 1;
}

// FF3 0x00194af0

static void EneTegataRelease(ENE_WRK *ene)
{
    if (ene->cmn->id == 0x1c && ene->type < 2)
        GetPhotoModeWrk()->TegataRelease();
}

// ──────────────────────────────────────────────────────────────────────
// IngameEnd, PlyrDeathCtrl and 0x1a08e0: every slot in use.
// FF3 0x00194b40

void EnemyReleaseAll(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state != ENE_STATE_FREE)
            EneRelease(i, 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The model, its fade envelope, the tegata and the map hit entry go, and
// the slot is free.
// FF3 0x00194ba0

static void EneWrkRelease(ENE_WRK *ene)
{
    if (ene == nullptr)
        return;

    if (ene->mdl != nullptr)
    {
        { ene->mdl->~GameModel(); operator_delete(ene->mdl); }         // FF3: delete through vtable +0x0c
        ene->mdl = nullptr;
    }
    if (ene->fx != nullptr)
    {
        operator_delete(ene->fx);
        ene->fx = nullptr;
    }
    EneTegataRelease(ene);
    if (ene->cmn->id != 0x1e && CharDrawFind(ene) != nullptr)
        CharDrawDelete(ene);

    memset(ene, 0, sizeof(*ene));
    ene->state = ENE_STATE_FREE;
}

// ──────────────────────────────────────────────────────────────────────
// Releases slot no. Bound enemies: the save point's hold, flag 8 unless 4
// is set, the linked enemies and the sounds; auto enemies drop their
// stream. Unforced, an enemy the area's saved positions list keeps only
// loses its state (flags .. hp cleared) and waits in state 2.
// FF3 0x00194c48; FF2 EneRelease (which had no force flag)

void EneRelease(int no, int force)
{
    ENE_WRK *ene = &ene_wrk[no];

    EneFlyAnmctrlRelease(ene);
    EneEffRelease(ene);

    if (ene->type == 1)
        RandGhostBgmStop();
    if (ene->type < 2)
    {
        RandGhostEneRelease(ene);
        if (EneIdFlgCheck(ene->type, (u_short)ene->id, 4) == 0)
            EneIdFlgSet(ene->type, (u_short)ene->id, 8);
        EneLinkRelease(ene);
        EneSndRelease(ene);
    }
    else if (ene->type == 2)
    {
        EneStreamReset(ene);
    }

    if (force == 0 && EvGhostTblExist(GetPlyrAreaNo(), ene->type, ene->id) != 0)
    {
        // FF3 clears EE 0x180..0x1e8: flags up to (not with) dat.
        memset(&ene->flags, 0, offsetof(ENE_WRK, dat) - offsetof(ENE_WRK, flags));
        ene->state = ENE_STATE_READY;
        return;
    }

    EneSeStopCheck(ene);
    TattooEvEneRelease(ene);
    EneWrkRelease(ene);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00194d88; FF2 EneReleaseReq (FF3 drops its printfs)

void EneReleaseReq(u_char type, short id)
{
    int no = SearchEneWrkNo(type, id);

    if (no >= 0)
        EneRelease(no, 1);
}

// ──────────────────────────────────────────────────────────────────────
// flags2 0x200000: its linked enemies go with it (those loading or ready,
// and the acting ones of kind != 4).
// FF3 0x00194dc0

static void EneLinkRelease(ENE_WRK *ene)
{
    int   i;
    int   no;
    short id;
    int   state;

    if ((ene->flags2 & 0x200000) == 0)
        return;

    for (i = 0; i < 8; i++)
    {
        id = ene->dat->link[i];
        if (id < 0)
            continue;
        no = SearchEneWrkNo(ene->type, id);
        if (no < 0)
            continue;
        state = ene_wrk[no].state;
        if ((u_int)(state - 1) < 2 || (state == ENE_STATE_ACT && ene_wrk[no].act_no != 4))
            EneRelease(no, 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The crawl's end or cancel releases the acting, live, bound enemies of
// kind 0x5f.
// FF3 0x00194ea0

void EneReleaseKind5f(void)
{
    ENE_WRK *ene;
    int      i;

    for (i = 0, ene = ene_wrk; i < ENE_WRK_MAX; i++, ene++)
    {
        if (ene->state == ENE_STATE_ACT && ene->hp != 0 && ene->type != 2 && ene->cmn->kind == 0x5f)
            EneReleaseReq(ene->type, ene->id);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Switches the model's +0x38 part: off for mode 0, on for 1..4 (5 and up
// only store the values). a / b come from the motion script in radians.
// FF3 0x00194f48

void EneMdlCtrlSet(float a, float b, ENE_WRK *ene, u_char mode)
{
    CharEnemyModel *mdl;

    if (ene == nullptr || ene->mdl == nullptr)
        return;
    mdl = static_cast<CharEnemyModel *>(ene->mdl);
    if (mdl->ctrl == nullptr)
        return;

    ene->mdl_ctrl_b = b;
    ene->mdl_ctrl_a = a;
    ene->mdl_ctrl   = mode;
    if (mode == 0)
        mdl->ctrl->off = 1;
    else if (mode < 5)
        mdl->ctrl->off = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Where the model looks this frame (its CharWatchControl): the script's
// watch parameters (else the defaults of the mdl_ctrl mode). Modes 1 / 3 /
// 4 look at the player's head (the camera 1.5 up while the camera is up or
// the player crawls), mode 2 at 20 ahead of the middle of its joints 0x10
// and 4, turned by mdl_ctrl_a / _b; other modes keep the row's point. The
// first three limits become 10000 during paralysis 0 and 1 (unless 10000)
// while it is invisible.
// FF3 0x00194fa8

void EneMdlWatchUpdate(ENE_WRK *ene)
{
    CharEnemyModel *mdl;
    CHAR_WATCH_PRM  prm;
    CHAR_WATCH_PRM *dat;
    sceVu0FVECTOR   tgt;
    sceVu0FVECTOR   in;
    sceVu0FVECTOR   v;
    sceVu0FVECTOR   a;
    sceVu0FVECTOR   b;
    sceVu0FMATRIX   m;
    float          *cam;
    int             idx;
    int             i;

    if (ene == nullptr || ene->mdl == nullptr)
        return;
    mdl = static_cast<CharEnemyModel *>(ene->mdl);
    if (mdl->ctrl == nullptr)
        return;

    dat = ene->watch;
    if (dat == nullptr)
    {
        idx = 1;
        if (ene->mdl_ctrl != 1)
        {
            if (ene->mdl_ctrl == 4)
                idx = 2;
            if (ene->mdl_ctrl == 3)
                idx = 0;
        }
        dat = &ene_watch_dflt[idx];
    }
    memcpy(&prm, dat, sizeof(prm));

    if ((ene->flags & 1) != 0)
    {
        for (i = 0; i < 3; i++)
        {
            if (prm.prm[i] != 10000.0f)
                prm.prm[i] = 10000.0f;
        }
    }
    else if (ene->alpha == 0)
    {
        for (i = 0; i < 3; i++)
        {
            if (prm.prm[i] != 10000.0f)
                prm.prm[i] = 1.0f;
        }
    }

    switch (ene->mdl_ctrl)
    {
    case 1:
    case 3:
    case 4:
        if ((ingame_wrk.plyr_wrk->flags & (PLYR_FLG_800000 | PLYR_FLG_8000000)) == 0)
        {
            in[0] = 0.2f;
            in[1] = 1.0f;
            in[2] = 0.0f;
            in[3] = 1.0f;
            PlyrGetJointPos(2, tgt, in);
        }
        else
        {
            in[0] = 0.0f;
            in[1] = 1.5f;
            in[2] = 0.0f;
            in[3] = 0.0f;
            cam = gra3dcamGetPosition();
            sceVu0AddVector(tgt, cam, in);
        }
        break;

    case 2:
        v[0] = 20.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
        v[3] = 1.0f;
        sceVu0UnitMatrix(m);
        sceVu0RotMatrixZ(m, m, ene->mdl_ctrl_a);
        sceVu0RotMatrixY(m, m, ene->mdl_ctrl_b);
        sceVu0ApplyMatrix(in, m, v);
        EneGetJointPos(ene, 0x10, a, in);
        EneGetJointPos(ene, 4, b, in);
        GetMidPos(tgt, a, b);
        break;

    default:
        // the row's own point
        mdl->ctrl->Update(&prm);         // FF3: through the control's vtable +0x08
        return;
    }

    for (i = 0; i < 4; i++)
        prm.pos[i] = tgt[i];
    mdl->ctrl->Update(&prm);             // FF3: through the control's vtable +0x08
}

// ──────────────────────────────────────────────────────────────────────
// Not FF2's SetEneMahiClear (a low-confidence match): no motion script, its
// frame row reset.
// FF3 0x001952b0

void EneMotScrClear(ENE_WRK *ene)
{
    ENE_MOT_SCR_WRK *w;

    ene->mot_scr = nullptr;
    w = &ene_mot_scr_wrk[ene->alg.idx];
    w->time_old  = -1;
    w->anime     = -1;
    w->frame_old = -1;
    w->frame     = -1;
    w->time      = -1;
}

static void EneMotScrFlagsClear(ENE_WRK *ene);

// ──────────────────────────────────────────────────────────────────────
// EneAnimeSet, as an anime starts: the script of (anime, sub) from the
// model's row of the ROM tables, its frame row reset (the open windows
// end), and the model ctrl mode and angles it asks for when the model has
// watch parameters. 0: no script (mot_scr nullptr).
// FF3 0x001952e8

int EneMotScrSet(ENE_WRK *ene, u_char anime, u_char sub)
{
    ENE_MOT_SCR_WRK *w;
    ENE_MOT_SCR   ***tbl;
    ENE_MOT_SCR    **row;
    ENE_MOT_SCR     *scr;
    short            type;

    ene->mot_scr = nullptr;
    ene->_554 = anime;
    ene->_555 = sub;
    ene->_6a8 = 0.0f;
    ene->_6ac = 0.0f;

    type = ene->cmn->mdl_type;
    if (type >= 0x20 || (tbl = ene_mot_scr_tbl[type]) == nullptr)
        return 0;

    w = &ene_mot_scr_wrk[ene->alg.idx];
    w->time_old  = -1;
    w->anime     = -1;
    w->frame_old = -1;
    w->frame     = -1;
    w->time      = -1;
    // FF3 compares the anime byte with -1 here, which never holds: this
    // always runs, also for anime 0xff.
    EneMotScrFlagsClear(ene);
    w->anime = ene->_554;

    row = tbl[ene->_554];
    if (row == nullptr)
        return 0;
    scr = row[sub];
    if (scr == nullptr)
        return 0;

    if (ene_watch_tbl[type] != nullptr)
    {
        ene->watch = ene_watch_tbl[type];
        EneMdlCtrlSet((float)scr->mdl_ctrl_a * 3.14159274f / 180.0f,
                      (float)scr->mdl_ctrl_b * 3.14159274f / 180.0f, ene, scr->mdl_ctrl);
    }
    ene->mot_scr = scr;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The motion script
// ──────────────────────────────────────────────────────────────────────

void EneSePlayRand(ENE_WRK *ene, int se0, int p0, int se1, int p1, int se2, int p2, int se3, int p3);

// ──────────────────────────────────────────────────────────────────────
// Sets or clears flags bits by op (the script's paired ops): 4 / 5 a
// shutter chance (0x800), 8 / 9 a fatal frame (0x1000; set only with lens
// function 1), 0xb / 0xc 0x10000, 0xe / 0xf 1 << 32, 0x11 / 0x12 2 << 32,
// 0x1b / 0x1c 0x20000000000.
// FF3 0x00195468

static void EneMotScrFlag(ENE_WRK *ene, int op)
{
    switch (op)
    {
    case 4:    ene->flags |= 0x800;                                break;
    case 5:    ene->flags &= ~(u_long)0x800;                       break;
    case 8:
        if (GetLensWrk()->GetFunc(LENS_FUNC_274) == 0)
            return;
        ene->flags |= 0x1000;
        break;
    case 9:    ene->flags &= ~(u_long)0x1000;                      break;
    case 0xb:  ene->flags |= 0x10000;                              break;
    case 0xc:  ene->flags &= ~(u_long)0x10000;                     break;
    case 0xe:  ene->flags |= 0x100000000ULL;                       break;
    case 0xf:  ene->flags &= ~(u_long)0x100000000ULL;              break;
    case 0x11: ene->flags |= 0x200000000ULL;                       break;
    case 0x12: ene->flags &= ~(u_long)0x200000000ULL;              break;
    case 0x1b: ene->flags |= 0x20000000000ULL;                     break;
    case 0x1c: ene->flags &= ~(u_long)0x20000000000ULL;            break;
    }
}

// Where a window closes: start plus its length scaled by the player's and
// the difficulty's rates (FF3 inlines this in each op).
static int EneMotScrEnd(const ENE_MOT_CMD *cmd, float diff_rate)
{
    float len = (float)(cmd->end - cmd->start);

    len = len * PlyrGetEneWinRate();
    len = len * diff_rate;
    return (int)len + cmd->start;
}

// FF3's test of a window edge: v was passed between last frame and this one.
static int EneMotScrCross(int v, int old, int cur)
{
    return v <= cur && old < v;
}

// The op on every linked enemy (dat->link), not on ene itself.
static void EneMotScrFlagLinks(ENE_WRK *ene, int op, int op2)
{
    int   i;
    int   no;
    short id;

    for (i = 0; i < 8; i++)
    {
        id = ene->dat->link[i];
        if (id < 0)
            continue;
        no = SearchEneWrkNo(ene->type, id);
        if (no < 0)
            continue;
        EneMotScrFlag(&ene_wrk[no], op);
        if (op2 != 0)
            EneMotScrFlag(&ene_wrk[no], op2);
    }
}

// ──────────────────────────────────────────────────────────────────────
// One command. Most act once, on the frame the anime passes start; the
// window ops also close at end (1 / 2 / 3 / 6 / 7: scaled by the player's
// and the difficulty's rates; 0xa / 0xd / 0x10 / 0x1a: as given), and the
// ones closing a flag may set a second one (arg[0]). Op 2 counts frame
// changes instead of frames; 3 and 7 act on the linked enemies.
// FF3 0x00195588
// callcheck: PlyrGetEneWinRate, SearchEneWrkNo (FF3 repeats the window end and the link loop in every op; here in EneMotScrEnd / EneMotScrFlagLinks)

static void EneMotScrOp(ENE_WRK *ene, ENE_MOT_CMD *cmd, ENE_MOT_SCR_WRK *w)
{
    int   start = cmd->start;
    int   end   = cmd->end;
    float diff_rate = ene_scr_rate[IngameWrkGetDifficulty(title_start_flg.mode)];
    int   e;

    if (cmd->op > 0x1a)
        return;

    switch (cmd->op)
    {
    case 1:     // a shutter chance over the window
    case 6:     // a fatal frame
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMotScrFlag(ene, cmd->op == 1 ? 4 : 8);
        e = EneMotScrEnd(cmd, diff_rate);
        if (EneMotScrCross(e, w->frame_old, w->frame))
        {
            EneMotScrFlag(ene, cmd->op == 1 ? 5 : 9);
            if (cmd->arg[0] != 0)
                EneMotScrFlag(ene, cmd->arg[0]);
        }
        return;

    case 2:     // a shutter chance over frame changes
        if (EneMotScrCross(start, w->time_old, w->time))
            EneMotScrFlag(ene, 4);
        e = EneMotScrEnd(cmd, diff_rate);
        if (EneMotScrCross(e, w->time_old, w->time))
        {
            EneMotScrFlag(ene, 5);
            if (cmd->arg[0] != 0)
                EneMotScrFlag(ene, cmd->arg[0]);
        }
        return;

    case 3:     // 1 / 6 on the linked enemies
    case 7:
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMotScrFlagLinks(ene, cmd->op == 3 ? 4 : 8, 0);
        e = EneMotScrEnd(cmd, diff_rate);
        if (EneMotScrCross(e, w->frame_old, w->frame))
            EneMotScrFlagLinks(ene, cmd->op == 3 ? 5 : 9, cmd->arg[0]);
        return;

    case 4:     // single flag ops at start
    case 5:
    case 8:
    case 9:
    case 0xb:
    case 0xc:
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMotScrFlag(ene, cmd->op);
        return;

    case 0xa:   // 0x10000 over the window
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMotScrFlag(ene, 0xb);
        if (EneMotScrCross(end, w->frame_old, w->frame))
            EneMotScrFlag(ene, 0xc);
        return;

    case 0xd:   // 1 << 32 or 2 << 32 over the window, with _6a8 / _6ac and the hit damage
    case 0x10:
        if (EneMotScrCross(start, w->frame_old, w->frame))
        {
            EneMotScrFlag(ene, cmd->op == 0xd ? 0xe : 0x11);
            ene->_6a8 = (float)cmd->arg[0] * 0.2f;
            ene->_6ac = (float)cmd->arg[1] * 3.14159274f / 180.0f;
            ene->hit_dmg = ene->dat->atk_dmg[cmd->arg[2]];
        }
        if (EneMotScrCross(end, w->frame_old, w->frame))
        {
            EneMotScrFlag(ene, cmd->op == 0xd ? 0xf : 0x12);
            ene->_6ac = 0.0f;
            ene->_6a8 = 0.0f;
        }
        return;

    case 0x13:  // flags2 bits on
        if (EneMotScrCross(start, w->frame_old, w->frame))
            ene->flags2 |= (u_int)(int)cmd->arg[0];
        return;

    case 0x14:  // ... and off
        if (EneMotScrCross(start, w->frame_old, w->frame))
            ene->flags2 &= ~(u_int)(int)cmd->arg[0];
        return;

    case 0x15:  // the model's +0x38 part, angles in degrees
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMdlCtrlSet((float)cmd->arg[1] * 3.14159274f / 180.0f, (float)cmd->arg[2] * 3.14159274f / 180.0f,
                          ene, (u_char)cmd->arg[0]);
        return;

    case 0x16:  // a turn
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneTurnReq(ene, (u_char)cmd->arg[0]);
        return;

    case 0x17:  // the model's fade envelope, while the camera is up
        if (EneMotScrCross(start, w->frame_old, w->frame) && (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0)
            ene->fx->Req(cmd->arg[0], cmd->arg[1], cmd->arg[2], 0x80, 1);
        return;

    case 0x18:  // an SE
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneSePlay(ene, (u_int)(int)cmd->arg[0]);
        return;

    case 0x19:  // one of three SEs by per cent
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneSePlayRand(ene, cmd->arg[0], cmd->arg[1], cmd->arg[2], cmd->arg[3], cmd->arg[4], cmd->arg[5], 0, 0);
        return;

    case 0x1a:  // 0x20000000000 over the window
        if (EneMotScrCross(start, w->frame_old, w->frame))
            EneMotScrFlag(ene, 0x1b);
        if (EneMotScrCross(end, w->frame_old, w->frame))
            EneMotScrFlag(ene, 0x1c);
        return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Empty in retail; called after each command with the same arguments.
// FF3 0x00196030

static void EneMotScrOpNop(ENE_WRK *ene, ENE_MOT_CMD *cmd, ENE_MOT_SCR_WRK *w)
{
    (void)ene;
    (void)cmd;
    (void)w;
}

// ──────────────────────────────────────────────────────────────────────
// Ends the script's open windows (ops 0xc, 5, 9, 0xf) and the attack
// values of ops 0xd / 0x10 (EneMotScrSet, EneMotScrWinMain).
// FF3 0x00196038

static void EneMotScrFlagsClear(ENE_WRK *ene)
{
    EneMotScrFlag(ene, 0xc);
    EneMotScrFlag(ene, 5);
    EneMotScrFlag(ene, 9);
    EneMotScrFlag(ene, 0xf);
    ene->_6ac = 0.0f;
    ene->_6a8 = 0.0f;
}

static void EneMotScrWinMain(ENE_WRK *ene);

// ──────────────────────────────────────────────────────────────────────
// Per acting frame (EnemyActOne): lens function 8 makes every bound enemy a
// fatal frame; then the script runs against this frame's anime frame (the
// frame-change count advances when it moved, the last frame resets when
// the anime wrapped).
// FF3 0x00196090

void EneMotScrMain(ENE_WRK *ene)
{
    ENE_MOT_SCR_WRK *w;
    ENE_MOT_CMD     *cmd;

    if (GetLensWrk()->GetFunc(LENS_FUNC_REFILL) != 0 && ene->type != 2)
        ene->flags |= 0x1000;

    if (ene->mot_scr == nullptr)
        return;

    cmd = ene->mot_scr->cmd;
    w = &ene_mot_scr_wrk[ene->alg.idx];
    w->frame = EneGetAnimeTime(ene);
    if (w->frame != w->frame_old)
        w->time++;
    if (w->frame < w->frame_old)
        w->frame_old = -1;

    for (; cmd->op != 0x1d; cmd++)
    {
        EneMotScrOp(ene, cmd, w);
        EneMotScrOpNop(ene, cmd, w);
    }

    EneMotScrWinMain(ene);
    w->frame_old = w->frame;
    w->time_old  = w->time;
}

// ──────────────────────────────────────────────────────────────────────
// EneMotScrSet for an anime started at frame: the commands run once with
// frame 0, then the frame row counts up to frame. FF3 does not rewind the
// command pointer, so the later frames run no commands.
// FF3 0x001961b8

void EneMotScrSetFrame(ENE_WRK *ene, u_char anime, u_char sub, short frame)
{
    ENE_MOT_SCR_WRK *w;
    ENE_MOT_CMD     *cmd;

    EneMotScrSet(ene, anime, sub);
    if (ene->mot_scr == nullptr)
        return;

    cmd = ene->mot_scr->cmd;
    w = &ene_mot_scr_wrk[ene->alg.idx];
    w->frame = 0;
    if (frame < 0)
        return;

    do
    {
        for (; cmd->op != 0x1d; cmd++)
            EneMotScrOp(ene, cmd, w);
        w->frame_old = w->frame;
        w->time_old  = w->time;
        w->time++;
        w->frame++;
    } while (w->frame <= frame);
}

// ──────────────────────────────────────────────────────────────────────
// The script's level windows (ENE_MOT_WIN), while dmg_react is 3..5: by the
// enemy's level (1..4 clamped) a shutter chance (flag 0x800, ops 4 /
// 5) and a fatal frame (0x1000, ops 8 / 9) open and close as the anime
// frame passes them; 0x10000 (op 0xb) is set when one closes while the
// other is not open. Window lengths scale by the player's and the
// difficulty's rates and, from level 4 on, by rate per level above 3 (at
// least 2 frames); both start a random 0..9 frames late (0..19 from level
// 4 on, drawn again when the anime wraps).
// FF3 0x001962a8

static void EneMotScrWinMain(ENE_WRK *ene)
{
    static u_char rnd_init;             // FF3 sbss gp-0x4f8
    static int    rnd;                  // FF3 sbss gp-0x4f0
    ENE_MOT_SCR_WRK *w;
    ENE_MOT_SCR   ***tbl;
    ENE_MOT_SCR    **row;
    ENE_MOT_SCR     *scr;
    ENE_MOT_WIN     *win;
    u_char           anime;
    int              frame;
    int              old;
    int              lv;
    int              idx;
    int              l;
    float            mul;
    float            prate;
    float            drate;
    float            sc_len;
    float            ff_len;
    int              sc;
    int              ff;

    if (rnd_init == 0)
    {
        rnd = GetRandValIByF(10);
        rnd_init = 1;
    }
    if (EneIsDmgReactBig(ene) == 0)
        return;

    anime = ene->_554;
    w = &ene_mot_scr_wrk[ene->alg.idx];
    frame = EneGetAnimeTime(ene);
    old = w->frame_old;

    tbl = ene_mot_scr_tbl[ene->cmn->mdl_type];
    if (tbl == nullptr || (row = tbl[anime]) == nullptr || (scr = row[ene->_555]) == nullptr)
    {
        w->anime = anime;
        return;
    }
    win = scr->win;
    if (win == nullptr)
        return;

    if (frame < old)
    {
        old = -1;
        EneMotScrFlagsClear(ene);
        rnd = GetRandValIByF((ene->level < 4) ? 10 : 20);
    }

    lv = ene->level - 1;
    idx = 3;
    if (lv < 4)
        idx = (lv < 0) ? 0 : lv;

    mul = 1.0f;
    prate = PlyrGetEneWinRate();
    if (ene->level > 3)
    {
        for (l = idx + 1; l <= ene->level; l++)
            mul *= win->rate;
    }
    drate = ene_scr_rate[(u_char)IngameWrkGetDifficulty(title_start_flg.mode)];

    sc_len = prate * mul * (float)(win->sc[idx][1] - win->sc[idx][0]) * drate;
    sc = win->sc[idx][0] + rnd;
    if (sc_len < 2.0f)
        sc_len = 2.0f;
    ff = win->ff[idx][0] + rnd;
    ff_len = prate * mul * (float)(win->ff[idx][1] - win->ff[idx][0]) * drate;
    if (ff_len < 2.0f)
        ff_len = 2.0f;

    if (EneMotScrCross(sc, old, frame))
    {
        EneMotScrFlag(ene, 0xc);
        EneMotScrFlag(ene, 4);
    }
    if (EneMotScrCross(sc + (int)sc_len, old, frame))
    {
        EneMotScrFlag(ene, 5);
        if ((ene->flags & 0x1000) == 0)
            EneMotScrFlag(ene, 0xb);
    }
    if (EneMotScrCross(ff, old, frame))
    {
        EneMotScrFlag(ene, 0xc);
        EneMotScrFlag(ene, 8);
    }
    if (EneMotScrCross(ff + (int)ff_len, old, frame))
    {
        EneMotScrFlag(ene, 9);
        if ((ene->flags & 0x800) == 0)
            EneMotScrFlag(ene, 0xb);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The enemy script's op 0x18aaa0: the attr byte of the (anime, sub) script
// of the enemy's model, 0: none.
// FF3 0x00196618

u_char EneMotScrAttrGet(ENE_WRK *ene, u_short anime, u_short sub)
{
    ENE_MOT_SCR ***tbl = ene_mot_scr_tbl[ene->cmn->mdl_type];
    ENE_MOT_SCR  **row;
    ENE_MOT_SCR   *scr;

    if (tbl == nullptr || (row = tbl[anime]) == nullptr || (scr = row[sub]) == nullptr)
        return 0;
    return scr->attr;
}

// ──────────────────────────────────────────────────────────────────────
// The enemy sound work
// ──────────────────────────────────────────────────────────────────────

static void EneSeEntry(int id);
static void EneSePitchUpdate(void);
static void EneStreamNearMain(void);
static void EneAutoStreamMain(void);
static void EneSeLoopMain(ENE_WRK *ene);
static void EneSeEntryClean(void);
static void EneAutoStreamPreload(ENE_WRK *ene);
static void EneAutoStreamPreloadPlay(ENE_WRK *ene);
static void EneAutoStreamPlay(ENE_WRK *ene);
static int  EneEvStreamMain(void);

// ──────────────────────────────────────────────────────────────────────
// EnemyMainInit.
// FF3 0x00196670

void EneSndInit(void)
{
    int i;
    int j;

    memset(&ene_snd_wrk, 0, sizeof(ene_snd_wrk));
    ene_snd_wrk.near_state  = 0;
    ene_snd_wrk.ev_stream   = -1;
    ene_snd_wrk.ev_state    = 7;
    ene_snd_wrk.near_stream = -1;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ene_snd_wrk.slot[i].state = 0;
        ene_snd_wrk.slot[i].bank  = 0;
        for (j = 0; j < 3; j++)
            ene_snd_wrk.slot[i].se[j] = -1;
    }
    for (i = 0; i < 16; i++)
        ene_snd_wrk.se[i] = -1;

    ene_snd_wrk.pitch_loop = 1.0f;
    ene_snd_wrk.pitch      = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Unless its bank is entered: a bound enemy's SE ids are reset; an auto
// enemy with a voice stream preloads it (adat->voice_wait != 0) or plays it.
// FF3 0x00196768

static void EneSndStart(ENE_WRK *ene)
{
    ENE_SND_SLOT *slot = &ene_snd_wrk.slot[ene->alg.idx];
    int           i;

    if (slot->bank == 1)
        return;

    if (ene->type < 2)
    {
        for (i = 0; i < 3; i++)
            slot->se[i] = -1;
        slot->state = 1;
    }
    else if (ene->type == 2)
    {
        ene->voice_wait = (float)ene->adat->voice_wait;
        if (ene->cmn->adpcm_no >= 0)
        {
            if (ene->adat->voice_wait != 0)
                EneAutoStreamPreload(ene);
            else
                EneAutoStreamPlay(ene);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// How well the player hears an enemy by its place on the camera: half the
// camera's view rate, half its distance rate.
// FF3 0x00196830

static float EneSndCamRate(float dmin, float dmax, float deg, float *pos)
{
    float view = PlyrCamViewRate(deg, pos) * 0.5f;

    return view + PlyrCamDistRate(dmin, dmax, pos) * 0.5f;
}

// ──────────────────────────────────────────────────────────────────────
// The player's distance rate (1 within dmin .. 0 at dmax). Not FF2's
// SetEneSealClear (a wrong match).
// FF3 0x001968b0

static float EneSndDistRate(float dmin, float dmax, float *pos)
{
    return PlyrDistRate(dmin, dmax, pos);
}

// ──────────────────────────────────────────────────────────────────────
// The enemy whose voice the near stream plays: of the acting, living bound
// enemies with a voice and flag 0x4000000000, by ENE_DAT.near_pri 0..2,
// the nearest (in XZ) to the player within 120; if none is that close, the
// nearest of the first priority found.
// FF3 0x001968c8

static ENE_WRK *EneStreamNearEneGet(void)
{
    ENE_WRK *best      = nullptr;
    float    best_dist = 9999.0f;
    ENE_WRK *near;
    float    near_dist;
    ENE_WRK *ene;
    float   *ppos;
    float    dx;
    float    dz;
    float    d;
    int      pri = 0;
    int      i;

    do
    {
        near      = nullptr;
        near_dist = 9999.0f;

        for (i = 0, ene = ene_wrk; i < ENE_WRK_MAX; i++, ene++)
        {
            if (ene->type == 2 || ene->state != ENE_STATE_ACT || ene->hp == 0)
                continue;
            if (ene->cmn->adpcm_no < 0 || (ene->flags & 0x4000000000ull) == 0)
                continue;
            if (ene->dat->near_pri != pri)
                continue;

            ppos = ingame_wrk.plyr_wrk->pos;
            dx = ene->pos[0] - ppos[0];
            dz = ene->pos[2] - ppos[2];
            d  = sqrtf(dx * dx + dz * dz);

            if (best == nullptr || (best->dat->near_pri == pri && d < best_dist))
            {
                best_dist = d;
                best      = ene;
            }
            if (d < 120.0f && d < near_dist)
            {
                near_dist = d;
                near      = ene;
            }
        }
    } while (near == nullptr && ++pri < 3);

    if (near != nullptr)
        return near;
    return best;
}

// ──────────────────────────────────────────────────────────────────────
// A bound enemy's looping SEs: vol and rate ease (0.01 a frame) to its
// distance and camera rates, which set the playing loops' volume and pitch.
// FF3 0x00196a30

static void EneSeLoopUpdate(ENE_WRK *ene)
{
    ENE_SND_SLOT *slot = &ene_snd_wrk.slot[ene->alg.idx];
    float        *pos  = ene->pos;
    float         dist = EneSndDistRate(20.0f, 140.0f, pos);
    float         cam  = EneSndCamRate(20.0f, 140.0f, 180.0f, pos);
    float         vol;
    float         pitch;
    int           i;

    if (fabsf(slot->vol - dist) > 0.01f)
        slot->vol = (dist < slot->vol) ? slot->vol - 0.01f : slot->vol + 0.01f;
    else
        slot->vol = dist;

    if (fabsf(slot->rate - cam) > 0.01f)
        slot->rate = (cam < slot->rate) ? slot->rate - 0.01f : slot->rate + 0.01f;
    else
        slot->rate = cam;

    if (ene->cmn->se_bank < 0)
        return;

    vol   = slot->vol * 79.0f + 48.0f;
    pitch = (slot->rate * 1024.0f + 3072.0f) * ene_snd_wrk.pitch_loop;

    for (i = 0; i < 3; i++)
    {
        if (ene->dat->se[i] == -1 || slot->se[i] == -1 || SndSeIsPlaying(slot->se[i]) == 0)
            continue;

        SndSeVolFade(slot->se[i], (int)vol, 0);
        SndSeFadePitch(slot->se[i], (int)pitch, 0);
        SndBufSetPosition(slot->se[i], pos);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The streams' rates (ene_snd_wrk.vol / .rate) ease to the enemy's: the
// volume by 0.01 a frame (heard to 1600 for YKN_15_IREZUMI2A, else 200),
// the pitch by 0.015.
// FF3 0x00196c70

static void EneStreamVolUpdate(ENE_WRK *ene)
{
    float dmax = (ene->cmn->adpcm_no == YKN_15_IREZUMI2A_STR_STR) ? 1600.0f : 200.0f;
    float cam  = EneSndCamRate(20.0f, 100.0f, 180.0f, ene->pos);
    float dist = EneSndDistRate(20.0f, dmax, ene->pos);

    if (fabsf(ene_snd_wrk.vol - dist) > 0.01f)
        dist = (dist < ene_snd_wrk.vol) ? ene_snd_wrk.vol - 0.01f : ene_snd_wrk.vol + 0.01f;
    ene_snd_wrk.vol = dist;

    if (fabsf(ene_snd_wrk.rate - cam) > 0.015f)
        cam = (cam < ene_snd_wrk.rate) ? ene_snd_wrk.rate - 0.015f : ene_snd_wrk.rate + 0.015f;
    ene_snd_wrk.rate = cam;
}

// ──────────────────────────────────────────────────────────────────────
// EnemyMain's sound step. With an ordinary enemy about (or the near stream
// still running): the event stream, else the near stream, and every slot's
// loops; with only auto enemies their delayed voices. Then the SE list.
// FF3 0x00196dd0

void EneSndMain(void)
{
    ENE_WRK *ene;
    int      i;

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_000020) != 0 || ene_snd_wrk.near_state != 0)
    {
        if (EneEvStreamMain() != 0)
            EneStreamNearMain();

        for (i = 0, ene = ene_wrk; i < ENE_WRK_MAX; i++, ene++)
        {
            if (ene->state != ENE_STATE_FREE)
            {
                EneSeLoopMain(ene);
                EneSeStopCheck(ene);
            }
        }
    }
    else if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_BATTLE2) != 0)
    {
        EneAutoStreamMain();
    }

    EneSeEntryClean();
}

// ──────────────────────────────────────────────────────────────────────
// The near stream: the voice of the singing enemy EneStreamNearEneGet
// picks plays at it (the room BGM, map_bgm, fades meanwhile), its
// volume and pitch from EneStreamVolUpdate. Another enemy taking over with
// the same file just moves the stream; otherwise it fades out (after 30
// frames while the old one still acts) and starts again.
// States: 0 idle, 1 another enemy nearer, 2 fade out, 3 wait for the
// stop, 4 start, 5 play, 6 stop.
// FF3 0x00196e90

static void EneStreamNearMain(void)
{
    ENE_SND_WRK *w   = &ene_snd_wrk;
    ENE_WRK     *ene = EneStreamNearEneGet();
    SND_3D_SET   s3d;
    float        rate;
    int          vol;
    int          pitch;
    int          state;

    if (ene != nullptr)
        w->near_file = (TattooEvGetToggle() != 0) ? YKN_19_IREZUMI2B_STR_STR : ene->cmn->adpcm_no;

    state = w->near_state;
    switch (w->near_state)
    {
    case 0:
        if (ene == nullptr)
            return;
        map_bgmFadeOut(30, 0);
        if (w->near_stream != -1 && StreamAutoIsPlaying(w->near_stream) != 0)
            state = 2;
        else
            state = 4;
        break;

    case 1:
        if (w->near_ene == ene)
            state = 5;
        else if (--w->near_time == 0)
            state = 2;
        break;

    case 2:
        if (w->near_stream != -1 && StreamAutoIsPlaying(w->near_stream) != 0)
            StreamAutoFadeOut(w->near_stream, 30);
        w->near_state = 3;
        // fall through
    case 3:
        // FF3 stays in 3 when the stream id is already -1.
        state = w->near_state;
        if (w->near_stream != -1 && StreamAutoIsPlaying(w->near_stream) == 0)
        {
            w->near_stream = -1;
            state = 4;
        }
        break;

    case 4:
        if (ene == nullptr)
        {
            w->near_state = 6;
        }
        else
        {
            rate = EneSndCamRate(20.0f, 100.0f, 180.0f, ene->pos);
            memset(&s3d, 0, sizeof(s3d));
            s3d.pos = ene->pos;
            w->near_stream   = StreamAutoPlayPitch(w->near_file, w->near_file + 1, 0xd, 1, 1,
                                                   SndConvVolume(0), 0, &s3d, 0x1000);
            w->near_file_old = w->near_file;
            w->near_time     = 30;
            w->near_state    = 5;
            w->vol           = 0.0f;
            w->rate          = rate;
            w->near_ene      = ene;
        }
        // fall through
    case 5:
        state = 6;
        if (ene == nullptr)
            break;

        if (w->near_ene == ene)
        {
            // FF3 takes volume and pitch before EneStreamVolUpdate moves them.
            w->near_time = 30;
            pitch = (int)((w->rate * 1024.0f + 3328.0f) * w->pitch_loop);
            vol   = (int)(w->vol * 115.0f + 12.0f);
            EneStreamVolUpdate(ene);
            state = w->near_state;
            if (w->near_stream != -1)
            {
                memset(&s3d, 0, sizeof(s3d));
                s3d.pos = w->near_ene->pos;
                StreamAutoSet3D(w->near_stream, &s3d);
                StreamAutoFade(w->near_stream, SndConvVolume(vol), 0);
                StreamAutoFadePitch(w->near_stream, pitch, 0);
            }
        }
        else
        {
            state = 2;
            if (w->near_ene->state == ENE_STATE_ACT && w->near_ene->hp != 0)
            {
                state = 1;
                if (w->near_file_old == w->near_file)
                {
                    pitch = (int)((w->rate * 1024.0f + 3328.0f) * w->pitch_loop);
                    vol   = (int)(w->vol * 115.0f + 12.0f);
                    if (w->near_stream != -1)
                    {
                        memset(&s3d, 0, sizeof(s3d));
                        s3d.pos = ene->pos;
                        StreamAutoSet3D(w->near_stream, &s3d);
                        StreamAutoFade(w->near_stream, SndConvVolume(vol), 0);
                        StreamAutoFadePitch(w->near_stream, pitch, 0);
                    }
                    w->near_time     = 30;
                    w->near_ene      = ene;
                    state            = w->near_state;
                    w->near_file_old = w->near_file;
                }
            }
        }
        break;

    case 6:
        if (w->near_stream != -1)
        {
            StreamAutoFadeOut(w->near_stream, 30);
            w->near_stream = -1;
        }
        map_bgmFadeIn(30);
        w->near_state = 0;
        w->near_ene   = nullptr;
        w->near_time  = 0;
        state = w->near_state;
        break;
    }

    w->near_state = state;
}

// ──────────────────────────────────────────────────────────────────────
// The story menu asks before it opens.
// FF3 0x001972d8

int EneStreamIsActive(void)
{
    if (ene_snd_wrk.near_stream != -1)
        return 1;
    return ene_snd_wrk.ev_stream != -1;
}

// ──────────────────────────────────────────────────────────────────────
// The auto enemies' (type 2) delayed voices: voice_wait runs down by the
// step, then the preloaded stream plays.
// FF3 0x00197308

static void EneAutoStreamMain(void)
{
    ENE_WRK *ene = ene_wrk;
    int      i;

    for (i = 0; i < ENE_WRK_MAX; i++, ene++)
    {
        if (IsActEnemy(i) == 0 || ene->type != 2 || ene->cmn->adpcm_no < 0 || ene->voice_wait == 0.0f)
            continue;

        if (ene->step < ene->voice_wait)
        {
            ene->voice_wait -= ene->step;
        }
        else
        {
            EneAutoStreamPreloadPlay(ene);
            ene->voice_wait = 0.0f;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// A bound enemy's looping SEs (ENE_DAT.se): 1 loads its bank, 2 starts
// them silent once loaded, 3 keeps them following it while it acts and
// lives, 4 stops them.
// FF3 0x001973c8

static void EneSeLoopMain(ENE_WRK *ene)
{
    ENE_SND_SLOT *slot = &ene_snd_wrk.slot[ene->alg.idx];
    SND_3D_SET    s3d;
    float         rate;
    int           pitch;
    int           i;

    switch (slot->state)
    {
    case 1:
        if (ene->cmn->se_bank < 0)
        {
            slot->state = 0;
            break;
        }
        SndSeBankLoad(ene->cmn->se_bank);
        slot->state = 2;
        slot->bank  = 1;
        break;

    case 2:
        if (SndSeBankIsLoaded(ene->cmn->se_bank) == 0)
            return;

        rate  = EneSndCamRate(20.0f, 140.0f, 180.0f, ene->pos);
        pitch = (int)((rate * 1024.0f + 3072.0f) * ene_snd_wrk.pitch_loop);
        for (i = 0; i < 3; i++)
        {
            memset(&s3d, 0, sizeof(s3d));
            s3d.pos = ene->pos;
            if (ene->dat->se[i] == -1)
            {
                slot->vol  = 0.0f;
                slot->rate = 0.0f;
            }
            else
            {
                slot->se[i] = SndSePlay((u_int)ene->dat->se[i], 0, pitch, 0, &s3d, 0);
                slot->vol   = 0.0f;
                slot->rate  = rate;
            }
        }
        slot->state = 3;
        break;

    case 3:
        if (ene->state == ENE_STATE_ACT && ene->hp != 0)
        {
            EneSeLoopUpdate(ene);
            return;
        }
        slot->state = 4;
        break;

    case 4:
        for (i = 0; i < 3; i++)
        {
            if (ene->dat->se[i] != -1)
            {
                SndSeFadeStop(slot->se[i], 0);
                slot->se[i] = -1;
            }
        }
        slot->state = 0;
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// A bound enemy's release: its SEs stop, its bank goes.
// FF3 0x00197620

static void EneSndRelease(ENE_WRK *ene)
{
    ENE_SND_SLOT *slot = &ene_snd_wrk.slot[ene->alg.idx];
    int           i;

    if (ene->cmn->se_bank >= 0 && SndSeBankIsEntry(ene->cmn->se_bank) != 0)
    {
        for (i = 0; i < 3; i++)
        {
            if (ene->dat->se[i] != -1)
            {
                SndSeFadeStop(slot->se[i], 0);
                slot->se[i] = -1;
            }
        }
        if (slot->bank == 1)
        {
            slot->bank = 0;
            SndSeBankRelease(ene->cmn->se_bank, 0);
        }
    }
    slot->state = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Its SE stops (fade 5) once it no longer acts, is dead, or its anime is
// not the SE's.
// FF3 0x00197700

static void EneSeStopCheck(ENE_WRK *ene)
{
    if (ene->se_id <= 0)
        return;
    if (ene->state == ENE_STATE_ACT && ene->hp != 0 && ene->_554 == ene->se_anime)
        return;
    if (SndSeIsPlaying(ene->se_id) == 0)
        return;

    SndSeFadeStop(ene->se_id, 5);
    ene->se_id = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197770

static void EneSeEntry(int id)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (ene_snd_wrk.se[i] == -1)
        {
            ene_snd_wrk.se[i] = id;
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Frees the SE list's entries that stopped.
// FF3 0x001977b0

static void EneSeEntryClean(void)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (ene_snd_wrk.se[i] != -1 && SndSeIsPlaying(ene_snd_wrk.se[i]) == 0)
            ene_snd_wrk.se[i] = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197818

static void EneSePitchUpdate(void)
{
    float pitch = ene_snd_wrk.pitch * 4096.0f;
    int   i;

    for (i = 0; i < 16; i++)
    {
        if (ene_snd_wrk.se[i] != -1)
            SndSeFadePitch(ene_snd_wrk.se[i], (int)pitch, 3);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The slow mode (PlyrGaugeSlowStart / End) slows the enemies' SEs.
// FF3 0x001978a8

void EneSndSlowSet(u_char slow)
{
    if (slow == 0)
    {
        ene_snd_wrk.pitch      = 1.0f;
        ene_snd_wrk.pitch_loop = ene_snd_wrk.pitch;
    }
    else
    {
        ene_snd_wrk.pitch_loop = 0.4f;
        ene_snd_wrk.pitch      = 0.7f;
    }
    EneSePitchUpdate();
}

// ──────────────────────────────────────────────────────────────────────
// SE se of the enemy's bank at its position, at the slow mode's pitch.
// FF3 0x00197910

void EneSePlay(ENE_WRK *ene, u_int se)
{
    SND_3D_SET s3d;
    int        id;

    memset(&s3d, 0, sizeof(s3d));
    if (ene == nullptr || ene->cmn == nullptr || ene->cmn->se_bank < 0)
        return;

    s3d.pos = ene->pos;                 // FF3 sets it before the tests
    id = SndSePlay(((u_int)ene->cmn->se_bank << 16) | se, 100, (int)(ene_snd_wrk.pitch * 4096.0f), 0, &s3d, 0);
    EneSeEntry(id);
}

// ──────────────────────────────────────────────────────────────────────
// One of four SEs by cumulative per cent (p0..p3); none above their sum.
// Not FF2's GetEnePos (a wrong match).
// FF3 0x001979b0

void EneSePlayRand(ENE_WRK *ene, int se0, int p0, int se1, int p1, int se2, int p2, int se3, int p3)
{
    int r = (s_char)GetRandValIByF(100);

    if (r < p0)
        EneSePlay(ene, (u_int)se0);
    else if (r < p0 + p1)
        EneSePlay(ene, (u_int)se1);
    else if (r < p0 + p1 + p2)
        EneSePlay(ene, (u_int)se2);
    else if (r < p0 + p3 + p2 + p1)
        EneSePlay(ene, (u_int)se3);
}

// ──────────────────────────────────────────────────────────────────────
// An auto enemy's voice, preloaded at its position (EneSndStart, when it
// waits voice_wait frames first).
// FF3 0x00197ac8

static void EneAutoStreamPreload(ENE_WRK *ene)
{
    SND_3D_SET s3d;
    int        file;

    memset(&s3d, 0, sizeof(s3d));
    s3d.pos = ene->pos;
    if (ene->cmn->adpcm_no < 0)
        return;

    file = SubTitleGetStreamFile(4, ene->cmn->adpcm_no);
    if (file < 0)
        return;

    ene->stream_id = StreamAutoPreloadPitch(file, file + 1, 0xd, 1, 0, SndConvVolume(100), 0, &s3d, 0x1000);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197b78

static void EneAutoStreamPreloadPlay(ENE_WRK *ene)
{
    StreamAutoPreloadPlay(ene->stream_id);
    SubTitleReqEne(ene->cmn->adpcm_no, ene->stream_id);
}

// ──────────────────────────────────────────────────────────────────────
// An auto enemy's voice at once, with its subtitle.
// FF3 0x00197bb0

static void EneAutoStreamPlay(ENE_WRK *ene)
{
    SND_3D_SET s3d;
    int        file = SubTitleGetStreamFile(4, ene->cmn->adpcm_no);

    memset(&s3d, 0, sizeof(s3d));
    s3d.pos = ene->pos;
    if (file >= 0)
        ene->stream_id = StreamAutoPlayPitch(file, file + 1, 0xd, 1, 0, SndConvVolume(100), 0, &s3d, 0x1000);

    SubTitleReqEne(ene->cmn->adpcm_no, ene->stream_id);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197c60

static void EneStreamReset(ENE_WRK *ene)
{
    if (ene->cmn->adpcm_no >= 0)
        ene->stream_id = -1;
}

// ──────────────────────────────────────────────────────────────────────
// The tattoo event (TattooEvStart): YKN_18_IREZUMI3A preloaded silent at
// the enemy, who the event stream then follows.
// FF3 0x00197c80

void EneEvStreamPreload(ENE_WRK *ene)
{
    SND_3D_SET s3d;

    memset(&s3d, 0, sizeof(s3d));
    s3d.pos = ene->pos;
    ene_snd_wrk.ev_ene    = ene;
    ene_snd_wrk.ev_stream = StreamAutoPreloadPitch(YKN_18_IREZUMI3A_STR_STR, YKN_18_IREZUMI3A_STR_HXD, 0xd, 1, 1,
                                                   SndConvVolume(0), 3, &s3d, 0x1000);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197d10

void EneStreamNearStop(void)
{
    StreamAutoFadeOut(ene_snd_wrk.near_stream, 20);
    ene_snd_wrk.near_state  = 0;
    ene_snd_wrk.near_stream = -1;
    ene_snd_wrk.near_ene    = nullptr;
    ene_snd_wrk.near_time   = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197d58

void EneEvStreamReset(void)
{
    ene_snd_wrk.ev_state = 7;
}

// ──────────────────────────────────────────────────────────────────────
// Starts the event stream: plays it when nothing was preloaded (then waits
// in 8 as for a preload), else plays the preload once it is ready.
// FF3 0x00197d68

void EneEvStreamPlay(void)
{
    SND_3D_SET s3d;

    if (ene_snd_wrk.ev_stream == -1)
    {
        memset(&s3d, 0, sizeof(s3d));
        s3d.pos = ene_snd_wrk.ev_ene->pos;
        ene_snd_wrk.ev_stream = StreamAutoPlayPitch(YKN_18_IREZUMI3A_STR_STR, YKN_18_IREZUMI3A_STR_HXD, 0xd, 1, 1,
                                                    SndConvVolume(0), 3, &s3d, 0x1000);
        ene_snd_wrk.ev_state = 8;
    }
    else if (StreamAutoIsPreload(ene_snd_wrk.ev_stream) == 0)
    {
        ene_snd_wrk.ev_state = 8;
    }
    else
    {
        StreamAutoPreloadPlay(ene_snd_wrk.ev_stream);
        ene_snd_wrk.ev_state = 9;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197e28

void EneStreamFadeOut(int time)
{
    if (ene_snd_wrk.ev_stream == -1)
        return;

    StreamAutoFadeOut(ene_snd_wrk.ev_stream, time);
    ene_snd_wrk.ev_stream = -1;
}

// ──────────────────────────────────────────────────────────────────────
// The event stream: 7 idle (returns 1: the near stream may run), 8 waits
// for the preload, 9 follows its enemy.
// FF3 0x00197e78

static int EneEvStreamMain(void)
{
    ENE_SND_WRK *w = &ene_snd_wrk;
    SND_3D_SET   s3d;
    float        vol;
    float        pitch;

    switch (w->ev_state)
    {
    case 7:
        return 1;

    case 8:
        if (StreamAutoIsPreload(w->ev_stream) != 0)
        {
            StreamAutoPreloadPlay(w->ev_stream);
            w->ev_state = 9;
        }
        break;

    case 9:
        EneStreamVolUpdate(w->ev_ene);
        pitch = (w->rate * 200.0f + 3800.0f) * w->pitch_loop;
        vol   = w->vol * 52.0f + 48.0f;
        if (w->ev_stream != -1)
        {
            memset(&s3d, 0, sizeof(s3d));
            s3d.pos = w->ev_ene->pos;
            StreamAutoSet3D(w->ev_stream, &s3d);
            StreamAutoFade(w->ev_stream, SndConvVolume((int)vol), 0);
            StreamAutoFadePitch(w->ev_stream, (int)pitch, 0);
        }
        break;
    }

    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00197fd8

static void EneAlgTblSet(ENE_WRK *ene, s_char no)
{
    ENE_ALG_NODE *p;

    if (no < 0)
    {
        ene->route[0] = nullptr;
        ene->route[2] = nullptr;
        ene->route[1] = nullptr;
        return;
    }

    p = ene_alg_tbl[no];
    ene->route[2] = p;
    ene->route[0] = p;
    ene->route[1] = p;
}

// ──────────────────────────────────────────────────────────────────────
// Turns (and follows) by mode: 0 faces the player at once, 0xfe turn_pos,
// 0xff stops following, n turns to the player at 40 degrees * the frame
// scale / n per frame.
// FF3 0x001931e0

void EneTurnReq(ENE_WRK *ene, u_char mode)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR rot;

    ene->follow = 1;

    if (mode == 0)
    {
        UnitGetTrgtRot(ene->pos, plyr->pos, rot, 3);
        ene->rot[0] = 0.0f;
        ene->rot[1] = rot[1];
        RotLimitChk(&ene->rot[1]);
        ene->turn_spd = 0.0f;
    }
    else if (mode == 0xfe)
    {
        UnitGetTrgtRot(ene->pos, ene->turn_pos, ene->rot, 2);
        ene->turn_spd = 0.0f;
    }
    else if (mode == 0xff)
    {
        ene->turn_spd = 0.0f;
        ene->rot_trgt[1] = ene->rot[1];
        ene->follow = 0;
    }
    else
    {
        UnitGetTrgtRot(ene->pos, plyr->pos, ene->rot_trgt, 3);
        ene->turn_spd = GetFrameTimeScale() * 0.698132038f / (float)mode;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The route (ENE_WRK.route): the next node, wrapping at the end. FF3 tests
// &ene->alg against nullptr, i.e. ene.
// FF3 0x00198020

void EneAlgNext(ENE_WRK *ene)
{
    ENE_ALG_NODE *cur;

    if (ene == nullptr)
        return;

    cur = ene->route[1];
    ene->route[2] = cur;
    ene->route[1] = cur + 1;
    if ((cur[1].flag & 0x8000) != 0)
        ene->route[1] = ene->route[0];
}

// ──────────────────────────────────────────────────────────────────────
// The route's node nearest (3D) the enemy becomes the current one.
// FF3 0x00198060

void EneAlgNearest(ENE_WRK *ene)
{
    ENE_ALG_NODE *n;
    float         best = 9999.0f;
    float         dx;
    float         dy;
    float         dz;
    float         d;

    if (ene == nullptr || ene->route[0]->flag == 0x8000)
        return;

    for (n = ene->route[0]; n->flag != 0x8000; n++)
    {
        dx = ene->pos[0] - n->pos[0];
        dy = ene->pos[1] - n->pos[1];
        dz = ene->pos[2] - n->pos[2];
        d  = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < best)
        {
            ene->route[1] = n;
            best = d;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001980e8

u_int EneAlgGetCur(ENE_WRK *ene)
{
    if (ene == nullptr)
        return 0;
    return ene->route[1]->flag;
}

// ──────────────────────────────────────────────────────────────────────
// FF2 utility.c's segment test, which FF3 keeps here: a bounding-box
// reject on both axes, then each segment straddles the other.
// FF3 0x00198108; from MioPan common/utility.c LineIntersect (FF3 tests x,
// y of e1 against e2 first, then of e2 against e1).

int LineIntersect(LINE_T *e1, LINE_T *e2)
{
    float e1x_min = (e1->a.x < e1->b.x) ? e1->a.x : e1->b.x;
    float e1x_max = (e1->a.x <= e1->b.x) ? e1->b.x : e1->a.x;
    float e1y_min = (e1->a.y < e1->b.y) ? e1->a.y : e1->b.y;
    float e1y_max = (e1->a.y <= e1->b.y) ? e1->b.y : e1->a.y;
    float e2x_min = (e2->a.x < e2->b.x) ? e2->a.x : e2->b.x;
    float e2x_max = (e2->a.x <= e2->b.x) ? e2->b.x : e2->a.x;
    float e2y_min = (e2->a.y < e2->b.y) ? e2->a.y : e2->b.y;
    float e2y_max = (e2->a.y <= e2->b.y) ? e2->b.y : e2->a.y;

    if (e1x_min > e2x_max || e1y_min > e2y_max || e2x_min > e1x_max || e2y_min > e1y_max)
        return 0;

    if (EneLineStraddle(e1, &e2->a, &e2->b) == 0)
        return 0;
    return EneLineStraddle(e2, &e1->a, &e1->b) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// a and b are not strictly on the same side of e (one on it counts).
// FF3 0x00198238; FF2 utility.c LineStraddle (the plain name is held by a
// wrong FF2 match at 0x2674b8). Not FF2's EnePosInfoSet (a wrong match).

int EneLineStraddle(LINE_T *e, POINT_T *a, POINT_T *b)
{
    return LineSide(a, e) * LineSide(b, e) < 1;
}

// ──────────────────────────────────────────────────────────────────────
// Which side of e p is on: the sign of twice the area of (p, e.a, e.b).
// FF3 0x00198298; from MioPan common/utility.c LineSide.

int LineSide(POINT_T *p, LINE_T *e)
{
    float d = e->b.x * (p->y - e->a.y) + e->a.x * (e->b.y - p->y) + p->x * (e->a.y - e->b.y);

    if (d > 0.0f)
        return 1;
    if (d < 0.0f)
        return -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Whether tgt is inside the triangle (in XZ) of length dist and half-angle
// ang (degrees, < 90) in front of pos facing rot. FF2 took the enemy and
// tested the player; FF3 takes both points (EneViewPlyrCheck, 0x198530).
// FF3 0x00198328; from MioPan ingame/enemy/enemy.c CheckEneView.

int CheckEneView(float rot, float dist, float ang, float *pos, float *tgt)
{
    sceVu0FMATRIX wlm;
    sceVu0FVECTOR wp[3];
    sceVu0FVECTOR mp[3];
    POINT_T       p;
    LINE_T        l;
    int           a1;
    int           a2;
    int           a3;
    int           i;

    memset(mp, 0, sizeof(mp));
    mp[0][3] = 1.0f;
    mp[1][3] = 1.0f;
    mp[2][3] = 1.0f;

    if (ang >= 90.0f)
        return 0;

    mp[2][0] = tanf(ang * 3.14159274f / 180.0f) * dist;
    mp[1][0] = -mp[2][0];
    mp[1][2] = dist;
    mp[2][2] = dist;

    sceVu0UnitMatrix(wlm);
    sceVu0RotMatrixY(wlm, wlm, rot);
    sceVu0TransMatrix(wlm, wlm, pos);
    for (i = 0; i < 3; i++)
        sceVu0ApplyMatrix(wp[i], wlm, mp[i]);

    p.x = tgt[0];
    p.y = tgt[2];

    l.a.x = wp[0][0];
    l.a.y = wp[0][2];
    l.b.x = wp[1][0];
    l.b.y = wp[1][2];
    a1 = LineSide(&p, &l);

    l.a.x = wp[1][0];
    l.a.y = wp[1][2];
    l.b.x = wp[2][0];
    l.b.y = wp[2][2];
    a2 = LineSide(&p, &l);

    l.a.x = wp[2][0];
    l.a.y = wp[2][2];
    l.b.x = wp[0][0];
    l.b.y = wp[0][2];
    a3 = LineSide(&p, &l);

    if (a1 == -1 && a2 == -1 && a3 == -1)
        return 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// 0x191988: the player's head is inside the cone (half-angle ang degrees,
// length dist) from the enemy's pos facing its joint 2's angle about y.
// FF3 0x00198530

int EneViewPlyrCheck(float dist, float ang, ENE_WRK *ene)
{
    sceVu0FVECTOR head;

    PlyrGetJointPos(2, head, nullptr);
    return CheckEneView(EneGetJointRot(ene, 2, 1), dist, ang, ene->pos, head) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The enemy script's route op (0x18dc20): the action of a node whose flag
// has 0x1000 (no = its low byte). The ROM has one row; FF3 reads further
// rows from the route lists that follow it.
// FF3 0x001985a8

ENE_ALG_ACT *EneAlgActGet(int no)
{
    return &ene_alg_act_tbl[no];
}
