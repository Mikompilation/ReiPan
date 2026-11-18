/* ==========================================================================
 *  ingame/menu/start_menu_top.h
 *
 *  The start menu's top page and its difficulty and costume pages, FF3
 *  0x1aedd0..0x1b0628 (FF3-only; a GPhaseCtrl atexit thunk 0x1b0608 ends
 *  the file). Children of phase 55 StartMenu_Main (ingame/menu/start_menu.c),
 *  which loads the SETUP layout (SETUP_PHF + language) they all draw on and
 *  owns their cursors and message window:
 *
 *    - phase  80 StartMenu_Top: seven entries (cursor 0x50), each opening a
 *      phase: 0 difficulty select (107) -> new game, 1 Story_Start (29),
 *      2 extra missions (83), 3 costumes (108), 4 bonus shop (109),
 *      5 gallery (82), 6 back to the title menu (a phase-12 prompt; cancel
 *      jumps here). Entry 1 is closed and greyed while FINDER_STATE_WRK._0c
 *      is set.
 *    - phase 107 StartMenu_Difficulty: the open difficulties (cursor 0x6b,
 *      TitleMenuIsItemOpen); OK resets the in-game state for a new game and
 *      goes to Story_Start (29).
 *    - phase 108 StartMenu_Costume: eight rows (cursor 0x6c): costumes of
 *      rows 0..2, accessories of rows 4..6, the cat's accessory on row 7
 *      (row 3, the cat's costume, has none). OK opens 118 on the row.
 *    - phase 118 StartMenu_CostumeSel: the row's bought entries (cursor
 *      0x76); its end applies all eight rows to the player and the cat.
 * ======================================================================== */
#ifndef _INGAME_MENU_START_MENU_TOP_H
#define _INGAME_MENU_START_MENU_TOP_H

#include "sdk/scetypes.h"

// Menu cursors (MenuCsr ids) of these pages; START_MENU_CSR_TOP (0x50) and
// START_MENU_CSR_DIFF (0x6b) are in start_menu.h.
#define START_MENU_CSR_COSTUME      0x6c    // the eight costume rows (table 0, made by phase 55)
#define START_MENU_CSR_COSTUME_SEL  0x76    // one row's entries (phase 118 makes and frees it)

// Called by phase 55 (StartMenuCsrInit / init_StartMenu_Main).
void StartMenuSetupSetHooks(void);                          // FF3 0x001af6f0: SETUP_PHF + language
void StartMenuTopCsrFilter(short *tbl, short w, short h);   // FF3 0x001af770: MenuCsr 0x50
void StartMenuDiffCsrFilter(short *tbl, short w, short h);  // FF3 0x001af808: MenuCsr 0x6b

// Phase 80 StartMenu_Top.
int  init_StartMenu_Top(void);              // FF3 0x001af878
void pre_StartMenu_Top(void);               // FF3 0x001afa50
int  one_StartMenu_Top(void);               // FF3 0x001afa58
void after_StartMenu_Top(void);             // FF3 0x001afc00
int  end_StartMenu_Top(void);               // FF3 0x001afc08
// Phase 107 StartMenu_Difficulty.
int  init_StartMenu_Difficulty(void);       // FF3 0x001afd38
int  one_StartMenu_Difficulty(void);        // FF3 0x001afe00
int  end_StartMenu_Difficulty(void);        // FF3 0x001aff40
// Phase 108 StartMenu_Costume.
int  init_StartMenu_Costume(void);          // FF3 0x001b0008
void pre_StartMenu_Costume(void);           // FF3 0x001b00d0
int  one_StartMenu_Costume(void);           // FF3 0x001b00d8
void after_StartMenu_Costume(void);         // FF3 0x001b01a8
int  end_StartMenu_Costume(void);           // FF3 0x001b01b0
// Phase 118 StartMenu_CostumeSel.
int  init_StartMenu_CostumeSel(void);       // FF3 0x001b04b0
int  one_StartMenu_CostumeSel(void);        // FF3 0x001b0508
int  end_StartMenu_CostumeSel(void);        // FF3 0x001b05d8

#endif /* _INGAME_MENU_START_MENU_TOP_H */
