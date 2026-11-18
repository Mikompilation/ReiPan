/* ==========================================================================
 *  ingame/puzzle/rousoku/rousoku_draw.h
 *
 *  The rousoku puzzle's board definitions and its PZL_ROUSOKU layout hooks:
 *  each candle item draws its flame, R_light the glow on the cursor's
 *  candle, R_kuikage00 the stake's shadow (pointing along the lit candles'
 *  summed positions, swinging over from the previous sum after a move),
 *  and the button guide.
 *  FF3 0x244e38..0x245580 + 0x2be300..0x2be570, FF3's own code.
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_ROUSOKU_ROUSOKU_DRAW_H
#define _INGAME_PUZZLE_ROUSOKU_ROUSOKU_DRAW_H

#include "ingame/puzzle/rousoku/rousoku_board.h"    // ROUSOKU_BOARD_DEF

#define ROUSOKU_BOARD_DEF_NUM   4       // PuzzleGetIdx 0..3

const ROUSOKU_BOARD_DEF *RousokuGetBoardDef(int idx);   // FF3 0x00244e38: data 0x361720[idx]
void RousokuDrawButtons(int show);      // FF3 0x002452d8: item 0x3f7, not under a phase-12 message
void RousokuDrawInit(void);             // FF3 0x00245360: hooks, the fixed candles' plates (on open)
void RousokuDraw(void);                 // FF3 0x00245518: phase 76's after (also steps the board)

#include "reipan/io/reipan_menu_mouse.h"
// PC: the mouse: where candle `cell` is drawn, around its flame and plate.
int RousokuDrawMouseBox(int cell, ReiPan_MenuBox *box);

#endif /* _INGAME_PUZZLE_ROUSOKU_ROUSOKU_DRAW_H */
