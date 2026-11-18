/* ==========================================================================
 *  graphics/effect/effect_scrsaver.c
 *
 *  The screen saver and the old-film noise (FF3-only file, name invented;
 *  see effect_scrsaver.h). Ported from MioPan's effect_scr.c screen saver
 *  (ScreenSaverInit / Main / OneTexInit / Draw) and checked against FF3,
 *  which differs:
 *    * no sound bank: FF2 loads a cue bank and chirps on every dither pulse,
 *      FF3 keeps play_id (-1, never set) and the stop on input only;
 *    * the input test reads the 16 buttons through CheckPadNowPlaySe and the
 *      sticks off-centre by 0x33 (FF2: key_now / analog ranges), the wait is
 *      7500 frames on PAL (150 s either way);
 *    * the sheet is file (ingame_wrk.b01 & 1) ^ 1 (FF2: 0x15 / 0x14 by the
 *      chapter's parity), sent with PK4SendVram every drawn frame;
 *    * the picture has keyframes for alpha, scale and a blur, dissolves in
 *      and out by alpha test (FF2: alpha only), and the frame gets the film
 *      noise (dust, scratch lines), a shake and a flicker copy;
 *    * the play step is out of line (0x2b7358), the dither pulse's envelope
 *      too (0x2b8178: 35 * env + 67; FF2 85 * env + 42).
 *
 *  PC: the saver sheet's picture is a real PSMT8 texture at 0x2bc0, which
 *  the renderer keeps apart from the 0x2bc0 capture slot the flicker copy
 *  fills (reipan_renderer.cpp, IsCaptureSlotAddr).
 *
 *  FF3 0x17f8b8..0x1806a8, 0x2b6ee0..0x2b8220.
 * ======================================================================== */

#include "common/reipan_stub.h"
#include "graphics/effect/effect_scrsaver.h"

#include <stdlib.h>                     // abs
#include <string.h>                     // memset

#include "graphics/effect/effect.h"     // EFFECT_CONT, eff_wrk, EFFECT_MALLOC / FREE
#include "graphics/effect/effect_oth.h" // SubBlur, SetDither3
#include "graphics/effect/effect_sub.h" // EffDispSprCentreScale, EffDispSprD2Direct
#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, SPRT_DAT2, DISP_SPRT2, CopySprDToSpr2, DispSprD2, LocalCopyLtoL
#include "graphics/graph2d/tim2.h"      // PK4SendVram
#include "ingame/menu/anim_2d.h"        // LangTexSend
#include "ingame/ingame_wrk.h"          // ingame_wrk.b01
#include "common/vu0math.h"             // Vu0Sinf
#include "system/os/system.h"           // GetSysWrk, GetPALMode
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/snd_buffer.h"    // SndBufIsPlaying, SndBufStop
#include "system/pad/pad_ctrl.h"        // CheckPadNowPlaySe, PadGetAnalogRaw
#include "reipan/os/reipan_rand.h"      // ReiPan_Rand, REIPAN_RAND_MAXF
#include "reipan/rendering/reipan_renderer.h" // PC: ReiPan_RendererGetViewBounds

// ──────────────────────────────────────────────────────────────────────
// State
// ──────────────────────────────────────────────────────────────────────

static SCRSAVER_CTRL scrsaver_ctrl;     // bss 0x3dee00

// The saver sheet's picture: 256x256 PSMT8 at 0x2bc0 (rdata 0x354ac0).
static SPRT_DAT scrsaver_sprt = { 0x2005980621312bc0ull, 0, 0, 0x100, 0x100, 0, 0, 0, 0x80, 0, 0 };

// The ALPHA values the film draws pick by their blend argument (rdata
// 0x354ae0, the table effect_obj.c / effect_ene.c also read).
static const u_long eff_film_alpha_tbl[4] = { 0x44, 0x48, 0x42, 0x84 };

// The dust specks and the scratch strips on the language pack's sheet 0
// (128x128 PSMT4 at 0x2bc0, clut 0x2ca0). FF3 builds both tables at start-up:
// FF3 0x00180360 (__static_initialization_and_destruction_0) stores every
// field, FF3 0x00180680 (_GLOBAL__I_) calls it; a static initializer does the
// same here. bss 0x3def70 / 0x3df090.
static const SPRT_DAT eff_film_dust_sprt[9] =
{
    { 0x20059405dd40abc0ull,   1,   1, 0x38, 0x36, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  59,   1, 0x1b, 0x18, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  88,   1, 0x09, 0x18, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  59,  27, 0x26, 0x1c, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,   1,  57, 0x1e, 0x2a, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  33,  57, 0x15, 0x1d, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  56,  57, 0x23, 0x1d, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  33,  88, 0x3a, 0x0f, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,   1, 105, 0x5a, 0x16, 0, 0, 0, 0x80, 0, 0 },
};
static const SPRT_DAT eff_film_line_sprt[4] =
{
    { 0x20059405dd40abc0ull,  93,  57, 0x04, 0x46, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull, 115,   1, 0x03, 0x7e, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull, 120,   1, 0x07, 0x7e, 0, 0, 0, 0x80, 0, 0 },
    { 0x20059405dd40abc0ull,  99,   1, 0x0e, 0x7e, 0, 0, 0, 0x80, 0, 0 },
};

// rand() / RAND_MAX as the ROM writes it (2147483520.0f, rdata 0x36f0f8..).
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)
static inline float EffFilmRand01(void)
{
    return (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
}

// ──────────────────────────────────────────────────────────────────────
// The saver
// ──────────────────────────────────────────────────────────────────────

// FF2 ScreenSaverInit; FF3 0x0017f8b8.
void ScreenSaverInit(void)
{
    memset(&scrsaver_ctrl, 0, sizeof(scrsaver_ctrl));
    scrsaver_ctrl.load_status = 0;
    scrsaver_ctrl.play_id     = -1;
    scrsaver_ctrl.load_id     = -1;
    scrsaver_ctrl.tex_buf     = nullptr;
}

// FF2 ScreenSaverMain; FF3 0x0017f900, every frame from InitEffectsEF. The
// play step of status 2 is ScreenSaverPlayMain.
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
// callcheck: EFFECT_MALLOC, EFFECT_FREE (read as macros); ScreenSaverPlayMain is a tail jump (j) in the ROM
void ScreenSaverMain(void)
{
    SCRSAVER_CTRL *ctrl = &scrsaver_ctrl;
    int ax0, ax1, ax2, ax3;
    int b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15;
    int cnt;
    int file;

    ax0 = abs((int)PadGetAnalogRaw(0, 0) - 0x80);
    ax1 = abs((int)PadGetAnalogRaw(1, 0) - 0x80);
    ax2 = abs((int)PadGetAnalogRaw(2, 0) - 0x80);
    ax3 = abs((int)PadGetAnalogRaw(3, 0) - 0x80);
    b0  = CheckPadNowPlaySe(0x4000, -1, 0);
    b1  = CheckPadNowPlaySe(0x0080, -1, 0);
    b2  = CheckPadNowPlaySe(0x8000, -1, 0);
    b3  = CheckPadNowPlaySe(0x0010, -1, 0);
    b4  = CheckPadNowPlaySe(0x1000, -1, 0);
    b5  = CheckPadNowPlaySe(0x2000, -1, 0);
    b6  = CheckPadNowPlaySe(0x0040, -1, 0);
    b7  = CheckPadNowPlaySe(0x0020, -1, 0);
    b8  = CheckPadNowPlaySe(0x0004, -1, 0);
    b9  = CheckPadNowPlaySe(0x0001, -1, 0);
    b10 = CheckPadNowPlaySe(0x0200, -1, 0);
    b11 = CheckPadNowPlaySe(0x0008, -1, 0);
    b12 = CheckPadNowPlaySe(0x0002, -1, 0);
    b13 = CheckPadNowPlaySe(0x0400, -1, 0);
    b14 = CheckPadNowPlaySe(0x0800, -1, 0);
    b15 = CheckPadNowPlaySe(0x0100, -1, 0);

    if (b0 + b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9 + b10 + b11 + b12 + b13 + b14 + b15
        + (ax2 >= 0x33) + (ax3 >= 0x33) + (ax0 >= 0x33) + (ax1 >= 0x33) == 0)
        ctrl->counter++;
    else
        ctrl->counter = 0;
    cnt = ctrl->counter;

    if (ctrl->load_status == 1)
    {
        if (IsLoadEnd(ctrl->load_id) != 0)
        {
            ctrl->interval_time = 0;
            ctrl->disp_time     = (int)(EffFilmRand01() * 116.0f + 38.0f);
            ScreenSaverOneTexInit(&ctrl->tex[0], ctrl->disp_time);
            ctrl->dither_change_time = 0;
            ctrl->dither_alpha       = 67.0f;
            ctrl->dither_interval    = (int)(EffFilmRand01() * 120.0f) + 0x78;
            EffFilmLineInit();
            EffFilmShakeInit();
            ctrl->load_status = 2;
        }
    }
    else if (ctrl->load_status < 2)
    {
        if (ctrl->load_status == 0)
        {
            if (GetPALMode() == 0)
            {
                if (cnt < 9000)
                    return;
            }
            else if (cnt < 7500)
            {
                return;
            }
            ctrl->counter = (GetPALMode() == 0) ? 9000 : 7500;

            // SCREENSAVER2_PK4 for an even b01 (CHANGE_CHAPTER's byte), else SCREENSAVER_PK4.
            file = (ingame_wrk.b01 & 1) ^ 1;
            ctrl->tex_buf = EFFECT_MALLOC((int)GetFileSize(file));
            if (ctrl->tex_buf != nullptr)
            {
                ctrl->load_id     = LoadReq(file, ctrl->tex_buf);
                ctrl->load_status = 1;
            }
        }
    }
    else if (ctrl->load_status == 2)
    {
        if (GetPALMode() == 0 ? cnt > 8999 : cnt > 7499)
        {
            ctrl->counter = (GetPALMode() == 0) ? 9000 : 7500;
            ScreenSaverPlayMain(ctrl);
            return;
        }

        // Input: free everything and count again.
        if (ctrl->tex_buf != nullptr)
        {
            EFFECT_FREE(ctrl->tex_buf);
            ctrl->tex_buf = nullptr;
        }
        if (ctrl->play_id != -1 && SndBufIsPlaying(ctrl->play_id) != 0)
            SndBufStop(ctrl->play_id);
        ctrl->load_status = 0;
        ctrl->play_id     = -1;
        ctrl->load_id     = -1;
    }
}

// FF2 ScreenSaverDraw; FF3 0x0017fdb0, from EffectControl (pass 0, before
// the room glow). The picture dissolves by alpha test (TEST AREF = aref);
// the more of it shows, the stronger the noise and the dither.
void ScreenSaverDraw(void)
{
    SCRSAVER_CTRL *ctrl = &scrsaver_ctrl;
    EFFECT_CONT    ec;
    SCRSAVER_TEX  *tex;
    float          dither_alp;
    float          f;
    u_int          min_aref;
    u_char         noise_alp;
    int            col[4];
    int            i;

    if (ctrl->load_status != 2)
        return;

    noise_alp  = 0x40;
    dither_alp = ctrl->dither_alpha;
    PK4SendVram(ctrl->tex_buf, 0, -1, -1);

    min_aref = 0x80;
    for (i = 0; i < 1; i++)
    {
        tex = &ctrl->tex[i];
        if (tex->disp_time == 0 || tex->alpha == 0)
            continue;

        col[0] = 0x80;
        col[1] = 0x80;
        col[2] = 0x80;
        col[3] = tex->alpha;
        EffDispSprCentreScale(tex->scale + tex->scale_add, tex->scale + tex->scale_add, 0x3020,
                              &scrsaver_sprt, tex->aref, tex->pos_x - (scrsaver_sprt.w >> 1),
                              tex->pos_y - (scrsaver_sprt.h >> 1), col, 0xa0, eff_film_alpha_tbl[1]);
        if (tex->blur_scw != 0)
            SubBlur((float)tex->blur_scw, tex->blur_sch, tex->blur_rot, 180.0f, 320.0f, 3, 1, 0xe0);
        if (tex->aref < min_aref)
            min_aref = tex->aref;
    }

    if (min_aref <= 0x80)
    {
        f          = (float)(int)((0x80 - min_aref) * 200) * 0.0078125f + 1.0f;
        dither_alp = dither_alp * f;
        noise_alp  = (u_char)((int)(f * 64.0f) & 0xff);   // wraps once f > 4, as in the ROM
    }
    EffFilmNoiseDraw(0.089f, 0.007f, noise_alp, noise_alp);

    if (0.0f < dither_alp)
    {
        // The ROM fills only these fields of a stack EFFECT_CONT; uc8[1] bit 0
        // makes SetDither3 reset it afterwards.
        memset(&ec, 0, sizeof(ec));
        ec.dat.uc8[0] = 2;
        ec.dat.uc8[1] = 1;
        ec.dat.uc8[2] = 8;              // dither type
        ec.dat.uc8[3] = 0x2c;           // alpha max
        ec.dat.uc8[4] = 0x0f;           // colour max
        ec.dat.fl32[2] = dither_alp;
        ec.dat.fl32[3] = 8.0f;          // speed
        SetDither3(&ec);
    }

    EffFilmShakeDraw(ctrl->disp_time != 0 ? 0.014f : 0.0f);
    if (ctrl->flicker_alpha != 0)
        EffFilmFrameCopyDraw(0.0f, 0.0f, 1.0f, 1.0f, ctrl->flicker_alpha, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The film noise
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00180068.
void EffFilmLineInit(void)
{
    int i;

    for (i = 0; i < 8; i++)
        memset(&scrsaver_ctrl.line[i], 0, sizeof(EFF_FILM_LINE));
}

// FF3 0x001800c0: the lines move unless the effects are stopped, then the
// language pack's sheet 0 goes to the GS and the dust / lines draw.
void EffFilmNoiseDraw(float dust_rate, float line_rate, u_char dust_alp, u_char line_alp)
{
    int i;

    if (eff_wrk.stop_flg == 0)
    {
        for (i = 0; i < 8; i++)
            EffFilmLineStep(line_rate, &scrsaver_ctrl.line[i]);
    }
    LangTexSend(0);
    if (dust_alp != 0)
        EffFilmDustDraw(dust_rate, dust_alp, 0);
    if (line_alp != 0)
    {
        for (i = 0; i < 8; i++)
            EffFilmLineDraw(&scrsaver_ctrl.line[i], line_alp, 0);
    }
}

// FF3 0x001801a0.
void EffFilmShakeInit(void)
{
    memset(&scrsaver_ctrl.shake, 0, sizeof(EFF_FILM_SHAKE));
}

// FF3 0x001801c8: an idle shake starts with probability rate; a running one
// is a sine pulse over its 4 frames (pi / 4 a frame), drawn as a frame copy
// moved up and stretched. dx / scw keep the ROM's zero factors.
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
// callcheck: EffFilmFrameCopyDraw is a tail jump (j) in the ROM
void EffFilmShakeDraw(float rate)
{
    EFF_FILM_SHAKE *sh = &scrsaver_ctrl.shake;
    float           s;

    if (sh->cnt < 1)
    {
        if (EffFilmRand01() < rate)
        {
            sh->sch   = 1.0f;
            sh->cnt   = 4;
            sh->dx    = 0;
            sh->dy    = 0;
            sh->scw   = 1.0f;
            sh->alpha = 0;
        }
    }
    else
    {
        s = Vu0Sinf((float)sh->cnt * 3.1415927f * 0.25f);
        sh->dx    = (short)(int)(s * 0.0f);
        sh->dy    = (short)(int)(s * -85.0f);
        sh->scw   = s * 0.0f + 1.0f;
        sh->cnt--;
        sh->sch   = s + 1.0f;
        sh->alpha = (u_char)(int)(s * 180.0f);
    }

    if (sh->cnt > 0 && sh->alpha != 0)
        EffFilmFrameCopyDraw((float)sh->dx, (float)sh->dy, sh->scw, sh->sch, sh->alpha, 1);
}

// ──────────────────────────────────────────────────────────────────────
// Out-of-line bodies (0x2b6ee0..)
// ──────────────────────────────────────────────────────────────────────

// FF2 ScreenSaverOneTexInit; FF3 0x002b6ee0. The ROM draws every value with
// its own rand(), in this order; its keep weight and three keyframe pairs
// scale rand by 0 (always 0 / 100).
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
void ScreenSaverOneTexInit(SCRSAVER_TEX *tex, int disp_time)
{
    const float zero = 0.0f;
    short       v;

    tex->scale         = EffFilmRand01() + 2.2f;
    tex->base_x        = (short)(int)(EffFilmRand01() * 590.0f);
    tex->disp_time     = disp_time;
    tex->pos_x         = tex->base_x;
    tex->disp_time_all = disp_time;
    v = (short)(int)(EffFilmRand01() * 398.0f);
    tex->pos_y         = v;
    tex->base_y        = v;
    tex->in_time       = (int)(EffFilmRand01() * 41.0f) + 0x39;
    tex->keep_time     = (int)(EffFilmRand01() * zero);
    tex->alpha         = 0;
    tex->blur_scw      = 0;
    tex->blur_rot      = 1.0f;
    tex->aref          = 0x80;
    tex->blur_sch      = 1.0f;
    tex->out_time      = (int)(EffFilmRand01() * 21.0f) + 0x18;

    tex->key_alpha[0]    = (short)(int)(EffFilmRand01() * 64.0f);
    tex->key_alpha[1]    = (short)((int)(EffFilmRand01() * 32.0f) + 0x60);
    tex->key_alpha[2]    = (short)(int)(EffFilmRand01() * 32.0f);
    tex->key_scale[0]    = (short)(int)(EffFilmRand01() * zero);
    tex->key_scale[1]    = (short)(int)(EffFilmRand01() * zero);
    tex->key_scale[2]    = (short)(int)(EffFilmRand01() * zero);
    tex->key_blur_scw[0] = (short)(int)(EffFilmRand01() * 10.0f);
    tex->key_blur_scw[1] = (short)((int)(EffFilmRand01() * 30.0f) + 10);
    tex->key_blur_scw[2] = (short)(int)(EffFilmRand01() * 20.0f);
    tex->key_blur_sch[0] = (short)((int)(EffFilmRand01() * zero) + 100);
    tex->key_blur_sch[1] = (short)((int)(EffFilmRand01() * zero) + 100);
    tex->key_blur_sch[2] = (short)((int)(EffFilmRand01() * zero) + 100);
    tex->key_blur_rot[0] = (short)((int)(EffFilmRand01() * 10.0f) + 100);
    tex->key_blur_rot[1] = (short)((int)(EffFilmRand01() * 10.0f) + 0x6e);
    tex->key_blur_rot[2] = (short)((int)(EffFilmRand01() * 10.0f) + 100);
}

// FF2 ScreenSaverMain's playing branch; FF3 0x002b7358 (ctrl == &tex[0]).
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
void ScreenSaverPlayMain(SCRSAVER_CTRL *ctrl)
{
    SCRSAVER_TEX *tex = &ctrl->tex[0];
    float         fy;
    float         r;
    int           t;
    int           n;

    if (tex->disp_time != 0)
    {
        tex->pos_x = (short)(int)((float)tex->base_x + EffFilmRand01() * 4.0f);
        fy = (float)tex->base_y;
        r  = EffFilmRand01();
        tex->pos_y = (short)(int)(fy + (r + r));
        ScreenSaverOneTexStep(tex);
        tex->disp_time--;
    }

    // The show / interval timers: a new picture once the interval runs out.
    if (ctrl->disp_time == 0)
    {
        if (ctrl->interval_time == 0 || --ctrl->interval_time == 0)
        {
            ctrl->disp_time = (int)(EffFilmRand01() * 116.0f + 38.0f);
            ScreenSaverOneTexInit(tex, ctrl->disp_time);
        }
    }
    else
    {
        ctrl->disp_time--;
        if (ctrl->disp_time == 0)
            ctrl->interval_time = (int)(EffFilmRand01() * 137.0f + 16.0f);
    }

    // The dither pulse: 30 frames, then a pause of 120..240.
    if (ctrl->dither_change_time == 0)
    {
        if (ctrl->dither_interval == 0 || --ctrl->dither_interval == 0)
            ctrl->dither_change_time = 30;
        t = 0;
    }
    else
    {
        ctrl->dither_change_time--;
        if (ctrl->dither_change_time == 0)
            ctrl->dither_interval = (int)(EffFilmRand01() * 120.0f + 120.0f);
        t = 30 - ctrl->dither_change_time;
    }
    ctrl->dither_alpha = ScreenSaverDitherAlphaGet(t);

    // The flicker: 30 on one frame of three (flicker_wait is never set, so
    // the reload follows at once).
    if (ctrl->flicker_cnt < 1)
    {
        ctrl->flicker_wait--;
        if (ctrl->flicker_wait < 1)
        {
            ctrl->flicker_wait = 0;
            ctrl->flicker_cnt  = 2;
        }
    }
    else
    {
        n = --ctrl->flicker_cnt;
        ctrl->flicker_alpha = (u_char)((n > 0) ? (2 - n) * 30 : 30 * n);
    }
}

// FF3 0x002b75e8: the shown frame into 0x2bc0 at half width, drawn back
// over the screen offset by (x, y) and scaled round the centre.
void EffFilmFrameCopyDraw(float x, float y, float scw, float sch, u_char alp, int blend)
{
    static const SPRT_DAT2 sd_half = { 0, 0.5f, 0.5f, 319.5f, 223.5f, 640.0f, 448.0f, 0.0f, 0.0f, 0, 0x80 };  // rdata 0x368268
    DISP_SPRT2 ds;

    LocalCopyLtoL(1, (int)((GetSysWrk()->count & 1) * 0x1180), 0x2bc0);
    CopySprDToSpr2(&ds, &sd_half);
    ds.csx    = x + 320.0f;
    ds.csy    = y + 224.0f;
    ds.x      = x;
    ds.y      = y;
    ds.z      = 0xfff5f;
    ds.scw    = scw;
    ds.sch    = sch;
    ds.tex0   = 0x2000000224116bc0ull;  // TBP 0x2bc0, TBW 5, PSMCT24 (rdata 0x36f198)
    ds.tex1   = 0x161;
    ds.alpreg = eff_film_alpha_tbl[blend];
    ds.zbuf   = 0x10a000118ull;
    ds.test   = 0x30003;
    ds.pri    = 0xa0;
    ds.r      = 0x80;
    ds.g      = 0x80;
    ds.b      = 0x80;
    ds.alp    = alp;
    EffDispSprD2Direct(&ds);
}

// FF3 0x002b7738: each dust row shows with probability rate at a random
// spot, scale and angle. The ROM passes the row's w / h as the far UV
// corner (u2 = w, v2 = h, not u + w), kept.
// PC: the spot falls anywhere in the window (the view bounds; 640 x 448 in
// the ROM), from the same draws, so the specks reach the sides of a wide
// window. As many specks as the ROM, so they are sparser there.
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
void EffFilmDustDraw(float rate, u_char alp, int blend)
{
    DISP_SPRT2 ds;
    SPRT_DAT2  sd;
    const SPRT_DAT *row;
    float      x, y, scw, sch, rot;
    float      x0, y0, x1, y1;
    int        i;

    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);  // PC
    for (i = 0; i < 9; i++)
    {
        row = &eff_film_dust_sprt[i];
        if (!(EffFilmRand01() < rate))
            continue;

        x   = x0 + EffFilmRand01() * (x1 - x0);
        y   = y0 + EffFilmRand01() * (y1 - y0);
        scw = EffFilmRand01();
        sch = EffFilmRand01();
        rot = EffFilmRand01() * 360.0f;
        scw = scw * 1.88f + 0.72f;
        sch = sch * 1.88f + 0.72f;

        sd.tex0  = 0;
        sd.u1    = (float)row->u;
        sd.v1    = (float)row->v;
        sd.u2    = (float)row->w;
        sd.v2    = (float)row->h;
        sd.w     = (float)row->w;
        sd.h     = (float)row->h;
        sd.x     = 0.0f;
        sd.y     = 0.0f;
        sd.pri   = 0;
        sd.alpha = 0x80;
        CopySprDToSpr2(&ds, &sd);
        ds.tex0   = row->tex0;
        ds.alpreg = eff_film_alpha_tbl[blend];
        ds.z      = 0xfff5f;
        ds.tex1   = 0x161;
        ds.rot    = rot;
        ds.scw    = scw;
        ds.pri    = 0xa0;
        ds.sch    = sch;
        ds.crx    = x;
        ds.cry    = y;
        ds.alp    = alp;
        ds.csx    = x;
        ds.csy    = y;
        ds.x      = x;
        ds.y      = y;
        DispSprD2(&ds);
    }
}

// FF3 0x002b79d8: a live line as a 480-high strip of its row, the row's v
// scrolled by the line's y and repeated inside the row (CLAMP REGION_REPEAT
// on v: MINV = h as the mask, MAXV = v as the fix), tilted by -2..2 degrees.
// The far UV corner is the row's w / h plus y, as in the ROM.
// PC: drawn over the window (the view bounds). ln->x stays in the ROM's
// 640 units -- EffFilmLineStep kills a line by it, and when a line dies
// decides when its respawn draws six more numbers from ReiPan_Rand -- and
// is mapped onto the window's width here; the strip is grown by the window's
// extra height. Both are the ROM's values on a 640:448 window.
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
void EffFilmLineDraw(EFF_FILM_LINE *ln, u_char alp, int blend)
{
    DISP_SPRT2 ds;
    SPRT_DAT2  sd;
    const SPRT_DAT *row;
    float      rot;
    float      x0, y0, x1, y1;
    float      x;

    if (ln->life <= 0)
        return;

    row = &eff_film_line_sprt[ln->type];
    rot = EffFilmRand01() * 4.0f - 2.0f;

    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);  // PC
    x = x0 + ln->x * ((x1 - x0) / 640.0f);              // PC

    sd.tex0  = 0;
    sd.u1    = (float)row->u;
    sd.v1    = (float)row->v + ln->y;
    sd.u2    = (float)row->w;
    sd.v2    = (float)row->h + ln->y;
    sd.w     = (float)row->w;
    sd.h     = (y1 - y0) + 32.0f;     // PC: 480 in the ROM
    sd.x     = 0.0f;
    sd.y     = 0.0f;
    sd.pri   = 0;
    sd.alpha = 0x80;
    CopySprDToSpr2(&ds, &sd);
    ds.tex0   = row->tex0;
    ds.tex1   = 0x161;
    ds.alpreg = eff_film_alpha_tbl[blend];
    ds.clmp   = ((u_long)row->h << 24) | ((u_long)row->v << 34) | 0xc;
    ds.pri    = 0xa0;
    ds.z      = 0xfff5f;
    ds.alp    = alp;
    ds.rot    = rot;
    ds.cry    = 224.0f;
    ds.x      = x;
    ds.crx    = x;
    ds.y      = y0 - 16.0f;           // PC: -16 in the ROM
    DispSprD2(&ds);
}

// FF3 0x002b7bc0: a free line respawns with probability rate; a live one
// drifts (x by dx * 8..12 a frame) and dies off-screen.
// callcheck: rand (EffFilmRand01: ReiPan_Rand)
void EffFilmLineStep(float rate, EFF_FILM_LINE *ln)
{
    float r;

    if (ln->life < 1)
    {
        if (!(EffFilmRand01() < rate))
            return;
        ln->life = (int)(EffFilmRand01() * 153.0f);
        ln->x    = EffFilmRand01() * 640.0f;
        ln->y    = EffFilmRand01() * 224.0f + 224.0f;
        ln->dx   = EffFilmRand01() * 3.15f + -2.0f;
        ln->dy   = EffFilmRand01() * 2.52f + -1.69f;
        ln->type = (u_char)(int)(EffFilmRand01() * 4.0f);
        return;
    }

    r = EffFilmRand01();
    ln->life--;
    ln->y = ln->y + ln->dy;
    ln->x = ln->x + ln->dx * (r * 0.4f + 0.8f) * 10.0f;
    if (ln->x < -640.0f || 1280.0f < ln->x || ln->y < 0.0f || 448.0f < ln->y)
        ln->life = 0;
}

// FF3 0x002b7e58: the picture's envelope. Each phase lasts disp_time_all
// * weight / (in + keep + out) frames; in lerps the keyframes [0] -> [1],
// keep holds [1], out lerps [1] -> [2]. The alpha test reference follows
// the rate: 0x80 (nothing) at 0, 0 (all) at 1.
void ScreenSaverOneTexStep(SCRSAVER_TEX *tex)
{
    int   all  = tex->disp_time_all;
    int   sum  = tex->in_time + tex->keep_time + tex->out_time;
    int   t_in = (all * tex->in_time) / sum;
    int   done = all - tex->disp_time;
    int   t_keep;
    float f = 0.0f;

    if (done < t_in)
    {
        if (tex->in_time != 0)
            f = (float)done / (float)t_in;
        tex->scale_add = (float)(tex->key_scale[1] - tex->key_scale[0]) * f + (float)tex->key_scale[0];
        tex->blur_sch  = (float)(tex->key_blur_sch[1] - tex->key_blur_sch[0]) * f + (float)tex->key_blur_sch[0];
        tex->blur_rot  = (float)(tex->key_blur_rot[1] - tex->key_blur_rot[0]) * f + (float)tex->key_blur_rot[0];
        tex->alpha     = (u_char)(tex->key_alpha[0]
                                  + (int)((float)(tex->key_alpha[1] - tex->key_alpha[0]) * f));
        tex->blur_scw  = (u_char)(tex->key_blur_scw[0]
                                  + (int)((float)(tex->key_blur_scw[1] - tex->key_blur_scw[0]) * f));
    }
    else
    {
        t_keep = t_in + (all * tex->keep_time) / sum;
        if (done < t_keep)
        {
            tex->alpha     = (u_char)tex->key_alpha[1];
            tex->blur_scw  = (u_char)tex->key_blur_scw[1];
            f              = 1.0f;
            tex->scale_add = (float)tex->key_scale[1];
            tex->blur_sch  = (float)tex->key_blur_sch[1];
            tex->blur_rot  = (float)tex->key_blur_rot[1];
        }
        else
        {
            if (tex->out_time != 0)
                f = 1.0f - (float)(done - t_keep) / (float)((all * tex->out_time) / sum);
            tex->scale_add = (float)(tex->key_scale[1] - tex->key_scale[2]) * f + (float)tex->key_scale[2];
            tex->blur_sch  = (float)(tex->key_blur_sch[1] - tex->key_blur_sch[2]) * f + (float)tex->key_blur_sch[2];
            tex->blur_rot  = (float)(tex->key_blur_rot[1] - tex->key_blur_rot[2]) * f + (float)tex->key_blur_rot[2];
            tex->alpha     = (u_char)(tex->key_alpha[2]
                                      + (int)((float)(tex->key_alpha[1] - tex->key_alpha[2]) * f));
            tex->blur_scw  = (u_char)(tex->key_blur_scw[2]
                                      + (int)((float)(tex->key_blur_scw[1] - tex->key_blur_scw[2]) * f));
        }
    }

    f = f * 128.0f;
    tex->blur_rot  = tex->blur_rot / 100.0f;
    tex->scale_add = tex->scale_add / 100.0f;
    tex->aref      = (u_char)(0x80 - (int)f);
    tex->blur_sch  = tex->blur_sch / 100.0f;
}

// FF3 0x002b8178: the dither pulse over 30 frames (FF2 inlined it): up over
// 5, held to 24, down over 5, then off; 35 * that + 67.
float ScreenSaverDitherAlphaGet(int t)
{
    float a;

    if (t < 5)
    {
        a = (float)t / 5.0f;
    }
    else
    {
        a = 1.0f;
        if (0x18 < t)
        {
            if (t < 0x1e)
                a = 1.0f - (float)(t - 0x19) / 5.0f;
            else
                a = 0.0f;
        }
    }
    return a * 35.0f + 67.0f;
}
