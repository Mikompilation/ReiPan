/* ==========================================================================
 *  outgame/option_button.c
 *
 *  Phase 23, the option "Controller" screen. FF3 0x222d50..0x223578,
 *  FF3-only. A pad picture on OPTION_BUTTON_PHF (+ language) with the
 *  action of each button written beside it, for the button layout
 *  OPTION_DAT.pad_cfg (0..2, one marker item 0x3ec + n each). Left / right
 *  steps the layout (wrapping), SQUARE switches between the normal view and
 *  the viewfinder (camera) view, each with its own label set; OK keeps the
 *  layout, cancel restores the one the screen opened with. Both go back to
 *  phase 19. There is no menu cursor and no help line.
 *
 *  A button's text is found by walking the view's action table: the first
 *  action whose configured pad bits (PadKeyGetConfig) include the
 *  button's bit (PadGetSceButton) names it.
 *
 *  FF3 0x00223548 / 0x00223550 (two empty unreferenced functions) and
 *  0x00223558 (OptionButtonGPhaseCtrlAtExit, the compiler's atexit thunk
 *  for the GetGPhaseCtrl static, ending the file) are not written.
 * ======================================================================== */
#include "outgame/option_button.h"

#include "graphics/graph2d/message.h"   // SetCurColorRGB
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, GetLabelSuffixType, SetDefaultGsDrawState
#include "main/gphase.h"
#include "outgame/option.h"             // OPTION_WRK, option_cancel, OptionSub*Step, LayoutItemAddPos, PrintMsgPos, CsrMoveUpDownSe
#include "system/b2d/B2dCtl.h"          // B2D_ITEM, B2dCtlFindItem
#include "system/b2d/B2dSpr.h"          // B2dSprGetPartData, B2dSprSetDisp
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo, BpCtlIsLoadEnd, BpPlayLabel
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"             // PadGetSceButton
#include "system/pad/pad_ctrl.h"        // pad_key, PadKeyGetConfig, CheckPadTrgPlaySe, CheckPadRptPlaySe

#define OPTION_BUTTON_LAYOUT_FILE   0x125f  // OPTION_BUTTON_PHF (+ language)

#define OPTION_BUTTON_SE_MOVE       0x1a9b0000
#define OPTION_BUTTON_SE_CANCEL     0x1a9b0001
#define OPTION_BUTTON_SE_OK         0x1a9b0003

#define OPTION_BUTTON_PAD_SQUARE    0x0080  // switches the view

#define OPTION_BUTTON_ITEM_WIN      0x3f4   // the fade layer item
#define OPTION_BUTTON_ITEM_CFG      0x3ec   // + n: the marker of button layout n (0..3 walked)
#define OPTION_BUTTON_ITEM_NORMAL   0x3f8   // hook item of the normal view's labels
#define OPTION_BUTTON_ITEM_FINDER   0x403   // hook item of the viewfinder view's labels

#define OPTION_BUTTON_MSG           0x65

// One action of a view: its pad_key_cfg slot and the message naming it.
struct OPTION_BUTTON_ACT            /* 0x08 */
{
    /* 0x00 */ int key;
    /* 0x04 */ int msg;
};
static_assert(sizeof(OPTION_BUTTON_ACT) == 0x08, "OPTION_BUTTON_ACT is 8 bytes");

// One labelled button: its sce_pad index and the label item of each view.
struct OPTION_BUTTON_LABEL          /* 0x08 */
{
    /* 0x00 */ int   button;
    /* 0x04 */ short item_finder;       // < 0: no label in that view
    /* 0x06 */ short item_normal;
};
static_assert(sizeof(OPTION_BUTTON_LABEL) == 0x08, "OPTION_BUTTON_LABEL is 8 bytes");
static_assert(offsetof(OPTION_BUTTON_LABEL, item_normal) == 0x06, "OPTION_BUTTON_LABEL.item_normal");

// data 0x35f9d0: the viewfinder view's actions.
static const OPTION_BUTTON_ACT option_button_act_finder[7] =
{
    {  3, 0x3c }, {  0, 0x3d }, {  2, 0x3e }, { 18, 0x3f }, { 13, 0x40 }, { 11, 0x41 }, { 12, 0x41 },
};

// data 0x35fa08: the normal view's actions.
static const OPTION_BUTTON_ACT option_button_act_normal[9] =
{
    {  4, 0x45 }, {  6, 0x46 }, {  7, 0x46 }, { 18, 0x3f }, {  5, 0x47 },
    { 19, 0x49 }, { 20, 0x49 }, { 21, 0x4b }, { 22, 0x4b },
};

// data 0x35fa50: the labelled buttons.
static const OPTION_BUTTON_LABEL option_button_label[9] =
{
    {  8, 0x403, 0x3f8 }, { 10, 0x406, 0x3f9 }, {  9, 0x405, 0x3fa }, { 11, 0x40d, 0x3fb },
    {  4, 0x408, 0x3fd }, {  5, 0x409, 0x3fe }, {  6, 0x40a, 0x3ff }, {  7, 0x40b, 0x400 },
    {  0, 0x407, 0x3fc },
};

// The view shown: 1 viewfinder, 0 normal. sdata 0x373750 (gp+0x260).
static int option_button_finder;

static int OptionButtonLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_BUTTON_LAYOUT_FILE;
}

// The message of the action that button layout 'cfg' puts on sce_pad
// button 'button' in the given view; 0x4e when none (button 0 has a fixed
// text per view).
// FF3 0x00222d50.
static int OptionButtonGetMsgNo(int cfg, int button, int finder)
{
    const OPTION_BUTTON_ACT *act = finder != 0 ? option_button_act_finder : option_button_act_normal;
    u_int                    num = finder != 0 ? 7 : 9;
    u_int                    i;

    if (button == 0)
        return finder != 0 ? 0x4e : 0x4d;

    for (i = 0; i < num; i++)
    {
        if ((PadKeyGetConfig(cfg, act[i].key) & PadGetSceButton(button)) != 0)
            return act[i].msg;
    }
    return 0x4e;
}

// One message at the item's own position (parts 6 / 7), aligned by the
// item's label suffix.
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
static void OptionButtonPrintAtItem(int handle, int id, int msg, int alpha)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);
    short     x    = (short)B2dSprGetPartData(item, 6);
    short     y    = (short)B2dSprGetPartData(item, 7);
    float     pos[2];
    int       type;

    pos[0] = (float)x;
    pos[1] = (float)y;
    type   = GetLabelSuffixType(item->name);
    LayoutItemAddPos(pos, item->name, OptionButtonLayoutFile(), nullptr);
    PrintMsgPos(OPTION_BUTTON_MSG, msg, pos, 0x2d, alpha, 0, type);
}

// The labels of one view: each button's action, then the view's title
// (item 0x401 / 0x40c, message 0x4f / 0x50).
// FF3 0x00222e30 (its rectangle argument is not read).
// callcheck: GetLanguage, GetSysWrk, B2dCtlFindItem, B2dSprGetPartData, GetLabelSuffixType, LayoutItemAddPos, PrintMsgPos (in OptionButtonPrintAtItem)
static void OptionButtonDrawLabels(int finder, int *rect, int alpha)
{
    int cfg    = GetOptionWrk()->now.pad_cfg;
    int handle = BpCtlGetHandleByFileNo(OptionButtonLayoutFile());
    int i;

    (void)rect;
    SetCurColorRGB(0x8c, 0x85, 0x62);

    for (i = 0; i < 9; i++)
    {
        const OPTION_BUTTON_LABEL *rec = &option_button_label[i];
        int                        id  = finder != 0 ? rec->item_finder : rec->item_normal;

        if (id < 0)
            continue;
        // FF3 looks the message up between reading the position and the label.
        OptionButtonPrintAtItem(handle, id, (short)OptionButtonGetMsgNo(cfg, rec->button, finder), alpha);
    }

    OptionButtonPrintAtItem(handle, finder != 0 ? 0x401 : 0x40c, finder != 0 ? 0x4f : 0x50, alpha);
    SetDefaultGsDrawState();
}

// on_wait of OptionSub*Step and every frame of one_OptionButton: only the
// marker of the current button layout shows.
// FF3 0x002230c8.
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
static void OptionButtonCfgDispUpdate(void)
{
    int handle = BpCtlGetHandleByFileNo(OptionButtonLayoutFile());
    int i;

    for (i = 0; i < 4; i++)
    {
        B2D_ITEM *item = B2dCtlFindItem(handle, OPTION_BUTTON_ITEM_CFG + i);

        B2dSprSetDisp(item, i == GetOptionWrk()->now.pad_cfg ? 1 : 0);
    }
}

// Item 0x403: the viewfinder view's labels. Words: t1 the item's
// rectangle, stack[2] alpha. FF3 0x00223178 (no Ghidra function of its own
// before 2026-10-21).
static int OptionButtonFinderDrawCallback(const B2D_HOOK_ARGS *args)
{
    OptionButtonDrawLabels(1, (int *)args->t1, (int)args->stack[2]);
    return 0;
}

// Item 0x3f8: the normal view's labels. FF3 0x002231a0.
static int OptionButtonNormalDrawCallback(const B2D_HOOK_ARGS *args)
{
    OptionButtonDrawLabels(0, (int *)args->t1, (int)args->stack[2]);
    return 0;
}

// on_ready of OptionSubInitStep: both views' hooks and the fade layer on
// item 0x3f4 (fade-layer label 0x15; the other option screens pass 0).
// FF3 0x002231c8 (the record list is copied to the stack from data 0x35f980).
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
static void OptionButtonAnimStart(void)
{
    const B2D_HOOK_REC hooks[] =
    {
        { OptionButtonFinderDrawCallback,  0, OPTION_BUTTON_ITEM_FINDER, nullptr },
        { OptionButtonNormalDrawCallback,  0, OPTION_BUTTON_ITEM_NORMAL, nullptr },
        { nullptr,                        -1, -1,                        nullptr },
    };

    B2dSetHookList(BpCtlGetHandleByFileNo(OptionButtonLayoutFile()), hooks);
    CmnFadeLayerShowItem(0x15, BpCtlGetHandleByFileNo(OptionButtonLayoutFile()), OPTION_BUTTON_ITEM_WIN);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 23
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00223280.
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
int init_OptionButton(void)
{
    option_button_finder = 0;
    return OptionSubInitStep(OptionButtonAnimStart, OptionButtonCfgDispUpdate,
                             OptionButtonLayoutFile(), "in");
}

// FF3 0x002232d8.
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
int one_OptionButton(void)
{
    OPTION_WRK *opt;

    OptionButtonCfgDispUpdate();
    if (!BpCtlIsLoadEnd(OptionButtonLayoutFile()))
        return 0;

    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], OPTION_BUTTON_SE_OK, 0))
    {
        if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], OPTION_BUTTON_SE_CANCEL, 0))
        {
            if (CheckPadTrgPlaySe(OPTION_BUTTON_PAD_SQUARE, OPTION_BUTTON_SE_OK, 0))
            {
                const char *label;

                if (option_button_finder != 0)
                {
                    option_button_finder = 0;
                    label = "normal";
                }
                else
                {
                    option_button_finder = 1;
                    label = "finder";
                }
                BpPlayLabel(BpCtlGetHandleByFileNo(OptionButtonLayoutFile()), label);
                return 0;
            }

            // Left (0x8000) / right (0x2000), repeated: the layout 0..2, wrapping.
            if (!CheckPadRptPlaySe(0xa000, -1, 0))
                return 0;
            opt = GetOptionWrk();
            opt->now.pad_cfg = (signed char)CsrMoveUpDownSe(opt->now.pad_cfg, 0, 3, 0x2000, 0x8000, 1,
                                                            OPTION_BUTTON_SE_MOVE);
            return 0;
        }
        option_cancel = 1;
    }

    GetGPhaseCtrl()->SetNext(0x13, 0);
    return 1;
}

// Plays the view's out label ("findaer_out" / "nomal_out", FF3's
// spellings) and ends with the fade layer item released.
// FF3 0x002234c8.
// callcheck: GetLanguage, GetSysWrk (OptionButtonLayoutFile)
int end_OptionButton(void)
{
    const char *label = option_button_finder != 0 ? "findaer_out" : "nomal_out";

    if (!OptionSubEndStep(OptionButtonCfgDispUpdate, OptionButtonLayoutFile(), label))
        return 0;
    CmnFadeLayerTermItem();
    return 1;
}
