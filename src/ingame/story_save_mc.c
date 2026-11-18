/* ==========================================================================
 *  ingame/story_save_mc.c
 *
 *  Phase 69, Story_SaveGame_Mc: the memory-card save screen (SAVELOAD_PHF
 *  layout over the menu BGM), opened from the save point menu (41) or the
 *  start menu's option / album pages. The play time stops while it is up.
 *  When the backdrop is the title movie (OutgameBgObjIsUsed) the screen
 *  fades out and in around it. Back to the requesting phase at the end.
 *  init/end step the leaf node's state_cnt; one steps save_mc_step.
 *  Same shape as story_load_mc.c (phase 68).
 *  FF3 0x250268..0x250850. FF3 0x00250830 (StorySaveMcGPhaseCtrlTerm) is not
 *  written: the compiler's atexit thunk for the GPhaseCtrl singleton
 *  (GPhaseCtrlTerm), the file-end marker.
 * ======================================================================== */
#include "ingame/story_save_mc.h"

#include "ingame/ingame.h"                              // IngameSaveInit/End, GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                          // PlayTimeGetStop, PlayTimeSetStop
#include "ingame/menu/anim_2d.h"                        // LayoutAnimMain
#include "main/gphase.h"                                // GetGPhaseCtrl
#include "outgame/save_scr.h"                           // SaveScr*
#include "outgame/outgame_bg.h"                         // OutgameBg*, OutgameBgm*, OutgameBgObj*
#include "save_load/prg/game_data_save.h"               // McSaveGame*, GameDataSaveEnd
#include "system/eeiop/cd_file_dat.h"                   // BGM_03_MENU3_STR_STR
#include "graphics/graph2d/fade.h"

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define SAVE_MC_PHASE               0x45                // 69 Story_SaveGame_Mc
#define SAVE_MC_FROM_OPTION_SAVE    0x6e                // 110 (under 81 StartMenu_Option)
#define SAVE_MC_FROM_ALBUM_SAVE     0x73                // 115 StartMenu_MissionSave (under 83 StartMenu_Mission)

static s_char save_mc_step;                             // sdata 0x3738f8 (gp+0x408): one's step
static int    save_mc_fade;                             // sdata 0x3738fc (gp+0x40c): fade around the screen
static int    save_mc_play_stop;                        // sdata 0x373900 (gp+0x410): PlayTimeGetStop on entry

// ──────────────────────────────────────────────────────────────────────
// Prepare the save, stop the play time, and with the title movie as the
// backdrop fade out and show it.
// FF3 0x00250268.

int init_Story_SaveGame_Mc(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (IngameSaveInit() == 0)
            return 0;

        save_mc_step = 0;
        save_mc_fade = 0;
        if (OutgameBgObjIsUsed())
            save_mc_fade = 1;
        save_mc_play_stop = PlayTimeGetStop();
        PlayTimeSetStop(1);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1 && save_mc_fade)
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
// Finish the save; with the movie, hide it between a fade out and a fade
// in. The play time runs again as it did before.
// FF3 0x00250440.

int end_Story_SaveGame_Mc(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (IngameSaveEnd() == 0)
            return 0;
        if (save_mc_fade)
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

    PlayTimeSetStop(save_mc_play_stop);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The save screen, one step chain per frame (several steps can pass in one
// frame):
//   0 memory-card init       1-2 load the SAVELOAD screen, menu BGM, fade in
//   3-4 backdrop on, screen "in"
//   5 run the memory-card save; when done BGM off, backdrop off (kept for
//     the start menu's option / album saves), fade out
//   6 screen "out", release  7 memory-card end, back to the requesting phase.
// FF3 0x00250608.

int one_Story_SaveGame_Mc(void)
{
    LayoutAnimMain();

    if (save_mc_step == 0)
    {
        McSaveGameInit();
        save_mc_step = 1;
    }

    if (save_mc_step == 1)
    {
        SaveScrCreate(SAVE_MC_PHASE);
        save_mc_step = 2;
    }

    if (save_mc_step == 2 && SaveScrIsLoaded())
    {
        OutgameBgmPlay(BGM_03_MENU3_STR_STR);
        save_mc_step = 3;
        if (save_mc_fade)
            FadeInReq(0, 0, 0, 10);
    }

    if (save_mc_step == 3)
    {
        OutgameBgEnable();
        SaveScrIn();
        save_mc_step = 4;
    }

    if (save_mc_step == 4 && SaveScrIsInEnd())
        save_mc_step = 5;

    if (save_mc_step == 5)
    {
        if (McSaveGameMain())
        {
            short from;

            OutgameBgmStop();
            from = GetGPhaseCtrl()->from;
            if (from != SAVE_MC_FROM_ALBUM_SAVE && from != SAVE_MC_FROM_OPTION_SAVE)
                OutgameBgDisable();
            SaveScrOut();
            save_mc_step = 6;
            if (save_mc_fade)
                FadeOutReq(0, 0, 0, 10);
        }
        SaveScrMain();
    }

    if (save_mc_step == 6 && SaveScrOutWait())
    {
        SaveScrRelease();
        save_mc_step = 7;
    }

    if (save_mc_step != 7)
        return 0;

    GameDataSaveEnd();
    GPhaseCtrlRequest(GetGPhaseCtrl()->from, 0);
    return 1;
}
