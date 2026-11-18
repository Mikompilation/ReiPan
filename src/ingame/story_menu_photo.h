/* ==========================================================================
 *  ingame/story_menu_photo.h
 *
 *  The document menu, phase 61 StoryMenu_Photo (FILE_TOP_PHF + language):
 *  the documents the player holds, in four tabs (tab_siryou / tab_kosyo /
 *  tab_tegami / tab_map: documents, old books, letters, maps), a picture of
 *  the one under the cursor and the menu window with its caption. OK opens
 *  phase 84 StoryMenu_PhotoDetail (the text and its pictures) or, on the map
 *  tab, phase 85 StoryMenu_PhotoMap (story_menu_photo_view.h).
 *  FF3 0x1b0628..0x1b24d8 + DocListFill 0x2b9118 (linkonce run) and
 *  DocGetMsgType 0x2adc78 (satellite); FF3-only (FF2's memo menu is a
 *  different program), named after the phase.
 *
 *  The documents are the message picture keys (msg_pic_key_dat, data
 *  0x358a10): one flag byte each at 0x325198 (DOC_FLAG_*), set by FILE_GET,
 *  the event queue's rewards, photos (photo step 14) and the start menu's
 *  bought extras; FILE_LOST clears one.
 * ======================================================================== */
#ifndef _INGAME_STORY_MENU_PHOTO_H
#define _INGAME_STORY_MENU_PHOTO_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// ──────────────────────────────────────────────────────────────────────
// The documents (message picture keys)
// ──────────────────────────────────────────────────────────────────────

// One entry per key (key == index): the document's kind (its tab), its
// pictures (the message view's picture set: files pic .. pic + pic_num - 1),
// the menu picture file (menu_pic; the map view shows menu_pic + 2) and its
// message (DocGetMsgType(key) by kind, msg1: the title; msg1 + 1 the window
// caption, msg1 + 2 the text). EE 0xc bytes.
struct MSG_PIC_KEY_DAT
{
    /* 0x00 */ short key;
    /* 0x02 */ short kind;              // DOC_KIND_*
    /* 0x04 */ short pic;               // first picture file of the text (MsgViewSetPics)
    /* 0x06 */ short pic_num;           // its pictures, 0 none
    /* 0x08 */ short menu_pic;          // the menu's picture file (MENU_PIC.pic)
    /* 0x0a */ short msg1;
};
static_assert(sizeof(MSG_PIC_KEY_DAT) == 0xc, "MSG_PIC_KEY_DAT is 0xc bytes");
static_assert(offsetof(MSG_PIC_KEY_DAT, kind) == 0x02, "MSG_PIC_KEY_DAT.kind");
static_assert(offsetof(MSG_PIC_KEY_DAT, pic) == 0x04, "MSG_PIC_KEY_DAT.pic");
static_assert(offsetof(MSG_PIC_KEY_DAT, pic_num) == 0x06, "MSG_PIC_KEY_DAT.pic_num");
static_assert(offsetof(MSG_PIC_KEY_DAT, menu_pic) == 0x08, "MSG_PIC_KEY_DAT.menu_pic");
static_assert(offsetof(MSG_PIC_KEY_DAT, msg1) == 0x0a, "MSG_PIC_KEY_DAT.msg1");

#define MSG_PIC_KEY_NUM 0xa4

// The kinds = the menu's tabs (message types 0x1d, 0x1e, 0x1c, 0x1f).
enum DOC_KIND
{
    DOC_KIND_SIRYOU = 0,                // documents
    DOC_KIND_KOSYO  = 1,                // old books
    DOC_KIND_TEGAMI = 2,                // letters
    DOC_KIND_MAP    = 3,                // maps: cleared on a new game, viewed in phase 85
    DOC_KIND_NUM    = 4,
};

// The flag bytes (0x325198).
#define DOC_FLAG_NONE   0               // not held
#define DOC_FLAG_NEW    1               // held, not read yet (the menu's new marker)
#define DOC_FLAG_READ   2               // held and opened in a view

extern const MSG_PIC_KEY_DAT msg_pic_key_dat[MSG_PIC_KEY_NUM];   // data 0x358a10

void DocFlagInit(void);                 // FF3 0x001b0628: IngameInit, all clear
void DocFlagNewGame(void);              // FF3 0x001b0670: IngameNewGameReset, the maps cleared
void DocFlagEnd(void);                  // FF3 0x001b06c0: IngameEnd (nothing)
int  DocFlagSet(short key);             // FF3 0x001b06d8: none -> new; 1 when it was not held
int  DocFlagClear(short key);           // FF3 0x001b0710: -> none; 1 when it was held
int  DocFlagSetRead(short key);         // FF3 0x001b0748: held -> read; 1 when held
int  DocFlagSaveGet(void **addr);       // FF3 0x001b0780: {0x325198, 0xa4} (save block 3)
void DocFlagDummy(void);                // FF3 0x001b07a8: no caller, an empty loop
void DocListDummy(void);                // FF3 0x001b0928: no caller, empty

// The message type of a document by its kind, 0 for an unknown kind. The
// second argument is unused (callers pass 0 title, 1 caption, 2 text).
// FF3 0x002adc78 (out of line in the linkonce run).
int  DocGetMsgType(short key, int part);

// The documents of one kind (EE 0x10 bytes, new'ed per tab by DocListCreate;
// MEMO_LIST's twin).
// layout: host -- list is a pointer (EE 4 bytes at +8); num follows it.
struct DOC_LIST
{
    DOC_LIST(short kind, int all);      // FF3 0x001b0808 (DocListCtor; base 0x001b07d8)
    ~DOC_LIST();                        // FF3 0x001b0850 (DocListDtor; base 0x001b0838)

    void  Build();                      // FF3 0x001b0868
    void  Free();                       // FF3 0x001b08d8
    short Fill(int store);              // FF3 0x002b9118: the count; with store into list

    /* 0x00 */ short  kind;
    /* 0x04 */ int    all;              // non-zero: documents not held too
    /* 0x08 */ short *list;             // num keys (system heap)
    /* 0x0c */ short  num;
};
static_assert(offsetof(DOC_LIST, all) == 4, "DOC_LIST.all");
static_assert(offsetof(DOC_LIST, list) == 8, "DOC_LIST.list");
static_assert(offsetof(DOC_LIST, num) == 8 + sizeof(short *), "DOC_LIST.num follows list");

// ──────────────────────────────────────────────────────────────────────
// The menu (what phases 84 / 85 use)
// ──────────────────────────────────────────────────────────────────────

void  DocMenuUpdate(void);              // FF3 0x001b0d68: the per-frame part of 61 / 84 / 85
short DocMenuGetTab(void);              // FF3 0x001b1340: 0..3
short DocMenuGetNum(int tab);           // FF3 0x001b1628: the tab's documents
int   DocCsrGetIndex(int tab);          // FF3 0x001b1648: the list index under the tab's cursor
short DocCsrSetIndex(int tab, int index);   // FF3 0x001b16a0: negative = the last, past the end = the first
short DocListGetPic(int tab, short i);  // FF3 0x001b1938: no caller, document i's first text picture
short DocListGetPicNum(int tab, short i);   // FF3 0x001b1970: no caller, its picture count
short DocMenuGetCurDoc(void);           // FF3 0x001b1a10: the key under the cursor, -1 none
int   DocMenuGetWdwAnim(void);          // FF3 0x001b23d0: the menu window's layout slot
void  DocMenuWdwUpdate(void);           // FF3 0x001b23d8: the window caption of the current document

// Phase 61 StoryMenu_Photo
int  init_StoryMenu_Photo(void);        // FF3 0x001b0930
int  one_StoryMenu_Photo(void);         // FF3 0x001b0d10
int  end_StoryMenu_Photo(void);         // FF3 0x001b0ba0
void pre_StoryMenu_Photo(void);         // FF3 0x001b0d58
void after_StoryMenu_Photo(void);       // FF3 0x001b0d60

#endif /* _INGAME_STORY_MENU_PHOTO_H */
