/* ==========================================================================
 *  ingame/puzzle/kaza/kaza_draw.h
 *
 *  The kaza puzzle's board definitions and draw (phase 73's after): the
 *  PZL_KAZA layout's moves-left digit, each ring turned to its angle with
 *  the cursor frame on the chosen outer ring, and the button guide.
 *  FF3 0x24acc8..0x24af48, FF3's own code.
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAZA_KAZA_DRAW_H
#define _INGAME_PUZZLE_KAZA_KAZA_DRAW_H

#include "ingame/puzzle/kaza/kaza_board.h"      // KAZA_BOARD_DEF

const KAZA_BOARD_DEF *KazaGetBoardDef(int idx); // FF3 0x0024acc8: data 0x361da0[idx]
void KazaDraw(void);                    // FF3 0x0024af00: the whole layout, every frame

void KazaDrawMouse(int csr);            // PC: the outer rings point cursor csr at themselves

#endif /* _INGAME_PUZZLE_KAZA_KAZA_DRAW_H */
