/* ==========================================================================
 *  ingame/story_menu_ghost_list.h
 *
 *  The ghost list, phase 63 StoryMenu_GhostList (a child of Story_Menu 37,
 *  back to 57 StoryMenu_Top), and its child phase 88
 *  StoryMenu_GhostListBonus: the soul list (soul_list.c) as a 9-row scrolled
 *  list of numbers and names, the cursor ghost's picture
 *  (GLIST_JG_000_TM2 + entry), title and description, how many are
 *  photographed, a mark on entries not read yet; OK on a photographed entry
 *  reads it. A newly reached bonus (50 / 100 / 150 / 217 ghosts) opens
 *  phase 88, a jingle and the bonus's message, before the list goes on.
 *  Layout G_LIST_PHF + GetLanguage().
 *  FF3 0x1d84f8..0x1da418; FF3-only file (the FF2 matches in it were FF2
 *  game_result.c / gameover_menu_top.c helpers of the same shape), named
 *  like the other story_menu_*.c phases.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_GHOST_LIST_H
#define _INGAME_STORY_MENU_GHOST_LIST_H

#include "sdk/scetypes.h"

int  init_StoryMenu_GhostList(void);        // FF3 0x001d9860
int  one_StoryMenu_GhostList(void);         // FF3 0x001d9bf0
int  end_StoryMenu_GhostList(void);         // FF3 0x001d9d88
void pre_StoryMenu_GhostList(void);         // FF3 0x001d9eb0
void after_StoryMenu_GhostList(void);       // FF3 0x001d9eb8

int  init_StoryMenu_GhostListBonus(void);   // FF3 0x001d9f38
int  one_StoryMenu_GhostListBonus(void);    // FF3 0x001da1b0
int  end_StoryMenu_GhostListBonus(void);    // FF3 0x001da220

// The message id of field `field` of ghost / subject `no`: three messages
// per entry, 0 the name, 1 the title, 2 the description (message types
// 0x54 here; the album and film menus use it with their subjects' types).
int  SoulListMsgNo(int field, int no);      // FF3 0x001d97a8

#endif /* _INGAME_STORY_MENU_GHOST_LIST_H */
