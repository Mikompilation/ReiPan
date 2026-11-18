/* ==========================================================================
 *  ingame/item/prg/item_tbl.c
 *
 *  The item sets of ITEM_RESET (see item_tbl.h). FF3 0x1ecd30..0x1ecdd8,
 *  FF3's own code.
 * ======================================================================== */
#include "ingame/item/prg/item_tbl.h"

#include "ingame/item/prg/item.h"       // ItemSetNum

// rodata 0x35b280. Each difficulty: table 0 sets the three items to 0,
// table 1 only the recovery item, tables 2..15 all three.
#define ITEM_TBL_ROW(film0, film1, heal)    { { 0, film0 }, { 1, film1 }, { 5, heal } }
#define ITEM_TBL_DIFFICULTY(film0, film1, heal)                                 \
    {                                                                           \
        ITEM_TBL_ROW(0, 0, 0),                                                  \
        ITEM_TBL_ROW(0, 0, heal),                                               \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
        ITEM_TBL_ROW(film0, film1, heal), ITEM_TBL_ROW(film0, film1, heal),     \
    }

const ITEM_TBL_SET item_tbl[ITEM_TBL_DIFFICULTY_NUM][ITEM_TBL_NUM][ITEM_TBL_SET_NUM] =
{
    ITEM_TBL_DIFFICULTY(99, 60, 8),
    ITEM_TBL_DIFFICULTY(50, 30, 3),
    ITEM_TBL_DIFFICULTY(30, 25, 2),
    ITEM_TBL_DIFFICULTY(20, 20, 1),
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001ecd30

int ItemTblIsValid(u_int tbl)
{
    return tbl < ITEM_TBL_NUM;
}

// ──────────────────────────────────────────────────────────────────────
// ITEM_RESET: the counts of table tbl at the difficulty, for every player.
// FF3 0x001ecd40 (EvItemReset)

int ItemTblReset(int tbl, u_int difficulty)
{
    const ITEM_TBL_SET *set = nullptr;
    int i;

    if (difficulty < ITEM_TBL_DIFFICULTY_NUM && ItemTblIsValid((u_int)tbl))
        set = item_tbl[difficulty][tbl];
    if (set == nullptr)
        return 0;
    for (i = 0; i < ITEM_TBL_SET_NUM; i++, set++)
        ItemSetNum(set->item, set->num, 1);
    return 1;
}
