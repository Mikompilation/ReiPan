/* ==========================================================================
 *  ingame/clear/ClearMenu.h
 *
 *  Phase 43 (0x2b), the menu the game clear ends in (CGameClear requests it,
 *  GameClear.c), and its empty child phase 70. Over the outgame backdrop
 *  movie (OUTGAME_BG_PSS) a select menu (the module at 0x1ed658, list
 *  0x350aa0) offers three rows: save (phase 0x45 Story_SaveGame_Mc), the
 *  album (phase 8) and the title (phase 5, after a yes/no confirm that stops
 *  every sound). FF3-only, FF3 0x14c7c8..0x14cd30; the file name is invented.
 * ======================================================================== */
#ifndef _INGAME_CLEAR_CLEARMENU_H
#define _INGAME_CLEAR_CLEARMENU_H

#include "sdk/scetypes.h"

int  init_Story_ClearMenu(void);        // FF3 0x0014c7e0
int  one_Story_ClearMenu(void);         // FF3 0x0014ca90
int  end_Story_ClearMenu(void);         // FF3 0x0014c9b8
void pre_Story_ClearMenu(void);         // FF3 0x0014ccb0
void after_Story_ClearMenu(void);       // FF3 0x0014ccc8

int  init_Story_ClearMenu_Main(void);   // FF3 0x0014c7c8: phase 70
int  one_Story_ClearMenu_Main(void);    // FF3 0x0014c7d8
int  end_Story_ClearMenu_Main(void);    // FF3 0x0014c7d0

#endif /* _INGAME_CLEAR_CLEARMENU_H */
