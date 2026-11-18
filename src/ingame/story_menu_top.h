/* ==========================================================================
 *  ingame/story_menu_top.h
 *
 *  Phase 57 StoryMenu_Top (a child of Story_Menu 37): the in-game menu's
 *  top list, the page every story_menu_*.c row returns to. It loads
 *  MENUTOP_PHF (+ language), lays the rows of the story menu list
 *  (StoryMenuListCreate) into the slots of the first item labels, runs the
 *  row cursor with its two "R csl_on" sprites and the help line in the menu
 *  window, and draws the play time and points through four item hooks.
 *  OK opens the row's phase (58..64), cancel or START leaves the menu.
 *  FF3 0x20b590..0x20c400; FF3-only file, named like the other
 *  story_menu_*.c phases.
 *
 *  The file also holds two cursor helpers nothing in the ROM calls
 *  (MenuCsrObjSetListPos and the MENU_YESNO object).
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_TOP_H
#define _INGAME_STORY_MENU_TOP_H

#include <stddef.h>                     // offsetof

#include "ingame/menu/menu_csr.h"       // MENU_CSR_WRK

// A two-choice cursor (heapCtrlMalloc 0x44 on the EE): 2 x 1 cells.
// layout: host -- MENU_CSR_WRK holds pointers; csr keeps its EE offset.
struct MENU_YESNO
{
    /* 0x00 */ int          mode;       // 2: the cursor moves (MenuYesNoUpdate)
    /* 0x04 */ int          arg;        // stored, never read
    /* 0x08 */ MENU_CSR_WRK csr;
};
static_assert(offsetof(MENU_YESNO, arg) == 0x04, "MENU_YESNO.arg");
static_assert(offsetof(MENU_YESNO, csr) == 0x08, "MENU_YESNO.csr");

// Unreferenced in the ROM (no jal, j or data word).
void        MenuCsrObjSetListPos(MENU_CSR_WRK *wrk, short pos, short num);  // FF3 0x0020b590
MENU_YESNO *MenuYesNoCreate(int mode, int arg, short pos);                  // FF3 0x0020b658
void        MenuYesNoDelete(MENU_YESNO *obj);                               // FF3 0x0020b710
int         MenuYesNoUpdate(MENU_YESNO *obj);                               // FF3 0x0020b770: decided

int init_StoryMenu_Top(void);           // FF3 0x0020bd28
int one_StoryMenu_Top(void);            // FF3 0x0020c340
int end_StoryMenu_Top(void);            // FF3 0x0020c1a8

#endif /* _INGAME_STORY_MENU_TOP_H */
