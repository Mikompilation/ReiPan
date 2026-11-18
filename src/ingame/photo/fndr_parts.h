/* ==========================================================================
 *  ingame/photo/fndr_parts.h
 *
 *  The camera HUD parts (FF3-only code, FF3 0x1c2d80..0x1c67c0, plus the
 *  linkonce template instances 0x2b9d18..0x2ba3c8): the sub-works the photo
 *  mode work (finder_photo.h) embeds, and their init / work / draw methods.
 *  FF3 rewrote FF2's photo widgets (ingame/photo/film_no.c, filament.c,
 *  damage_disp.c, photo_charger.c, hp_bar.c ...) as plain structs driven by
 *  free functions; no code matches FF2's, but the widget templates of FF2's
 *  common/variable.h survive with their parameters:
 *    - film readout: FF2 CFilmNo's CBlinkSwitchVariable<char,70,90,15,40>;
 *    - filament: FF2 CFilament's four blink switches <90,118> <90,112>
 *      <90,116> <75,112> and its blink bias 115;
 *    - damage number: FF2 CDamageDisp's CWaitVariable<short> + char pulse;
 *    - shutter charger: FF2 CPhotoCharger's wait + pulse.
 *  New in FF3: the lens / spirit-stone HUD (LENS_WRK), the enemy markers that
 *  follow the effect slots, the HP gauge with a trailing bar. LENS_WRK has a
 *  constructor (0x2b95b8) and is written as a class; the other sub-works
 *  have no constructor code of their own.
 *  The file name is descriptive: FF3 probably had several object files here
 *  (no static initializer splits the run); "fndr" sorts between fly_ctrl.c and
 *  fod.c, as FF3's alphabetical link order requires.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_FNDR_PARTS_H
#define _INGAME_PHOTO_FNDR_PARTS_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "common/cmn_fade_layer.h"      // CWrkVariable

// ──────────────────────────────────────────────────────────────────────
// Widget templates (FF2 common/variable.h, C++ classes there and here). The
// bounds are template parameters; FF3 emitted each method it used out of
// line per instance (linkonce), so each is an explicit specialization below
// with its own address. CWrkVariable is in common/cmn_fade_layer.h.
// ──────────────────────────────────────────────────────────────────────

// A one-shot countdown: Work() is 1 on the frame it reaches 0.
template <typename T>
struct CWaitVariable
{
    int Work();                         // FF2 CWaitVariable::Work

    /* 0x00 */ T val;
};
static_assert(sizeof(CWaitVariable<short>) == 0x2, "CWaitVariable<short> is 2 bytes");

// A value that walks to a target at a computed speed and stops on it.
template <typename T>
struct CFadeVariable
{
    void Work();                        // FF2 CFadeVariable::Work: one step, stops dead on the target
    void Fade(T target, T time);        // FF2 CFadeVariable::Fade: aims at target over time frames (0: at once)

    /* 0x00 */ T val;
    /* 0x02 */ T speed;
    /* 0x04 */ T target;
};
static_assert(sizeof(CFadeVariable<short>) == 0x6, "CFadeVariable<short> is 6 bytes");
static_assert(offsetof(CFadeVariable<short>, target) == 0x4, "CFadeVariable.target");

// Switched on, the value pulses between MIN and MAX ((MAX - MIN) / TIME a
// tick); switched off it decays to INIT and stays there.
#define BLINK_SW_ON     0x1             // CBlinkSwitchVariable.flags
#define BLINK_SW_UP     0x2

template <typename T, int MIN, int MAX, int TIME, int INIT>
struct CBlinkSwitchVariable
{
    void Work();                        // FF2 CBlinkSwitchVariable::Work

    // Init (value INIT, off, pointing up) and BlinkOn, as GCC folds them
    // (inline in FF3, no address).
    void InitOn()
    {
        val   = (T)INIT;
        flags = (flags & ~BLINK_SW_ON) | BLINK_SW_UP;
        flags |= BLINK_SW_ON;
    }

    /* 0x00 */ T      val;
    /* 0x01 */ u_char flags;            // BLINK_SW_*
};
static_assert(sizeof(CBlinkSwitchVariable<s_char, 70, 90, 15, 40>) == 0x2, "CBlinkSwitchVariable<char> is 2 bytes");

// FF3's pulse (0x10 bytes; FF2 CDamageDisp's wait + CBlinkVariable<char>
// grown into one template, floor MIN): after 'delay' frames the value rises
// to 127 at 'speed', holds there until 'wait' runs out (never while 'hold'
// is set), then falls back to MIN and stops (speed 0).
template <int MIN>
struct CPulseVariable
{
    void Work();                                    // one frame
    void Req(s_char time, short wait, int delay);   // arms it: up in 'time' frames after 'delay'

    /* 0x00 */ CWaitVariable<short> wait;
    /* 0x02 */ s_char val;
    /* 0x03 */ s_char speed;            // 0: idle (the IsOn test)
    /* 0x04 */ int    on;               // at the top, waiting
    /* 0x08 */ int    delay;            // frames before the rise
    /* 0x0c */ int    hold;             // non-zero: stays at the top
};
static_assert(sizeof(CPulseVariable<0>) == 0x10, "CPulseVariable is 0x10 bytes");
static_assert(offsetof(CPulseVariable<0>, val) == 0x2, "CPulseVariable.val");
static_assert(offsetof(CPulseVariable<0>, on) == 0x4, "CPulseVariable.on");
static_assert(offsetof(CPulseVariable<0>, hold) == 0xc, "CPulseVariable.hold");

typedef CWrkVariable<short, 0, 128> CWRK_VAR_SHORT_0_128;
static_assert(sizeof(CWRK_VAR_SHORT_0_128) == 0x4, "CWrkVariable<short,0,128> is 4 bytes");

// CWrkVariable<char, ...> instances fndr_effect.c runs (FF2 CWrkVariable::LoopWork /
// Work: 'val' plus 'delta', wrapped or clamped to the bounds).
typedef CWrkVariable<s_char, 0, 100> CWRK_VAR_CHAR_0_100;
typedef CWrkVariable<s_char, 0, 60>  CWRK_VAR_CHAR_0_60;
static_assert(sizeof(CWRK_VAR_CHAR_0_100) == 0x2, "CWrkVariable<char,0,100> is 2 bytes");
static_assert(offsetof(CWRK_VAR_CHAR_0_100, delta) == 0x1, "CWrkVariable<char>.delta");

template <> void CPulseVariable<0>::Work();                                     // FF3 0x002b9d18 (CPulseVariable_0_Work)
template <> void CPulseVariable<0>::Req(s_char time, short wait, int delay);    // FF3 0x002b9dd0 (CPulseVariable_0_Req)
template <> int  CWaitVariable<short>::Work();                                  // FF3 0x002b9e28 (CWaitVariable_short_Work)
template <> int  CWrkVariable<s_char, 0, 100>::LoopWork();                      // FF3 0x002b9e60 (CWrkVariable_char_0_100_LoopWork)
template <> void CFadeVariable<short>::Work();                                  // FF3 0x002b9ea0 (CFadeVariable_short_Work)
template <> void CFadeVariable<short>::Fade(short target, short time);          // FF3 0x002b9f18 (CFadeVariable_short_Fade)
template <> void CWrkVariable<s_char, 0, 60>::Work();                           // FF3 0x002b9f68 (CWrkVariable_char_0_60_Work)
template <> void CBlinkSwitchVariable<s_char, 70, 90, 15, 40>::Work();          // FF3 0x002ba048 (CBlinkSwitchVariable_char_70_90_Work)
template <> void CWrkVariable<short, 0, 128>::Work();                           // FF3 0x002ba0b8 (CWrkVariable_short_0_128_Work)
template <> void CBlinkSwitchVariable<s_char, 90, 118, 6, 90>::Work();          // FF3 0x002ba0f0 (CBlinkSwitchVariable_char_90_118_Work)
template <> void CBlinkSwitchVariable<s_char, 90, 112, 11, 90>::Work();         // FF3 0x002ba160 (CBlinkSwitchVariable_char_90_112_Work)
template <> void CBlinkSwitchVariable<s_char, 90, 116, 13, 90>::Work();         // FF3 0x002ba1d0 (CBlinkSwitchVariable_char_90_116_Work)
template <> void CBlinkSwitchVariable<s_char, 75, 112, 17, 75>::Work();         // FF3 0x002ba240 (CBlinkSwitchVariable_char_75_112_Work)
template <> void CPulseVariable<50>::Req(s_char time, short wait, int delay);   // FF3 0x002ba2b0 (CPulseVariable_50_Req)
template <> void CPulseVariable<50>::Work();                                    // FF3 0x002ba308 (CPulseVariable_50_Work)

float FloatWrap(float a, float b);                                              // FF3 0x002b9fa0

// ──────────────────────────────────────────────────────────────────────
// The sub-works
// ──────────────────────────────────────────────────────────────────────

// The damage number over a hit ghost (FF2 CDamageDisp; one per effect slot,
// ENE_WRK.dmg_disp). plyr_mdl.c requests it, PhotoModeDrawNearEnemy draws
// the nearest one that is on.
struct DAMAGE_DISP                      /* 0x14 */
{
    /* 0x00 */ CPulseVariable<0> pulse;
    /* 0x10 */ short  damage;
    /* 0x12 */ u_char _12[2];
};
static_assert(sizeof(DAMAGE_DISP) == 0x14, "DAMAGE_DISP is 0x14 bytes");
static_assert(offsetof(DAMAGE_DISP, damage) == 0x10, "DAMAGE_DISP.damage");

// A screen marker per slot of the 0x2fa3a0 work: EneMarkWork moves 'cur'
// from 'from' to 'to' in 10 steps; EneMarkDraw fades 'alpha'.
struct PHOTO_ENE_MARK                   /* 0x18 */
{
    /* 0x00 */ int    step;             // 0..10
    /* 0x04 */ short  to_x;
    /* 0x06 */ short  to_y;
    /* 0x08 */ short  from_x;
    /* 0x0a */ short  from_y;
    /* 0x0c */ short  cur_x;
    /* 0x0e */ short  cur_y;
    /* 0x10 */ u_char alpha;            // 0..the view alpha, by 6 (PAL) / 8 a frame
    /* 0x11 */ u_char tracking;         // 1: following the slot's screen position
    /* 0x12 */ u_char _12[2];
    /* 0x14 */ int    id;               // the slot's target pointer (+0x50c) when last seen (PC: its low 32 bits)
};
static_assert(sizeof(PHOTO_ENE_MARK) == 0x18, "PHOTO_ENE_MARK is 0x18 bytes");
static_assert(offsetof(PHOTO_ENE_MARK, to_x) == 0x04, "PHOTO_ENE_MARK.to_x");
static_assert(offsetof(PHOTO_ENE_MARK, cur_x) == 0x0c, "PHOTO_ENE_MARK.cur_x");
static_assert(offsetof(PHOTO_ENE_MARK, alpha) == 0x10, "PHOTO_ENE_MARK.alpha");
static_assert(offsetof(PHOTO_ENE_MARK, tracking) == 0x11, "PHOTO_ENE_MARK.tracking");
static_assert(offsetof(PHOTO_ENE_MARK, id) == 0x14, "PHOTO_ENE_MARK.id");

// The HP gauge (photo mode work +0x1d0): 'disp' follows 'now' (rises 0.02,
// falls 0.04 after wait_disp), 'tail' trails it (falls 0.02 after
// wait_tail); 'phase' pulses the gauge at low HP. 0x1502b0 resets +0..+0x10.
struct HUD_HP_WRK                       /* 0x18 */
{
    /* 0x00 */ int   wait_tail;
    /* 0x04 */ int   wait_disp;
    /* 0x08 */ float disp;              // 0x1506f0 sizes R HP with it
    /* 0x0c */ float now;
    /* 0x10 */ float tail;
    /* 0x14 */ float phase;             // HudHpDraw pulses with cos(phase)
};
static_assert(sizeof(HUD_HP_WRK) == 0x18, "HUD_HP_WRK is 0x18 bytes");
static_assert(offsetof(HUD_HP_WRK, disp) == 0x08, "HUD_HP_WRK.disp");
static_assert(offsetof(HUD_HP_WRK, tail) == 0x10, "HUD_HP_WRK.tail");
static_assert(offsetof(HUD_HP_WRK, phase) == 0x14, "HUD_HP_WRK.phase");

// The shutter charger (photo mode work +0x220; FF2 CPhotoCharger): 'charge'
// counts down; when it ends the flash pulse starts and 'ready' is set.
struct PHOTO_CHARGER                    /* 0x20 */
{
    /* 0x00 */ u_char               _00[4];
    /* 0x04 */ CWaitVariable<short> charge;     // 0 (constructor, PhotoModeWrkInit)
    /* 0x06 */ u_char               _06[2];
    /* 0x08 */ CPulseVariable<50>   flash;      // wait 0 (constructor)
    /* 0x18 */ short                charge_time;
    /* 0x1a */ short                se0_wait;   // SE 0 when it reaches 1
    /* 0x1c */ short                se7_wait;   // SE 7 when it reaches 1
    /* 0x1e */ short                ready;      // PhotoChargerIsReady
};
static_assert(sizeof(PHOTO_CHARGER) == 0x20, "PHOTO_CHARGER is 0x20 bytes");
static_assert(offsetof(PHOTO_CHARGER, charge) == 0x04, "PHOTO_CHARGER.charge");
static_assert(offsetof(PHOTO_CHARGER, flash) == 0x08, "PHOTO_CHARGER.flash");
static_assert(offsetof(PHOTO_CHARGER, charge_time) == 0x18, "PHOTO_CHARGER.charge_time");
static_assert(offsetof(PHOTO_CHARGER, ready) == 0x1e, "PHOTO_CHARGER.ready");

// The loaded film (photo mode work +0x240, GetFilmWrk 0x3e7b58; FF2 CFilmNo
// plus the film type). FilmGetSaveData saves the first byte.
struct FILM_WRK                         /* 0x4 */
{
    /* 0x00 */ u_char type;             // 0..4, index of the film item table (PhotoModeGetFilmPower, the HUD icon)
    /* 0x01 */ CBlinkSwitchVariable<s_char, 70, 90, 15, 40> blink;
    /* 0x03 */ u_char _03;
};
static_assert(sizeof(FILM_WRK) == 0x4, "FILM_WRK is 4 bytes");
static_assert(offsetof(FILM_WRK, blink) == 0x1, "FILM_WRK.blink");

#define FILM_TYPE_NUM   5

// The filament (photo mode work +0x244; FF2 CFilament). Three deflection
// rates (battle > auto > hint) pick 'rate' each frame; the HUD fade 1 rises
// while 'rate' > 0.
struct FILAMENT_WRK                     /* 0x30 */
{
    /* 0x00 */ CWRK_VAR_SHORT_0_128 master_alp;  // 0x80 (FilamentInit), 0 (constructor)
    /* 0x04 */ float  rate;
    /* 0x08 */ u_char _08[4];
    /* 0x0c */ int    lock;             // non-zero: no draw
    /* 0x10 */ float  hint;
    /* 0x14 */ float  auto_rate;
    /* 0x18 */ float  battle;
    /* 0x1c */ u_char rt_ev_wrk[0x14];  // FF2's RT block: cleared, never read
};
static_assert(sizeof(FILAMENT_WRK) == 0x30, "FILAMENT_WRK is 0x30 bytes");
static_assert(offsetof(FILAMENT_WRK, rate) == 0x04, "FILAMENT_WRK.rate");
static_assert(offsetof(FILAMENT_WRK, lock) == 0x0c, "FILAMENT_WRK.lock");
static_assert(offsetof(FILAMENT_WRK, hint) == 0x10, "FILAMENT_WRK.hint");
static_assert(offsetof(FILAMENT_WRK, battle) == 0x18, "FILAMENT_WRK.battle");
static_assert(offsetof(FILAMENT_WRK, rt_ev_wrk) == 0x1c, "FILAMENT_WRK.rt_ev_wrk");

// Photo mode work +0x274: two turning angles and an alpha that pulses
// 0x40..0x80 while ShutterChanceChk() == 2 (drawn while camera function 1 is on).
struct PHOTO_WRK_274                    /* 0x14 */
{
    /* 0x00 */ int   state;             // 0 off, 1 rising, 2 falling, 3 fading out
    /* 0x04 */ int   alpha;             // 0..0x80
    /* 0x08 */ float rot0;              // degrees, +10 a frame
    /* 0x0c */ float rot1;              // degrees, -12 a frame (20 at init)
    /* 0x10 */ int   se_cnt;            // SE 0xd every 15 frames
};
static_assert(sizeof(PHOTO_WRK_274) == 0x14, "PHOTO_WRK_274 is 0x14 bytes");
static_assert(offsetof(PHOTO_WRK_274, rot1) == 0x0c, "PHOTO_WRK_274.rot1");
static_assert(offsetof(PHOTO_WRK_274, se_cnt) == 0x10, "PHOTO_WRK_274.se_cnt");

// Photo mode work +0x4bc: a gauge like HUD_HP_WRK, falling only: 'disp' and
// 'tail' fall to 'now' by 0.05 after their delays (drawn by FUN_001c3160
// while camera function 4 is on).
struct PHOTO_WRK_4BC                    /* 0x14 */
{
    /* 0x00 */ int   wait_tail;
    /* 0x04 */ int   wait_disp;
    /* 0x08 */ float now;
    /* 0x0c */ float disp;
    /* 0x10 */ float tail;
};
static_assert(sizeof(PHOTO_WRK_4BC) == 0x14, "PHOTO_WRK_4BC is 0x14 bytes");
static_assert(offsetof(PHOTO_WRK_4BC, now) == 0x08, "PHOTO_WRK_4BC.now");
static_assert(offsetof(PHOTO_WRK_4BC, tail) == 0x10, "PHOTO_WRK_4BC.tail");

// ── The lenses (photo mode work +0x3cc, GetLensWrk 0x3e7ce4) ──────────
// Per player (0..2; player 1 has no lenses): a grade, the lenses owned,
// three equip slots, and a spirit-stone charge (100 per stone). Lenses cost
// stones (LENS_DAT.cost). The HUD shows the selected slot's lens, a switch
// animation, and one icon per charged stone.

#define LENS_NUM        9               // lens numbers 0..8
#define LENS_SLOT_NUM   3
#define LENS_CHR_NUM    3
#define LENS_ICON_NUM   7
#define LENS_NONE       (-1)            // an empty equip slot / no lens
#define LENS_FUNC_NUM   9               // camera functions (LENS_WRK.have_func / func)
#define LENS_SAVE_SIZE  0x70            // LENS_WRK +4.. (LensGetSaveData, LensSaveBackup)

struct LENS_CHR                         /* 0x16 */
{
    /* 0x00 */ u_char grade;            // row of the stone capacity table (LensGetStoneMax)
    /* 0x01 */ u_char have[LENS_NUM];   // non-zero: owned (LensGetHaveNum)
    /* 0x0a */ s_char equip[LENS_SLOT_NUM];   // lens number, LENS_NONE (LensGetEquip)
    /* 0x0d */ u_char level[LENS_NUM];  // 0..LENS_LEVEL_MAX, index of LENS_DAT.rate / val (LensGetLevel)
};
static_assert(sizeof(LENS_CHR) == 0x16, "LENS_CHR is 0x16 bytes");
static_assert(offsetof(LENS_CHR, have) == 0x01, "LENS_CHR.have");
static_assert(offsetof(LENS_CHR, equip) == 0x0a, "LENS_CHR.equip");
static_assert(offsetof(LENS_CHR, level) == 0x0d, "LENS_CHR.level");

#define LENS_GRADE_MAX  3               // LENS_CHR.grade (LensGradeUp)
#define LENS_LEVEL_MAX  3               // LENS_CHR.level (LensLevelUp)

struct LENS_STONE                       /* 0x8 */
{
    /* 0x00 */ int    charge;           // stones * 100 + the next stone's charge
    /* 0x04 */ u_char sel;              // selected equip slot 0..2
    /* 0x05 */ u_char _05[3];
};
static_assert(sizeof(LENS_STONE) == 0x8, "LENS_STONE is 8 bytes");

// LENS_ICON.state
enum LENS_ICON_STATE
{
    LENS_ICON_OFF    = 0,
    LENS_ICON_APPEAR = 1,               // t 0 -> 1 by 0.2
    LENS_ICON_FLASH  = 2,               // spins, t 0 -> 1 by 0.1
    LENS_ICON_ON     = 3,               // spins
    LENS_ICON_VANISH = 4,               // spins, t 1 -> 0 by 1/15
};

struct LENS_ICON                        /* 0xc */
{
    /* 0x00 */ u_int state;             // LENS_ICON_STATE
    /* 0x04 */ float t;
    /* 0x08 */ float rot;               // degrees, +6 a frame
};
static_assert(sizeof(LENS_ICON) == 0xc, "LENS_ICON is 0xc bytes");

struct LENS_SWITCH                      /* 0xc */
{
    /* 0x00 */ int    state;            // 0 idle, 1 / 2 switching (direction)
    /* 0x04 */ float  t;                // 0 -> 1 by 0.125
    /* 0x08 */ s_char equip;            // the lens shown
    /* 0x09 */ u_char _09[3];
};
static_assert(sizeof(LENS_SWITCH) == 0xc, "LENS_SWITCH is 0xc bytes");

struct LENS_SE                          /* 0xc */
{
    /* 0x00 */ int interval;            // frames to the next SE 0xb
    /* 0x04 */ int count;               // SE 0xb still to play
    /* 0x08 */ int end;                 // > 0: SE 0xe, all cleared
};
static_assert(sizeof(LENS_SE) == 0xc, "LENS_SE is 0xc bytes");

struct LENS_DAT;                    // below
struct ENE_WRK;                     // ingame/enemy/ene_main.h

// The lens work (PHOTO_MODE_WRK +0x3cc; GetLensWrk). C++ in FF3: its
// constructor 0x2b95b8 runs from PHOTO_MODE_WRK's.
struct LENS_WRK                         /* 0xf0 */
{
    LENS_WRK();                                        // FF3 0x002b95b8 (LensWrkCtor; PHOTO_MODE_WRK's constructor runs it)
    void Init();                                       // FF3 0x001c4ef0 (LensInit)
    void LabelReset();                                 // FF3 0x001c5020 (LensLabelReset)
    void Work();                                       // FF3 0x001c5050 (LensWork)
    void Draw(float x, float y, int alpha);            // FF3 0x001c52a0 (LensDraw): at the finder point
    void AddCharge(u_int amount, u_char chr);          // FF3 0x001c54c0 (LensAddCharge)
    void SeEnd();                                      // FF3 0x001c55b8 (LensSeEnd)
    int GetStoneNum(u_char chr);                       // FF3 0x001c55c8 (LensGetStoneNum)
    int GetStoneFrac(u_char chr);                      // FF3 0x001c5630 (LensGetStoneFrac)
    float GetStoneFill(u_char i, u_char chr);          // FF3 0x001c5698 (LensGetStoneFill)
    int UseStone(int num, u_char chr);                 // FF3 0x001c5770 (LensUseStone)
    float GetStoneRate(u_char stones);                 // FF3 0x001c57f0 (LensGetStoneRate)
    float GetLevelRate(u_char level);                  // FF3 0x001c5810 (LensGetLevelRate): level 0xff: the current player's grade
    u_char GetGrade(u_char chr);                       // FF3 0x001c5868 (LensGetGrade)
    void GradeUp(u_char chr);                          // FF3 0x001c58c0 (LensGradeUp)
    u_char GetStoneMax(u_char grade, u_char chr);      // FF3 0x001c5930 (LensGetStoneMax): grade 0xff: the player's
    void SelectCtrl(u_char chr);                       // FF3 0x001c59c8 (LensSelectCtrl)
    u_char GetSel(u_char chr);                         // FF3 0x001c5b68 (LensGetSel)
    void SetSel(s_char slot, u_char chr);              // FF3 0x001c5bb8 (LensSetSel)
    s_char GetSelEquip(u_char chr);                    // FF3 0x001c5c18 (LensGetSelEquip)
    u_char GetSelLevel(u_char chr);                    // FF3 0x001c5c78 (LensGetSelLevel)
    float GetSelRate();                                // FF3 0x001c5cf8 (LensGetSelRate)
    float GetSelVal();                                 // FF3 0x001c5d70 (LensGetSelVal)
    int IsSelUsable();                                 // FF3 0x001c5de8 (LensIsSelUsable)
    int UseSel();                                      // FF3 0x001c5e60 (LensUseSel)
    float GetRate(u_char no);                          // FF3 0x001c5ee8 (LensGetRate)
    u_char GetLevel(u_char no, u_char chr);            // FF3 0x001c5f58 (LensGetLevel)
    void LevelUp(u_char no, u_char chr);               // FF3 0x001c5fc0 (LensLevelUp)
    void AddFunc(u_char func);                         // FF3 0x001c6048 (LensAddFunc)
    u_char HaveFunc(u_char func);                      // FF3 0x001c6070 (LensHaveFunc)
    int GetHaveFuncNum();                              // FF3 0x001c6080 (LensGetHaveFuncNum)
    int SetFunc(u_char func);                          // FF3 0x001c60e0 (LensSetFunc)
    u_char GetFunc(u_char func);                       // FF3 0x001c6138 (LensGetFunc)
    void ClearFunc(u_char func);                       // FF3 0x001c6148 (LensClearFunc): not function 5
    void AddHave(u_char no, u_char chr);               // FF3 0x001c6168 (LensAddHave)
    u_char IsHave(u_char no, u_char chr);              // FF3 0x001c61c8 (LensIsHave)
    int IsEquip(u_char no, u_char chr);                // FF3 0x001c6230 (LensIsEquip)
    int SetEquip(u_char slot, u_char no, u_char chr);  // FF3 0x001c62e8 (LensSetEquip): 0: not owned / player 1
    s_char GetEquip(u_char slot, u_char chr);          // FF3 0x001c6388 (LensGetEquip): LENS_NONE: empty / player 1
    void Unequip(u_char slot, u_char chr);             // FF3 0x001c63f0 (LensUnequip)
    LENS_DAT *GetDat(u_char no);                        // FF3 0x001c6450 (LensGetDat)
    int GetHaveNum(u_char chr);                        // FF3 0x001c6480 (LensGetHaveNum)
    void HitEffectReq(ENE_WRK *slot, u_char no);       // FF3 0x001c6500 (LensHitEffectReq)
    void     * GetSaveData();                          // FF3 0x001c6638 (LensGetSaveData): 0x70 bytes
    int IsFinderReady();                               // FF3 0x001c6648 (LensIsFinderReady)
    int GetSlotNum();                                  // FF3 0x001c6668 (LensGetSlotNum): 0 (finder not ready), 1, or 3 with LENS_FUNC_SLOTS
    void SaveBackup();                                 // FF3 0x001c66b8 (LensSaveBackup)
    void RestoreBackup();                              // FF3 0x001c6708 (LensRestoreBackup)
    void ClearCharge();                                // FF3 0x001c6790 (LensClearCharge)

    /* 0x00 */ u_int                backup;              // PC: non-zero while LensSaveBackup holds a copy (FF3: the copy's heap address)
    /* 0x04 */ LENS_CHR             chr[LENS_CHR_NUM];   // } the save block (LensGetSaveData,
    /* 0x46 */ u_char               _46[2];              // }  0x70 bytes from +4)
    /* 0x48 */ LENS_STONE           stone[LENS_CHR_NUM]; // }
    /* 0x60 */ u_char               have_func[LENS_FUNC_NUM];  // } camera functions owned (LensHaveFunc)
    /* 0x69 */ u_char               func[LENS_FUNC_NUM]; // } camera functions on (LensGetFunc)
    /* 0x72 */ u_char               _72[2];              // }
    /* 0x74 */ LENS_ICON            icon[LENS_ICON_NUM];
    /* 0xc8 */ LENS_SWITCH          sw;
    /* 0xd4 */ CFadeVariable<short> fade_lens;   // 0..0x80: the selected lens is usable
    /* 0xda */ CFadeVariable<short> fade_label;
    /* 0xe0 */ CWaitVariable<short> label_wait;  // non-zero: the label is hidden
    /* 0xe2 */ u_char               _e2[2];
    /* 0xe4 */ LENS_SE              se;
};
static_assert(sizeof(LENS_WRK) == 0xf0, "LENS_WRK is 0xf0 bytes");
static_assert(offsetof(LENS_WRK, chr) == 0x04, "LENS_WRK.chr");
static_assert(offsetof(LENS_WRK, stone) == 0x48, "LENS_WRK.stone");
static_assert(offsetof(LENS_WRK, have_func) == 0x60, "LENS_WRK.have_func");
static_assert(offsetof(LENS_WRK, func) == 0x69, "LENS_WRK.func");
static_assert(offsetof(LENS_WRK, icon) == 0x74, "LENS_WRK.icon");
static_assert(offsetof(LENS_WRK, sw) == 0xc8, "LENS_WRK.sw");
static_assert(offsetof(LENS_WRK, fade_lens) == 0xd4, "LENS_WRK.fade_lens");
static_assert(offsetof(LENS_WRK, fade_label) == 0xda, "LENS_WRK.fade_label");
static_assert(offsetof(LENS_WRK, label_wait) == 0xe0, "LENS_WRK.label_wait");
static_assert(offsetof(LENS_WRK, se) == 0xe4, "LENS_WRK.se");

// Camera functions tested here and in finder_photo.c (LENS_WRK.func index).
#define LENS_FUNC_SLOTS     0           // three equip slots and the lens HUD (else one)
#define LENS_FUNC_274       1           // draws the +0x274 sub-work
#define LENS_FUNC_4BC       4           // draws the +0x4bc sub-work
#define LENS_FUNC_ZOOM      6           // the zoom buttons work
#define LENS_FUNC_FREE_FILM 7           // shots take no film, except with title_start_flg.mode 2 (PlyrFinderShotCanTake)
#define LENS_FUNC_REFILL    8           // keeps players 0 and 2 at full stones, player 1's slow-mode gauge full (PlyrGaugeMain)

// One lens (table data 0x359828, 9 entries).
struct LENS_DAT                         /* 0x24 */
{
    /* 0x00 */ u_char cost;             // spirit stones it needs (LensIsSelUsable)
    /* 0x01 */ u_char _01[3];
    /* 0x04 */ float  rate[4];
    /* 0x14 */ float  val[4];
};
static_assert(sizeof(LENS_DAT) == 0x24, "LENS_DAT is 0x24 bytes");

// The camera shake (bss 0x3e9c30..; FUN_001c4088 starts it, FUN_001c40d8
// runs it on the gra3d camera).
extern int   photo_shake_state;         // bss 0x3e9c30
extern int   photo_shake_cnt;           // bss 0x3e9c34
extern int   photo_shake_state2;        // bss 0x3e9c38
extern int   photo_shake_cnt2;          // bss 0x3e9c3c
extern float photo_shake_fov;           // bss 0x3e9c40

// A gauge in each effect slot (ENE_WRK +0x5a4): 'val' 0..'max' drains
// by 'dec' a frame once 'wait' runs out, while below full. Set by enemy code
// (not reconstructed yet); EneGaugeInit leaves 'dec' 0, so it holds.
struct ENE_GAUGE                        /* 0x18 */
{
    /* 0x00 */ float val;
    /* 0x04 */ float max;               // 100
    /* 0x08 */ float _08;               // 1.0 (EneGaugeInit), not read here
    /* 0x0c */ float dec;
    /* 0x10 */ int   wait_time;         // 175 (PAL) / 210 frames: 3.5 s, not read here
    /* 0x14 */ int   wait;
};
static_assert(sizeof(ENE_GAUGE) == 0x18, "ENE_GAUGE is 0x18 bytes");
static_assert(offsetof(ENE_GAUGE, dec) == 0x0c, "ENE_GAUGE.dec");
static_assert(offsetof(ENE_GAUGE, wait_time) == 0x10, "ENE_GAUGE.wait_time");
static_assert(offsetof(ENE_GAUGE, wait) == 0x14, "ENE_GAUGE.wait");

// ──────────────────────────────────────────────────────────────────────
// Methods. chr: a player number, 0xff for the current one (PlyrGetCurNo).
// x, y: the draw offset; alpha: the view alpha 0..0x80 (0..100 in the
// viewfinder where noted).
// ──────────────────────────────────────────────────────────────────────

struct PHOTO_WRK_1E8;                   // fndr_effect.h

void DamageDispInit(DAMAGE_DISP *d);
void DamageDispWork(DAMAGE_DISP *d);
void DamageDispReq(DAMAGE_DISP *d, short damage, int delay);
void DamageDispDraw(float x, float y, DAMAGE_DISP *d, int alpha);
int  DamageDispIsOn(DAMAGE_DISP *d);

void PhotoGage4bcSnap(PHOTO_WRK_4BC *w);
void PhotoGage4bcWork(PHOTO_WRK_4BC *w);
void PhotoGage4bcDraw(float x, float y, PHOTO_WRK_4BC *w, int alpha);    // camera function 4, during lightning
void PhotoGage4bcSet(PHOTO_WRK_4BC *w, float rate);
void PhotoGage4bcSetNow(PHOTO_WRK_4BC *w, float rate);

void   FilmInit(FILM_WRK *film);
void   FilmWork(FILM_WRK *film);
void   FilmBlinkOn(FILM_WRK *film);
void   FilmBlinkOff(FILM_WRK *film);
void   FilmSetByItem(FILM_WRK *film, int item);
void   FilmSelectNext(FILM_WRK *film);
void   FilmSelectPrev(FILM_WRK *film);
int    FilmGetType(FILM_WRK *film);
int    FilmGetNum(FILM_WRK *film);                  // shots left of the loaded film
void  *FilmGetSaveData(FILM_WRK *film);             // 1 byte
int    FilmGetHaveTypeNum(void);                    // film types with shots left
void   FilmDrawTypeBox(float x, float y, FILM_WRK *film, int unused, u_char alpha);
u_char FilmGetChargeTime(void);                     // frames, by the loaded film's type

void PhotoWrk274Init(PHOTO_WRK_274 *w);
void PhotoWrk274Work(PHOTO_WRK_274 *w);
void PhotoWrk274Draw(float x, float y, PHOTO_WRK_274 *w, int alpha);

void FilamentInit(FILAMENT_WRK *w);
void FilamentSetBattle(FILAMENT_WRK *w, float rate);
void FilamentSetHint(FILAMENT_WRK *w, float rate);
void FilamentSetAuto(FILAMENT_WRK *w, float rate);
void FilamentWork(FILAMENT_WRK *w);
void FilamentDraw(float x, float y, FILAMENT_WRK *w, int alpha);     // alpha 0..0x80 (0..100 in the viewfinder)
void FilamentFadeIn(FILAMENT_WRK *w);
void FilamentFadeOut(FILAMENT_WRK *w);

void PhotoChargerWork(PHOTO_CHARGER *w, int real);
void PhotoChargerInit(PHOTO_CHARGER *w, short time);
int  PhotoChargerIsReady(PHOTO_CHARGER *w);
void PhotoChargerSetup(PHOTO_CHARGER *w);           // finder mode: the film's charge time
void PhotoChargerSetSe0Wait(PHOTO_CHARGER *w, short wait);

void PhotoShakeReset(void);
void PhotoShakeReq(void);
void PhotoShakeWork(void);

void  EneGaugeInit(ENE_GAUGE *g);
void  EneGaugeWork(ENE_GAUGE *g);
float EneGaugeGetRate(ENE_GAUGE *g);                // percent

void EneMarkSlotInit(PHOTO_ENE_MARK *mark, ENE_WRK *slot);     // the slot's marker parked on the finder point
void EneMarkInit(PHOTO_ENE_MARK *mark);
void EneMarkWork(PHOTO_ENE_MARK *mark);
void EneMarkDraw(float x, float y, PHOTO_ENE_MARK *mark, int alpha, int real);

void HudHpWork(HUD_HP_WRK *hp);
void HudHpDraw(float x, float y, HUD_HP_WRK *hp, int alpha);         // alpha 0..0x80 (0..100 in the viewfinder)
void HudHpSet(HUD_HP_WRK *hp, float rate);
void HudHpSetNow(HUD_HP_WRK *hp, float rate);


#endif /* _INGAME_PHOTO_FNDR_PARTS_H */
