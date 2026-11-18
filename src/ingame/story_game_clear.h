/* ==========================================================================
 *  ingame/story_game_clear.h
 *
 *  Phase 42 (0x2a), Story_GameClear: the game-clear sequence. init news a
 *  CGameClear (ingame/clear/GameClear.c: ending movie, results page, reward
 *  lists, then phase 43), one runs it a frame at a time, end deletes it.
 *  Requested by GameClearRequest (GAME_CLEAR_, EV_MISSION_CLEAR / FAILED).
 *  FF3-only, FF3 0x258a68..0x258b20 (between snd_ctrl.c and tel_talk.c);
 *  the file name is invented.
 * ======================================================================== */
#ifndef _INGAME_STORY_GAME_CLEAR_H
#define _INGAME_STORY_GAME_CLEAR_H

#include "sdk/scetypes.h"

int  init_Story_GameClear(void);        // FF3 0x00258a68
int  one_Story_GameClear(void);         // FF3 0x00258ad8
int  end_Story_GameClear(void);         // FF3 0x00258aa0

#endif /* _INGAME_STORY_GAME_CLEAR_H */
