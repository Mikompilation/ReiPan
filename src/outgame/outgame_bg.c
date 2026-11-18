/* ==========================================================================
 *  outgame/outgame_bg.c
 *
 *  The outgame backdrop (see outgame_bg.h): the backdrop of the type
 *  OutgameBgStart chose, the OUTGAME_TTLBG_PHF cover over it, the menu BGM
 *  pair and the OutgameBgObj wrappers of the movie object.
 *  FF3 0x24fb28..0x250268, FF3's own code.
 * ======================================================================== */
#include "outgame/outgame_bg.h"

#include <new>                              // operator new / delete

#include "system/b2d/BpObj.h"              // BP_OBJ, BpObj*
#include "common/cmn_fade_layer.h"          // GetCmnFadeLayer, capCtl::Term
#include "ingame/menu/menu_bg.h"          // MenuBg*
#include "outgame/title_obj.h"             // TITLE_OBJ, TitleObj*
#include "ingame/sound/snd_ctrl.h"          // SndBgmPlay, SndBgmStop
#include "system/b2d/B2dCtl.h"              // B2dCtlFindItem
#include "system/b2d/B2dSpr.h"              // B2dSprSetDisp
#include "system/eeiop/cd_file_dat.h"       // OUTGAME_TTLBG_PHF
#include "system/eeiop/snd_buffer.h"        // SndBufAllPauseLoopSnd, SndBufAllRestartLoopSnd
#include "system/eeiop/stream_auto.h"       // StreamAutoSetExclusiveMode

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The cover's states (outgame_bg_cover_state).
enum
{
    OUTGAME_BG_COVER_IDLE    = 0,           // waiting for OutgameBgEnable
    OUTGAME_BG_COVER_IN      = 1,           // playing "in"
    OUTGAME_BG_COVER_SHOWN   = 2,           // waiting for OutgameBgDisable
    OUTGAME_BG_COVER_OUT     = 3,           // playing "out"
    OUTGAME_BG_COVER_NONE    = -1,          // no cover (0xff)
};

static BP_OBJ    *outgame_bg_cover;         // sdata 0x372088: OUTGAME_TTLBG_PHF
static int        outgame_bg_enable;        // sdata 0x37208c
static s_char     outgame_bg_cover_state;   // sdata 0x372090
static const char outgame_bg_label_in[]  = "in";    // sdata 0x372098
static const char outgame_bg_label_out[] = "out";   // sdata 0x3720a0
static int        outgame_bg_type = OUTGAME_BG_NONE;    // sdata 0x3720a4: OUTGAME_BG_TYPE
static int        outgame_bg_loaded;        // sdata 0x3720a8
static int        outgame_bg_arg = -1;      // sdata 0x3720ac: OutgameBgStart's arg
static BP_OBJ    *outgame_bg_layout;        // sdata 0x3720b0: type 1
static TITLE_OBJ *outgame_bg_movie;         // sdata 0x3720b4: type 0

// ──────────────────────────────────────────────────────────────────────
// The cover
// ──────────────────────────────────────────────────────────────────────

// New the cover layout object, idle and disabled.
// FF3 0x0024fb28.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)

static void OutgameBgCoverInit(void)
{
    BP_OBJ *obj = new BP_OBJ(OUTGAME_TTLBG_PHF);
    outgame_bg_cover = obj;
    outgame_bg_cover_state = OUTGAME_BG_COVER_NONE;
    outgame_bg_enable = 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the cover layout is loaded (or there is none); the cover is idle
// from then on.
// FF3 0x0024fb70.

int OutgameBgLoadWait(void)
{
    if (outgame_bg_cover == nullptr)
        return 1;
    if (outgame_bg_cover->Load() == 0)
        return 0;
    outgame_bg_cover_state = OUTGAME_BG_COVER_IDLE;
    return 1;
}

// FF3 0x0024fbb0.

void OutgameBgEnable(void)
{
    outgame_bg_enable = 1;
}

// FF3 0x0024fbc0.

void OutgameBgDisable(void)
{
    outgame_bg_enable = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Step the cover: "in" once enabled, "out" once disabled again. Returns 1
// while it is idle (not shown, nothing playing), 0 while it shows or plays.
// FF3 0x0024fbc8.

static int OutgameBgCoverMain(void)
{
    switch (outgame_bg_cover_state)
    {
    case OUTGAME_BG_COVER_IDLE:
        if (outgame_bg_enable == 0 || outgame_bg_cover == nullptr)
            return 1;
        outgame_bg_cover->In(outgame_bg_label_in);
        outgame_bg_cover_state = OUTGAME_BG_COVER_IN;
        // fall through
    case OUTGAME_BG_COVER_IN:
        if (outgame_bg_cover == nullptr)
            return 1;
        if (outgame_bg_cover->IsInEnd() == 0)
            return 0;
        outgame_bg_cover_state = OUTGAME_BG_COVER_SHOWN;
        // fall through
    case OUTGAME_BG_COVER_SHOWN:
        if (outgame_bg_enable != 0)
            return 0;
        outgame_bg_cover_state = OUTGAME_BG_COVER_OUT;
        // fall through
    case OUTGAME_BG_COVER_OUT:
        if (outgame_bg_cover == nullptr)
            return 1;
        if (outgame_bg_cover->Out(outgame_bg_label_out) == 0)
            return 0;
        outgame_bg_cover_state = OUTGAME_BG_COVER_IDLE;
        return 1;
    default:
        return 1;
    }
}

// FF3 0x0024fca8.
// callcheck: operator_delete (operator delete)

static void OutgameBgCoverRelease(void)
{
    BP_OBJ *obj = outgame_bg_cover;

    if (obj != nullptr)
    {
        obj->Release();
        operator delete(obj);
    }
    outgame_bg_cover = nullptr;
    outgame_bg_enable = 0;
    outgame_bg_cover_state = OUTGAME_BG_COVER_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// The backdrop
// ──────────────────────────────────────────────────────────────────────

// Start the backdrop of `type` from `file` (outgame_bg.h) and the cover.
// FF3 0x0024fcf0.
// callcheck: operator_new, BpObjCtor, TitleObjCtor (new BP_OBJ, new TITLE_OBJ)

void OutgameBgStart(int type, int file, int arg)
{
    outgame_bg_loaded = 0;
    outgame_bg_layout = nullptr;
    outgame_bg_movie = nullptr;
    outgame_bg_type = type;
    outgame_bg_arg = arg;
    OutgameBgCoverInit();

    if (outgame_bg_type == OUTGAME_BG_LAYOUT)
    {
        BP_OBJ *obj = new BP_OBJ(file);
        outgame_bg_layout = obj;
    }
    else if (outgame_bg_type == OUTGAME_BG_MOVIE)
    {
        TITLE_OBJ *obj = new TITLE_OBJ(file);
        outgame_bg_movie = obj;
    }
    else if (outgame_bg_type == OUTGAME_BG_MENU)
    {
        MenuBgStart(file, 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the backdrop is loaded (kept until the next OutgameBgStart). A
// layout starts "in" as soon as it is loaded, with item `arg` hidden.
// FF3 0x0024fdb8.

int OutgameBgIsLoaded(void)
{
    int ret = 1;

    if (outgame_bg_loaded != 0)
        return 1;

    switch (outgame_bg_type)
    {
    case OUTGAME_BG_LAYOUT:
        if (outgame_bg_layout != nullptr)
        {
            ret = outgame_bg_layout->Load() != 0;
            if (ret)
            {
                outgame_bg_layout->In(outgame_bg_label_in);
                if (outgame_bg_arg != -1)
                    B2dSprSetDisp(B2dCtlFindItem(outgame_bg_layout->GetHandle(), outgame_bg_arg), 0);
            }
        }
        break;

    case OUTGAME_BG_MOVIE:
        if (outgame_bg_movie != nullptr && outgame_bg_movie->CheckReady() == 0)
            ret = 0;
        break;

    case OUTGAME_BG_MENU:
        ret = MenuBgIsReady(outgame_bg_arg) != 0;
        break;

    default:
        break;
    }
    outgame_bg_loaded = ret;
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the cover, then the movie or the menu backdrop (a layout
// draws itself from BpCtlMain).
// FF3 0x0024fea8.

void OutgameBgDraw(void)
{
    OutgameBgCoverMain();

    if (outgame_bg_type == OUTGAME_BG_MOVIE)
    {
        if (outgame_bg_movie != nullptr)
            outgame_bg_movie->Draw();
    }
    else if (outgame_bg_type == OUTGAME_BG_MENU)
    {
        MenuBgDraw();
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the backdrop is fully shown: the layout's "in" ended, the movie
// faded in (10 frames), the menu backdrop's parts stopped.
// FF3 0x0024fef8.

int OutgameBgIsReady(void)
{
    switch (outgame_bg_type)
    {
    case OUTGAME_BG_LAYOUT:
        if (outgame_bg_layout == nullptr)
            return 1;
        return outgame_bg_layout->IsInEnd() != 0;

    case OUTGAME_BG_MOVIE:
        if (outgame_bg_movie == nullptr)
            return 1;
        return outgame_bg_movie->FadeIn(10) != 0;

    case OUTGAME_BG_MENU:
        return MenuBgIsInEnd() != 0;

    default:
        return 1;
    }
}

// Only the menu backdrop has a stop request; the others leave in
// OutgameBgIsStopped.
// FF3 0x0024ff88.

void OutgameBgStop(void)
{
    if (outgame_bg_type == OUTGAME_BG_MENU)
        MenuBgStop();
}

// ──────────────────────────────────────────────────────────────────────
// Take the backdrop away: the layout plays "out", the movie fades out (10
// frames), the menu backdrop releases. 1 once that and the cover are done.
// FF3 0x0024ffb0.

int OutgameBgIsStopped(void)
{
    int cover_idle = OutgameBgCoverMain() != 0;

    switch (outgame_bg_type)
    {
    case OUTGAME_BG_LAYOUT:
        if (outgame_bg_layout == nullptr)
            return cover_idle;
        return outgame_bg_layout->Out(outgame_bg_label_out) != 0 && cover_idle;

    case OUTGAME_BG_MOVIE:
        if (outgame_bg_movie == nullptr)
            return cover_idle;
        return outgame_bg_movie->FadeOut(10) != 0 && cover_idle;

    case OUTGAME_BG_MENU:
        return MenuBgRelease() != 0 && cover_idle;

    default:
        return cover_idle;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Free the cover and the backdrop object and clear the fade layer. FF3
// leaves the type set when a movie / layout backdrop has no object.
// FF3 0x00250050.
// callcheck: operator_delete, TitleObjDtor (delete)

void OutgameBgRelease(void)
{
    OutgameBgCoverRelease();

    if (outgame_bg_type == OUTGAME_BG_MOVIE)
    {
        TITLE_OBJ *obj = outgame_bg_movie;

        if (obj == nullptr)
            return;
        delete obj;
        outgame_bg_movie = nullptr;
    }
    else if (outgame_bg_type == OUTGAME_BG_LAYOUT)
    {
        BP_OBJ *obj = outgame_bg_layout;

        if (obj == nullptr)
            return;
        obj->Release();
        operator delete(obj);
        outgame_bg_layout = nullptr;
    }
    else
    {
        outgame_bg_type = OUTGAME_BG_NONE;
        return;
    }
    GetCmnFadeLayer()->Term();
    outgame_bg_type = OUTGAME_BG_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// The menu BGM of the load / save screens: the stream gets exclusive use
// (30-frame fades) and the looping SEs pause while it plays.
// FF3 0x002500e8.

void OutgameBgmPlay(int file)
{
    SndBufAllPauseLoopSnd();
    StreamAutoSetExclusiveMode(1, 0x1e);
    SndBgmPlay(file, 100, 0x1000, 0x1e, nullptr, 1, 0);
}

// FF3 0x00250138.

void OutgameBgmStop(void)
{
    SndBgmStop(0x1e, 0);
    StreamAutoSetExclusiveMode(0, 0x1e);
    SndBufAllRestartLoopSnd();
}

// ──────────────────────────────────────────────────────────────────────
// The movie object (type 0) as the in-game load / save screens see it.
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00250168.

int OutgameBgObjIsUsed(void)
{
    return outgame_bg_type == OUTGAME_BG_MOVIE;
}

// FF3 0x00250178.

void OutgameBgObjShow(void)
{
    if (OutgameBgObjIsUsed() && outgame_bg_movie != nullptr)
        outgame_bg_movie->Pause();
}

// FF3 0x002501b0.

int OutgameBgObjIsShown(void)
{
    if (OutgameBgObjIsUsed() && outgame_bg_movie != nullptr)
        return outgame_bg_movie->IsPaused();
    return 1;
}

// FF3 0x002501f0.

void OutgameBgObjHide(void)
{
    if (OutgameBgObjIsUsed() && outgame_bg_movie != nullptr)
        outgame_bg_movie->Resume();
}

// FF3 0x00250228.

int OutgameBgObjIsReady(void)
{
    if (OutgameBgObjIsUsed() && outgame_bg_movie != nullptr)
        return outgame_bg_movie->CheckReady();
    return 1;
}
