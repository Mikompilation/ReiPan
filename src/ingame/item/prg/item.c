/* ==========================================================================
 *  ingame/item/prg/item.c
 *
 *  Items: the per-player counts, getting, using and losing items
 *  (FF3 0x1ea2b0..0x1eac20; FF2's item.c, rewritten). See item.h for the
 *  tables. FF2 kept one PLYR_ITEM row per item and asserted on every id;
 *  FF3 keeps a count per item and player; ItemGetCnt does not check the
 *  id, its callers here read 0 for an id >= 0x45 (inlined in the ROM).
 *
 *  0x1eabc0 is a compiler atexit thunk (CandleControl's destructor 0x2ad380
 *  on GetCandleControl's static 0x41e540, this file's copy); the host runs
 *  the static's destructor itself.
 * ======================================================================== */
#include "ingame/item/prg/item.h"

#include "ingame/candle_ctrl.h"         // GetCandleControl, CandleControl::SetPower
#include "ingame/ingame_wrk.h"          // ingame_wrk (the current player work)
#include "ingame/photo/finder_photo.h"  // GetFilmWrk
#include "ingame/photo/fndr_parts.h"    // FilmSetByItem
#include "ingame/plyr/player.h"         // PlyrGetCurNo, PlyrAddHp
#include "ingame/plyr/plyr_param.h"     // PlyrMdlGetHpMax
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "ingame/story_menu.h"          // IsLockTblOn

static_assert(ITEM_PLYR_NUM == PLYR_NUM, "a count row per player work");

#define ITEM_LOCK_USE   0x14            // IsLockTblOn id that forbids using items
#define ITEM_CANDLE     0x11            // ITEM_TYPE_CONSUME item that relights the candle
#define SE_ITEM_HEAL    0x1a9b0006      // ItemPlyrHPRecover's sound

// The item data table, rodata 0x35ad38:
//    id    type               world menu max value  pic   _0e
const ITEM_DAT item_dat[ITEM_NUM] =
{
    { 0x00, ITEM_TYPE_FILM,      1, 4,  99,  99, 0x52, 0x00 },
    { 0x01, ITEM_TYPE_FILM,      1, 4,  99,  99, 0x54, 0x01 },
    { 0x02, ITEM_TYPE_FILM,      1, 4,  99,  99, 0x56, 0x02 },
    { 0x03, ITEM_TYPE_FILM,      1, 4,  99,  99, 0x58, 0x03 },
    { 0x04, ITEM_TYPE_FILM,      1, 4,  99,  99, 0x5a, 0x04 },
    { 0x05, ITEM_TYPE_HP,        1, 1,  99,  30, 0x5c, 0x05 },
    { 0x06, ITEM_TYPE_HP,        1, 1,  99, 100, 0x5e, 0x06 },
    { 0x07, ITEM_TYPE_CONSUME,   1, 8,   1, 100, 0x60, 0x07 },
    { 0x08, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x2a, 0x0b },
    { 0x09, ITEM_TYPE_KEY,       2, 8,   1,  99, 0x2a, 0x0c },
    { 0x0a, ITEM_TYPE_KEY,       3, 8,   1,  99, 0xba, 0x23 },
    { 0x0b, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x62, 0x09 },
    { 0x0c, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x6e, 0x29 },
    { 0x0d, ITEM_TYPE_KEY,       2, 8,   1,  99, 0x6e, 0x2a },
    { 0x0e, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x2c, 0x0d },
    { 0x0f, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x2e, 0x0e },
    { 0x10, ITEM_TYPE_KEY,       2, 8,   1,  99, 0x30, 0x0f },
    { 0x11, ITEM_TYPE_CONSUME,   1, 1,   1,   0, 0x2e, 0x12 },
    { 0x12, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x50, 0x16 },
    { 0x13, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x4a, 0x17 },
    { 0x14, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x3a, 0x19 },
    { 0x15, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x48, 0x1a },
    { 0x16, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x4e, 0x1b },
    { 0x17, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x66, 0x25 },
    { 0x18, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x7c, 0x30 },
    { 0x19, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x8c, 0x38 },
    { 0x1a, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x8e, 0x39 },
    { 0x1b, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x90, 0x3a },
    { 0x1c, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x9e, 0x40 },
    { 0x1d, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xa0, 0x41 },
    { 0x1e, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xd2, 0x55 },
    { 0x1f, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xbc, 0x56 },
    { 0x20, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xc2, 0x57 },
    { 0x21, ITEM_TYPE_KEY,       3, 8,   1,  99, 0xc0, 0x59 },
    { 0x22, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xa4, 0x43 },
    { 0x23, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xa6, 0x44 },
    { 0x24, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xa8, 0x45 },
    { 0x25, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xaa, 0x46 },
    { 0x26, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xb0, 0x49 },
    { 0x27, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xb2, 0x4a },
    { 0x28, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xb4, 0x4b },
    { 0x29, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xb6, 0x4c },
    { 0x2a, ITEM_TYPE_KEY,       1, 8,   1,  99, 0xb8, 0x4d },
    { 0x2b, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x64, 0x24 },
    { 0x2c, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x74, 0x2d },
    { 0x2d, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x7a, 0x2f },
    { 0x2e, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x6a, 0x27 },
    { 0x2f, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x94, 0x3c },
    { 0x30, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x98, 0x3d },
    { 0x31, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x7e, 0x31 },
    { 0x32, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x80, 0x32 },
    { 0x33, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x82, 0x33 },
    { 0x34, ITEM_TYPE_KEY,       1, 8,   1,  99, 0x84, 0x34 },
    { 0x35, ITEM_TYPE_KEY,       2, 8,   1,  99, 0x6c, 0x28 },
    { 0x36, ITEM_TYPE_KEY,       2, 8,   1,  99, 0x76, 0x2e },
    { 0x37, ITEM_TYPE_KEY,       2, 8,   1,  99, 0xa2, 0x42 },
    { 0x38, ITEM_TYPE_KEY,       2, 8,   1,  99, 0xae, 0x48 },
    { 0x39, ITEM_TYPE_KEY,       2, 8,   1,  99, 0xc4, 0x4e },
    { 0x3a, ITEM_TYPE_KEY,       2, 8,   1,  99, 0xc6, 0x4f },
    { 0x3b, ITEM_TYPE_KEY,       2, 8,   1,  99, 0xc6, 0x74 },
    { 0x3c, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xce, 0x26 },
    { 0x3d, ITEM_TYPE_KEY_SHARED, 3, 8,   1,  99, 0x68, 0x50 },
    { 0x3e, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xc8, 0x51 },
    { 0x3f, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xca, 0x52 },
    { 0x40, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xcc, 0x53 },
    { 0x41, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xd0, 0x54 },
    { 0x42, ITEM_TYPE_KEY_SHARED, 3, 8,   1,  99, 0xca, 0x71 },
    { 0x43, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xca, 0x71 },
    { 0x44, ITEM_TYPE_KEY_SHARED, 1, 8,   1,  99, 0xd0, 0x73 },
};

// Each player's count of each item, data 0x328fe0 (0x19e bytes, save block 0).
short plyr_item_cnt[ITEM_PLYR_NUM][ITEM_NUM];

// sbss gp-0xb90: GetCandleControl() cached on the first candle item used.
static CandleControl *item_candle;

// ──────────────────────────────────────────────────────────────────────
// Counts.

// FF3 0x001ea2b0
short *ItemGetCnt(short item)
{
    return &plyr_item_cnt[(s_char)PlyrGetCurNo()][item];
}

// Empty; AllPlyrItemInit calls it first.
// FF3 0x001eabe0
static void AllPlyrItemInitSub(void)
{
}

// FF3 0x001ea300
void AllPlyrItemInit(void)
{
    AllPlyrItemInitSub();

    for (short i = 0; i < ITEM_NUM; i++) {
        for (s_char p = 0; p < ITEM_PLYR_NUM; p++) {
            plyr_item_cnt[p][i] = 0;
        }
    }
}

// New Game keeps the shared key items, everything else goes.
// FF3 0x001ea368
void AllPlyrItemNewGame(void)
{
    for (short i = 0; i < ITEM_NUM; i++) {
        if (item_dat[i].type != ITEM_TYPE_KEY_SHARED) {
            for (s_char p = 0; p < ITEM_PLYR_NUM; p++) {
                plyr_item_cnt[p][i] = 0;
            }
        }
    }
}

// FF3 0x001ea3d8
void AllPlyrItemEnd(void)
{
}

// Sets the count, clamped to 0..max; for every player when all is set or
// the item is shared. Answers whether num was in range.
// FF3 0x001ea3f0
int ItemSetNum(short item, short num, int all)
{
    int set;

    if (num < 0) {
        set = 0;
    }
    else {
        set = num;
        if (item_dat[item].max < num) {
            set = item_dat[item].max;
        }
    }

    if (all == 0 && item_dat[item].type != ITEM_TYPE_KEY_SHARED) {
        *ItemGetCnt(item) = (short)set;
    }
    else {
        for (s_char p = 0; p < ITEM_PLYR_NUM; p++) {
            plyr_item_cnt[p][item] = (short)set;
        }
    }
    return num == set;
}

// FF3 0x001ea4b8
int ItemUsePossible(u_short item, short num)
{
    int have = item < ITEM_NUM ? *ItemGetCnt((short)item) : 0;

    return have - num >= 0;
}

// FF3 0x001ea518
int ItemLost(u_short item, short num)
{
    if (!ItemUsePossible(item, 1)) {
        return 0;
    }
    int have = item < ITEM_NUM ? *ItemGetCnt((short)item) : 0;

    return ItemSetNum((short)item, (short)(have - num), 0) ? 1 : -1;
}

// FF3 0x001ea5a8
int ItemCanUse(short item, short num)
{
    if (IsLockTblOn(ITEM_LOCK_USE)) {
        return 0;
    }
    return ItemUsePossible(item, num) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Use handlers, by type.

// FF3 0x001ea908
static int ItemFilmEquip(short item, short num)
{
    FilmSetByItem(GetFilmWrk(), item);
    return 1;
}

// One item at a time while the player still takes hp: hp_max * value %
// each. 1 when all num were used, -1 when only some, 0 when none.
// FF3 0x001ea940
static int ItemPlyrHPRecover(short item, short num)
{
    int have = (u_short)item < ITEM_NUM ? *ItemGetCnt(item) : 0;
    int used = 0;
    int res = 0;
    int heal = (int)((u_int)(PlyrMdlGetHpMax(PlyrGetCurNo(), 0xff) * item_dat[item].value) / 100);

    while (used < num) {
        if (!ItemCanUse(item, 1) || !PlyrAddHp(ingame_wrk.plyr_wrk, heal)) {
            res = used != 0 ? -1 : 0;
            break;
        }
        used++;
        ItemLost(item, 1);
    }

    int left = (u_short)item < ITEM_NUM ? *ItemGetCnt(item) : 0;

    if (have - left == num) {
        res = 1;
    }
    if (res == -1 || res == 1) {
        SndSePlay(SE_ITEM_HEAL, 100, 0x1000, 0, 0, 0);
    }
    return res;
}

// FF3 0x001eaad0
static int ItemUseConsume(short item, short num)
{
    if (item == ITEM_CANDLE) {
        if (item_candle == nullptr) {
            item_candle = GetCandleControl();
        }
        if (item_candle->grade == 0) {
            item_candle->grade = 4;
        }
        item_candle->SetPower(100.0f);          // sdata 0x370dbc
    }
    return ItemLost(item, num);
}

// FF3 0x001eab58
static int ItemUseDiscard(short item, short num)
{
    return ItemLost(item, num);
}

// FF3 0x001eab80
static int ItemUseKey(short item, short num)
{
    return 0;
}

// FF3 0x001eab88
static int ItemUseKeyShared(short item, short num)
{
    return 0;
}

// The ROM asks ItemCanUse twice.
// FF3 0x001ea608
int ItemUse(short item, short num)
{
    int res = 0;

    if (ItemCanUse(item, 1) && ItemCanUse(item, 1)) {
        switch (item_dat[item].type) {
        case ITEM_TYPE_FILM:
            res = ItemFilmEquip(item, num);
            break;
        case ITEM_TYPE_HP:
            res = ItemPlyrHPRecover(item, num);
            break;
        case ITEM_TYPE_CONSUME:
            res = ItemUseConsume(item, num);
            break;
        case ITEM_TYPE_DISCARD:
            res = ItemUseDiscard(item, num);
            break;
        case ITEM_TYPE_KEY:
            res = ItemUseKey(item, num);
            break;
        case ITEM_TYPE_KEY_SHARED:
            res = ItemUseKeyShared(item, num);
            break;
        }
    }
    return res;
}

// ──────────────────────────────────────────────────────────────────────
// Getting items.

// FF3 0x001ea718
int ItemGetPossible(u_short item, short num)
{
    return item < ITEM_NUM && *ItemGetCnt((short)item) + num <= item_dat[(short)item].max;
}

// The per-type get hooks are all empty in the ROM.
// FF3 0x001eab90
static void ItemOnGetFilm(short item, short num)
{
}

// FF3 0x001eab98
static void ItemOnGetHP(short item, short num)
{
}

// FF3 0x001eaba0
static void ItemOnGetConsume(short item, short num)
{
}

// FF3 0x001eaba8
static void ItemOnGetDiscard(short item, short num)
{
}

// FF3 0x001eabb0
static void ItemOnGetKey(short item, short num)
{
}

// FF3 0x001eabb8
static void ItemOnGetKeyShared(short item, short num)
{
}

// FF3 0x001ea790
int ItemGet(u_short item, short num)
{
    short no = (short)item;

    if (!ItemGetPossible(no, 1)) {
        return 0;
    }

    switch (item_dat[no].type) {
    case ITEM_TYPE_FILM:
        ItemOnGetFilm(no, num);
        break;
    case ITEM_TYPE_HP:
        ItemOnGetHP(no, num);
        break;
    case ITEM_TYPE_CONSUME:
        ItemOnGetConsume(no, num);
        break;
    case ITEM_TYPE_DISCARD:
        ItemOnGetDiscard(no, num);
        break;
    case ITEM_TYPE_KEY:
        ItemOnGetKey(no, num);
        break;
    case ITEM_TYPE_KEY_SHARED:
        ItemOnGetKeyShared(no, num);
        break;
    }

    int have = item < ITEM_NUM ? *ItemGetCnt(no) : 0;

    return ItemSetNum(no, (short)(have + num), 0) ? 1 : -1;
}

// ──────────────────────────────────────────────────────────────────────

// FF3 0x001ea8c8
int ItemGetWorldKind(s_char world)
{
    if (world == 0) {
        return 1;
    }
    if (world == 1) {
        return 2;
    }
    return 3;
}

// FF3 0x001ea8f0
int ItemSaveGet(void **addr)
{
    *addr = plyr_item_cnt;
    return sizeof(plyr_item_cnt);
}
