/* ==========================================================================
 *  ingame/plyr/plyr_gauge.h
 *
 *  FF3 0x129968..0x12a068 (FF3-only; file name invented): player 1's (Miku,
 *  PlyrGetCurNo() == 1) slow-mode gauge, kept in the photo mode work (+0x3c0,
 *  GetAlbumSaveDisp). Damage fills it (PlyrGaugeAdd from PlyrDamage, times
 *  PlyrEquip3Rate) up to 1000; with 50 or more the camera button starts the
 *  slow mode (PlyrGaugeSlowStart, player flag 0x100000), which drains it
 *  every frame (times PlyrEquip4Rate) and ends at 0 (PlyrGaugeSlowEnd).
 *  While it is on the player's timers run at 0.67 (PlyrGaugeTimeStep) and a
 *  hold hurts at 0.34 (PlyrGaugeDmgRate). Lens function 8 keeps it full.
 *  Drawn on the HUD by PlyrGaugeDraw (finder_photo.c). Was inside
 *  album_save.c's inferred range until 2026-10-14 (0x129968 was a wrong low
 *  FF2 match, AlbumSaveDispInit; the photo mode work field keeps that name).
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLYR_GAUGE_H
#define _INGAME_PLYR_PLYR_GAUGE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define PLYR_GAUGE_MAX      1000.0f     // the fill's cap (PlyrGaugeAdd; lens function 8 sets it)
#define PLYR_GAUGE_COST     50.0f       // the slow mode needs and spends this much (PlyrGaugeSlowStart)

struct PLYR_GAUGE                       /* 0xc */
{
    /* 0x00 */ float disp;              // the shown fill, 0..1: eases toward fill / 1000 by <= 0.02 a frame
    /* 0x04 */ float fill;              // 0..1000
    /* 0x08 */ int   se_no;             // the slow mode's loop SE (SndSePlay id), -1: none
};
static_assert(sizeof(PLYR_GAUGE) == 0xc, "PLYR_GAUGE is 0xc bytes");
static_assert(offsetof(PLYR_GAUGE, fill) == 0x04, "PLYR_GAUGE.fill");
static_assert(offsetof(PLYR_GAUGE, se_no) == 0x08, "PLYR_GAUGE.se_no");

void  PlyrGaugeInit(PLYR_GAUGE *gauge);                                 // FF3 0x00129968
void  PlyrGaugeAdd(float val, PLYR_GAUGE *gauge);                       // PlyrDamage: dmg * rate * 0.07
void  PlyrGaugeSub(float val, PLYR_GAUGE *gauge);                       // at 0: PlyrGaugeSlowEnd
float PlyrGaugeGet(PLYR_GAUGE *gauge);                                  // the fill (player 1), else 0
float PlyrGaugeGetRate(PLYR_GAUGE *gauge);                              // the fill / 1000 (player 1), else 0
void  PlyrGaugeSlowStart(PLYR_GAUGE *gauge);                            // player 1's camera button (PlyrActionCtrl; flag 0x100000 clear)
void  PlyrGaugeSlowEnd(PLYR_GAUGE *gauge);                              // the same with flag 0x100000
void  PlyrGaugeSeStop(PLYR_GAUGE *gauge);                               // stops its loop SE (PlyrSeStopAll)
void  PlyrGaugeDraw(float x, float y, PLYR_GAUGE *gauge, int alpha);    // HUD; alpha 0..0x80 (0..100 with the viewfinder)
void  PlyrGaugeMain(PLYR_GAUGE *gauge);                                 // per frame (PhotoModeWrkFinderMain / RealMain)
float PlyrGaugeTimeStep(PLYR_GAUGE *gauge);                             // the player's timer step (PlayerMain, PlyrStatusCtrl, ...)
float PlyrGaugeDmgRate(PLYR_GAUGE *gauge);                              // the hold damage rate (0.34 in the slow mode)
void *PlyrGaugeGetSaveData(PLYR_GAUGE *gauge);                          // its save block: disp and fill (8 bytes)

#endif /* _INGAME_PLYR_PLYR_GAUGE_H */
