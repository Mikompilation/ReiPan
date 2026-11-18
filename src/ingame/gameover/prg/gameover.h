/* ==========================================================================
 *  ingame/gameover/prg/gameover.h
 *
 *  The game over (FF3 0x1d7a58..0x1d8208). When Rei dies (PlyrDeathCtrl) or
 *  an event says GAMEOVER_REQUEST, GameOverReq sends the story to phase 10,
 *  which silences every sound, plays the "game over" movie (GAMEOVER_PSS)
 *  and hands on to phase 40 Story_LoadGame: the in-game state is reset
 *  (IngameInit) and the game over menu (ingame/gameover_menu.c) runs over
 *  the outgame backdrop movie; its rows load a game (phase 68, linked; a
 *  loaded game goes on through phase 30 Story_NowLoading), open the album
 *  (phase 8, linked) or quit to the title (phase 5). In an extra mission
 *  (title start mode 2) a death is the mission's failure instead
 *  (GameClear mode 5, phase 50). FF2's gameover.c held FF2's three
 *  GID_STORY_GAMEOVER_* phases; FF3 kept only the movie.
 * ======================================================================== */
#ifndef _INGAME_GAMEOVER_PRG_GAMEOVER_H
#define _INGAME_GAMEOVER_PRG_GAMEOVER_H

#include "sdk/scetypes.h"

void GameOverReq(void);                 // FF3 0x001d7a58: GAMEOVER_REQUEST, PlyrDeathCtrl

// Phase 10: the "game over" movie.
int  init_GameOver(void);               // FF3 0x001d7ab0
int  one_GameOver(void);                // FF3 0x001d7b70
int  end_GameOver(void);                // FF3 0x001d7b68

// Phase 40 Story_LoadGame: the game over menu.
int  init_Story_LoadGame(void);         // FF3 0x001d7c68
int  one_Story_LoadGame(void);          // FF3 0x001d7f68
int  end_Story_LoadGame(void);          // FF3 0x001d7e88
void pre_Story_LoadGame(void);          // FF3 0x001d8178
void after_Story_LoadGame(void);        // FF3 0x001d81a0

#endif /* _INGAME_GAMEOVER_PRG_GAMEOVER_H */
