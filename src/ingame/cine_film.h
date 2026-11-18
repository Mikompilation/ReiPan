/* ==========================================================================
 *  ingame/cine_film.h
 *
 *  The projector films (FF3-only, FF3 0x14ac58..0x14c7c8; no FF2 code).
 *  Three rooms hold a film projector (areas 0x13, 0x30, 0x1c; projector
 *  numbers 0..2). The event script (CINE_FILM_PLAY) starts the film put in
 *  the projector of an area: a PSS movie (MapSky's movie player, TBP 0x2bc0,
 *  160 x 128) drawn on the room's wall screen, a quad split into 20 x 16
 *  cells, with gra3d's special light on, the projector's rattle (SE bank
 *  0x1b41 EVSE_018_EISYAKI) at its turning reel (map object "f033" of area
 *  0x13, the only projector with one). The film pauses its rattle while the
 *  player is out of the projector's area and skips its frames while she goes
 *  through a door; at its end the projector object is deleted.
 *
 *  Each of the 11 films (rows of the film table 0x3508c8, cine_menu.c) has
 *  a flag byte: 1 played through, 2 its photo window reached, 4 playing,
 *  8 photographed in its window (PlyrFinderShot with the screen centre in
 *  the viewfinder). The flags and the projectors' films are saved
 *  (save_game_data entries 40 / 39); CINE_FLAG_CHECK reads the flags.
 * ======================================================================== */
#ifndef _INGAME_CINE_FILM_H
#define _INGAME_CINE_FILM_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

#define CINE_PROJECTOR_NUM  3           // rooms with a projector (tables 0x3509a0..0x3509e0)
#define CINE_FILM_NUM       11          // film flag bytes (0x391860), rows of the film table
#define CINE_GRID_COLS      20          // screen cells across (CineProjector::Setup)
#define CINE_GRID_ROWS      16          // and down

// cine_flag bits
#define CINE_FLAG_END       0x01        // played through (CineFlagWatchEnd)
#define CINE_FLAG_SEEN      0x02        // the frame count reached its photo window
#define CINE_FLAG_PLAY      0x04        // playing now
#define CINE_FLAG_PHOTO     0x08        // photographed inside its photo window

// One film running on a projector (operator_new(0x20) in CineFilmPlayReq).
// layout: host -- corners and the three grid arrays are pointers (EE 4
// bytes each); the EE offsets are in the comments. Only built on the heap.
struct CineProjector
{
    CineProjector(int file, sceVu0FVECTOR *corners, int dark);     // FF3 0x0014b480
    ~CineProjector();                                               // FF3 0x0014b628

    void SetReel(int area, const char *name, s_char axis);          // FF3 0x0014bb98
    int  Setup();                                                   // FF3 0x0014b748: 1 when the film runs
    int  Main(int skip, int pause);                                 // FF3 0x0014b880: 1 at the film's end
    void Draw();                                                    // FF3 0x0014b970

    /* 0x00 */ int            bank;     // rattle SE bank (SndBankNew), -1: none
    /* 0x04 */ int            voice;    // SndBankPlay id, CSND_BUF_PLAY_NO_ID: none
    /* 0x08 */ sceVu0FVECTOR *corners;  // the screen quad: TL, TR, BL, BR (cine_screen row)
    /* 0x0c */ sceVu0FVECTOR *vtx;      // 4 corners per cell, strip order (system heap)
    /* 0x10 */ float         *u;        // their u fractions 0..1
    /* 0x14 */ float         *v;        // their v fractions 0..1
    /* 0x18 */ int            dark;     // draws black at half the alpha (projector 2)
    /* 0x1c */ int            ready;    // Setup has built the grid
};
static_assert(offsetof(CineProjector, voice) == 0x04, "CineProjector.voice");
static_assert(offsetof(CineProjector, corners) == 0x08, "CineProjector.corners");

extern int    cine_film[CINE_PROJECTOR_NUM];    // bss 0x391850: film file per projector, -1: empty
extern u_char cine_flag[CINE_FILM_NUM];         // bss 0x391860: CINE_FLAG_* per film

void CineInit(void);                    // FF3 0x0014bf78: IngameInit, IngameNewGameReset
int  CineMain(void);                    // FF3 0x0014c160: IngameMain; 1 while a projector runs
void CineDraw(void);                    // FF3 0x0014c118: IngameDrawSub
void CineEnd(void);                     // FF3 0x0014c180: IngameEnd; deletes the projector

int  CineGetProjectorAreaNo(void);      // FF3 0x0014bf30: the area of the last CineFilmPlayReq
int  CineGetProjectorSpotId(void);      // FF3 0x0014c4a0: the projector's light slot in the player's area, else 0x7fffffff

// Starts the film of the projector in area: with film >= 0 that film is put
// in first. 1 when the projector holds a film (it plays, or already did).
int  CineFilmPlayReq(int area, int film);       // FF3 0x0014c248: EvCineFilmPlay, the cine menu
int  CineFilmIsPlay(void);              // FF3 0x0014c310: CINE_FILM_END
void CineFilmStop(void);                // FF3 0x0014c320: CINE_FILM_STOP
int  CineFilmSelect(int no);            // FF3 0x0014c550: film 6 reads as 7 once 6 was played through
int  CineGetFilm(void);                 // FF3 0x0014c378: the current projector's film, -1 (no callers)

int  CineFlagCheck(int no, u_char mask);        // FF3 0x0014c608: CINE_FLAG_CHECK
void CineFlagWatchPhoto(void);          // FF3 0x0014c6f0: PlyrFinderShot

int  CineSaveGetProjector(void **addr); // FF3 0x0014c570: save_game_data entry 39 {cine_film, 0xc}
int  CineSaveGetFlag(void **addr);      // FF3 0x0014c7b0: save_game_data entry 40 {cine_flag, 0xb}

#endif /* _INGAME_CINE_FILM_H */
