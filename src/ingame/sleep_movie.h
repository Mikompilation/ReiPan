/* ==========================================================================
 *  ingame/sleep_movie.h
 *
 *  FF3-only object between unit_ctl.c and the libstdc++ throw helpers (FF3
 *  0x25fce0..0x25fdb8; the code is unnamed, the file is named for what it
 *  holds): the wakeup / sleep movie work (bss 0x424b00, save block 23).
 *  SET_WAKEUP_MOVIE / SET_SLEEP_MOVIE (ev_command.c) pick a scene and
 *  WAKEUP_MOVIE_PLAY / SLEEP_MOVIE_PLAY play it once through MovieReqPlay
 *  (phase 0x27 Story_SceneMovie). Ghidra keeps the old guess "FinderBank" in
 *  the names (GetFinderBankWrk, FinderBankWrkInit, FinderBankSaveGet), and
 *  the type keeps it here so the names stay searchable.
 *
 *  The singleton getter GetFinderBankWrk (FF3 0x2b8f28) rides with
 *  ev_command.c, its main caller.
 * ======================================================================== */
#ifndef _INGAME_SLEEP_MOVIE_H
#define _INGAME_SLEEP_MOVIE_H

#include <stddef.h>                     // offsetof

// layout: EE -- ints only
struct FINDER_BANK_MOVIE                /* 0x8 */
{
    /* 0x0 */ int no;                   // scene number for MovieReqPlay; -1: none
    /* 0x4 */ int played;               // set by the PLAY command; passed on as MovieReqPlay's unread a1
};
static_assert(sizeof(FINDER_BANK_MOVIE) == 0x8, "FINDER_BANK_MOVIE is 8 bytes");
static_assert(offsetof(FINDER_BANK_MOVIE, played) == 0x04, "FINDER_BANK_MOVIE.played");

// Movie kinds, the index into the work (MoviePlay's argument).
#define SLEEP_MOVIE_KIND_WAKEUP     0
#define SLEEP_MOVIE_KIND_SLEEP      1

// A C++ object in FF3: the static's constructor runs Init (IngameInit and
// IngameNewGameReset call it too), and FF3 registers its empty destructor
// 0x1a12c8 with atexit.
struct FINDER_BANK_WRK                  /* 0x10 */
{
    FINDER_BANK_WRK();                  // runs Init

    void Init();                        // FF3 0x0025fd38 (FinderBankWrkInit): both {-1, 0}
    void MoviePlay(int kind);           // FF3 0x0025fd68 (FinderBankMoviePlay): plays movie `kind` unless start mode 2 or none set

    /* 0x00 */ FINDER_BANK_MOVIE wakeup;
    /* 0x08 */ FINDER_BANK_MOVIE sleep;
};
static_assert(sizeof(FINDER_BANK_WRK) == 0x10, "FINDER_BANK_WRK is 0x10 bytes");
static_assert(offsetof(FINDER_BANK_WRK, sleep) == 0x08, "FINDER_BANK_WRK.sleep");

FINDER_BANK_WRK *GetFinderBankWrk(void);                // FF3 0x002b8f28 (ev_command.c)

int FinderBankSaveGet(void **out);                      // FF3 0x0025fce0: save block 23, 0x10 bytes

#endif /* _INGAME_SLEEP_MOVIE_H */
