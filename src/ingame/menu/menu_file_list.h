/* ==========================================================================
 *  ingame/menu/menu_file_list.h
 *
 *  The file menu, phase 62 StoryMenu_File (a child of 37 Story_Menu), and
 *  its first-visit explanation, phase 87 StoryMenu_FileFirst. FF3-only; the
 *  name is invented (FF2's menu_file.c link slot holds the detail page,
 *  phase 86, ingame/menu/menu_file.c). FF3 0x204810..0x207490, one object:
 *  no atexit thunk between memo.c's (0x203e98) and this one's (0x207470).
 *
 *  The menu (MEMO_PHF + language): a tab column, one tab per memo
 *  category that has memos (all four once the game was cleared), each
 *  drawn from CATEGORY_TM2; a list of the current tab's memos by title
 *  (KOUMOKU1_TM2.., nine rows a page), a FUSEN sticky note beside the new
 *  ones, the category's picture (CATEGORY_PIC_01..04), and grid sprites
 *  that follow the cursors. OK / right moves from the tabs to the rows and
 *  from a found memo to its detail page (86), cancel / left back. The very
 *  first visit goes to phase 87 first: message 99 in the message viewer.
 *
 *  The memo-menu request (bss 0x3f6048) is how the rest of the game opens
 *  the menu at a memo: MemoMenuReq (the memo notice's pad button, the
 *  event command MEMO_MENU_OPEN) and MemoDetailReq (no callers).
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_FILE_LIST_H
#define _INGAME_MENU_MENU_FILE_LIST_H

#include "sdk/scetypes.h"

// ---- phase 62 StoryMenu_File ----
int  init_StoryMenu_File(void);         // FF3 0x00204810
int  end_StoryMenu_File(void);          // FF3 0x00204a80
int  one_StoryMenu_File(void);          // FF3 0x00204c00
void pre_StoryMenu_File(void);          // FF3 0x00204c48
void after_StoryMenu_File(void);        // FF3 0x00204c50

// ---- phase 87 StoryMenu_FileFirst ----
int  init_StoryMenu_FileFirst(void);    // FF3 0x00207290
int  end_StoryMenu_FileFirst(void);     // FF3 0x00207298
int  one_StoryMenu_FileFirst(void);     // FF3 0x002072a0

// The file list's layouts per frame: koumoku_next / back on a focus
// change, the grid sprites, the scroll arrows, the tab stickers, the fade
// layer, LayoutAnimMain when anim, BpCtlMain. Phases 62, 86 and 87 run it.
void  MenuFileListMain(int anim);       // FF3 0x00204c58

short MenuFileGetTab(void);             // FF3 0x002055c0: the tab cursor
short MenuFileGetRow(void);             // FF3 0x002055d8: the row cursor
short MenuFileSetRow(int row);          // FF3 0x002055f0 (no callers)

// From the row cursor, the next row in the sign of dir (wrapping) whose memo
// has been found; puts the row cursor there and returns the row (unchanged
// when none).
short  MenuFileStepMemo(int dir);       // FF3 0x00205610
short  MenuFileGetMemo(void);           // FF3 0x00205748: the memo under the cursors, -1 none
s_char MenuFileGetPageNum(void);        // FF3 0x00205780: text pages of its current page (MemoGetPageBreakNum)
short  MenuFileListGetMemo(short tab, short row);   // FF3 0x00205b30: -1 none

int   GetMenuLangFileNo(void);          // FF3 0x00205f70: MEMO_PHF + GetLanguage(), the file menu layout

// ---- the memo-menu request (bss 0x3f6048) ----
// MEMO_MENU_REQ.mode
#define MEMO_MENU_REQ_TAB       0       // cursors on the memo, focus on the tabs
#define MEMO_MENU_REQ_ROW       2       // focus on the rows ("koumoku_in")
#define MEMO_MENU_REQ_DETAIL    3       // straight to the detail page (MemoDetailReq)

void  MemoMenuReqInit(void);            // FF3 0x00207010: IngameInit; no request
short MemoMenuReqGetMemo(void);         // FF3 0x00207030
int   MemoMenuReqGetMode(void);         // FF3 0x00207040
int   MemoMenuReqIsSet(void);           // FF3 0x00207050
int   MemoMenuReqIsRowSelect(void);     // FF3 0x00207068
int   MemoMenuReqGetLink(void);         // FF3 0x002070a0

// The memo menu may open: unless force, the finder lock (0x12) is off and
// IsMenuEnable; then memo_flg.open_flg, not in the finder, and a game clear
// or a memo page found.
int MemoMenuIsEnable(int force);        // FF3 0x002070b0

// Opens the file menu (GPhaseCtrlRequest(62, link)); a found memo puts the
// cursors on it, on the rows when select. 1 when requested.
int MemoMenuReq(short memo, int select, int link, int force);  // FF3 0x00207148
// Opens a found memo's detail page (phase 86). No callers.
int MemoDetailReq(short memo, int link);                        // FF3 0x00207200

#endif /* _INGAME_MENU_MENU_FILE_LIST_H */
