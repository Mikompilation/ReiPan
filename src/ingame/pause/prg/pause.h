/* ==========================================================================
 *  ingame/pause/prg/pause.h
 *
 *  Phase 44, Story_Pause (FF2 ingame/pause/prg/pause.c), FF3
 *  0x225dc8..0x226138. FF3 draws the pause menu as layout animation 4 over a
 *  screen capture of the game; the menu's own pad and draw code is in the
 *  layout animation callbacks (0x14fa28..), which keep the choice
 *  (PauseMenuGetSel): 0 continue, 1 options (phase 7, then back here),
 *  2 quit to the title (or phase 11 in start mode 2).
 * ======================================================================== */
#ifndef _INGAME_PAUSE_PRG_PAUSE_H
#define _INGAME_PAUSE_PRG_PAUSE_H

int IsPauseEnable(void);                // no lock 0x10 and no layout animation running

int init_Story_Pause(void);
int one_Story_Pause(void);
int end_Story_Pause(void);

#endif /* _INGAME_PAUSE_PRG_PAUSE_H */
