/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_pzl.c
 *
 *  The rousoku puzzle's hooks and phases 76 / 104 / 105 (see
 *  rousoku_pzl.h). FF3 0x2447b0..0x244e38 + 0x2be128..0x2be300, FF3's own
 *  code.
 * ======================================================================== */
#include "ingame/puzzle/rousoku/rousoku_pzl.h"

#include "ingame/ingame.h"                      // PHASE12_PARAM, Phase12ParamSet, Phase12FadeLayerOn
#include "ingame/puzzle/puzzle.h"               // PuzzleGetIdx / GetLayoutFile / GetRetPhase / Clear
#include "ingame/puzzle/rousoku/rousoku_draw.h" // RousokuGetBoardDef, RousokuDrawInit, RousokuDraw
#include "ingame/sound/snd_ctrl.h"              // SndSePlay
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, BpCtlIsLoadEnd, BpCtlRelease, BpPlayLabel
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe, CheckPadRptPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

ROUSOKU_BOARD *rousoku_board;                   // sdata 0x371e48

static u_short rousoku_csr;                     // sdata 0x37385c: the cursor cell

static const char rousoku_label_out[] = "out";  // sdata 0x371e50

// The cursor's neighbours by cell: up, down, left, right (-1 none). data 0x361690.
static const short rousoku_csr_nav[ROUSOKU_CELL_NUM][4] =
{
    { -1,  8,  7,  1 }, {  0,  2,  0,  2 }, {  1,  3,  8, 11 }, {  2,  4,  4,  2 }, {  8, -1,  5,  3 },
    {  6,  4,  6,  4 }, {  7,  5, -1,  8 }, {  0,  6,  6,  0 }, {  0,  4,  6,  2 },
    { -1, 17, 10, 16 }, {  9, 11, 11,  9 }, { 10, 12,  2, 17 }, { 11, 13, 11, 13 }, { 17, -1, 12, 14 },
    { 15, 13, 13, 15 }, { 16, 14, 17, -1 }, {  9, 15,  9, 15 }, {  9, 13, 11, 15 },
};

#define ROUSOKU_CSR_LEFT_CENTRE     8   // the first cursor: the left ring's centre
#define ROUSOKU_CSR_RIGHT_CENTRE    17  // when the left ring has nothing movable

#define ROUSOKU_MSG             0x74    // phase 12's message group; msg1 below
#define ROUSOKU_SWING           20      // frames the shadow swings after a move

#define ROUSOKU_PHASE_SELECT    104
#define ROUSOKU_PHASE_CHECK     105
#define ROUSOKU_PHASE_MSG       12

#define PAD_UP_BTN              0x1000
#define PAD_RIGHT_BTN           0x2000
#define PAD_DOWN_BTN            0x4000
#define PAD_LEFT_BTN            0x8000

#define SE_CURSOR               0x1a9b0000
#define SE_CANCEL               0x1a9b0001
#define SE_BUZZER               0x1a9b0002
#define SE_ROUSOKU_LIGHT        0x1bcf0000  // the puzzle's own bank (PAZZLE_ROUSOKU_BD_BD)
#define SE_ROUSOKU_OUT          0x1bcf0001
#define SE_ROUSOKU_CLEAR        0x1bcf0002

// A phase-12 message (puzzle.c's one_Story_PuzzleNoItem fills it the same way).
// PC: a shared helper; FF3 fills the stack param inline in each of its three
// callers, in this order.
static void RousokuMsgSet(short msg1, int type, short next_ok, short next_alt, short next_cancel)
{
    PHASE12_PARAM param;

    param.msg0        = ROUSOKU_MSG;
    param.msg1        = msg1;
    param.type        = type;
    param.next_ok     = next_ok;
    param.next_alt    = next_alt;
    param.next_cancel = next_cancel;
    param._0e         = 0;      // PC: FF3 leaves this stack half unset except in the rules (unread)
    param.end_func    = nullptr;
    param.win         = nullptr;
    Phase12ParamSet(&param);
    Phase12FadeLayerOn();
    GetGPhaseCtrl()->SetNext(ROUSOKU_PHASE_MSG, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The board and the puzzle table's hooks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002447b0.
// callcheck: operator_new, RousokuBoardCtor (new ROUSOKU_BOARD)

void RousokuBoardCreate(void)
{
    int idx = PuzzleGetIdx();

    rousoku_board = new ROUSOKU_BOARD(RousokuGetBoardDef(idx));
    rousoku_csr = ROUSOKU_CSR_LEFT_CENTRE;
    if (rousoku_board->half_movable[0] == 0)
        rousoku_csr = ROUSOKU_CSR_RIGHT_CENTRE;
    rousoku_board->cursor = rousoku_csr;
}

// FF3 0x00244828.
// callcheck: RousokuBoardDtor, operator_delete (delete rousoku_board)

void RousokuBoardDelete(void)
{
    if (rousoku_board != nullptr)
        delete rousoku_board;
    rousoku_board = nullptr;
}

// FF3 0x00244868.

void RousokuPzlOpen(void)
{
    RousokuBoardCreate();
    RousokuDrawInit();
}

// FF3 0x00244888.

int RousokuPzlHasItem(void)
{
    return 1;
}

// The cursor along the neighbour table, on repeat; it stays when the move
// would leave it in a half without a movable candle. A press sounds the
// cursor SE when it moved, the buzzer when not. Returns 1 on a press.
// FF3 0x002be128.

static int RousokuCsrMove(u_short *csr)
{
    u_short old = *csr;
    int     pressed = 0;
    int     dir = -1;
    int     movable;

    if (old >= ROUSOKU_CELL_NUM)
        return 0;

    if (CheckPadRptPlaySe(PAD_UP_BTN, -1, 0) != 0)
    {
        pressed = 1;
        if (rousoku_csr_nav[old][0] >= 0)
            dir = 0;
    }
    if (dir == -1 && CheckPadRptPlaySe(PAD_DOWN_BTN, -1, 0) != 0)
    {
        pressed = 1;
        if (rousoku_csr_nav[old][1] >= 0)
            dir = 1;
    }
    if (dir == -1 && CheckPadRptPlaySe(PAD_LEFT_BTN, -1, 0) != 0)
    {
        pressed = 1;
        if (rousoku_csr_nav[old][2] >= 0)
            dir = 2;
    }
    if (dir == -1 && CheckPadRptPlaySe(PAD_RIGHT_BTN, -1, 0) != 0)
    {
        pressed = 1;
        if (rousoku_csr_nav[old][3] >= 0)
            dir = 3;
    }
    if (dir != -1)
        *csr = (u_short)rousoku_csr_nav[old][dir];

    if ((short)*csr < ROUSOKU_HALF_CELLS)
        movable = rousoku_board->half_movable[0];
    else
        movable = rousoku_board->half_movable[1];
    if (movable == 0)
        *csr = old;

    if (pressed != 0)
        SndSePlay(*csr != old ? SE_CURSOR : SE_BUZZER, 100, 0x1000, 0, nullptr, 0);
    return pressed;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 76 Story_PzlRousoku
// ──────────────────────────────────────────────────────────────────────

// One frame, then the layout's opening animation.
// FF3 0x00244890.

int init_Story_PzlRousoku(void)
{
    GPHASE_NODE *leaf = GetGPhaseCtrl()->leaf;

    if (leaf->state_cnt == 0)
    {
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }
    if (leaf->state_cnt == 1)
        return BpCtlIsLoadEnd(PuzzleGetLayoutFile()) != 0;
    return 0;
}

// The rules, then the board.
// FF3 0x00244928.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn, GetGPhaseCtrl, GPhaseCtrlSetNext (RousokuMsgSet)

int one_Story_PzlRousoku(void)
{
    RousokuMsgSet(0, 2, ROUSOKU_PHASE_SELECT, -1, -1);
    return 1;
}

// The layout plays "out"; once it ended the layout and the board go.
// FF3 0x002449b0.

int end_Story_PzlRousoku(void)
{
    GPHASE_NODE *leaf = GetGPhaseCtrl()->leaf;

    if (leaf->state_cnt == 0)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), rousoku_label_out);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }
    if (leaf->state_cnt == 1)
    {
        if (BpCtlIsLoadEnd(PuzzleGetLayoutFile()) == 0)
            return 0;
        BpCtlRelease(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()));
        RousokuBoardDelete();
        return 1;
    }
    return 0;
}

// FF3 0x00244a80.

void pre_Story_PzlRousoku(void)
{
}

// FF3 0x00244a88.

void after_Story_PzlRousoku(void)
{
    RousokuDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 104 PzlRousoku_Select
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00244aa0.

int init_PzlRousoku_Select(void)
{
    return 1;
}

// The cursor; OK lights or puts out the cursor's movable candle and lets
// the shadow swing (105), or buzzes on a fixed place; cancel asks to quit --
// yes returns to the phase that opened the puzzle, no stays.
// FF3 0x00244aa8.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (RousokuMsgSet)

// PC: the mouse. The candles of a ring with something to move can be pointed
// at; a click puts the cursor there and lights or snuffs it.
static void RousokuMouse(void)
{
    static const int guide[3] = { -1, REIPAN_MENU_CROSS, REIPAN_MENU_TRIANGLE };
    ReiPan_MenuBox box;

    ReiPan_MenuMouseBegin(0, nullptr);
    MenuMouseGuide(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), "R_cap", guide, 3);
    for (int i = 0; i < ROUSOKU_CELL_NUM; i++)
    {
        if (rousoku_board->half_movable[i < ROUSOKU_HALF_CELLS ? 0 : 1] == 0 || !RousokuDrawMouseBox(i, &box))
            continue;
        if (ReiPan_MenuMouseClick(box.x, box.y, box.w, box.h))
        {
            rousoku_csr = (u_short)i;
            ReiPan_MenuMousePress(REIPAN_MENU_CROSS);
        }
        else if (ReiPan_MenuMouseHover(box.x, box.y, box.w, box.h) && rousoku_csr != i)
        {
            rousoku_csr = (u_short)i;
            SndSePlay(SE_CURSOR, 100, 0x1000, 0, nullptr, 0);
        }
    }
}

int one_PzlRousoku_Select(void)
{
    RousokuMouse();                     // PC
    int moved = RousokuCsrMove(&rousoku_csr);

    rousoku_board->cursor = rousoku_csr;
    rousoku_board->show_buttons = 1;
    if (moved != 0)
        return 0;

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) != 0)
    {
        if ((rousoku_board->cell[rousoku_board->cursor] & ROUSOKU_CELL_FIXED) == 0)
        {
            rousoku_board->SaveShadow();
            rousoku_board->Toggle();
            SndSePlay((rousoku_board->cell[rousoku_board->cursor] & ROUSOKU_CELL_LIT) ? SE_ROUSOKU_LIGHT : SE_ROUSOKU_OUT,
                      100, 0x1000, 0, nullptr, 0);
            rousoku_board->swing = ROUSOKU_SWING;
            GetGPhaseCtrl()->SetNext(ROUSOKU_PHASE_CHECK, 0);
            return 1;
        }
        SndSePlay(SE_BUZZER, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) == 0)
        return 0;
    RousokuMsgSet(1, 1, PuzzleGetRetPhase(), ROUSOKU_PHASE_SELECT, ROUSOKU_PHASE_SELECT);
    rousoku_board->show_buttons = 0;
    return 1;
}

// FF3 0x00244c80.

int end_PzlRousoku_Select(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 105 PzlRousoku_Check
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00244c88.

int init_PzlRousoku_Check(void)
{
    return 1;
}

// Not balanced: back to the cursor once the swing is a tenth through.
// Balanced: once the swing is over every candle lights and flares, the
// shadow goes, and the clear flag, the clear sound and the "solved" message
// lead back to the phase that opened the puzzle. Cancel ends the swing.
// FF3 0x00244c90.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (RousokuMsgSet)

int one_PzlRousoku_Check(void)
{
    if (rousoku_board->IsBalanced() == 0)
    {
        if ((float)rousoku_board->swing / 20.0f <= 0.9f)    // sdata 0x371e54 / 0x371e58
        {
            GetGPhaseCtrl()->SetNext(ROUSOKU_PHASE_SELECT, 0);
            return 1;
        }
    }
    else if (rousoku_board->swing < 1)
    {
        rousoku_board->LightAll();
        rousoku_board->show_shadow = 0;
        SndSePlay(SE_ROUSOKU_CLEAR, 100, 0x1000, 0, nullptr, 0);
        PuzzleClear();
        RousokuMsgSet(2, 2, PuzzleGetRetPhase(), -1, -1);
        rousoku_board->show_buttons = 0;
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0) != 0)
        rousoku_board->swing = 0;
    return 0;
}

// FF3 0x00244e10.

int end_PzlRousoku_Check(void)
{
    return 1;
}

// FF3 0x00244e18 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.
