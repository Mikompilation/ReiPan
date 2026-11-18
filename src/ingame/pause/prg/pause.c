/* ==========================================================================
 *  ingame/pause/prg/pause.c
 *
 *  Phase 44, Story_Pause (see pause.h). FF3 0x225dc8..0x226138; the file
 *  keeps only FF2's phase functions, the menu itself moved into the layout
 *  animation system.
 * ======================================================================== */
#include "ingame/pause/prg/pause.h"

#include "common/cmn_fade_layer.h"
#include "graphics/gs_reg.h"            // ScreenCaptureSave, ScreenCaptureDisp
#include "graphics/motion/anicode.h"    // PauseMenuGetSel, PauseMenuSetSel
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"          // title_start_flg
#include "ingame/menu/anim_2d.h"
#include "ingame/mission/ExtraMissionShelt.h"  // GetExtraMissionShelt
#include "ingame/plyr/player.h"         // PlyrSetB260Mode2
#include "ingame/sound/snd_ctrl.h"      // SndAllBufStop, SndBgmStopAll
#include "main/gphase.h"
#include "main/gphase_tree.h"           // GPHASE_TREE::Reset
#include "album/prg/album.h"             // AlbumSetIngame
#include "system/b2d/BpCtl.h"           // BpCtlMain
#include "system/eeiop/stream_auto.h"   // StreamAutoAllStop
#include "ingame/story_menu.h"  // IsLockTblOn
#include "outgame/option.h"             // PC: OptionBg*, option_from_title
#include "outgame/outgame_bg.h"         // PC: OutgameBgDraw, OutgameBgRelease
#include "reipan/reipan_ui/reipan_rml_options.h"  // PC: the RmlUi options screen
#include "reipan/reipan_ui/reipan_rml_pause.h"  // PC: the RmlUi pause screen

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────



// ──────────────────────────────────────────────────────────────────────

static int pause_anim;                  // sdata 0x3717e0: the menu's layout anim slot, -1: none
static int pause_ret_phase;             // sdata 0x3717e4: phase to resume (GPhaseCtrl from)
static int pause_from_option;           // sdata 0x373784 (gp+0x294): back from phase 7

// PC: the menu itself is the port's RmlUi pause screen (reipan_rml_pause.h)
// when it can open, in place of layout animation 4.  This file still owns the
// phase: the paused frame, and what each choice does, since the screen answers
// in PauseMenuGetSel()'s own codes.  Options is the exception: the RmlUi
// options screen (reipan_rml_options.h) opens over the pause screen without
// an answer, so the phase never leaves; the screen answers 1, phase 7, only
// when that cannot open.  The layout menu runs as it always did whenever
// RmlUi is unavailable, and its Options is phase 7.
static int pause_rml;

// PC: what the game's option screens show behind themselves when they are
// opened from here -- the menu backdrop, its blue lattice, the falling snow
// and the drifting fog (OptionBgStart, type 2) -- runs under the RmlUi
// options screen while it is up, in phase 7's own steps: started, drawn every
// frame (pre_Option_Main), enabled once loaded (init_Option_Main), stopped
// and released once it has played out (end_Option_Main).
enum
{
    PAUSE_OPT_BG_OFF,
    PAUSE_OPT_BG_LOADING,
    PAUSE_OPT_BG_UP,
    PAUSE_OPT_BG_GOING,
};
static int pause_opt_bg;

static void PauseOptionBgStep(void)
{
    const int want = ReiPan_RmlOptionsIsOpen();

    if (pause_opt_bg == PAUSE_OPT_BG_OFF)
    {
        if (!want)
            return;
        option_from_title = 0;          // as init_Option_Main sets it, opened from here
        OptionBgStart();
        pause_opt_bg = PAUSE_OPT_BG_LOADING;
    }

    OutgameBgDraw();

    if (pause_opt_bg == PAUSE_OPT_BG_LOADING)
    {
        if (!OptionBgLoadWait())
            return;
        OptionBgIsReady();              // enables it; ready at once off the title
        pause_opt_bg = PAUSE_OPT_BG_UP;
    }
    if (pause_opt_bg == PAUSE_OPT_BG_UP)
    {
        if (want)
            return;
        OptionBgStop();
        pause_opt_bg = PAUSE_OPT_BG_GOING;
    }
    if (OptionBgIsStopped())
    {
        OutgameBgRelease();
        pause_opt_bg = PAUSE_OPT_BG_OFF;
    }
}

// FF3 0x00225dc8.
int IsPauseEnable(void)
{
    if (IsLockTblOn(0x10))
        return 0;
    return !LayoutAnimIsAnyActive();
}

// FF3 0x00225e00.
int init_Story_Pause(void)
{
    pause_rml = ReiPan_RmlPauseOpen(REIPAN_RML_PAUSE_CONTINUE);  // PC
    pause_anim = pause_rml != 0 ? -1 : LayoutAnimStart(4, 0);
    PauseMenuSetSel(0);
    pause_ret_phase = GetGPhaseCtrl()->from;
    pause_from_option = 0;
    ScreenCaptureSave(2, 1);
    return 1;
}

// FF3 0x00225e68.
int end_Story_Pause(void)
{
    // PC: whatever ended the phase, the RmlUi pause screen goes with it.
    ReiPan_RmlPauseClose();
    pause_rml = 0;

    GetCmnFadeLayer()->Term();
    // PC: no layout menu ran under the RmlUi screen, and a slot of -1 would
    // reset every layout animation.
    if (pause_anim >= 0)
        LayoutAnimReset(pause_anim);
    pause_anim = -1;
    PlyrSetB260Mode2();
    return 1;
}

// The captured game screen is drawn darker as the fade layer comes in; once
// the menu animation reaches state 2 the fade layer shows label 15 tied to
// item 0x3eb of the menu layout. On state 4 the choice is carried out.
// FF3 0x00225ea8.
int one_Story_Pause(void)
{
    capCtl *layer;
    u_char          c;
    int             state;

    if (pause_from_option != 0)
    {
        // Back from the options: the menu restarts on its options entry.
        pause_rml = ReiPan_RmlPauseOpen(REIPAN_RML_PAUSE_OPTIONS);  // PC
        pause_anim = pause_rml != 0 ? -1 : LayoutAnimStart(4, 0);
        PauseMenuSetSel(1);
        GetGPhaseTree()->Reset();
        pause_from_option = 0;
    }

    layer = GetCmnFadeLayer();
    c = (u_char)(0x80 - (int)((float)layer->alpha * 0.4f));    // float 0x3717e8
    ScreenCaptureDisp(c, c, c, 0x80);

    if (pause_rml != 0)
    {
        // PC: the RmlUi screen draws its own panel over the paused frame and
        // answers once; what follows is the layout menu's state 4.  The
        // options backdrop is drawn before the layouts, as pre_Option_Main
        // does, and has played out before an answer is taken, so the phase
        // never ends with it up.
        PauseOptionBgStep();
        BpCtlMain();
        if (pause_opt_bg != PAUSE_OPT_BG_OFF)
            return 0;
        state = ReiPan_RmlPauseConsumeResult();
        if (state == REIPAN_RML_PAUSE_NONE)
            return 0;
        ReiPan_RmlPauseClose();
        PauseMenuSetSel(state);
        if (state == 2)
        {
            SndBgmStopAll();
            StreamAutoAllStop();
        }
    }
    else
    {
        if (CmnFadeLayerIsReady() && LayoutAnimGetState(pause_anim) == 2)
        {
            GetCmnFadeLayer()->Show(0xf);
            GetCmnFadeLayer()->SetItem(LayoutAnimGetSlot(pause_anim)->handle, 0x3eb);
        }

        GetCmnFadeLayer()->Update();
        LayoutAnimMain();
        BpCtlMain();

        state = LayoutAnimGetState(pause_anim);
        if (LayoutAnimIsStateChanged(pause_anim) && state == 3 && PauseMenuGetSel() == 2)
        {
            SndBgmStopAll();
            StreamAutoAllStop();
        }

        if (state != 4)
            return 0;
    }

    GetCmnFadeLayer()->SetItemHandle(nullptr);
    switch (PauseMenuGetSel())
    {
    case 0:                                 // continue
        GPhaseCtrlRequest((short)pause_ret_phase, 0);
        return 1;

    case 1:                                 // options (PC: the fallback, see pause_rml)
        GPhaseCtrlRequest(7, 1);
        pause_from_option = 1;
        end_Story_Pause();
        return 1;

    case 2:                                 // quit
        SndAllBufStop();
        if (title_start_flg.mode == 2)
        {
            GetExtraMissionShelt()->CarryStore();
            GPhaseCtrlRequest(0xb, 0);
            AlbumSetIngame(1, 0);
            return 1;
        }
        GPhaseCtrlRequest(5, 0);            // Title_Mode
        return 1;

    default:
        return 1;
    }
}
