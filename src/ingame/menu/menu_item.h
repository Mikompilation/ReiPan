/* ==========================================================================
 *  ingame/menu/menu_item.h
 *
 *  The item menu, phase 59 StoryMenu_Item (ITEM_PHF + language): the items
 *  the player holds in two tabs (tabA: films, recovery and the candle, item
 *  types 0x0f; tabB: keys, types 0x30), with the counts on tabA, two
 *  pictures of the item under the cursor and its description in the menu
 *  window. OK on a usable item asks "use it?" / "load this film?" (LayoutMsg
 *  0x61) and uses one; an unusable one gets its reason. FF3 0x1eac20..
 *  0x1ecd30 + ItemMenuListFill 0x2bccd8 (out of line); FF3's own program in
 *  FF2's menu_item.c slot (FF2's item menu is a different program).
 *  story_menu_photo.c (the document menu) is its twin.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_ITEM_H
#define _INGAME_MENU_MENU_ITEM_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define ITEM_MENU_TAB_NUM   2           // tabA, tabB

// The items of one tab (EE 0x10 bytes, new'ed per tab by ItemMenuListNew;
// DOC_LIST's twin): items held whose ITEM_DAT type / world / menu bits AND
// with the masks (0xffff: any).
// layout: host -- list is a pointer (EE 4 bytes at +8); num follows it.
struct ITEM_MENU_LIST
{
    ITEM_MENU_LIST(short type, short world, short menu);  // FF3 0x001eac60 (ItemMenuListCtor; base 0x001eac20)
    ~ITEM_MENU_LIST();                  // FF3 0x001eacb8 (ItemMenuListDtor; base 0x001eaca0)

    void  Build();                      // FF3 0x001eacd0
    void  Free();                       // FF3 0x001ead40
    short Fill(int store);              // FF3 0x002bccd8: the count; with store into list

    /* 0x00 */ u_short type;            // ITEM_TYPE_* mask
    /* 0x02 */ u_short world;           // ITEM_WORLD_* mask (ItemGetWorldKind)
    /* 0x04 */ u_short menu;            // ITEM_MENU_* mask
    /* 0x08 */ short  *list;            // num item ids (system heap)
    /* 0x0c */ short   num;
};
static_assert(offsetof(ITEM_MENU_LIST, world) == 2, "ITEM_MENU_LIST.world");
static_assert(offsetof(ITEM_MENU_LIST, menu) == 4, "ITEM_MENU_LIST.menu");
static_assert(offsetof(ITEM_MENU_LIST, list) == 8, "ITEM_MENU_LIST.list");
static_assert(offsetof(ITEM_MENU_LIST, num) == 8 + sizeof(short *), "ITEM_MENU_LIST.num follows list");

// The cursor on list index (negative: the last, past the end: the first),
// the view kept full when the list allows. No caller in FF3.
void ItemMenuCsrSetIndex(int tab, int index);   // FF3 0x001ec128

// Phase 59 StoryMenu_Item (no pre / after functions)
int  init_StoryMenu_Item(void);         // FF3 0x001eadd0
int  one_StoryMenu_Item(void);          // FF3 0x001eb310
int  end_StoryMenu_Item(void);          // FF3 0x001eb108

#endif /* _INGAME_MENU_MENU_ITEM_H */
