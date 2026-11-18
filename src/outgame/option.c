/* ==========================================================================
 *  outgame/option.c
 *
 *  The option work and phase 7, Option_Main. FF3 0x224618..0x2254b8 (FF2
 *  outgame/option.c by OptionInit; the rest is FF3's own): the work's
 *  defaults and saved copies, the option menu cursors and background, the
 *  init/end step helpers of the option screens (phases 19..23, each in its
 *  own file before this one), text and cursor helpers many menus share, the
 *  sound settings, and phase 7 itself.
 *
 *  FF3 reads the singletons through gp-relative cached pointers; nothing
 *  depends on the cache, so calls go to the getters directly. FF3 0x00225498
 *  is the GPhaseCtrl atexit thunk that ends the file (compiler-generated).
 * ======================================================================== */
#include "outgame/option.h"

#include <string.h>                     // memcmp

#include "common/cmn_fade_layer.h"
#include "graphics/graph2d/message.h"   // MsgPrintData, MsgGetPageBreakNum, GetMsgDataAddr
#include "graphics/gs_reg.h"            // ScreenCaptureDisp
#include "graphics/motion/anicode.h"    // GetLabelSuffixType, LabelAlignGetX
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "common/WindowMsg.h"           // CWindowMsg
#include "ingame/menu/anim_2d.h"
#include "ingame/menu/menu_csr.h"       // MenuCsrInit, MenuCsrRelease
#include "ingame/sound/snd_ctrl.h"      // SndSetVolume, SndSetMonoMode
#include "main/gphase.h"
#include "outgame/outgame_bg.h"       // OutgameBg*
#include "outgame/option_top.h"       // OptionTopAnimStart (phase 19)
#include "system/b2d/B2dCtl.h"
#include "system/b2d/B2dSpr.h"
#include "system/b2d/BpCtl.h"
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

int      option_cancel;                 // sdata 0x3717c8
int      option_from_title;             // sdata 0x3717cc
int      option_from_phase;             // sdata 0x3717d0
CWindowMsg *option_sel_win;                // sdata 0x3717d8

static int option_pad_1a8;              // sdata 0x373764: pad 0's stick_dpad while phase 7 runs

static int OptionLayoutFile(void)
{
    return GetSysWrk()->GetLanguage() + OPTION_LAYOUT_FILE;
}

// ──────────────────────────────────────────────────────────────────────
// The option work
// ──────────────────────────────────────────────────────────────────────

// Save block 17 (save_data.c): the live settings. FF3 0x00224618.
int OptionWrkSaveGet(void **addr)
{
    *addr = GetOptionWrk();
    return sizeof(OPTION_DAT);
}

// FF3 0x00224670.
void OptionInit(OPTION_WRK *opt)
{
    OPTION_DAT *d = &opt->now;

    d->b13      = 0;
    d->vol_ch0  = 10;
    d->subtitle = 1;
    d->b00      = 6;
    d->vol_ch1  = 10;
    d->vibration = 1;
    d->pad_cfg  = 0;
    d->b09      = 0;
    d->mono     = 0;
    d->b11      = 0;
    d->b12      = 0;
}

// FF3 0x002246b0.
int OptionIsSameAsSaved(OPTION_WRK *opt, int slot)
{
    return memcmp(&opt->now, &opt->saved[(signed char)slot], sizeof(OPTION_DAT)) == 0;
}

// FF3 0x002246f0.
void OptionLoadSaved(OPTION_WRK *opt, int slot)
{
    opt->now = opt->saved[(signed char)slot];
}

// The cursors of the option screens. FF3 0x00224740.
void OptionMenuCsrInit(void)
{
    MenuCsrInit(0x13, 1, 6, 0);
    MenuCsrInit(0x14, 1, 5, 0);
    MenuCsrInit(0x16, 1, 4, 0);
    MenuCsrInit(0x17, 3, 1, 0);
}

// FF3 0x002247a0.
void OptionMenuCsrRelease(void)
{
    MenuCsrRelease(0x13);
    MenuCsrRelease(0x14);
    MenuCsrRelease(0x16);
    MenuCsrRelease(0x17);
}

// ──────────────────────────────────────────────────────────────────────
// The background behind the option screens: the title movie object when
// opened from the title menu, otherwise background type 2 (file by
// ingame_wrk.b03).
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002247d0.
void OptionBgStart(void)
{
    int type = 0;
    int file;
    int arg;

    if (option_from_title == 0)
    {
        type = 2;
        if (ingame_wrk.b03 != 1)
        {
            file = 0x11c4;
            arg  = 0x3fb;
        }
        else
        {
            file = 0x11c5;
            arg  = 0x3f0;
        }
    }
    else
    {
        file = 0xa4a;
        arg  = -1;
    }
    OutgameBgStart(type, file, arg);
}

// FF3 0x00224828.
int OptionBgLoadWait(void)
{
    return OutgameBgIsLoaded() && OutgameBgLoadWait();
}

// FF3 0x00224860.
int OptionBgIsReady(void)
{
    OutgameBgEnable();
    if (option_from_title != 0)
        return OutgameBgIsReady();
    return 1;
}

// FF3 0x00224898.
void OptionBgStop(void)
{
    OutgameBgStop();
    OutgameBgDisable();
}

// FF3 0x002248b8.
int OptionBgIsStopped(void)
{
    return OutgameBgIsStopped();
}

// callcheck: GetLanguage, GetSysWrk (OptionLayoutFile)
// FF3 0x002248d0.
void OptionRelease(void)
{
    OutgameBgRelease();
    BpCtlRelease(BpCtlGetHandleByFileNo(OptionLayoutFile()));
}

// ──────────────────────────────────────────────────────────────────────
// Shared text / cursor helpers
// ──────────────────────────────────────────────────────────────────────

// Moves pos to text item 'label' of layout 'file': labels of suffix type 1
// add the item's scaled part width (parts 3 * 0xc * 8 / 1024), and rect (if
// given) adds its aligned x and rect[1]. FF3 0x00224918.
void LayoutItemAddPos(float *pos, const char *label, int file, int *rect)
{
    int       handle = BpCtlGetHandleByFileNo(file);
    int       type   = GetLabelSuffixType(label);
    B2D_ITEM *item   = B2dCtlFindItemByName(handle, label);

    if (type == 1)
    {
        short p3 = (short)B2dSprGetPartData(item, 3);
        short pc = (short)B2dSprGetPartData(item, 0xc);
        short p8 = (short)B2dSprGetPartData(item, 8);

        pos[0] = pos[0] + (float)(p3 * pc) * ((float)p8 * (1.0f / 1024.0f));   // float 0x3717dc
    }

    if (rect != nullptr)
    {
        pos[0] = pos[0] + (float)LabelAlignGetX(type, rect);
        pos[1] = pos[1] + (float)rect[1];
    }
}

// Prints message (msg0, msg1) at pos; messages with page breaks print their
// current page. FF3 0x00224a48.
void PrintMsgPos(int msg0, int msg1, float *pos, int a3, int a4, int a5, int a6)
{
    if (MsgGetPageBreakNum(msg0, msg1) == 0)
        MsgPrintData(GetMsgDataAddr(msg0, msg1), (int)pos[0], (int)pos[1], a3, a4, a5, 0, 0, a6);
    else
        MsgPrintPage(msg0, msg1, (int)pos[0], (int)pos[1], a3, a4, a5, 0, 0, a6);
}

// Steps a cursor in [min, min + count) on pad 0's repeat bits: mask_inc
// moves forward, mask_dec back; at either end it wraps when 'wrap' is set,
// else it stops. Plays se (if >= 0) when the cursor moved.
// FF3 0x00224b40.
int CsrMoveUpDownSe(int pos, int min, int count, u_short mask_inc, u_short mask_dec,
                    int wrap, int se)
{
    int end  = min + count;
    int next = pos;

    if (CheckPadRptPlaySe(mask_inc, -1, 0))
    {
        next = pos + 1;
        if (next >= end)
            next = wrap ? min : end - 1;
    }
    else if (CheckPadRptPlaySe(mask_dec, -1, 0))
    {
        next = pos - 1;
        if (next < min)
            next = wrap ? end - 1 : min;
    }

    if (next != pos && se >= 0)
        SndSePlay(se, 100, 0x1000, 0, 0, 0);
    return next;
}

// ──────────────────────────────────────────────────────────────────────
// Option-screen steps. The screens' init: state 0 saves the settings to
// slot 0 and loads 'file', 1 waits for its data, calls on_ready and plays
// 'label', 2 calls on_wait until the layout reports load end. The end plays
// 'label', calls on_wait until load end, releases the layout and, when
// option_cancel is set, restores slot 0.
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00224c38.
int OptionSubInitStep(void (*on_ready)(void), void (*on_wait)(void), int file, const char *label)
{
    GPHASE_NODE *leaf = GetGPhaseCtrl()->leaf;

    if (leaf->state_cnt == 0)
    {
        OPTION_WRK *opt = GetOptionWrk();

        option_cancel  = 0;
        opt->saved[0]  = opt->now;
        BpCtlLoadReq(file, nullptr);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }

    if (leaf->state_cnt == 1)
    {
        if (!BpCtlIsDataReady(BpCtlGetHandleByFileNo(file)))
            return 0;
        if (on_ready != nullptr)
            on_ready();
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        GetGPhaseCtrl()->leaf->state_cnt = 2;
        return 0;
    }

    if (on_wait != nullptr)
        on_wait();
    return BpCtlIsLoadEnd(file) ? 1 : 0;
}

// FF3 0x00224dd0.
int OptionSubEndStep(void (*on_wait)(void), int file, const char *label)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        BpPlayLabel(BpCtlGetHandleByFileNo(file), label);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (on_wait != nullptr)
        on_wait();

    if (!BpCtlIsLoadEnd(file))
        return 0;

    BpCtlRelease(BpCtlGetHandleByFileNo(file));
    if (option_cancel != 0)
        OptionLoadSaved(GetOptionWrk(), 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Common fade layer wrappers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00224ec8.
void CmnFadeLayerShowItem(int label, int handle, int id)
{
    capCtl *layer = GetCmnFadeLayer();

    layer->Show(label);
    layer->SetItem(handle, id);
}

// FF3 0x00224f28.
void CmnFadeLayerSwitchItem(int label)
{
    GetCmnFadeLayer()->Switch(label, 0x1e);
}

// FF3 0x00224f58.
void CmnFadeLayerTermItem(void)
{
    GetCmnFadeLayer()->Term();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 7, Option_Main: the container of the option screens (19 top menu,
// 20..23). Opened from the title menu (17) or an in-game menu; the opener
// is kept in option_from_phase for the screens' way back.
// ──────────────────────────────────────────────────────────────────────

// Steps: 0 saves the settings to slot 1, forces pad 0's stick_dpad on (left stick moves the cursor), sets up
// cursors and background and opens a selection window; 1 waits for the
// background and loads the option layout; 2 waits for window and layout;
// 3 waits until the background is ready and starts the top menu animation.
// callcheck: GetLanguage, GetSysWrk (OptionLayoutFile)
// callcheck: operator_new, SelWinCtor (new CWindowMsg)
// FF3 0x00224f78.
int init_Option_Main(void)
{
    switch (GetGPhaseCtrl()->leaf->state_cnt)
    {
    case 0:
    {
        OPTION_WRK *opt  = GetOptionWrk();
        short       from;

        opt->saved[1] = opt->now;
        option_pad_1a8 = pad_wrk[0].stick_dpad;
        pad_wrk[0].stick_dpad = 1;
        OptionMenuCsrInit();

        from = GetGPhaseCtrl()->from;
        option_from_phase = from;
        option_from_title = from == 0x11;
        if (from == 0x11)
            Anim2dInit();
        OptionBgStart();

        option_sel_win = nullptr;
        option_sel_win = new CWindowMsg;           // operator_new(0x1c) + SelWinCtor
        option_sel_win->Open();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }

    case 1:
        if (!OptionBgLoadWait())
            return 0;
        BpCtlLoadReq(OptionLayoutFile(), nullptr);
        GetGPhaseCtrl()->leaf->state_cnt = 2;
        return 0;

    case 2:
        if (!option_sel_win->IsReady())
            return 0;
        if (!BpCtlIsDataReady(BpCtlGetHandleByFileNo(OptionLayoutFile())))
            return 0;
        option_sel_win->SetAlpha(0);
        option_sel_win->SetFlags(1, 1, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 3;
        return 0;

    default:
        if (!OptionBgIsReady())
            return 0;
        OptionTopAnimStart();
        return 1;
    }
}

// FF3 0x00225208.
void pre_Option_Main(void)
{
    if (option_from_title == 0)
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    OutgameBgDraw();
    GetCmnFadeLayer()->Update();
    if (option_sel_win != nullptr)
    {
        option_sel_win->Main();
        LayoutAnimMain();
    }
    BpCtlMain();
}

// FF3 0x00225270.
int one_Option_Main(void)
{
    GetGPhaseCtrl()->SetNext(0x13, 0);    // 19: the option top menu
    return 1;
}

// FF3 0x002252b0.
void after_Option_Main(void)
{
    OptionApplySound();
}

// FF3 0x002252c8.
int end_Option_Main(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        OptionBgStop();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (!OptionBgIsStopped())
        return 0;

    OptionRelease();
    option_sel_win->Close();
    delete option_sel_win;                      // vtable +4: deleting destructor
    CmnFadeLayerTermItem();
    pad_wrk[0].stick_dpad = option_pad_1a8;
    OptionMenuCsrRelease();
    if (option_from_title != 0)
        LayoutAnimResetAll();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sound settings
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002253a0.
void OptionApplySound(void)
{
    SndSetVolume(0, OptionGetSndVolume0());
    SndSetVolume(1, OptionGetSndVolume1());
    SndSetMonoMode(GetOptionWrk()->now.mono);
}

// FF3 0x002253f8.
int OptionGetSndVolume0(void)
{
    return (GetOptionWrk()->now.vol_ch0 << 8) / 19;
}

// FF3 0x00225440.
int OptionGetSndVolume1(void)
{
    return (GetOptionWrk()->now.vol_ch1 << 8) / 19;
}

// ──────────────────────────────────────────────────────────────────────
// The option work singleton (bss 0x41e580), set to defaults by OptionInit
// on first use (FF3 registers an atexit destructor 0x161f00 with it).
// callcheck: atexit (the host static's destructor)
// FF3 0x002b0310.

OPTION_WRK *GetOptionWrk(void)
{
    // layout: host -- wrapper that runs OptionInit on first use, not FF3 data.
    struct OPTION_WRK_INST
    {
        OPTION_WRK wrk;
        OPTION_WRK_INST() { OptionInit(&wrk); }
    };
    static OPTION_WRK_INST inst;
    return &inst.wrk;
}
