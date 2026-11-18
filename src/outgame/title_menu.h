/* ==========================================================================
 *  outgame/title_menu.h
 *
 *  The title (FF3 0x25d000..0x25e9e0, one object): the background movie
 *  object, phases 5 Title_Mode, 15 Title_Top, 16 Title_Move_Movie, 17
 *  Title_Menu and 51 Title_NewGame (the difficulty select), and the title
 *  layout's item hooks. FF2 split this into outgame/title.c (the phases) and
 *  title_menu.c (the menu); FF3 keeps it in one file, which link order put
 *  in title_menu.c's slot.
 *
 *  Files: TITLE_PHF + language (the menu layout, labels START_in / START_out
 *  / START_back / START_next / next / back / difficulty_*), TITLEBG_PHF (the
 *  layer over the movie), OUTGAME_BG_PSS (the movie), BGM_00_TITLE, SE bank
 *  MENU_START_BD. Menu cursors 0x11 (title menu, 5 items) and 0x33
 *  (difficulty, 4 items, nightmare only when TitleMenuIsItemOpen(3)).
 * ======================================================================== */
#ifndef _OUTGAME_TITLE_MENU_H
#define _OUTGAME_TITLE_MENU_H

#include "sdk/scetypes.h"

// The difficulty the player picked (sdata gp+0x504): IngameWrkInit takes it.
int  TitleMenuCtrlInit(void);           // FF3 0x0025e130: returns it
void TitleMenuFlagSet(void);            // FF3 0x0025e120: sets it to 1 (normal)
void TitleMovieFlagClear(void);         // FF3 0x0025dd48: title menu cursor back to item 0

void TitleObjCreate(void);              // FF3 0x0025d000: new background movie object
void TitleObjDelete(void);              // FF3 0x0025d040
int  TitleObjIsReady(void);             // FF3 0x0025d080
void TitleSetLayoutCallbacks(void);     // FF3 0x0025d138: movie hook on TITLE_PHF, TITLEBG_PHF label
void TitleMenuDecide(int pos);          // FF3 0x0025d1f0: start flags for title menu item pos
int  TitleMenuGetEnableItems(int *items);   // FF3 0x0025d320: open difficulties 0..3, count

// Phases (GPhase tables, main/gphase.c).
int  init_Title_Mode(void);
int  one_Title_Mode(void);
int  end_Title_Mode(void);
void pre_Title_Mode(void);
void after_Title_Mode(void);
int  init_Title_Top(void);
int  one_Title_Top(void);
int  end_Title_Top(void);
int  init_Title_Move_Movie(void);
int  one_Title_Move_Movie(void);
int  end_Title_Move_Movie(void);
int  init_Title_Menu(void);
int  one_Title_Menu(void);
int  end_Title_Menu(void);
void pre_Title_Menu(void);
void after_Title_Menu(void);
int  init_Title_NewGame(void);
int  one_Title_NewGame(void);
int  end_Title_NewGame(void);

#endif /* _OUTGAME_TITLE_MENU_H */
