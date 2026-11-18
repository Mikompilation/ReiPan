/* ==========================================================================
 *  outgame/option_akarusa.h
 *
 *  Phase 21, the option "Brightness" screen (akarusa: brightness; FF3-only,
 *  FF3 0x222170..0x222640): a slider over the brightness level (option byte
 *  0, 0..11) on the layout OPTION_AKARUSA_PHF (+ language), run inside
 *  phase 7 Option_Main (outgame/option.c).
 * ======================================================================== */
#ifndef _OUTGAME_OPTION_AKARUSA_H
#define _OUTGAME_OPTION_AKARUSA_H

#define OPTION_AKARUSA_LEVELS   12      // brightness levels 0..11 (CsrMoveUpDownSe count)

int  init_OptionAkarusa(void);
int  one_OptionAkarusa(void);
int  end_OptionAkarusa(void);

#endif /* _OUTGAME_OPTION_AKARUSA_H */
