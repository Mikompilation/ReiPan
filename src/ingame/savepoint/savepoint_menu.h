/* ==========================================================================
 *  ingame/savepoint/savepoint_menu.h
 *
 *  The save point menu (phase 41 Story_SaveGame, ingame/story_save.c):
 *  the SAVEPOINT_PHF layout (+ language) with one row per choice; each row
 *  names the phase it opens. FF3 0x135690..0x135e08, FF3's own code (file
 *  name invented). Its twin is the game over menu (ingame/gameover_menu.c).
 * ======================================================================== */
#ifndef _INGAME_SAVEPOINT_SAVEPOINT_MENU_H
#define _INGAME_SAVEPOINT_SAVEPOINT_MENU_H

#include <stddef.h>                     // offsetof

// One row of a phase menu (ROM tables 0x34f160 / 0x34eea8, EE 0x10 bytes;
// the list ends with id -1). Messages are of the menu's message kind.
struct MENU_PHASE_ROW
{
    /* 0x00 */ int id;                  // layout item of the row (its name's 5th char is '1'..)
    /* 0x04 */ int msg;                 // the row's text
    /* 0x08 */ int help;                // shown in the yes/no window while the cursor is on it
    /* 0x0c */ int phase;               // phase the row opens, -1 leave
};
static_assert(sizeof(MENU_PHASE_ROW) == 0x10, "MENU_PHASE_ROW is 0x10 bytes");
static_assert(offsetof(MENU_PHASE_ROW, help) == 0x08, "MENU_PHASE_ROW.help");
static_assert(offsetof(MENU_PHASE_ROW, phase) == 0x0c, "MENU_PHASE_ROW.phase");

void SavePointMenuCreate(void);                     // FF3 0x00135800
int  SavePointMenuIsLoaded(void);                   // FF3 0x001359d0: plays "in" once loaded
int  SavePointMenuIsInEnd(void);                    // FF3 0x00135b10
int  SavePointMenuSelect(void);                     // FF3 0x00135b38: 1 when a row is chosen
int  SavePointMenuConfirm(void);                    // FF3 0x00135c80: 1 yes, -1 no, 0 waiting
int  SavePointMenuGetPhase(void);                   // FF3 0x00135d40: the row's phase, -1 none
int  SavePointMenuOutWait(void);                    // FF3 0x00135d48: 1 when "out" is done
void SavePointMenuRelease(void);                    // FF3 0x00135d70

#endif /* _INGAME_SAVEPOINT_SAVEPOINT_MENU_H */
