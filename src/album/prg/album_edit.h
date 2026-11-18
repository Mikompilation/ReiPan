/* ==========================================================================
 *  album/prg/album_edit.h
 *
 *  The album's phases (FF3 0x1229a0..0x125318, FF2 album_edit.c's place in
 *  the link order). FF2 drove the edit page from a table of ten modes; FF3
 *  rebuilt it as game phases under phase 8, with the helpers they share:
 *    - phase  8 Album_Main:    load / release the album, the edit page (the
 *                              two pages of 16 photos, cover choice, copy,
 *                              erase, the menu, leave);
 *    - phase 24 Album_Open:    on entry, per side: a new album or one from
 *                              the memory card (phase 28), its cover;
 *    - phase 25 Album_Copy:    the destination cursor on the other page;
 *    - phase 26 Album_Menu:    the five-row menu, rows -> phases 53 / 52 /
 *                              27 / 28 / 54 (album_menu_phase);
 *    - phase 52 Album_Sort:    the sort key list;
 *    - phase 54 Album_Help:    the message viewer on an album message;
 *    - phase 78 Album_Confirm: a message with or without a yes / no cursor,
 *                              linked under whichever phase asks
 *                              (AlbumEditConfirmReq).
 *  Phases 27 / 28 (save / load to the card, FF3 0x128a58..0x128e30) and 53
 *  (the zoom page, 0x126738..0x126f68) are other files. Album_Main is the
 *  title menu's third item and the in-game album (pause, save point).
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_EDIT_H
#define _ALBUM_PRG_ALBUM_EDIT_H

#include "sdk/scetypes.h"

// AlbumEditConfFlag* bits (album_conf_flag, sbss 0x372c35): what the open
// Album_Confirm asks about.
#define ALBUM_CONF_COPY     0x1         // Album_Copy is up (overwrite confirm)
#define ALBUM_CONF_ERASE    0x2         // Album_Main's erase confirm

#define ALBUM_KEY_ROW_NUM   5           // AlbumKeyCsrInit's mask bits

extern int    album_open_arg;           // sdata 0x36b920: AlbumSetIngame's second argument (album.c sets it)
extern s_char album_conf_default;       // sbss 0x372c33: Album_Confirm's default row, -1 a message only
extern s_char album_conf_pos;           // sbss 0x372c34: its answer: 0 yes, 1 no, -1 cancelled
extern s_char album_key_sel;            // sdata 0x36b910: the key under the key cursor, -1 no list

// Cursors.
void   AlbumSortCsrSave(void);                      // FF3 0x001229a0: album_sort_key_req = the row under the cursor
void   AlbumKeyCsrInit(int id, u_char mask);        // FF3 0x001229f0: rows = the set bits of mask
int    AlbumKeyCsrUpdate(int id);                   // FF3 0x00122aa8: moved
void   AlbumKeyCsrRelease(int id);                  // FF3 0x00122b20
void   AlbumConfCsrInit(void);                      // FF3 0x00122cc8: yes / no on album_conf_default
int    AlbumConfCsrUpdate(void);                    // FF3 0x00122d00: moved
void   AlbumConfCsrRelease(void);                   // FF3 0x00122d50

// The help lines (the three album message slots).
void   AlbumEditGuideSet(int guide);                // FF3 0x00122b50: row of the ROM table 0x34dd48
void   AlbumEditGuideClear(void);                   // FF3 0x00122bc0
void   AlbumEditGuideSetMenu(void);                 // FF3 0x00122c08: the menu row's line
void   AlbumEditConfirmReq(int guide, int def);     // FF3 0x00122c68: Album_Confirm over the caller

int    AlbumEditCoverPad(int shoulder);             // FF3 0x00122d68: 1 when the cover changed
void   AlbumEditConfFlagClearAll(void);             // FF3 0x00122e40
void   AlbumEditConfFlagSet(u_char bits);           // FF3 0x00122e48: ALBUM_CONF_*
void   AlbumEditConfFlagClear(u_char bits);         // FF3 0x00122e60
int    AlbumEditConfFlagIsSet(u_char bits);         // FF3 0x00122e78

// Photo operations on the photo under the cursor; the checks play the SE
// (0x1a9b0003 go, 0x1a9b0002 buzz).
int    AlbumEditCurIsUsed(void);                    // FF3 0x00122e90
int    AlbumEditCopySrcSet(void);                   // FF3 0x00122ec8: 1 when it is the copy source now
int    AlbumEditPasteCheck(void);                   // FF3 0x00122f50: 1 free, 0 used (ask), -1 protected
int    AlbumEditPasteExec(void);                    // FF3 0x00123020
int    AlbumEditEraseCheck(void);                   // FF3 0x00123080: 1 erasable, 0 empty, -1 protected
int    AlbumEditEraseReq(void);                     // FF3 0x00123150: = AlbumEditEraseCheck
int    AlbumEditEraseExec(void);                    // FF3 0x00123170
void   AlbumEditSortOrderReset(void);               // FF3 0x001231a8
int    AlbumEditSort(s_char key);                   // FF3 0x001231d8: 1 when sorted (the order flips)

// The menu rows' tests (ROM list 0x34dd30) and their mask.
int    AlbumMenuCondHasPhoto(void);                 // FF3 0x00123268: row 0
int    AlbumMenuCondSortKeys(void);                 // FF3 0x00123290: row 1; also Album_Sort's key mask
int    AlbumMenuCondSave(void);                     // FF3 0x001232f0: row 2
int    AlbumMenuCondLoad(void);                     // FF3 0x00123340: row 3
int    AlbumMenuCondHelp(void);                     // FF3 0x00123368: row 4
u_char AlbumMenuGetRowMask(void);                   // FF3 0x001233c0
int    AlbumGetOpenArg(void);                       // FF3 0x00123478: album_open_arg

// Phase 8, Album_Main.
int  init_Album_Main(void);                         // FF3 0x00123480
int  end_Album_Main(void);                          // FF3 0x001239c8
int  one_Album_Main(void);                          // FF3 0x00123ce0
void pre_Album_Main(void);                          // FF3 0x00124050
void after_Album_Main(void);                        // FF3 0x00124080
// Phase 24, Album_Open.
int  init_Album_Open(void);                         // FF3 0x001240b8
int  end_Album_Open(void);                          // FF3 0x001240e8
int  one_Album_Open(void);                          // FF3 0x001240f0
// Phase 25, Album_Copy.
int  init_Album_Copy(void);                         // FF3 0x00124490
int  end_Album_Copy(void);                          // FF3 0x00124508
int  one_Album_Copy(void);                          // FF3 0x00124580
// Phase 26, Album_Menu.
int  init_Album_Menu(void);                         // FF3 0x00124778
int  end_Album_Menu(void);                          // FF3 0x00124890
void pre_Album_Menu(void);                          // FF3 0x001248e0
int  one_Album_Menu(void);                          // FF3 0x001248e8
void after_Album_Menu(void);                        // FF3 0x00124ce0
// Phase 52, Album_Sort.
int  init_Album_Sort(void);                         // FF3 0x00124d88
int  end_Album_Sort(void);                          // FF3 0x00124e58
void pre_Album_Sort(void);                          // FF3 0x00124e98
int  one_Album_Sort(void);                          // FF3 0x00124ea0
void after_Album_Sort(void);                        // FF3 0x00124f80
// Phase 54, Album_Help.
int  init_Album_Help(void);                         // FF3 0x00124f88
int  end_Album_Help(void);                          // FF3 0x00124ff0
int  one_Album_Help(void);                          // FF3 0x00124ff8
// Phase 78, Album_Confirm.
int  init_Album_Confirm(void);                      // FF3 0x001250d0
int  end_Album_Confirm(void);                       // FF3 0x00125120
int  one_Album_Confirm(void);                       // FF3 0x00125158

#endif /* _ALBUM_PRG_ALBUM_EDIT_H */
