/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_pzl.c
 *
 *  The kamon puzzle's hooks and phases 74 / 97 / 98 / 99 (see kamon_pzl.h).
 *  FF3 0x248658..0x248f80, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kamon/kamon_pzl.h"

#include "graphics/gs_reg.h"                    // ScreenCaptureSave
#include "ingame/ingame.h"                      // PHASE12_PARAM, Phase12ParamSet, Phase12FadeLayerOn
#include "ingame/item/prg/item.h"               // ItemGetCnt
#include "ingame/puzzle/kamon/kamon_draw.h"     // KamonGetBoardDef, KamonDraw, the fades
#include "ingame/puzzle/puzzle.h"               // PuzzleGetIdx / GetLayoutFile / GetRetPhase / Clear, cap alpha
#include "ingame/sound/snd_ctrl.h"              // SndSePlay
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, BpCtlIsLoadEnd, BpPlayLabel
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe / RptPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

KAMON_BOARD *kamon_board;                       // sdata 0x371f18

static const char kamon_label_out[]  = "out";   // sdata 0x371f20
static const char kamon_label_next[] = "next";  // sdata 0x371f28
static const char kamon_label_back[] = "back";  // sdata 0x371f30
static const char kamon_label_in[]   = "in";    // sdata 0x371f38

#define KAMON_ITEM          0x2e        // the item the puzzle needs
#define KAMON_MSG           0x76        // phase 12's message group; msg1 below

#define KAMON_PHASE_SELECT  97
#define KAMON_PHASE_MOVE    98
#define KAMON_PHASE_RETRY   99
#define KAMON_PHASE_MSG     12

#define SE_CURSOR           0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_BUZZER           0x1a9b0002
#define SE_KAMON_SOLVED     0x1bcb0000  // the puzzle's own bank (PAZZLE_STONE_BD_BD)
#define SE_KAMON_MOVE       0x1bcb0001
#define SE_KAMON_CLEAR      0x1bcb0002

#define PAD_RIGHT_BTN       0x2000
#define PAD_LEFT_BTN        0x8000
#define PAD_CIRCLE_BTN      0x20

// A phase-12 message (puzzle.c's one_Story_PuzzleNoItem fills it the same way).
// PC: a shared helper; FF3 fills the stack param inline in each of its four
// callers, in this order.
static void KamonMsgSet(short msg1, int type, short next_ok, short next_alt, short next_cancel)
{
    PHASE12_PARAM param;

    param.msg0        = KAMON_MSG;
    param.msg1        = msg1;
    param.type        = type;
    param.next_ok     = next_ok;
    param.next_alt    = next_alt;
    param.next_cancel = next_cancel;
    param._0e         = 0;      // PC: FF3 leaves this stack half unset (unread)
    param.end_func    = nullptr;
    param.win         = nullptr;
    Phase12ParamSet(&param);
    Phase12FadeLayerOn();
    GetGPhaseCtrl()->SetNext(KAMON_PHASE_MSG, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The board and the puzzle table's hooks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00248658.
// callcheck: operator_new, KamonBoardCtor (new KAMON_BOARD)

void KamonBoardCreate(void)
{
    const KAMON_BOARD_DEF *def = KamonGetBoardDef(PuzzleGetIdx());
    kamon_board = new KAMON_BOARD(def);
}

// FF3 0x002486a8.
// callcheck: KamonBoardDtor, operator_delete (delete kamon_board)

void KamonBoardDelete(void)
{
    if (kamon_board != nullptr)
        delete kamon_board;
}

// FF3 0x002486f0.

void KamonPzlOpen(void)
{
    KamonBoardCreate();
    KamonFadeReset();
    KamonDraw();
}

// FF3 0x00248718.

void KamonPzlClose(void)
{
    KamonBoardDelete();
}

// FF3 0x00248730.

int KamonPzlHasItem(void)
{
    return *ItemGetCnt(KAMON_ITEM) > 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 74 Story_PzlKamon
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00248758.

int init_Story_PzlKamon(void)
{
    return 1;
}

// The rules, then the board.
// FF3 0x00248760.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn, GetGPhaseCtrl, GPhaseCtrlSetNext (KamonMsgSet)

int one_Story_PzlKamon(void)
{
    KamonMsgSet(1, 2, KAMON_PHASE_SELECT, -1, -1);
    return 1;
}

// FF3 0x002487e8.

int end_Story_PzlKamon(void)
{
    return 1;
}

// FF3 0x002487f0.

void pre_Story_PzlKamon(void)
{
}

// FF3 0x002487f8.

void after_Story_PzlKamon(void)
{
    KamonDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 97 PzlKamon_Select
// ──────────────────────────────────────────────────────────────────────

#define KAMON_MOUSE_W   64.0f           // PC: a piece's sprite, from its slot's corner
#define KAMON_MOUSE_H   62.0f

// FF3 0x00248810.

int init_PzlKamon_Select(void)
{
    return 1;
}

// Left / right: the cursor. Circle, once a move was made: start over under
// a screen capture (phase 99). OK: move (98), or the buzzer without moves.
// Cancel: undo (98), or with nothing to undo ask to quit -- yes returns to
// the phase that opened the puzzle, no stays.
// FF3 0x00248818.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KamonMsgSet)

// PC: the mouse. Any piece joined to the hole can be pointed at, as the d-pad
// reaches them; a click on one moves it.
static void KamonMouse(void)
{
    static const int guide_in[3]   = { -1, REIPAN_MENU_CROSS, REIPAN_MENU_TRIANGLE };
    static const int guide_next[4] = { -1, REIPAN_MENU_CROSS, REIPAN_MENU_TRIANGLE, REIPAN_MENU_CIRCLE };
    const int hole   = kamon_board->GetHole();
    const int handle = BpCtlGetHandleByFileNo(PuzzleGetLayoutFile());

    ReiPan_MenuMouseBegin(REIPAN_MM_WHEEL_HORIZONTAL, nullptr);
    MenuMouseGuide(handle, "R caption_in", guide_in, 3);
    MenuMouseGuide(handle, "R caption_next", guide_next, 4);
    for (int s = 0; s < KAMON_SLOT_NUM; s++)
    {
        KAMON_LINK link(s, hole);

        if (s == hole || !kamon_board->HasLink(&link))
            continue;

        const KAMON_POS *pos = KamonGetSlotPos(s);

        if (ReiPan_MenuMouseClick(pos->x, pos->y, KAMON_MOUSE_W, KAMON_MOUSE_H))
        {
            kamon_board->cursor = s;
            ReiPan_MenuMousePress(REIPAN_MENU_CROSS);
        }
        else if (ReiPan_MenuMouseHover(pos->x, pos->y, KAMON_MOUSE_W, KAMON_MOUSE_H) && kamon_board->cursor != s)
        {
            kamon_board->cursor = s;
            SndSePlay(SE_CURSOR, 100, 0x1000, 0, nullptr, 0);
        }
    }
}

int one_PzlKamon_Select(void)
{
    KamonMouse();                       // PC
    if (CheckPadRptPlaySe(PAD_RIGHT_BTN, SE_CURSOR, 0) != 0)
    {
        kamon_board->CursorNext();
        return 0;
    }
    if (CheckPadRptPlaySe(PAD_LEFT_BTN, SE_CURSOR, 0) != 0)
    {
        kamon_board->CursorPrev();
        return 0;
    }

    if (kamon_board->rest_max - kamon_board->rest >= 1 && CheckPadTrgPlaySe(PAD_CIRCLE_BTN, -1, 0) != 0)
    {
        ScreenCaptureSave(2, 0);
        PuzzleSetCapAlpha(0x80);
        KamonBoardDelete();
        KamonBoardCreate();
        GetGPhaseCtrl()->SetNext(KAMON_PHASE_RETRY, 0);
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) != 0)
    {
        if (kamon_board->Move() == 0)
        {
            SndSePlay(SE_BUZZER, 100, 0x1000, 0, nullptr, 0);
            return 0;
        }
    }
    else
    {
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0) == 0)
            return 0;
        if (kamon_board->Undo() == 0)
        {
            SndSePlay(SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
            KamonMsgSet(2, 1, PuzzleGetRetPhase(), KAMON_PHASE_SELECT, KAMON_PHASE_SELECT);
            return 1;
        }
    }

    GetGPhaseCtrl()->SetNext(KAMON_PHASE_MOVE, 0);
    return 1;
}

// FF3 0x00248a30.

int end_PzlKamon_Select(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 98 PzlKamon_Move
// ──────────────────────────────────────────────────────────────────────

// The move, on the leaf's state_cnt: 0 the slide sound; 1 the slide, then
// solved -> 2, or a layout label ("out" out of moves, "next" after the first
// move, "back" when every move was undone) -> 4, else done; 2 the missing
// crest fades in, "out", -> 3; 3 the links light up -> 4; 4 done once the
// layout's animation ended.
// FF3 0x00248a38.

int init_PzlKamon_Move(void)
{
    int file = PuzzleGetLayoutFile();
    GPHASE_NODE *leaf;

    leaf = GetGPhaseCtrl()->leaf;
    if (leaf->state_cnt == 0)
    {
        SndSePlay(SE_KAMON_MOVE, 100, 0x1000, 0, nullptr, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    leaf = GetGPhaseCtrl()->leaf;
    if (leaf->state_cnt == 1)
    {
        if (kamon_board->IsMoving() != 0)
        {
            kamon_board->Step();
            return 0;
        }

        int rest_max  = kamon_board->rest_max;
        int rest      = kamon_board->rest;
        int rest_prev = kamon_board->rest_prev;

        if (kamon_board->IsSolved() != 0)
        {
            SndSePlay(SE_KAMON_SOLVED, 100, 0x1000, 0, nullptr, 0);
            KamonPieceFadeInStart();
            GetGPhaseCtrl()->leaf->state_cnt = 2;
            return 0;
        }

        const char *label;
        if (kamon_board->IsFailed() != 0)
            label = kamon_label_out;
        else if (rest_max - rest == 1 && rest_max == rest_prev)
            label = kamon_label_next;
        else if (rest_max == rest)
            label = kamon_label_back;
        else
            return 1;
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
    }
    else if (leaf->state_cnt == 2)
    {
        if (KamonPieceFadeInIsEnd() == 0)
            return 0;
        BpPlayLabel(BpCtlGetHandleByFileNo(file), kamon_label_out);
        KamonClearGlowStart();
        GetGPhaseCtrl()->leaf->state_cnt = 3;
        return 0;
    }
    else if (leaf->state_cnt == 3)
    {
        if (KamonClearGlowIsEnd() == 0)
            return 0;
    }
    else if (leaf->state_cnt == 4)
    {
        return BpCtlIsLoadEnd(file) != 0;
    }
    else
    {
        return 0;
    }

    GetGPhaseCtrl()->leaf->state_cnt = 4;
    return 0;
}

// Solved: the clear flag, the clear sound and the "solved" message back to
// the phase that opened the puzzle; out of moves: the "failed" message, the
// same way; else the next move.
// FF3 0x00248d40.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KamonMsgSet)

int one_PzlKamon_Move(void)
{
    if (kamon_board->IsSolved() != 0)
    {
        PuzzleClear();
        SndSePlay(SE_KAMON_CLEAR, 100, 0x1000, 0, nullptr, 0);
        KamonMsgSet(4, 2, PuzzleGetRetPhase(), -1, -1);
        return 1;
    }
    if (kamon_board->IsFailed() != 0)
    {
        KamonMsgSet(3, 2, PuzzleGetRetPhase(), -1, -1);
        return 1;
    }
    GetGPhaseCtrl()->SetNext(KAMON_PHASE_SELECT, 0);
    return 1;
}

// FF3 0x00248e98.

int end_PzlKamon_Move(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 99 PzlKamon_Retry
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00248ea0.

int init_PzlKamon_Retry(void)
{
    BpPlayLabel(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), kamon_label_in);
    return 1;
}

// The capture of the old board fades out, 0x10 a frame (after_Story_Puzzle
// draws it), over the new one playing "in".
// FF3 0x00248ed8.

int one_PzlKamon_Retry(void)
{
    PuzzleSetCapAlpha((short)(PuzzleGetCapAlpha() - 0x10));
    if (BpCtlIsLoadEnd(PuzzleGetLayoutFile()) == 0)
        return 0;
    if (PuzzleGetCapAlpha() != 0)
        return 0;
    GetGPhaseCtrl()->SetNext(KAMON_PHASE_SELECT, 0);
    return 1;
}

// FF3 0x00248f58.

int end_PzlKamon_Retry(void)
{
    return 1;
}

// FF3 0x00248f60 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.
