/* ==========================================================================
 *  ingame/story_save_mc.h
 *
 *  Phase 69, Story_SaveGame_Mc (child of 41 Story_SaveGame): the memory-card
 *  save screen. FF3 0x250268..0x250850, FF3-only file (name mirrors
 *  story_load_mc.c, phase 68).
 * ======================================================================== */
#ifndef _INGAME_STORY_SAVE_MC_H
#define _INGAME_STORY_SAVE_MC_H

int init_Story_SaveGame_Mc(void);
int one_Story_SaveGame_Mc(void);
int end_Story_SaveGame_Mc(void);

#endif /* _INGAME_STORY_SAVE_MC_H */
