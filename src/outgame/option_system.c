/* ==========================================================================
 *  outgame/option_system.c
 *
 *  Phase 20, the option "System Settings" screen. FF3 0x223578..0x223e60,
 *  FF3-only. Five rows on OPTION_SYSTEM_PHF (+ language): vibration,
 *  subtitles, stereo / mono, then the two volume sliders (vol_ch0,
 *  vol_ch1). Left / right changes the row under the cursor at once (phase
 *  7's after step pushes the volumes and the mono mode to the sound layer
 *  every frame); OK keeps the changes, cancel restores what the screen
 *  opened with (OptionSubEndStep with option_cancel). Both go back to the
 *  top menu, phase 19.
 *
 *  The row names (message 0x65 rows 0x14..0x18) and the on / off and
 *  stereo / mono words are printed by two item hooks; the help line of the
 *  row (0x65 rows 0xf..0x13) goes into the option selection window.
 *
 *  FF3 0x00223e40 (OptionSystemGPhaseCtrlAtExit) is not written: the
 *  compiler's atexit thunk for the GetGPhaseCtrl static, ending the file.
 * ======================================================================== */
#include "outgame/option_system.h"

#include "common/WindowMsg.h"           // CWindowMsg
#include "graphics/graph2d/message.h"   // SetCurColorRGB
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetDefaultGsDrawState
#include "ingame/menu/menu_csr.h"       // MenuCsrUpdate, MenuCsrGetPos, MenuCsrIsMovedSe
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"
#include "outgame/option.h"             // OPTION_WRK, option_*, OptionSub*Step, LayoutItemAddPos, PrintMsgPos, CsrMoveUpDownSe
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"          // B2dSprSetPos, B2dSprFreezePart
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe, CheckPadRptPlaySe

#define OPTION_SYSTEM_LAYOUT_FILE   0x125a  // OPTION_SYSTEM_PHF (+ language)

#define OPTION_SYSTEM_SE_MOVE       0x1a9b0000
#define OPTION_SYSTEM_SE_CANCEL     0x1a9b0001
#define OPTION_SYSTEM_SE_OK         0x1a9b0003

#define OPTION_SYSTEM_ITEM_VOL0     0x3f1   // the vol_ch0 slider knob
#define OPTION_SYSTEM_ITEM_VOL1     0x3f9   // the vol_ch1 slider knob
#define OPTION_SYSTEM_ITEM_CSR      0x3fa   // the cursor bar
#define OPTION_SYSTEM_ITEM_WIN      0x3ff   // the window frame item (fade layer and CWindowMsg)
#define OPTION_SYSTEM_ITEM_NAME     0x400   // the row names, OptionSystemNameDrawCallback
#define OPTION_SYSTEM_ITEM_VALUE    0x405   // the on / off words, OptionSystemValueDrawCallback

#define OPTION_SYSTEM_MSG           0x65
#define OPTION_SYSTEM_MSG_HELP      0xf     // + row: the help lines
#define OPTION_SYSTEM_MSG_NAME      0x14    // + row: the row names
#define OPTION_SYSTEM_MSG_ON        0x19
#define OPTION_SYSTEM_MSG_OFF       0x1a
#define OPTION_SYSTEM_MSG_STEREO    0x1b
#define OPTION_SYSTEM_MSG_MONO      0x1c

#define OPTION_SYSTEM_ROW_Y0        0x72    // the first row's y in the table

// The rows' y in the layout (data 0x35fae0); the text sits at y - 0x72,
// relative to the hook item's own position.
static const short option_system_row_y[OPTION_SYSTEM_ROWS] = { 114, 146, 218, 250, 282 };

static int OptionSystemLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_SYSTEM_LAYOUT_FILE;
}

// The help line of the row under the cursor. FF3 0x00223578.
static int OptionSystemGetMsgNo(void)
{
    return MenuCsrGetPos(OPTION_SYSTEM_CSR) + OPTION_SYSTEM_MSG_HELP;
}

// Every frame while the screen runs (on_wait of OptionSub*Step and
// one_OptionSystem): the cursor bar on the row, the volume knobs at
// x = 357 + v * 156 / 19 (v 0..19).
// FF3 0x00223598.
// callcheck: GetLanguage, GetSysWrk (OptionSystemLayoutFile), GetOptionWrk
static void OptionSystemCsrSetPos(void)
{
    int         handle = BpCtlGetHandleByFileNo(OptionSystemLayoutFile());
    int         pos    = MenuCsrGetPos(OPTION_SYSTEM_CSR);
    OPTION_WRK *opt;

    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_SYSTEM_ITEM_CSR), 0,
                 option_system_row_y[pos] - OPTION_SYSTEM_ROW_Y0);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_CSR, 6, 0);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_CSR, 7, 0);

    opt = GetOptionWrk();
    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_SYSTEM_ITEM_VOL0), opt->now.vol_ch0 * 0x9c / 0x13 + 0x165, 0xff);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_VOL0, 6, 0);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_VOL0, 7, 0);
    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_SYSTEM_ITEM_VOL1), opt->now.vol_ch1 * 0x9c / 0x13 + 0x165, 0x120);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_VOL1, 6, 0);
    B2dSprFreezePart(handle, OPTION_SYSTEM_ITEM_VOL1, 7, 0);
}

// The text colour of a row: lit under the cursor.
static void OptionSystemSetRowColor(int row, int cur)
{
    if (row == cur)
        SetCurColorRGB(0xbd, 0xb8, 0x9d);
    else
        SetCurColorRGB(0x8c, 0x85, 0x62);
}

// One message at (0, y) of the hook item. Words: a0 the item's label, t1
// its rectangle, stack[2] alpha.
static void OptionSystemPrintRow(const B2D_HOOK_ARGS *args, int msg, float y)
{
    const char *label = (const char *)args->a0;
    float       pos[2];
    int         type;

    pos[0] = 0.0f;
    pos[1] = y;
    type   = GetLabelSuffixType(label);
    LayoutItemAddPos(pos, label, OptionSystemLayoutFile(), (int *)args->t1);
    PrintMsgPos(OPTION_SYSTEM_MSG, msg, pos, 0x2d, (int)args->stack[2], 0, type);
}

// Item 0x400: the five row names, the cursor row lit; then the help line
// of the cursor row into the window (FF3 does it here, every frame).
// FF3 0x00223728 (no Ghidra function of its own before 2026-10-21).
// callcheck: GetLanguage, GetSysWrk, GetLabelSuffixType, LayoutItemAddPos, PrintMsgPos, SetCurColorRGB (in OptionSystemPrintRow / SetRowColor)
static int OptionSystemNameDrawCallback(const B2D_HOOK_ARGS *args)
{
    int cur = MenuCsrGetPos(OPTION_SYSTEM_CSR);
    int i;

    for (i = 0; i < OPTION_SYSTEM_ROWS; i++)
    {
        OptionSystemSetRowColor(i, cur);
        OptionSystemPrintRow(args, OPTION_SYSTEM_MSG_NAME + i,
                             (float)(option_system_row_y[i] - OPTION_SYSTEM_ROW_Y0));
    }

    option_sel_win->StartMsg(OPTION_SYSTEM_MSG, OptionSystemGetMsgNo(), 1);
    SetDefaultGsDrawState();
    return 0;
}

// Item 0x405: "on" / "off" for vibration and subtitles, "stereo" / "mono"
// on row 2 (the sliders of rows 3 / 4 are sprites, OptionSystemCsrSetPos).
// FF3 0x00223878 (no Ghidra function of its own before 2026-10-21).
// callcheck: GetOptionWrk, GetLanguage, GetSysWrk, GetLabelSuffixType, LayoutItemAddPos, PrintMsgPos, SetCurColorRGB (OptionSystemPrintRow / SetRowColor)
static int OptionSystemValueDrawCallback(const B2D_HOOK_ARGS *args)
{
    OPTION_WRK *opt = GetOptionWrk();
    int         cur = MenuCsrGetPos(OPTION_SYSTEM_CSR);
    int         i;

    for (i = 0; i < 2; i++)
    {
        int on = i == 0 ? opt->now.vibration : opt->now.subtitle;

        OptionSystemSetRowColor(i, cur);
        OptionSystemPrintRow(args, on != 0 ? OPTION_SYSTEM_MSG_ON : OPTION_SYSTEM_MSG_OFF,
                             (float)(option_system_row_y[i] - OPTION_SYSTEM_ROW_Y0));
    }

    // FF3 reads row 2's y as a float constant (sdata 0x371790), the table's 218 - 0x72.
    OptionSystemSetRowColor(2, cur);
    OptionSystemPrintRow(args, opt->now.mono != 0 ? OPTION_SYSTEM_MSG_MONO : OPTION_SYSTEM_MSG_STEREO, 104.0f);

    SetDefaultGsDrawState();
    return 0;
}

// on_ready of OptionSubInitStep: the two hooks, the window on item 0x3ff
// with the help line, and the fade layer on the same item.
// FF3 0x00223aa8 (the record list is copied to the stack from data 0x35fa98).
// callcheck: GetLanguage, GetSysWrk (OptionSystemLayoutFile)
static void OptionSystemAnimStart(void)
{
    int                handle = BpCtlGetHandleByFileNo(OptionSystemLayoutFile());
    const B2D_HOOK_REC hooks[] =
    {
        { OptionSystemNameDrawCallback,  0, OPTION_SYSTEM_ITEM_NAME,  nullptr },
        { OptionSystemValueDrawCallback, 0, OPTION_SYSTEM_ITEM_VALUE, nullptr },
        { nullptr,                      -1, -1,                       nullptr },
    };

    B2dSetHookList(handle, hooks);
    option_sel_win->SetItem(handle, OPTION_SYSTEM_ITEM_WIN);
    option_sel_win->StartMsg(OPTION_SYSTEM_MSG, OptionSystemGetMsgNo(), 1);
    CmnFadeLayerShowItem(0, handle, OPTION_SYSTEM_ITEM_WIN);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 20
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00223b80.
// callcheck: GetLanguage, GetSysWrk (OptionSystemLayoutFile)
int init_OptionSystem(void)
{
    return OptionSubInitStep(OptionSystemAnimStart, OptionSystemCsrSetPos,
                             OptionSystemLayoutFile(), "in");
}

// FF3 0x00223bd0.
int one_OptionSystem(void)
{
    OPTION_WRK *opt = GetOptionWrk();
    int         pos;

    MenuCsrUpdate(OPTION_SYSTEM_CSR, 0);
    OptionSystemCsrSetPos();
    pos = MenuCsrGetPos(OPTION_SYSTEM_CSR);

    if (MenuCsrIsMovedSe(OPTION_SYSTEM_CSR, OPTION_SYSTEM_SE_MOVE))
        return 0;

    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], OPTION_SYSTEM_SE_OK, 0))
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], OPTION_SYSTEM_SE_CANCEL, 0))
        {
            // Left (0x8000) / right (0x2000), repeated.
            if (!CheckPadRptPlaySe(0xa000, -1, 0))
                return 0;

            switch (pos)
            {
            case 0:
                opt->now.vibration = opt->now.vibration == 0;
                break;
            case 1:
                opt->now.subtitle = opt->now.subtitle == 0;
                break;
            case 2:
                opt->now.mono = opt->now.mono == 0;
                break;
            case 3:
            case 4:
            {
                // CsrMoveUpDownSe plays the move SE itself.
                signed char *vol = pos == 4 ? &opt->now.vol_ch1 : &opt->now.vol_ch0;

                *vol = (signed char)CsrMoveUpDownSe(*vol, 0, 0x14, 0x2000, 0x8000, 0, OPTION_SYSTEM_SE_MOVE);
                return 0;
            }
            default:
                return 0;
            }
            SndSePlay(OPTION_SYSTEM_SE_MOVE, 100, 0x1000, 0, nullptr, 0);
            return 0;
        }
        option_cancel = 1;
    }

    GetGPhaseCtrl()->SetNext(0x13, 0);
    return 1;
}

// FF3 0x00223dc0.
// callcheck: GetLanguage, GetSysWrk (OptionSystemLayoutFile)
int end_OptionSystem(void)
{
    if (!OptionSubEndStep(OptionSystemCsrSetPos, OptionSystemLayoutFile(), "out"))
        return 0;
    CmnFadeLayerTermItem();
    option_sel_win->SetAlphaItem(nullptr);
    return 1;
}
