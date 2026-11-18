/* ==========================================================================
 *  common/cmn_util.c
 *
 *  FF3-only in-game helpers (see cmn_util.h). FF3 0x00109dd8..0x0010afa8:
 *  after CBgSe's static initializer pair, before CDebugEdit's CEditRoot
 *  constructors.
 * ======================================================================== */
#include "common/cmn_util.h"

#include <string.h>                     // memset

#include "graphics/graph2d/message.h"   // MsgPrintPage, PrintNumber, PrintNumber_N
#include "ingame/clear/GameClear.h"     // GameClearGetKeptFinderFlag
#include "common/cmn_time.h"           // PlayTimeMax
#include "ingame/ingame_wrk.h"          // ingame_wrk, PLAY_TIME, IngameWrkGetDifficulty
#include "ingame/menu/menuCamBasic.h"   // MenuCamGetLensWrk
#include "ingame/menu/start_menu.h"     // FINDER_STATE_WRK, GetFinderStateWrk, FUN_001a5e60
#include "ingame/photo/fndr_parts.h"    // LensGetGrade, LensGradeUp
#include "ingame/plyr/player.h"         // PlyrGetCurNo, PlyrGetEquipLv, PlyrEquipLvUp
#include "ingame/plyr/plyr_param.h"     // PlyrGetCamLv*, PlyrCamLv*Up
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "system/pad/pad_ctrl.h"        // CheckPadRptPlaySe
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// Point costs per item and player row (data 0x2ef890): items 0..8, then the
// camera levels UP_COST_CAM_LV + CAM_LV_*.
static const UP_COST up_cost_tbl[UP_COST_NUM] = {
    {    0, {{10000, 15000, 20000}, {    0,     0,     0}, {    0,     0,     0}}},
    {    1, {{15000, 25000, 35000}, {    0,     0,     0}, {15000, 25000, 35000}}},
    {    2, {{20000, 35000, 50000}, {    0,     0,     0}, {20000, 35000, 50000}}},
    {    3, {{20000, 35000, 50000}, {    0,     0,     0}, {20000, 35000, 50000}}},
    {    4, {{    0,     0,     0}, {    0,     0,     0}, {10000, 15000, 20000}}},
    {    5, {{15000, 25000, 35000}, {    0,     0,     0}, {15000, 25000, 35000}}},
    {    6, {{10000, 15000, 20000}, {    0,     0,     0}, {10000, 15000, 20000}}},
    {    7, {{10000, 15000, 20000}, {    0,     0,     0}, {10000, 15000, 20000}}},
    {    8, {{    0,     0,     0}, {    0,     0,     0}, {15000, 25000, 35000}}},
    { 2000, {{10000, 20000, 40000}, {15000, 30000, 50000}, {10000, 20000, 40000}}},
    { 2001, {{10000, 20000, 40000}, {15000, 30000, 50000}, {10000, 20000, 40000}}},
    { 2002, {{10000, 20000, 40000}, {15000, 30000, 50000}, {10000, 20000, 40000}}},
    { 2003, {{10000, 15000, 20000}, {20000, 40000, 60000}, {10000, 15000, 20000}}},
    { 2004, {{10000, 15000, 20000}, {20000, 40000, 60000}, {    0,     0,     0}}},
};

// Starting points per camera level kind and player (rodata 0x34bf50).
static const int cam_lv_base[CAM_LV_NUM][3] = {
    {1, 2, 0},
    {2, 0, 1},
    {1, 2, 0},
    {0, 0, 0},
    {0, 0, 0},
};

// The rank limits (rodata 0x34bfb8): a clear within limit i gets rank i.
// frame 99 is past any real frame count (0..29), so a limit includes its
// whole last second.
static const PLAY_TIME clear_rank_time[CLEAR_RANK_NUM] = {
    { 3, 30, 0, 99},
    { 4,  0, 0, 99},
    { 6,  0, 0, 99},
    {10,  0, 0, 99},
    {15,  0, 0, 99},
};

static FINDER_STATE_WRK *clear_fsw;     // sbss gp-0xc80: GetFinderStateWrk(), fetched on first use
static int clear_item2_opened;          // sdata gp-0x7ef0: title menu item 2 was open before this clear
static int clear_item3_opened;          // sdata gp-0x7eec: title menu item 3 was open before this clear

#define CMN_NUM_COL         0x2d        // message colour label of the number printers
#define CMN_NUM_STEP        0xc         // half-width digit advance
#define CMN_MSG_TYPE        0x82        // message type of the separators
#define CMN_MSG_SLASH       6           // "/" between PrintNumberPair's numbers
#define CMN_MSG_UNIT        7           // the unit after PrintNumberUnit's number

// Pad bits CSR_GRID::Move reads (PAD_WRK.rpt).
#define CSR_PAD_L2          0x0001
#define CSR_PAD_R2          0x0002
#define CSR_PAD_L1          0x0004
#define CSR_PAD_R1          0x0008
#define CSR_PAD_UP          0x1000
#define CSR_PAD_RIGHT       0x2000
#define CSR_PAD_DOWN        0x4000
#define CSR_PAD_LEFT        0x8000

#define CSR_SE_VOL          100
#define CSR_SE_PITCH        0x1000

// ──────────────────────────────────────────────────────────────────────
// Level-up point costs
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00109dd8: the cost row of a player, -1 for none.
static int UpCostGetRow(int plyr)
{
    switch (plyr)
    {
    case 0:  return 0;
    case 1:  return 1;
    case 2:  return 2;
    default: return -1;
    }
}

// FF3 0x00109e18
int UpCostGet(int plyr, int id, int lv)
{
    int row;
    int i;

    if (lv < 0)
        return 0;

    row = UpCostGetRow(plyr);
    for (i = 0; up_cost_tbl[i].id != id; i++)
    {
        if (i + 1 >= UP_COST_NUM)
            return -1;
    }

    // PC guard: FF3 reads cost[-1][lv] (the entry before) for a player
    // outside 0..2; PlyrGetCurNo never gives one.
    if (row < 0)
        return 0;
    return (&up_cost_tbl[i].cost[0][0])[row * 3 + lv];
}

// FF3 0x00109eb0
int CamLvGetCost(int plyr, int kind, int lv)
{
    int id;

    switch (kind)
    {
    case CAM_LV_CHARGE: id = UP_COST_CAM_LV + CAM_LV_CHARGE; break;
    case CAM_LV_FINDER: id = UP_COST_CAM_LV + CAM_LV_FINDER; break;
    case CAM_LV_RANGE:  id = UP_COST_CAM_LV + CAM_LV_RANGE;  break;
    case CAM_LV_SP1:    id = UP_COST_CAM_LV + CAM_LV_SP1;    break;
    case CAM_LV_SP2:    id = UP_COST_CAM_LV + CAM_LV_SP2;    break;
    default:            id = UP_COST_CAM_LV;                 break;
    }
    return UpCostGet(plyr, id, lv);
}

// ──────────────────────────────────────────────────────────────────────
// The step sequencer
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00109f18 (StepSeqInitTwin) is the base-object twin with the same
// body; nothing calls it.
// FF3 0x00109f58 (StepSeqInit)
STEP_SEQ::STEP_SEQ()
{
    Clear();
    owner = nullptr;
    jump  = -1;
    stop  = 0;
    num   = 0;
    cur   = 0;
}

// FF3 0x00109f98
void STEP_SEQ::Jump(STEP_FUNC f)
{
    int i;

    for (i = 0; i < num; i++)
    {
        if (func[i] == f)
        {
            jump = i;
            return;
        }
    }
}

// FF3 0x00109fd8: no bound check against STEP_SEQ_MAX (as in FF3).
void STEP_SEQ::Add(const STEP_FUNC *funcs)
{
    int i;

    for (i = 0; funcs[i] != nullptr; i++)
        func[num++] = funcs[i];
}

// FF3 0x0010a028
void STEP_SEQ::Clear()
{
    int i;

    for (i = 0; i < STEP_SEQ_MAX; i++)
        func[i] = nullptr;
    stop = 0;
    num  = 0;
    cur  = 0;
}

// FF3 0x0010a060
int STEP_SEQ::Run()
{
    if (stop != 0 || cur >= num)
        return 1;

    switch (func[cur](this))
    {
    case STEP_NEXT:
        if (jump < 0)
        {
            cur++;
        }
        else
        {
            cur  = jump;
            jump = -1;
        }
        return 0;
    case STEP_END:
        cur = num;
        return 1;
    case STEP_STOP:
        stop = 1;
        return 1;
    case STEP_WAIT:
        return 1;
    case STEP_CONT:
    default:
        return 0;
    }
}

// FF3 0x0010a128
STEP_FUNC STEP_SEQ::GetFunc() const
{
    if (stop != 0 || cur >= num)
        return nullptr;
    return func[cur];
}

// ──────────────────────────────────────────────────────────────────────
// Camera levels
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010a158: rows 0..2 show their own kind; row 3 shows special slot 3,
// or slot 4 when the player has no slot 3; row 4 shows slot 4 when both are
// there.
int CamLvGetKind(int slot)
{
    int sp1 = CamLvSpGetEquip(CAM_LV_SP1);
    int sp2 = CamLvSpGetEquip(CAM_LV_SP2);

    if (slot == 3)
    {
        if (sp1 != 0)
            return CAM_LV_SP1;
    }
    else if (slot < 4)
    {
        if (slot < 0)
            return -1;
        return slot;
    }
    else
    {
        if (slot != 4)
            return -1;
        if (sp1 == 0)
            return -1;
    }
    return (sp2 != 0) ? CAM_LV_SP2 : -1;
}

// FF3 0x0010a1e8: which of the four special abilities slot 3 / 4 holds for
// the current player: player 0 0 / 1, player 1 2 / 3, player 2 0 / none.
int CamLvGetSpKind(int slot)
{
    int plyr;

    if (CamLvSpGetEquip(slot) == 0)
        return -1;

    plyr = PlyrGetCurNo();
    switch (plyr)
    {
    case 0:  return (slot != CAM_LV_SP1) ? 1 : 0;
    case 1:  return (slot != CAM_LV_SP1) ? 3 : 2;
    case 2:  return (slot == CAM_LV_SP1) ? 0 : -1;
    default: return -1;
    }
}

// FF3 0x0010a268
int CamLvSpGetEquip(int kind)
{
    switch (PlyrGetCurNo())
    {
    case 0:
        if (kind == CAM_LV_SP1)
            return 1;
        return PlyrGetEquipLv(0);
    case 1:
        return PlyrGetEquipLv((kind == CAM_LV_SP1) ? 2 : 5);
    case 2:
        return kind == CAM_LV_SP1;
    default:
        return -1;
    }
}

// FF3 0x0010a300: CamLvGet of the special slots.
static int CamLvGetSp(int kind)
{
    int       plyr = PlyrGetCurNo();
    LENS_WRK *lens = MenuCamGetLensWrk();

    switch (plyr)
    {
    case 0:
        if (kind != CAM_LV_SP1)
            return PlyrGetEquipLv(1);
        return lens->GetGrade(0);
    case 1:
        return PlyrGetEquipLv((kind == CAM_LV_SP1) ? 4 : 6);
    case 2:
        if (kind == CAM_LV_SP1)
            return lens->GetGrade(2);
        return -1;
    default:
        return -1;
    }
}

// FF3 0x0010a3c0
int CamLvGet(int kind)
{
    u_char plyr = PlyrGetCurNo();

    switch (kind)
    {
    case CAM_LV_CHARGE: return PlyrGetCamLvCharge(plyr);
    case CAM_LV_FINDER: return PlyrGetCamLvFinder(plyr);
    case CAM_LV_RANGE:  return PlyrGetCamLvRange(plyr);
    case CAM_LV_SP1:
    case CAM_LV_SP2:    return CamLvGetSp(kind);
    default:            return 0;
    }
}

// FF3 0x0010a450: CamLvUp of the special slots.
static void CamLvSpUp(int kind)
{
    int       plyr = PlyrGetCurNo();
    LENS_WRK *lens = MenuCamGetLensWrk();

    switch (plyr)
    {
    case 0:
        if (kind != CAM_LV_SP1)
            PlyrEquipLvUp(1);
        else
            lens->GradeUp(0);
        break;
    case 1:
        PlyrEquipLvUp((kind == CAM_LV_SP1) ? 4 : 6);
        break;
    case 2:
        if (kind == CAM_LV_SP1)
            lens->GradeUp(2);
        break;
    }
}

// FF3 0x0010a510
void CamLvUp(int kind)
{
    u_char plyr = PlyrGetCurNo();

    switch (kind)
    {
    case CAM_LV_CHARGE: PlyrCamLvChargeUp(plyr); break;
    case CAM_LV_FINDER: PlyrCamLvFinderUp(plyr); break;
    case CAM_LV_RANGE:  PlyrCamLvRangeUp(plyr);  break;
    case CAM_LV_SP1:
    case CAM_LV_SP2:    CamLvSpUp(kind);         break;
    }
}

// FF3 0x0010a5a8
int CamLvGetBase(int kind)
{
    return cam_lv_base[kind][ingame_wrk.b00];
}

// ──────────────────────────────────────────────────────────────────────
// Game clear
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010a5d8: the first rank whose limit is later than time.
int ClearRankGet(const PLAY_TIME *time)
{
    int i;

    for (i = 0; i < CLEAR_RANK_NUM; i++)
    {
        if (PlayTimeMax(&clear_rank_time[i], time) == &clear_rank_time[i])
            return i;
    }
    return CLEAR_RANK_NUM;
}

// FF3 0x0010a640: rewards 0..5 come with the first clear (5 only when title
// menu item 2 was still closed), 6 with the first clear on difficulty 2
// (item 3 still closed), 7 / 8 with the clear flags _04 / _08 unless kept,
// 9 with clears 1..3 or the first clear on difficulty 3.
int ClearRewardIsOpen(int i)
{
    FINDER_STATE_WRK *fsw;

    if (clear_fsw == nullptr)
        clear_fsw = GetFinderStateWrk();
    fsw = clear_fsw;

    switch (i)
    {
    case 5:
        if (clear_item2_opened != 0)
            return 0;
        // fall through
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        return fsw->FUN_001a5e60() == 1;
    case 6:
        if (clear_item3_opened != 0)
            return 0;
        if (IngameWrkGetDifficulty(1) != 2)
            return 0;
        return fsw->cnt[2] == 1;
    case 7:
        if (GameClearGetKeptFinderFlag(0) != 0)
            return 0;
        if (clear_fsw == nullptr)
            clear_fsw = GetFinderStateWrk();
        return clear_fsw->_04 != 0;
    case 8:
        if (GameClearGetKeptFinderFlag(1) != 0)
            return 0;
        if (clear_fsw == nullptr)
            clear_fsw = GetFinderStateWrk();
        return clear_fsw->_08 != 0;
    case 9:
        if ((u_int)(fsw->FUN_001a5e60() - 1) < 3)
            return 1;
        if (IngameWrkGetDifficulty(1) != 3)
            return 0;
        return fsw->cnt[3] == 1;
    default:
        return 0;
    }
}

// FF3 0x0010a7b0
int ClearRewardAny(int lo, int hi)
{
    return ClearRewardCount(lo, hi) > 0;
}

// FF3 0x0010a7d0
int ClearRewardCount(int lo, int hi)
{
    int cnt = 0;

    for (; lo <= hi; lo++)
    {
        if (ClearRewardIsOpen(lo) != 0)
            cnt++;
    }
    return cnt;
}

// FF3 0x0010aa78: EvGameClear (GAME_CLEAR_) passes TitleMenuIsItemOpen(2), (3)
// before it opens them.
void ClearRewardSetOpened(int item2, int item3)
{
    clear_item2_opened = item2;
    clear_item3_opened = item3;
}

// ──────────────────────────────────────────────────────────────────────
// Numbers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010a830
int NumGetDigits(int n, int min)
{
    int digits = 0;

    for (; n > 0; n /= 10)
        digits++;
    if (digits < min)
        digits = min;
    return digits;
}

// FF3 0x0010a868
void PrintNumberDigits(int n, int x, int y, u_char alpha, int digits)
{
    PrintNumber_N(n, digits, x, y, CMN_NUM_COL, alpha, 0, 1, 1);
}

// FF3 0x0010a8b0
void PrintNumberOrZero(int n, int x, int y, u_char alpha)
{
    if (n <= 0)
    {
        PrintNumber_N(0, 1, x, y, CMN_NUM_COL, alpha, 0, 1, 1);
        return;
    }
    PrintNumber(n, x, y, CMN_NUM_COL, alpha, 0, 1);
}

// FF3 0x0010a918: min is a's minimum digit count (where the "/" goes).
void PrintNumberPair(int a, int b, int x, int y, int alpha, int min)
{
    PrintNumberOrZero(a, x, y, (u_char)alpha);
    x += NumGetDigits(a, min) * CMN_NUM_STEP;
    MsgPrintPage(CMN_MSG_TYPE, CMN_MSG_SLASH, x + 1, y, CMN_NUM_COL, alpha, 0, 0, 0, 0);
    PrintNumberOrZero(b, x + 0xf, y, (u_char)alpha);
}

// FF3 0x0010a9e0
void PrintNumberUnit(int n, int x, int y, int alpha, int min)
{
    PrintNumberOrZero(n, x, y, (u_char)alpha);
    MsgPrintPage(CMN_MSG_TYPE, CMN_MSG_UNIT, x + NumGetDigits(n, min) * CMN_NUM_STEP, y, CMN_NUM_COL, alpha,
                 0, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The two-axis menu cursor
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0010aa88: empty; every axis accessor calls it with the axis in a0
// (a compiled-out range check), so it is no member.
static void CsrGridCheckAxis(int axis)
{
    (void)axis;
}

// FF3 0x0010aa90 (CsrGridInitTwin) is the base-object twin with the same
// body; nothing calls it.
// FF3 0x0010aac0 (CsrGridInit): moved is left as it is.
CSR_GRID::CSR_GRID()
{
    flags = 0;
    se    = -1;
    wrap  = 0;
    memset(axis, 0, sizeof(axis));
}

// FF3 0x0010aaf0
void CSR_GRID::SetRange(int axis, int min, int max)
{
    CSR_AXIS *a;

    CsrGridCheckAxis(axis);
    a = &this->axis[axis];
    a->cur = min;
    a->max = max;
    a->min = min;
}

// FF3 0x0010ab50
void CSR_GRID::SetPos(int axis, int pos)
{
    CSR_AXIS *a;

    CsrGridCheckAxis(axis);
    a = &this->axis[axis];
    if (pos < a->min)
        pos = a->min;
    if (a->max < pos)
        pos = a->max;
    a->cur = pos;
}

// FF3 0x0010abb8
void CSR_GRID::AddSkip(int axis, u_int mask)
{
    CsrGridCheckAxis(axis);
    this->axis[axis].skip |= mask;
}

// FF3 0x0010ac10
void CSR_GRID::SetSkip(int axis, int pos, int on)
{
    CSR_AXIS *a;
    u_int     bit;

    CsrGridCheckAxis(axis);
    a   = &this->axis[axis];
    bit = 1u << (pos & 0x1f);
    if (on != 0)
        a->skip |= bit;
    else
        a->skip &= ~bit;
}

// FF3 0x0010ac90
void CSR_GRID::ClearSkip(int axis)
{
    CsrGridCheckAxis(axis);
    this->axis[axis].skip = 0;
}

// Whether position pos is skipped (positions outside 0..31 never are).
static int CsrAxisIsSkip(const CSR_AXIS *a, int pos)
{
    if ((u_int)pos >= 32)
        return 0;
    return (a->skip >> pos) & 1;
}

// FF3 0x0010acd0: moves by d, wrapping from one end to the other; -1 when it
// wrapped below min, 1 above max, else 0. Stops on its start when every
// position is skipped.
static int CsrAxisMoveWrap(CSR_AXIS *a, int d)
{
    int edge = 0;
    int pos  = a->cur;

    do
    {
        pos += d;
        if (pos < a->min)
        {
            edge = -1;
            pos  = a->max;
        }
        else if (a->max < pos)
        {
            edge = 1;
            pos  = a->min;
        }
    } while (CsrAxisIsSkip(a, pos) && a->cur != pos);

    a->cur = pos;
    return edge;
}

// FF3 0x0010ad40: moves by d to the next position that is not skipped;
// returns d and leaves cur when there is none before the end, else 0.
static int CsrAxisMoveClamp(CSR_AXIS *a, int d)
{
    int pos = a->cur;

    // PC guard: FF3 spins forever when d is 0 and cur sits on a skipped
    // position; no caller sets one up.
    if (d == 0)
        return 0;

    do
    {
        pos += d;
        if (pos < a->min || a->max < pos)
            return d;
    } while (CsrAxisIsSkip(a, pos));

    a->cur = pos;
    return 0;
}

// FF3 0x0010ada8: one direction per frame (left, right, up, down in that
// order of priority) on the pad repeat, moves the axes, records the edges
// in moved and plays the cursor SE for an input on an axis with
// CSR_GRID_SE_*. Returns dx | dy (-1, 0, 1).
int CSR_GRID::Move()
{
    static const u_int wrap_bit[2] = { CSR_GRID_WRAP_X, CSR_GRID_WRAP_Y };  // data 0x36b608
    u_short left  = CSR_PAD_LEFT;
    u_short right = CSR_PAD_RIGHT;
    u_short up    = CSR_PAD_UP;
    u_short down  = CSR_PAD_DOWN;
    int     d[2]  = { 0, 0 };
    int     i;
    int     se;

    moved = 0;
    if (flags & CSR_GRID_LR2)
    {
        left  |= CSR_PAD_L2;
        right |= CSR_PAD_R2;
    }
    if (flags & CSR_GRID_LR1)
    {
        up   |= CSR_PAD_L1;
        down |= CSR_PAD_R1;
    }

    if (CheckPadRptPlaySe(left, -1, 0) != 0)
        d[0] = -1;
    else if (CheckPadRptPlaySe(right, -1, 0) != 0)
        d[0] = 1;
    else if (CheckPadRptPlaySe(up, -1, 0) != 0)
        d[1] = -1;
    else if (CheckPadRptPlaySe(down, -1, 0) != 0)
        d[1] = 1;

    for (i = 0; i < 2; i++)
    {
        int edge;

        if (wrap & wrap_bit[i])
            edge = CsrAxisMoveWrap(&axis[i], d[i]);
        else
            edge = CsrAxisMoveClamp(&axis[i], d[i]);

        switch ((i + 1) * edge)
        {
        case -1: moved |= CSR_GRID_MOVED_LEFT;  break;
        case -2: moved |= CSR_GRID_MOVED_UP;    break;
        case 1:  moved |= CSR_GRID_MOVED_RIGHT; break;
        case 2:  moved |= CSR_GRID_MOVED_DOWN;  break;
        }
    }

    // PC: a move the mouse asked for: to the menu it is a step of the d-pad.
    {
        int axis_m = 0;
        const int step_m = MenuMouseGridApply(this, &axis_m);

        MenuMouseGridClaim(this);
        if (step_m != 0)
            d[axis_m] = step_m;
    }

    se = 0;
    if (d[1] != 0)
        se = (flags & CSR_GRID_SE_Y) != 0;
    if (d[0] != 0 && (flags & CSR_GRID_SE_X))
        se = 1;
    if (se)
        SndSePlay(se, CSR_SE_VOL, CSR_SE_PITCH, 0, nullptr, 0);

    return d[1] | d[0];
}
