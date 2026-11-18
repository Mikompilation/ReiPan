/* ==========================================================================
 *  ingame/map/map_bgm.h
 *
 *  Per-room background music (FF2 map_bgm.c). FF3 0x1fa768..0x1fada8.
 *  FF2 kept one stream file per room id; FF3 keys its table by (room,
 *  sub-room) with five stream columns (SET_BGM_LIST picks one) and a reverb
 *  depth per row, and picks fixed streams for the candle, chapter 0 and the
 *  other side.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAP_BGM_H
#define _INGAME_MAP_MAP_BGM_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define MAP_BGM_TBL_NUM  125            // rows of map_bgm_tbl
#define MAP_BGM_LIST_NUM 5              // stream columns per row

// One row of the disc table (data 0x32a6d8, 0x1c bytes): the streams of a
// room / sub-room. Rows nobody matches fall back to row 0.
struct MAP_BGM_DAT                      /* 0x1c */
{
    /* 0x00 */ short room;              // 1..509 (floor * 100 + room)
    /* 0x02 */ short sub;
    /* 0x04 */ int   str_file[MAP_BGM_LIST_NUM];    // CD_FILE_DAT of the stream (its header is file + 1), -1 none
    /* 0x18 */ short rev_depth;         // reverb depth 0..127 (SndSetEffect core 0)
    /* 0x1a */ short rev_mode;          // always 3; the code passes a constant 3
};
static_assert(sizeof(MAP_BGM_DAT) == 0x1c, "MAP_BGM_DAT is 0x1c bytes");
static_assert(offsetof(MAP_BGM_DAT, str_file) == 0x04, "MAP_BGM_DAT.str_file");
static_assert(offsetof(MAP_BGM_DAT, rev_depth) == 0x18, "MAP_BGM_DAT.rev_depth");

// The saved block (bss 0x3f3038, 0x1fc bytes; save block 13).
struct MAP_BGM_SAVE                     /* 0x1fc */
{
    /* 0x000 */ int aMapWrkBGMTbl[MAP_BGM_TBL_NUM];  // column 0 copied at init; EvMapStreamChange edits it
    /* 0x1f4 */ int disable_cnt;        // map_bgmFadeOut / map_bgmFadeIn nesting
    /* 0x1f8 */ int list;               // the column of map_bgm_tbl played (SET_BGM_LIST)
};
static_assert(sizeof(MAP_BGM_SAVE) == 0x1fc, "MAP_BGM_SAVE is 0x1fc bytes");
static_assert(offsetof(MAP_BGM_SAVE, disable_cnt) == 0x1f4, "MAP_BGM_SAVE.disable_cnt");
static_assert(offsetof(MAP_BGM_SAVE, list) == 0x1f8, "MAP_BGM_SAVE.list");

int  map_bgmSetSave(void **addr);       // FF3 0x001fa768: {&map_bgmSave, 0x1fc}
void map_bgmSetList(int list);          // FF3 0x001fa780: SET_BGM_LIST
void map_bgmInit(void);                 // FF3 0x001fa7e8: IngameInit, IngameNewGameReset
void map_bgmChangeTbl(int no, int str_file);    // FF3 0x001fa858: EvMapStreamChange (row no)
void map_bgmMain(void);                 // FF3 0x001fab88: IngameMain
void map_bgmFadeOut(int time, int vol); // FF3 0x001fabc0: vol 0 stops it (keeping its sector), else a duck
void map_bgmRelease(int time);          // FF3 0x001fac68: IngameEnd, IngameNewGameReset
void map_bgmFadeIn(int time);           // FF3 0x001faca0: undoes one map_bgmFadeOut
int  map_bgmIsPlaying(void);            // FF3 0x001fad40

#endif /* _INGAME_MAP_MAP_BGM_H */
