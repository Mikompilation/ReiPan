/* ==========================================================================
 *  ingame/movie_room_menu/prg/movie_projecter.c
 *
 *  Movie requests: FF3 0x218678..0x2188f8 (see movie_projecter.h). FF3-only
 *  code in FF2's movie_projecter.c slot; none of FF2's projector survives.
 * ======================================================================== */
#include "ingame/movie_room_menu/prg/movie_projecter.h"

#include "graphics/graph2d/fade.h"      // FadeMain
#include "graphics/gs_reg.h"            // ScreenCaptureDisp
#include "graphics/scene/scene.h"       // GetFileNoFromSceneNo
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "main/gphase.h"                // GetGPhaseCtrl
#include "system/eeiop/cddat.h"         // GetFileSize

#define MOVIE_PHASE_SCENE_MOVIE 0x27    // 39 Story_SceneMovie: picks 66 or 67
#define MOVIE_PHASE_SCENE       0x42    // 66 Story_Scene
#define MOVIE_PHASE_MOVIE       0x43    // 67 Story_Movie
#define MOVIE_PHASE_SCENE_MAIN  0x59    // 89 Story_Scene_Main
#define MOVIE_PHASE_MOVIE_MAIN  0x5a    // 90 Story_Movie_Main

#define MOVIE_SCENE_NUM         0x44    // rows of scene.c's table

static MOVIE_REQ movie_req;             // 0x3f6110

// ──────────────────────────────────────────────────────────────────────
// No request, every option on. FF3 0x00218678.

void MovieReqInit(void)
{
    movie_req.scene_no  = -1;
    movie_req.skip_mask = (u_int)-1;
    movie_req.ret_phase = -1;
    movie_req.film_mode = 1;
    movie_req.play      = 1;
    movie_req.skip      = 1;
}

// FF3 0x002186a8.

void MovieReqEnd(void)
{
}

// FF3 0x002186b0.

int MovieReqGetSceneNo(void)
{
    return movie_req.scene_no;
}

// FF3 0x002186c0.

short MovieReqGetRetPhase(void)
{
    return movie_req.ret_phase;
}

// FF3 0x002186d0.

int MovieReqIsPlay(void)
{
    return movie_req.play;
}

// FF3 0x002186e0.

int MovieReqGetSkip(void)
{
    return movie_req.skip;
}

// FF3 0x002186f0.

void MovieReqSetFilmMode(int mode)
{
    movie_req.film_mode = mode;
}

// FF3 0x00218700.

void MovieReqSetSkipMask(u_int mask)
{
    movie_req.skip_mask = mask;
}

// FF3 0x00218710.

u_int MovieReqGetSkipMask(void)
{
    return movie_req.skip_mask;
}

// The scene and movie play phases. FF3 0x00218720.

int IsMoviePhase(void)
{
    return (u_int)(GetGPhaseCtrl()->leaf->id - MOVIE_PHASE_SCENE_MAIN) < 2;
}

// FF3 0x00218760.

static int MovieReqCheckSceneNo(u_int scene_no)
{
    return scene_no < MOVIE_SCENE_NUM;
}

// Refused while a movie phase (39, 66, 67, 89, 90) runs and for a scene
// whose file is missing or empty. FF3 inlines a helper returning whether
// the request went through (its out-of-line "return 0" tail is at 0x218890)
// and only then sets play; a1 is never read.
// FF3 0x00218770.

void MovieReqPlay(int scene_no, int unused)
{
    short id = GetGPhaseCtrl()->leaf->id;
    int   ok = 0;

    (void)unused;
    if (IsMoviePhase() == 0 && id != MOVIE_PHASE_SCENE_MOVIE && id != MOVIE_PHASE_SCENE &&
        id != MOVIE_PHASE_MOVIE && MovieReqCheckSceneNo(scene_no) != 0 &&
        GetFileSize(GetFileNoFromSceneNo(scene_no)) != (u_int)-1 &&
        GetFileSize(GetFileNoFromSceneNo(scene_no)) != 0)
    {
        movie_req.scene_no  = scene_no;
        movie_req.skip      = 1;
        movie_req.ret_phase = GetGPhaseCtrl()->leaf->id;
        movie_req.film_mode = 1;
        GPhaseCtrlRequest(MOVIE_PHASE_SCENE_MOVIE, 0);
        ok = 1;
    }
    if (ok != 0)
    {
        movie_req.play = 1;
    }
}

// FadeMain is a tail call (j) in FF3.
// FF3 0x00218898.

void MovieReqDispCapture(void)
{
    if (movie_req.film_mode != 0)
    {
        ScreenCaptureDisp(0x80, 0x80, 0x80, 0x80);
        FadeMain();
    }
}

// FF3 0x002188d8 (MovieReqGPhaseCtrlAtExit) is compiler output: the atexit
// thunk of the GPhaseCtrl singleton (GPhaseCtrlTerm(0x41e468)), whose C++
// static gphase.c owns here.
