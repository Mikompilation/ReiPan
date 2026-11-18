/* ==========================================================================
 *  outgame/option_akarusa.c
 *
 *  Phase 21, the option "Brightness" screen (akarusa: brightness). FF3
 *  0x222170..0x222640, FF3-only. One slider on OPTION_AKARUSA_PHF
 *  (+ language) over OPTION_DAT byte 0, the brightness level 0..11 (6 the
 *  default). The sample picture (item 0x3f9) is drawn under the in-game
 *  brightness filter at the level being chosen, and the help line below
 *  (message 0x65, 0x32 + one of five texts) follows the level. Left / right
 *  moves the knob; OK keeps the level, cancel restores what the screen
 *  opened with. Both go back to phase 19.
 *
 *  FF3 0x00222620 (OptionAkarusaGPhaseCtrlAtExit, the compiler's atexit
 *  thunk for the GetGPhaseCtrl static, ending the file) is not written.
 * ======================================================================== */
#include "outgame/option_akarusa.h"

#include "common/WindowMsg.h"           // CWindowMsg
#include "graphics/effect/effect_oth.h" // BrightnessAdjustmentFilterDraw
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList
#include "main/gphase.h"
#include "outgame/option.h"             // OPTION_WRK, option_*, OptionSub*Step, CsrMoveUpDownSe
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"          // B2dSprSetPos, B2dSprFreezePart
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe, CheckPadRptPlaySe

#define OPTION_AKARUSA_LAYOUT_FILE  0x1269  // OPTION_AKARUSA_PHF (+ language)

#define OPTION_AKARUSA_SE_MOVE      0x1a9b0000
#define OPTION_AKARUSA_SE_CANCEL    0x1a9b0001
#define OPTION_AKARUSA_SE_OK        0x1a9b0003

#define OPTION_AKARUSA_ITEM_KNOB    0x3ef   // the slider knob
#define OPTION_AKARUSA_ITEM_WIN     0x3f8   // the window frame item (fade layer and CWindowMsg)
#define OPTION_AKARUSA_ITEM_SAMPLE  0x3f9   // the sample picture, OptionAkarusaFilterDrawCallback

#define OPTION_AKARUSA_MSG          0x65
#define OPTION_AKARUSA_MSG_HELP     0x32    // + OptionAkarusaGetHelpIdx(): the help lines

#define OPTION_AKARUSA_DEFAULT      6       // the default level, the knob's centre
#define OPTION_AKARUSA_KNOB_X0      0x143   // the knob's x at the default level
#define OPTION_AKARUSA_KNOB_STEP    0x24    // per level
#define OPTION_AKARUSA_KNOB_Y       0x125

// The help line per level (data 0x35f8f0, copied to the stack in FF3).
static const int option_akarusa_help[OPTION_AKARUSA_LEVELS] =
{
    0, 0, 1, 1, 1, 1, 2, 3, 3, 3, 4, 4,
};

static int OptionAkarusaLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_AKARUSA_LAYOUT_FILE;
}

// The level as FF3 reads it (lb: a signed byte).
static int OptionAkarusaLevel(void)
{
    return (signed char)GetOptionWrk()->now.b00;
}

// FF3 0x00222170.
// callcheck: GetOptionWrk (OptionAkarusaLevel)
static int OptionAkarusaGetHelpIdx(void)
{
    return option_akarusa_help[OptionAkarusaLevel()];
}

// The help line of the level. FF3 0x002221e8.
static int OptionAkarusaGetMsgNo(void)
{
    return OptionAkarusaGetHelpIdx() + OPTION_AKARUSA_MSG_HELP;
}

// Every frame while the screen runs: the knob on the level.
// FF3 0x00222208.
// callcheck: GetLanguage, GetSysWrk (OptionAkarusaLayoutFile)
// callcheck: GetOptionWrk (OptionAkarusaLevel)
static void OptionAkarusaCsrSetPos(void)
{
    int level  = OptionAkarusaLevel();
    int handle = BpCtlGetHandleByFileNo(OptionAkarusaLayoutFile());

    B2dSprSetPos(B2dCtlFindItem(handle, OPTION_AKARUSA_ITEM_KNOB),
                 (level - OPTION_AKARUSA_DEFAULT) * OPTION_AKARUSA_KNOB_STEP + OPTION_AKARUSA_KNOB_X0,
                 OPTION_AKARUSA_KNOB_Y);
    B2dSprFreezePart(handle, OPTION_AKARUSA_ITEM_KNOB, 6, 0);
    B2dSprFreezePart(handle, OPTION_AKARUSA_ITEM_KNOB, 7, 0);
}

// Item 0x3f9 (hook kind 2: a1 w, a2 h, t1 the rectangle, stack[2] alpha):
// the brightness filter over the sample picture, at the item's alpha; then
// the help line of the level into the window (FF3 does it here, every frame).
// FF3 0x002222c0 (no Ghidra function of its own before 2026-10-21).
static int OptionAkarusaFilterDrawCallback(const B2D_HOOK_ARGS *args)
{
    const int *rect = (const int *)args->t1;

    BrightnessAdjustmentFilterDraw((float)(int)args->stack[2] * (1.0f / 128.0f),
                                   rect[0], rect[1], (int)args->a1, (int)args->a2);
    option_sel_win->StartMsg(OPTION_AKARUSA_MSG, OptionAkarusaGetMsgNo(), 1);
    return 0;
}

// on_ready of OptionSubInitStep: the filter hook, the window on item 0x3f8
// with the help line, and the fade layer on the same item.
// FF3 0x00222328 (the record list is built on the stack).
// callcheck: GetLanguage, GetSysWrk (OptionAkarusaLayoutFile)
static void OptionAkarusaAnimStart(void)
{
    const B2D_HOOK_REC hooks[] =
    {
        { OptionAkarusaFilterDrawCallback, 0, OPTION_AKARUSA_ITEM_SAMPLE, nullptr },
        { nullptr,                        -1, -1,                         nullptr },
    };
    int handle;

    B2dSetHookList(BpCtlGetHandleByFileNo(OptionAkarusaLayoutFile()), hooks);
    handle = BpCtlGetHandleByFileNo(OptionAkarusaLayoutFile());
    option_sel_win->SetItem(handle, OPTION_AKARUSA_ITEM_WIN);
    option_sel_win->StartMsg(OPTION_AKARUSA_MSG, OptionAkarusaGetMsgNo(), 1);
    CmnFadeLayerShowItem(0, handle, OPTION_AKARUSA_ITEM_WIN);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 21
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00222430.
// callcheck: GetLanguage, GetSysWrk (OptionAkarusaLayoutFile)
int init_OptionAkarusa(void)
{
    return OptionSubInitStep(OptionAkarusaAnimStart, OptionAkarusaCsrSetPos,
                             OptionAkarusaLayoutFile(), "in");
}

// FF3 0x00222480.
int one_OptionAkarusa(void)
{
    OptionAkarusaCsrSetPos();

    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], OPTION_AKARUSA_SE_OK, 0))
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], OPTION_AKARUSA_SE_CANCEL, 0))
        {
            OPTION_WRK *opt;

            // Left (0x8000) / right (0x2000), repeated; the SE comes from
            // CsrMoveUpDownSe, which stops at both ends.
            if (!CheckPadRptPlaySe(0xa000, -1, 0))
                return 0;
            opt          = GetOptionWrk();
            opt->now.b00 = (u_char)CsrMoveUpDownSe(opt->now.b00, 0, OPTION_AKARUSA_LEVELS,
                                                   0x2000, 0x8000, 0, OPTION_AKARUSA_SE_MOVE);
            return 0;
        }
        option_cancel = 1;
    }

    GetGPhaseCtrl()->SetNext(0x13, 0);
    return 1;
}

// FF3 0x002225a0.
// callcheck: GetLanguage, GetSysWrk (OptionAkarusaLayoutFile)
int end_OptionAkarusa(void)
{
    if (!OptionSubEndStep(OptionAkarusaCsrSetPos, OptionAkarusaLayoutFile(), "out"))
        return 0;
    CmnFadeLayerTermItem();
    option_sel_win->SetAlphaItem(nullptr);
    return 1;
}
