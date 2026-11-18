/* ==========================================================================
 *  ingame/item/prg/soul_list.h
 *
 *  The ghost list ("soul list", FF2 ingame/item/prg/soul_list.c reshaped
 *  for FF3; FF3 0x1d8208..0x1d84f8). FF3 keeps two bytes per entry and no
 *  score: `have` is set when a photo catches the ghost (SoulListSetHave,
 *  from the photo code), `read` when the list shows it. Only the first 33
 *  entries count before the game has been cleared once; after a clear all
 *  217 do (SoulListGetNum). Reaching 50 / 100 / 150 / 217 entries opens
 *  four bonuses (the start menu's unlock slots, SoulListBonusIsReached);
 *  the four flags remember the ones handed out. Both arrays are save
 *  blocks (save_data.c 21 / 22).
 * ======================================================================== */
#ifndef _INGAME_ITEM_PRG_SOUL_LIST_H
#define _INGAME_ITEM_PRG_SOUL_LIST_H

#include "sdk/scetypes.h"

#define SOUL_LIST_MAX           217     // entries (0x1b2 bytes of save data)
#define SOUL_LIST_FIRST_NUM     33      // entries that count before the first clear
#define SOUL_LIST_BONUS_NUM     4       // bonus thresholds (sdata 0x370ae8)

// One entry (bss 0x3eb3b0 + no * 2).
struct SOUL_LIST
{
    /* 0x0 */ s_char have;              // 1: photographed (FF2 SOUL_LIST_STATE_HAVE)
    /* 0x1 */ s_char read;              // 1: looked at in the list (FF2 SOUL_LIST_STATE_READ)
};
static_assert(sizeof(SOUL_LIST) == 2, "SOUL_LIST is 2 bytes");

extern int       soul_list_bonus[SOUL_LIST_BONUS_NUM];  // bss 0x3eb3a0: bonus i handed out
extern SOUL_LIST soul_list[SOUL_LIST_MAX];              // bss 0x3eb3b0

int    SoulListSaveGet(void **addr);        // FF3 0x001d8208: {soul_list, 0x1b2}
int    SoulListBonusSaveGet(void **addr);   // FF3 0x001d8220: {soul_list_bonus, 0x10}
void   SoulListInit(void);                  // FF3 0x001d8238: IngameInit
s_char SoulListIsHave(int no);              // FF3 0x001d82c8
void   SoulListSetHave(int no);             // FF3 0x001d82e0: a ghost photographed
s_char SoulListIsRead(int no);              // FF3 0x001d8300
void   SoulListSetRead(int no);             // FF3 0x001d8318
int    SoulListGetGroup(int no);            // FF3 0x001d8338: 0..3 by entry range (no caller in FF3)
int    SoulListIsFull(void);                // FF3 0x001d8368: the game was cleared once: all entries count
int    SoulListGetNum(void);                // FF3 0x001d83a0: 217, or 33 before a clear
int    SoulListGetHaveNum(void);            // FF3 0x001d83d0: photographed entries of those
int    SoulListGetNewBonus(void);           // FF3 0x001d8430: first reached bonus not yet handed out, 4 none
int    SoulListBonusIsReached(int no);      // FF3 0x001d8498: SoulListGetHaveNum() >= threshold no
void   SoulListBonusSet(int no);            // FF3 0x001d84d8: bonus no handed out

#endif /* _INGAME_ITEM_PRG_SOUL_LIST_H */
