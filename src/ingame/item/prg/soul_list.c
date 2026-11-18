/* ==========================================================================
 *  ingame/item/prg/soul_list.c
 *
 *  The ghost list (see soul_list.h). FF2's soul_list.c kept a state and a
 *  best score per entry and asserted every label; FF3 keeps two flag bytes,
 *  checks nothing and adds the entry-count bonuses. FF3 0x1d8208..0x1d84f8.
 * ======================================================================== */
#include "ingame/item/prg/soul_list.h"

#include <string.h>                     // memset

#include "ingame/menu/start_menu.h"     // GetFinderStateWrk, FINDER_STATE_WRK

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

int       soul_list_bonus[SOUL_LIST_BONUS_NUM];         // bss 0x3eb3a0
SOUL_LIST soul_list[SOUL_LIST_MAX];                     // bss 0x3eb3b0

static_assert(sizeof(soul_list) == 0x1b2, "soul_list is the 0x1b2-byte save block 21");

// Entries photographed for each bonus.
static short soul_list_bonus_num[SOUL_LIST_BONUS_NUM] = // sdata 0x370ae8
{
    50, 100, 150, 217,
};

// ──────────────────────────────────────────────────────────────────────
// Save blocks 21 and 22 (system/mc/dat/save_data.c).
// FF3 0x001d8208

int SoulListSaveGet(void **addr)
{
    *addr = soul_list;
    return sizeof(soul_list);
}

// FF3 0x001d8220

int SoulListBonusSaveGet(void **addr)
{
    *addr = soul_list_bonus;
    return sizeof(soul_list_bonus);
}

// The ROM clears both arrays twice: memset, then loops.
// FF3 0x001d8238

void SoulListInit(void)
{
    int i;

    memset(soul_list_bonus, 0, sizeof(soul_list_bonus));
    memset(soul_list, 0, sizeof(soul_list));

    for (i = 0; i < SOUL_LIST_BONUS_NUM; i++)
        soul_list_bonus[i] = 0;

    for (i = 0; i < SOUL_LIST_MAX; i++)
    {
        soul_list[i].have = 0;
        soul_list[i].read = 0;
    }
}

// FF3 0x001d82c8

s_char SoulListIsHave(int no)
{
    return soul_list[no].have;
}

// FF3 0x001d82e0

void SoulListSetHave(int no)
{
    soul_list[no].have = 1;
}

// FF3 0x001d8300

s_char SoulListIsRead(int no)
{
    return soul_list[no].read;
}

// FF3 0x001d8318

void SoulListSetRead(int no)
{
    soul_list[no].read = 1;
}

// Which part of the list an entry lies in: 0 the first 33, 1 up to 0xa9,
// 2 up to 0xc1, 3 the rest. Nothing in FF3 calls it (an inline function's
// out-of-line copy).
// FF3 0x001d8338

int SoulListGetGroup(int no)
{
    if (no < 0x21)
        return 0;
    if (no < 0xaa)
        return 1;
    if (no < 0xc2)
        return 2;
    return 3;
}

// FF3 0x001d8368

int SoulListIsFull(void)
{
    return GetFinderStateWrk()->FUN_001a5e60() > 0;
}

// FF3 0x001d83a0

int SoulListGetNum(void)
{
    return SoulListIsFull() ? SOUL_LIST_MAX : SOUL_LIST_FIRST_NUM;
}

// FF3 0x001d83d0

int SoulListGetHaveNum(void)
{
    int num;
    int cnt;
    int i;

    cnt = 0;
    num = SoulListGetNum();
    for (i = 0; i < num; i++)
    {
        if (SoulListIsHave(i) != 0)
            cnt++;
    }
    return cnt;
}

// FF3 0x001d8430

int SoulListGetNewBonus(void)
{
    int i;

    for (i = 0; i < SOUL_LIST_BONUS_NUM; i++)
    {
        if (soul_list_bonus[i] == 0 && SoulListBonusIsReached(i) != 0)
            return i;
    }
    return SOUL_LIST_BONUS_NUM;
}

// FF3 0x001d8498

int SoulListBonusIsReached(int no)
{
    return (short)SoulListGetHaveNum() >= soul_list_bonus_num[no];
}

// FF3 0x001d84d8

void SoulListBonusSet(int no)
{
    soul_list_bonus[no] = 1;
}
