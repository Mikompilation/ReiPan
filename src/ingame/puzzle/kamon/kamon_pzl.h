/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_pzl.h
 *
 *  The kamon (family crest) puzzle, PUZZLE_KAMON of puzzle.c's table: its
 *  open / close / has-item hooks and its phases. Phase 74 Story_PzlKamon
 *  (under 49 Story_Puzzle) shows the rules as a phase-12 message, then its
 *  children run the board:
 *    97 PzlKamon_Select  cursor left / right, OK moves, cancel undoes (or asks
 *                        to quit when nothing is to undo), circle starts over
 *    98 PzlKamon_Move    the piece slides; solved: the missing crest fades in,
 *                        every link lights up, PuzzleClear; out of moves: the
 *                        "failed" message; else back to 97
 *    99 PzlKamon_Retry   a fresh board behind the fading screen capture
 *  FF3 0x248658..0x248f80, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAMON_KAMON_PZL_H
#define _INGAME_PUZZLE_KAMON_KAMON_PZL_H

#include "ingame/puzzle/kamon/kamon_board.h"    // KAMON_BOARD

extern KAMON_BOARD *kamon_board;        // sdata 0x371f18 (left dangling after KamonBoardDelete, as in FF3)

void KamonBoardCreate(void);            // FF3 0x00248658: the current board (PuzzleGetIdx) from its definition
void KamonBoardDelete(void);            // FF3 0x002486a8

void KamonPzlOpen(void);                // FF3 0x002486f0: puzzle table open
void KamonPzlClose(void);               // FF3 0x00248718: puzzle table close
int  KamonPzlHasItem(void);             // FF3 0x00248730: puzzle table has-item (item 0x2e)

// Phase 74 and its children 97..99.
int  init_Story_PzlKamon(void);         // FF3 0x00248758
int  one_Story_PzlKamon(void);          // FF3 0x00248760
int  end_Story_PzlKamon(void);          // FF3 0x002487e8
void pre_Story_PzlKamon(void);          // FF3 0x002487f0
void after_Story_PzlKamon(void);        // FF3 0x002487f8
int  init_PzlKamon_Select(void);        // FF3 0x00248810
int  one_PzlKamon_Select(void);         // FF3 0x00248818
int  end_PzlKamon_Select(void);         // FF3 0x00248a30
int  init_PzlKamon_Move(void);          // FF3 0x00248a38
int  one_PzlKamon_Move(void);           // FF3 0x00248d40
int  end_PzlKamon_Move(void);           // FF3 0x00248e98
int  init_PzlKamon_Retry(void);         // FF3 0x00248ea0
int  one_PzlKamon_Retry(void);          // FF3 0x00248ed8
int  end_PzlKamon_Retry(void);          // FF3 0x00248f58

#endif /* _INGAME_PUZZLE_KAMON_KAMON_PZL_H */
