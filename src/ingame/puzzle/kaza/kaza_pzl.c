/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_pzl.c
 *
 *  The kaza puzzle's hooks and phases 73 / 95 / 96 (see kaza_pzl.h).
 *  FF3 0x24a6c8..0x24acc8, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kaza/kaza_pzl.h"

#include "ingame/ingame.h"                      // PHASE12_PARAM, Phase12ParamSet, Phase12FadeLayerOn
#include "ingame/item/prg/item.h"               // ItemGetCnt, ITEM_NUM
#include "ingame/menu/menu_csr.h"               // MenuCsrInit / Update / Release / IsMovedSe / GetPos
#include "ingame/puzzle/kaza/kaza_draw.h"       // KazaGetBoardDef, KazaDraw
#include "ingame/puzzle/puzzle.h"               // PuzzleGetIdx / GetLayoutFile / GetRetPhase / Clear
#include "ingame/sound/snd_ctrl.h"              // SndSePlay
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, BpCtlIsLoadEnd, BpPlayLabel
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe, pad_key

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

KAZA_BOARD *kaza_board;                         // sdata 0x371f78

static const char kaza_label_out[] = "out";     // sdata 0x371f80

#define KAZA_ITEM_IDX0      0x2f        // the item board 0 needs
#define KAZA_ITEM_IDX1      0x30        // board 1's
#define KAZA_MSG            0x77        // phase 12's message group; msg1 below
#define KAZA_CSR            0x5f        // the menu cursor on the outer rings

#define KAZA_PHASE_SELECT   95
#define KAZA_PHASE_TURN     96
#define KAZA_PHASE_MSG      12

#define SE_CURSOR           0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_KAZA_OPEN        0x1bcb0000  // the puzzle's own bank (PAZZLE_STONE_BD_BD)
#define SE_KAZA_TURN        0x1bcb0001
#define SE_KAZA_CLEAR       0x1bcb0002

// A phase-12 message (puzzle.c's one_Story_PuzzleNoItem fills it the same way).
// PC: a shared helper; FF3 fills the stack param inline in each of its four
// callers, in this order.
static void KazaMsgSet(short msg1, int type, short next_ok, short next_alt, short next_cancel)
{
    PHASE12_PARAM param;

    param.msg0        = KAZA_MSG;
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
    GetGPhaseCtrl()->SetNext(KAZA_PHASE_MSG, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The board and the puzzle table's hooks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0024a6c8.
// callcheck: operator_new, KazaBoardCtor (new KAZA_BOARD)

void KazaBoardCreate(void)
{
    const KAZA_BOARD_DEF *def = KazaGetBoardDef(PuzzleGetIdx());
    kaza_board = new KAZA_BOARD(def);
}

// FF3 0x0024a718.
// callcheck: operator_delete (delete kaza_board; the rings' empty dtors run first in FF3)

void KazaBoardDelete(void)
{
    if (kaza_board != nullptr)
        delete kaza_board;
}

// FF3 0x0024a760.

void KazaPzlOpen(void)
{
    KazaBoardCreate();
    MenuCsrInit(KAZA_CSR, 2, 2, 0);
    KazaDraw();
}

// FF3 0x0024a790.

void KazaPzlClose(void)
{
    KazaBoardDelete();
    MenuCsrRelease(KAZA_CSR);
}

// FF3 0x0024a7b0.

int KazaPzlHasItem(void)
{
    int item = PuzzleGetIdx() == 0 ? KAZA_ITEM_IDX0 : KAZA_ITEM_IDX1;

    if (item >= ITEM_NUM)
        return 0;
    return *ItemGetCnt(item) > 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 73 Story_PzlKaza
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0024a808.

int init_Story_PzlKaza(void)
{
    SndSePlay(SE_KAZA_OPEN, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// The rules, then the board.
// FF3 0x0024a840.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn, GetGPhaseCtrl, GPhaseCtrlSetNext (KazaMsgSet)

int one_Story_PzlKaza(void)
{
    KazaMsgSet(1, 2, KAZA_PHASE_SELECT, -1, -1);
    return 1;
}

// FF3 0x0024a8c8.

int end_Story_PzlKaza(void)
{
    return 1;
}

// FF3 0x0024a8d0.

void pre_Story_PzlKaza(void)
{
}

// FF3 0x0024a8d8.

void after_Story_PzlKaza(void)
{
    KazaDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 95 PzlKaza_Select
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0024a8f0.

int init_PzlKaza_Select(void)
{
    return 1;
}

// The menu cursor picks the outer ring. OK turns it a quarter (the centre
// the other way, one move) and runs the turn (96); cancel asks to quit --
// yes returns to the phase that opened the puzzle, no stays.
// FF3 0x0024a8f8.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KazaMsgSet)

int one_PzlKaza_Select(void)
{
    KazaDrawMouse(KAZA_CSR);            // PC
    MenuCsrUpdate(KAZA_CSR, 0);
    kaza_board->SetCursor(MenuCsrGetPos(KAZA_CSR));
    if (MenuCsrIsMovedSe(KAZA_CSR, SE_CURSOR) != 0)
        return 0;

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) != 0)
    {
        kaza_board->TurnCursor(0);
        SndSePlay(SE_KAZA_TURN, 100, 0x1000, 0, nullptr, 0);
        GetGPhaseCtrl()->SetNext(KAZA_PHASE_TURN, 0);
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) == 0)
        return 0;
    KazaMsgSet(2, 1, PuzzleGetRetPhase(), KAZA_PHASE_SELECT, KAZA_PHASE_SELECT);
    return 1;
}

// FF3 0x0024aa60.

int end_PzlKaza_Select(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 96 PzlKaza_Turn
// ──────────────────────────────────────────────────────────────────────

// The rings turn until all rest; solved, the layout plays "out" (leaf
// state_cnt 1) and the phase ends once that animation is over.
// FF3 0x0024aa68.

int init_PzlKaza_Turn(void)
{
    int file = PuzzleGetLayoutFile();

    if (GetGPhaseCtrl()->leaf->state_cnt != 0)
        return BpCtlIsLoadEnd(file) != 0;

    if (kaza_board->IsTurning() != 0)
    {
        kaza_board->Step();
        return 0;
    }
    if (kaza_board->IsSolved() == 0)
        return 1;

    BpPlayLabel(BpCtlGetHandleByFileNo(file), kaza_label_out);
    GetGPhaseCtrl()->leaf->state_cnt = 1;
    return 0;
}

// Solved: the clear flag, the clear sound and the "solved" message back to
// the phase that opened the puzzle; out of moves: the "failed" message, the
// same way; else the next turn.
// FF3 0x0024ab48.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KazaMsgSet)

int one_PzlKaza_Turn(void)
{
    if (kaza_board->IsSolved() != 0)
    {
        PuzzleClear();
        SndSePlay(SE_KAZA_CLEAR, 100, 0x1000, 0, nullptr, 0);
        KazaMsgSet(4, 2, PuzzleGetRetPhase(), -1, -1);
        return 1;
    }
    if (kaza_board->IsFailed() != 0)
    {
        KazaMsgSet(3, 2, PuzzleGetRetPhase(), -1, -1);
        return 1;
    }
    GetGPhaseCtrl()->SetNext(KAZA_PHASE_SELECT, 0);
    return 1;
}

// FF3 0x0024aca0.

int end_PzlKaza_Turn(void)
{
    return 1;
}

// FF3 0x0024aca8 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.
