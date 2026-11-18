/* ==========================================================================
 *  graphics/graph2d/fade.c
 *
 *  Full-screen colour fade (FF2 fade.c, ported from MioPan and checked
 *  against FF3 function by function). One fade-control block holds the fade
 *  colour, the overlay alpha, the per-frame alpha step and a small state
 *  machine; FadeMain() draws a 640x448 square of the fade colour at the
 *  current alpha each frame and advances the state.
 *
 *  FF3 differences from FF2: FadeMain lost the "ERROR!! FadeMain()" default
 *  case; GetFadeAlpha (0x1cb0e8) is new.
 *
 *  FF3 0x1cae98..0x1cb110.
 * ======================================================================== */
#include "graphics/graph2d/fade.h"
#include "graphics/graph2d/g2d_draw.h"  // DISP_SQAR / SQAR_DAT / CopySqrDToSqr / DispSqrD

#include <string.h>                     // memset

FADE_MODE_CTRL fade_ctrl;               // data 0x327550

// Reset to idle, overlay black and fully covering (alpha 0x80): FadeMain
// draws a black screen until the first FadeInReq (FF2 does the same).
// FF3 0x001cae98
void FadeCtrlInit(void)
{
    memset(&fade_ctrl, 0, sizeof(FADE_MODE_CTRL));
    fade_ctrl.alpha = 0x80;
}

// Per-frame fade: build the full-screen square at the current alpha (before
// stepping), step the state machine, draw the square unless alpha is 0.
// FF3 0x001caed8
void FadeMain(void)
{
    DISP_SQAR dsq;
    SQAR_DAT  fade_bg;

    fade_bg.w = 640;
    fade_bg.h = 448;
    fade_bg.x = 0;
    fade_bg.y = 0;
    fade_bg.pri = 0;
    fade_bg.r = fade_ctrl.r;
    fade_bg.g = fade_ctrl.g;
    fade_bg.b = fade_ctrl.b;
    fade_bg.alpha = fade_ctrl.alpha;
    CopySqrDToSqr(&dsq, &fade_bg);

    switch (fade_ctrl.fade_state)
    {
    case 1:                             // fading in: alpha -> 0
        fade_ctrl.alpha = fade_ctrl.alpha - fade_ctrl.change_alp;
        if ((int)((u_int)fade_ctrl.alpha - (u_int)fade_ctrl.change_alp) < 1)
        {
            fade_ctrl.fade_state = 0;
            fade_ctrl.alpha = 0;
        }
        break;
    case 3:                             // fading out: alpha -> 0x80
        fade_ctrl.alpha = fade_ctrl.alpha + fade_ctrl.change_alp;
        if (fade_ctrl.alpha & 0x80)
        {
            fade_ctrl.fade_state = 0;
            fade_ctrl.alpha = 0x80;
        }
        break;
    }

    if (fade_ctrl.alpha != 0)
    {
        DispSqrD(&dsq);
    }
}

// 1 once the fade has finished (state back to idle).
// FF3 0x001cafe8
int GetFadeState(void)
{
    return fade_ctrl.fade_state == 0;
}

// Fade in from the given colour over fade_in_time frames (0 = at once).
// FF3 0x001caff8
void FadeInReq(u_char r, u_char g, u_char b, u_int fade_in_time)
{
    fade_ctrl.alpha = 0x80;
    fade_ctrl.r = r;
    fade_ctrl.g = g;
    fade_ctrl.b = b;
    fade_ctrl.fade_state = 1;

    if (fade_in_time == 0)
    {
        fade_ctrl.fade_state = 0;
        fade_ctrl.alpha = 0;
        fade_ctrl.change_alp = 0;
        return;
    }

    if (fade_in_time > 0x80)
    {
        fade_ctrl.change_alp = 1;
        return;
    }

    fade_ctrl.change_alp = (u_char)(0x80 / (int)fade_in_time);
}

// Fade out to the given colour over fade_out_time frames (0 = at once). The
// remainder of 0x80 / time starts the alpha so the last step lands on 0x80.
// FF3 0x001cb068
void FadeOutReq(u_char r, u_char g, u_char b, u_int fade_out_time)
{
    fade_ctrl.fade_state = 3;
    fade_ctrl.alpha = 0;
    fade_ctrl.r = r;
    fade_ctrl.g = g;
    fade_ctrl.b = b;

    if (fade_out_time == 0)
    {
        fade_ctrl.fade_state = 0;
        fade_ctrl.change_alp = 0;
        fade_ctrl.alpha = 0x80;
        return;
    }

    if (fade_out_time > 0x80)
    {
        fade_ctrl.change_alp = 1;
        return;
    }

    fade_ctrl.change_alp = (u_char)(0x80 / (int)fade_out_time);
    fade_ctrl.alpha = (u_char)(0x80 % (int)fade_out_time);
}

// The overlay alpha, or its complement (0x80 - alpha) when invert is set:
// 0x227080 (photo_dat.c) puts the complement into the common fade
// layer so that layer fades opposite to the overlay.
// FF3 0x001cb0e8
u_char GetFadeAlpha(int invert)
{
    if (invert != 0)
    {
        return (u_char)(0x80 - fade_ctrl.alpha);
    }
    return fade_ctrl.alpha;
}
