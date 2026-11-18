/* ==========================================================================
 *  common/cmn_util.h
 *
 *  FF3-only in-game helpers, FF3 0x00109dd8..0x0010afa8 (between CBgSe and
 *  CDebugEdit's CEditRoot): the level-up point costs, the step sequencer
 *  (STEP_SEQ), the camera level accessors (CamLv*), the game-clear reward
 *  and rank tests, a few number printers and the two-axis menu cursor
 *  (CSR_GRID). No FF2 counterpart. The file is C++ on the EE (the
 *  sequencer and the cursor are classes with C1/C2 constructor twins) but
 *  has no RTTI class; FF3's class files link alphabetically by class name, so its real
 *  name sorts between CBgSe and CDebugEdit.
 * ======================================================================== */
#ifndef _COMMON_CMN_UTIL_H
#define _COMMON_CMN_UTIL_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

struct PLAY_TIME;                       // common/cmn_time.h

// ──────────────────────────────────────────────────────────────────────
// Level-up point costs (data 0x2ef890)
// ──────────────────────────────────────────────────────────────────────

#define UP_COST_NUM         14
#define UP_COST_CAM_LV      2000        // + CAM_LV_*: the camera levels; 0..8 are the other items

// One cost entry: cost[row][lv], row by player (UpCostGetRow).
struct UP_COST                          /* 0x28 */
{
    /* 0x00 */ int id;
    /* 0x04 */ int cost[3][3];
};
static_assert(sizeof(UP_COST) == 0x28, "UP_COST is 0x28 bytes");
static_assert(offsetof(UP_COST, cost) == 0x04, "UP_COST.cost");

int  UpCostGet(int plyr, int id, int lv);       // FF3 0x00109e18: cost of the level after lv; 0 when lv < 0, -1 unknown id
int  CamLvGetCost(int plyr, int kind, int lv);  // FF3 0x00109eb0: UpCostGet of UP_COST_CAM_LV + kind

// ──────────────────────────────────────────────────────────────────────
// The step sequencer (FF3 0x00109f18..0x0010a158). Owners new one (or hold
// one), Add a nullptr-terminated step list and Run it each frame.
// ──────────────────────────────────────────────────────────────────────

struct STEP_SEQ;

// A step: returns STEP_* (STEP_SEQ::Run). Gets the sequencer, not the owner.
typedef int (*STEP_FUNC)(STEP_SEQ *seq);

#define STEP_CONT   0                   // run this step again next frame
#define STEP_NEXT   1                   // the jump target, else the next step
#define STEP_END    2                   // past the last step (finished)
#define STEP_STOP   3                   // stopped for good
#define STEP_WAIT   4                   // Run returns 1 this frame, same step next

#define STEP_SEQ_MAX 16

// 0x54 bytes on the EE. C++ in FF3 (a C1/C2 constructor pair, run by its
// owners' constructors).
// layout: host -- func[] and owner are 4-byte pointers on the EE (func at
// +0x00, owner +0x40, num +0x44, cur +0x48, jump +0x4c, stop +0x50).
struct STEP_SEQ
{
    STEP_SEQ();                         // FF3 0x00109f58 (StepSeqInit; base-object twin 0x00109f18 StepSeqInitTwin): cleared, owner nullptr, jump -1
    void Jump(STEP_FUNC f);             // FF3 0x00109f98 (StepSeqJump): jump = f's index (unchanged if absent)
    void Add(const STEP_FUNC *funcs);   // FF3 0x00109fd8 (StepSeqAdd): appends a nullptr-terminated list
    void Clear();                       // FF3 0x0010a028 (StepSeqClear): func[] 0, stop, num, cur 0
    int  Run();                         // FF3 0x0010a060 (StepSeqRun): one step; 1 when finished / stopped / waiting, 0 while running
    STEP_FUNC GetFunc() const;          // FF3 0x0010a128 (StepSeqGetFunc): func[cur]; nullptr when stopped or past the end

    STEP_FUNC func[STEP_SEQ_MAX];       // EE +0x00
    void     *owner;                    // EE +0x40: the object the steps work on
    int       num;                      // EE +0x44: steps in func[]
    int       cur;                      // EE +0x48: the running step
    int       jump;                     // EE +0x4c: step after the next STEP_NEXT, -1 none
    int       stop;                     // EE +0x50: Run does nothing once set
};
static_assert(offsetof(STEP_SEQ, owner) == STEP_SEQ_MAX * sizeof(STEP_FUNC), "STEP_SEQ.owner follows func[]");
static_assert(offsetof(STEP_SEQ, num) == offsetof(STEP_SEQ, owner) + sizeof(void *), "STEP_SEQ.num follows owner");
static_assert(offsetof(STEP_SEQ, stop) == offsetof(STEP_SEQ, num) + 0xc, "STEP_SEQ.stop");

// ──────────────────────────────────────────────────────────────────────
// Camera levels (FF3 0x0010a158..0x0010a5d8): kinds 0..2 are every player's
// charge / finder / range levels, 3 and 4 the current player's two special
// slots (lens grades or equipment levels, by player).
// ──────────────────────────────────────────────────────────────────────

#define CAM_LV_CHARGE   0
#define CAM_LV_FINDER   1
#define CAM_LV_RANGE    2
#define CAM_LV_SP1      3
#define CAM_LV_SP2      4
#define CAM_LV_NUM      5

int  CamLvGetKind(int slot);            // FF3 0x0010a158: kind of pip row 0..4, -1 none (3 falls back to 4)
int  CamLvGetSpKind(int slot);          // FF3 0x0010a1e8: slots 3, 4: the player's own ability (0..3), -1
int  CamLvSpGetEquip(int kind);         // FF3 0x0010a268: special slot 3 / 4 available (its level), -1 none
int  CamLvGet(int kind);                // FF3 0x0010a3c0: the current player's level of a kind
void CamLvUp(int kind);                 // FF3 0x0010a510: one level up
int  CamLvGetBase(int kind);            // FF3 0x0010a5a8: its starting points (table 0x34bf50)

// ──────────────────────────────────────────────────────────────────────
// Game clear (FF3 0x0010a5d8..0x0010a830, 0x0010aa78)
// ──────────────────────────────────────────────────────────────────────

#define CLEAR_RANK_NUM      5           // ranks with a time limit; ClearRankGet returns this one past them
#define CLEAR_REWARD_NUM    10

int  ClearRankGet(const PLAY_TIME *time);   // FF3 0x0010a5d8: 0 (S) .. 5 (E) by the rank times 0x34bfb8
int  ClearRewardIsOpen(int i);              // FF3 0x0010a640: reward 0..9 opened by this clear
int  ClearRewardAny(int lo, int hi);        // FF3 0x0010a7b0: ClearRewardCount > 0
int  ClearRewardCount(int lo, int hi);      // FF3 0x0010a7d0: open rewards in lo..hi
void ClearRewardSetOpened(int item2, int item3);    // FF3 0x0010aa78: title menu items 2 / 3 were open before the clear

// ──────────────────────────────────────────────────────────────────────
// Numbers (FF3 0x0010a830..0x0010aa78): message colour 0x2d, half-width
// digits 12 pixels apart.
// ──────────────────────────────────────────────────────────────────────

int  NumGetDigits(int n, int min);                                      // FF3 0x0010a830: decimal digits of n, at least min
void PrintNumberDigits(int n, int x, int y, u_char alpha, int digits);  // FF3 0x0010a868: leading zeros
void PrintNumberOrZero(int n, int x, int y, u_char alpha);              // FF3 0x0010a8b0: "0" for n <= 0
void PrintNumberPair(int a, int b, int x, int y, int alpha, int min);   // FF3 0x0010a918: "a / b"
void PrintNumberUnit(int n, int x, int y, int alpha, int min);          // FF3 0x0010a9e0: n and message 0x82/7

// ──────────────────────────────────────────────────────────────────────
// A two-axis menu cursor (FF3 0x0010aa88..0x0010afa8).
// ──────────────────────────────────────────────────────────────────────

struct CSR_AXIS                         /* 0x10 */
{
    /* 0x00 */ int   min;
    /* 0x04 */ int   max;
    /* 0x08 */ int   cur;
    /* 0x0c */ u_int skip;              // bit n: position n is jumped over
};
static_assert(sizeof(CSR_AXIS) == 0x10, "CSR_AXIS is 0x10 bytes");

// CSR_GRID.wrap (DAT_0036b608): bit per axis; a clear bit clamps.
#define CSR_GRID_WRAP_X     0x01
#define CSR_GRID_WRAP_Y     0x02
// CSR_GRID.moved: the edges CSR_GRID::Move ran into this frame (a wrapping axis
// wrapped, a clamping one stood at its end); not set by ordinary moves.
#define CSR_GRID_MOVED_LEFT 0x01
#define CSR_GRID_MOVED_UP   0x02
#define CSR_GRID_MOVED_RIGHT 0x04
#define CSR_GRID_MOVED_DOWN 0x08
// CSR_GRID.flags.
#define CSR_GRID_LR2        0x01        // L2 / R2 also move along x
#define CSR_GRID_LR1        0x02        // L1 / R1 also move along y
#define CSR_GRID_SE_X       0x10        // x input plays the SE
#define CSR_GRID_SE_Y       0x20        // y input plays the SE

// C++ in FF3 (a C1/C2 constructor pair; the menu pages' constructors build
// their arrays of them).
struct CSR_GRID                         /* 0x30 */
{
    CSR_GRID();                                     // FF3 0x0010aac0 (CsrGridInit; base-object twin 0x0010aa90 CsrGridInitTwin): all 0, se -1
    void SetRange(int axis, int min, int max);      // FF3 0x0010aaf0 (CsrGridSetRange): cur = min
    void SetPos(int axis, int pos);                 // FF3 0x0010ab50 (CsrGridSetPos): clamped
    void AddSkip(int axis, u_int mask);             // FF3 0x0010abb8 (CsrGridAddSkip)
    void SetSkip(int axis, int pos, int on);        // FF3 0x0010ac10 (CsrGridSetSkip)
    void ClearSkip(int axis);                       // FF3 0x0010ac90 (CsrGridClearSkip)
    int  Move();                                    // FF3 0x0010ada8 (CsrGridMove): per frame; the input (dx | dy), nonzero on input

    /* 0x00 */ CSR_AXIS axis[2];        // x, y
    /* 0x20 */ u_int    wrap;           // CSR_GRID_WRAP_*
    /* 0x24 */ u_int    moved;          // CSR_GRID_MOVED_*
    /* 0x28 */ u_int    flags;          // CSR_GRID_*
    /* 0x2c */ int      se;             // SndSePlay id, -1 none
};
static_assert(sizeof(CSR_GRID) == 0x30, "CSR_GRID is 0x30 bytes");
static_assert(offsetof(CSR_GRID, wrap) == 0x20, "CSR_GRID.wrap");
static_assert(offsetof(CSR_GRID, moved) == 0x24, "CSR_GRID.moved");
static_assert(offsetof(CSR_GRID, flags) == 0x28, "CSR_GRID.flags");
static_assert(offsetof(CSR_GRID, se) == 0x2c, "CSR_GRID.se");

#endif // _COMMON_CMN_UTIL_H
