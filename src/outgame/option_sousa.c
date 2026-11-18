/* ==========================================================================
 *  outgame/option_sousa.c
 *
 *  Phase 22, the option "Game Play" screen (sousa: the controls). FF3
 *  0x222640..0x222d50, FF3-only. Four two-way rows on OPTION_SOUSA_PHF
 *  (+ language), each an OPTION_DAT byte that is 0 or 1:
 *    0 b11 (d-pad mashing), 1 b12 (stick / d-pad pressure),
 *    2 b09 (look stick y inverted), 3 b13 (sticks swapped).
 *  Left / right flips the row under the cursor; OK keeps the changes,
 *  cancel restores what the screen opened with. Both go back to phase 19.
 *  Row names are message 0x65 rows 0x1e..0x21, the two choices of row i
 *  0x22 + 2i (value 0) / 0x23 + 2i (value 1), the help lines 0x2a..0x2d.
 *
 *  FF3 0x00222d20 / 0x00222d28 (two empty unreferenced functions) and
 *  0x00222d30 (OptionSousaGPhaseCtrlAtExit, the compiler's atexit thunk for
 *  the GetGPhaseCtrl static, ending the file) are not written.
 * ======================================================================== */
#include "outgame/option_sousa.h"

#include "common/WindowMsg.h"           // CWindowMsg
#include "graphics/graph2d/message.h"   // SetCurColorRGB
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetDefaultGsDrawState
#include "ingame/menu/menu_csr.h"       // MenuCsrUpdate, MenuCsrGetPos, MenuCsrIsMovedSe
#include "main/gphase.h"
#include "outgame/option.h"             // OPTION_WRK, option_*, OptionSub*Step, LayoutItemAddPos, PrintMsgPos
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"          // B2dSprSetPos, B2dSprFreezePart
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe, CheckPadRptPlaySe

#define OPTION_SOUSA_LAYOUT_FILE    0x1264  // OPTION_SOUSA_PHF (+ language)

#define OPTION_SOUSA_SE_MOVE        0x1a9b0000
#define OPTION_SOUSA_SE_CANCEL      0x1a9b0001
#define OPTION_SOUSA_SE_OK          0x1a9b0003

#define OPTION_SOUSA_ITEM_CSR       0x3f9   // the cursor bar
#define OPTION_SOUSA_ITEM_WIN       0x3fb   // the window frame item (fade layer and CWindowMsg)
#define OPTION_SOUSA_ITEM_NAME      0x3fc   // the row names, OptionSousaNameDrawCallback
#define OPTION_SOUSA_ITEM_VALUE     0x400   // the chosen values, OptionSousaValueDrawCallback

#define OPTION_SOUSA_MSG            0x65
#define OPTION_SOUSA_MSG_HELP       0x2a    // + row: the help lines
#define OPTION_SOUSA_MSG_NAME       0x1e    // + row: the row names
#define OPTION_SOUSA_MSG_VALUE      0x22    // + 2 * row (+ 1 when the setting is 1)

#define OPTION_SOUSA_ROW_Y0         0x72    // the first row's y in the table

// The rows' y in the layout (data 0x35f970); the text sits at y - 0x72.
static const short option_sousa_row_y[OPTION_SOUSA_ROWS] = { 114, 146, 218, 250 };

static int OptionSousaLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_SOUSA_LAYOUT_FILE;
}

// The setting of a row (signed bytes as FF3 reads them, lb).
static signed char *OptionSousaRowValue(OPTION_WRK *opt, int row)
{
    switch (row)
    {
    case 0:  return (signed char *)&opt->now.b11;
    case 1:  return (signed char *)&opt->now.b12;
    case 2:  return (signed char *)&opt->now.b09;
    default: return (signed char *)&opt->now.b13;
    }
}

// The help line of the row under the cursor. FF3 0x00222640.
static int OptionSousaGetMsgNo(void)
{
    return MenuCsrGetPos(OPTION_SOUSA_CSR) + OPTION_SOUSA_MSG_HELP;
}

// Every frame while the screen runs: the cursor bar on the row.
// FF3 0x00222660.
// callcheck: GetLanguage, GetSysWrk (OptionSousaLayoutFile)
static void OptionSousaCsrSetPos(void)
{
    int handle = BpCtlGetHandleByFileNo(OptionSousaLayoutFile());
    int pos    = MenuCsrGetPos(OPTION_SOUSA_CSR);

    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_SOUSA_ITEM_CSR), 0,
                 option_sousa_row_y[pos] - OPTION_SOUSA_ROW_Y0);
    B2dSprFreezePart(handle, OPTION_SOUSA_ITEM_CSR, 6, 0);
    B2dSprFreezePart(handle, OPTION_SOUSA_ITEM_CSR, 7, 0);
}

// One row's message at (0, row y - 0x72) of the hook item, lit under the
// cursor. Words: a0 the item's label, t1 its rectangle, stack[2] alpha.
// callcheck: GetLanguage, GetSysWrk (OptionSousaLayoutFile)
static void OptionSousaPrintRow(const B2D_HOOK_ARGS *args, int row, int cur, int msg)
{
    const char *label = (const char *)args->a0;
    float       pos[2];
    int         type;

    pos[0] = 0.0f;
    pos[1] = (float)(option_sousa_row_y[row] - OPTION_SOUSA_ROW_Y0);
    if (row == cur)
        SetCurColorRGB(0xbd, 0xb8, 0x9d);
    else
        SetCurColorRGB(0x8c, 0x85, 0x62);
    type = GetLabelSuffixType(label);
    LayoutItemAddPos(pos, label, OptionSousaLayoutFile(), (int *)args->t1);
    PrintMsgPos(OPTION_SOUSA_MSG, msg, pos, 0x2d, (int)args->stack[2], 0, type);
}

// Item 0x3fc: the four row names; then the help line of the cursor row
// into the window (FF3 does it here, every frame).
// FF3 0x00222710 (no Ghidra function of its own before 2026-10-21).
// callcheck: GetLabelSuffixType, LayoutItemAddPos, PrintMsgPos, SetCurColorRGB, GetLanguage, GetSysWrk (OptionSousaPrintRow)
static int OptionSousaNameDrawCallback(const B2D_HOOK_ARGS *args)
{
    int cur = MenuCsrGetPos(OPTION_SOUSA_CSR);
    int i;

    for (i = 0; i < OPTION_SOUSA_ROWS; i++)
        OptionSousaPrintRow(args, i, cur, OPTION_SOUSA_MSG_NAME + i);

    option_sel_win->StartMsg(OPTION_SOUSA_MSG, OptionSousaGetMsgNo(), 1);
    SetDefaultGsDrawState();
    return 0;
}

// Item 0x400: each row's chosen value. FF3 tests the setting == 1, so a
// value other than 0 / 1 shows the first word.
// FF3 0x00222860 (no Ghidra function of its own before 2026-10-21).
// callcheck: GetLabelSuffixType, LayoutItemAddPos, PrintMsgPos, SetCurColorRGB, GetLanguage, GetSysWrk (OptionSousaPrintRow)
static int OptionSousaValueDrawCallback(const B2D_HOOK_ARGS *args)
{
    OPTION_WRK *opt = GetOptionWrk();
    int         cur = MenuCsrGetPos(OPTION_SOUSA_CSR);
    int         i;

    for (i = 0; i < OPTION_SOUSA_ROWS; i++)
    {
        int msg = OPTION_SOUSA_MSG_VALUE + i * 2;

        if (*OptionSousaRowValue(opt, i) == 1)
            msg++;
        OptionSousaPrintRow(args, i, cur, msg);
    }

    SetDefaultGsDrawState();
    return 0;
}

// on_ready of OptionSubInitStep: the two hooks, the window on item 0x3fb
// with the help line, and the fade layer on the same item.
// FF3 0x00222a18 (the record list is copied to the stack from data 0x35f940).
// callcheck: GetLanguage, GetSysWrk (OptionSousaLayoutFile)
static void OptionSousaAnimStart(void)
{
    int                handle = BpCtlGetHandleByFileNo(OptionSousaLayoutFile());
    const B2D_HOOK_REC hooks[] =
    {
        { OptionSousaNameDrawCallback,  0, OPTION_SOUSA_ITEM_NAME,  nullptr },
        { OptionSousaValueDrawCallback, 0, OPTION_SOUSA_ITEM_VALUE, nullptr },
        { nullptr,                     -1, -1,                      nullptr },
    };

    B2dSetHookList(handle, hooks);
    option_sel_win->SetItem(handle, OPTION_SOUSA_ITEM_WIN);
    option_sel_win->StartMsg(OPTION_SOUSA_MSG, OptionSousaGetMsgNo(), 1);
    CmnFadeLayerShowItem(0, handle, OPTION_SOUSA_ITEM_WIN);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 22
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00222af0.
// callcheck: GetLanguage, GetSysWrk (OptionSousaLayoutFile)
int init_OptionSousa(void)
{
    return OptionSubInitStep(OptionSousaAnimStart, OptionSousaCsrSetPos,
                             OptionSousaLayoutFile(), "in");
}

// FF3 0x00222b40.
int one_OptionSousa(void)
{
    int pos;

    MenuCsrUpdate(OPTION_SOUSA_CSR, 0);
    OptionSousaCsrSetPos();
    pos = MenuCsrGetPos(OPTION_SOUSA_CSR);

    if (MenuCsrIsMovedSe(OPTION_SOUSA_CSR, OPTION_SOUSA_SE_MOVE))
        return 0;

    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], OPTION_SOUSA_SE_OK, 0))
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], OPTION_SOUSA_SE_CANCEL, 0))
        {
            // Left (0x8000) / right (0x2000), repeated, with the move SE.
            if (!CheckPadRptPlaySe(0xa000, OPTION_SOUSA_SE_MOVE, 0))
                return 0;
            if (pos >= 0 && pos < OPTION_SOUSA_ROWS)
            {
                signed char *value = OptionSousaRowValue(GetOptionWrk(), pos);

                *value = *value == 0;
            }
            return 0;
        }
        option_cancel = 1;
    }

    GetGPhaseCtrl()->SetNext(0x13, 0);
    return 1;
}

// FF3 0x00222cb0.
// callcheck: GetLanguage, GetSysWrk (OptionSousaLayoutFile)
int end_OptionSousa(void)
{
    if (!OptionSubEndStep(OptionSousaCsrSetPos, OptionSousaLayoutFile(), "out"))
        return 0;
    CmnFadeLayerTermItem();
    option_sel_win->SetAlphaItem(nullptr);
    return 1;
}
