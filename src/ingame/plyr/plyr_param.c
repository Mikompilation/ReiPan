/* ==========================================================================
 *  ingame/plyr/plyr_param.c
 *
 *  Per-player parameter tables (FF3-only, no FF2 counterpart): FF3
 *  0x2394a8..0x239ac0 and the rodata block 0x361270..0x3613e8. See
 *  plyr_param.h for the camera levels and the 0xff conventions.
 * ======================================================================== */
#include "ingame/plyr/plyr_param.h"

#include "ingame/ingame_wrk.h"          // IngameWrkGetDifficulty, title_start_flg
#include "ingame/plyr/player.h"         // PLYR_WRK, PLYR_NUM, GetPlyrWrk, PlyrGetCurNo, PlyrGetEquipLv

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// rodata 0x361270: one row per player.
typedef struct
{
    /* 0x00 */ float win_rate;          // PlyrGetEneWinRate
    /* 0x04 */ float charge_scale;      // PlyrGetChargeScale
    /* 0x08 */ float shot_rate;         // PlyrGetShotRate
} PLYR_PARAM;
static_assert(sizeof(PLYR_PARAM) == 0xc, "PLYR_PARAM");
static_assert(offsetof(PLYR_PARAM, charge_scale) == 0x4, "PLYR_PARAM.charge_scale");
static_assert(offsetof(PLYR_PARAM, shot_rate) == 0x8, "PLYR_PARAM.shot_rate");

static const PLYR_PARAM plyr_param[PLYR_NUM] = {
    { 1.0f,  0.7f, 1.0f },
    { 0.85f, 1.0f, 1.0f },
    { 0.92f, 0.5f, 1.0f },
};

// The byte tables are int tables in FF3 (4-byte rows); the getters return
// their low byte (lbu).

// rodata 0x361298: charge steps by camera level PLYR_CAM_LV_CHARGE.
static const int plyr_charge_max[PLYR_NUM][PLYR_CAM_LV_NUM] = {
    { 4, 6, 8, 10 },
    { 5, 7, 9, 11 },
    { 1, 1, 1, 1 },
};

// rodata 0x3612c8: shot distance by camera level PLYR_CAM_LV_RANGE.
static const float plyr_shot_range[PLYR_NUM][PLYR_CAM_LV_NUM] = {
    { 80.0f, 90.0f, 100.0f, 120.0f },
    { 90.0f, 110.0f, 130.0f, 140.0f },
    { 70.0f, 80.0f, 90.0f, 100.0f },
};

// rodata 0x3612f8: charge speed by camera level PLYR_CAM_LV_RANGE.
static const int plyr_charge_speed[PLYR_NUM][PLYR_CAM_LV_NUM] = {
    { 35, 40, 45, 50 },
    { 60, 70, 80, 90 },
    { 50, 70, 90, 110 },
};

// rodata 0x361328: charge speed by equipment level 6.
static const int plyr_charge_speed_equip[PLYR_CAM_LV_NUM] = { 40, 50, 60, 70 };

// rodata 0x361338: capture circle scale by camera level PLYR_CAM_LV_FINDER.
static const float plyr_finder_scale[PLYR_NUM][PLYR_CAM_LV_NUM] = {
    { 0.8f, 0.87f, 0.94f, 1.0f },
    { 0.65f, 0.7f, 0.75f, 0.8f },
    { 0.7f, 0.77f, 0.84f, 0.9f },
};

// rodata 0x361368: the rate for each fifth of the charge.
static const float plyr_charge_step_rate[PLYR_NUM][5] = {
    { 1.9f, 1.7f, 1.5f, 1.0f, 0.5f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
};

// rodata 0x3613a8: charge drained per frame.
static const int plyr_charge_drain[PLYR_NUM] = { 5, 0, 10 };

// rodata 0x3613b8: hp by difficulty (IngameWrkGetDifficulty: 0..3).
static const int plyr_hp_max[PLYR_NUM][4] = {
    { 20000, 9500, 6500, 4000 },
    { 15000, 7500, 5000, 3500 },
    { 30000, 13000, 8500, 6000 },
};

// rodata 0x371ba4: the capture circle's radius on screen at scale 1.
#define PLYR_FINDER_RADIUS 125.0f

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002394a8.

float PlyrGetEneWinRate(void)
{
    return plyr_param[PlyrGetCurNo()].win_rate;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002394e0.

float PlyrGetChargeScale(void)
{
    return plyr_param[PlyrGetCurNo()].charge_scale;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239518.

float PlyrGetShotRate(void)
{
    return plyr_param[PlyrGetCurNo()].shot_rate;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239550.

u_char PlyrGetChargeDrain(void)
{
    return (u_char)plyr_charge_drain[PlyrGetCurNo()];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239580.

float PlyrGetChargeStepRate(u_char step)
{
    return plyr_charge_step_rate[PlyrGetCurNo()][step];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002395c8.

u_char PlyrGetChargeMax(u_char plyr, u_char lv)
{
    if (plyr == PLYR_PARAM_CUR)
        plyr = PlyrGetCurNo();
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetCamLvCharge(plyr);
    return (u_char)plyr_charge_max[plyr][lv];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239640.

float PlyrGetShotRange(u_char plyr, u_char lv)
{
    if (plyr == PLYR_PARAM_CUR)
        plyr = PlyrGetCurNo();
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetCamLvRange(plyr);
    return plyr_shot_range[plyr][lv];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002396b8.

u_char PlyrGetChargeSpeed(u_char plyr, u_char lv)
{
    if (plyr == PLYR_PARAM_CUR)
        plyr = PlyrGetCurNo();
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetCamLvRange(plyr);
    return (u_char)plyr_charge_speed[plyr][lv];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239730.

float PlyrGetFinderRadius(u_char plyr, u_char lv)
{
    if (plyr == PLYR_PARAM_CUR)
        plyr = PlyrGetCurNo();
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetCamLvFinder(plyr);
    return plyr_finder_scale[plyr][lv] * PLYR_FINDER_RADIUS;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002397b8.

float PlyrGetFinderScale(u_char plyr, u_char lv)
{
    if (plyr == PLYR_PARAM_CUR)
        plyr = PlyrGetCurNo();
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetCamLvFinder(plyr);
    return plyr_finder_scale[plyr][lv];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239830.

u_char PlyrGetChargeSpeedEquip(u_char lv)
{
    if (lv == PLYR_PARAM_CUR)
        lv = PlyrGetEquipLv(6);
    return (u_char)plyr_charge_speed_equip[lv];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239878.

u_char PlyrGetCamLvCharge(u_char plyr)
{
    return GetPlyrWrk((s_char)plyr)->equip[PLYR_CAM_LV_CHARGE];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002398a0.

u_char PlyrGetCamLvRange(u_char plyr)
{
    return GetPlyrWrk((s_char)plyr)->equip[PLYR_CAM_LV_RANGE];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002398c8.

u_char PlyrGetCamLvFinder(u_char plyr)
{
    return GetPlyrWrk((s_char)plyr)->equip[PLYR_CAM_LV_FINDER];
}

// ──────────────────────────────────────────────────────────────────────
// The three level-ups: one more, up to PLYR_CAM_LV_MAX.

static void PlyrCamLvUp(u_char plyr, int which)
{
    PLYR_WRK *p = GetPlyrWrk((s_char)plyr);
    int       lv = p->equip[which] + 1;

    p->equip[which] = (u_char)((lv < PLYR_CAM_LV_MAX + 1) ? lv : PLYR_CAM_LV_MAX);
}

// FF3 0x002398f0.
// callcheck: GetPlyrWrk (in PlyrCamLvUp; FF3 inlines it in each)

void PlyrCamLvChargeUp(u_char plyr)
{
    PlyrCamLvUp(plyr, PLYR_CAM_LV_CHARGE);
}

// FF3 0x00239968.
// callcheck: GetPlyrWrk (in PlyrCamLvUp; FF3 inlines it in each)

void PlyrCamLvRangeUp(u_char plyr)
{
    PlyrCamLvUp(plyr, PLYR_CAM_LV_RANGE);
}

// FF3 0x002399e0.
// callcheck: GetPlyrWrk (in PlyrCamLvUp; FF3 inlines it in each)

void PlyrCamLvFinderUp(u_char plyr)
{
    PlyrCamLvUp(plyr, PLYR_CAM_LV_FINDER);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00239a58.

int PlyrMdlGetHpMax(u_char plyr, u_char diff)
{
    if (diff == PLYR_PARAM_CUR)
        diff = (u_char)IngameWrkGetDifficulty(title_start_flg.mode);
    return plyr_hp_max[plyr][diff];
}
