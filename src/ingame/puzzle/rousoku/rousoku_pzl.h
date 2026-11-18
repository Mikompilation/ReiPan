/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_pzl.h
 *
 *  The rousoku (candle) puzzle, PUZZLE_ROUSOKU of puzzle.c's table: its
 *  open / has-item hooks (it has no close) and its phases. Phase 76
 *  Story_PzlRousoku (under 49 Story_Puzzle) waits for its layout, shows the
 *  rules as a phase-12 message, then its children run the board:
 *    104 PzlRousoku_Select   the cursor moves between the candle places of
 *                            the halves that hold a movable candle; OK
 *                            lights or puts out a movable candle (a buzz on
 *                            a fixed place), cancel asks to quit
 *    105 PzlRousoku_Check    the stake's shadow swings to the new sum;
 *                            balanced (0, 0): every candle lights and
 *                            flares, PuzzleClear and the "solved" message;
 *                            else back to 104
 *  Its end plays the layout's "out", releases the layout and the board.
 *  Four boards (PuzzleGetIdx 0..3), no item needed.
 *  FF3 0x2447b0..0x244e38 + 0x2be128..0x2be300, FF3's own code (FF2's
 *  roku_pzl.c is a different puzzle; this file took its link-order slot).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_ROUSOKU_ROUSOKU_PZL_H
#define _INGAME_PUZZLE_ROUSOKU_ROUSOKU_PZL_H

#include "ingame/puzzle/rousoku/rousoku_board.h"    // ROUSOKU_BOARD

extern ROUSOKU_BOARD *rousoku_board;    // sdata 0x371e48

void RousokuBoardCreate(void);          // FF3 0x002447b0: the current board (PuzzleGetIdx), cursor on a movable half
void RousokuBoardDelete(void);          // FF3 0x00244828

void RousokuPzlOpen(void);              // FF3 0x00244868: puzzle table open
int  RousokuPzlHasItem(void);           // FF3 0x00244888: puzzle table has-item (always 1)

// Phase 76 and its children 104 / 105.
int  init_Story_PzlRousoku(void);       // FF3 0x00244890
int  one_Story_PzlRousoku(void);        // FF3 0x00244928
int  end_Story_PzlRousoku(void);        // FF3 0x002449b0
void pre_Story_PzlRousoku(void);        // FF3 0x00244a80
void after_Story_PzlRousoku(void);      // FF3 0x00244a88
int  init_PzlRousoku_Select(void);      // FF3 0x00244aa0
int  one_PzlRousoku_Select(void);       // FF3 0x00244aa8
int  end_PzlRousoku_Select(void);       // FF3 0x00244c80
int  init_PzlRousoku_Check(void);       // FF3 0x00244c88
int  one_PzlRousoku_Check(void);        // FF3 0x00244c90
int  end_PzlRousoku_Check(void);        // FF3 0x00244e10

#endif /* _INGAME_PUZZLE_ROUSOKU_ROUSOKU_PZL_H */
