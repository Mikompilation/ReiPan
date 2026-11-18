/* ==========================================================================
 *  ingame/plyr/plyr_gauge.c
 *
 *  Player 1's slow-mode gauge (FF3-only, no FF2 counterpart): FF3
 *  0x129968..0x12a068. See plyr_gauge.h for the gauge's roles.
 * ======================================================================== */
#include "ingame/plyr/plyr_gauge.h"

#include <math.h>                       // fabsf

#include "ingame/enemy/ene_main.h"     // ene_wrk (the enemy work)
#include "ingame/enemy/enemy.h"         // EneSndSlowSet
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/photo/finder_photo.h"  // GetLensWrk
#include "ingame/photo/finder_view_hud.h" // HudSlowGaugeDraw
#include "ingame/photo/fndr_parts.h"    // LensGetFunc, LENS_FUNC_REFILL
#include "ingame/plyr/player.h"         // PLYR_WRK, PlyrGetCurNo, PlyrEquip3Rate / 4Rate
#include "ingame/sound/snd_ctrl.h"      // SndSePlay, SndSeIsPlaying, SndSeFadeStop
#include "system/os/system.h"           // GetFrameTimeScale

// The slow mode's SEs (player 1's bank).
#define PLYR_GAUGE_SE_START 0x1b7f0006
#define PLYR_GAUGE_SE_LOOP  0x1b7f0007
#define PLYR_GAUGE_SE_END   0x1b7f0008

// HudSlowGaugeDraw's colour rows.
#define PLYR_GAUGE_COL_NORMAL 0
#define PLYR_GAUGE_COL_SLOW   1         // the slow mode is on
#define PLYR_GAUGE_COL_LOW    2         // under PLYR_GAUGE_COST: cannot start

static int PlyrGaugeIsSlow(void)
{
    return (ingame_wrk.plyr_wrk->flags & PLYR_FLG_100000) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129968.

void PlyrGaugeInit(PLYR_GAUGE *gauge)
{
    gauge->fill = 0.0f;
    gauge->se_no = -1;
    gauge->disp = 0.0f;
}

// Fills by val (scaled by equipment 3's level), not while the slow mode is on.
// FF3 0x00129980.

void PlyrGaugeAdd(float val, PLYR_GAUGE *gauge)
{
    float fill;

    if (PlyrGetCurNo() != 1 || PlyrGaugeIsSlow())
        return;

    fill = gauge->fill + val * PlyrEquip3Rate(0xff);
    if (!(fill < PLYR_GAUGE_MAX))
        fill = PLYR_GAUGE_MAX;
    gauge->fill = fill;
}

// Drains by val (scaled by equipment 4's level); empty ends the slow mode.
// FF3 0x00129a08.

void PlyrGaugeSub(float val, PLYR_GAUGE *gauge)
{
    float fill;

    if (PlyrGetCurNo() != 1)
        return;

    fill = gauge->fill - val * PlyrEquip4Rate(0xff);
    if (!(0.0f < fill))
        fill = 0.0f;
    gauge->fill = fill;
    if (fill <= 0.0f)
        PlyrGaugeSlowEnd(gauge);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129a98.

float PlyrGaugeGet(PLYR_GAUGE *gauge)
{
    if (PlyrGetCurNo() != 1)
        return 0.0f;
    return gauge->fill;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129ad0.

float PlyrGaugeGetRate(PLYR_GAUGE *gauge)
{
    if (PlyrGetCurNo() != 1)
        return 0.0f;
    return gauge->fill / PLYR_GAUGE_MAX;
}

// Starts the slow mode when the gauge holds the cost, and spends it.
// FF3 0x00129b20.

void PlyrGaugeSlowStart(PLYR_GAUGE *gauge)
{
    if (PlyrGetCurNo() != 1)
        return;
    if (gauge->fill < PLYR_GAUGE_COST)
        return;

    PlyrGaugeSub(PLYR_GAUGE_COST, gauge);
    ingame_wrk.plyr_wrk->flags |= PLYR_FLG_100000;
    EneSndSlowSet(1);
    SndSePlay(PLYR_GAUGE_SE_START, 100, 0x1000, 0, nullptr, 0);
    gauge->se_no = SndSePlay(PLYR_GAUGE_SE_LOOP, 100, 0x1000, 0, nullptr, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129bd8.

void PlyrGaugeSlowEnd(PLYR_GAUGE *gauge)
{
    ingame_wrk.plyr_wrk->flags &= ~(u_long)PLYR_FLG_100000;
    EneSndSlowSet(0);
    SndSePlay(PLYR_GAUGE_SE_END, 100, 0x1000, 0, nullptr, 0);
    PlyrGaugeSeStop(gauge);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129c40.

void PlyrGaugeSeStop(PLYR_GAUGE *gauge)
{
    if (gauge->se_no == -1)
        return;
    if (!SndSeIsPlaying(gauge->se_no))
        return;

    SndSeFadeStop(gauge->se_no, 5);
    gauge->se_no = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00129c98.

void PlyrGaugeDraw(float x, float y, PLYR_GAUGE *gauge, int alpha)
{
    int col;

    if (PlyrGetCurNo() != 1)
        return;

    col = PLYR_GAUGE_COL_NORMAL;
    if (PlyrGaugeIsSlow())
        col = PLYR_GAUGE_COL_SLOW;
    else if (gauge->fill < PLYR_GAUGE_COST)
        col = PLYR_GAUGE_COL_LOW;

    HudSlowGaugeDraw(col, x, y, (float)alpha * (1.0f / 128.0f), gauge->disp);
}

// Drains the gauge while the slow mode is on, keeps it full with lens
// function 8, and moves the shown value toward the fill by at most 0.02.
// FF3 0x00129d70.

void PlyrGaugeMain(PLYR_GAUGE *gauge)
{
    float rate;
    float step;
    float disp;

    if (PlyrGetCurNo() != 1)
        return;

    if (PlyrGaugeIsSlow())
        PlyrGaugeSub(GetFrameTimeScale(), gauge);

    if (GetLensWrk()->GetFunc(LENS_FUNC_REFILL))
        gauge->fill = PLYR_GAUGE_MAX;
    else if (gauge->fill < 0.0f)
        gauge->fill = 0.0f;

    rate = PlyrGaugeGetRate(gauge);
    if (rate == gauge->disp)
        return;

    step = fabsf(rate - gauge->disp);
    if (0.02f < step)
        step = 0.02f;

    disp = gauge->disp;
    if (disp < rate)
        disp += step;
    else
        disp -= step;
    gauge->disp = disp;

    if (1.0f < disp)
        disp = 1.0f;
    else if (disp < 0.0f)
        disp = 0.0f;
    gauge->disp = disp;
}

// The player's timer step: 0.6 with flag 4, else 1; 1 in the death scene;
// times 0.67 in the slow mode. Held (modes 1..4) by a live targetable enemy
// in a hold reaction, the enemy's own step instead (ENE_WRK.step).
// The gauge is not read.
// FF3 0x00129ea8.

float PlyrGaugeTimeStep(PLYR_GAUGE *gauge)
{
    PLYR_WRK     *plyr;
    ENE_WRK *ene;
    float         base;
    float         step;

    (void)gauge;

    base = ((ingame_wrk.plyr_wrk->flags & 4) != 0) ? 0.6f : 1.0f;
    if (ingame_wrk.plyr_wrk->mode == PLYR_MODE_7)
        return 1.0f;

    step = base;
    if (PlyrGetCurNo() == 1 && PlyrGaugeIsSlow())
        step = base * 0.67f;

    plyr = ingame_wrk.plyr_wrk;
    if ((u_int)(plyr->mode - 1) >= 4)
        return step;

    ene = &ene_wrk[plyr->ene_no];
    if (ene->state != 3)
        return step;
    if (ene->hp == 0)
        return step;

    switch ((u_short)plyr->dmg_type)
    {
    case 3:
    case 4:
    case 5:
    case 6:
    case 10:
    case 11:
        return ene->step;
    default:
        return step;
    }
}

// The hold damage rate: 0.34 in the slow mode (not in the death scene).
// FF3 0x00129fd0.

float PlyrGaugeDmgRate(PLYR_GAUGE *gauge)
{
    (void)gauge;

    if (ingame_wrk.plyr_wrk->mode == PLYR_MODE_7)
        return 1.0f;
    if (PlyrGetCurNo() != 1)
        return 1.0f;
    if (!PlyrGaugeIsSlow())
        return 1.0f;
    return 0.34f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0012a060.

void *PlyrGaugeGetSaveData(PLYR_GAUGE *gauge)
{
    return gauge;
}
