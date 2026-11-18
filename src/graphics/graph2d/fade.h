/* ==========================================================================
 *  graphics/graph2d/fade.h
 *
 *  Full-screen colour fade (FF2 fade.c): the fade-control block and the
 *  request / per-frame API. FF3 0x1cae98..0x1cb110.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH2D_FADE_H
#define _GRAPHICS_GRAPH2D_FADE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// Fade-control block (data 0x327550). fade_state: 0 idle, 1 fading in
// (alpha 0x80 -> 0), 3 fading out (alpha 0 -> 0x80). alpha is the overlay's
// GS alpha, 0x80 = fully covered.
struct FADE_MODE_CTRL                   /* 0xc */
{
    /* 0x0 */ u_char fade_state;
    /* 0x1 */ u_char alpha;
    /* 0x2 */ u_char change_alp;        // alpha step per frame
    /* 0x3 */ u_char r;
    /* 0x4 */ u_char g;
    /* 0x5 */ u_char b;
    /* 0x8 */ int    timer;             // FF2 field; FF3 never reads it
};
static_assert(sizeof(FADE_MODE_CTRL) == 0xc, "FADE_MODE_CTRL is 0xc bytes on the PS2");
static_assert(offsetof(FADE_MODE_CTRL, alpha) == 0x1, "FADE_MODE_CTRL.alpha");
static_assert(offsetof(FADE_MODE_CTRL, change_alp) == 0x2, "FADE_MODE_CTRL.change_alp");
static_assert(offsetof(FADE_MODE_CTRL, r) == 0x3, "FADE_MODE_CTRL.r");
static_assert(offsetof(FADE_MODE_CTRL, b) == 0x5, "FADE_MODE_CTRL.b");

extern FADE_MODE_CTRL fade_ctrl;        // data 0x327550

void   FadeCtrlInit(void);                                          // FF3 0x001cae98
void   FadeMain(void);                                              // FF3 0x001caed8
int    GetFadeState(void);                                          // FF3 0x001cafe8
void   FadeInReq(u_char r, u_char g, u_char b, u_int fade_in_time);   // FF3 0x001caff8
void   FadeOutReq(u_char r, u_char g, u_char b, u_int fade_out_time); // FF3 0x001cb068
u_char GetFadeAlpha(int invert);                                    // FF3 0x001cb0e8

#endif /* _GRAPHICS_GRAPH2D_FADE_H */
