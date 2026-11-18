/* ==========================================================================
 *  ingame/sleep_movie.c
 *
 *  The wakeup / sleep movie work; see sleep_movie.h. FF3 0x25fce0..0x25fdb8.
 *
 *  FF3 0x25fdb0 (jr ra; nop) is an empty function nothing references (no
 *  call, jump or data word); not written.
 * ======================================================================== */
#include "ingame/sleep_movie.h"

#include "ingame/ingame_wrk.h"                          // title_start_flg
#include "ingame/movie_room_menu/prg/movie_projecter.h" // MovieReqPlay

// ──────────────────────────────────────────────────────────────────────
// Save data
// ──────────────────────────────────────────────────────────────────────

// Save block 23 (save_data.c's table): the work itself, 0x10 bytes.
// FF3 caches GetFinderBankWrk() in sbss (gp-0xaac); the getter is called
// directly.
// FF3 0x0025fce0.

int FinderBankSaveGet(void **out)
{
    *out = GetFinderBankWrk();
    return sizeof(FINDER_BANK_WRK);
}

// ──────────────────────────────────────────────────────────────────────
// FINDER_BANK_WRK
// ──────────────────────────────────────────────────────────────────────

// Both movies unset: scene -1, not played.
// FF3 0x0025fd38.

void FINDER_BANK_WRK::Init()
{
    wakeup.no     = -1;
    wakeup.played = 0;
    sleep.no      = -1;
    sleep.played  = 0;
}

// Requests movie `kind` (0 wakeup, 1 sleep) when one is set, except in
// start mode 2. FF3 indexes the work by kind * 8; the two callers pass 0
// and 1. The played word goes to MovieReqPlay's a1, which it does not read.
// callcheck: MovieReqPlay is a tail jump in FF3 (j 0x218770), not a jal.
// FF3 0x0025fd68.

void FINDER_BANK_WRK::MoviePlay(int kind)
{
    FINDER_BANK_MOVIE *movie = (kind == SLEEP_MOVIE_KIND_WAKEUP) ? &wakeup : &sleep;

    if (title_start_flg.mode != 2 && movie->no >= 0)
    {
        MovieReqPlay(movie->no, movie->played);
    }
}
