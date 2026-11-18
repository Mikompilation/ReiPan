/* ==========================================================================
 *  ingame/photo/finder_view.h
 *
 *  The viewfinder camera and the camera HUD's sprite helpers (FF3-only, FF3
 *  0x1b6f20..0x1bb4c8; the object file ends with the static initializer
 *  pair 0x1b7998/0x1bb4a8 that fills the HUD sprite table 0x3252b0).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_FINDER_VIEW_H
#define _INGAME_PHOTO_FINDER_VIEW_H

#include "sdk/scetypes.h"
#include "graphics/graph2d/g2d_draw.h"  // DISP_SPRT, SPRT_DAT

struct PLYR_WRK;                        // ingame/plyr/player.h

// The camera HUD's sprite table, data 0x3252b0: zero in the ROM, filled at
// start-up by the static initializer 0x1b7998 (FinderViewStaticInit, called
// by FinderViewGlobalCtor 0x1bb4a8; here an aggregate initializer). It ends
// where finder_view_hud.c's data begins (0x327190 = 0x3252b0 + 0xf7 * 0x20);
// sprite number 0xf7 means "none" (HudPartDraw).
#define HUD_SPRT_NUM    0xf7
extern SPRT_DAT hud_sprt[HUD_SPRT_NUM];

// The sprite helpers every camera HUD draw goes through. x, y: position;
// a: alpha scale (times the table's alpha, clamped to 0..255); sx, sy:
// scale (scw, sch); rot: rotation (DISP_SPRT.rot); ds: the caller's sprite
// buffer; no: a hud_sprt entry; blend: row of the u_long table 0x3596e8
// for DISP_SPRT.alphar (0 0x44, 1 0x48 additive, 2 0x42, 3 0x84).
void HudSprtSetBlend(DISP_SPRT *ds, int blend);                         // FF3 0x001b7360
// HudSprtSet at (x, y) + the table entry's (x, y).
void HudSprtSetOfs(float x, float y, float a, float sx, float sy, float rot,
                   DISP_SPRT *ds, int no);                              // FF3 0x001b7380
// HudSprtSetOfs, blend, colour, DispSprD.
void HudSprtDrawOfs(float x, float y, float a, float sx, float sy, float rot,
                    DISP_SPRT *ds, int no, int blend, u_char r, u_char g, u_char b);   // FF3 0x001b73c8
// CopySprDToSpr of the entry, top-left and rotation / scale centre (x, y).
void HudSprtSet(float x, float y, float a, float sx, float sy, float rot,
                DISP_SPRT *ds, int no);                                 // FF3 0x001b7438
// HudSprtSet, blend, colour, DispSprD.
void HudSprtDraw(float x, float y, float a, float sx, float sy, float rot,
                 DISP_SPRT *ds, int no, int blend, u_char r, u_char g, u_char b);      // FF3 0x001b7518
// Like HudSprtSet, centred on (x, y).
void HudSprtSetCenter(float x, float y, float a, float sx, float sy, float rot,
                      DISP_SPRT *ds, int no);                           // FF3 0x001b7588
// HudSprtSetCenter, blend, colour, DispSprD.
void HudSprtDrawCenter(float x, float y, float a, float sx, float sy, float rot,
                       DISP_SPRT *ds, int no, int blend, u_char r, u_char g, u_char b); // FF3 0x001b76a8
// One quadrant mirrored into four around (x, y): the sprite's bottom-right
// corner on (x, y), drawn with (scw, sch) = (sx, sy), (-sx, sy), (sx, -sy),
// (-sx, -sy).
void HudSprtDrawMirror4(float x, float y, float a, float sx, float sy, float rot,
                        DISP_SPRT *ds, int no, int blend, u_char r, u_char g, u_char b); // FF3 0x001b7718
// 2: player 1's camera (PlyrCamAble); 1: the flash (PlyrFlashAble); 0: none.
int  HudGetCamType(void);               // FF3 0x001b7868
// PhotoModeGetViewAlpha(GetPhotoModeWrk()) != 0: the viewfinder is up.
int  HudIsViewOn(void);                 // FF3 0x001b78a0

// Sends the camera HUD's LANG_TEX pictures (LangTexSend 0x21, 0x22; the
// twenty viewfinder ones too while HudIsViewOn). PhotoModeWrkDraw passes
// mode != PHOTO_MODE_FINDER, which FF3's body ignores.
void HudTexSend(int real);              // FF3 0x001b78c8

// The viewfinder camera (CameraMain): the eye at the current player's pos
// raised by crawl_h and turned by rot, looking 1000 ahead along
// (finder_pitch, rot[1]); fov PhotoModeGetZoom * 0.835 * (1.85 - the
// shutter-chance display rate). Runs the view turn first (FinderViewTurnCtrl,
// or with flags2 0x200000 the timed turn by view_turn). PLYR_WRK _0e0 bit 0
// / bit 1 hand the frame to CameraSetFromRotPair / CatViewCameraSet.
void FinderViewCameraCtrl(void);        // FF3 0x001b6f20
// One frame of the view turn: with PLYR_FLG_VIEW_EASE the pitch / yaw ease
// from view_from by 0.5 * view_turn * t^2 until t (_000, counting up) reaches
// the time FinderViewSetTurnTime set; else rot[1] += view_turn[1] while _000
// counts down.
void FinderViewTurnCtrl(void);          // FF3 0x001b71b8
void FinderViewHook0(PLYR_WRK *p, PLYR_WRK *q);     // FF3 0x001b7168: empty
void FinderViewHook1(PLYR_WRK *p, PLYR_WRK *q);     // FF3 0x001b7160: empty
void FinderViewSetTurnTime(float time); // FF3 0x001b7170: the eased turn's length in frames
// _000 = 10 frames, view_turn[1] (the whole turn) / 10 per frame.
void FinderViewTurnStart(void);         // FF3 0x001b7178
void FinderViewTurnReq(void);           // FF3 0x001b7330: FinderViewTurnStart
void FinderViewTurnMain(void);          // FF3 0x001b7348: FinderViewTurnCtrl

#endif /* _INGAME_PHOTO_FINDER_VIEW_H */
