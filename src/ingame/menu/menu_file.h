/* ==========================================================================
 *  ingame/menu/menu_file.h
 *
 *  The file menu's detail page, phase 86 StoryMenu_FileDetail (a child of
 *  62 StoryMenu_File): one memo page of the file menu (MEMO_PHF) read in
 *  full -- its text by text page (menu cursor 0x56), its picture (MENU_PIC),
 *  "n / m", and a 3D flip panel when it opens from the file list.
 *  FF3 0x207490..0x208140 (the object ends with the GPhaseCtrl atexit thunk
 *  0x208120). FF2's menu_file.c is the same menu, but nothing of FF2's code
 *  survives here; the FF2 names Ghidra had on this run were wrong matches.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_FILE_H
#define _INGAME_MENU_MENU_FILE_H

#include "sdk/scetypes.h"

int init_StoryMenu_FileDetail(void);    // FF3 0x00207490
int one_StoryMenu_FileDetail(void);     // FF3 0x002076d0
int end_StoryMenu_FileDetail(void);     // FF3 0x002075c8

short MenuFileGetMemoPage(void);        // FF3 0x002079e0 (no callers): the memo_page row of the selected memo

#endif /* _INGAME_MENU_MENU_FILE_H */
