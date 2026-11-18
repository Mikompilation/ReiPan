/* ==========================================================================
 *  ingame/gameover/prg/gameover.c
 *
 *  The game over: phase 10 (the movie) and phase 40 Story_LoadGame (the game
 *  over menu), see gameover.h. FF3 0x1d7a58..0x1d8208. FF3 0x001d81d0 and
 *  0x001d81f0 are not written: the compiler's atexit thunks for the
 *  GPhaseCtrl and GPhaseTree singletons (GPhaseCtrlTerm, 0x1d4228 on
 *  sdata gp+0x6a0), the file-end markers.
 * ======================================================================== */
#include "ingame/gameover/prg/gameover.h"

#include "album/prg/album.h"                // AlbumSetIngame
#include "common/cmn_fade_layer.h"          // GetCmnFadeLayer, capCtl::Update
#include "graphics/graph2d/fade.h"          // FadeInReq, FadeMain
#include "graphics/movie/movie.h"           // StartMovie, MovieDispMain, EndMovie
#include "ingame/clear/GameClear.h"         // GameClearSetMode, GAME_CLEAR_MODE_MISSION_FAILED
#include "ingame/gameover_menu.h"           // GameOverMenu*
#include "ingame/ingame.h"                  // IngameInit, IngameLoadRestore, IngameStartEnd, GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"              // title_start_flg
#include "ingame/menu/anim_2d.h"            // LayoutAnimMain
#include "ingame/sound/snd_ctrl.h"          // SndBgmStopAll, SndAllBufStop
#include "main/gphase.h"                    // GetGPhaseCtrl, GetGPhaseTree
#include "main/gphase_tree.h"               // GPHASE_TREE::Reset
#include "outgame/outgame_bg.h"             // OutgameBg*
#include "system/b2d/BpCtl.h"               // BpCtlMain
#include "system/eeiop/cd_file_dat.h"       // GAMEOVER_PSS, OUTGAME_BG_PSS
#include "system/os/eecdvd.h"               // IsLoadEndAll

// Phases this file requests.
#define GAMEOVER_PHASE_TITLE        5       // Title_Mode
#define GAMEOVER_PHASE_ALBUM        8       // the album (linked)
#define GAMEOVER_PHASE_MOVIE        10      // the "game over" movie
#define GAMEOVER_PHASE_NOW_LOADING  0x1e    // 30 Story_NowLoading, after a load
#define GAMEOVER_PHASE_MENU         0x28    // 40 Story_LoadGame
#define GAMEOVER_PHASE_MISSION_END  0x32    // 50, an extra mission's end
#define GAMEOVER_PHASE_LOAD_MC      0x44    // 68 Story_LoadGame_Mc (linked)

#define GAMEOVER_WAIT               10      // frames of silence before the movie

// One's steps (load_game_step).
enum
{
    LOAD_GAME_STEP_CREATE  = 0,             // make the menu
    LOAD_GAME_STEP_LOAD    = 1,             // wait for it to load ("in" starts)
    LOAD_GAME_STEP_IN      = 2,             // wait for "in"
    LOAD_GAME_STEP_SELECT  = 3,             // pick a row
    LOAD_GAME_STEP_CONFIRM = 4,             // the title row's yes / no
    LOAD_GAME_STEP_OUT     = 5,             // "out", then the row's phase
    LOAD_GAME_STEP_LOADED  = 6,             // back from the load (phase 68)
    LOAD_GAME_STEP_BACK    = 7,             // back from the album (phase 8)
};

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static int game_over_wait;                  // sdata 0x370ae0 (gp-0x2a10): init_GameOver's frames
static int load_game_step;                  // sdata 0x373330 (gp-0x1c0): one_Story_LoadGame's step
static int load_game_bg;                    // sdata 0x373334 (gp-0x1bc): the backdrop movie is up (pre draws it)

// ──────────────────────────────────────────────────────────────────────
// Rei is dead or an event ended the game: the movie, or in an extra
// mission its failure.
// FF3 0x001d7a58

void GameOverReq(void)
{
    if (title_start_flg.mode == 2)
    {
        GameClearSetMode(GAME_CLEAR_MODE_MISSION_FAILED, 0);
        GPhaseCtrlRequest(GAMEOVER_PHASE_MISSION_END, 0);
        return;
    }
    GPhaseCtrlRequest(GAMEOVER_PHASE_MOVIE, 0);
    AlbumSetIngame(2, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 10: stop the BGM and every SE voice, then wait ten frames.
// FF3 0x001d7ab0

int init_GameOver(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        SndBgmStopAll();
        SndAllBufStop();
        game_over_wait = GAMEOVER_WAIT;
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1 && game_over_wait > 0)
    {
        game_over_wait--;
        return 0;
    }
    return 1;
}

// FF3 0x001d7b68

int end_GameOver(void)
{
    return 1;
}

// The "game over" movie (skippable), then the game over menu.
// FF3 0x001d7b70

int one_GameOver(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        if (StartMovie(GAMEOVER_PSS, 100, 1) != 0)
            GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1)
    {
        if (MovieDispMain(1, -1) == 0)
            return 0;
        EndMovie();
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }
    GPhaseCtrlRequest(GAMEOVER_PHASE_MENU, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 40: reset the in-game state, start the backdrop movie and fade in
// once it shows. Steps in the node's state_cnt (0..5).
// FF3 0x001d7c68

int init_Story_LoadGame(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        load_game_bg = 0;
        IngameInit();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 1)
    {
        if (IsLoadEndAll() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 2)
    {
        OutgameBgStart(0, OUTGAME_BG_PSS, -1);
        load_game_bg = 1;
        GetGPhaseCtrl()->leaf->state_cnt = 3;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 3)
    {
        if (OutgameBgIsLoaded() == 0)
            return 0;
        if (OutgameBgLoadWait() == 0)
            return 0;
        FadeInReq(0, 0, 0, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 4;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt == 4)
    {
        if (OutgameBgIsReady() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 5;
    }

    load_game_step = LOAD_GAME_STEP_CREATE;
    return 1;
}

// Stop and release the backdrop movie, then the in-game start's end.
// FF3 0x001d7e88

int end_Story_LoadGame(void)
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
        load_game_bg = 0;
        GetGPhaseCtrl()->leaf->state_cnt = 2;
    }
    return IngameStartEnd();
}

// The game over menu, one step chain per frame (several steps can pass in
// one frame). Back from a linked phase the tree is reset: after a load
// (title_start_flg.load) the game goes on through Now Loading, else the
// menu opens again.
// FF3 0x001d7f68

int one_Story_LoadGame(void)
{
    int res;

    LayoutAnimMain();

    if (load_game_step == LOAD_GAME_STEP_LOADED)
    {
        GetGPhaseTree()->Reset();
        if (title_start_flg.load != 0)
        {
            IngameLoadRestore();
            GPhaseCtrlRequest(GAMEOVER_PHASE_NOW_LOADING, 0);
            return 1;
        }
        load_game_step = LOAD_GAME_STEP_CREATE;
    }

    if (load_game_step == LOAD_GAME_STEP_BACK)
    {
        GetGPhaseTree()->Reset();
        load_game_step = LOAD_GAME_STEP_CREATE;
    }

    if (load_game_step == LOAD_GAME_STEP_CREATE)
    {
        GameOverMenuCreate();
        load_game_step = LOAD_GAME_STEP_LOAD;
    }

    if (load_game_step == LOAD_GAME_STEP_LOAD && GameOverMenuIsLoaded() != 0)
        load_game_step = LOAD_GAME_STEP_IN;

    if (load_game_step == LOAD_GAME_STEP_IN && GameOverMenuIsInEnd() != 0)
        load_game_step = LOAD_GAME_STEP_SELECT;

    if (load_game_step == LOAD_GAME_STEP_SELECT && GameOverMenuSelect() != 0)
    {
        if (GameOverMenuGetPhase() == GAMEOVER_PHASE_TITLE)
        {
            load_game_step = LOAD_GAME_STEP_CONFIRM;
            return 0;
        }
        load_game_step = LOAD_GAME_STEP_OUT;
    }

    if (load_game_step == LOAD_GAME_STEP_CONFIRM)
    {
        res = GameOverMenuConfirm();
        if (res < 0)
            load_game_step = LOAD_GAME_STEP_SELECT;
        if (res > 0)
            load_game_step = LOAD_GAME_STEP_OUT;
    }

    if (load_game_step != LOAD_GAME_STEP_OUT)
        return 0;

    if (GameOverMenuOutWait() == 0)
        return 0;
    GameOverMenuRelease();

    res = GameOverMenuGetPhase();
    if (res == GAMEOVER_PHASE_ALBUM)
    {
        GPhaseCtrlRequest(GAMEOVER_PHASE_ALBUM, 1);
        load_game_step = LOAD_GAME_STEP_BACK;
        return 1;
    }
    if (res == GAMEOVER_PHASE_LOAD_MC)
    {
        GPhaseCtrlRequest(GAMEOVER_PHASE_LOAD_MC, 1);
        load_game_step = LOAD_GAME_STEP_LOADED;
        return 1;
    }
    if (res != GAMEOVER_PHASE_TITLE)
        return 0;
    GPhaseCtrlRequest(GAMEOVER_PHASE_TITLE, 0);
    return 1;
}

// FF3 0x001d8178

void pre_Story_LoadGame(void)
{
    if (load_game_bg != 0)
        OutgameBgDraw();
}

// FF3 0x001d81a0

void after_Story_LoadGame(void)
{
    GetCmnFadeLayer()->Update();
    BpCtlMain();
    FadeMain();
}
