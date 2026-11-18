/* ==========================================================================
 *  ingame/puzzle/puzzle.h
 *
 *  The puzzle driver (FF2's puzzle.c, rebuilt for FF3's five puzzles): an
 *  event command picks a puzzle (PuzzleSet) and requests phase 49
 *  Story_Puzzle, which captures the screen, loads the puzzle's layout and SE
 *  bank and hands over to the puzzle's own phase -- or, when the puzzle's
 *  item is not held, to phase 71, a phase-12 message over the background
 *  layout. The clear flags live here and go into the save.
 *  FF3 0x243e00..0x2447b0, FF3's own code (FF2 had a different driver).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_PUZZLE_H
#define _INGAME_PUZZLE_PUZZLE_H

// Puzzle types (PuzzleSet's type). Each has 1..4 boards (PuzzleSet's idx).
enum PUZZLE_TYPE
{
    PUZZLE_KAZA       = 0,              // 2 boards: PZL_KAZA1/2_PHF, phase 73
    PUZZLE_KAMON      = 1,              // 1: PZL_KAMON_PHF, phase 74
    PUZZLE_KUSI       = 2,              // 3: PZL_KUSI_YLW/BL/RD_PHF, phase 72
    PUZZLE_SIZUMEDAMA = 3,              // 1: PZL_SIZUMEDAMA_PHF, phase 75
    PUZZLE_ROUSOKU    = 4,              // 4: PZL_ROUSOKU_PHF, phase 76
    PUZZLE_NONE       = 5,              // after end_Story_Puzzle
    PUZZLE_TYPE_NUM   = 5,
};

void PuzzleSet(int type, int idx);      // FF3 0x002441e8: the next puzzle (then request phase 49)
int  PuzzleGetType(void);               // FF3 0x00244108
int  PuzzleGetIdx(void);                // FF3 0x00244110
int  PuzzleGetLayoutFile(void);         // FF3 0x00244120: the current puzzle's Bp layout
int  PuzzleHasItem(void);               // FF3 0x00243ed0: the puzzle's item is held (types 3, 4: always)
short PuzzleGetRetPhase(void);          // FF3 0x00243f80: the phase that requested the puzzle

int  PuzzleGetSaveWrk(void **addr);     // FF3 0x00243f40: {clear flags 0x3f7a20, 0x2c}
void PuzzleClearInfoReset(void);        // FF3 0x00243f58: all clear flags 0
int  GetPuzzleClearInfo(int type, int idx);     // FF3 0x00243f88: the board's clear flag
void PuzzleClear(void);                 // FF3 0x00244030: the current board solved

void  PuzzleSetCapAlpha(short alpha);   // FF3 0x002441f8: screen capture alpha, clamped 0..0x80
short PuzzleGetCapAlpha(void);          // FF3 0x00244220

// Phase 49 Story_Puzzle and its child 71 Story_PuzzleNoItem.
int  init_Story_Puzzle(void);           // FF3 0x00244228
int  one_Story_Puzzle(void);            // FF3 0x002444a8
int  end_Story_Puzzle(void);            // FF3 0x00244348
void pre_Story_Puzzle(void);            // FF3 0x00244648
void after_Story_Puzzle(void);          // FF3 0x00244650
int  init_Story_PuzzleNoItem(void);     // FF3 0x002446c8
int  one_Story_PuzzleNoItem(void);      // FF3 0x002446d8
int  end_Story_PuzzleNoItem(void);      // FF3 0x002446d0

// The puzzles' open / close / has-item functions (table 0x361600) are in
// the puzzles' own headers: ingame/puzzle/kaza/kaza_pzl.h, kamon/kamon_pzl.h,
// kusi/kusi_pzl.h, sizumedama/sizumedama_pzl.h, rousoku/rousoku_pzl.h.

#endif /* _INGAME_PUZZLE_PUZZLE_H */
