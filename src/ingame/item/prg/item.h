/* ==========================================================================
 *  ingame/item/prg/item.h
 *
 *  Items (FF2 ingame/item/prg/item.c, rewritten for FF3; FF3 0x1ea2b0..
 *  0x1eac20). FF3 keeps one count per item and player: plyr_item_cnt
 *  (data 0x328fe0) holds three rows of 0x45 shorts, one per player work
 *  (Rei, Miku, Kei); ItemGetCnt points into the current player's row. The
 *  item data table item_dat (rodata 0x35ad38) gives each item its type,
 *  maximum and value. Items of type ITEM_TYPE_KEY_SHARED are written to all
 *  three rows and survive New Game (AllPlyrItemNewGame).
 *
 *  Return codes of ItemGet / ItemLost: 1 done, -1 done but clamped to
 *  0..max, 0 not possible.
 * ======================================================================== */
#ifndef _INGAME_ITEM_PRG_ITEM_H
#define _INGAME_ITEM_PRG_ITEM_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define ITEM_NUM        0x45            // items (ids 0..0x44)
#define ITEM_PLYR_NUM   3               // count rows: one per player work (PLYR_NUM)

// ITEM_DAT::type (a bit each; ItemUse / ItemGet dispatch on it).
#define ITEM_TYPE_FILM          0x01    // ids 0..4: ItemFilmEquip
#define ITEM_TYPE_HP            0x02    // ids 5, 6: ItemPlyrHPRecover by value %
#define ITEM_TYPE_CONSUME       0x04    // ids 7, 0x11: ItemUseConsume (0x11 relights the candle)
#define ITEM_TYPE_DISCARD       0x08    // ItemUseDiscard (no item has this type)
#define ITEM_TYPE_KEY           0x10    // ids 8..0x3b except 0x11: cannot be used
#define ITEM_TYPE_KEY_SHARED    0x20    // ids 0x3c..0x44: one count for all players, kept over New Game

// ITEM_DAT::world: the item menu lists an item when this ANDs with
// ItemGetWorldKind(ingame_wrk.b03) (1 the real world, 2 the dream world).
#define ITEM_WORLD_REAL         0x01
#define ITEM_WORLD_DREAM        0x02

// ITEM_DAT::menu: ItemMenuUseCheck lets the item menu use an item only with
// ITEM_MENU_USE or ITEM_MENU_FILM set (& 5); the lists take every bit (0xf).
#define ITEM_MENU_USE           0x01    // recovery items, the candle 0x11
#define ITEM_MENU_FILM          0x04    // films: loaded into the camera
#define ITEM_MENU_NONE          0x08    // keys and 0x07: listed, not usable

// The item data table, rodata 0x35ad38 (0x45 rows of 0x10).
struct ITEM_DAT
{
    /* 0x00 */ short id;            // = the row
    /* 0x02 */ short type;          // ITEM_TYPE_*
    /* 0x04 */ short world;         // ITEM_WORLD_* bits: the worlds whose item menu lists it (ItemGetWorldKind)
    /* 0x06 */ short menu;          // ITEM_MENU_* bits: 4 films, 1 recovery / 0x11, 8 the rest
    /* 0x08 */ short max;           // the most one player can hold (ItemSetNum clamps to it)
    /* 0x0a */ short value;         // ITEM_TYPE_HP: % of hp_max healed per item
    /* 0x0c */ short pic;           // even numbers 0x2a..0xd2: the item menu's picture (MENU_PIC.pic), pic + 1 its additive glow
    /* 0x0e */ short _0e;           // 0..0x74: message number of the name (kind 0x56) and description (kind 0x55); the cassette menu
};
static_assert(sizeof(ITEM_DAT) == 0x10, "ITEM_DAT size");
static_assert(offsetof(ITEM_DAT, type) == 0x02, "ITEM_DAT.type");
static_assert(offsetof(ITEM_DAT, world) == 0x04, "ITEM_DAT.world");
static_assert(offsetof(ITEM_DAT, menu) == 0x06, "ITEM_DAT.menu");
static_assert(offsetof(ITEM_DAT, max) == 0x08, "ITEM_DAT.max");
static_assert(offsetof(ITEM_DAT, pic) == 0x0c, "ITEM_DAT.pic");
static_assert(offsetof(ITEM_DAT, value) == 0x0a, "ITEM_DAT.value");
static_assert(offsetof(ITEM_DAT, _0e) == 0x0e, "ITEM_DAT._0e");

extern const ITEM_DAT item_dat[ITEM_NUM];                   // rodata 0x35ad38
extern short plyr_item_cnt[ITEM_PLYR_NUM][ITEM_NUM];        // data 0x328fe0 (save block 0)
static_assert(sizeof(plyr_item_cnt) == 0x19e, "plyr_item_cnt size (ItemSaveGet)");

short *ItemGetCnt(short item);                      // FF3 0x001ea2b0: the current player's count
void   AllPlyrItemInit(void);                       // FF3 0x001ea300: IngameInit, all counts 0
void   AllPlyrItemNewGame(void);                    // FF3 0x001ea368: IngameNewGameReset
void   AllPlyrItemEnd(void);                        // FF3 0x001ea3d8: IngameEnd
int    ItemSetNum(short item, short num, int all);  // FF3 0x001ea3f0: clamped; all: every player
int    ItemUsePossible(u_short item, short num);    // FF3 0x001ea4b8: count >= num
int    ItemLost(u_short item, short num);           // FF3 0x001ea518: ITEM_LOST
int    ItemCanUse(short item, short num);           // FF3 0x001ea5a8: not locked (lock 0x14) and possible
int    ItemUse(short item, short num);              // FF3 0x001ea608: ITEM_USE
int    ItemGetPossible(u_short item, short num);    // FF3 0x001ea718: count + num <= max
int    ItemGet(u_short item, short num);            // FF3 0x001ea790
int    ItemGetWorldKind(s_char world);              // FF3 0x001ea8c8: 0 -> 1, 1 -> 2, else 3
int    ItemSaveGet(void **addr);                    // FF3 0x001ea8f0: {plyr_item_cnt, 0x19e}

#endif /* _INGAME_ITEM_PRG_ITEM_H */
