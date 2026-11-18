/* ==========================================================================
 *  outgame/option_top.h
 *
 *  Phase 19, the option top menu (FF3-only; FF3 0x223e60..0x224618): the six
 *  entries of the option layout OPTION_TOP_PHF (+ language) under menu
 *  cursor 0x13, run inside phase 7 Option_Main (outgame/option.c).
 * ======================================================================== */
#ifndef _OUTGAME_OPTION_TOP_H
#define _OUTGAME_OPTION_TOP_H

#define OPTION_TOP_CSR      0x13        // menu cursor of the top menu (1 x 6, OptionMenuCsrInit)
#define OPTION_TOP_MSG      0x65        // message: rows 0..5 entry names, 6..11 help lines, 12 / 13 prompts
#define OPTION_TOP_ROWS     6
#define OPTION_TOP_ROW_DY   0x27        // 39 px between rows (text and cursor bar)

void OptionTopAnimStart(void);          // the entries' draw hook; init_Option_Main's last step

int  init_OptionTop(void);
int  one_OptionTop(void);
int  end_OptionTop(void);

#endif /* _OUTGAME_OPTION_TOP_H */
