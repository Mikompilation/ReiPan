/* ==========================================================================
 *  ingame/item/prg/item_tbl.h
 *
 *  The item sets of ITEM_RESET (FF3-only, FF3 0x1ecd30..0x1ecdd8, between
 *  menu_item.c's atexit thunk and lang_sel.c; the file name is
 *  descriptive). An event's ITEM_RESET (EvItemReset) sets the counts of
 *  three items for every player from the table ingame_wrk._24 names
 *  (ITEM_TABLE_CHANGE_ picks it) at the game's difficulty: films 0 and 1
 *  and the recovery item 5. Table 0 empties them; tables 2..15 hold
 *  99 / 60 / 8 on difficulty 0 down to 20 / 20 / 1 on difficulty 3.
 * ======================================================================== */
#ifndef _INGAME_ITEM_PRG_ITEM_TBL_H
#define _INGAME_ITEM_PRG_ITEM_TBL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define ITEM_TBL_DIFFICULTY_NUM 4       // IngameWrkGetDifficulty's range
#define ITEM_TBL_NUM            0x10    // tables per difficulty
#define ITEM_TBL_SET_NUM        3       // (item, count) pairs per table

// One pair: ItemSetNum(item, num, 1).
struct ITEM_TBL_SET
{
    /* 0x00 */ short item;
    /* 0x02 */ short num;
};
static_assert(sizeof(ITEM_TBL_SET) == 4, "ITEM_TBL_SET is 4 bytes");
static_assert(offsetof(ITEM_TBL_SET, num) == 2, "ITEM_TBL_SET.num");

// rodata 0x35b280: [difficulty][table][pair], 0xc0 bytes a difficulty.
extern const ITEM_TBL_SET item_tbl[ITEM_TBL_DIFFICULTY_NUM][ITEM_TBL_NUM][ITEM_TBL_SET_NUM];
static_assert(sizeof(item_tbl[0]) == 0xc0, "item_tbl: 0xc0 bytes a difficulty");
static_assert(sizeof(item_tbl[0][0]) == 0xc, "item_tbl: 0xc bytes a table");

int ItemTblIsValid(u_int tbl);          // FF3 0x001ecd30: tbl < 0x10
int ItemTblReset(int tbl, u_int difficulty);    // FF3 0x001ecd40: ITEM_RESET; 1 done, 0 out of range

#endif /* _INGAME_ITEM_PRG_ITEM_TBL_H */
