/* ==========================================================================
 *  ingame/puzzle/sizumedama/sizumedama_pzl.h
 *
 *  The sizumedama (stone pedestal) puzzle, PUZZLE_SIZUMEDAMA of puzzle.c's
 *  table: its open / has-item hooks (it has no close) and its phases. Phase
 *  75 Story_PzlSizume (under 49 Story_Puzzle) opens its own CWindowMsg,
 *  waits for its layout, shows the rules as a phase-12 message in that
 *  window, then its children run the board:
 *    100 PzlSizume_Daiza      the pedestal cursor (MenuCsr 100); OK on an
 *                             empty pedestal chooses a stone (101), on a
 *                             stone asks to take it back (102); key 10 on a
 *                             stone plays its voice (103); cancel asks to quit
 *    101 PzlSizume_Stone      the stone cursor (MenuCsr 0x65, placed stones
 *                             filtered out); OK puts the stone (then 103),
 *                             cancel goes back to 100
 *    102 PzlSizume_Remove     takes the pedestal's stone back, then 100
 *    103 PzlSizume_Play       the pedestal's voice: solved, 116 plays the
 *                             whole recording with its subtitles, PuzzleClear
 *                             and the "solved" message; else 117 plays the
 *                             stone's clip and returns to 100
 *  Its end plays the layout's "out", releases the layout, the window and the
 *  board. One board (PuzzleGetIdx 0), no item needed.
 *  FF3 0x24b490..0x24c1c0, FF3's own code (the link order put it in FF2's
 *  savepoint_main.c slot; four FF2 save point names matched it wrongly).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_PZL_H
#define _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_PZL_H

#include "ingame/puzzle/sizumedama/sizumedama_board.h"  // SIZUME_BOARD

class CWindowMsg;                       // common/WindowMsg.h

extern SIZUME_BOARD *sizume_board;      // sdata 0x371fa0
extern CWindowMsg   *sizume_win;        // sbss 0x3738c8: the window phase 12 shows the puzzle's messages in

void SizumeBoardCreate(void);           // FF3 0x0024b4d8: the board (PuzzleGetIdx), cursors 100 / 0x65
void SizumeBoardDelete(void);           // FF3 0x0024b558

void SizumePzlOpen(void);               // FF3 0x0024b5b8: puzzle table open
int  SizumePzlHasItem(void);            // FF3 0x0024b5d8: puzzle table has-item (always 1)

// Phase 75 and its children 100..103, 116, 117.
int  init_Story_PzlSizume(void);        // FF3 0x0024b5e0
int  one_Story_PzlSizume(void);         // FF3 0x0024b6b8
int  end_Story_PzlSizume(void);         // FF3 0x0024b740
void pre_Story_PzlSizume(void);         // FF3 0x0024b860
void after_Story_PzlSizume(void);       // FF3 0x0024b868
int  init_PzlSizume_Daiza(void);        // FF3 0x0024b8c8
int  one_PzlSizume_Daiza(void);         // FF3 0x0024b8e8
int  end_PzlSizume_Daiza(void);         // FF3 0x0024bb98
int  init_PzlSizume_Stone(void);        // FF3 0x0024bba0
int  one_PzlSizume_Stone(void);         // FF3 0x0024bc70
int  end_PzlSizume_Stone(void);         // FF3 0x0024bdd0
int  init_PzlSizume_Remove(void);       // FF3 0x0024bdd8
int  one_PzlSizume_Remove(void);        // FF3 0x0024bde0
int  end_PzlSizume_Remove(void);        // FF3 0x0024be30
int  init_PzlSizume_Play(void);         // FF3 0x0024be38
int  one_PzlSizume_Play(void);          // FF3 0x0024be40
int  end_PzlSizume_Play(void);          // FF3 0x0024beb0
void pre_PzlSizume_Play(void);          // FF3 0x0024bef0
void after_PzlSizume_Play(void);        // FF3 0x0024bef8
int  init_PzlSizume_PlayClear(void);    // FF3 0x0024bf00
int  one_PzlSizume_PlayClear(void);     // FF3 0x0024bf48
int  end_PzlSizume_PlayClear(void);     // FF3 0x0024c0a0
int  init_PzlSizume_PlayWrong(void);    // FF3 0x0024c0a8
int  one_PzlSizume_PlayWrong(void);     // FF3 0x0024c0f0
int  end_PzlSizume_PlayWrong(void);     // FF3 0x0024c198

#endif /* _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_PZL_H */
