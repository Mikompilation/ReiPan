/* ==========================================================================
 *  ingame/story_main.c
 *
 *  The story phases around play:
 *    9  Story_Main        parent of every in-game phase: IngamePre each
 *                         frame, IngameEnd when the game ends; its one runs
 *                         once (on entry) and goes to 29.
 *    29 Story_Start       IngameInit; with title_start_flg.load the card load
 *                         (68, linked); then the start menu (55) when the
 *                         finder state has any count; then a new game
 *                         (mission mode) or the restore of the loaded game,
 *                         and on to 30.
 *    30 Story_NowLoading  the Now Loading layout (CcmLoadingMission in
 *                         mission mode, else CcmLoadingNormal) while
 *                         IngameNowLoadingMain loads; when the layout has
 *                         left, on to 32.
 *    32 Story_Normal      IngameMain + IngameDrawSub every frame.
 *  FF3 0x120b18..0x121000 (0x120fe0 is the compiler's atexit thunk for the
 *  GetGPhaseCtrl static, not written). FF2 kept these phases in
 *  ingame/ingame.c (init_Story_NowLoading / init_Story_Normal there are
 *  FF2's stream/sound restarts); FF3's are its own.
 * ======================================================================== */
#include "ingame/story_main.h"

#include "graphics/motion/anicode.h"                    // CcmLoading, CcmLoadingNormal, CcmLoadingMission
#include "ingame/ingame.h"                              // Ingame*
#include "ingame/ingame_wrk.h"                          // title_start_flg, PlayTimeSetStop, IngameSetOutgame
#include "ingame/event/prg/ev_command.h"                // EventMacroLoad_StreamInit, AreaChange
#include "ingame/menu/start_menu.h"                     // FinderStateHasAny
#include "ingame/map/BgSe.h"                            // GetLayoutSeWrk, CBgSe::SetArea
#include "ingame/plyr/player.h"                         // PlyrClear234
#include "ingame/story_door.h"                          // MapAreaNoGet
#include "main/gphase.h"                                // GetGPhaseCtrl, GPHASE_CTRL::SetNext
#include "outgame/title_menu.h"                         // TitleMovieFlagClear
#include "system/eeiop/cd_file_dat.h"                   // MISSION_LOAD_PHF, LOADING_PHF
#include "system/os/system.h"                           // GetSysWrk, SYS_WRK::GetLanguage

// ──────────────────────────────────────────────────────────────────────
// Callees in files that are not settled yet. Move each block to its real
// header when that file is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define PHASE_STORY_START       29
#define PHASE_STORY_NOWLOADING  30
#define PHASE_STORY_NORMAL      32
#define PHASE_STARTMENU_MAIN    55
#define PHASE_STORY_LOADGAME_MC 68

#define TITLE_START_MODE_MISSION 2                      // title_start_flg.mode: mission (finder) mode

static s_char      story_start_step;                    // sdata 0x372bec (gp-0x904)
static s_char      story_loading_step;                  // sdata 0x372bed (gp-0x903)
static CcmLoading *story_loading;                       // sdata 0x372bf0 (gp-0x900)

// ──────────────────────────────────────────────────────────────────────
// Phase 9 Story_Main.
// FF3 0x00120b18: tail call of IngameMainInit.

int init_Story_Main(void)
{
    return IngameMainInit();
}

// FF3 0x00120b30.

int end_Story_Main(void)
{
    IngameEnd();
    return 1;
}

// Runs only while 9 is the deepest phase, that is on entry: go on to 29.
// FF3 0x00120b50.

int one_Story_Main(void)
{
    GetGPhaseCtrl()->SetNext(PHASE_STORY_START, 0);
    return 1;
}

// FF3 0x00120b90.

void pre_Story_Main(void)
{
    IngamePre();
}

// FF3 0x00120ba8.

void after_Story_Main(void)
{
    IngameAfter();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 29 Story_Start.
// The start menu (init_StartMenu_Top / init_StartMenu_Mission) sets the step
// to 2, so Story_Start starts the game when 55 hands back.
// FF3 0x00120bc0; was SpCmnStart (wrong FF2 match).

void StoryStartSetStep(u_char step)
{
    story_start_step = step;
}

// FF3 0x00120bc8.

int init_Story_Start(void)
{
    story_start_step = 0;
    return IngameInit();
}

// FF3 0x00120be0: tail call of IngameStartEnd.

int end_Story_Start(void)
{
    return IngameStartEnd();
}

// FF3 0x00120bf8.

void pre_Story_Start(void)
{
}

// FF3 0x00120c00.

void after_Story_Start(void)
{
}

// Step 0: the card load first (68, linked under 29) when the title asked
// for it. Step 1: the start menu (55) when the finder state has any count.
// Step 2: start the game and go to the Now Loading screen.
// FF3 0x00120c08.

int one_Story_Start(void)
{
    switch (story_start_step)
    {
    case 0:
        story_start_step = 1;
        if (title_start_flg.load == 0)
            return 0;
        GetGPhaseCtrl()->SetNext(PHASE_STORY_LOADGAME_MC, 1);
        return 1;

    case 1:
        story_start_step = 2;
        if (!FinderStateHasAny())
            return 0;
        GetGPhaseCtrl()->SetNext(PHASE_STARTMENU_MAIN, 0);
        return 1;

    case 2:
        if (title_start_flg.mode == TITLE_START_MODE_MISSION)
            IngameStartNewGame();
        else
            IngameLoadRestore();
        GetGPhaseCtrl()->SetNext(PHASE_STORY_NOWLOADING, 0);
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 30 Story_NowLoading: the loading layout in this language.
// FF3 0x00120d18; FF2 init_Story_NowLoading stopped the streams instead.
// callcheck: operator_new, CcmLoadingMissionCtor, CcmLoadingNormalCtor (new)

int init_Story_NowLoading(void)
{
    story_loading_step = 0;
    if (title_start_flg.mode == TITLE_START_MODE_MISSION)
        story_loading = new CcmLoadingMission(GetSysWrk()->GetLanguage() + MISSION_LOAD_PHF);
    else
        story_loading = new CcmLoadingNormal(GetSysWrk()->GetLanguage() + LOADING_PHF);
    return 1;
}

// FF3 0x00120dd0.

int end_Story_NowLoading(void)
{
    delete story_loading;                               // FF3: vtable +0x18
    story_loading = nullptr;
    EventMacroLoad_StreamInit();
    GetLayoutSeWrk()->SetArea(MapAreaNoGet());
    return 1;
}

// Step 0: load (IngameNowLoadingMain) until done, then let the layout
// leave. Step 1: wait for the layout's state 3. Step 2: play (32); outside
// mission mode the title's start mode becomes 1, the title movie cursor
// resets, the play timer runs and the game is no longer outgame.
// FF3 0x00120e30.

int one_Story_NowLoading(void)
{
    story_loading->Main();                              // FF3: vtable +0x1c

    switch (story_loading_step)
    {
    case 0:
        if (IngameNowLoadingMain())
        {
            story_loading_step = 1;
            story_loading->End();                       // FF3: vtable +0x20
        }
        return 0;

    case 1:
        if (story_loading->state == 3)
            story_loading_step = 2;
        return 0;

    case 2:
        if (title_start_flg.mode == TITLE_START_MODE_MISSION)
        {
            AreaChange(1, 1);
            GetGPhaseCtrl()->SetNext(PHASE_STORY_NORMAL, 0);
            return 1;
        }
        GetGPhaseCtrl()->SetNext(PHASE_STORY_NORMAL, 0);
        title_start_flg.mode = 1;
        TitleMovieFlagClear();
        PlayTimeSetStop(0);
        IngameSetOutgame(0);
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 32 Story_Normal: play.
// FF3 0x00120f78.

int init_Story_Normal(void)
{
    PlyrClear234();
    return 1;
}

// FF3 0x00120f98.

int end_Story_Normal(void)
{
    return 1;
}

// FF3 0x00120fa0.

int one_Story_Normal(void)
{
    int ret = IngameMain(0);
    IngameDrawSub(0);
    return ret;
}

// Empty and unreferenced (between one_Story_Normal and the atexit thunk).
// FF3 0x00120fd8.

void StoryMainNop(void)
{
}
