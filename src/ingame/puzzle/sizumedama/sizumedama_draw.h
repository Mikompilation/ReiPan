/* ==========================================================================
 *  ingame/puzzle/sizumedama/sizumedama_draw.h
 *
 *  The sizumedama puzzle's board definition and its PZL_SIZUMEDAMA layout
 *  hooks: "sphere" draws each pedestal's stone, "sphere2D" the stone under
 *  the stone cursor and "spherename_M" its name, "R flea_sphere" /
 *  "R flea_base" the glow on the cursor pedestal (on its stone / on the
 *  empty base; on every stone once solved), "jimaku_M" the cursor
 *  pedestal's subtitle line (once solved the running subtitle) with items
 *  0x40b / 0x400 / 0x40c framing it; the button guide and the stone
 *  cursor's arrows.
 *  FF3 0x24c1c0..0x24ca90, FF3's own code.
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_DRAW_H
#define _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_DRAW_H

#include "ingame/puzzle/sizumedama/sizumedama_board.h"  // SIZUME_BOARD_DEF

#define SIZUME_BOARD_DEF_NUM    1       // PuzzleGetIdx 0

const SIZUME_BOARD_DEF *SizumeGetBoardDef(int idx);     // FF3 0x0024c1c0: data 0x361ec8[idx]
void SizumeDrawButtons(int show);       // FF3 0x0024c870: the guide, not under a phase-12 message
void SizumeDrawInit(void);              // FF3 0x0024c970: hooks (on open)
void SizumeDraw(void);                  // FF3 0x0024c9f0: phase 75's after

#endif /* _INGAME_PUZZLE_SIZUMEDAMA_SIZUMEDAMA_DRAW_H */
