/* ==========================================================================
 *  system/b2d/B2dSpr.h
 *
 *  FF3's B2d sprite parts (file string "B2dSpr.c", 0x105998..0x106370): the
 *  sine / cosine tables the core rotates and eases with, per-handle part
 *  freezing (game code that moves parts itself freezes them), and setters
 *  for an item's part values.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DSPR_H
#define _SYSTEM_B2D_B2DSPR_H

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM

#define B2D_TRIG_TBL    0x400           // a full turn; values are 0x1000 = 1.0

extern int b2d_sin_tbl[B2D_TRIG_TBL];   // 0x375730
extern int b2d_cos_tbl[B2D_TRIG_TBL];   // 0x376730

// ---- trigonometry ----
int  B2dSin(int angle);                                 // angle & 0x3ff
int  B2dCos(int angle);
void B2dRotatePoints(int *xy, int n, int cx, int cy, int deg);  // n x/y pairs around cx, cy; deg 0 = none
void B2dMakeSinCosTbl(int *sin_tbl, int *cos_tbl);
void B2dTrigInit(void);                                 // B2dSystemInit
void B2dTrigEnd(void);                                  // B2dSystemEnd

// ---- freezing (part flags bit 0) ----
// mode 0 freezes, 1 thaws, anything else does nothing; -1 for a bad handle.
int B2dSprFreezePart(int handle, int id, int part, int mode);
int B2dSprFreezePartByName(int handle, const char *name, int part, int mode);
int B2dSprFreezeItem(int handle, int id, int mode);     // parts 6..17 (the transform and look, not the source rect)
int B2dSprFreezeItemByName(int handle, const char *name, int mode);
int B2dSprFreezeAll(int handle, int mode);

// ---- one item (error print and nothing done without one) ----
int  B2dSprSetDisp(B2D_ITEM *item, int disp);           // 0 hides, 1 shows; -1 without an item
void B2dSprSetPos(B2D_ITEM *item, int x, int y);        // parts 6 / 7
void B2dSprSetScale(B2D_ITEM *item, int sx, int sy);    // parts 8 / 9
void B2dSprSetAlpha(B2D_ITEM *item, int alpha);         // part 10
void B2dSprSetText(B2D_ITEM *item, int text);           // part 11
void B2dSprSetRepeat(B2D_ITEM *item, int x, int y);     // parts 12 / 13
void B2dSprSetAnchor(B2D_ITEM *item, int x, int y);     // parts 14 / 15
void B2dSprSetRot(B2D_ITEM *item, int deg);             // part 16
void B2dSprSetRgb(B2D_ITEM *item, u_int r, u_int g, u_int b);   // part 17, alpha byte 0x80

// Value of part `no` (< 18); -1 without an item, -2 for a bad part. Part 10
// is the item's alpha (capCtl::Update).
int    B2dSprGetPartData(B2D_ITEM *item, u_int no);
u_short B2dSprGetKind(B2D_ITEM *item);                  // draw kind 0..3, 0 without an item
B2D_ITEM *B2dSprGetParent(int handle, B2D_ITEM *item);  // FF3 0x00106320: the item of its part 5 id, nullptr for -1

#endif /* _SYSTEM_B2D_B2DSPR_H */
