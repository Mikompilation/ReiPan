/* ==========================================================================
 *  outgame/option_button.h
 *
 *  Phase 23, the option "Controller" screen (FF3-only; FF3
 *  0x222d50..0x223578): the button layout 0..2 (OPTION_DAT.pad_cfg) shown
 *  on a pad picture of the layout OPTION_BUTTON_PHF (+ language), in the
 *  normal or the viewfinder view, run inside phase 7 Option_Main
 *  (outgame/option.c).
 * ======================================================================== */
#ifndef _OUTGAME_OPTION_BUTTON_H
#define _OUTGAME_OPTION_BUTTON_H

int  init_OptionButton(void);
int  one_OptionButton(void);
int  end_OptionButton(void);

#endif /* _OUTGAME_OPTION_BUTTON_H */
