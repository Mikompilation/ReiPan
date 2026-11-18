/* ==========================================================================
 *  ingame/photo/photo_make.h
 *
 *  Photo image production (FF2 photo_make.c; FF3 0x22ada0..0x22ca90, the
 *  drawing half -- FF2's CompressData / UncompressData are FF3's
 *  photo_comp.c, 0x228718..0x2289a8 between photo_dat.c and photo_obj.c):
 *    * capture: MakeSmallPhotoV (the 45x15 album thumbnail) and
 *      DrawSpecialFurnPhoto (a scenery shot's 384x128 page) re-draw the shown
 *      frame into their own VRAM page; CopyScreenToBuffer2 pulls a page down
 *      into EE memory;
 *    * presentation: DispPhotoFrame1 (one frame of phase 33's frozen picture
 *      and flash), DrawPhotoFrame (the 14-sprite border), DrawPhotoHinttex
 *      (the hint picture over it);
 *    * stored pictures: DrawPhotoBuffer uploads an EE picture to TBP 0x2bc0
 *      and draws it; the wrappers name the thumbnail and decode areas.
 *  The photo is 384x256 on screen, stored 2:1 squashed (384x128); the
 *  sequence (photo_dat.c) draws it with its corner at (128, 96).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_MAKE_H
#define _INGAME_PHOTO_PHOTO_MAKE_H

#include <stdint.h>                     // uintptr_t

#include "sdk/scetypes.h"

#define PHOTO_W             384         // stored picture, texels
#define PHOTO_H             128         // (drawn 2x high)
#define SPHOTO_W            45          // album thumbnail, texels
#define SPHOTO_H            15          // (drawn 2x high)

// EE work areas (FF3 constants; on PC ReiPan_GetHostPointer resolves them).
#define SPHOTO_AREA         0x1207000   // thumbnails, SPHOTO_ONE_SIZE each
#define SPHOTO_ONE_SIZE     0x1000
#define PHOTO_AREA          0x1217000   // encoded pictures (UncompressData's slots)
#define PHOTO_EXPAND_ADDR   0x117b000   // a decoded picture (g2d_draw.c's EE stash 1)

// DrawPhotoBuffer's ftype: the border drawn round the picture.
#define PHOTO_FTYPE_INSET   1           // UVs inset half a texel, no border
#define PHOTO_FTYPE_PANEL   2           // the sepia panel (DrawPhotoPanel)
#define PHOTO_FTYPE_FRAME   3           // DrawPhotoFrame

// DispPhotoFrame1's fl: 0 starts the flash (resets the statics), 1 runs it,
// 2 holds the frame, 3 the fading copy of the shown frame.
// DrawPhotoHinttex's flags.
#define PHOTO_HINT_FIX      0x01        // through its own alpha (ATST GEQUAL 0x80 - a, ALPHA FIX a)
#define PHOTO_HINT_SPREAD   0x02        // four copies spread while faint, at a third of the alpha
#define PHOTO_HINT_ADD      0x04        // additive (ALPHA 0x48)

// The stored picture at EE `addr` (mszw x mszh PSMCT32) uploaded to TBP
// 0x2bc0 in 100-line strips and drawn szw x szh at (x, y). pri, szfl and
// ztype are unread in FF3.
void DrawPhotoBuffer(u_int pri, uintptr_t addr, int szfl, int x, int y, int szw, int szh,
                     int mszw, int mszh, int ftype, u_char alp, int ztype);     // FF3 0x0022add8
// GS page addr_i (as LocalCopyLtoB type 3 stores it, 640x224) to EE addr_o:
// mw x mh texels from (mx, my / 2), at a 64-texel stride.
void CopyScreenToBuffer2(int addr_i, uintptr_t addr_o, int szfl, int mx, int my,
                         int mw, int mh);                                       // FF3 0x0022b1a0
void MakeSmallPhotoV(float x, float y);                                         // FF3 0x0022b2b0
void DrawSPhotoFromSmallPhotoArea(int n, int pri, int ftype, int x, int y,
                                  u_char alp);                                  // FF3 0x0022b528
void DrawSPhotoFromSmallPhotoAreaAddr(uintptr_t addr, int n, int pri, int ftype,
                                      int x, int y, u_char alp);                // FF3 0x0022b580
void DrawSPhotoFromSmallPhotoArea2(int n, int pri, int ftype, int x, int y,
                                   int szw, int szh, u_char alp);               // FF3 0x0022b5d8
void DrawSPhotoFromSmallPhotoAreaAddr2(uintptr_t addr, int n, int pri, int ftype,
                                       int x, int y, int szw, int szh, u_char alp); // FF3 0x0022b628
void DrawPhotoFromWorkArea(int pri, int ftype, int x, int y, int szw, int szh,
                           u_char alp);                                         // FF3 0x0022b688
void DrawPhotoFromWorkAreaAddr(uintptr_t addr, int pri, int ftype, int x, int y,
                               int szw, int szh, u_char alp);                   // FF3 0x0022b780
// One frame of the photo presentation; the picture's corner at (x, y),
// iPercent 0..100 the brightness of the frozen frame.
void DispPhotoFrame1(float x, float y, int fl, int iPercent);                  // FF3 0x0022b888
// The hint alpha of a fade step (0 in, 1 hold, 2 out) at a 50-frame cnt.
int  PhotoHintAlpha(int step, int cnt, int max);                               // FF3 0x0022c0d0
// The hint picture (TIM2 `tim2`, w x h) over the photo at (x, y).
void DrawPhotoHinttex(float x, float y, int w, int h, int alpha, void *tim2, int flags,
                      int special);                                             // FF3 0x0022c128
void DrawPhotoFrame(float x, float y);                                          // FF3 0x0022c4c0
void DrawSpecialFurnPhoto(float x, float y);                                    // FF3 0x0022c7d8

#endif /* _INGAME_PHOTO_PHOTO_MAKE_H */
