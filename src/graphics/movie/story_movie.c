/* ==========================================================================
 *  graphics/movie/story_movie.c
 *
 *  The movie phases 90 Story_Movie_Main and 67 Story_Movie, FF3
 *  0x2188f8..0x218d90 (see story_movie.h). FF2's IngameScene.c versions
 *  played the movie from 67 itself; FF3 loads the scene's effect data in 67
 *  and plays in its child 90.
 *
 *  FF3 0x00218d70 is not written: the compiler's atexit thunk for the
 *  GPhaseCtrl singleton (GPhaseCtrlTerm(0x41e468)), whose C++ static
 *  gphase.c owns here; it marks the end of the file.
 * ======================================================================== */
#include "graphics/movie/story_movie.h"

#include "common/heapctrl.h"            // heapCtrlMalloc / Free
#include "graphics/effect/effect.h"     // eff_wrk
#include "graphics/graph3d/gra3d.h"     // gra3dApplyFog
#include "graphics/movie/movie_title.h" // *MovieWithTitle
#include "graphics/scene/scene.h"       // GetFileNoFromSceneNo, SceneEffectDataFileNoGet
#include "graphics/scene/scene_effect.h"    // SceneEffectInit / End
#include "ingame/ingame.h"              // GPhaseCtrlRequest, SndVolume0Mute
#include "ingame/movie_room_menu/prg/movie_projecter.h" // MovieReq*
#include "main/gphase.h"                // GetGPhaseCtrl
#include "outgame/option.h"             // GetOptionWrk
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/snd_stream.h"    // SndStreamAllStop
#include "system/eeiop/stream_auto.h"   // StreamAuto*
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEndAll
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPALMode

#define STORY_MOVIE_PHASE_MAIN  0x5a    // 90 Story_Movie_Main (67's child)

// Streams still playing after this many frames are stopped (900 NTSC,
// 750 PAL: 15 s).
#define STORY_MOVIE_STREAM_WAIT_NTSC    900
#define STORY_MOVIE_STREAM_WAIT_PAL     750

// Phase 67's load: 0 no movie file, 1 started, 2 loading, 3 loaded.
static int   story_movie_load_step;     // bss 0x3f6128
static void *story_movie_eff_data;      // bss 0x3f612c: the scene's effect data (scene file + 2), system heap
static int   story_movie_load_id;       // bss 0x3f6130: LoadReq's id (not read)

// Phase 90: 1 wait for the streams, 2 play, 3 back to the requesting phase.
static int story_movie_main_step;       // sbss 0x3736c8
static int story_movie_main_wait;       // sbss 0x3736cc: frames waited for the streams

// ──────────────────────────────────────────────────────────────────────
// Phase 90 Story_Movie_Main
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002188f8.

int init_Story_Movie_Main(void)
{
    StreamAutoSetPlayNum(0, 3);
    story_movie_main_step = 1;
    story_movie_main_wait = 0;
    return 1;
}

// FF3 0x00218930.

int end_Story_Movie_Main(void)
{
    StreamAutoSetPlayNum(2, 100);
    return 1;
}

// Waits until every stream has stopped (forcing them after 15 s), plays the
// movie with the sound muted and the screen effects off (eff_wrk._18 0),
// then requests the phase that asked for the movie.
// FF3 0x00218958.

int one_Story_Movie_Main(void)
{
    switch (story_movie_main_step)
    {
    case 1:
        MovieReqDispCapture();
        if (StreamAutoIsAllStop() != 0)
        {
            SndVolume0Mute(1);
            eff_wrk._18 = 0;
            SceneEffectInit(1);
            InitMovieWithTitle(MovieReqGetSceneNo(), 1);
            story_movie_main_step = 2;
        }
        else
        {
            story_movie_main_wait++;
            if (story_movie_main_wait > (GetPALMode() != 0 ? STORY_MOVIE_STREAM_WAIT_PAL
                                                            : STORY_MOVIE_STREAM_WAIT_NTSC))
            {
                PrintStreamAutoStatus();
                StreamAutoAllStop();
                SndStreamAllStop();
                story_movie_main_wait = 0;
            }
        }
        break;

    case 2:
        if (PlayMovieWithTitle(StoryMovieGetEffectData(), GetOptionWrk()->now.subtitle,
                               MovieReqGetSkip(), MovieReqGetSkipMask()) == 0)
        {
            return 0;
        }
        EndMovieWithTitle();
        SceneEffectEnd();
        eff_wrk._18 = 1;
        SndVolume0Mute(0);
        gra3dApplyFog();
        MovieReqDispCapture();
        story_movie_main_step = 3;
        break;

    case 3:
        MovieReqDispCapture();
        GPhaseCtrlRequest(MovieReqGetRetPhase(), 0);
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 67 Story_Movie
// ──────────────────────────────────────────────────────────────────────

// Starts loading the scene's effect data into the system heap (when it has
// any); step 0 when the movie file itself is missing or empty.
// FF3 0x00218b18.

int init_Story_Movie(void)
{
    int scene_no = MovieReqGetSceneNo();
    int eff_file = SceneEffectDataFileNoGet(scene_no);

    story_movie_eff_data  = nullptr;
    story_movie_load_step = 1;
    story_movie_load_id   = 0;

    if (GetFileSize(GetFileNoFromSceneNo(scene_no)) == (u_int)-1 ||
        GetFileSize(GetFileNoFromSceneNo(scene_no)) == 0)
    {
        story_movie_load_step = 0;
        return 1;
    }

    u_int size = GetFileSize(eff_file);
    if (GetFileSize(eff_file) != (u_int)-1 && GetFileSize(eff_file) != 0)
    {
        story_movie_eff_data = heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
    }
    if (story_movie_eff_data != nullptr)
    {
        story_movie_load_id = LoadReq(eff_file, story_movie_eff_data);
    }
    return 1;
}

// FF3 0x00218c30.

int end_Story_Movie(void)
{
    if (story_movie_eff_data != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), story_movie_eff_data, 0, 0, 0);
        story_movie_eff_data = nullptr;
    }
    return 1;
}

// Once the effect data is in, goes on to 90 Story_Movie_Main, or straight
// back when the request does not play the movie; without a movie file, back
// to the phase that requested this one.
// FF3 0x00218c88.

int one_Story_Movie(void)
{
    MovieReqDispCapture();
    switch (story_movie_load_step)
    {
    case 0:
        GPhaseCtrlRequest(GetGPhaseCtrl()->from, 0);
        return 1;

    case 1:
        story_movie_load_step = 2;
        // fall through
    case 2:
        if (IsLoadEndAll() != 0)
        {
            story_movie_load_step = 3;
        }
        return 0;

    case 3:
        GPhaseCtrlRequest(MovieReqIsPlay() != 0 ? (short)STORY_MOVIE_PHASE_MAIN
                                                : MovieReqGetRetPhase(), 0);
        return 1;
    }
    return 0;
}

// FF3 0x00218d50.

void pre_Story_Movie(void)
{
}

// FF3 0x00218d58.

void after_Story_Movie(void)
{
}

// FF3 0x00218d60.

void *StoryMovieGetEffectData(void)
{
    return story_movie_eff_data;
}
