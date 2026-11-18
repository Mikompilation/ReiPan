/* ==========================================================================
 *  graphics/effect/effect_scrsaver.h
 *
 *  The screen saver and the old-film noise (FF3-only file; the name is
 *  invented). FF2 keeps its screen saver in effect_scr.c; FF3 moved it
 *  here, after the map particles, and gave it a film look:
 *
 *    * Saver    150 seconds without pad input (9000 frames, PAL 7500) load
 *               SCREENSAVER_PK4 / SCREENSAVER2_PK4 and show one picture at
 *               a time at a random spot, dissolving in and out by alpha
 *               test, with a blur, a pulsing dither and a flicker; any input
 *               frees it (FF2 ScreenSaverInit / Main / OneTexInit / Draw).
 *    * Film     dust specks and drifting scratch lines from the language
 *               pack's sheet (LangTexSend 0, 128x128 PSMT4 at 0x2bc0), a
 *               short vertical shake of the frame and a frame copy drawn
 *               back over the screen. Effect kind 0x11 (effect_oth.c) uses
 *               the dust / lines and the shake on their own.
 *
 *  FF3 0x17f8b8..0x1806a8 (ends with its static initializer 0x180360 and
 *  _GLOBAL__I_ 0x180680) and the out-of-line bodies 0x2b6ee0..0x2b8220.
 * ======================================================================== */
#ifndef _GRAPHICS_EFFECT_EFFECT_SCRSAVER_H
#define _GRAPHICS_EFFECT_EFFECT_SCRSAVER_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// One picture of the saver (FF2 SCREEN_SAVER_TEX, 0x2c; FF3 adds the blur
// and the keyframes of its in / keep / out envelope). Keyframe triples are
// [0] start, [1] held, [2] end; ScreenSaverOneTexStep lerps between them.
struct SCRSAVER_TEX                     /* 0x50 */
{
    /* 0x00 */ float  scale;            // 2.2..3.2
    /* 0x04 */ float  scale_add;        // key_scale / 100, added to scale
    /* 0x08 */ short  base_x;           // 0..590
    /* 0x0a */ short  base_y;           // 0..398
    /* 0x0c */ short  pos_x;            // base + a jitter every frame
    /* 0x0e */ short  pos_y;
    /* 0x10 */ int    in_time;          // envelope weights: each phase gets
    /* 0x14 */ int    keep_time;        // disp_time_all * weight / sum
    /* 0x18 */ int    out_time;
    /* 0x1c */ int    disp_time;        // frames left
    /* 0x20 */ int    disp_time_all;
    /* 0x24 */ u_char alpha;
    /* 0x25 */ u_char aref;             // alpha test reference: 0x80 hidden .. 0 whole
    /* 0x26 */ u_char blur_scw;         // SubBlur's first scale; 0: no blur
    /* 0x28 */ float  blur_sch;         // key_blur_sch / 100
    /* 0x2c */ float  blur_rot;         // key_blur_rot / 100
    /* 0x30 */ short  key_alpha[3];
    /* 0x36 */ short  key_scale[3];
    /* 0x3c */ short  key_blur_scw[3];
    /* 0x42 */ short  key_blur_sch[3];
    /* 0x48 */ short  key_blur_rot[3];
};
static_assert(sizeof(SCRSAVER_TEX) == 0x50, "SCRSAVER_TEX: 0x50 bytes (ScreenSaverDraw steps 0x50)");
static_assert(offsetof(SCRSAVER_TEX, in_time) == 0x10, "SCRSAVER_TEX.in_time");
static_assert(offsetof(SCRSAVER_TEX, disp_time) == 0x1c, "SCRSAVER_TEX.disp_time");
static_assert(offsetof(SCRSAVER_TEX, alpha) == 0x24, "SCRSAVER_TEX.alpha");
static_assert(offsetof(SCRSAVER_TEX, blur_sch) == 0x28, "SCRSAVER_TEX.blur_sch");
static_assert(offsetof(SCRSAVER_TEX, key_alpha) == 0x30, "SCRSAVER_TEX.key_alpha");
static_assert(offsetof(SCRSAVER_TEX, key_blur_scw) == 0x3c, "SCRSAVER_TEX.key_blur_scw");
static_assert(offsetof(SCRSAVER_TEX, key_blur_rot) == 0x48, "SCRSAVER_TEX.key_blur_rot");

// One scratch line of the film noise.
struct EFF_FILM_LINE                    /* 0x18 */
{
    /* 0x00 */ int    life;             // frames left; <= 0: free
    /* 0x04 */ float  x;
    /* 0x08 */ float  y;                // scrolls the strip's v
    /* 0x0c */ float  dx;               // drift per frame (times 8..12)
    /* 0x10 */ float  dy;
    /* 0x14 */ u_char type;             // row of eff_film_line_sprt
};
static_assert(sizeof(EFF_FILM_LINE) == 0x18, "EFF_FILM_LINE: 0x18 bytes (EffFilmLineInit memsets 0x18)");
static_assert(offsetof(EFF_FILM_LINE, dy) == 0x10, "EFF_FILM_LINE.dy");
static_assert(offsetof(EFF_FILM_LINE, type) == 0x14, "EFF_FILM_LINE.type");

// The film's vertical shake (a 4-frame sine pulse).
struct EFF_FILM_SHAKE                   /* 0x14 */
{
    /* 0x00 */ int    cnt;              // frames left
    /* 0x04 */ short  dx;               // always 0 (the ROM scales the pulse by 0)
    /* 0x06 */ short  dy;               // up to -85
    /* 0x08 */ float  scw;              // always 1
    /* 0x0c */ float  sch;              // 1..2
    /* 0x10 */ u_char alpha;            // up to 180
};
static_assert(sizeof(EFF_FILM_SHAKE) == 0x14, "EFF_FILM_SHAKE: 0x14 bytes (EffFilmShakeInit memsets 0x14)");
static_assert(offsetof(EFF_FILM_SHAKE, scw) == 0x08, "EFF_FILM_SHAKE.scw");
static_assert(offsetof(EFF_FILM_SHAKE, alpha) == 0x10, "EFF_FILM_SHAKE.alpha");

// The saver work, bss 0x3dee00 (ScreenSaverInit clears 0x170 bytes). FF2
// SCREEN_SAVER_CTRL's fields, the film noise and a flicker added.
// layout: host -- tex_buf is a pointer (EE 4 bytes at +0x128); the offsets
// before it match the ROM, the ints follow it.
struct SCRSAVER_CTRL                    /* 0x170 on the EE */
{
    /* 0x000 */ SCRSAVER_TEX   tex[1];
    /* 0x050 */ EFF_FILM_LINE  line[8];
    /* 0x110 */ EFF_FILM_SHAKE shake;
    /* 0x124 */ int            load_status;         // 0 counting, 1 loading, 2 playing
    /* 0x128 */ void          *tex_buf;             // the saver sheet (EFFECT_MALLOC)
    /* 0x12c */ int            load_id;
    /* 0x130 */ int            play_id;             // FF2's chirp; FF3 never sets it (-1)
    /* 0x134 */ int            counter;             // frames without input
    /* 0x138 */ int            interval_time;       // frames between two pictures
    /* 0x13c */ int            disp_time;           // frames the picture stays
    /* 0x140 */ int            dither_interval;     // frames between two dither pulses
    /* 0x144 */ float          dither_alpha;
    /* 0x148 */ int            dither_change_time;  // frames left of a pulse (30)
    /* 0x14c */ int            flicker_cnt;         // 2, 1, 0: flicker_alpha 30 on one frame in 3
    /* 0x150 */ int            flicker_wait;
    /* 0x154 */ u_char         flicker_alpha;
    /* 0x155 */ u_char         _155[0x1b];
};
static_assert(offsetof(SCRSAVER_CTRL, line) == 0x50, "SCRSAVER_CTRL.line");
static_assert(offsetof(SCRSAVER_CTRL, shake) == 0x110, "SCRSAVER_CTRL.shake");
static_assert(offsetof(SCRSAVER_CTRL, load_status) == 0x124, "SCRSAVER_CTRL.load_status");
static_assert(offsetof(SCRSAVER_CTRL, tex_buf) == 0x128, "SCRSAVER_CTRL.tex_buf");
static_assert(offsetof(SCRSAVER_CTRL, load_id) == offsetof(SCRSAVER_CTRL, tex_buf) + sizeof(void *),
              "SCRSAVER_CTRL.load_id follows tex_buf (EE 0x12c)");
static_assert(offsetof(SCRSAVER_CTRL, flicker_alpha) == offsetof(SCRSAVER_CTRL, load_id) + 0x28,
              "SCRSAVER_CTRL.flicker_alpha (EE 0x154)");
static_assert(sizeof(SCRSAVER_CTRL) == 0x170 || sizeof(void *) == 8,
              "SCRSAVER_CTRL: 0x170 bytes on the EE");

// The saver (InitEffects / InitEffectsEF / EffectControl).
void ScreenSaverInit(void);                                         // FF3 0x0017f8b8
void ScreenSaverMain(void);                                         // FF3 0x0017f900
void ScreenSaverDraw(void);                                         // FF3 0x0017fdb0

// The film noise (ScreenSaverDraw and effect kind 0x11, FUN_0017b970).
void EffFilmLineInit(void);                                         // FF3 0x00180068
void EffFilmNoiseDraw(float dust_rate, float line_rate, u_char dust_alp, u_char line_alp);  // FF3 0x001800c0
void EffFilmShakeInit(void);                                        // FF3 0x001801a0
void EffFilmShakeDraw(float rate);                                  // FF3 0x001801c8

// The out-of-line bodies.
void  ScreenSaverOneTexInit(SCRSAVER_TEX *tex, int disp_time);      // FF3 0x002b6ee0
void  ScreenSaverPlayMain(SCRSAVER_CTRL *ctrl);                     // FF3 0x002b7358
void  EffFilmFrameCopyDraw(float x, float y, float scw, float sch, u_char alp, int blend);  // FF3 0x002b75e8
void  EffFilmDustDraw(float rate, u_char alp, int blend);           // FF3 0x002b7738
void  EffFilmLineDraw(EFF_FILM_LINE *ln, u_char alp, int blend);    // FF3 0x002b79d8
void  EffFilmLineStep(float rate, EFF_FILM_LINE *ln);               // FF3 0x002b7bc0
void  ScreenSaverOneTexStep(SCRSAVER_TEX *tex);                     // FF3 0x002b7e58
float ScreenSaverDitherAlphaGet(int t);                             // FF3 0x002b8178

#endif // _GRAPHICS_EFFECT_EFFECT_SCRSAVER_H
