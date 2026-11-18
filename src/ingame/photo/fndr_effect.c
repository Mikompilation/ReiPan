/* ==========================================================================
 *  ingame/photo/fndr_effect.c
 *
 *  The photo mode work's sub-works +0x180 (the finder point's glow), +0x1e8
 *  (the spirit charge) and +0x288 (the pulse-in panel): FF3-only code, FF3
 *  0x1c1418..0x1c2d80, between fly_ctrl.c and fndr_parts.c. finder_photo.c
 *  inits, runs and draws them; the enemy HUD (ene_main.c EnePhotoHudCtrl),
 *  the play data and the player code (0x23b0b0, 0x23c028, 0x23d178: the
 *  charge per enemy) set their flags and feed the charge. Layouts and roles:
 *  fndr_effect.h.
 * ======================================================================== */
#include "ingame/photo/fndr_effect.h"

#include <string.h>                         // memset

#include "ingame/enemy/ene_main.h"         // EneAnyDmgReactBig
#include "ingame/enemy/ene_main.h"          // ShutterChanceChk
#include "ingame/ingame_wrk.h"              // ingame_wrk
#include "ingame/photo/finder_photo.h"      // GetPhotoModeWrk180, GetLensWrk, PhotoModeSePlay
#include "ingame/photo/finder_view_hud.h"   // the HUD draws
#include "ingame/plyr/player.h"             // PlyrGetCurNo, PlyrGetEquipLv, PLYR_FLG_000020
#include "ingame/plyr/plyr_param.h"         // the camera-level tables
#include "ingame/sound/snd_ctrl.h"          // SndSePlay
#include "system/os/system.h"               // GetPALMode, GetFrameTimeScale

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// data 0x327378 / 0x327390 / 0x3273a8: the +0x180 sub-work's RGBA rows, one
// table per group, six rows each (PHOTO180_FLG_* pick the row).
static const u_char photo180_col_tbl[PHOTO180_COL_GRP][PHOTO180_COL_ROW][4] =
{
    {   { 0, 0, 0, 0 }, { 220, 160, 60, 128 }, { 69, 136, 167, 90 },
        { 69, 136, 167, 90 }, { 255, 50, 82, 90 }, { 0, 0, 0, 0 } },
    {   { 0, 0, 0, 0 }, { 255, 179, 103, 64 }, { 69, 136, 167, 128 },
        { 69, 136, 167, 128 }, { 207, 72, 43, 103 }, { 0, 0, 0, 0 } },
    {   { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 69, 136, 167, 100 },
        { 69, 136, 167, 100 }, { 255, 50, 82, 90 }, { 0, 0, 0, 0 } },
};

// sdata 0x370768 (gp-0x2d88): the +0x180 glow's fixed sprite colour (RGBA).
static const u_char photo180_base_col[4] = { 0xa7, 0x88, 0x45, 0x4d };

// data 0x3273c0: the charge display PhotoWrk1e8Draw fills each draw.
static PHOTO_CHARGE_DISP photo_charge_disp;

// ══════════════════════════════════════════════════════════════════════
// +0x288: the pulse-in panel
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Everything off and the pulses idle.
// FF3 0x001c1418.

void PhotoWrk288Init(PHOTO_WRK_288 *w)
{
    int i;

    for (i = 0; i < PHOTO288_ENT_NUM; i++)
        w->on[i] = 0;

    w->score    = 0;
    w->kill_cnt = 0;
    w->hit_cnt  = 0;
    w->sp_cnt   = 0;
    w->level    = 0;
    w->corner_a = 0;
    w->corner_b = 0;

    w->pulse.val      = 0;
    w->pulse.speed    = 0;
    w->pulse.wait.val = 0;

    for (i = 0; i < PHOTO288_ENT_NUM; i++)
    {
        w->ent[i].val      = 0;
        w->ent[i].speed    = 0;
        w->ent[i].wait.val = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The corner pulse holds while the level is 2 or more and EneAnyDmgReactBig is
// on; the corners and icons 1..13 run (ent[0] never does).
// FF3 0x001c1490.

void PhotoWrk288Main(PHOTO_WRK_288 *w)
{
    int hold = 0;
    int i;

    if (w->level > 1)
        hold = EneAnyDmgReactBig() != 0;
    w->pulse.hold = hold;

    w->pulse.Work();
    for (i = 1; i < PHOTO288_ENT_NUM; i++)
        w->ent[i].Work();
}

// ──────────────────────────────────────────────────────────────────────
// Outside the real camera: four corner marks closing in as the pulse rises
// (by level, else corner_a / corner_b), then each shown icon 1..12 sliding
// in (from the left while its pulse rises, from above otherwise), 23 px
// apart. The number (ent[13]) follows below them in both modes.
// FF3 0x001c1500.

void PhotoWrk288Draw(float x, float y, PHOTO_WRK_288 *w, int alpha, int real)
{
    float t, a, dx, dy, x2, y2;
    s_char rise;
    int i;

    if (real == 0)
    {
        t = (float)w->pulse.val / 127.0f;
        if (0.0f < t)
        {
            dy = (1.0f - t) * 4.4f;
            dx = (1.0f - t) * 6.0f;
            a  = (float)alpha * t * 0.0078125f * 0.25f;

            if (w->level >= 2)
            {
                HudPanelLevelDraw(x - dx, y - dy, a, w->level);
                HudPanelLevelDraw(x - dx, y + dy, a, w->level);
                HudPanelLevelDraw(x + dx, y - dy, a, w->level);
                HudPanelLevelDraw(x + dx, y + dy, a, w->level);
            }
            else if (w->corner_a != 0)
            {
                HudPanelMark1Draw(x - dx, y - dy, a);
                HudPanelMark1Draw(x - dx, y + dy, a);
                HudPanelMark1Draw(x + dx, y - dy, a);
                HudPanelMark1Draw(x + dx, y + dy, a);
            }
            else if (w->corner_b != 0)
            {
                HudPanelMark2Draw(x - dx, y - dy, a);
                HudPanelMark2Draw(x - dx, y + dy, a);
                HudPanelMark2Draw(x + dx, y - dy, a);
                HudPanelMark2Draw(x + dx, y + dy, a);
            }
        }

        for (i = 1; i < PHOTO288_ENT_NO; i++)
        {
            CPulseVariable<0> *p = &w->ent[i];

            if (w->on[i] == 0)
                continue;
            t = (float)p->val / 127.0f;
            if (!(0.0f < t))
                continue;

            dx = (1.0f - t) * 12.0f;
            dy = (1.0f - t) * 8.0f;
            a  = (float)alpha * t * 0.0078125f * 0.5f;

            rise = (p->on == 0 && p->delay < 1) ? p->speed : 0;
            if (rise < 1)
            {
                HudPanelIconDraw(x, y - dy, a, i);
                x2 = x;
                y2 = y - dy * 0.5f;
            }
            else
            {
                HudPanelIconDraw(x - dx, y, a, i);
                x2 = x - dx * 0.25f;
                y2 = y;
            }
            HudPanelIconDraw(x2, y2, a, i);
            y += 23.0f;
        }
    }

    if (0 < w->num)
    {
        CPulseVariable<0> *p = &w->ent[PHOTO288_ENT_NO];

        t = (float)p->val / 127.0f;
        if (0.0f < t)
        {
            dy = (1.0f - t) * 8.0f;
            dx = (1.0f - t) * 12.0f;
            a  = (float)alpha * t * 0.0078125f * 0.5f;

            rise = (p->on == 0 && p->delay < 1) ? p->speed : 0;
            if (rise < 1)
            {
                HudPanelNumDraw(x, y - dy, a, w->num);
                y = y - dy * 0.5f;
            }
            else
            {
                HudPanelNumDraw(x - dx, y, a, w->num);
                x = x - dx * 0.25f;
            }
            HudPanelNumDraw(x, y, a, w->num);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Shows the number and pulses the corners in after 'delay' frames, then
// each shown icon 3 frames after the previous one (all hold until frame
// 0x28 of the corner pulse).
// FF3 0x001c1a08.

void PhotoWrk288Req(PHOTO_WRK_288 *w, int num, int delay)
{
    int i;

    w->num = num;
    w->pulse.Req(GetPALMode() ? 5 : 6, 0x28, delay);

    for (i = 1; i < PHOTO288_ENT_NUM; i++)
    {
        CPulseVariable<0> *p = &w->ent[i];
        int next = delay;

        p->wait.val = 0;
        p->val      = 0;
        p->speed    = 0;
        if (w->on[i] != 0)
        {
            p->Req(GetPALMode() ? 5 : 6, (short)(0x28 - delay), delay);
            next = delay + 3;
        }
        delay = next;
    }
}

// ══════════════════════════════════════════════════════════════════════
// +0x180: the finder point's glow
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Row 0 (all clear), the ripple phase at 0 stepping 2, flags off.
// FF3 0x001c1af0.

void PhotoWrk180Init(PHOTO_WRK_180 *w)
{
    int i, g;

    w->wave.val   = 0;
    w->wave.delta = 2;
    for (i = 0; i < 4; i++)
    {
        for (g = 0; g < PHOTO180_COL_GRP; g++)
        {
            w->col[g][i].target = photo180_col_tbl[g][0][i];
            w->col[g][i].val    = photo180_col_tbl[g][0][i];
            w->col[g][i].speed  = 0;
        }
    }
    w->rate  = 0.0f;
    w->flags &= ~PHOTO180_FLG_ALL;
}

// ──────────────────────────────────────────────────────────────────────
// Flag setters (FF3 masks one bit of the first word).
// FF3 0x001c1bc0.

void PhotoWrk180SetBit0(PHOTO_WRK_180 *w, int on)
{
    w->flags = (u_char)((w->flags & ~PHOTO180_FLG_01) | (on & 1));
}

// FF3 0x001c1be0.

void PhotoWrk180SetBit1(PHOTO_WRK_180 *w, int on)
{
    w->flags = (u_char)((w->flags & ~PHOTO180_FLG_02) | ((on & 1) << 1));
}

// FF3 0x001c1c00.

void PhotoWrk180SetBit2(PHOTO_WRK_180 *w, int on)
{
    w->flags = (u_char)((w->flags & ~PHOTO180_FLG_04) | ((on & 1) << 2));
}

// FF3 0x001c1c40.

void PhotoWrk180SetBit4(PHOTO_WRK_180 *w, int on)
{
    w->flags = (u_char)((w->flags & ~PHOTO180_FLG_10) | ((on & 1) << 4));
}

// FF3 0x001c1c60.

void PhotoWrk180SetBit5(PHOTO_WRK_180 *w, int on)
{
    w->flags = (u_char)((w->flags & ~PHOTO180_FLG_20) | ((on & 1) << 5));
}

// ──────────────────────────────────────────────────────────────────────
// Picks the colour row from the flags, fades the three RGBA groups to it
// (at once: time 0), loops the ripple phase, and moves the rate 0.2 a frame
// toward 1 while an ordinary enemy is active (PLYR_FLG_000020), else toward 0.
// FF3 0x001c1c80.

void PhotoWrk180Main(PHOTO_WRK_180 *w)
{
    u_char f = w->flags;
    int row, i, g;

    if ((f & PHOTO180_FLG_20) != 0)
        row = 5;
    else if ((f & PHOTO180_FLG_10) != 0)
        row = 4;
    else if ((f & PHOTO180_FLG_08) != 0)
        row = 1;
    else if ((f & (PHOTO180_FLG_01 | PHOTO180_FLG_02)) != 0)
        row = 2;
    else
        row = (f & PHOTO180_FLG_04) != 0;

    w->wave.LoopWork();

    for (i = 0; i < 4; i++)
    {
        for (g = 0; g < PHOTO180_COL_GRP; g++)
        {
            u_char c = photo180_col_tbl[g][row][i];

            if ((u_short)w->col[g][i].target != c)
                w->col[g][i].Fade(c, 0);
        }
        for (g = 0; g < PHOTO180_COL_GRP; g++)
            w->col[g][i].Work();
    }

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_000020) == 0)
        w->rate = w->rate - 0.2f;
    else
        w->rate = w->rate + 0.2f;

    if (w->rate < 0.0f)
        w->rate = 0.0f;
    else if (1.0f < w->rate)
        w->rate = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// At the finder point: sprite 0 in the fixed colour, sprite 1 in group 1,
// sprite 0 again in group 0, and four ripple rings of sprite 0 in group 2
// at the looping phase. Alphas scale by alpha / 128.
// FF3 0x001c1e60.

void PhotoWrk180Draw(float x, float y, PHOTO_WRK_180 *w, int alpha, int real)
{
    int   rl = real != 0;
    float t  = (float)w->wave.val / 100.0f;

    HudPointGlowDraw(x, y, 0.0f, 1.0f, 1.0f, 0,
                 photo180_base_col[0], photo180_base_col[1], photo180_base_col[2],
                 (u_char)((photo180_base_col[3] * alpha) / 128), 1, rl);
    HudPointGlowDraw(x, y, 0.0f, 1.0f, 1.0f, 1,
                 (u_char)w->col[1][0].val, (u_char)w->col[1][1].val, (u_char)w->col[1][2].val,
                 (u_char)((w->col[1][3].val * alpha) / 128), 0, rl);
    HudPointGlowDraw(x, y, 0.0f, 1.0f, 1.0f, 0,
                 (u_char)w->col[0][0].val, (u_char)w->col[0][1].val, (u_char)w->col[0][2].val,
                 (u_char)((w->col[0][3].val * alpha) / 128), 1, rl);
    HudPointRingsDraw(x, y, 0.16f, t, 0.0f, 1.0f, 1.0f, 0, 4,
                 (u_char)w->col[2][0].val, (u_char)w->col[2][1].val, (u_char)w->col[2][2].val,
                 (u_char)((w->col[2][3].val * alpha) / 128), 1, rl);
}

// ══════════════════════════════════════════════════════════════════════
// +0x1e8: the spirit charge
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Charges, steps and timers to 0 (mode, the SE and the display stay).
// FF3 0x001c2048.

void PHOTO_WRK_1E8::Init()
{
    ramp        = 0.0f;
    num         = 0;
    charge      = 0;
    num2        = 0;
    charge2     = 0;
    charge2_old = 0;
    pct         = 0;
    fade.val    = 0;
    fade.delta  = 0;
    flash       = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2078.

void PHOTO_WRK_1E8::Reset()
{
    mode      = 0;
    se_id     = -1;
    disp      = 0.0f;
    disp_hold = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Fills the charge display for the current player (player 2, Kei, shows
// only whether a step is full) and draws it at the finder point: the
// steps scaled by the +0x180 rate, the full-charge glow while the +0x180
// rate is up, the step-filled flash ring, the gauge at the fade's alpha,
// and player 1's second charge ring with equipment 5 in mode != 0.
// FF3 0x001c2090.

void PHOTO_WRK_1E8::Draw(float x, float y, int alpha)
{
    PHOTO_CHARGE_DISP *d = &photo_charge_disp;
    s_char chr  = (s_char)PlyrGetCurNo();
    float  rate = GetPhotoModeWrk180()->rate;
    short  r    = GetPhotoModeWrk180()->col[0][0].val;
    short  g    = GetPhotoModeWrk180()->col[0][1].val;
    short  b    = GetPhotoModeWrk180()->col[0][2].val;
    float  a    = (float)alpha * 0.0078125f * (float)fade.val / 60.0f;

    memset(d, 0, sizeof(*d));
    if (chr == 2)
    {
        d->max     = chr;
        d->num_lit = (s_char)((num != 0) << 1);
        d->frac    = (float)(charge % PHOTO_CHARGE_STEP) / 600.0f;
        d->flash   = (0.0f < flash) ? 1.0f - flash : 0.0f;
        d->chr     = 2;
        d->charging = (GetPhotoModeWrk180()->flags & (PHOTO180_FLG_10 | PHOTO180_FLG_20)) == PHOTO180_FLG_10;
        d->level   = (s_char)PlyrGetCamLvCharge(2);
        d->ramp    = 0.0f;
    }
    else
    {
        d->max       = (s_char)PlyrGetChargeMax(0xff, 0xff);
        d->num_lit   = (s_char)num;
        d->num       = (s_char)num;
        d->frac      = (float)(charge % PHOTO_CHARGE_STEP) / 600.0f;
        d->num2      = (s_char)num2;
        d->num_round = (s_char)(int)((float)charge / 600.0f + 0.5f);
        d->frac2     = (float)(charge2 % PHOTO_CHARGE_STEP) / 600.0f;
        d->flash     = (0.0f < flash) ? 1.0f - flash : 0.0f;
        d->chr       = chr;
        d->charging  = (GetPhotoModeWrk180()->flags & (PHOTO180_FLG_10 | PHOTO180_FLG_20)) == PHOTO180_FLG_10;
        d->level     = (s_char)PlyrGetCamLvCharge((u_char)chr);
        d->ramp      = ramp;
    }

    HudChargeStepsDraw(x, y, rate * 0.7f, d->ramp, 1.0f, 1.0f, d->max, d->chr);

    if (IsMax() && 0.0f < GetPhotoModeWrk180()->rate)
        HudChargeFullDraw(x, y, (float)alpha * 0.5f * 0.0078125f, d->ramp, 1.0f, 1.0f,
                     (u_char)r, (u_char)g, (u_char)b);

    if (0.0f < d->flash)
        HudPointRingsDraw(x, y, 0.2f, d->flash, 0.0f, 1.0f, 1.0f, 2, 1,
                     (u_char)r, (u_char)g, (u_char)b, (u_char)(alpha / 2), 0, 0);

    HudChargeGaugeDraw(x, y, a, 1.0f, 1.0f, d);

    if (chr == 1 && PlyrGetEquipLv(5) != 0 && mode != 0)
        HudCharge2Draw(x, y, a, 1.0f, 1.0f, d);
}

// ──────────────────────────────────────────────────────────────────────
// The viewfinder camera's (finder_view.c) and cat_ctrl.c's term.
// FF3 0x001c2550.

float PHOTO_WRK_1E8::GetDispRate()
{
    return disp + 0.85f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2568.

void PHOTO_WRK_1E8::FadeIn()
{
    fade.delta = GetPALMode() ? 0x10 : 0x14;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c25a0.

void PHOTO_WRK_1E8::FadeOut()
{
    fade.delta = GetPALMode() ? -0x10 : -0x14;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c25e8.

int PHOTO_WRK_1E8::GetNum()
{
    return num;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c25f0.

int PHOTO_WRK_1E8::GetNumSub()
{
    return num2;
}

// ──────────────────────────────────────────────────────────────────────
// One charged enemy's share for this frame (0x23d178): speed x the rates
// x k, at the frame rate; none while EneAnyDmgReactBig is on.
// FF3 0x001c25f8.

void PHOTO_WRK_1E8::Add(float dist_rate, float rad_rate, float step_rate, float k, int speed)
{
    int add;

    if (EneAnyDmgReactBig() != 0)
        return;

    add = (int)((float)speed * dist_rate * rad_rate * step_rate * k * GetFrameTimeScale());
    if (mode == 0)
        charge += add;
    else
        charge2 += add;
}

// ──────────────────────────────────────────────────────────────────────
// Nothing charged this frame: the charge drains by the player's rate
// (truncated to a byte), floor 0; none while EneAnyDmgReactBig is on. chr is
// passed by the caller and unused.
// FF3 0x001c26b8.

void PHOTO_WRK_1E8::Drain(u_char chr)
{
    u_int sub;

    (void)chr;
    if (EneAnyDmgReactBig() != 0)
        return;

    sub = (u_int)(int)((float)PlyrGetChargeDrain() * GetFrameTimeScale()) & 0xff;
    if (mode == 0)
        charge = (charge <= (int)sub) ? 0 : charge - (int)sub;
    else
        charge2 = (charge2 <= (int)sub) ? 0 : charge2 - (int)sub;
}

// ──────────────────────────────────────────────────────────────────────
// Once a frame (0x23d178), max = PhotoWrk1e8GetMax: lens function 8 with the
// +0x180 rate up fills the charge (player 1: charge2 follows in mode != 0;
// players 0 / 2: charge2 0), else both are clamped to max and charge2 to
// charge. A new full step plays SE 10 (9 between hundreds of percent);
// charge2 reaching charge plays 0x1b7f000a. The display eases toward 0.3
// in a shutter chance (0.35 up, 0.2 down after a 1-frame hold), the flash
// starts when a step change lands on 100 %, the ramp follows the mode.
// FF3 0x001c2748.

void PHOTO_WRK_1E8::Main(int max)
{
    int   d1 = 0, d2 = 0;
    int   c1, c2, n1, n2, chance;
    float ratio, target, cur, diff, step;

    if (GetLensWrk()->GetFunc(8) != 0 && 0.0f < GetPhotoModeWrk180()->rate)
    {
        charge = max;
        if (PlyrGetCurNo() != 1)
        {
            charge2 = 0;
            c2 = 0;
        }
        else if (mode == 0)
        {
            c2 = charge2;
        }
        else
        {
            charge2 = charge;
            c2 = charge;
        }
        c1 = charge;
    }
    else
    {
        c1 = charge;
        if (max < c1)
        {
            charge = max;
            c1 = max;
        }
        c2 = charge2;
        if (c1 < c2)
        {
            charge2 = c1;
            c2 = c1;
        }
    }

    n1 = c1 / PHOTO_CHARGE_STEP;
    if (num != n1)
        d1 = (num < n1) ? 1 : -1;
    n2 = c2 / PHOTO_CHARGE_STEP;
    if (num2 != n2)
        d2 = (num2 < n2) ? 1 : -1;

    // FF3 multiplies the fill ratio by 0.0 here: only the shutter chance moves
    // the display.
    // PC: max 0 would divide by zero (the EE's div gives garbage, x86 faults).
    ratio = (max != 0) ? ((float)c1 / (float)max) * 0.0f : 0.0f;
    num  = n1;
    num2 = n2;
    pct  = (max != 0) ? ((c1 + c2) * 100) / max : 0;

    chance = ShutterChanceChk();
    target = (chance == 1 || chance == 2) ? ratio + 0.3f : ratio + 0.0f;

    cur = disp;
    if (cur == target)
    {
        disp_hold = 1.0f;
    }
    else if (cur < target)
    {
        diff = target - cur;
        step = diff * 0.35f;
        disp = (0.001f < diff - step) ? cur + step : target;
        disp_hold = 1.0f;
    }
    else if (0.0f < disp_hold)
    {
        disp_hold = disp_hold - 1.0f;
    }
    else
    {
        diff = cur - target;
        step = diff * 0.2f;
        disp = (0.001f < diff - step) ? cur - step : target;
    }

    if (0 < d1)
        PhotoModeSePlay((pct % 100 != 0) ? 9 : 10, 100, 0x1000);

    if (charge2_old < charge && charge <= charge2)
        SndSePlay(0x1b7f000a, 100, 0x1000, 0, nullptr, 0);
    charge2_old = charge2;

    fade.Work();

    if ((0 < d1 || 0 < d2) && pct == 100)
        flash = 1.0f;

    flash = flash - 0x1.555554p-4f;          // 0x3707b4, a hair under 1/12
    if (flash <= 0.0f)
        flash = 0.0f;

    if (mode == 0)
    {
        ramp = 0.0f;
    }
    else
    {
        ramp = ramp + 0x1.999998p-3f;        // 0x3707b8, a hair under 0.2
        if (1.0f < ramp)
            ramp = 1.0f;
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1.2 nearer than 20, 1.0 down to 0.5 over 20..the shot distance, 0.5 past it.
// FF3 0x001c2b38.

float PHOTO_WRK_1E8::DistRate(float dist, u_char chr, u_char lv)
{
    float range = PlyrGetShotRange(chr, lv);
    float r     = 1.2f;

    (void)this;
    if (20.0f <= dist)
    {
        r = 0.5f;
        if (dist <= range)
            r = (1.0f - (dist - 20.0f) / (range - 20.0f)) * 0.5f + 0.5f;
    }
    return r;
}

// ──────────────────────────────────────────────────────────────────────
// 1.2 at the finder point down to 0.8 at the target radius, 0.8 outside.
// FF3 0x001c2be0.

float PHOTO_WRK_1E8::RadiusRate(float rad, u_char chr, u_char lv)
{
    float lim = PlyrGetFinderRadius(chr, lv);
    float r   = 0.8f;

    (void)this;
    if (rad <= lim)
        r = (1.0f - rad / lim) * 0x1.99999cp-2f + 0.8f;   // 0x3707d4, a hair over 0.4
    return r;
}

// ──────────────────────────────────────────────────────────────────────
// The player's rate for the fifth of max the running charge is in (0..4).
// chr is passed by the caller and unused.
// FF3 0x001c2c68.

float PHOTO_WRK_1E8::StepRate(u_char chr, int max)
{
    int c = (mode == 0) ? charge : charge2;
    int s = ((c * 100) / max) / 20;     // FF3 traps (break 7) on max 0

    (void)chr;
    return PlyrGetChargeStepRate((u_char)((s < 5) ? s : 4));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2cc0.

int PHOTO_WRK_1E8::GetMaxNum(u_char chr, u_char lv)
{
    (void)this;
    return PlyrGetChargeMax(chr, lv);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2ce8.

int PHOTO_WRK_1E8::GetMax(u_char chr, u_char lv)
{
    return GetMaxNum(chr, lv) * PHOTO_CHARGE_STEP;
}

// ──────────────────────────────────────────────────────────────────────
// No callers.
// FF3 0x001c2d10.

float PHOTO_WRK_1E8::GetRange(u_char chr, u_char lv)
{
    (void)this;
    return PlyrGetShotRange(chr, lv);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2d38.

int PHOTO_WRK_1E8::GetSpeed(u_char chr, u_char lv)
{
    (void)this;
    return PlyrGetChargeSpeed(chr, lv);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2d60.

int PHOTO_WRK_1E8::GetSpeedEquip(u_char lv)
{
    (void)this;
    return PlyrGetChargeSpeedEquip(lv);
}
