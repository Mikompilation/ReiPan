/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_pzl.c
 *
 *  The kusi puzzle's hooks and phases 72 / 91 / 92 / 93 / 94 (see
 *  kusi_pzl.h). FF3 0x245d38..0x246958, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/kusi/kusi_pzl.h"

#include "graphics/gs_reg.h"                    // ScreenCaptureSave
#include "ingame/ingame.h"                      // PHASE12_PARAM, Phase12ParamSet, Phase12FadeLayerOn
#include "ingame/item/prg/item.h"               // ItemGetCnt
#include "ingame/menu/menu_csr.h"               // MenuCsr*
#include "ingame/puzzle/kusi/kusi_draw.h"       // KusiGetBoardDef, KusiGetCellPos, KusiDrawInit, KusiDraw
#include "ingame/puzzle/puzzle.h"               // PuzzleGetIdx / GetLayoutFile / GetRetPhase / Clear, cap alpha
#include "ingame/sound/snd_ctrl.h"              // SndSePlay
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "system/b2d/BpCtl.h"                   // BpCtlGetHandleByFileNo, BpCtlIsLoadEnd, BpPlayLabel
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe, pad_key

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

KUSI_BOARD *kusi_board;                         // sdata 0x371eb0

static const short kusi_item[3] = { 43, 44, 45 };   // sdata 0x371eb8: the item each board needs
#define KUSI_ITEM_NUM       0x45        // ItemGetCnt's range

static const char kusi_label_out[] = "out";     // sdata 0x371ec0
static const char kusi_label_in[]  = "in";      // sdata 0x371ec8

static int kusi_move_cell;              // sbss 0x373870: the cell phase 92 slides to
static int kusi_move_fwd;               // sbss 0x373874: 1 a move, 0 an undo (phase 93 commits it)
static int kusi_cap_on;                 // sbss 0x373878: KusiCapIsOn

#define KUSI_MSG            0x75        // phase 12's message group; msg1 below

#define KUSI_PHASE_SELECT   91
#define KUSI_PHASE_MOVE     92
#define KUSI_PHASE_SLIDE    93
#define KUSI_PHASE_RETRY    94
#define KUSI_PHASE_MSG      12

#define SE_CURSOR           0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_BUZZER           0x1a9b0002
#define SE_OK               0x1a9b0003
#define SE_KUSI_MOVE        0x1bcb0001  // the puzzle's own bank (PAZZLE_STONE_BD_BD)
#define SE_KUSI_CLEAR       0x1bcb0002

#define PAD_UP_BTN          0x1000
#define PAD_RIGHT_BTN       0x2000
#define PAD_DOWN_BTN        0x4000
#define PAD_LEFT_BTN        0x8000
#define PAD_CIRCLE_BTN      0x20

// A phase-12 message (puzzle.c's one_Story_PuzzleNoItem fills it the same way).
// PC: a shared helper; FF3 fills the stack param inline in each of its three
// callers, in this order (the quit prompt leaves _0e unset).
static void KusiMsgSet(short msg1, int type, short next_ok, short next_alt, short next_cancel)
{
    PHASE12_PARAM param;

    param.msg0        = KUSI_MSG;
    param.msg1        = msg1;
    param.type        = type;
    param.next_ok     = next_ok;
    param.next_alt    = next_alt;
    param.next_cancel = next_cancel;
    param._0e         = 0;
    param.end_func    = nullptr;
    param.win         = nullptr;
    Phase12ParamSet(&param);
    Phase12FadeLayerOn();
    GetGPhaseCtrl()->SetNext(KUSI_PHASE_MSG, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The board and the puzzle table's hooks
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00245d38.
// callcheck: operator_new, KusiBoardCtor (new KUSI_BOARD)

void KusiBoardCreate(void)
{
    const KUSI_BOARD_DEF *def = KusiGetBoardDef(PuzzleGetIdx());
    kusi_board = new KUSI_BOARD(def);
}

// FF3 0x00245d88.
// callcheck: KusiBoardDtor, operator_delete (delete kusi_board)

void KusiBoardDelete(void)
{
    if (kusi_board != nullptr)
        delete kusi_board;
}

// The cursor skips the cells the board does not have.
// FF3 0x00245dd0.
// callcheck: KusiBoardGetOpen

static void KusiCsrFilter(short *tbl, short w, short h)
{
    for (int i = 0; i < w; i++)
    {
        for (int j = 0; j < h; j++)
        {
            if (*kusi_board->GetOpen(j + i * h) == 0)
                *tbl = -1;
            else
                *tbl = (short)(j + i * w);
            tbl++;
        }
    }
}

// The cursor follows the selected piece.
// FF3 0x00245eb8.

static void KusiCsrSync(void)
{
    if (kusi_board->selected != nullptr)
        MenuCsrSetPos(KUSI_CSR, (short)kusi_board->selected->cell);
}

// Start over: the board as it is now stays on screen as a capture that
// phase 94 fades out over a fresh board.
// FF3 0x00245ee8.

static void KusiRetryStart(void)
{
    ScreenCaptureSave(2, 0);
    PuzzleSetCapAlpha(0x80);
    KusiBoardDelete();
    KusiBoardCreate();
}

// FF3 0x00245f18.

void KusiPzlOpen(void)
{
    KusiBoardCreate();
    kusi_cap_on = 0;
    MenuCsrInitWithFilter(KusiCsrFilter, KUSI_CSR, KUSI_CELL_W, KUSI_CELL_W, 0);
    KusiDrawInit();
    KusiDraw();
}

// FF3 0x00245f60.

void KusiPzlClose(void)
{
    KusiBoardDelete();
    MenuCsrRelease(KUSI_CSR);
}

// FF3 0x00245f80.

int KusiPzlHasItem(void)
{
    u_short item = (u_short)kusi_item[PuzzleGetIdx()];

    if (item >= KUSI_ITEM_NUM)
        return 0;
    return *ItemGetCnt((short)item) > 0;
}

// The cell next to the cursor in a direction; only the 0..8 range is
// checked (the board's links rule out wrapping to the next row).
// FF3 0x00245ff8.

int KusiCsrGetCell(int dir)
{
    int pos = MenuCsrGetPos(KUSI_CSR);
    u_int cell;

    switch (dir)
    {
        case KUSI_DIR_RIGHT: cell = pos + 1;           break;
        case KUSI_DIR_UP:    cell = pos - KUSI_CELL_W; break;
        case KUSI_DIR_DOWN:  cell = pos + KUSI_CELL_W; break;
        case KUSI_DIR_LEFT:  cell = pos - 1;           break;
        default:             return -1;
    }
    return cell < KUSI_CELL_NUM ? (int)cell : -1;
}

// The cell the pad's direction points at this frame, -1 for none.
// FF3 0x00246080.

static int KusiPadGetCell(void)
{
    int dir;

    if (CheckPadTrgPlaySe(PAD_RIGHT_BTN, -1, 0) != 0)
        dir = KUSI_DIR_RIGHT;
    else if (CheckPadTrgPlaySe(PAD_LEFT_BTN, -1, 0) != 0)
        dir = KUSI_DIR_LEFT;
    else if (CheckPadTrgPlaySe(PAD_UP_BTN, -1, 0) != 0)
        dir = KUSI_DIR_UP;
    else if (CheckPadTrgPlaySe(PAD_DOWN_BTN, -1, 0) != 0)
        dir = KUSI_DIR_DOWN;
    else
        return -1;
    return KusiCsrGetCell(dir);
}

// FF3 0x00246930.

int KusiCapIsOn(void)
{
    return kusi_cap_on;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 72 Story_PzlKusi
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00246108.

int init_Story_PzlKusi(void)
{
    return 1;
}

// The rules, then the board.
// FF3 0x00246110.
// callcheck: Phase12ParamSet, Phase12FadeLayerOn, GetGPhaseCtrl, GPhaseCtrlSetNext (KusiMsgSet)

int one_Story_PzlKusi(void)
{
    KusiMsgSet(1, 2, KUSI_PHASE_SELECT, -1, -1);
    return 1;
}

// FF3 0x00246198.

int end_Story_PzlKusi(void)
{
    return 1;
}

// FF3 0x002461a0.

void pre_Story_PzlKusi(void)
{
}

// FF3 0x002461a8.

void after_Story_PzlKusi(void)
{
    KusiDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 91 PzlKusi_Select
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002461c0.

int init_PzlKusi_Select(void)
{
    kusi_board->move_cnt = 0;
    kusi_board->selected = nullptr;
    kusi_cap_on = 1;
    return 1;
}

// The cursor over the cells. Circle, once a move was made: start over
// (94). OK: pick the piece under the cursor if it has moves left (92), else
// the buzzer. Cancel: take back the last move, whichever piece made it
// (93), or with nothing to take back ask to quit -- yes returns to the
// phase that opened the puzzle, no stays.
// FF3 0x002461e0.
// callcheck: KusiBoardGetRest, KusiBoardCanSelect, KusiBoardSelect, KusiBoardHasHistory, KusiBoardGetPrevCell, KusiBoardSlideTo
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KusiMsgSet)

int one_PzlKusi_Select(void)
{
    KusiDrawMouseSelect(KUSI_CSR);      // PC
    MenuCsrUpdate(KUSI_CSR, 0);
    short pos = MenuCsrGetPos(KUSI_CSR);
    if (MenuCsrIsMovedSe(KUSI_CSR, SE_CURSOR) != 0)
        return 0;

    if (kusi_board->GetRest() != kusi_board->rest_max && CheckPadTrgPlaySe(PAD_CIRCLE_BTN, -1, 0) != 0)
    {
        KusiRetryStart();
        GetGPhaseCtrl()->SetNext(KUSI_PHASE_RETRY, 0);
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], -1, 0) != 0)
    {
        if (kusi_board->CanSelect(pos) == 0)
        {
            SndSePlay(SE_BUZZER, 100, 0x1000, 0, nullptr, 0);
            return 0;
        }
        kusi_board->Select(pos);
        SndSePlay(SE_OK, 100, 0x1000, 0, nullptr, 0);
        GetGPhaseCtrl()->SetNext(KUSI_PHASE_MOVE, 0);
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0) == 0)
        return 0;

    if (kusi_board->HasHistory() == 0)
    {
        SndSePlay(SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
        KusiMsgSet(2, 1, PuzzleGetRetPhase(), KUSI_PHASE_SELECT, KUSI_PHASE_SELECT);
        return 1;
    }

    kusi_board->SlideTo(KusiGetCellPos(kusi_board->GetPrevCell()));
    kusi_move_fwd = 0;
    GetGPhaseCtrl()->SetNext(KUSI_PHASE_SLIDE, 0);
    return 1;
}

// FF3 0x00246440.

int end_PzlKusi_Select(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 92 PzlKusi_Move
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00246448.

int init_PzlKusi_Move(void)
{
    return 1;
}

// A direction the picked piece can go: slide there (93). Circle, once a
// move was made: start over (94). OK: the piece's turn ends (91). Cancel:
// step it back one cell (93) if it moved this turn, else back to 91.
// FF3 0x00246450.
// callcheck: KusiBoardCanMove, KusiBoardSlideTo, KusiBoardGetRest, KusiBoardGetPrevCell

int one_PzlKusi_Move(void)
{
    KusiDrawMouseMove();                // PC
    int cell = KusiPadGetCell();

    kusi_move_cell = cell;
    if (kusi_board->CanMove(cell) != 0)
    {
        kusi_board->SlideTo(KusiGetCellPos(cell));
        kusi_move_fwd = 1;
        GetGPhaseCtrl()->SetNext(KUSI_PHASE_SLIDE, 0);
        return 1;
    }

    if (kusi_board->GetRest() != kusi_board->rest_max && CheckPadTrgPlaySe(PAD_CIRCLE_BTN, -1, 0) != 0)
    {
        KusiRetryStart();
        GetGPhaseCtrl()->SetNext(KUSI_PHASE_RETRY, 0);
        return 1;
    }

    if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], SE_OK, 0) == 0)
    {
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], -1, 0) == 0)
            return 0;
        if (kusi_board->move_cnt != 0)
        {
            kusi_board->SlideTo(KusiGetCellPos(kusi_board->GetPrevCell()));
            kusi_move_fwd = 0;
            GetGPhaseCtrl()->SetNext(KUSI_PHASE_SLIDE, 0);
            return 1;
        }
        SndSePlay(SE_CANCEL, 100, 0x1000, 0, nullptr, 0);
    }
    GetGPhaseCtrl()->SetNext(KUSI_PHASE_SELECT, 0);
    return 1;
}

// FF3 0x002465e8.

int end_PzlKusi_Move(void)
{
    KusiCsrSync();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 93 PzlKusi_Slide
// ──────────────────────────────────────────────────────────────────────

// The slide (its sound on the first frame), then the move or the undo is
// made on the board.
// FF3 0x00246608.
// callcheck: KusiBoardStep, KusiBoardIsMoving, KusiBoardUndo, KusiBoardMove

int init_PzlKusi_Slide(void)
{
    kusi_board->Step();
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        SndSePlay(SE_KUSI_MOVE, 100, 0x1000, 0, nullptr, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (kusi_board->IsMoving() != 0)
        return 0;

    if (kusi_move_fwd == 0)
        kusi_board->Undo();
    else
        kusi_board->Move(kusi_move_cell);
    KusiCsrSync();
    return 1;
}

// Not solved: back to the phase that asked for the slide (91 or 92).
// Solved: the layout plays "out", then the clear flag, the clear sound and
// the "solved" message back to the phase that opened the puzzle.
// FF3 0x002466d8.
// callcheck: KusiBoardIsSolved, KusiBoardIsFailed
// callcheck: Phase12ParamSet, Phase12FadeLayerOn (KusiMsgSet)

int one_PzlKusi_Slide(void)
{
    if (kusi_board->IsSolved() == 0)
    {
        kusi_board->IsFailed();         // the result is dropped (it is never true anyway)
        GPHASE_CTRL *ctrl = GetGPhaseCtrl();
        ctrl->SetNext(ctrl->from, 0);
        return 1;
    }

    int file = PuzzleGetLayoutFile();
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), kusi_label_out);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (BpCtlIsLoadEnd(file) == 0)
        return 0;

    PuzzleClear();
    SndSePlay(SE_KUSI_CLEAR, 100, 0x1000, 0, nullptr, 0);
    KusiMsgSet(3, 2, PuzzleGetRetPhase(), -1, -1);
    return 1;
}

// FF3 0x00246868.

int end_PzlKusi_Slide(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 94 PzlKusi_Retry
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00246870.

int init_PzlKusi_Retry(void)
{
    BpPlayLabel(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), kusi_label_in);
    return 1;
}

// The capture of the old board fades out, 0x10 a frame (after_Story_Puzzle
// draws it), over the new one playing "in".
// FF3 0x002468a8.

int one_PzlKusi_Retry(void)
{
    PuzzleSetCapAlpha((short)(PuzzleGetCapAlpha() - 0x10));
    if (BpCtlIsLoadEnd(PuzzleGetLayoutFile()) == 0)
        return 0;
    if (PuzzleGetCapAlpha() != 0)
        return 0;
    GetGPhaseCtrl()->SetNext(KUSI_PHASE_SELECT, 0);
    return 1;
}

// FF3 0x00246928.

int end_PzlKusi_Retry(void)
{
    return 1;
}

// FF3 0x00246938 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here;
// it marks the end of the file.
