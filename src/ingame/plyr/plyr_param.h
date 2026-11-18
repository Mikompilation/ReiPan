/* ==========================================================================
 *  ingame/plyr/plyr_param.h
 *
 *  Per-player parameter tables (FF3-only, no FF2 counterpart): FF3
 *  0x2394a8..0x239ac0, between player.c and plyr_mdl.c. One rodata block
 *  0x361270..0x3613e8 holds, per player (PlyrGetCurNo: 0..2), a few rates
 *  and, per camera level, the camera's numbers; the levels are the three
 *  bytes PLYR_WRK.equip[] (kept over a new game, raised by the camera
 *  upgrade items: FUN_0010a510 tail-calls the *Up functions).
 *
 *  Getters taking (plyr, lv) read 0xff as "the current player" and "the
 *  player's own level".
 * ======================================================================== */
#ifndef _INGAME_PLYR_PLYR_PARAM_H
#define _INGAME_PLYR_PLYR_PARAM_H

#include "sdk/scetypes.h"

// PLYR_WRK.equip[] indices: the three camera levels, 0..PLYR_CAM_LV_MAX.
#define PLYR_CAM_LV_CHARGE 0            // charge steps (PlyrGetChargeMax)
#define PLYR_CAM_LV_RANGE  1            // shot distance, charge speed
#define PLYR_CAM_LV_FINDER 2            // capture circle scale / radius
#define PLYR_CAM_LV_MAX    3
#define PLYR_CAM_LV_NUM    4

#define PLYR_PARAM_CUR     0xff         // plyr / lv / diff: the current one

float  PlyrGetEneWinRate(void);         // FF3 0x002394a8: 1, 0.85, 0.92; enemy motion script windows scale by it
float  PlyrGetChargeScale(void);        // FF3 0x002394e0: 0.7, 1, 0.5; PhotoChargerSetup
float  PlyrGetShotRate(void);           // FF3 0x00239518: 1 for all; FUN_0023cad0 (not player 2)
u_char PlyrGetChargeDrain(void);        // FF3 0x00239550: 5, 0, 10 per frame; PhotoWrk1e8Drain
float  PlyrGetChargeStepRate(u_char step); // FF3 0x00239580: the rate for a fifth (0..4) of the charge
u_char PlyrGetChargeMax(u_char plyr, u_char lv);     // FF3 0x002395c8: charge steps (4..11; player 2: 1)
float  PlyrGetShotRange(u_char plyr, u_char lv);     // FF3 0x00239640: shot distance (70..140)
u_char PlyrGetChargeSpeed(u_char plyr, u_char lv);   // FF3 0x002396b8: charge speed (35..110)
float  PlyrGetFinderRadius(u_char plyr, u_char lv);  // FF3 0x00239730: capture circle radius on screen (scale * 125)
float  PlyrGetFinderScale(u_char plyr, u_char lv);   // FF3 0x002397b8: capture circle scale (0.65..1); the HUD draws by it
u_char PlyrGetChargeSpeedEquip(u_char lv);           // FF3 0x00239830: 40..70; lv 0xff: PlyrGetEquipLv(6)
u_char PlyrGetCamLvCharge(u_char plyr); // FF3 0x00239878: equip[PLYR_CAM_LV_CHARGE]
u_char PlyrGetCamLvRange(u_char plyr);  // FF3 0x002398a0: equip[PLYR_CAM_LV_RANGE]
u_char PlyrGetCamLvFinder(u_char plyr); // FF3 0x002398c8: equip[PLYR_CAM_LV_FINDER]
void   PlyrCamLvChargeUp(u_char plyr);  // FF3 0x002398f0: +1, up to PLYR_CAM_LV_MAX
void   PlyrCamLvRangeUp(u_char plyr);   // FF3 0x00239968
void   PlyrCamLvFinderUp(u_char plyr);  // FF3 0x002399e0
int    PlyrMdlGetHpMax(u_char plyr, u_char diff);    // FF3 0x00239a58: hp by player and difficulty; diff 0xff: the current one

#endif /* _INGAME_PLYR_PLYR_PARAM_H */
