/* ==========================================================================
 *  ingame/photo/fndr_parts.c
 *
 *  The camera HUD parts (FF3 0x1c2d80..0x1c67c0, plus the linkonce template
 *  instances 0x2b9d18..0x2ba3c8): the photo mode work's sub-works -- damage
 *  number, film, filament, shutter charger, enemy markers, HP gauge, lenses
 *  and spirit stones, the camera shake, the effect-slot gauge, and three
 *  whose role is still open (+0x1e8's SE, +0x274, +0x4bc). See fndr_parts.h
 *  for the layouts and the FF2 widgets they come from. finder_photo.c runs
 *  them (PhotoModeWrkInit / Main / Draw). FF3's global constructor 0x1c4050
 *  ends an object file at 0x1c4070 (shake onwards is a second file).
 * ======================================================================== */
#include "ingame/photo/fndr_parts.h"
#include "common/reipan_stub.h"

#include <math.h>                           // fabsf
#include <string.h>                         // memset, memcpy

#include <libgraph.h>                       // SCE_GS_SET_ALPHA, SCE_GS_SET_ZBUF

#include "common/heapctrl.h"                // heapCtrlMalloc, heapCtrlFree
#include "graphics/effect/effect_ene.h"     // EneHitEffectReq
#include "ingame/enemy/ene_main.h"         // ene_wrk, IsActEnemy, ShutterChanceChk, EneGetGaugeTarget
#include "graphics/graph2d/g2d_draw.h"      // DISP_SQAR, CopySqrDToSqr, DispSqrD
#include "graphics/graph3d/gra3d.h"         // the camera (PhotoShakeWork)
#include "ingame/ingame_wrk.h"              // ingame_wrk
#include "ingame/photo/finder_photo.h"      // PHOTO_WRK_1E8, GetPhotoModeWrk180, PhotoModeSePlay
#include "ingame/photo/finder_view_hud.h"   // the HUD draws
#include "ingame/plyr/player.h"             // PlyrGetCurNo, PlyrGetEquipLv
#include "ingame/plyr/plyr_param.h"         // PlyrGetChargeMax, PlyrGetChargeScale
#include "ingame/sound/snd_ctrl.h"          // SndSe*
#include "system/os/sys_heap.h"             // GetSystemHeapWrkP
#include "system/os/system.h"               // GetPALMode
#include "system/pad/pad_ctrl.h"            // CheckPadTrgPlaySe, pad_key
#include "ingame/item/prg/item.h"      // ItemGetCnt, ITEM_NUM

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@001ef370 (libvu0 helpers)
float NormalizeAngle(float fAngle);
float Vu0Cosf(float x);

// ──────────────────────────────────────────────────────────────────────

#define CHR_CUR         0xff            // chr / grade argument: the current player's
#define CHR_NO_LENS     1               // player 1 has no lenses

// data 0x3597e0: the item number of each film type (read as u_short).
static const int film_item_no[FILM_TYPE_NUM] = { 0, 1, 2, 3, 4 };


// data 0x359828: the lenses.
static LENS_DAT lens_dat[LENS_NUM] =
{
    { 2, {}, { 1.2f, 1.4f, 1.6f, 1.8f },   { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 3, {}, { 1.4f, 1.8f, 2.2f, 2.6f },   { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 4, {}, { 1.6f, 2.2f, 2.8f, 3.4f },   { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 1, {}, { 1.25f, 1.5f, 1.75f, 2.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 2, {}, { 1.0f, 1.1f, 1.3f, 1.5f },   { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 2, {}, { 0.0f, 0.0f, 0.0f, 0.0f },   { 3.0f, 5.0f, 7.0f, 9.0f } },
    { 1, {}, { 0.0f, 0.0f, 0.0f, 0.0f },   { 3.0f, 5.0f, 7.0f, 9.0f } },
    { 2, {}, { 1.1f, 1.16f, 1.23f, 1.3f }, { 4.0f, 6.0f, 8.0f, 10.0f } },
    { 1, {}, { 0.6f, 0.8f, 1.0f, 1.2f },   { 0.0f, 0.0f, 0.0f, 0.0f } },
};

// data 0x359990: spirit-stone capacity per player and grade (words, read
// as bytes).
static const int lens_stone_max[LENS_CHR_NUM][4] =
{
    { 2, 3, 4, 5 },
    { 0, 0, 0, 0 },
    { 1, 3, 5, 7 },
};

// data 0x359970: lens 8's damage scale by the stones charged (LensGetStoneRate).
static const float lens_stone_rate[8] = { 0.0f, 0.8f, 1.0f, 1.2f, 2.0f, 3.2f, 4.6f, 6.5f };

// data 0x3599c0: a scale per lens level (LensGetLevelRate).
static const float lens_level_rate[LENS_LEVEL_MAX + 1] = { 1.1f, 1.2f, 1.3f, 1.5f };

// data 0x3597f8: the film type box colours (FilmDrawTypeBox; byte 0 unread).
struct FILM_BOX_COL
{
    u_char _0;
    u_char r;
    u_char g;
    u_char b;
};
static_assert(sizeof(FILM_BOX_COL) == 0x4, "FILM_BOX_COL is 4 bytes");
static_assert(offsetof(FILM_BOX_COL, r) == 0x1, "FILM_BOX_COL.r");
static const FILM_BOX_COL film_box_col[FILM_TYPE_NUM] =
{
    { 0x07, 0x3c, 0x32, 0x32 },
    { 0x0e, 0x20, 0x20, 0x80 },
    { 0x3d, 0x18, 0x50, 0x18 },
    { 0x5a, 0x50, 0x10, 0x10 },
    { 0x00, 0x50, 0x50, 0x18 },
};

// data 0x3273e0: the camera shake's height offsets (/ 25, one per 3 frames,
// ends on 0).
static const float photo_shake_y[] =
{
    14.0f, -48.0f, 42.0f, -34.0f, 27.0f, -21.0f, 15.0f, -10.0f, -6.0f, 3.4f, -1.6f, -0.5f, 0.1f, 0.0f,
};

// data 0x327418: the camera shake's field-of-view offsets (one per 5 frames,
// ends on 0).
static const float photo_shake_fov_ofs[] = { -0.3f, 0.15f, 0.0f };

// The filament's four flicker switches (FF2 filament.o's file-scope
// mBlinkAlphaCore1/2, mBlinkAlphaWhole1/2; zero in the ROM image,
// FilamentInit seeds them).
static CBlinkSwitchVariable<s_char, 90, 118,  6, 90> fil_blink_core1;    // sdata 0x370828
static CBlinkSwitchVariable<s_char, 90, 112, 11, 90> fil_blink_core2;    // sdata 0x370830
static CBlinkSwitchVariable<s_char, 90, 116, 13, 90> fil_blink_whole1;   // sdata 0x370838
static CBlinkSwitchVariable<s_char, 75, 112, 17, 75> fil_blink_whole2;   // sdata 0x370840

#define FIL_BLINK_BIAS  115             // added to each blink value (FF2 filament.c)

int   photo_shake_state;                // bss 0x3e9c30
int   photo_shake_cnt;                  // bss 0x3e9c34
int   photo_shake_state2;               // bss 0x3e9c38
int   photo_shake_cnt2;                 // bss 0x3e9c3c
float photo_shake_fov;                  // bss 0x3e9c40

// ══════════════════════════════════════════════════════════════════════
// Template instances (linkonce, 0x2b9d18..)
// ══════════════════════════════════════════════════════════════════════

// FF2 CBlinkSwitchVariable::Work (common/variable.h): the body every
// instance's Work() below runs (FF3 emitted one copy per instance).
template <typename T, int MIN, int MAX, int TIME, int INIT>
static void BlinkSwitchWork(CBlinkSwitchVariable<T, MIN, MAX, TIME, INIT> *b)
{
    int    v = b->val;
    u_char f = b->flags;

    if ((f & BLINK_SW_ON) == 0)
    {
        v -= (MAX - MIN) / TIME;
        if (v <= INIT)
            v = INIT;
    }
    else if ((f & BLINK_SW_UP) == 0)
    {
        v -= (MAX - MIN) / TIME;
        if (v <= MIN)
        {
            b->flags = f | BLINK_SW_UP;
            v        = MIN;
        }
    }
    else
    {
        v += (MAX - MIN) / TIME;
        if (v >= MAX)
        {
            b->flags = f & ~BLINK_SW_UP;
            v        = MAX;
        }
    }
    b->val = (T)v;
}

// ──────────────────────────────────────────────────────────────────────
// Delay, rise to 127, hold until the wait ends (unless 'hold'), fall to the
// floor and stop.
// FF3 0x002b9d18.

template <>
void CPulseVariable<0>::Work()
{
    int val;

    if (speed == 0)
        return;

    if (on != 0)
    {
        if (hold == 0 && wait.Work())
            on = 0;
        return;
    }

    if (delay > 0)
    {
        delay--;
        return;
    }

    val = this->val + speed;
    if (val >= 127 && speed > 0)
    {
        val   = 127;
        speed = -speed;
        on    = 1;
    }
    if (val <= 0 && speed < 0)
    {
        speed = 0;
        val   = 0;
    }
    this->val = (s_char)val;
}

// ──────────────────────────────────────────────────────────────────────
// Arms the pulse: up in 'time' frames after 'delay'; the wait is kept if
// still running.
// FF3 0x002b9dd0.

template <>
void CPulseVariable<0>::Req(s_char time, short wait, int delay)
{
    this->delay = delay;
    on          = 0;
    hold        = 0;
    if (this->wait.val == 0)
        this->wait.val = wait;
    if (time != 0)
        speed = (s_char)((127 - val) / time);
}

// ──────────────────────────────────────────────────────────────────────
// Counts down; 1 only on the frame it reaches 0. FF2 CWaitVariable::Work.
// FF3 0x002b9e28.

template <>
int CWaitVariable<short>::Work()
{
    short next = val - 1;

    if (val != 0)
    {
        val = next;
        if (next == 0)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// val += delta; past a bound it restarts from the other one and returns 1.
// FF2 CWrkVariable::LoopWork (fly_ctrl.c's instance).
// FF3 0x002b9e60.

template <>
int CWrkVariable<s_char, 0, 100>::LoopWork()
{
    int val = this->val + delta;

    if (val > 100)
    {
        this->val = 0;
        return 1;
    }
    if (val < 0)
    {
        this->val = 100;
        return 1;
    }
    this->val = (s_char)val;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Steps and stops dead on the target. FF2 CFadeVariable::Work.
// FF3 0x002b9ea0.

template <>
void CFadeVariable<short>::Work()
{
    val = val + speed;

    if (target < val && speed > 0)
    {
        val   = target;
        speed = 0;
    }
    if (val < target && speed < 0)
    {
        val   = target;
        speed = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Aims at target over time frames (0: at once). FF2 CFadeVariable::Fade.
// FF3 0x002b9f18.

template <>
void CFadeVariable<short>::Fade(short target, short time)
{
    if (time == 0)
        speed = 0;
    else
        speed = (short)((target - val) / time);

    if (speed == 0)
        val = target;

    this->target = target;
}

// ──────────────────────────────────────────────────────────────────────
// val += delta, clamped to 0..60. FF2 CWrkVariable::Work (fly_ctrl.c's
// instance).
// FF3 0x002b9f68.

template <>
void CWrkVariable<s_char, 0, 60>::Work()
{
    int val = this->val + delta;

    if (val > 60)
    {
        this->val = 60;
        return;
    }
    if (val < 0)
    {
        this->val = 0;
        return;
    }
    this->val = (s_char)val;
}

// ──────────────────────────────────────────────────────────────────────
// 'a' brought into (0, |b|] by subtracting |b|, with a's sign: 1.0 stays
// 1.0, 1.25 becomes 0.25. 'a' unchanged when a * b is 0.
// FF3 0x002b9fa0.

float FloatWrap(float a, float b)
{
    float sign = 1.0f;
    float step;

    if (a * b != 0.0f)
    {
        sign = (a >= 0.0f) ? 1.0f : -1.0f;
        step = fabsf(b);
        for (a = fabsf(a); step < a; a -= step)
            ;
    }
    return sign * a;
}

// ──────────────────────────────────────────────────────────────────────
// The film readout's blink switch (FF2 film_no.o's instance).
// FF3 0x002ba048.

template <>
void CBlinkSwitchVariable<s_char, 70, 90, 15, 40>::Work()
{
    BlinkSwitchWork(this);
}

// ──────────────────────────────────────────────────────────────────────
// val += delta, clamped to 0..0x80. FF2 CWrkVariable::Work.
// FF3 0x002ba0b8.

template <>
void CWrkVariable<short, 0, 128>::Work()
{
    int val = this->val + delta;

    if (val > 0x80)
    {
        this->val = 0x80;
        return;
    }
    if (val < 0)
    {
        this->val = 0;
        return;
    }
    this->val = (short)val;
}

// ──────────────────────────────────────────────────────────────────────
// The filament's four switches (FF2 filament.o's instances).
// FF3 0x002ba0f0.

template <>
void CBlinkSwitchVariable<s_char, 90, 118, 6, 90>::Work()
{
    BlinkSwitchWork(this);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba160.

template <>
void CBlinkSwitchVariable<s_char, 90, 112, 11, 90>::Work()
{
    BlinkSwitchWork(this);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba1d0.

template <>
void CBlinkSwitchVariable<s_char, 90, 116, 13, 90>::Work()
{
    BlinkSwitchWork(this);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba240.

template <>
void CBlinkSwitchVariable<s_char, 75, 112, 17, 75>::Work()
{
    BlinkSwitchWork(this);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba2b0.

template <>
void CPulseVariable<50>::Req(s_char time, short wait, int delay)
{
    this->delay = delay;
    on          = 0;
    hold        = 0;
    if (this->wait.val == 0)
        this->wait.val = wait;
    if (time != 0)
        speed = (s_char)((127 - val) / time);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba308.

template <>
void CPulseVariable<50>::Work()
{
    int val;

    if (speed == 0)
        return;

    if (on != 0)
    {
        if (hold == 0 && wait.Work())
            on = 0;
        return;
    }

    if (delay > 0)
    {
        delay--;
        return;
    }

    val = this->val + speed;
    if (val >= 127 && speed > 0)
    {
        val   = 127;
        speed = -speed;
        on    = 1;
    }
    if (val <= 50 && speed < 0)
    {
        speed = 0;
        val   = 50;
    }
    this->val = (s_char)val;
}

// ══════════════════════════════════════════════════════════════════════
// +0x1e8: the looping SE
// ══════════════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2d80.

int PHOTO_WRK_1E8::IsMax()
{
    return PlyrGetChargeMax(0xff, 0xff) == num;
}

// ──────────────────────────────────────────────────────────────────────
// Player 1 with PlyrGetEquipLv(5): while the camera is up, charge2 runs and
// the +0x180 sub-work's rate is positive, loops SE 0x1b7f0009 with its pitch
// and volume rising with charge2 as a percentage of PlyrGetChargeMax() * 600; else
// fades it out.
// FF3 0x001c2db8.

void PHOTO_WRK_1E8::SeMain()
{
    int pct, pitch, vol;

    if ((s_char)PlyrGetCurNo() != 1 || PlyrGetEquipLv(5) == 0)
        return;

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0 || charge2 == 0
        || !(GetPhotoModeWrk180()->rate > 0.0f))
    {
        SeStop();
        return;
    }

    pct   = (charge2 * 100) / (PlyrGetChargeMax(0xff, 0xff) * 600);
    pitch = pct * 0x28 + 0x1000;
    vol   = ((pct / 2) * 0x7f + 0x18ce) / 100;

    if (se_id != -1 && SndSeIsPlaying(se_id))
    {
        SndSeFadePitch(se_id, pitch, 10);
        SndSeVolFade(se_id, vol, 10);
        return;
    }
    se_id = SndSePlay(0x1b7f0009, vol, pitch, 10, nullptr, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2f40.

void PHOTO_WRK_1E8::SeStop()
{
    if (se_id != -1 && SndSeIsPlaying(se_id))
    {
        SndSeFadeStop(se_id, 10);
        se_id = -1;
    }
}

// ══════════════════════════════════════════════════════════════════════
// The damage number (FF2 CDamageDisp)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2f98.

void DamageDispInit(DAMAGE_DISP *d)
{
    d->pulse.wait.val = 0;
    d->pulse.val      = 0;
    d->pulse.speed    = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c2fa8.

void DamageDispWork(DAMAGE_DISP *d)
{
    d->pulse.Work();
}

// ──────────────────────────────────────────────────────────────────────
// Up in 5 (PAL) / 6 frames after 'delay', held 0x14 frames, then down.
// FF3 0x001c2fc0.

void DamageDispReq(DAMAGE_DISP *d, short damage, int delay)
{
    DamageDispInit(d);
    d->pulse.Req(GetPALMode() ? 5 : 6, 0x14, delay);
    d->damage = damage;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3028.

void DamageDispDraw(float x, float y, DAMAGE_DISP *d, int alpha)
{
    float a = (((float)alpha * (float)(int)d->pulse.val) / 127.0f) * 0.0078125f;

    if (a <= 0.0f)
        return;
    HudDamageNumDraw(x, y, a, d->damage);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c30a0.

int DamageDispIsOn(DAMAGE_DISP *d)
{
    return d->pulse.speed != 0;
}

// ══════════════════════════════════════════════════════════════════════
// +0x4bc: the falling gauge
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c30b0.

void PhotoGage4bcSnap(PHOTO_WRK_4BC *w)
{
    w->tail = w->now;
    w->disp = w->now;
}

// ──────────────────────────────────────────────────────────────────────
// 'disp' and 'tail' fall to 'now' by 0.05 a frame after their delays.
// FF3 0x001c30c0. (When 'tail' is not above 'now', FF3 stores 'now' into
// 'disp', not 'tail'; kept.)

void PhotoGage4bcWork(PHOTO_WRK_4BC *w)
{
    float now = w->now;
    float v;

    if (now < w->disp)
    {
        if (w->wait_disp > 0)
            w->wait_disp--;
        else
        {
            v       = w->disp - 0.05f;
            w->disp = v;
            if (v < now)
                w->disp = now;
        }
    }
    else
        w->disp = now;

    if (now < w->tail)
    {
        if (w->wait_tail > 0)
        {
            w->wait_tail--;
            return;
        }
        v       = w->tail - 0.05f;
        w->tail = v;
        if (v < now)
            w->tail = now;
    }
    else
        w->disp = now;
}

// ──────────────────────────────────────────────────────────────────────
// Drawn only during a lightning flash; 'disp' and 'tail' wrapped into
// (0, 1].
// FF3 0x001c3160.

void PhotoGage4bcDraw(float x, float y, PHOTO_WRK_4BC *w, int alpha)
{
    float disp;
    float tail;

    if (EneGetGaugeTarget() == nullptr)
        return;

    disp = FloatWrap(w->disp, 1.0f);
    tail = FloatWrap(w->tail, 1.0f);
    HudDrawGage4bc(x, y, (float)alpha * 0.0078125f, disp, tail);
}

// ──────────────────────────────────────────────────────────────────────
// A new target: 'disp' restarts from the old one and both fall after
// their delays.
// FF3 0x001c3230.

void PhotoGage4bcSet(PHOTO_WRK_4BC *w, float rate)
{
    float old = w->now;

    if (old != rate)
    {
        w->now       = rate;
        w->disp      = old;
        w->wait_tail = 0x23;
        w->wait_disp = 5;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3260.

void PhotoGage4bcSetNow(PHOTO_WRK_4BC *w, float rate)
{
    if (w->now != rate)
    {
        w->now  = rate;
        w->disp = rate;
        w->tail = rate;
    }
}

// ══════════════════════════════════════════════════════════════════════
// The film (FF2 CFilmNo)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3288.

void FilmInit(FILM_WRK *film)
{
    film->blink.val   = 40;
    film->type        = 1;
    film->blink.flags = (film->blink.flags & ~BLINK_SW_ON) | BLINK_SW_UP;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c32b0.

void FilmWork(FILM_WRK *film)
{
    film->blink.Work();
}

// ──────────────────────────────────────────────────────────────────────
// No callers in FF3.
// FF3 0x001c32c8.

void FilmBlinkOn(FILM_WRK *film)
{
    film->blink.flags |= BLINK_SW_ON;
}

// ──────────────────────────────────────────────────────────────────────
// No callers in FF3.
// FF3 0x001c32e0.

void FilmBlinkOff(FILM_WRK *film)
{
    film->blink.flags &= ~BLINK_SW_ON;
}

// ──────────────────────────────────────────────────────────────────────
// How many film types have shots left.
// FF3 0x001c32f8.

int FilmGetHaveTypeNum(void)
{
    int num = 0;

    for (u_int i = 0; i < FILM_TYPE_NUM; i++)
    {
        short have = 0;

        if ((u_short)film_item_no[i] < ITEM_NUM)
            have = *ItemGetCnt((short)film_item_no[i]);
        num += (have > 0);
    }
    return num;
}

// ──────────────────────────────────────────────────────────────────────
// The film type whose item is 'item' (the last match).
// FF3 0x001c3378.

void FilmSetByItem(FILM_WRK *film, int item)
{
    for (u_int i = 0; i < FILM_TYPE_NUM; i++)
    {
        if (film_item_no[i] == item)
            film->type = (u_char)i;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The next film type with shots left (the current one last), SE 0x10.
// FF3 0x001c33b8.

void FilmSelectNext(FILM_WRK *film)
{
    int found = -1;
    int i     = 0;

    for (;;)
    {
        int   type = (int)(film->type + i + 1) % FILM_TYPE_NUM;
        short num  = 0;

        if ((u_short)film_item_no[type] < ITEM_NUM)
            num = *ItemGetCnt((short)film_item_no[type]);
        i++;
        if (num != 0)
            found = type;
        if (i > 4 || found != -1)
            break;
    }

    if (found != -1)
    {
        film->type = (u_char)found;
        PhotoModeSePlay(0x10, 100, 0x1000);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The previous film type with shots left, SE 0x10.
// FF3 0x001c34c0.

void FilmSelectPrev(FILM_WRK *film)
{
    int found = -1;
    int i     = 0;

    for (;;)
    {
        int   type = (int)(film->type - i + 4) % FILM_TYPE_NUM;
        short num  = 0;

        if ((u_short)film_item_no[type] < ITEM_NUM)
            num = *ItemGetCnt((short)film_item_no[type]);
        i++;
        if (num != 0)
            found = type;
        if (i > 4 || found != -1)
            break;
    }

    if (found != -1)
    {
        film->type = (u_char)found;
        PhotoModeSePlay(0x10, 100, 0x1000);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c35c8.

int FilmGetType(FILM_WRK *film)
{
    return film->type;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c35d0.

int FilmGetNum(FILM_WRK *film)
{
    int    type = FilmGetType(film);
    u_short num = 0;

    if ((u_short)film_item_no[type] < ITEM_NUM)
        num = (u_short)*ItemGetCnt((short)film_item_no[type]);
    return num & 0xff;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3628.

void *FilmGetSaveData(FILM_WRK *film)
{
    return film;
}

// ──────────────────────────────────────────────────────────────────────
// A 40 x 30 box in the film type's colour at (x + 0x212, y + 0x17c),
// priority 0x10. Nothing in FF3 calls it.
// FF3 0x001c3630.

void FilmDrawTypeBox(float x, float y, FILM_WRK *film, int unused, u_char alpha)
{
    const FILM_BOX_COL *col = &film_box_col[film->type];
    u_char    r = col->r;
    u_char    g = col->g;
    u_char    b = col->b;
    SQAR_DAT  dat;
    DISP_SQAR sqr;

    (void)unused;

    // FF3 passes an unset record; zeroing it changes nothing below.
    memset(&dat, 0, sizeof(dat));
    CopySqrDToSqr(&sqr, &dat);

    sqr.pri    = 0x10;
    sqr.z      = 0xfffff - 0x10;
    sqr.zbuf   = SCE_GS_SET_ZBUF(0x118, SCE_GS_PSMZ16S & 0xf, 1);
    sqr.x[1]   = (int)x + 0x23a;
    sqr.x[0]   = (int)x + 0x212;
    sqr.y[2]   = (int)y + 0x19a;
    sqr.alphar = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0x80);
    sqr.y[0]   = (int)y + 0x17c;

    for (int i = 0; i < 4; i++)
    {
        sqr.r[i] = r;
        sqr.b[i] = b;
        sqr.g[i] = g;
    }

    sqr.x[2]  = sqr.x[0];
    sqr.x[3]  = sqr.x[1];
    sqr.y[1]  = sqr.y[0];
    sqr.y[3]  = sqr.y[2];
    sqr.alpha = alpha;
    DispSqrD(&sqr);
}

// ══════════════════════════════════════════════════════════════════════
// +0x274
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3798.

void PhotoWrk274Init(PHOTO_WRK_274 *w)
{
    w->se_cnt = 0;
    w->state  = 0;
    w->alpha  = 0;
    w->rot1   = 20.0f;
    w->rot0   = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Turns the two angles; while ShutterChanceChk() == 2 the alpha rises to 0x80
// and pulses 0x80..0x40 (SE 0xd every 15 frames), else it fades out.
// FF3 0x001c37c0.

void PhotoWrk274Work(PHOTO_WRK_274 *w)
{
    int on = ShutterChanceChk() == 2;
    float r;

    r       = w->rot0 + 10.0f;
    w->rot0 = r;
    if (r > 180.0f)
        w->rot0 = r - 360.0f;
    r       = w->rot1 - 12.0f;
    w->rot1 = r;
    if (r < -180.0f)
        w->rot1 = r + 360.0f;

    if (on)
    {
        if (w->se_cnt % 15 == 0)
            PhotoModeSePlay(0xd, 100, 0x1000);
        w->se_cnt++;
    }
    else
        w->se_cnt = 0;

    switch (w->state)
    {
    case 0:
        if (!on)
        {
            w->alpha = 0;
            return;
        }
        w->state = 1;
        // fall through
    case 1:
        if (!on)
        {
            w->state = 3;
            return;
        }
        w->alpha += 0x20;
        if (w->alpha < 0x80)
            return;
        w->state = 2;
        w->alpha = 0x80;
        return;

    case 2:
        if (!on)
        {
            w->state = 3;
            return;
        }
        w->alpha -= 0x20;
        if (w->alpha > 0x40)
            return;
        w->state = 1;
        w->alpha = 0x40;
        return;

    case 3:
        if (on)
        {
            w->state = 1;
            return;
        }
        w->alpha -= 5;
        if (w->alpha > 0)
            return;
        w->state = 0;
        w->alpha = 0;
        return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3988.

void PhotoWrk274Draw(float x, float y, PHOTO_WRK_274 *w, int alpha)
{
    HudWrk274Draw(x, y, 1.0f, (float)(w->alpha * alpha) * 0.0078125f * 0.0078125f);
}

// ══════════════════════════════════════════════════════════════════════
// The filament (FF2 CFilament)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c39d0; FF2 CFilament::Init (FrameReset inlined at the end).

void FilamentInit(FILAMENT_WRK *w)
{
    memset(w->rt_ev_wrk, 0, sizeof(w->rt_ev_wrk));
    w->lock             = 0;
    w->master_alp.val   = 0x80;
    w->master_alp.delta = 0;
    fil_blink_core1.InitOn();
    fil_blink_core2.InitOn();
    fil_blink_whole1.InitOn();
    fil_blink_whole2.InitOn();
    w->hint      = 0.0f;
    w->battle    = 0.0f;
    w->auto_rate = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// The master alpha rises to 0x80 (8 frames; PAL 7).
// FF3 0x001c3ab8.

void FilamentFadeIn(FILAMENT_WRK *w)
{
    w->master_alp.delta = GetPALMode() ? 0x13 : 0x10;
}

// ──────────────────────────────────────────────────────────────────────
// The master alpha falls to 0.
// FF3 0x001c3af0.

void FilamentFadeOut(FILAMENT_WRK *w)
{
    w->master_alp.delta = GetPALMode() ? -0x13 : -0x10;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3b28.

void FilamentSetBattle(FILAMENT_WRK *w, float rate)
{
    w->battle = rate;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3b30.

void FilamentSetHint(FILAMENT_WRK *w, float rate)
{
    w->hint = rate;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3b38.

void FilamentSetAuto(FILAMENT_WRK *w, float rate)
{
    w->auto_rate = rate;
}

// ──────────────────────────────────────────────────────────────────────
// The fade and the flicker step; the rate is battle, else auto, else hint
// (FF2 picked it in Draw).
// FF3 0x001c3b40.

void FilamentWork(FILAMENT_WRK *w)
{
    w->master_alp.Work();
    fil_blink_core1.Work();
    fil_blink_core2.Work();
    fil_blink_whole1.Work();
    fil_blink_whole2.Work();

    w->rate = 0.0f;
    if (w->battle != w->rate)
        w->rate = w->battle;
    else if (w->auto_rate != w->rate)
        w->rate = w->auto_rate;
    else if (w->hint != w->rate)
        w->rate = w->hint;
}

// ──────────────────────────────────────────────────────────────────────
// The needle's core and whole alphas: the rate and master fade scale the
// view alpha, each flickers with its two switches (+ 115, / 256). Player 2
// draws a brighter whole and an extra part.
// FF3 0x001c3bd0.

void FilamentDraw(float x, float y, FILAMENT_WRK *w, int alpha)
{
    const float k = 0.00390625f;        // 1 / 256
    int   type;
    float a, core, whole;

    if (w->lock != 0)
        return;

    type = 0;
    if (w->battle != 0.0f)
        type = 1;
    else if (w->auto_rate != 0.0f || w->hint != 0.0f)
        type = 2;

    if (!(w->rate > 0.0f))
        return;

    a    = (float)alpha * 0.0078125f * w->rate * (float)(int)w->master_alp.val * 0.0078125f;
    core = a * (float)(fil_blink_core1.val + FIL_BLINK_BIAS) * k
             * (float)(fil_blink_core2.val + FIL_BLINK_BIAS) * k;
    if (PlyrGetCurNo() == 2)
        whole = (a * 0.75f + 0.05f) * (float)(fil_blink_whole1.val + FIL_BLINK_BIAS) * k
              * (float)(fil_blink_whole2.val + FIL_BLINK_BIAS);
    else
        whole = a * (float)(fil_blink_whole1.val + FIL_BLINK_BIAS) * k
              * (float)(fil_blink_whole2.val + FIL_BLINK_BIAS);

    HudFilamentDraw(x, y, a, core, whole * k, type);
    if (PlyrGetCurNo() == 2)
        HudFilament2Draw(x, y, whole * k, type);
}

// ══════════════════════════════════════════════════════════════════════
// The shutter charger (FF2 CPhotoCharger)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// SE countdowns; when the charge runs out the flash pulse (up in 12 (PAL)
// / 15 frames) starts and the shutter is ready. real: the real camera,
// SE 0x12 at charge 7.
// FF3 0x001c3dd8.

void PhotoChargerWork(PHOTO_CHARGER *w, int real)
{
    if (real && w->charge.val == 7)
        PhotoModeSePlay(0x12, 100, 0x1000);

    if (w->se0_wait > 0)
    {
        if (w->se0_wait == 1)
            PhotoModeSePlay(0, 100, 0x1000);
        w->se0_wait--;
    }
    if (w->se7_wait > 0)
    {
        if (w->se7_wait == 1)
            PhotoModeSePlay(7, 100, 0x1000);
        w->se7_wait--;
    }

    if (w->charge.Work())
    {
        w->flash.Req(GetPALMode() ? 12 : 15, 1, 0);
        w->ready = 1;
    }
    w->flash.Work();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3ed8.

void PhotoChargerInit(PHOTO_CHARGER *w, short time)
{
    w->charge_time    = time;
    w->ready          = 0;
    w->flash.val      = 50;
    w->flash.speed    = 0;
    w->flash.wait.val = 0;
    if (w->charge.val == 0)
        w->charge.val = w->charge_time;
    w->se7_wait = 0;
    w->se0_wait = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3f10.

int PhotoChargerIsReady(PHOTO_CHARGER *w)
{
    return w->ready;
}

// ──────────────────────────────────────────────────────────────────────
// Finder mode: the loaded film's charge time, scaled per player.
// FF3 0x001c3f20.

void PhotoChargerSetup(PHOTO_CHARGER *w)
{
    int   time  = FilmGetChargeTime();
    float scale = PlyrGetChargeScale();

    PhotoChargerInit(w, (short)(int)((float)time * scale));
}

// ──────────────────────────────────────────────────────────────────────
// The charge time of each film type (sdata 0x370858, gp-0x2c98), set at
// start-up.

static u_char film_charge_time[FILM_TYPE_NUM];

// FF3 0x001c3f68.

u_char FilmGetChargeTime(void)
{
    return film_charge_time[FilmGetType(GetFilmWrk())];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c3f98.

void PhotoChargerSetSe0Wait(PHOTO_CHARGER *w, short wait)
{
    w->se0_wait = wait;
}

// ──────────────────────────────────────────────────────────────────────
// 25 / 50 frames in PAL, else 30 / 60. FF3 runs this before main, when the
// video mode is still InitSysWrk's NTSC, so the 60 Hz times stay on PAL
// too; the host's static initialisation does the same.
// FF3 0x001c3fa8 (__static_initialization_and_destruction_0), called with
// (1, 0xffff) by the global constructor 0x001c4050, which ends an FF3
// object file (see open-items).

static void FilmChargeStaticInit(int initialize, int priority)
{
    u_char t0, t1, t2, t3;

    if (priority != 0xffff || initialize != 1)
        return;

    t0 = GetPALMode() ? 25 : 30;
    t1 = GetPALMode() ? 25 : 30;
    t2 = GetPALMode() ? 25 : 30;
    t3 = GetPALMode() ? 25 : 30;
    film_charge_time[4] = GetPALMode() ? 50 : 60;
    film_charge_time[0] = t0;
    film_charge_time[1] = t1;
    film_charge_time[2] = t2;
    film_charge_time[3] = t3;
}

// layout: host -- an empty object standing for FF3's global constructor.
struct FILM_CHARGE_CTOR
{
    FILM_CHARGE_CTOR() { FilmChargeStaticInit(1, 0xffff); }    // FF3 0x001c4050
};
static FILM_CHARGE_CTOR film_charge_ctor;

// ══════════════════════════════════════════════════════════════════════
// The camera shake
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4070.

void PhotoShakeReset(void)
{
    photo_shake_state  = 0;
    photo_shake_state2 = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Starts the shake (both parts) unless one is running; keeps the field of
// view to come back to.
// FF3 0x001c4088.

void PhotoShakeReq(void)
{
    if (photo_shake_state != 0 || photo_shake_state2 != 0)
        return;

    photo_shake_state  = 1;
    photo_shake_state2 = 1;
    photo_shake_fov    = gra3dGetCamera()->fFov;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the camera position and target bob by photo_shake_y / 25, the
// field of view by photo_shake_fov_ofs, until each table's 0; then the
// camera is applied.
// FF3 0x001c40d8.

void PhotoShakeWork(void)
{
    sceVu0FVECTOR v;
    float        *p;
    float         ofs;

    if (photo_shake_state == 1)
    {
        photo_shake_cnt   = 0;
        photo_shake_state = 2;
    }
    if (photo_shake_state == 2)
    {
        p    = gra3dcamGetPosition();
        v[0] = p[0];
        v[2] = p[2];
        v[3] = 1.0f;
        v[1] = p[1] + photo_shake_y[photo_shake_cnt / 3] / 25.0f;
        gra3dcamSetPosition(v);

        p    = gra3dcamGetTarget();
        v[0] = p[0];
        v[2] = p[2];
        v[3] = 1.0f;
        v[1] = p[1] + photo_shake_y[photo_shake_cnt / 3] / 25.0f;
        gra3dcamSetTarget(v, 1);

        if (photo_shake_y[photo_shake_cnt / 3] == 0.0f)
            photo_shake_state = 0;
        else
            photo_shake_cnt++;
    }

    if (photo_shake_state2 == 1)
    {
        photo_shake_cnt2   = 0;
        photo_shake_state2 = 2;
    }
    if (photo_shake_state2 == 2)
    {
        ofs                     = photo_shake_fov_ofs[photo_shake_cnt2 / 5];
        gra3dGetCamera()->fFov  = photo_shake_fov + ofs;
        if (ofs == 0.0f)
        {
            gra3dGetCamera()->fFov = photo_shake_fov;
            photo_shake_state2     = 0;
        }
        else
            photo_shake_cnt2++;
        gra3dcamSetFov(gra3dGetCamera()->fFov);
    }

    gra3dApplyCamera(NULL, 1, 1);
}

// ══════════════════════════════════════════════════════════════════════
// The effect slot gauge (ENE_WRK +0x5a4)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c43d0.

void EneGaugeInit(ENE_GAUGE *g)
{
    g->dec       = 0.0f;
    g->wait      = 0;
    g->val       = 0.0f;
    g->wait_time = GetPALMode() ? 175 : 210;
    g->max       = 100.0f;
    g->_08       = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Below full and above 0: waits, then drains by 'dec' to 0.
// FF3 0x001c4440.

void EneGaugeWork(ENE_GAUGE *g)
{
    float val;

    if (!(EneGaugeGetRate(g) < 100.0f))
        return;
    val = g->val;
    if (val <= 0.0f)
        return;

    if (g->wait > 0)
    {
        g->wait--;
        return;
    }
    g->val = (g->dec < val) ? val - g->dec : 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4508.

float EneGaugeGetRate(ENE_GAUGE *g)
{
    return (g->val / g->max) * 100.0f;
}

// ══════════════════════════════════════════════════════════════════════
// The enemy markers
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The slot's marker parked on the finder point.
// FF3 0x001c4530.

void EneMarkSlotInit(PHOTO_ENE_MARK *mark, ENE_WRK *slot)
{
    PHOTO_ENE_MARK *m  = &mark[slot->alg.idx];
    short           fx = (short)(int)ingame_wrk.plyr_wrk->finder_x;
    short           fy;

    m->from_x   = fx;
    m->cur_x    = fx;
    m->tracking = 0;
    m->step     = 0;
    fy          = (short)(int)ingame_wrk.plyr_wrk->finder_y;
    m->from_y   = fy;
    m->cur_y    = fy;
    slot->mark_x = (short)(int)ingame_wrk.plyr_wrk->finder_x;
    slot->mark_y = (short)(int)ingame_wrk.plyr_wrk->finder_y;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c45b0.

void EneMarkInit(PHOTO_ENE_MARK *mark)
{
    for (int i = 0; i < ENE_WRK_MAX; i++)
        EneMarkSlotInit(mark, &ene_wrk[i]);
}

// The slot's target pointer (ENE_WRK.tpos) as the marker keeps it: the
// EE stores the 32-bit pointer; PC keeps its low 32 bits, enough to see a
// change.
static int EneMarkId(const ENE_WRK *slot)
{
    return (int)(uintptr_t)slot->tpos;
}

// ──────────────────────────────────────────────────────────────────────
// Each marker heads for its slot's screen point while the slot is on
// screen (flags 0x80, not flags2 0x800), else for the finder point; a new
// target (tracking change, slot id change) restarts the 10-step move.
// FF3 0x001c4608. (Returns 10 in a register; nothing reads it.)

void EneMarkWork(PHOTO_ENE_MARK *mark)
{
    for (int i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK   *slot = &ene_wrk[i];
        PHOTO_ENE_MARK *m    = &mark[i];
        int             step, next;

        if ((slot->flags & 0x80) == 0 || (slot->flags2 & 0x800) != 0)
        {
            if (m->tracking == 1)
            {
                m->from_x   = m->cur_x;
                m->from_y   = m->cur_y;
                m->step     = 0;
                m->tracking = 0;
            }
            m->to_x = (short)(int)ingame_wrk.plyr_wrk->finder_x;
            m->to_y = (short)(int)ingame_wrk.plyr_wrk->finder_y;
        }
        else
        {
            if (m->tracking == 0)
            {
                m->from_x   = m->cur_x;
                m->from_y   = m->cur_y;
                m->step     = 0;
                m->tracking = 1;
            }
            if (m->id != EneMarkId(slot))
            {
                m->from_x = m->cur_x;
                m->from_y = m->cur_y;
                m->step   = 0;
            }
            m->to_x = (short)(int)slot->scr_x;
            m->to_y = (short)(int)slot->scr_y;
        }

        step = m->step;
        next = step + 1;
        if (next > 10)
            next = 10;
        m->cur_x = m->from_x + (short)(((m->to_x - m->from_x) * step) / 10);
        m->step  = next;
        m->cur_y = m->from_y + (short)(((m->to_y - m->from_y) * step) / 10);
        m->id    = EneMarkId(slot);
        slot->mark_x = m->cur_x;
        slot->mark_y = m->cur_y;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Each marker fades in (up to the view alpha) while IsActEnemy(slot)
// says so, else out, by 6 (PAL) / 8 a frame, and draws while it shows;
// with none showing, the centre marker at the finder point; then the
// centre frame.
// FF3 0x001c4798.

void EneMarkDraw(float x, float y, PHOTO_ENE_MARK *mark, int alpha, int real)
{
    static u_char inited;               // sbss 0x373280
    static int    step;                 // sbss 0x373288

    const float k = 0.0078125f;
    float       a = (float)alpha * k;
    int         n = 0;

    if (inited == 0)
    {
        step   = GetPALMode() ? 6 : 8;
        inited = 1;
    }

    for (int i = 0; i < ENE_WRK_MAX; i++)
    {
        PHOTO_ENE_MARK *m = &mark[i];
        int             v;

        if ((ene_wrk[i].flags2 & 0x800) != 0)
            continue;

        short cx = m->cur_x;
        float cy = (float)(int)m->cur_y;

        if (IsActEnemy(i) == 0)
        {
            v = m->alpha - step;
            if (v < 0)
                v = 0;
        }
        else
        {
            v = m->alpha + step;
            if (v >= alpha)
                v = alpha;
        }
        m->alpha = (u_char)v;

        if (m->alpha != 0)
        {
            n++;
            HudEneMarkDraw((float)(int)cx, cy, (float)m->alpha * k, 1.0f, 1.0f, real != 0);
        }
    }

    if (n == 0)
        HudEneMarkDraw(x, y, a, 1.0f, 1.0f, real != 0);
    HudEneMarkCenterDraw(x, y, a, 1.0f, 1.0f, real != 0);
}

// ══════════════════════════════════════════════════════════════════════
// The HP gauge
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// 'disp' falls to 'now' by 0.04 after its delay or rises by 0.02; 'tail'
// falls by 0.02 after its delay or jumps up to 'disp'. The pulse phase runs
// while 0 < hp <= hp_max / 5.
// FF3 0x001c4990.

void HudHpWork(HUD_HP_WRK *hp)
{
    float     now = hp->now;
    float     v;
    PLYR_WRK *plyr;

    if (now < hp->disp)
    {
        if (hp->wait_disp > 0)
            hp->wait_disp--;
        else
        {
            v        = hp->disp - 0.04f;
            hp->disp = v;
            if (v < now)
                hp->disp = now;
        }
    }
    else
    {
        v = hp->disp + 0.02f;
        if (!(v < now))
            v = now;
        hp->disp = v;
    }

    if (now < hp->tail)
    {
        if (hp->wait_tail > 0)
            hp->wait_tail--;
        else
        {
            v        = hp->tail - 0.02f;
            hp->tail = v;
            if (v < now)
                hp->tail = now;
        }
    }
    else if (hp->tail < hp->disp)
        hp->tail = hp->disp;

    plyr = ingame_wrk.plyr_wrk;
    if (plyr->hp < 1 || plyr->hp_max / 5 < plyr->hp)
        hp->phase = 0.0f;
    else
        hp->phase = NormalizeAngle(hp->phase + 0.31415927f);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4ac8.

void HudHpDraw(float x, float y, HUD_HP_WRK *hp, int alpha)
{
    if (ingame_wrk.b03 != 0)
        return;

    float a = (float)alpha * 0.0078125f;
    HudHpGaugeDraw(x, y, a, hp->disp, hp->tail, Vu0Cosf(hp->phase) * 0.25f + 0.75f);
}

// ══════════════════════════════════════════════════════════════════════
// The lenses and spirit stones
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// A new HP rate: the trail waits 0x23 frames; when 'disp' sat on the old
// rate it waits 2.
// FF3 0x001c4b88.

void HudHpSet(HUD_HP_WRK *hp, float rate)
{
    if (hp->now == rate)
        return;

    hp->wait_tail = 0x23;
    if (hp->now == hp->disp)
        hp->wait_disp = 2;
    hp->now = rate;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4bc0.

void HudHpSetNow(HUD_HP_WRK *hp, float rate)
{
    if (hp->now == rate)
        return;

    hp->now  = rate;
    hp->disp = rate;
    hp->tail = rate;
}

// ──────────────────────────────────────────────────────────────────────
// One stone icon: appears, flashes while spinning, spins; vanishes when
// the stone is gone.
// FF3 0x001c4be8.

static void LensStoneWork(LENS_ICON *ic, int on)
{
    float r;

    if (!on && ic->state == LENS_ICON_ON)
    {
        ic->state = LENS_ICON_VANISH;
        ic->t     = 1.0f;
    }

    switch (ic->state)
    {
    case LENS_ICON_OFF:
        if (on)
        {
            ic->t     = 0.0f;
            ic->state = LENS_ICON_APPEAR;
        }
        break;

    case LENS_ICON_APPEAR:
        ic->t += 0.19999999f;
        if (ic->t >= 1.0f)
        {
            ic->state = LENS_ICON_FLASH;
            ic->rot   = 0.0f;
            ic->t     = 0.0f;
        }
        break;

    case LENS_ICON_FLASH:
        r       = ic->rot + 6.0f;
        ic->rot = r;
        ic->t  += 0.099999994f;
        if (r > 360.0f)
            ic->rot = r - 360.0f;
        if (ic->t >= 1.0f)
        {
            ic->t     = 0.0f;
            ic->state = LENS_ICON_ON;
        }
        break;

    case LENS_ICON_ON:
        r       = ic->rot + 6.0f;
        ic->rot = r;
        if (r > 360.0f)
            ic->rot = r - 360.0f;
        break;

    case LENS_ICON_VANISH:
        r       = ic->rot + 6.0f;
        ic->rot = r;
        if (r > 360.0f)
            ic->rot = r - 360.0f;
        ic->t -= 0.06666666f;
        if (ic->t <= 0.0f)
        {
            ic->rot   = 0.0f;
            ic->state = LENS_ICON_OFF;
            ic->t     = 0.0f;
        }
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4da8.

static void LensSwitchWork(LENS_SWITCH *sw, s_char equip)
{
    int state = sw->state;

    if ((u_int)(state - 1) < 2)
    {
        float t = sw->t + 0.125f;

        sw->t = t;
        if (t >= 1.0f)
        {
            sw->state = 0;
            sw->t     = 0.0f;
            state     = 0;
            sw->equip = LENS_NONE;
        }
    }
    if (state == 0)
        sw->equip = equip;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4e10.

static void LensSwitchReq(LENS_SWITCH *sw, int dir)
{
    if (sw->state == 0)
    {
        sw->state = dir;
        sw->t     = 0.0f;
    }
}

// ──────────────────────────────────────────────────────────────────────
// SE 0xb 'count' times a frame apart, SE 0xe on 'end'; all cleared while
// the camera is down (or on 'reset').
// FF3 0x001c4e30.

static void LensSeWork(LENS_SE *se, int reset)
{
    if (reset || (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
    {
        se->interval = 0;
        se->count    = 0;
        se->end      = 0;
    }

    if (se->end > 0)
    {
        PhotoModeSePlay(0xe, 100, 0x1000);
        se->interval = 0;
        se->count    = 0;
        se->end      = 0;
    }

    if (se->count > 0)
    {
        if (se->interval < 1)
        {
            PhotoModeSePlay(0xb, 100, 0x1000);
            se->interval = 1;
            se->count--;
        }
        else
            se->interval--;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c4ef0.

void LENS_WRK::Init()
{
    for (int i = 0; i < LENS_FUNC_NUM; i++)
        have_func[i] = 0;
    for (int i = 0; i < LENS_FUNC_NUM; i++)
        func[i] = 0;

    for (int c = 0; c < LENS_CHR_NUM; c++)
    {
        LENS_CHR *chr = &this->chr[c];

        chr->grade          = 0;
        stone[c].charge = 0;
        stone[c].sel    = 0;
        for (int i = 0; i < LENS_NUM; i++)
        {
            chr->level[i] = 0;
            chr->have[i] = 0;
        }
        for (int i = 0; i < LENS_SLOT_NUM; i++)
            chr->equip[i] = LENS_NONE;
    }

    for (int i = 0; i < LENS_ICON_NUM; i++)
    {
        icon[i].state = LENS_ICON_OFF;
        icon[i].t     = 0.0f;
        icon[i].rot   = 0.0f;
    }

    sw.state          = 0;
    sw.t              = 0.0f;
    sw.equip          = LENS_NONE;
    fade_lens.speed   = 0;
    fade_lens.target  = 0;
    fade_label.speed  = 0;
    fade_label.target = 0;
    fade_lens.val     = 0;
    fade_label.val    = 0;
    label_wait.val    = 0;
    se.interval       = 0;
    se.end            = 0;
    se.count          = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The lens label off at once and hidden for 10 frames (unless already).
// FF3 0x001c5020.

void LENS_WRK::LabelReset()
{
    fade_label.val    = 0;
    fade_label.speed  = 0;
    fade_label.target = 0;
    if (label_wait.val == 0)
        label_wait.val = 10;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame (finder mode): the SE block; players 0 and 2 kept full with
// function 8; the selection reset without function 0, the slot selection
// while the camera is up; the stone icons; the switch animation; the
// usable-lens and label fades.
// FF3 0x001c5050.

void LENS_WRK::Work()
{
    LensSeWork(&se, 0);

    if (PlyrGetCurNo() == CHR_NO_LENS)
        return;

    if (GetLensWrk()->GetFunc(LENS_FUNC_REFILL))
    {
        stone[0].charge = GetStoneMax(GetGrade(0), 0) * 100;
        stone[2].charge = GetStoneMax(GetGrade(2), 2) * 100;
    }

    if (GetFunc(LENS_FUNC_SLOTS) == 0 && GetSel(CHR_CUR) != 0)
        SetSel(0, CHR_CUR);

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) != 0)
        SelectCtrl(CHR_CUR);

    int n = GetStoneNum(CHR_CUR);
    for (int i = 0; i < LENS_ICON_NUM; i++)
        LensStoneWork(&icon[i], i < n);

    LensSwitchWork(&sw, GetEquip(GetSel(CHR_CUR), CHR_CUR));

    if (IsSelUsable() == 0 || sw.state != 0)
    {
        if (fade_lens.target != 0)
            fade_lens.Fade(0, 1);
        if (fade_label.target != 0)
            fade_label.Fade(0, 0);
    }
    else
    {
        if (fade_lens.target != 0x80)
            fade_lens.Fade(0x80, 4);
        if (fade_label.target != 0x80)
            fade_label.Fade(0x80, 8);
    }

    label_wait.Work();
    if (label_wait.val != 0 && fade_label.target != 0)
        fade_label.Fade(0, 0);

    fade_lens.Work();
    fade_label.Work();
}

// ──────────────────────────────────────────────────────────────────────
// The lens HUD around the finder point (x, y; drawn relative to the screen
// centre): frame and switch, slots, the shown lens, its label, the stone
// sockets and stones. Only with the finder ready and a lens owned.
// FF3 0x001c52a0.

void LENS_WRK::Draw(float x, float y, int alpha)
{
    const float k = 0.0078125f;

    if (PlyrGetCurNo() == CHR_NO_LENS)
        return;

    int show = 0;
    if (IsFinderReady())
        show = GetHaveNum(CHR_CUR) != 0;
    if (!show)
        return;

    float  a     = (float)alpha * k;
    float  ox    = x - 320.0f;
    float  oy    = y - 224.0f;
    int    slots = GetSlotNum();
    float  t     = sw.t;
    int    state = sw.state;

    HudLensSlotLensDraw(ox, oy, a, t, slots, state);
    HudLensSlotsDraw(ox, oy, a, slots);

    s_char equip = sw.equip;
    HudLensDraw(ox, oy, a, t, (float)(int)fade_lens.val * k, equip, state);
    if (label_wait.val == 0)
        HudLensMarkDraw(ox, oy, a, (float)(int)fade_label.val * k, equip);

    int max = GetStoneMax(CHR_CUR, CHR_CUR);
    if (max <= 0)
        return;
    HudStoneSocketsDraw(ox, oy, a, max);
    for (int i = 0; i < max; i++)
    {
        LENS_ICON *ic = &icon[i];

        HudStoneDraw(ox, oy, a, GetStoneFill((u_char)i, CHR_CUR), ic->t, ic->rot, i, ic->state);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A hit's charge for the stones, up to the grade's capacity (only for a
// player who owns a lens); every call counts an SE in 'se.count'.
// FF3 0x001c54c0.
// callcheck: __fixunssfsi (the (u_int) cast)

void LENS_WRK::AddCharge(u_int amount, u_char chr)
{
    LENS_STONE *st;
    u_int       add;
    u_int       max;
    u_int       charge;

    se.count++;
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;
    if (GetHaveNum(chr) == 0)
        return;

    st     = &stone[chr];
    add    = (u_int)(float)amount;      // FF3 goes through float (exact below 2^24)
    max    = GetStoneMax(GetGrade(chr), chr) * 100;
    charge = (u_int)st->charge + add;
    if (charge >= max)
        charge = max;
    st->charge = (int)charge;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c55b8.

void LENS_WRK::SeEnd()
{
    se.end = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c55c8.

int LENS_WRK::GetStoneNum(u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return stone[chr].charge / 100;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5630.

int LENS_WRK::GetStoneFrac(u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return stone[chr].charge % 100;
}

// ──────────────────────────────────────────────────────────────────────
// Stone i's fill: 1 below the charged count, the next stone's charge at
// it, 0 above.
// FF3 0x001c5698.

float LENS_WRK::GetStoneFill(u_char i, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0.0f;

    int n = GetStoneNum(chr);
    if (i > n)
        return 0.0f;
    if (i < n)
        return 1.0f;
    return (float)GetStoneFrac(chr) / 100.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Spends 'num' stones if charged; 1 on success.
// FF3 0x001c5770.

int LENS_WRK::UseStone(int num, u_char chr)
{
    LENS_STONE *st;

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;

    st = &stone[chr];
    if ((u_int)st->charge < (u_int)(num * 100))
        return 0;
    st->charge -= num * 100;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c57f0.

float LENS_WRK::GetStoneRate(u_char stones)
{
    (void)this;
    return lens_stone_rate[stones];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5810.

float LENS_WRK::GetLevelRate(u_char level)
{
    if (level == CHR_CUR)
        level = GetGrade(PlyrGetCurNo());
    return lens_level_rate[level];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5868.

u_char LENS_WRK::GetGrade(u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return this->chr[chr].grade;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c58c0.

void LENS_WRK::GradeUp(u_char chr)
{
    u_int grade;

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;

    grade = this->chr[chr].grade + 1;
    if (grade > LENS_GRADE_MAX)
        grade = LENS_GRADE_MAX;
    this->chr[chr].grade = (u_char)grade;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5930.

u_char LENS_WRK::GetStoneMax(u_char grade, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (grade == CHR_CUR)
        grade = GetGrade(chr);
    if (chr == CHR_NO_LENS)
        return 0;
    return (u_char)lens_stone_max[chr][grade];
}

// ──────────────────────────────────────────────────────────────────────
// With three slots, the finder ready and a lens owned, and no switch
// running: pad_key LENS_NEXT / LENS_PREV select the next / previous slot,
// start the switch animation (direction 2 / 1), SE 0xf.
// FF3 0x001c59c8.

void LENS_WRK::SelectCtrl(u_char chr)
{
    if (sw.state != 0)
        return;
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;
    if (GetFunc(LENS_FUNC_SLOTS) == 0)
        return;

    int show = 0;
    if (IsFinderReady())
        show = GetHaveNum(CHR_CUR) != 0;
    if (!show)
        return;

    LENS_STONE *st = &stone[chr];

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_LENS_NEXT], -1, 0) == 0
        && CheckPadTrgPlaySe(pad_key[PAD_KEY_LENS_PREV], -1, 0) == 0)
        return;

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_LENS_NEXT], -1, 0))
    {
        SetSel((s_char)((st->sel + 1) % 3), CHR_CUR);
        LensSwitchReq(&sw, 2);
    }
    else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_LENS_PREV], -1, 0))
    {
        SetSel((s_char)((st->sel + 2) % 3), CHR_CUR);
        LensSwitchReq(&sw, 1);
    }
    PhotoModeSePlay(0xf, 100, 0x1000);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5b68.

u_char LENS_WRK::GetSel(u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return stone[chr].sel;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5bb8. (For player 1 FF3 clears its byte first; kept.)

void LENS_WRK::SetSel(s_char slot, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        stone[CHR_NO_LENS].sel = 0;
    stone[chr].sel = (u_char)slot;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5c18. (The equip lookup always uses the current player.)

s_char LENS_WRK::GetSelEquip(u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return GetEquip(stone[chr].sel, CHR_CUR);
}

// ──────────────────────────────────────────────────────────────────────
// The level of the selected slot's lens. (The selection is the current
// player's even when 'chr' names another.)
// FF3 0x001c5c78.

u_char LENS_WRK::GetSelLevel(u_char chr)
{
    s_char equip = GetSelEquip(CHR_CUR);

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS || equip == LENS_NONE)
        return 0;
    return this->chr[chr].level[equip];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5cf8.

float LENS_WRK::GetSelRate()
{
    s_char    equip = GetSelEquip(CHR_CUR);
    LENS_DAT *dat;

    if (equip == LENS_NONE)
        return 0.0f;
    dat = GetDat((u_char)equip);
    if (dat == nullptr)
        return 0.0f;
    return dat->rate[GetSelLevel(CHR_CUR)];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5d70.

float LENS_WRK::GetSelVal()
{
    s_char    equip = GetSelEquip(CHR_CUR);
    LENS_DAT *dat;

    if (equip == LENS_NONE)
        return 0.0f;
    dat = GetDat((u_char)equip);
    if (dat == nullptr)
        return 0.0f;
    return dat->val[GetSelLevel(CHR_CUR)];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5de8.

int LENS_WRK::IsSelUsable()
{
    s_char    equip = GetSelEquip(CHR_CUR);
    LENS_DAT *dat;

    if (equip == LENS_NONE)
        return 0;
    dat = GetDat((u_char)equip);
    if (dat == nullptr)
        return 0;
    return (u_int)GetStoneNum(CHR_CUR) >= dat->cost;
}

// ──────────────────────────────────────────────────────────────────────
// Pays the selected lens's cost in stones; 1 if it could.
// FF3 0x001c5e60.

int LENS_WRK::UseSel()
{
    s_char    equip = GetSelEquip(CHR_CUR);
    LENS_DAT *dat;

    if (equip == LENS_NONE)
        return 0;
    dat = GetDat((u_char)equip);
    if (dat == nullptr)
        return 0;
    if ((u_int)GetStoneNum(CHR_CUR) < dat->cost)
        return 0;
    UseStone(dat->cost, CHR_CUR);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5ee8.

float LENS_WRK::GetRate(u_char no)
{
    LENS_DAT *dat = GetDat(no);

    if (dat == nullptr)
        return 0.0f;
    return dat->rate[GetLevel(no, CHR_CUR)];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5f58.

u_char LENS_WRK::GetLevel(u_char no, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return this->chr[chr].level[no];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c5fc0.

void LENS_WRK::LevelUp(u_char no, u_char chr)
{
    u_int level;

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;

    level = this->chr[chr].level[no] + 1;
    if (level > LENS_LEVEL_MAX)
        level = LENS_LEVEL_MAX;
    this->chr[chr].level[no] = (u_char)level;
}

// ──────────────────────────────────────────────────────────────────────
// A camera function gained: owned, and switched on.
// FF3 0x001c6048.

void LENS_WRK::AddFunc(u_char func)
{
    have_func[func] = 1;
    SetFunc(func);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6070.

u_char LENS_WRK::HaveFunc(u_char func)
{
    return have_func[func];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6080.

int LENS_WRK::GetHaveFuncNum()
{
    int n = 0;

    for (int i = 0; i < LENS_FUNC_NUM; i++)
    {
        if (HaveFunc((u_char)i))
            n++;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Switches an owned camera function on; 0 if it is not owned.
// FF3 0x001c60e0.

int LENS_WRK::SetFunc(u_char func)
{
    if (HaveFunc(func) == 0)
        return 0;
    this->func[func] = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6138.

u_char LENS_WRK::GetFunc(u_char func)
{
    return this->func[func];
}

// ──────────────────────────────────────────────────────────────────────
// Switches a camera function off; function 5 stays on. FF3 0x001c6148.

void LENS_WRK::ClearFunc(u_char func)
{
    if (func != 5)
        this->func[func] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6168.

void LENS_WRK::AddHave(u_char no, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;
    this->chr[chr].have[no] = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c61c8.

u_char LENS_WRK::IsHave(u_char no, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    return this->chr[chr].have[no];
}

// ──────────────────────────────────────────────────────────────────────
// Is lens 'no' in one of the equip slots (three with LENS_FUNC_SLOTS, else
// the first)? (The slots read are the current player's.)
// FF3 0x001c6230.

int LENS_WRK::IsEquip(u_char no, u_char chr)
{
    int slot_num;

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;

    slot_num = GetFunc(LENS_FUNC_SLOTS) ? 3 : 1;
    for (int i = 0; i < slot_num; i++)
    {
        if ((int)GetEquip((u_char)i, CHR_CUR) == (int)no)    // LENS_NONE never matches
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Puts lens 'no' in a slot if the current player owns it; 1 if it did.
// FF3 0x001c62e8 (no Ghidra function before this session).

int LENS_WRK::SetEquip(u_char slot, u_char no, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    if (IsHave(no, CHR_CUR) == 0)
        return 0;
    this->chr[chr].equip[slot] = (s_char)no;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6388.

s_char LENS_WRK::GetEquip(u_char slot, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return LENS_NONE;
    return this->chr[chr].equip[slot];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c63f0.

void LENS_WRK::Unequip(u_char slot, u_char chr)
{
    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return;
    this->chr[chr].equip[slot] = LENS_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6450.

LENS_DAT *LENS_WRK::GetDat(u_char no)
{
    (void)this;
    if (no < LENS_NUM)
        return &lens_dat[no];
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6480.

int LENS_WRK::GetHaveNum(u_char chr)
{
    int n = 0;

    if (chr == CHR_CUR)
        chr = PlyrGetCurNo();
    if (chr == CHR_NO_LENS)
        return 0;
    for (u_int i = 0; i < LENS_NUM; i++)
        n += this->chr[chr].have[i] != 0;
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The hit flash for a shot with lens 'no' (EneHitEffectReq labels, as in
// FF2's n_equip_tray SetEffect): the lens's plain label, +1 in a shutter
// chance (slot flag 0x800), +2 in a fatal frame (0x1000). Lenses 4, 5 and
// 6 have one label; others none.
// FF3 0x001c6500.

void LENS_WRK::HitEffectReq(ENE_WRK *slot, u_char no)
{
    int label;

    (void)this;

    switch (no)
    {
    case 0: label = 7;  break;              // three labels each
    case 1: label = 4;  break;
    case 2: label = 12; break;
    case 3: label = 15; break;
    case 7: label = 20; break;
    case 8: label = 23; break;

    case 4:
        EneHitEffectReq(slot->alg.idx, slot->mpos, 7);
        return;
    case 5:
        EneHitEffectReq(slot->alg.idx, slot->mpos, 0x13);
        return;
    case 6:
        EneHitEffectReq(slot->alg.idx, slot->mpos, 3);
        return;

    default:
        return;
    }

    if (slot->flags & 0x1000)
        label += 2;
    else if (slot->flags & 0x800)
        label += 1;
    EneHitEffectReq(slot->alg.idx, slot->mpos, label);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6638.

void *LENS_WRK::GetSaveData()
{
    return chr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6648.

int LENS_WRK::IsFinderReady()
{
    (void)this;
    return GetPhotoModeWrk()->IsFinderReady();
}

// ──────────────────────────────────────────────────────────────────────
// Lens slots on the HUD: 0 while the finder is not ready, 3 with the
// slots function on, else 1. FF3 0x001c6668.

int LENS_WRK::GetSlotNum()
{
    if (IsFinderReady() == 0)
        return 0;
    return GetFunc(LENS_FUNC_SLOTS) != 0 ? 3 : 1;
}

// ──────────────────────────────────────────────────────────────────────
// A copy of the save block in the system heap, kept over the story game's
// reload after an extra mission (CExtraMissionShelt::CarryStore).
// FF3 0x001c66b8.

// PC: FF3 keeps the copy's address in the work's first word; a host pointer
// does not fit, so it lives here and 'backup' only flags it.
static void *lens_backup_buf;

void LENS_WRK::SaveBackup()
{
    void *buf = heapCtrlMalloc(GetSystemHeapWrkP(), LENS_SAVE_SIZE, 0, 0, 0);

    lens_backup_buf = buf;
    backup    = buf != nullptr;
    memcpy(buf, chr, LENS_SAVE_SIZE);
}

// ──────────────────────────────────────────────────────────────────────
// Back to the copy, but the stones charged since stay; the copy is freed.
// FF3 0x001c6708.

void LENS_WRK::RestoreBackup()
{
    u_char stone[sizeof(this->stone)];

    if (backup == 0)
        return;

    memcpy(stone, this->stone, sizeof(this->stone));
    memcpy(chr, lens_backup_buf, LENS_SAVE_SIZE);
    memcpy(this->stone, stone, sizeof(this->stone));
    heapCtrlFree(GetSystemHeapWrkP(), lens_backup_buf, 0, 0, 0);
    lens_backup_buf = nullptr;
    backup    = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c6790.

void LENS_WRK::ClearCharge()
{
    for (int i = LENS_CHR_NUM - 1; i >= 0; i--)
        stone[i].charge = 0;
}
