/* ==========================================================================
 *  graphics/movie/movie_title_dat.h
 *
 *  Movie caption tables (FF2 movie_title_dat.c, data only there), FF3
 *  0x2193a0..0x219540: FF3 gives the file its lookups. The caption lists
 *  are found through a {scene, list} table (data 0x35cb60) instead of FF2's
 *  array indexed by scene number.
 * ======================================================================== */
#ifndef _GRAPHICS_MOVIE_MOVIE_TITLE_DAT_H
#define _GRAPHICS_MOVIE_MOVIE_TITLE_DAT_H

#include "graphics/movie/movie_title.h" // MOVIE_TITLE_DAT

// One row of data 0x35cb60; the table ends with a null list.
// layout: host -- dat is a pointer (EE row 8 bytes: scene_no +0, dat +4).
struct MOVIE_TITLE_SCENE
{
    int              scene_no;
    MOVIE_TITLE_DAT *dat;
};

MOVIE_TITLE_DAT *GetMovieTitleDat(int scene_no);                // FF3 0x002193a0: null: no captions
int  GetMovieTitleDatTblPos(MOVIE_TITLE_DAT *data, int timer);  // FF3 0x002193f8: -1: none now
int  MovieTitleIsEveryDisp(int msg_id);                         // FF3 0x002194e8

#endif /* _GRAPHICS_MOVIE_MOVIE_TITLE_DAT_H */
