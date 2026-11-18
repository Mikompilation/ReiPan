/* ==========================================================================
 *  ingame/puzzle/kusi/kusi_draw.h
 *
 *  The kusi puzzle's tables and draw: the board definitions, the cells'
 *  positions, and the PZL_KUSI layout's item hooks -- each piece with its
 *  moves left, the cursor, the arrows round the piece being moved (where it
 *  can go, and where cancel steps it back), and the three button captions
 *  that fade between "select" (no move yet), "moves made" and "moving".
 *  FF3 0x246958..0x247710, FF3's own code.
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KUSI_KUSI_DRAW_H
#define _INGAME_PUZZLE_KUSI_KUSI_DRAW_H

#include "ingame/puzzle/kusi/kusi_board.h"      // KUSI_BOARD_DEF, KUSI_POS, KUSI_PIECE

const KUSI_BOARD_DEF *KusiGetBoardDef(int idx);     // FF3 0x00246958: data 0x330b38[idx]
const KUSI_POS *KusiGetCellPos(int cell);           // FF3 0x002474d0: data 0x330be0[cell]
KUSI_PIECE *KusiFindPiece(int cell);                // FF3 0x00246978: kusi_board's piece on a cell

void KusiDrawInit(void);                // FF3 0x002474e8: the layout's item hooks, captions cleared
void KusiDraw(void);                    // FF3 0x002476d8: every frame (the captions)

// PC: the mouse. Pointing at the cells (no piece picked), or at the picked
// piece and the cells around it.
void KusiDrawMouseSelect(int csr);
void KusiDrawMouseMove(void);

#endif /* _INGAME_PUZZLE_KUSI_KUSI_DRAW_H */
