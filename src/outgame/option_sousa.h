/* ==========================================================================
 *  outgame/option_sousa.h
 *
 *  Phase 22, the option "Game Play" (controls) screen (FF3-only; FF3
 *  0x222640..0x222d50): four on / off rows on the layout OPTION_SOUSA_PHF
 *  (+ language), under menu cursor 0x16, run inside phase 7 Option_Main
 *  (outgame/option.c).
 * ======================================================================== */
#ifndef _OUTGAME_OPTION_SOUSA_H
#define _OUTGAME_OPTION_SOUSA_H

#define OPTION_SOUSA_CSR    0x16        // menu cursor of the screen (1 x 4, OptionMenuCsrInit)
#define OPTION_SOUSA_ROWS   4

int  init_OptionSousa(void);
int  one_OptionSousa(void);
int  end_OptionSousa(void);

#endif /* _OUTGAME_OPTION_SOUSA_H */
