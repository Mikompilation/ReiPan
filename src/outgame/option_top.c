/* ==========================================================================
 *  outgame/option_top.c
 *
 *  Phase 19, the option top menu. FF3 0x223e60..0x224618, FF3-only (FF2's
 *  option screen was one phase). Phase 7 Option_Main (option.c) loads the
 *  option layout OPTION_TOP_PHF (+ language), opens the selection window and
 *  starts this menu's draw hook; one_Option_Main then links phase 19 under
 *  it. The six entries (message 0x65 rows 0..5) lead to the option screens
 *  20 / 22 / 21 / 23, the reset-to-defaults prompt and back to the opener;
 *  the window shows the help line of the entry under the cursor (rows
 *  6..11). Cancel leaves at once when nothing changed since phase 7 began,
 *  else asks whether to discard the changes.
 *
 *  FF3 reads the singletons through gp-relative cached pointers; nothing
 *  depends on the cache, so calls go to the getters directly. FF3 0x002245f8
 *  (OptionTopAtexitGPhaseCtrl) is not written: the compiler's atexit thunk
 *  for the GetGPhaseCtrl static (GPhaseCtrlTerm), ending the file.
 * ======================================================================== */
#include "outgame/option_top.h"

#include "common/WindowMsg.h"           // CWindowMsg
#include "graphics/graph2d/message.h"   // SetCurColorRGB
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetDefaultGsDrawState
#include "ingame/ingame.h"              // PHASE12_PARAM, Phase12ParamSet
#include "ingame/menu/menu_csr.h"       // MenuCsrUpdate, MenuCsrGetPos, MenuCsrIsMovedSe
#include "main/gphase.h"
#include "outgame/option.h"             // OPTION_WRK, option_*, LayoutItemAddPos, PrintMsgPos, CmnFadeLayer*Item
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"          // B2dSprSetPos, B2dSprFreezePart
#include "system/b2d/BpCtl.h"
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe

#define OPTION_TOP_SE_MOVE      0x1a9b0000
#define OPTION_TOP_SE_CANCEL    0x1a9b0001
#define OPTION_TOP_SE_OK        0x1a9b0003

#define OPTION_TOP_ITEM_WIN     0x3f4   // the window frame item (fade layer and CWindowMsg)
#define OPTION_TOP_ITEM_CSR     0x402   // the cursor bar
#define OPTION_TOP_ITEM_TEXT    0x406   // the entries' text, drawn by OptionTopItemDrawCallback

#define OPTION_TOP_MSG_RESET    0xc     // "restore the defaults?"
#define OPTION_TOP_MSG_DISCARD  0xd     // "discard the changes?"

static int OptionTopLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_LAYOUT_FILE;
}

// The help line of the entry under the cursor. FF3 0x00223e60.
static int OptionTopGetMsgNo(void)
{
    return MenuCsrGetPos(OPTION_TOP_CSR) + OPTION_TOP_ROWS;
}

// Phase 12's end of the reset prompt. FF3 0x00223e80.
static void OptionTopResetEnd(int result)
{
    if (result == 1)
        OptionInit(GetOptionWrk());
}

// Phase 12's end of the discard prompt: back to the settings phase 7 began
// with. FF3 0x00223ec8.
static void OptionTopDiscardEnd(int result)
{
    if (result == 1)
        OptionLoadSaved(GetOptionWrk(), 1);
}

// Moves the cursor bar to row 'pos'. FF3 0x00223f10.
// callcheck: GetLanguage, GetSysWrk (OptionTopLayoutFile)
static void OptionTopCsrSetPos(int pos)
{
    int handle = BpCtlGetHandleByFileNo(OptionTopLayoutFile());

    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_TOP_ITEM_CSR), 0, pos * OPTION_TOP_ROW_DY - 2);
    B2dSprFreezePart(handle, OPTION_TOP_ITEM_CSR, 7, 0);
}

// Item 0x406: the six entry names, one row each, the row under the cursor
// lit. Words: a0 the item's label, t1 its rectangle, stack[2] alpha.
// FF3 0x00223f98 (no Ghidra function before 2026-10-21: inside 0x223f10).
// callcheck: GetLanguage, GetSysWrk (OptionTopLayoutFile)
static int OptionTopItemDrawCallback(const B2D_HOOK_ARGS *args)
{
    const char *label = (const char *)args->a0;
    int        *rect  = (int *)args->t1;
    int         alpha = (int)args->stack[2];
    int         cur   = MenuCsrGetPos(OPTION_TOP_CSR);
    int         i;

    for (i = 0; i < OPTION_TOP_ROWS; i++)
    {
        float pos[2];
        int   type;

        pos[0] = 0.0f;
        pos[1] = (float)(i * OPTION_TOP_ROW_DY);
        if (i == cur)
            SetCurColorRGB(0xd6, 0xe3, 0xcc);
        else
            SetCurColorRGB(0x9d, 0xaf, 0x8f);
        type = GetLabelSuffixType(label);
        LayoutItemAddPos(pos, label, OptionTopLayoutFile(), rect);
        PrintMsgPos(OPTION_TOP_MSG, i, pos, 0x2d, alpha, 0, type);
    }

    SetDefaultGsDrawState();
    return 0;
}

// FF3 0x002240c0 (the record list is built on the stack).
// callcheck: GetLanguage, GetSysWrk (OptionTopLayoutFile)
void OptionTopAnimStart(void)
{
    const B2D_HOOK_REC hooks[] =
    {
        { OptionTopItemDrawCallback, 0, OPTION_TOP_ITEM_TEXT, nullptr },
        { nullptr,                  -1, -1,                   nullptr },
    };

    B2dSetHookList(BpCtlGetHandleByFileNo(OptionTopLayoutFile()), hooks);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 19
// ──────────────────────────────────────────────────────────────────────

// Plays "outgame_in" (first entry from the title menu), "in" (first entry
// from elsewhere) or "back" (back from a screen or prompt), puts the window
// on item 0x3f4 with the help line, then waits for the label to end.
// callcheck: GetLanguage, GetSysWrk (OptionTopLayoutFile)
// FF3 0x00224150.
int init_OptionTop(void)
{
    int file   = OptionTopLayoutFile();
    int handle = BpCtlGetHandleByFileNo(file);

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        const char *label = "back";

        if (GetGPhaseCtrl()->from == 7)
            label = option_from_title != 0 ? "outgame_in" : "in";
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        CmnFadeLayerShowItem(0, handle, OPTION_TOP_ITEM_WIN);
        option_sel_win->SetItem(handle, OPTION_TOP_ITEM_WIN);
        option_sel_win->StartMsg(OPTION_TOP_MSG, OptionTopGetMsgNo(), 1);
        B2dSprFreezePart(handle, OPTION_TOP_ITEM_CSR, 7, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    return BpCtlIsLoadEnd(file);
}

// FF3 0x002242b0.
int one_OptionTop(void)
{
    PHASE12_PARAM param;
    int           pos;
    int           next;

    MenuCsrUpdate(OPTION_TOP_CSR, 0);
    pos = MenuCsrGetPos(OPTION_TOP_CSR);
    option_sel_win->StartMsg(OPTION_TOP_MSG, OptionTopGetMsgNo(), 1);
    OptionTopCsrSetPos(pos);

    if (MenuCsrIsMovedSe(OPTION_TOP_CSR, OPTION_TOP_SE_MOVE))
        return 0;

    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], OPTION_TOP_SE_OK, 0))
    {
        // The entries' phases: 20, 22, 21, 23, the reset prompt, back.
        short to[OPTION_TOP_ROWS] = { 0x14, 0x16, 0x15, 0x17, 0xc, (short)option_from_phase };

        next = to[pos];
        if (next != 0xc)
        {
            GetGPhaseCtrl()->SetNext(next, 0);
            return 1;
        }
        param.msg0        = OPTION_TOP_MSG;
        param.msg1        = OPTION_TOP_MSG_RESET;
        param.type        = 1;
        param.next_ok     = 0x13;
        param.next_alt    = 0x13;
        param.next_cancel = 0x13;
        param._0e         = 0;                      // FF3 leaves it unset
        param.end_func    = OptionTopResetEnd;
        param.win         = option_sel_win;
    }
    else
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], OPTION_TOP_SE_CANCEL, 0))
            return 0;
        if (OptionIsSameAsSaved(GetOptionWrk(), 1))
        {
            GetGPhaseCtrl()->SetNext(option_from_phase, 0);
            return 1;
        }
        param.msg0        = OPTION_TOP_MSG;
        param.msg1        = OPTION_TOP_MSG_DISCARD;
        param.type        = 1;
        param.next_ok     = (short)option_from_phase;
        param.next_alt    = 0x13;
        param.next_cancel = 0x13;
        param._0e         = 0;                      // FF3 leaves it unset
        param.end_func    = OptionTopDiscardEnd;
        param.win         = option_sel_win;
    }

    Phase12ParamSet(&param);
    GetGPhaseCtrl()->SetNext(0xc, 1);
    return 1;
}

// Plays "out" when leaving to the opener, "next" otherwise; when it ends,
// takes the window off the fade layer item.
// callcheck: GetLanguage, GetSysWrk (OptionTopLayoutFile)
// FF3 0x002244d0.
int end_OptionTop(void)
{
    int file = OptionTopLayoutFile();

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        const char *label = GetGPhaseCtrl()->now == option_from_phase ? "out" : "next";

        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (!BpCtlIsLoadEnd(file))
        return 0;
    CmnFadeLayerTermItem();
    option_sel_win->SetAlphaItem(nullptr);
    return 1;
}
