/* ==========================================================================
 *  ingame/story_menu.h
 *
 *  The lock table and the story menu (phase 37, Story_Menu: the in-game menu
 *  the menu button opens). FF3 0x1f1600..0x1f20b8, FF3-only file (FF2 kept
 *  Story_Menu in ingame.c and had no lock table); file name is descriptive.
 *
 *  Lock table (INGAME_WRK.lock_tbl, data 0x327590, 0x1c ids): one byte per
 *  id, bit 0 a flag (LockTblSet/Reset) and bits 1..7 a signed nesting count
 *  (LockTblInc/Dec). An id is on while either is set (IsLockTblOn). Known
 *  ids: 2/3 fade-layer idle, 4 in-game input, 8/9 player action, 0xd finder
 *  draw, 0xf menu, 0x10 pause, 0x11 player death, 0x12 finder, 0x15 photo
 *  mode, 0x1b story-menu map row.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_H
#define _INGAME_STORY_MENU_H

#include "sdk/scetypes.h"

#define STORY_MENU_CSR  0x25                            // the menu's cursor id (MenuCsr*)

#define LOCK_TBL_NUM    0x1c        // ids (IngameWrkInit clears 0x1c bytes)

void LockTblInit(void);
void LockTblSet(int no);            // flag bit on
void LockTblReset(int no);          // flag bit off
int  IsLockTblSet(int no);          // flag bit
void LockTblInc(int no);            // count + 1
void LockTblDec(int no);            // count - 1
int  IsLockTblCnt(int no);          // count != 0
int  IsLockTblOn(int no);           // flag bit or count

// One entry of the story-menu item table (data 0x35b670): the top menu's
// rows. label is the B2d item label ("R map" .. "R list"), msg the help
// message StoryMenuGetHelpMsg hands to MenuWdwSetMsg(0x6e, ..).
// layout: host -- label is a pointer (EE 4 bytes); 0xc bytes per entry on the EE.
struct STORY_MENU_ITEM
{
    const char *label;
    int         id;                 // 0..6; the list rows carry it
    int         msg;
};

short       StoryMenuGetRetPhase(void);         // phase the menu returns to on cancel
int         StoryMenuGetItemNum(void);          // 7
const char *StoryMenuGetItemLabel(int i);
const STORY_MENU_ITEM *StoryMenuFindItem(int id);
const char *StoryMenuGetItemLabelById(int id);
int         StoryMenuGetHelpMsg(int row);

int IsMenuEnable(void);             // the menu button may open phase 37

struct MENU_LIST;                   // ingame/menu/menu_bg.h
MENU_LIST *StoryMenuListCreate(void);
void  StoryMenuCsrInit(void);
void  StoryMenuCsrRelease(void);
short StoryMenuGetRowId(short row);
short StoryMenuGetRowPhase(short row);
short StoryMenuGetRowNum(void);
short StoryMenuGetCsrRow(void);
void  StoryMenuCsrUpdate(void);
int   StoryMenuCsrMovedSe(void);         // cursor moved (plays SE 0x1a9b0000)

// The menu BGM (story_menu_bgm_mode): 2 plays BGM_01_MENU1 over the paused
// game sounds, 0 leaves the game's sound running (fights, b03 == 1, the
// map row as target).
int  StoryMenuGetBgmMode(void);
void StoryMenuBgmIn(void);
void StoryMenuBgmOut(void);

// Phase 37, Story_Menu (children 57..64: the rows).
int  init_Story_Menu(void);
int  one_Story_Menu(void);
int  end_Story_Menu(void);
void pre_Story_Menu(void);
void after_Story_Menu(void);

#endif /* _INGAME_STORY_MENU_H */
