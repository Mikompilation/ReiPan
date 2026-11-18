/* ==========================================================================
 *  ingame/story_menu_photo_view.h
 *
 *  The document views under the document menu (phase 61, story_menu_photo.h):
 *  phase 84 StoryMenu_PhotoDetail, the message view (layout type 0) with the
 *  document's text and pictures, and phase 85 StoryMenu_PhotoMap, a map
 *  (FILE_ETURAN_PHOTO_PHF: the picture on FILE_PHOTO, its title on
 *  photoname_M). The page keys (pad_key[16] / [17]) step to the previous / next
 *  document of the tab;
 *  both go back to phase 61. FF3 0x1b24d8..0x1b3078, FF3-only; the phase
 *  names are inferred (the phase table has none for 84 / 85).
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_PHOTO_VIEW_H
#define _INGAME_STORY_MENU_PHOTO_VIEW_H

#include "sdk/scetypes.h"

// Phase 84 StoryMenu_PhotoDetail (no pre / after)
int init_StoryMenu_PhotoDetail(void);   // FF3 0x001b24d8
int one_StoryMenu_PhotoDetail(void);    // FF3 0x001b2528
int end_StoryMenu_PhotoDetail(void);    // FF3 0x001b2500

// Phase 85 StoryMenu_PhotoMap (no pre / after)
int init_StoryMenu_PhotoMap(void);      // FF3 0x001b28a8
int one_StoryMenu_PhotoMap(void);       // FF3 0x001b2af8
int end_StoryMenu_PhotoMap(void);       // FF3 0x001b2a30

#endif /* _INGAME_STORY_MENU_PHOTO_VIEW_H */
