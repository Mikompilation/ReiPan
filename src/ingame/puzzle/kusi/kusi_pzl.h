/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_pzl.h
 *
 *  The kusi puzzle, PUZZLE_KUSI of puzzle.c's table (three boards: yellow,
 *  blue, red): its open / close / has-item hooks and its phases. Phase 72
 *  Story_PzlKusi (under 49 Story_Puzzle) shows the rules as a phase-12
 *  message, then its children run the board:
 *    91 PzlKusi_Select  the cursor over the 3 x 3 cells; OK picks a piece
 *                       with moves left, cancel takes back the last move (or
 *                       asks to quit), circle starts over once a move was made
 *    92 PzlKusi_Move    a direction moves the picked piece one linked cell;
 *                       OK ends its turn, cancel steps it back
 *    93 PzlKusi_Slide   the slide, then the move (or undo) is made; solved:
 *                       PuzzleClear and the "solved" message
 *    94 PzlKusi_Retry   a fresh board behind the fading screen capture
 *  FF3 0x245d38..0x246958, FF3's own code (FF2 has no such puzzle).
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KUSI_KUSI_PZL_H
#define _INGAME_PUZZLE_KUSI_KUSI_PZL_H

#include "ingame/puzzle/kusi/kusi_board.h"      // KUSI_BOARD

#define KUSI_CSR            0x5b        // the MenuCsr over the cells (3 x 3, item = cell)

extern KUSI_BOARD *kusi_board;          // sdata 0x371eb0 (left dangling after KusiBoardDelete, as in FF3)

void KusiBoardCreate(void);             // FF3 0x00245d38: the current board (PuzzleGetIdx) from its definition
void KusiBoardDelete(void);             // FF3 0x00245d88

int  KusiCsrGetCell(int dir);           // FF3 0x00245ff8: the cell next to the cursor (KUSI_DIR_*), -1 for none
int  KusiCapIsOn(void);                 // FF3 0x00246930: the captions fade (set once the board is in play)

void KusiPzlOpen(void);                 // FF3 0x00245f18: puzzle table open
void KusiPzlClose(void);                // FF3 0x00245f60: puzzle table close
int  KusiPzlHasItem(void);              // FF3 0x00245f80: puzzle table has-item (items 43 / 44 / 45 by board)

// Phase 72 and its children 91..94.
int  init_Story_PzlKusi(void);          // FF3 0x00246108
int  one_Story_PzlKusi(void);           // FF3 0x00246110
int  end_Story_PzlKusi(void);           // FF3 0x00246198
void pre_Story_PzlKusi(void);           // FF3 0x002461a0
void after_Story_PzlKusi(void);         // FF3 0x002461a8
int  init_PzlKusi_Select(void);         // FF3 0x002461c0
int  one_PzlKusi_Select(void);          // FF3 0x002461e0
int  end_PzlKusi_Select(void);          // FF3 0x00246440
int  init_PzlKusi_Move(void);           // FF3 0x00246448
int  one_PzlKusi_Move(void);            // FF3 0x00246450
int  end_PzlKusi_Move(void);            // FF3 0x002465e8
int  init_PzlKusi_Slide(void);          // FF3 0x00246608
int  one_PzlKusi_Slide(void);           // FF3 0x002466d8
int  end_PzlKusi_Slide(void);           // FF3 0x00246868
int  init_PzlKusi_Retry(void);          // FF3 0x00246870
int  one_PzlKusi_Retry(void);           // FF3 0x002468a8
int  end_PzlKusi_Retry(void);           // FF3 0x00246928

#endif /* _INGAME_PUZZLE_KUSI_KUSI_PZL_H */
