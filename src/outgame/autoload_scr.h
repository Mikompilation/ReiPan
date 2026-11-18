/* ==========================================================================
 *  outgame/autoload_scr.h
 *
 *  The screen of the boot memory-card auto load (FF3-only, FF3
 *  0x130210..0x130638): phase 14 AutoLoad_Main (outgame/boot.c) opens it
 *  around AutoLoadMain (outgame/autoload.c). It keeps the yes/no window
 *  (layout anim type 10) on screen with the autoload's current message
 *  (0x83, msg_id) and, in its two question steps, the yes/no cursor; the
 *  common fade layer's label 10 is the backdrop, darkened in only behind
 *  the error messages that wait for OK.
 * ======================================================================== */
#ifndef _OUTGAME_AUTOLOAD_SCR_H
#define _OUTGAME_AUTOLOAD_SCR_H

#define AUTO_LOAD_SCR_MSG_KIND  0x83    // message kind of every autoload message
#define AUTO_LOAD_SCR_ANIM      10      // layout anim type: the yes/no window
#define AUTO_LOAD_SCR_FADE_LABEL 10     // common fade layer label behind it

// Phase 14's sequence (one_AutoLoad_Main).
void AutoLoadScrInit(void);             // opens the window, backdrop alpha 0
int  AutoLoadScrIsReady(void);          // window loaded and fade layer ready: shows label 10
int  AutoLoadScrIsOpen(void);           // always 1
void AutoLoadScrMain(void);             // per frame while AutoLoadMain runs
void AutoLoadScrClose(void);            // hides the fade layer over 30 frames, closes the window
int  AutoLoadScrIsClosed(void);         // always 1 (fades the backdrop one step)
void AutoLoadScrEnd(void);              // backdrop alpha 0, window released

// The autoload's confirm steps wait for the window before reading the pad.
int  AutoLoadScrMsgIsOpen(void);        // 1: no window or the window is open (state 2)

#endif /* _OUTGAME_AUTOLOAD_SCR_H */
