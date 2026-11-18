/* ==========================================================================
 *  ingame/photo/finder_view_hud.h
 *
 *  The camera HUD parts (FF3-only, FF3 0x1bb4c8..0x1bf108, plus the
 *  out-of-line helpers 0x2b9610..0x2b9cb8 that use its sdata): drawn by
 *  PhotoModeWrkDraw (ingame/photo/finder_photo.c), the photo sub-works
 *  (fndr_effect.c) and the lens / enemy HUD (fndr_parts.c), all through the
 *  sprite helpers of ingame/photo/finder_view.c on its table hud_sprt.
 *  x, y: the HUD's offset from its home position (0, 0 for the HUD, the
 *  viewfinder point - (320, 224) for the viewfinder parts); a: alpha scale
 *  0..1 (a fade / 128). Sprite numbers are hud_sprt entries.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_FINDER_VIEW_HUD_H
#define _INGAME_PHOTO_FINDER_VIEW_HUD_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "graphics/graph2d/g2d_draw.h"  // DISP_SPRT

struct PHOTO_CHARGE_DISP;               // fndr_effect.h

// One part of a ring gauge (data 0x327190 / 0x3271b8 / 0x3271e0, one per
// gauge): a sprite placed by its centre in screen space, then moved and
// scaled as a whole by HudPartDraw.
#define HUD_PART_NONE   0xf7            // no: nothing to draw (HUD_SPRT_NUM)
struct HUD_PART                         /* 0x24 */
{
    /* 0x00 */ int    no;               // hud_sprt entry, HUD_PART_NONE
    /* 0x04 */ float  x;                // centre on the 640x448 screen
    /* 0x08 */ float  y;
    /* 0x0c */ float  rot;
    /* 0x10 */ float  sx;
    /* 0x14 */ float  sy;
    /* 0x18 */ u_char att;              // ORed into DISP_SPRT.att
    /* 0x19 */ u_char r;
    /* 0x1a */ u_char g;
    /* 0x1b */ u_char b;
    /* 0x1c */ float  alpha;            // times the draw's alpha
    /* 0x20 */ int    _20;              // 0 / 1 per part; nothing in this file reads it
};
static_assert(sizeof(HUD_PART) == 0x24, "HUD_PART is 0x24 bytes");
static_assert(offsetof(HUD_PART, rot) == 0x0c, "HUD_PART.rot");
static_assert(offsetof(HUD_PART, att) == 0x18, "HUD_PART.att");
static_assert(offsetof(HUD_PART, r) == 0x19, "HUD_PART.r");
static_assert(offsetof(HUD_PART, alpha) == 0x1c, "HUD_PART.alpha");
static_assert(offsetof(HUD_PART, _20) == 0x20, "HUD_PART._20");

// The viewfinder and the HUD frame (PhotoModeWrkFinder/RealDraw).
void HudGageFrameDraw(float x, float y, float a);               // sprites 0x20, 0x1e, 0x1f turned 90 degrees (viewfinder down)
void HudFinderFrameDraw(float x, float y, float a, int real);   // viewfinder frame: 0x30..0x39, 0x29, 0x27, 0x28 (+ 0x3a, 0x3c by lens function); real: 0x78..0x8b
void HudHpFrameDraw(float x, float y, float a);                 // HP frame: sprites 4, 0..3 (player 1's camera: 10, 6..9)
void HudHpGaugeDraw(float x, float y, float a, float disp, float tail, float pulse);   // HP gauge (HudHpDraw): sprite 5, tail red at a / 2
void HudSlowGaugeDraw(int col, float x, float y, float a, float rate);  // player 1's slow-mode gauge bar; col 0 / 1 (on) / 2 (low) picks the RGB row of 0x359790
void HudFinderCountDraw(float x, float y, float a, int film_type, int film_num, int can_shoot,
                        int flash_num, int flash_ready);        // film and flash counters in the viewfinder
void HudFlashCountDraw(float x, float y, float a, int flash_num, int flash_ready);   // flash counter on the HUD (viewfinder down)

// The photo mode work's +0x180 / +0x1e8 / +0x288 sub-works (fndr_effect.c).
// no: a sprite of the HUD set (no + 0x49, real camera: no + 0x8c); flag:
// the blend (HudSprtSetBlend); real: the "real" camera mode's sprite set.
void HudPointGlowDraw(float x, float y, float rot, float sx, float sy, int no,
                      u_char r, u_char g, u_char b, u_char a, int flag, int real);   // one mirrored sprite, scaled by the camera level's rate
void HudPointRingsDraw(float x, float y, float amp, float t, float rot, float sx, float sy, int no, int rings,
                       u_char r, u_char g, u_char b, u_char a, int flag, int real);  // 'rings' copies of a sprite, growing and fading with phase t (0..1)
void HudChargeStepsDraw(float x, float y, float a, float ramp, float sx, float sy, int max, int chr);   // the charge gauge's empty steps
void HudChargeGaugeDraw(float x, float y, float a, float sx, float sy, PHOTO_CHARGE_DISP *disp);      // the charge gauge (disp->max > 0)
void HudChargeFullDraw(float x, float y, float a, float ramp, float sx, float sy, u_char r, u_char g, u_char b);   // the full-charge glow (sprite 0x4b)
void HudCharge2Draw(float x, float y, float a, float sx, float sy, PHOTO_CHARGE_DISP *disp);      // player 1's second charge ring
void HudPanelNumDraw(float x, float y, float a, int num);       // +0x288's number (digits 0xb6.., tail 0xc0)
void HudPanelLevelDraw(float x, float y, float a, int level);   // +0x288's corner number by level (digits 0x9f.., tail 0xa9)
void HudPanelIconDraw(float x, float y, float a, int no);       // +0x288's icon 'no' (table 0x359708, < 0: none)
void HudPanelMark1Draw(float x, float y, float a);              // +0x288's corner mark, sprites 0x9b, 0x9c
void HudPanelMark2Draw(float x, float y, float a);              // +0x288's corner mark, sprites 0x9d, 0x9e

// The lens / enemy HUD (fndr_parts.c).
void HudEneMarkDraw(float x, float y, float a, float sx, float sy, int real);   // one enemy marker (EneMarkDraw)
void HudEneMarkCenterDraw(float x, float y, float a, float sx, float sy, int real);   // the marker centre at the finder point
void HudDamageNumDraw(float x, float y, float a, short damage);                 // damage number (DamageDispDraw)
void HudFilamentDraw(float x, float y, float a, float core, float whole, int type);  // filament; type 0 none, 1 battle, 2 auto / hint
void HudFilament2Draw(float x, float y, float a, int type);                     // filament, player 2's extra part
void HudDrawGage4bc(float x, float y, float a, float disp, float tail);         // the +0x4bc gauge (PhotoGage4bcDraw): max(disp, tail) red at a / 2, disp grey
void HudWrk274Draw(float x, float y, float a, float rate);                      // the +0x274 part (PhotoWrk274Draw): sprite 0x3d at a * rate
void HudLensSlotsDraw(float x, float y, float a, int slot_num);                 // lens equip slots
void HudLensSlotLensDraw(float x, float y, float a, float t, int slot_num, int sw_state);    // the slots' lenses, switch animation
void HudLensDraw(float x, float y, float a, float t, float usable, int equip, int sw_state);  // the shown lens
void HudLensMarkDraw(float x, float y, float a, float label, int equip);        // the lens's turning marks
void HudStoneSocketsDraw(float x, float y, float a, int stone_max);             // spirit-stone sockets
void HudStoneDraw(float x, float y, float a, float fill, float t, float rot, int i, int state);    // one spirit stone (LENS_ICON)

#endif /* _INGAME_PHOTO_FINDER_VIEW_HUD_H */
