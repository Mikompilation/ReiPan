/* ==========================================================================
 *  outgame/option_system.h
 *
 *  Phase 20, the option "System Settings" screen (FF3-only; FF3
 *  0x223578..0x223e60): vibration, subtitles, stereo / mono and the two
 *  volumes on the layout OPTION_SYSTEM_PHF (+ language), under menu cursor
 *  0x14, run inside phase 7 Option_Main (outgame/option.c).
 * ======================================================================== */
#ifndef _OUTGAME_OPTION_SYSTEM_H
#define _OUTGAME_OPTION_SYSTEM_H

#define OPTION_SYSTEM_CSR   0x14        // menu cursor of the screen (1 x 5, OptionMenuCsrInit)
#define OPTION_SYSTEM_ROWS  5

int  init_OptionSystem(void);
int  one_OptionSystem(void);
int  end_OptionSystem(void);

#endif /* _OUTGAME_OPTION_SYSTEM_H */
