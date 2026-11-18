/* ==========================================================================
 *  outgame/autoload_scr.c
 *
 *  The boot memory-card auto load's screen (see autoload_scr.h). FF3
 *  0x130210..0x130638, FF3-only: FF2's autoload.c drew its own two-line
 *  window and yes/no selector (DrawCmnTwoLineWindow / DrawCmnYesNoSel) from
 *  AutoLoadDispMain; FF3 shows the autoload's messages in the yes/no layout
 *  window and drives it from here.
 * ======================================================================== */
#include "outgame/autoload_scr.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/motion/anicode.h"    // YesNoSetMsg, YesNoSetSel, YesNoReqClose
#include "ingame/menu/anim_2d.h"        // LayoutAnim*
#include "outgame/autoload.h"           // auto_load_ctrl, AL_STEP_*

// Layout anim states (anim_2d.h LAYOUT_ANIM.state).
#define LA_STATE_OPEN    2
#define LA_STATE_CLOSING 3
#define LA_STATE_CLOSED  4

#define YESNO_SEL_NONE   2              // YesNoSetSel: no choices shown

static int    auto_load_scr_anim;       // sdata 0x372cfc (gp-0x7f4): window slot, -1: none
static int    auto_load_scr_ask;        // sdata 0x372d00 (gp-0x7f0): 1 while a question shows (kept, never read elsewhere)
static u_char auto_load_scr_alpha;      // sdata 0x372d04 (gp-0x7ec): backdrop alpha 0..0x80

// 1 in the two yes/no question steps.
// FF3 0x00130210.
static int AutoLoadScrIsAskStep(void)
{
    return auto_load_ctrl.mc_step == AL_STEP_DEF_START
        || auto_load_ctrl.mc_step == AL_STEP_EMPTY_ASK;
}

// Keeps the window open (starts it again once it has closed) and shows the
// autoload's current message; the choices only in the question steps.
// FF3 0x00130238.
static void AutoLoadScrMsgUpdate(void)
{
    if (auto_load_scr_anim != -1 && LayoutAnimGetState(auto_load_scr_anim) == LA_STATE_CLOSED)
    {
        LayoutAnimReset(auto_load_scr_anim);
        auto_load_scr_anim = -1;
    }
    if (auto_load_scr_anim == -1)
    {
        auto_load_scr_anim = LayoutAnimStart(AUTO_LOAD_SCR_ANIM, 0);
        if (auto_load_scr_anim == -1)
            return;
    }

    if (AutoLoadScrIsAskStep())
    {
        if (auto_load_scr_ask == 0)
            auto_load_scr_ask = 1;
        YesNoSetMsg(AUTO_LOAD_SCR_MSG_KIND, auto_load_ctrl.msg_id);
        YesNoSetSel(auto_load_ctrl.csr);
    }
    else
    {
        if (auto_load_scr_ask == 1)
            auto_load_scr_ask = 0;
        YesNoSetMsg(AUTO_LOAD_SCR_MSG_KIND, auto_load_ctrl.msg_id);
        YesNoSetSel(YESNO_SEL_NONE);
    }
}

// FF3 0x00130310.
static void AutoLoadScrMsgInit(void)
{
    auto_load_scr_anim = -1;
    auto_load_scr_ask = AutoLoadScrIsAskStep() ? 1 : 0;
    AutoLoadScrMsgUpdate();
}

// FF3 0x00130348.
static int AutoLoadScrMsgIsReady(void)
{
    return LayoutAnimIsDataReady(auto_load_scr_anim) != 0;
}

// 0 while the window plays its close.
// FF3 0x00130370.
static int AutoLoadScrMsgIsNotClosing(void)
{
    if (auto_load_scr_anim != -1 && LayoutAnimGetState(auto_load_scr_anim) == LA_STATE_CLOSING)
        return 0;
    return 1;
}

// FF3 0x001303b0.
static void AutoLoadScrMsgReset(void)
{
    if (auto_load_scr_anim != -1)
    {
        LayoutAnimReset(auto_load_scr_anim);
        auto_load_scr_anim = -1;
    }
}

// FF3 0x001303e8.
int AutoLoadScrMsgIsOpen(void)
{
    if (auto_load_scr_anim != -1 && LayoutAnimGetState(auto_load_scr_anim) != LA_STATE_OPEN)
        return 0;
    return 1;
}

// Backdrop alpha up by 0x20 a frame; 1 once at 0x80.
// FF3 0x00130428.
static int AutoLoadScrBackFadeIn(void)
{
    if (auto_load_scr_alpha + 0x20 < 0x80)
    {
        auto_load_scr_alpha += 0x20;
        GetCmnFadeLayer()->base_alpha = auto_load_scr_alpha;
        return 0;
    }
    auto_load_scr_alpha = 0x80;
    GetCmnFadeLayer()->base_alpha = 0x80;
    return 1;
}

// Backdrop alpha down by 0x10 a frame; 1 once at 0.
// FF3 0x00130488.
static int AutoLoadScrBackFadeOut(void)
{
    if (auto_load_scr_alpha - 0x10 > 0)
    {
        auto_load_scr_alpha -= 0x10;
        GetCmnFadeLayer()->base_alpha = auto_load_scr_alpha;
        return 0;
    }
    auto_load_scr_alpha = 0;
    GetCmnFadeLayer()->base_alpha = 0;
    return 1;
}

// FF3 0x001304e0.
void AutoLoadScrInit(void)
{
    AutoLoadScrMsgInit();
    auto_load_scr_alpha = 0;
    GetCmnFadeLayer()->base_alpha = 0;
}

// FF3 passes the layer to CmnFadeLayerIsReady (it reads a global).
// FF3 0x00130508.
int AutoLoadScrIsReady(void)
{
    if (!AutoLoadScrMsgIsReady())
        return 0;
    if (!CmnFadeLayerIsReady())
        return 0;
    GetCmnFadeLayer()->Show(AUTO_LOAD_SCR_FADE_LABEL);
    return 1;
}

// FF3 0x00130560.
int AutoLoadScrIsOpen(void)
{
    return 1;
}

// The backdrop darkens behind the messages that wait for OK (except
// message 0x38) and clears otherwise.
// FF3 0x00130568.
void AutoLoadScrMain(void)
{
    AutoLoadScrMsgUpdate();
    if (auto_load_ctrl.mc_step == AL_STEP_EMPTY_ERROR
        || (auto_load_ctrl.mc_step == AL_STEP_ERROR_CONF && auto_load_ctrl.msg_id != AL_MSG_NO_BACKDROP))
    {
        AutoLoadScrBackFadeIn();
        return;
    }
    AutoLoadScrBackFadeOut();
}

// FF3 0x001305c0.
void AutoLoadScrClose(void)
{
    GetCmnFadeLayer()->Hide(0x1e);
    YesNoReqClose(1);
}

// FF3 ignores both results: phase 14 goes on the next frame.
// FF3 0x001305f0.
int AutoLoadScrIsClosed(void)
{
    AutoLoadScrBackFadeOut();
    AutoLoadScrMsgIsNotClosing();
    return 1;
}

// FF3 0x00130618.
void AutoLoadScrEnd(void)
{
    GetCmnFadeLayer()->base_alpha = 0;
    AutoLoadScrMsgReset();
}
