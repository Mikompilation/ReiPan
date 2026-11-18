/* ==========================================================================
 *  graphics/movie/movie.h
 *
 *  PSS movie playback (FF2 movie.c), FF3 0x217c90..0x218678: the buffers
 *  around playpss (one 2.7 MB block borrowed from the map loader or the
 *  system heap, BORROW_BUF), the start / per-frame draw / end of a movie,
 *  the frame counter MovieCountGet hands the subtitles, and phase 39
 *  Story_SceneMovie, which turns a MovieReqPlay request into phase 66
 *  Story_Scene or 67 Story_Movie.
 * ======================================================================== */
#ifndef _GRAPHICS_MOVIE_MOVIE_H
#define _GRAPHICS_MOVIE_MOVIE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

struct playPssRsrcs;

// A large temporary buffer (InitMovie: the movie's EE buffers; scene.c's
// scene data too). In a game (title_start_flg.mode 1 or 2) it borrows the
// room buffer the map loader is not showing -- nothing at all when both are
// shown -- and elsewhere it allocates from the system heap. FF3 keeps it on
// the C++ heap (operator_new(8) + the constructor).
// layout: host -- adrs is a 4-byte EE address on the PS2 (+0x4, size 0x8).
struct BORROW_BUF
{
    BORROW_BUF(u_int size);             // FF3 0x002183e8 (BorrowBufCtor; base-object copy 0x00218310)
    ~BORROW_BUF();                      // FF3 0x00218530 (BorrowBufDtor; base-object copy 0x002184c0): frees a heap block

    /* 0x0 */ int     heap;             // 1: adrs came from the system heap (the destructor frees it)
    /* 0x4 */ u_char *adrs;             // the buffer; nullptr when none could be had
};
static_assert(offsetof(BORROW_BUF, adrs) == sizeof(void *), "BORROW_BUF.adrs (EE +0x4)");

int  iopalloc(u_int size, const char *desc);    // FF3 0x00217c90: IOP heap block, hangs when there is none
void InitMovie(playPssRsrcs *rsrcs, int audio_flg); // FF3 0x00217cf8: carves the buffers out of one BORROW_BUF

// Starts PSS file_no (vol: the per-scene volume, scales the BGM group
// volume / 256; audio_flg 0: a silent movie). 1 when it plays, 0 when the
// buffers could not be had.
int  StartMovie(int file_no, int vol, int audio_flg);  // FF3 0x00217f40
void EndMovie(void);                    // FF3 0x00218250: stop a playing movie, restore 2 V-blanks per frame
// One frame of the playing PSS movie (playPssMain, the frame drawn as a
// sprite over black); skip: a trigger of pad (-1: OK | 0x800) stops it.
// Nonzero when finished (also when none plays).
int  MovieDispMain(int skip, int pad);  // FF3 0x00218048
int  MovieCountGet(void);               // FF3 0x002182d0: frames played (halved in PAL), -1 when none plays
void MovieResetPlaying(void);           // FF3 0x00218300: the counter to -1 (GameSystemStart)

// Phase 39 Story_SceneMovie: the scene MovieReqPlay asked for goes to phase
// 67 Story_Movie (a movie scene) or 66 Story_Scene; a missing or empty file
// goes straight back to the phase that asked.
int  init_Story_SceneMovie(void);       // FF3 0x002185a0
int  one_Story_SceneMovie(void);        // FF3 0x002185d0
int  end_Story_SceneMovie(void);        // FF3 0x002185c8
void pre_Story_SceneMovie(void);        // FF3 0x00218668
void after_Story_SceneMovie(void);      // FF3 0x00218670

#endif /* _GRAPHICS_MOVIE_MOVIE_H */
