/* ==========================================================================
 *  ingame/story_load_mc.c
 *
 *  Phase 68, Story_LoadGame_Mc: the memory-card load screen (SAVELOAD_PHF
 *  layout over the menu BGM). Two ways in:
 *  - from the title (the requesting phase is 29, Story_Start): the
 *    outgame backdrop movie (OUTGAME_BG_PSS) plays behind the screen and
 *    the fade layer is driven here; a cancelled load returns to phase 17
 *    (Title_Menu);
 *  - in game: the screen fades out/in around the outgame backdrop object
 *    (when OutgameBgObjIsUsed), and returns to the requesting phase.
 *  init/end step the leaf node's state_cnt; one steps load_mc_step.
 *  FF3 0x1f0e08..0x1f1600. FF3 0x001f15e0 is not written: the compiler's
 *  atexit thunk for the GPhaseCtrl singleton (GPhaseCtrlTerm), the file-end
 *  marker.
 * ======================================================================== */
#include "ingame/story_load_mc.h"

#include "common/cmn_fade_layer.h"                      // GetCmnFadeLayer, capCtl::Update
#include "ingame/ingame.h"                              // IngameLoadWait/End, GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                          // title_start_flg
#include "ingame/menu/anim_2d.h"                        // LayoutAnimMain
#include "main/gphase.h"                                // GetGPhaseCtrl
#include "outgame/outgame_bg.h"                         // OutgameBg*, OutgameBgm*, OutgameBgObj*
#include "outgame/save_load_scr.h"                      // SaveLoadScr*
#include "save_load/prg/game_data_load.h"               // McLoadGame*
#include "system/b2d/BpCtl.h"                           // BpCtlMain
#include "system/eeiop/cd_file_dat.h"                   // OUTGAME_BG_PSS, BGM_03_MENU3_STR_STR
#include "graphics/graph2d/fade.h"

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static s_char load_mc_step;                             // sdata 0x373490 (gp-0x60): one's step
static int    load_mc_bg;                               // sdata 0x373494 (gp-0x5c): from the title, backdrop movie
static int    load_mc_fade;                             // sdata 0x373498 (gp-0x58): in game, fade around the screen

// ──────────────────────────────────────────────────────────────────────
// Wait for the file loads, choose the way in, and in game fade out and show
// the outgame backdrop object.
// FF3 0x001f0e08.

int init_Story_LoadGame_Mc(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (IngameLoadWait() == 0)
            return 0;

        load_mc_step = 0;
        load_mc_bg = 0;
        load_mc_fade = 0;
        if (GetGPhaseCtrl()->from == 0x1d)              // Story_Start: the title's "load"
            load_mc_bg = 1;
        else if (OutgameBgObjIsUsed())
            load_mc_fade = 1;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1 && load_mc_fade)
    {
        FadeOutReq(0, 0, 0, 10);
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 2)
    {
        if (GetFadeState() == 0)
            return 0;
        OutgameBgObjShow();
        GetGPhaseCtrl()->leaf->state_cnt = 3;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 3)
    {
        if (OutgameBgObjIsShown() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 4;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Restore the loaded game; in game, hide the backdrop object between a
// fade out and a fade in.
// FF3 0x001f1008.

int end_Story_LoadGame_Mc(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (IngameLoadEnd() == 0)
            return 0;
        if (load_mc_fade)
            GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1)
    {
        if (GetFadeState() == 0)
            return 0;
        OutgameBgObjHide();
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 2)
    {
        if (OutgameBgObjIsReady() == 0)
            return 0;
        FadeInReq(0, 0, 0, 10);
        GetGPhaseCtrl()->leaf->state_cnt = 3;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 3)
    {
        if (GetFadeState() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 4;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The load screen, one step chain per frame (several steps can pass in one
// frame):
//   0 memory-card init;            1-2 (title) start/load the backdrop movie
//   3-4 load the SAVELOAD screen, menu BGM, in game fade in
//   5-7 (title) backdrop object up  8-9 backdrop on, screen "in"
//   10 run the memory-card load     11 (title) BGM and movie off
//   12 screen "out", release        13-14 (title) release the movie
//   15 memory-card end, back to the requesting phase (title, load not
//      taken: phase 17 Title_Menu).
// FF3 0x001f11c8.

int one_Story_LoadGame_Mc(void)
{
    LayoutAnimMain();

    if (load_mc_step == 0)
    {
        McLoadGameInit();
        load_mc_step = load_mc_bg ? 1 : 3;
    }

    if (load_mc_step == 1)
    {
        OutgameBgStart(0, OUTGAME_BG_PSS, -1);
        load_mc_step = 2;
    }

    if (load_mc_step == 2 && OutgameBgIsLoaded() && OutgameBgLoadWait())
        load_mc_step = 3;

    if (load_mc_step == 3)
    {
        SaveLoadScrCreate(0x44);                        // phase 68
        load_mc_step = 4;
    }

    if (load_mc_step == 4 && SaveLoadScrIsLoaded())
    {
        OutgameBgmPlay(BGM_03_MENU3_STR_STR);
        load_mc_step = load_mc_bg ? 5 : 8;
        if (load_mc_fade)
            FadeInReq(0, 0, 0, 10);
    }

    if (load_mc_step == 5 && OutgameBgObjIsReady())
    {
        OutgameBgDraw();
        OutgameBgObjShow();
        load_mc_step = 6;
    }

    if (load_mc_step == 6 && OutgameBgObjIsShown())
        load_mc_step = 7;

    if (load_mc_step == 7)
        load_mc_step = 8;

    if (load_mc_step > 6 && load_mc_bg)
        OutgameBgDraw();

    if (load_mc_step == 8)
    {
        OutgameBgEnable();
        SaveLoadScrIn();
        load_mc_step = 9;
    }

    if (load_mc_step == 9)
    {
        int ready = 1;

        if (load_mc_bg)
            ready = OutgameBgIsReady() != 0;
        if (SaveLoadScrIsInEnd() == 0)
            ready = 0;
        if (ready)
            load_mc_step = 10;
    }

    if (load_mc_step == 10)
    {
        if (McLoadGameMain())
        {
            OutgameBgDisable();
            if (load_mc_bg)
            {
                load_mc_step = 11;
            }
            else
            {
                OutgameBgmStop();
                load_mc_step = 12;
            }
            if (load_mc_fade)
                FadeOutReq(0, 0, 0, 10);
        }
        SaveLoadScrMain();
    }

    if (load_mc_step == 11)
    {
        OutgameBgmStop();
        OutgameBgStop();
        load_mc_step = 12;
    }

    if (load_mc_step == 12)
    {
        int done = 1;

        if (load_mc_bg)
            done = OutgameBgIsStopped() != 0;
        if (SaveLoadScrOutWait() == 0)
            done = 0;
        if (done)
        {
            SaveLoadScrRelease();
            load_mc_step = load_mc_bg ? 13 : 15;
        }
    }

    if (load_mc_step == 13)
        load_mc_step = 14;

    if (load_mc_step == 14)
    {
        OutgameBgRelease();
        load_mc_step = 15;
    }

    if (load_mc_bg)
    {
        GetCmnFadeLayer()->Update();
        BpCtlMain();
    }

    if (load_mc_step != 15)
        return 0;

    McLoadGameEnd();
    if (load_mc_bg == 0 || title_start_flg.load != 0)
        GPhaseCtrlRequest(GetGPhaseCtrl()->from, 0);
    else
        GPhaseCtrlRequest(0x11, 0);                     // Title_Menu
    return 1;
}
