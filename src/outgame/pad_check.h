/* ==========================================================================
 *  outgame/pad_check.h
 *
 *  Phase 1, Boot_PadCheck (FF2 outgame/pad_check.c), FF3 0x225a58..0x225dc8:
 *  the first boot phase. It loads the boot data and the common fade layer,
 *  then checks pad 0; when the pad cannot enter pressure mode it shows a
 *  message (layout animation 0xc) until it is dismissed, then goes on to
 *  phase 2 (LangSel_Main). The message helpers are shared with the pause
 *  menu's pad code (0x14fa28).
 * ======================================================================== */
#ifndef _OUTGAME_PAD_CHECK_H
#define _OUTGAME_PAD_CHECK_H

int  PadErrMsgOpen(void);               // starts the message; returns its layout anim slot
int  PadErrMsgIsClosed(int slot);       // 1 when slot < 0 or the message has finished
void PadErrMsgRelease(int slot);        // nothing in FF3

int init_Boot_PadCheck(void);
int one_Boot_PadCheck(void);
int end_Boot_PadCheck(void);

#endif /* _OUTGAME_PAD_CHECK_H */
