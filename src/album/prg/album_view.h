/* ==========================================================================
 *  album/prg/album_view.h
 *
 *  The album edit page's layout layer (FF2 album_view.c's place; FF3
 *  0x12a068..0x12d9f8). The album runs on five B2d
 *  layouts held as BP_OBJs (album_layout_obj): the edit page itself
 *  (ALBM_HENSYU: the current photo, its room / date / score / subjects),
 *  the thumbnail strip (ALBM_THUMBNAIL), the common frame (ALBM_CMN: the
 *  two album pages' photo and protect marks, menus, arrows, the fade layer
 *  item), the zoom page (ALBM_KAKUDAI) and the card screens (ALBM_SENTAKU,
 *  album_save_scr.c / album_load_scr.c). This file creates, loads, plays in
 *  and out and releases them, hooks the items that draw album data, and
 *  plays the page animations album_edit.c asks for. Each album page (side 0
 *  / 1) fades with its own alpha.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_VIEW_H
#define _ALBUM_PRG_ALBUM_VIEW_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct BP_OBJ;

// The album's five layout objects (bss 0x2f15b0; AlbumViewLayoutCreate news
// and ctors them over the ROM file list 0x34e880 + language,
// AlbumViewLayoutLoad loads them, AlbumViewLayoutRelease releases them).
// Indices follow the ROM list.
enum ALBUM_LAYOUT
{
    ALBUM_LAYOUT_HENSYU    = 0,         // ALBM_HENSYU_PHF: the edit page
    ALBUM_LAYOUT_KAKUDAI   = 1,         // ALBM_KAKUDAI_PHF: the zoom page
    ALBUM_LAYOUT_THUMBNAIL = 2,         // ALBM_THUMBNAIL_PHF
    ALBUM_LAYOUT_CMN       = 3,         // ALBM_CMN_PHF: its handle feeds the fade layer (item 0x3f1)
    ALBUM_LAYOUT_SENTAKU   = 4,         // ALBM_SENTAKU_PHF: the album save / load screens
    ALBUM_LAYOUT_NUM       = 5,
};

extern BP_OBJ *album_layout_obj[ALBUM_LAYOUT_NUM];  // bss 0x2f15b0

// One of the edit page's three message slots (bss 0x390c20): the message
// asked for and the one shown with its alpha. AlbumViewMsgStep swaps a new
// one in at once and fades an emptied one out (steps of 0x80: in practice
// instant). Slot 0 is printed whole by ALBM_CMN item 0x3fa, slots 1 / 2 as
// one centred line by items 0x458 / 0x459. Types and ids are
// GetMsgDataAddr's; -1 is none.
#define ALBUM_VIEW_MSG_NUM  3
struct ALBUM_VIEW_MSG                   /* 0x14 */
{
    /* 0x00 */ int    req_type;
    /* 0x04 */ int    req_id;
    /* 0x08 */ int    type;             // shown
    /* 0x0c */ int    id;
    /* 0x10 */ u_char alpha;            // 0..0x80
};
static_assert(sizeof(ALBUM_VIEW_MSG) == 0x14, "ALBUM_VIEW_MSG is 0x14 bytes on the EE");
static_assert(offsetof(ALBUM_VIEW_MSG, type) == 0x08, "ALBUM_VIEW_MSG.type");
static_assert(offsetof(ALBUM_VIEW_MSG, id) == 0x0c, "ALBUM_VIEW_MSG.id");
static_assert(offsetof(ALBUM_VIEW_MSG, alpha) == 0x10, "ALBUM_VIEW_MSG.alpha");

// The slot cursor of one album page (bss 0x390c10, one per side): drawn by
// ALBM_CMN items 0x42b / 0x3e9 at slot pos of the page's 8 x 2 grid.
struct ALBUM_VIEW_PAGE_CSR              /* 0x08 */
{
    /* 0x00 */ s_char pos;              // photo slot 0..15
    /* 0x01 */ u_char gray;             // sprite RGB (gray, gray, gray)
    /* 0x02 */ u_char alpha;            // times the page alpha / 0x80
    /* 0x04 */ int    on;
};
static_assert(sizeof(ALBUM_VIEW_PAGE_CSR) == 0x08, "ALBUM_VIEW_PAGE_CSR is 8 bytes on the EE");
static_assert(offsetof(ALBUM_VIEW_PAGE_CSR, gray) == 0x01, "ALBUM_VIEW_PAGE_CSR.gray");
static_assert(offsetof(ALBUM_VIEW_PAGE_CSR, alpha) == 0x02, "ALBUM_VIEW_PAGE_CSR.alpha");
static_assert(offsetof(ALBUM_VIEW_PAGE_CSR, on) == 0x04, "ALBUM_VIEW_PAGE_CSR.on");

// Messages (album_edit.c, the 0x1240e8 run).
void   AlbumViewMsgSet(s_char no, int type, int id);   // FF3 0x0012a068
void   AlbumViewMsgInit(void);                          // FF3 0x0012a090
void   AlbumViewMsgStep(void);                          // FF3 0x0012a0d8: per frame
u_char AlbumViewMsgGetAlpha(s_char no, int invert);     // FF3 0x0012a1f8: 0x80 - alpha with invert
// Which of the two looks the help icons (0x464 / 0x465 swap sprites) and
// message lines 1 / 2 (bright on 0 / 1) take.
void   AlbumViewSetMsgVariant(u_char variant);          // FF3 0x0012ade8

// The album pages' selection: frames, arrows (never shown for the camera
// album), the slot cursors, and the menu / sort rows with their cursors.
void AlbumViewArrowSetDisp(s_char side, int disp);      // FF3 0x0012a540
void AlbumViewPageCsrSetOn(s_char side, int on);        // FF3 0x0012a6c8
void AlbumViewPageCsrSetGray(s_char side, u_char gray); // FF3 0x0012a710
void AlbumViewPageCsrFreeze(s_char side, int mode);     // FF3 0x0012a738: B2dSprFreezeItem of 0x42b / 0x42d
void AlbumViewSelectPage(s_char side);                  // FF3 0x0012aa08: frames and arrows move to side
void AlbumViewSelectSlot(s_char side, s_char pos);      // FF3 0x0012aa78: and the cursor
void AlbumViewPageCsrSetPos(s_char side, s_char pos);   // FF3 0x0012ab28: the cursor only
void AlbumViewMenuRowSetRgb(u_char mask);               // FF3 0x0012ab88: bit i set: row i bright
void AlbumViewMenuCsrSet(s_char pos);                   // FF3 0x0012ac50: < 0 hides
void AlbumViewSortRowSetRgb(u_char mask);               // FF3 0x0012aca8
void AlbumViewSortCsrSet(s_char pos);                   // FF3 0x0012ad70
void AlbumViewShowItem45a(int on);                      // FF3 0x0012b458: ALBM_CMN 0x45a alpha 0x80 / 0
// Off: the page fades out and its cover picture goes blank.
void AlbumViewPageSetOn(s_char side, int on);           // FF3 0x0012b4e0

// The page alpha of album side n (sbss gp-0x838): 0 reset, +0x10 / -0x20 a
// step, both return the new value.
void   AlbumViewPageAlphaReset(s_char side);        // FF3 0x0012b928
u_char AlbumViewPageFadeIn(s_char side);            // FF3 0x0012b940
u_char AlbumViewPageFadeOut(s_char side);           // FF3 0x0012b988

// The layouts' life (album_edit.c 0x123480 / 0x1239c8).
void AlbumViewLayoutCreate(void);       // FF3 0x0012d2c8
int  AlbumViewLayoutLoad(void);         // FF3 0x0012d388: 1 when all five are loaded
void AlbumViewLayoutIn(int pic_mode);   // FF3 0x0012d430: "hensyu_in" and the edit page's items
int  AlbumViewLayoutIsInEnd(void);      // FF3 0x0012d4c8
int  AlbumViewLayoutOut(void);          // FF3 0x0012d568: "hensyu_out"; 1 when done
void AlbumViewLayoutRelease(void);      // FF3 0x0012d5d8

// Per frame: both pages fade by their album; with follow the photo items
// of the side not shown (the other one with swap) take its alpha.
void AlbumViewUpdatePages(int follow, int swap);    // FF3 0x0012d520
void AlbumViewShowCopyItem(int show);   // FF3 0x0012cfa0: ALBM_HENSYU item 0x414

// One-shot page animations; 1 (and reset) once every layout has stopped.
int  AlbumViewPlayMenu(int on);         // FF3 0x0012d678
int  AlbumViewPlayPageTurn(int back, s_char thumb_b);  // FF3 0x0012d6e0
int  AlbumViewPlayLoad(int in);         // FF3 0x0012d7f8
int  AlbumViewPlaySort(int on);         // FF3 0x0012d8d8

void AlbumViewSetFadeLayer(int phase);  // FF3 0x0012d940: the fade layer's label for a game phase

// PC: the mouse.
#include "reipan/io/reipan_menu_mouse.h"
#define ALBUM_VIEW_MOUSE_ROW_DY 37.0f   // the row cursors' step
int AlbumViewMouseCell(s_char side, int cell, ReiPan_MenuBox *box);
int AlbumViewMouseMenuBox(ReiPan_MenuBox *box);
int AlbumViewMouseSortBox(ReiPan_MenuBox *box);
int AlbumViewMouseConfBox(int pos, ReiPan_MenuBox *box);

#endif /* _ALBUM_PRG_ALBUM_VIEW_H */
