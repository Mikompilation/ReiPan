/* ==========================================================================
 *  ingame/puzzle/kamon/kamon_draw.h
 *
 *  The kamon puzzle's tables and draw (phase 74's after): the board
 *  definitions, the slot positions, and the PZL_KAMON layout's items --
 *  moves left, pieces and cursor, links, buttons -- plus the two fades that
 *  play when the board is solved (the missing piece appears in the hole,
 *  then every link and piece lights up).
 *  FF3 0x248f80..0x249930, FF3's own code.
 * ======================================================================== */
#ifndef _INGAME_PUZZLE_KAMON_KAMON_DRAW_H
#define _INGAME_PUZZLE_KAMON_KAMON_DRAW_H

#include "ingame/puzzle/kamon/kamon_board.h"    // KAMON_BOARD_DEF, KAMON_POS

const KAMON_BOARD_DEF *KamonGetBoardDef(int idx);  // FF3 0x00248f80: data 0x361c50[idx]
const KAMON_POS *KamonGetSlotPos(int slot);         // FF3 0x00249888: data 0x330c28[slot]

void KamonDraw(void);                   // FF3 0x00249848: the whole layout, every frame
void KamonFadeReset(void);              // FF3 0x00249838: both fades off
void KamonPieceFadeInStart(void);       // FF3 0x002498a0
int  KamonPieceFadeInIsEnd(void);       // FF3 0x002498b0
void KamonClearGlowStart(void);         // FF3 0x002498d8
int  KamonClearGlowIsEnd(void);         // FF3 0x002498e8

#endif /* _INGAME_PUZZLE_KAMON_KAMON_DRAW_H */
