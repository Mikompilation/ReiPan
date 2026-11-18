/* ==========================================================================
 *  ingame/story_main.h
 *
 *  The story phases around play: 9 Story_Main (the parent of everything in
 *  the game, IngamePre each frame, IngameEnd at the end), 29 Story_Start
 *  (start menu 55 or the card load 68, then new game / load restore), 30
 *  Story_NowLoading (the Now Loading screen while IngameNowLoadingMain
 *  loads) and 32 Story_Normal (IngameMain + IngameDrawSub every frame).
 *  FF3 0x120b18..0x121000; FF2 kept these phases in ingame/ingame.c, FF3 in
 *  a file of their own (name descriptive).
 * ======================================================================== */
#ifndef _INGAME_STORY_MAIN_H
#define _INGAME_STORY_MAIN_H

#include "sdk/scetypes.h"

int  init_Story_Main(void);
int  one_Story_Main(void);
int  end_Story_Main(void);
void pre_Story_Main(void);
void after_Story_Main(void);

void StoryStartSetStep(u_char step);    // FF3 0x00120bc0: the start menu (55) passes 2 = start the game
int  init_Story_Start(void);
int  one_Story_Start(void);
int  end_Story_Start(void);
void pre_Story_Start(void);
void after_Story_Start(void);

int  init_Story_NowLoading(void);
int  one_Story_NowLoading(void);
int  end_Story_NowLoading(void);

int  init_Story_Normal(void);
int  one_Story_Normal(void);
int  end_Story_Normal(void);
void StoryMainNop(void);               // FF3 0x00120fd8: empty, unreferenced

#endif /* _INGAME_STORY_MAIN_H */
