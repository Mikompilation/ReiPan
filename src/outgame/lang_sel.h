/* ==========================================================================
 *  outgame/lang_sel.h
 *
 *  Phase 2, LangSel_Main (FF2 outgame/lang_sel.c), FF3 0x1ecdd8..0x1ed658:
 *  the language select. Five flags in a column with the language names
 *  beside them over the common fade layer's label 10; up/down move the
 *  cursor, OK stores the language (SYS_WRK.language = cursor + 2) and the
 *  boot goes on to phase 3 (Boot_Main). FF3 dropped FF2's own background
 *  and outgame heap; the screen fades in and out through an alpha of its
 *  own, and soft reset is locked while it runs.
 * ======================================================================== */
#ifndef _OUTGAME_LANG_SEL_H
#define _OUTGAME_LANG_SEL_H

int init_LangSel_Main(void);
int one_LangSel_Main(void);
int end_LangSel_Main(void);

#endif /* _OUTGAME_LANG_SEL_H */
