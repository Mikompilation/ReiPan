/* ==========================================================================
 *  outgame/title_obj.h
 *
 *  TITLE_OBJ: a PSS movie played through the map sky's movie player
 *  (GetMapSky) and drawn as a full-screen sprite; the title's background
 *  movie (title_menu.c) and the outgame backdrop's movie type (outgame_bg.c).
 *  FF3 0x2439d8..0x243e00, FF3's own code.
 * ======================================================================== */
#ifndef _OUTGAME_TITLE_OBJ_H
#define _OUTGAME_TITLE_OBJ_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// 0x18 bytes; callers new and delete it. C++ in FF3: C1/C2 constructor and
// destructor copies, operator new / delete at the callers.
struct TITLE_OBJ                        /* 0x18 */
{
    TITLE_OBJ(int file);                // FF3 0x00243a40 (TitleObjCtor; base-object copy 0x002439d8): sets the movie up (MapSkyMovieSet)
    ~TITLE_OBJ();                       // FF3 0x00243b00 (TitleObjDtor; base-object copy 0x00243aa8): MapSkyReset, file -1
    int  CheckReady();                  // FF3 0x00243b58 (TitleObjCheckReady): 1 when the movie can play (or there is none)
    int  Draw();                        // FF3 0x00243ba0 (TitleObjDraw): steps the movie, draws its frame; 0 when drawn
    int  FadeIn(int frames);            // FF3 0x00243d18 (TitleObjFadeIn): alpha up to 0x80 in frames; 1 when there
    int  FadeOut(int frames);           // FF3 0x00243d70 (TitleObjFadeOut): alpha down to 0 in frames; 1 when there
    void Pause();                       // FF3 0x00243dc0 (TitleObjPause): pause = 1 (with a movie)
    int  IsPaused();                    // FF3 0x00243dd8 (TitleObjIsPaused): returns 1
    void Resume();                      // FF3 0x00243de0 (TitleObjResume): pause = 0 (with a movie)

    /* 0x00 */ int    file;             // PSS file number, -1: none (destroyed)
    /* 0x04 */ int    alpha;            // sprite alpha 0..0x80 (FadeIn / FadeOut)
    /* 0x08 */ u_long alpha_reg;        // GS ALPHA_1 of the sprite (0x44; the title: 0x68 | alpha << 32)
    /* 0x10 */ int    pause;            // MapSkyMovieProc's argument: non-zero keeps the last frame
    /* 0x14 */ int    _014;             // never written by FF3's code
};
static_assert(sizeof(TITLE_OBJ) == 0x18, "TITLE_OBJ is 0x18 bytes");
static_assert(offsetof(TITLE_OBJ, alpha) == 0x04, "TITLE_OBJ.alpha");
static_assert(offsetof(TITLE_OBJ, alpha_reg) == 0x08, "TITLE_OBJ.alpha_reg");
static_assert(offsetof(TITLE_OBJ, pause) == 0x10, "TITLE_OBJ.pause");

#endif /* _OUTGAME_TITLE_OBJ_H */
