/* ==========================================================================
 *  ingame/photo/fndr_effect.h
 *
 *  Three more sub-works of the photo mode work (FF3-only code, FF3
 *  0x1c1418..0x1c2d80, between fly_ctrl.c and fndr_parts.c):
 *    - +0x180 (PHOTO_WRK_180): the finder point's glow -- three RGBA fades
 *      that walk to one of six colour rows chosen by six flag bits (enemy
 *      HUD, charger, shutter chance ...), a looping ripple phase and a 0..1
 *      rate that rises while an ordinary enemy is active;
 *    - +0x1e8 (PHOTO_WRK_1E8): the spirit charge -- 'charge' fills while the
 *      camera holds a ghost (rate by distance, screen radius and how full it
 *      is), 600 per step, up to a per-player / camera-level number of steps;
 *      player 1 (Miku) can run a second charge. Drawn at the finder point
 *      through the display struct data 0x3273c0 (PHOTO_CHARGE_DISP);
 *    - +0x288 (PHOTO_WRK_288): a pulse-in panel -- corner marks, up to 12
 *      item icons stacked 23 px apart and a number, each a CPulseVariable<0>.
 *  The file name is descriptive (no FF2 counterpart, no strings): FF3's link
 *  order is alphabetical by basename, and "fndr_effect" sorts between
 *  fly_ctrl.c and fndr_parts.c. Roles of the flag bits and of the +0x288
 *  items are read from their callers (enemy HUD, play data, player model),
 *  not from strings (open-items.md).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_FNDR_EFFECT_H
#define _INGAME_PHOTO_FNDR_EFFECT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/photo/fndr_parts.h"    // CPulseVariable, CFadeVariable, CWRK_VAR_CHAR_*

// ──────────────────────────────────────────────────────────────────────
// +0x180: the finder point's glow
// ──────────────────────────────────────────────────────────────────────

// PHOTO_WRK_180.flags (FF3 reads and writes the first word with masks; only
// bits 0..5 are used). The colour row PhotoWrk180Main picks: 0x20 -> 5,
// 0x10 -> 4, 0x08 -> 1, 0x01 / 0x02 -> 2, 0x04 -> 1, none -> 0.
#define PHOTO180_FLG_01     0x01        // PlayDataMain (PhotoWrk180SetBit0)
#define PHOTO180_FLG_02     0x02        // EnePhotoHudCtrl: the special enemy is on screen
#define PHOTO180_FLG_04     0x04        // EnePhotoHudCtrl: the shutter charger is ready for the target
#define PHOTO180_FLG_08     0x08        // no setter in this file
#define PHOTO180_FLG_10     0x10        // 0x23d178: charging a target with the charger ready
#define PHOTO180_FLG_20     0x20        // EnePhotoHudCtrl: the nearest target is in a shutter chance / has no gauge
#define PHOTO180_FLG_ALL    0x3f

#define PHOTO180_COL_GRP    3           // the three RGBA groups
#define PHOTO180_COL_ROW    6           // colour rows (tables data 0x327378 / 0x327390 / 0x3273a8)

struct PHOTO_WRK_180                    /* 0x50 */
{
    /* 0x00 */ u_char               flags;      // PHOTO180_FLG_*
    /* 0x01 */ CWRK_VAR_CHAR_0_100  wave;       // the ripple phase: 0..100 looping, delta 2
    /* 0x03 */ u_char               _03;
    /* 0x04 */ CFadeVariable<short> col[PHOTO180_COL_GRP][4];   // RGBA fades: [0] sprite 0, [1] sprite 1, [2] the ripple
    /* 0x4c */ float                rate;       // 0..1, +-0.2 a frame by PLYR_FLG_000020; > 0: the charge's SE and glow
};
static_assert(sizeof(PHOTO_WRK_180) == 0x50, "PHOTO_WRK_180 is 0x50 bytes");
static_assert(offsetof(PHOTO_WRK_180, wave) == 0x01, "PHOTO_WRK_180.wave");
static_assert(offsetof(PHOTO_WRK_180, col) == 0x04, "PHOTO_WRK_180.col");
static_assert(sizeof(CFadeVariable<short>) * 4 == 0x18, "PHOTO_WRK_180.col: groups at 0x04, 0x1c, 0x34");
static_assert(offsetof(PHOTO_WRK_180, rate) == 0x4c, "PHOTO_WRK_180.rate");

void PhotoWrk180Init(PHOTO_WRK_180 *w);                 // FF3 0x001c1af0
void PhotoWrk180SetBit0(PHOTO_WRK_180 *w, int on);      // FF3 0x001c1bc0 (PlayDataMain)
void PhotoWrk180SetBit1(PHOTO_WRK_180 *w, int on);      // FF3 0x001c1be0
void PhotoWrk180SetBit2(PHOTO_WRK_180 *w, int on);      // FF3 0x001c1c00
void PhotoWrk180SetBit4(PHOTO_WRK_180 *w, int on);      // FF3 0x001c1c40 (0x23d178)
void PhotoWrk180SetBit5(PHOTO_WRK_180 *w, int on);      // FF3 0x001c1c60
void PhotoWrk180Main(PHOTO_WRK_180 *w);                 // FF3 0x001c1c80 (camera up)
void PhotoWrk180Draw(float x, float y, PHOTO_WRK_180 *w, int alpha, int real);   // FF3 0x001c1e60: at the finder point; alpha 0..100

// ──────────────────────────────────────────────────────────────────────
// +0x1e8: the spirit charge
// ──────────────────────────────────────────────────────────────────────

#define PHOTO_CHARGE_STEP   600         // charge units per step

// C++ in FF3: PHOTO_MODE_WRK's constructor runs Init on it (its inline
// constructor). Draw / Add / DistRate / RadiusRate keep FF3's argument
// order: the floats go in f12.., the object in a0 as for every member.
struct PHOTO_WRK_1E8                    /* 0x38 */
{
    void Init();                            // FF3 0x001c2048 (PhotoWrk1e8Init; constructor, PhotoModeWrkInit, 0x23d178)
    void Reset();                           // FF3 0x001c2078 (PhotoWrk1e8Reset; 0x23b0b0)
    void  Draw(float x, float y, int alpha);                    // FF3 0x001c2090 (PhotoWrk1e8Draw): at the finder point; alpha 0..100
    float GetDispRate();                    // FF3 0x001c2550 (PhotoWrk1e8GetDispRate): disp + 0.85
    void FadeIn();                          // FF3 0x001c2568 (PhotoWrk1e8FadeIn; EnePhotoHudCtrl: an ordinary target)
    void FadeOut();                         // FF3 0x001c25a0 (PhotoWrk1e8FadeOut; a target in a shutter chance or without gauge)
    int GetNum();                           // FF3 0x001c25e8 (PhotoWrk1e8GetNum)
    int GetNumSub();                        // FF3 0x001c25f0 (PhotoWrk1e8GetNumSub)
    void  Add(float dist_rate, float rad_rate, float step_rate, float k, int speed);    // FF3 0x001c25f8 (PhotoWrk1e8Add)
    void Drain(u_char chr);                 // FF3 0x001c26b8 (PhotoWrk1e8Drain)
    void Main(int max);                     // FF3 0x001c2748 (PhotoWrk1e8Main)
    float DistRate(float dist, u_char chr, u_char lv);          // FF3 0x001c2b38 (PhotoWrk1e8DistRate)
    float RadiusRate(float rad, u_char chr, u_char lv);         // FF3 0x001c2be0 (PhotoWrk1e8RadiusRate)
    float StepRate(u_char chr, int max);    // FF3 0x001c2c68 (PhotoWrk1e8StepRate)
    int GetMaxNum(u_char chr, u_char lv);   // FF3 0x001c2cc0 (PhotoWrk1e8GetMaxNum)
    int GetMax(u_char chr, u_char lv);      // FF3 0x001c2ce8 (PhotoWrk1e8GetMax)
    float GetRange(u_char chr, u_char lv);  // FF3 0x001c2d10 (PhotoWrk1e8GetRange)
    int GetSpeed(u_char chr, u_char lv);    // FF3 0x001c2d38 (PhotoWrk1e8GetSpeed)
    int GetSpeedEquip(u_char lv);           // FF3 0x001c2d60 (PhotoWrk1e8GetSpeedEquip)
    int IsMax();                            // FF3 0x001c2d80 (PhotoWrk1e8IsMax)
    void SeMain();                          // FF3 0x001c2db8 (PhotoWrk1e8SeMain)
    void SeStop();                          // FF3 0x001c2f40 (PhotoWrk1e8SeStop; PlyrSeStopAll calls it too)

    /* 0x00 */ int    mode;             // 0: the enemy charge fills 'charge'; else 'charge2' (player 1) follows it
    /* 0x04 */ int    pct;              // (charge + charge2) * 100 / max
    /* 0x08 */ int    charge;           // PHOTO_CHARGE_STEP per step
    /* 0x0c */ int    num;              // charge / 600 (PhotoWrk1e8GetNum); PhotoWrk1e8IsMax: == PlyrGetChargeMax(0xff, 0xff)
    /* 0x10 */ int    charge2;          // PhotoWrk1e8SeMain: pitch / volume from charge2 * 100 / (PlyrGetChargeMax * 600)
    /* 0x14 */ int    charge2_old;      // last frame's charge2 (SE 0x1b7f000a when it reaches charge)
    /* 0x18 */ int    num2;             // charge2 / 600 (PhotoWrk1e8GetNumSub)
    /* 0x1c */ u_char _1c[4];
    /* 0x20 */ int    se_id;            // the looping SE 0x1b7f0009, -1: none
    /* 0x24 */ float  disp;             // smoothed toward 0.3 in a shutter chance (PhotoWrk1e8GetDispRate)
    /* 0x28 */ float  disp_hold;        // 1.0, counts down before disp may fall
    /* 0x2c */ CWRK_VAR_CHAR_0_60 fade; // 0..60: the gauge's alpha (PhotoWrk1e8FadeIn / Out)
    /* 0x2e */ u_char _2e[2];
    /* 0x30 */ float  flash;            // 1.0 when a step fills to 100 %, -1/12 a frame
    /* 0x34 */ float  ramp;             // mode != 0: rises 0.2 a frame to 1.0, else 0
};
static_assert(sizeof(PHOTO_WRK_1E8) == 0x38, "PHOTO_WRK_1E8 is 0x38 bytes");
static_assert(offsetof(PHOTO_WRK_1E8, num) == 0x0c, "PHOTO_WRK_1E8.num");
static_assert(offsetof(PHOTO_WRK_1E8, charge2) == 0x10, "PHOTO_WRK_1E8.charge2");
static_assert(offsetof(PHOTO_WRK_1E8, num2) == 0x18, "PHOTO_WRK_1E8.num2");
static_assert(offsetof(PHOTO_WRK_1E8, se_id) == 0x20, "PHOTO_WRK_1E8.se_id");
static_assert(offsetof(PHOTO_WRK_1E8, disp) == 0x24, "PHOTO_WRK_1E8.disp");
static_assert(offsetof(PHOTO_WRK_1E8, fade) == 0x2c, "PHOTO_WRK_1E8.fade");
static_assert(offsetof(PHOTO_WRK_1E8, flash) == 0x30, "PHOTO_WRK_1E8.flash");
static_assert(offsetof(PHOTO_WRK_1E8, ramp) == 0x34, "PHOTO_WRK_1E8.ramp");

// What PHOTO_WRK_1E8::Draw hands the HUD draws (data 0x3273c0, cleared each
// draw; HudChargeGaugeDraw / HudCharge2Draw in finder_view_hud.c read it).
struct PHOTO_CHARGE_DISP                /* 0x20 */
{
    /* 0x00 */ s_char max;              // steps (PlyrGetChargeMax); player 2: 2
    /* 0x01 */ s_char num;              // full steps; player 2: 0
    /* 0x02 */ s_char num_lit;          // = num; player 2: num != 0 ? 2 : 0
    /* 0x03 */ s_char num2;             // the second charge's steps
    /* 0x04 */ s_char num_round;        // charge / 600 rounded
    /* 0x05 */ s_char level;            // PlyrGetCamLvCharge: the player's camera level (PLYR_WRK +0x230)
    /* 0x06 */ u_char _06[2];
    /* 0x08 */ float  frac;             // the filling step, 0..1
    /* 0x0c */ float  frac2;            // the second charge's filling step
    /* 0x10 */ float  flash;            // 1 - PHOTO_WRK_1E8.flash while it runs
    /* 0x14 */ float  ramp;             // PHOTO_WRK_1E8.ramp; player 2: 0
    /* 0x18 */ int    charging;         // +0x180 flags & (0x10 | 0x20) == 0x10
    /* 0x1c */ int    chr;              // the player number
};
static_assert(sizeof(PHOTO_CHARGE_DISP) == 0x20, "PHOTO_CHARGE_DISP is 0x20 bytes");
static_assert(offsetof(PHOTO_CHARGE_DISP, level) == 0x05, "PHOTO_CHARGE_DISP.level");
static_assert(offsetof(PHOTO_CHARGE_DISP, frac) == 0x08, "PHOTO_CHARGE_DISP.frac");
static_assert(offsetof(PHOTO_CHARGE_DISP, flash) == 0x10, "PHOTO_CHARGE_DISP.flash");
static_assert(offsetof(PHOTO_CHARGE_DISP, charging) == 0x18, "PHOTO_CHARGE_DISP.charging");
static_assert(offsetof(PHOTO_CHARGE_DISP, chr) == 0x1c, "PHOTO_CHARGE_DISP.chr");

// chr / lv arguments: player numbers and camera levels, 0xff for the
// current one (as PlyrGetChargeMax and its siblings take them).

// ──────────────────────────────────────────────────────────────────────
// +0x288: the pulse-in panel
// ──────────────────────────────────────────────────────────────────────

#define PHOTO288_ENT_NUM    14          // ent[0] is never run; 1..12 icons, 13 the number
#define PHOTO288_ENT_NO     13          // the number's pulse

struct PHOTO_WRK_288                    /* 0x138 */
{
    /* 0x000 */ CPulseVariable<0> pulse;               // the corner marks
    /* 0x010 */ CPulseVariable<0> ent[PHOTO288_ENT_NUM];
    /* 0x0f0 */ int               num;                 // drawn by HudPanelNumDraw while > 0 (PhotoWrk288Req)
    /* 0x0f4 */ int               on[PHOTO288_ENT_NUM]; // icon i is shown: on[1..12] count the shot's bonuses, on[13] sums the raw score (PlyrShotScoreEne)
    /* 0x12c */ int               score;               // on[13] clamped to 99999 (PlyrShotScore; PlyrShotRecord files it)
    /* 0x130 */ u_char            kill_cnt;            // ghosts the shot finished (PlyrShotDamage); 2 / 3+: bonus 11 / 12
    /* 0x131 */ u_char            hit_cnt;             // charged ghosts the shot hit (PlyrShotJudge); 2 / 3+: bonus 9 / 10
    /* 0x132 */ u_char            sp_cnt;              // special (type 2) ghosts it caught; non-zero: charger SE 7 (PlyrShotJudge)
    /* 0x133 */ u_char            level;               // >= 2: corners HudPanelLevelDraw(level), and the pulse holds while EneAnyDmgReactBig
    /* 0x134 */ u_char            corner_a;            // level < 2: corners HudPanelMark1Draw
    /* 0x135 */ u_char            corner_b;            // else: corners HudPanelMark2Draw
    /* 0x136 */ u_char            _136[2];
};
static_assert(sizeof(PHOTO_WRK_288) == 0x138, "PHOTO_WRK_288 is 0x138 bytes");
static_assert(offsetof(PHOTO_WRK_288, ent) == 0x010, "PHOTO_WRK_288.ent");
static_assert(offsetof(PHOTO_WRK_288, num) == 0x0f0, "PHOTO_WRK_288.num");
static_assert(offsetof(PHOTO_WRK_288, on) == 0x0f4, "PHOTO_WRK_288.on");
static_assert(offsetof(PHOTO_WRK_288, score) == 0x12c, "PHOTO_WRK_288.score");
static_assert(offsetof(PHOTO_WRK_288, kill_cnt) == 0x130, "PHOTO_WRK_288.kill_cnt");
static_assert(offsetof(PHOTO_WRK_288, sp_cnt) == 0x132, "PHOTO_WRK_288.sp_cnt");
static_assert(offsetof(PHOTO_WRK_288, level) == 0x133, "PHOTO_WRK_288.level");
static_assert(offsetof(PHOTO_WRK_288, corner_b) == 0x135, "PHOTO_WRK_288.corner_b");

void PhotoWrk288Init(PHOTO_WRK_288 *w);                 // FF3 0x001c1418
void PhotoWrk288Main(PHOTO_WRK_288 *w);                 // FF3 0x001c1490 (camera up)
void PhotoWrk288Draw(float x, float y, PHOTO_WRK_288 *w, int alpha, int real);   // FF3 0x001c1500; alpha 0..0x80
void PhotoWrk288Req(PHOTO_WRK_288 *w, int num, int delay);  // FF3 0x001c1a08 (0x23c028)

#endif /* _INGAME_PHOTO_FNDR_EFFECT_H */
