/* ==========================================================================
 *  ingame/puzzle/puzzle.c
 *
 *  The puzzle driver (see puzzle.h): the puzzle table, the clear flags, phase
 *  49 Story_Puzzle (capture the screen, load the puzzle, cross-fade into its
 *  phase, cross-fade back at the end) and phase 71 Story_PuzzleNoItem.
 *  FF3 0x243e00..0x2447b0, FF3's own code.
 * ======================================================================== */
#include "ingame/puzzle/puzzle.h"

#include <stddef.h>                             // offsetof
#include <string.h>                             // memset

#include "common/cmn_fade_layer.h"              // GetCmnFadeLayer, CmnFadeLayerIsReady / Update
#include "graphics/gs_reg.h"                    // ScreenCaptureSave / Disp
#include "ingame/ingame.h"                      // PHASE12_PARAM, Phase12ParamSet, Phase12FadeLayerOn, IngameDrawSub
#include "ingame/puzzle/kamon/kamon_pzl.h"      // KamonPzlOpen / Close / HasItem
#include "ingame/puzzle/kaza/kaza_pzl.h"        // KazaPzlOpen / Close / HasItem
#include "ingame/puzzle/kusi/kusi_pzl.h"        // KusiPzlOpen / Close / HasItem
#include "ingame/puzzle/rousoku/rousoku_pzl.h"  // RousokuPzlOpen / HasItem
#include "ingame/puzzle/sizumedama/sizumedama_pzl.h"   // SizumePzlOpen / HasItem
#include "ingame/sound/snd_ctrl.h"              // SndSeBankLoad / IsLoaded / Release
#include "main/gphase.h"                        // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "system/b2d/BpCtl.h"                   // BpCtl*, BpPlayLabel
#include "system/eeiop/cd_file_dat.h"           // PZL_*_PHF, PZLBG_*_PHF, PAZZLE_*_BD_BD
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/os/system.h"                   // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"                     // pad_wrk[0].stick_dpad

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// One puzzle type (data 0x361600, 5 x 0x18 on the EE).
// layout: host -- the three function pointers are 8 bytes here; the shorts
// before them keep their ROM offsets.
struct PUZZLE_DEF
{
    /* 0x00 */ short phase;                 // the puzzle's phase (one_Story_Puzzle's next)
    /* 0x02 */ short num;                   // boards (clear flags) of the type
    /* 0x04 */ short file_item;             // layout + language (+ idx * 5 for KAZA / KUSI)
    /* 0x06 */ short file_bg;               // layout without the item (+ idx for KAZA / KUSI)
    /* 0x08 */ short msg;                   // phase 71's message
    /* 0x0a */ short se_bank;               // SE bank file
    /* 0x0c */ void (*open)(void);          // one_Story_Puzzle, first frame, item held
    /* 0x10 */ void (*close)(void);         // end_Story_Puzzle, item held
    /* 0x14 */ int  (*has_item)(void);      // PuzzleHasItem
};
static_assert(offsetof(PUZZLE_DEF, se_bank) == 0x0a, "PUZZLE_DEF.se_bank");

static const PUZZLE_DEF puzzle_def[PUZZLE_TYPE_NUM] =    // data 0x361600
{
    { 73, 2, PZL_KAZA1_PHF,      PZLBG_KAZA1_PHF,    119, PAZZLE_STONE_BD_BD,   KazaPzlOpen,  KazaPzlClose,  KazaPzlHasItem },
    { 74, 1, PZL_KAMON_PHF,      PZLBG_KAMON_PHF,    118, PAZZLE_STONE_BD_BD,   KamonPzlOpen, KamonPzlClose, KamonPzlHasItem },
    { 72, 3, PZL_KUSI_YLW_PHF,   PZLBG_KUSI_YLW_PHF, 117, PAZZLE_STONE_BD_BD,   KusiPzlOpen,  KusiPzlClose,  KusiPzlHasItem },
    { 75, 1, PZL_SIZUMEDAMA_PHF, PZLBG_KAZA1_PHF,    120, PAZZLE_SHIZUME_BD_BD, SizumePzlOpen, nullptr,     SizumePzlHasItem },
    { 76, 4, PZL_ROUSOKU_PHF,    PZLBG_KAZA1_PHF,    116, PAZZLE_ROUSOKU_BD_BD, RousokuPzlOpen, nullptr,    RousokuPzlHasItem },
};

// The clear flags, one word per board (bss 0x3f7a20; saved as is).
struct PUZZLE_CLEAR_INFO                    /* 0x2c */
{
    /* 0x00 */ int kaza[2];
    /* 0x08 */ int kamon[1];
    /* 0x0c */ int kusi[3];
    /* 0x18 */ int sizumedama[1];
    /* 0x1c */ int rousoku[4];
};
static_assert(sizeof(PUZZLE_CLEAR_INFO) == 0x2c, "PUZZLE_CLEAR_INFO is 0x2c bytes");
static_assert(offsetof(PUZZLE_CLEAR_INFO, kusi) == 0x0c, "PUZZLE_CLEAR_INFO.kusi");
static_assert(offsetof(PUZZLE_CLEAR_INFO, rousoku) == 0x1c, "PUZZLE_CLEAR_INFO.rousoku");

static PUZZLE_CLEAR_INFO puzzle_clear;      // bss 0x3f7a20

static int   puzzle_type = PUZZLE_NONE;     // sdata 0x371e30
static int   puzzle_idx;                    // sdata 0x371e34
static const char puzzle_label_in[]     = "in";         // sdata 0x371e38
static const char puzzle_label_layout[] = "layout";     // sdata 0x371e40
static short puzzle_ret_phase;              // sbss 0x373854
static short puzzle_cap_alpha;              // sbss 0x373856

// The phase-71 fallback when the puzzle's item is not held.
#define PUZZLE_NO_ITEM_PHASE    71

// ──────────────────────────────────────────────────────────────────────
// The puzzle table
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00243e00.

static void PuzzleCallOpen(void)
{
    if (puzzle_def[PuzzleGetType()].open != nullptr)
        puzzle_def[PuzzleGetType()].open();
}

// FF3 0x00243e68.

static void PuzzleCallClose(void)
{
    if (puzzle_def[PuzzleGetType()].close != nullptr)
        puzzle_def[PuzzleGetType()].close();
}

// FF3 0x00243ed0.

int PuzzleHasItem(void)
{
    if (puzzle_def[PuzzleGetType()].has_item == nullptr)
        return 0;
    return puzzle_def[PuzzleGetType()].has_item();
}

// ──────────────────────────────────────────────────────────────────────
// Clear flags
// ──────────────────────────────────────────────────────────────────────

// Save-data entry (save_data.c).
// FF3 0x00243f40.

int PuzzleGetSaveWrk(void **addr)
{
    *addr = &puzzle_clear;
    return sizeof(PUZZLE_CLEAR_INFO);
}

// FF3 0x00243f58.

void PuzzleClearInfoReset(void)
{
    memset(&puzzle_clear, 0, sizeof(PUZZLE_CLEAR_INFO));
}

// FF3 0x00243f80.

short PuzzleGetRetPhase(void)
{
    return puzzle_ret_phase;
}

// FF3 0x00243f88.

int GetPuzzleClearInfo(int type, int idx)
{
    switch ((u_int)type)
    {
    case PUZZLE_KAZA:       return puzzle_clear.kaza[idx];
    case PUZZLE_KAMON:      return puzzle_clear.kamon[idx];
    case PUZZLE_KUSI:       return puzzle_clear.kusi[idx];
    case PUZZLE_SIZUMEDAMA: return puzzle_clear.sizumedama[idx];
    case PUZZLE_ROUSOKU:    return puzzle_clear.rousoku[idx];
    }
    return 0;
}

// FF3 0x00244030.

void PuzzleClear(void)
{
    u_int type = (u_int)PuzzleGetType();
    int   idx  = PuzzleGetIdx();

    switch (type)
    {
    case PUZZLE_KAZA:       puzzle_clear.kaza[idx] = 1;       break;
    case PUZZLE_KAMON:      puzzle_clear.kamon[idx] = 1;      break;
    case PUZZLE_KUSI:       puzzle_clear.kusi[idx] = 1;       break;
    case PUZZLE_SIZUMEDAMA: puzzle_clear.sizumedama[idx] = 1; break;
    case PUZZLE_ROUSOKU:    puzzle_clear.rousoku[idx] = 1;    break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Current puzzle
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00244108.

int PuzzleGetType(void)
{
    return puzzle_type;
}

// FF3 0x00244110.

int PuzzleGetIdx(void)
{
    return puzzle_idx;
}

// The puzzle's layout: with the item the language's PZL_* file (KAZA and
// KUSI: five languages per board), without it the PZLBG_* background (one
// per board).
// FF3 0x00244120.

int PuzzleGetLayoutFile(void)
{
    int type = PuzzleGetType();
    int idx  = PuzzleGetIdx();

    if (PuzzleHasItem() == 0)
    {
        if (type != PUZZLE_KAZA && type != PUZZLE_KUSI)
            return puzzle_def[type].file_bg;
        return puzzle_def[type].file_bg + idx;
    }

    int file = puzzle_def[type].file_item + GetSysWrk()->GetLanguage();
    if (type != PUZZLE_KAZA && type != PUZZLE_KUSI)
        return file;
    return file + idx * 5;
}

// FF3 0x002441e8.

void PuzzleSet(int type, int idx)
{
    puzzle_type = type;
    puzzle_idx = idx;
}

// FF3 0x002441f8.

void PuzzleSetCapAlpha(short alpha)
{
    puzzle_cap_alpha = 0;
    if (alpha >= 0)
        puzzle_cap_alpha = alpha > 0x80 ? 0x80 : alpha;
}

// FF3 0x00244220.

short PuzzleGetCapAlpha(void)
{
    return puzzle_cap_alpha;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 49 Story_Puzzle
// ──────────────────────────────────────────────────────────────────────

// First call: capture the screen (shown at full alpha by after_), stick as
// d-pad on, load the layout and the SE bank. Done once the layout is loaded
// and the bank too (a bank file that is missing or empty does not count).
// FF3 0x00244228.

int init_Story_Puzzle(void)
{
    int file = PuzzleGetLayoutFile();
    int bank = puzzle_def[PuzzleGetType()].se_bank;

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        PuzzleSetCapAlpha(0x80);
        pad_wrk[0].stick_dpad = 1;
        BpCtlLoadReq(file, nullptr);
        SndSeBankLoad(bank);
        ScreenCaptureSave(2, 1);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (BpCtlIsDataReady(BpCtlGetHandleByFileNo(file)) == 0)
        return 0;
    if (GetFileSize(bank) == (u_int)-1)
        return 1;
    if (GetFileSize(bank) == 0)
        return 1;
    return SndSeBankIsLoaded(bank) != 0;
}

// First frame: play "in" (item held, and open the puzzle) or "layout" (the
// background). Then fade the capture out, 0x19 a frame; at 0 go to the
// puzzle's phase or to 71, remembering the phase to return to.
// FF3 0x002444a8.

int one_Story_Puzzle(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        const char *label = PuzzleHasItem() == 0 ? puzzle_label_layout : puzzle_label_in;

        BpPlayLabel(BpCtlGetHandleByFileNo(PuzzleGetLayoutFile()), label);
        if (PuzzleHasItem() != 0)
            PuzzleCallOpen();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    PuzzleSetCapAlpha((short)(PuzzleGetCapAlpha() - 0x19));
    if (PuzzleGetCapAlpha() > 0)
        return 0;

    puzzle_ret_phase = GetGPhaseCtrl()->from;

    int next;
    if (PuzzleHasItem() == 0)
        next = PUZZLE_NO_ITEM_PHASE;
    else
        next = puzzle_def[PuzzleGetType()].phase;
    GetGPhaseCtrl()->SetNext(next, 0);
    return 1;
}

// FF3 0x002b90f8: an out-of-line clamp (lo when below, hi when above),
// shared by several files; written here until its home is known.
static int PuzzleClamp(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

// Capture the puzzle's last frame, then draw the game under it while the
// capture fades out, 0x10 a frame (the alpha is kept in the node's
// state_wrk). Once gone: close the puzzle, free layout and SE bank, stick
// as d-pad off, no puzzle.
// FF3 0x00244348.
// callcheck: FUN_002b90f8 (PuzzleClamp)

int end_Story_Puzzle(void)
{
    if ((short)GetGPhaseCtrl()->leaf->state_wrk == 0)
        ScreenCaptureSave(2, 1);
    IngameDrawSub(0);

    int alpha = PuzzleClamp((short)GetGPhaseCtrl()->leaf->state_wrk + 0x10, 0, 0x80);
    ScreenCaptureDisp(0x80, 0x80, 0x80, (u_char)(0x80 - alpha));
    GetGPhaseCtrl()->leaf->state_wrk = (short)alpha;
    PuzzleSetCapAlpha(0);
    if (alpha < 0x80)
        return 0;

    int file = PuzzleGetLayoutFile();
    int bank = puzzle_def[PuzzleGetType()].se_bank;

    if (PuzzleHasItem() != 0)
        PuzzleCallClose();
    BpCtlRelease(BpCtlGetHandleByFileNo(file));
    SndSeBankRelease(bank, 0);
    pad_wrk[0].stick_dpad = 0;
    puzzle_type = PUZZLE_NONE;
    puzzle_idx = 0;
    return 1;
}

// FF3 0x00244648.

void pre_Story_Puzzle(void)
{
}

// Layouts, the common fade layer, and the capture while it shows.
// FF3 0x00244650.

void after_Story_Puzzle(void)
{
    BpCtlMain();

    capCtl *layer = GetCmnFadeLayer();
    if (CmnFadeLayerIsReady())
        layer->Update();

    if (PuzzleGetCapAlpha() > 0)
        ScreenCaptureDisp(0x80, 0x80, 0x80, (u_char)PuzzleGetCapAlpha());
}

// ──────────────────────────────────────────────────────────────────────
// Phase 71 Story_PuzzleNoItem
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002446c8.

int init_Story_PuzzleNoItem(void)
{
    return 1;
}

// FF3 0x002446d0.

int end_Story_PuzzleNoItem(void)
{
    return 1;
}

// The puzzle's "item missing" message as phase 12 (message pages), which
// returns to the phase that requested the puzzle.
// FF3 0x002446d8.

int one_Story_PuzzleNoItem(void)
{
    PHASE12_PARAM param;

    param.msg0        = puzzle_def[PuzzleGetType()].msg;
    param.msg1        = 0;
    param.type        = 2;
    param.next_ok     = PuzzleGetRetPhase();
    param.next_alt    = -1;
    param.next_cancel = -1;
    param._0e         = 0;      // PC: FF3 leaves this stack half unset (unread)
    param.end_func    = nullptr;
    param.win         = nullptr;
    Phase12ParamSet(&param);
    Phase12FadeLayerOn();
    GetGPhaseCtrl()->SetNext(12, 1);
    return 1;
}

// FF3 0x00244790 is compiler output: the atexit thunk of the GPhaseCtrl
// singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static gphase.c owns here.
