/* ==========================================================================
 *  ingame/story_menu_camera.h
 *
 *  The camera menu phase, 64 StoryMenu_Camera (a child of Story_Menu 37,
 *  back to 57 StoryMenu_Top when it closes): it runs CmenuCamMain
 *  (ingame/menu/menuCamMain.h) through a five-step STEP_SEQ.
 *  FF3 0x208140..0x208370; FF3-only file, named like the other
 *  story_menu_*.c phases.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_CAMERA_H
#define _INGAME_STORY_MENU_CAMERA_H

int init_StoryMenu_Camera(void);        // FF3 0x00208238
int one_StoryMenu_Camera(void);         // FF3 0x002082d0
int end_StoryMenu_Camera(void);         // FF3 0x00208320

#endif /* _INGAME_STORY_MENU_CAMERA_H */
