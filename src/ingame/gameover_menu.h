/* ==========================================================================
 *  ingame/gameover_menu.h
 *
 *  The game over menu (phase 40 Story_LoadGame, one_Story_LoadGame): the
 *  GAMEOVER_PHF layout (+ language) with one row per choice, each naming
 *  the phase it opens. FF3 0x133960..0x1340f8, FF3's own code (file name
 *  invented); the save point menu's twin (ingame/savepoint/savepoint_menu.c,
 *  whose MENU_PHASE_ROW it shares).
 * ======================================================================== */
#ifndef _INGAME_GAMEOVER_MENU_H
#define _INGAME_GAMEOVER_MENU_H

void GameOverMenuCreate(void);                      // FF3 0x00133ad0
int  GameOverMenuIsLoaded(void);                    // FF3 0x00133cb0: plays "in" once loaded
int  GameOverMenuIsInEnd(void);                     // FF3 0x00133df0
int  GameOverMenuSelect(void);                      // FF3 0x00133e18: 1 when a row is chosen
int  GameOverMenuConfirm(void);                     // FF3 0x00133f50: 1 yes, -1 no, 0 waiting
int  GameOverMenuGetPhase(void);                    // FF3 0x00134010: the row's phase, -1 none
int  GameOverMenuOutWait(void);                     // FF3 0x00134018: 1 when "out" is done
void GameOverMenuRelease(void);                     // FF3 0x00134040

#endif /* _INGAME_GAMEOVER_MENU_H */
