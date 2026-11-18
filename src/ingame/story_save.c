/* ==========================================================================
 *  ingame/story_save.c
 *
 *  Phase 41, Story_SaveGame: the save point menu. init captures the screen
 *  and starts the outgame backdrop (in the real world, GENJITU_SVPT_BG_PHF
 *  as a layout; else the menu backdrop MENUBG_PHF); one runs the menu
 *  (SAVEPOINT_PHF): each row names the phase it opens (69 Story_SaveGame_Mc
 *  and the others), row phase 5 (Title_Mode) asks yes/no first, cancel goes
 *  back to the phase that requested 41. A row's phase runs as a child
 *  (linked under 41); back from it the tree is reset and the menu opens
 *  again. pre draws the captured screen and the backdrop.
 *  FF3 0x250850..0x250d40, FF3's own code.
 * ======================================================================== */
#include "ingame/story_save.h"

#include "common/cmn_fade_layer.h"                      // GetCmnFadeLayer, capCtl::Update
#include "graphics/gs_reg.h"                            // ScreenCaptureSave, ScreenCaptureDisp
#include "ingame/ingame.h"                              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"                          // ingame_wrk
#include "ingame/menu/anim_2d.h"                        // LayoutAnimMain
#include "ingame/savepoint/savepoint_menu.h"             // SavePointMenu*
#include "main/gphase.h"                                // GetGPhaseCtrl, GetGPhaseTree
#include "main/gphase_tree.h"                           // GPHASE_TREE::Reset
#include "album/prg/album.h"                             // AlbumSetIngame
#include "outgame/outgame_bg.h"                         // OutgameBg*
#include "system/b2d/BpCtl.h"                           // BpCtlMain
#include "system/eeiop/cd_file_dat.h"                   // GENJITU_SVPT_BG_PHF, MENUBG_PHF
#include "graphics/graph2d/fade.h"

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define STORY_SAVE_PHASE        0x29                    // 41 Story_SaveGame
#define STORY_SAVE_PHASE_TITLE  5                       // Title_Mode: the row that asks first

// The backdrop: item hidden in GENJITU_SVPT_BG_PHF / MenuBgIsReady's label.
#define STORY_SAVE_BG_ITEM_GENJITU  0x3f0
#define STORY_SAVE_BG_LABEL_MENU    0x3fb

static int story_save_step;                             // sdata 0x373908 (gp+0x418): one's step
static int story_save_from;                             // sdata 0x37390c (gp+0x41c): phase that requested 41

// ──────────────────────────────────────────────────────────────────────
// Open the save point menu (the save point's request).
// FF3 0x00250850.

void StorySaveGameReq(void)
{
    GPhaseCtrlRequest(STORY_SAVE_PHASE, 0);
    AlbumSetIngame(1, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Capture the screen, start the backdrop and wait until it is shown.
// FF3 0x00250878.

int init_Story_SaveGame(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        ScreenCaptureSave(2, 1);
        FadeInReq(0, 0, 0, 0);
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (ingame_wrk.b03 == 1)
            OutgameBgStart(OUTGAME_BG_LAYOUT, GENJITU_SVPT_BG_PHF, STORY_SAVE_BG_ITEM_GENJITU);
        else
            OutgameBgStart(OUTGAME_BG_MENU, MENUBG_PHF, STORY_SAVE_BG_LABEL_MENU);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1)
    {
        if (OutgameBgIsLoaded() == 0)
            return 0;
        if (OutgameBgLoadWait() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 2)
    {
        if (OutgameBgIsReady() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 3;
    }

    story_save_from = GetGPhaseCtrl()->from;
    story_save_step = 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Take the backdrop away.
// FF3 0x00250a50.

int end_Story_SaveGame(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        OutgameBgStop();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1)
    {
        if (OutgameBgIsStopped() == 0)
            return 0;
        OutgameBgRelease();
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The menu, one step chain per frame:
//   0 create  1 loaded, "in"  2 in ended  3 choose a row (4: the title row
//   asks yes/no; no goes back to 3)  5 "out", release, then the row's phase
//   as a child (step 6: back from it, reset the tree and start over) or,
//   without one, back to the requesting phase.
// FF3 0x00250b28.

int one_Story_SaveGame(void)
{
    LayoutAnimMain();

    if (story_save_step == 6)
    {
        GetGPhaseTree()->Reset();
        story_save_step = 0;
    }

    if (story_save_step == 0)
    {
        SavePointMenuCreate();
        story_save_step = 1;
    }

    if (story_save_step == 1 && SavePointMenuIsLoaded())
        story_save_step = 2;

    if (story_save_step == 2 && SavePointMenuIsInEnd())
        story_save_step = 3;

    if (story_save_step == 3 && SavePointMenuSelect())
    {
        if (SavePointMenuGetPhase() == STORY_SAVE_PHASE_TITLE)
        {
            story_save_step = 4;
            return 0;
        }
        story_save_step = 5;
    }

    if (story_save_step == 4)
    {
        int yes = SavePointMenuConfirm();

        if (yes < 0)
            story_save_step = 3;
        if (yes > 0)
            story_save_step = 5;
    }

    if (story_save_step == 5 && SavePointMenuOutWait())
    {
        int phase;

        SavePointMenuRelease();
        phase = SavePointMenuGetPhase();
        if (phase == -1)
        {
            GPhaseCtrlRequest(story_save_from, 0);
            return 1;
        }
        GPhaseCtrlRequest(phase, 1);
        story_save_step = 6;
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The captured screen at full colour, then the backdrop.
// FF3 0x00250cb0.

void pre_Story_SaveGame(void)
{
    ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
    OutgameBgDraw();
}

// FF3 0x00250cd8.

void after_Story_SaveGame(void)
{
    GetCmnFadeLayer()->Update();
    BpCtlMain();
    FadeMain();
}
