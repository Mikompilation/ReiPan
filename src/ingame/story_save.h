/* ==========================================================================
 *  ingame/story_save.h
 *
 *  Phase 41, Story_SaveGame (child of 9 Story_Main, parent of 69
 *  Story_SaveGame_Mc): the save point menu over the captured screen and the
 *  outgame backdrop. FF3 0x250850..0x250d40, FF3-only file (name
 *  descriptive).
 * ======================================================================== */
#ifndef _INGAME_STORY_SAVE_H
#define _INGAME_STORY_SAVE_H

void StorySaveGameReq(void);            // FF3 0x00250850: request phase 41 (the save point, 0x1a1cb8)

int  init_Story_SaveGame(void);
int  one_Story_SaveGame(void);
int  end_Story_SaveGame(void);
void pre_Story_SaveGame(void);
void after_Story_SaveGame(void);

#endif /* _INGAME_STORY_SAVE_H */
