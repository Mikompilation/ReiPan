/* ==========================================================================
 *  ingame/movie_room_menu/prg/movie_projecter.h
 *
 *  Movie requests: FF3 0x218678..0x2188f8, the FF3 descendant of FF2's
 *  projector (no film numbers, no save block).
 *
 *  MovieReqPlay stores the scene to show and the phase to come back to and
 *  requests phase 39 Story_SceneMovie, which goes to 66 Story_Scene or 67
 *  Story_Movie; those read the request through the getters below and
 *  request MovieReqGetRetPhase when done. Film mode keeps the screen that
 *  was captured before the movie drawn under the movie phases.
 * ======================================================================== */
#ifndef _INGAME_MOVIE_ROOM_MENU_PRG_MOVIE_PROJECTER_H
#define _INGAME_MOVIE_ROOM_MENU_PRG_MOVIE_PROJECTER_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// The one request (data 0x3f6110).
struct MOVIE_REQ
{
    short scene_no;                     // 0x00: scene (scene.c table row), -1 none
    short ret_phase;                    // 0x02: phase that ran when it was asked for
    u_int skip_mask;                    // 0x04: pad buttons that skip (-1: OK or START)
    int   skip;                         // 0x08: MovieDispMain may be skipped
    int   play;                         // 0x0c: Story_Movie goes on to phase 90 (else straight back)
    int   film_mode;                    // 0x10: MovieReqDispCapture redraws the captured screen
};
static_assert(sizeof(MOVIE_REQ) == 0x14, "MOVIE_REQ size");
static_assert(offsetof(MOVIE_REQ, ret_phase) == 0x02, "MOVIE_REQ.ret_phase");
static_assert(offsetof(MOVIE_REQ, skip_mask) == 0x04, "MOVIE_REQ.skip_mask");
static_assert(offsetof(MOVIE_REQ, skip) == 0x08, "MOVIE_REQ.skip");
static_assert(offsetof(MOVIE_REQ, play) == 0x0c, "MOVIE_REQ.play");
static_assert(offsetof(MOVIE_REQ, film_mode) == 0x10, "MOVIE_REQ.film_mode");

void  MovieReqInit(void);               // FF3 0x00218678: GameSystemStart, IngameInit
void  MovieReqEnd(void);                // FF3 0x002186a8: empty (IngameEnd)
int   MovieReqGetSceneNo(void);         // FF3 0x002186b0
short MovieReqGetRetPhase(void);        // FF3 0x002186c0
int   MovieReqIsPlay(void);             // FF3 0x002186d0: one_Story_Movie / one_Story_Scene
int   MovieReqGetSkip(void);            // FF3 0x002186e0: MovieDispMain's skip
void  MovieReqSetFilmMode(int mode);    // FF3 0x002186f0
void  MovieReqSetSkipMask(u_int mask);  // FF3 0x00218700
u_int MovieReqGetSkipMask(void);        // FF3 0x00218710: MovieDispMain's pad
int   IsMoviePhase(void);               // FF3 0x00218720: leaf phase 89 or 90
// Requests phase 0x27 (Story_SceneMovie) for scene `scene_no` unless a movie
// phase runs or the scene's file is missing; a1 is not read (callers pass 1).
void  MovieReqPlay(int scene_no, int unused);   // FF3 0x00218770
// Film mode on: redraws the captured screen (ScreenCaptureDisp) and runs
// FadeMain; the movie phases call it every frame.
void  MovieReqDispCapture(void);        // FF3 0x00218898

#endif /* _INGAME_MOVIE_ROOM_MENU_PRG_MOVIE_PROJECTER_H */
