/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_pzl.h
 *
 *  The kaza (turning rings) puzzle, PUZZLE_KAZA of puzzle.c's table: its
 *  open / close / has-item hooks and its phases. Phase 73 Story_PzlKaza
 *  (under 49 Story_Puzzle) shows the rules as a phase-12 message, then its
 *  children run the board:
 *    95 PzlKaza_Select   menu cursor 0x5f picks one of the four outer rings,
 *                        OK turns it (and the centre the other way), cancel
 *                        asks to quit
 *    96 PzlKaza_Turn     the rings turn; solved: the layout plays "out",
 *                        PuzzleClear and the "solved" message; out of moves:
 *                        the "failed" message; else back to 95
 *  Two boards (PuzzleGetIdx 0 / 1), needing items 0x2f / 0x30.
 *  FF3 0x24a6c8..0x24acc8, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAZA_KAZA_PZL_H
#define _INGAME_PUZZLE_KAZA_KAZA_PZL_H

#include "ingame/puzzle/kaza/kaza_board.h"      // KAZA_BOARD

extern KAZA_BOARD *kaza_board;          // sdata 0x371f78 (left dangling after KazaBoardDelete, as in FF3)

void KazaBoardCreate(void);             // FF3 0x0024a6c8: the current board (PuzzleGetIdx) from its definition
void KazaBoardDelete(void);             // FF3 0x0024a718

void KazaPzlOpen(void);                 // FF3 0x0024a760: puzzle table open
void KazaPzlClose(void);                // FF3 0x0024a790: puzzle table close
int  KazaPzlHasItem(void);              // FF3 0x0024a7b0: puzzle table has-item (item 0x2f / 0x30 by board)

// Phase 73 and its children 95 / 96.
int  init_Story_PzlKaza(void);          // FF3 0x0024a808
int  one_Story_PzlKaza(void);           // FF3 0x0024a840
int  end_Story_PzlKaza(void);           // FF3 0x0024a8c8
void pre_Story_PzlKaza(void);           // FF3 0x0024a8d0
void after_Story_PzlKaza(void);         // FF3 0x0024a8d8
int  init_PzlKaza_Select(void);         // FF3 0x0024a8f0
int  one_PzlKaza_Select(void);          // FF3 0x0024a8f8
int  end_PzlKaza_Select(void);          // FF3 0x0024aa60
int  init_PzlKaza_Turn(void);           // FF3 0x0024aa68
int  one_PzlKaza_Turn(void);            // FF3 0x0024ab48
int  end_PzlKaza_Turn(void);            // FF3 0x0024aca0

#endif /* _INGAME_PUZZLE_KAZA_KAZA_PZL_H */
