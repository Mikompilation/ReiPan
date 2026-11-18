/* ==========================================================================
 *  system/mc/prg/mc_set_data.h
 *
 *  Memory-card paths, sizes and save-data marshalling (FF2 mc_set_data.c,
 *  FF3 0x202270..0x203160).
 *
 *    - Path builders: "/<dir>/<file>" from the directory names
 *      "BESLES-53825ZERO-BD" (game data) and "...-BA1".."-BA5" (albums).
 *    - The block walker: SetMemoryCardSaveDataInfo runs a save manifest
 *      (system/mc/dat/save_data.c) into {addr, size} pairs; the file size,
 *      the gather into a card buffer and the scatter back are built on it.
 *    - The play-data head: a 0x24-byte summary per save slot (five), itself
 *      the only block of the file "BESLES-53825ZERO-BD", so the load screen
 *      lists every slot after reading one small file.
 *    - icon.sys and the icon model's paths and sizes.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_SET_DATA_H
#define _SYSTEM_MC_PRG_MC_SET_DATA_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libmc.h"                  // sceMcIconSys
#include "ingame/ingame_wrk.h"          // PLAY_TIME

// One block of a card file: where it lives in memory and how many bytes.
// {nullptr, -1} ends a list. FF3 keeps 100 of them on the stack (0x320 bytes).
// layout: host -- addr is a pointer (EE 4 bytes, size at +4).
struct MC_SAVE_DATA
{
    /* 0x00 */ void *addr;
    /* 0x04 */ int   size;
};
static_assert(offsetof(MC_SAVE_DATA, addr) == 0x00, "MC_SAVE_DATA.addr");

// A save manifest entry: stores the block's address through `addr` and
// returns its size (FF2's callbacks filled the whole MC_SAVE_DATA instead).
typedef int (*MC_SET_SAVE_FUNC)(void **addr);

// Play-data head of one save slot (data 0x3f5df0, five slots, 0xb4 bytes),
// filled at save by SetMemoryCardPlayDataHead.
struct MC_PLAY_DATA_HEAD                /* 0x24 */
{
    /* 0x00 */ PLAY_TIME play_time;     // ingame_wrk.play_time
    /* 0x10 */ int       clear_num;     // FUN_001a5e60: the four finder-state counters summed
    /* 0x14 */ int       chapter;       // ingame_wrk._28
    /* 0x18 */ int       area;          // MapAreaNoGet()
    /* 0x1c */ s_char    b04;           // ingame_wrk.b04 (-1: none)
    /* 0x1d */ s_char    b02;           // ingame_wrk.b02
    /* 0x1e */ u_char    player;        // ingame_wrk.b00, current player number
    /* 0x1f */ u_char    difficulty;    // IngameWrkGetDifficulty(1)
    /* 0x20 */ s_char    finder_flg;    // finder state word +0xc, as a byte
    /* 0x21 */ s_char    data_flg;      // 1: the slot holds a save
    /* 0x22 */ u_char    _22[2];
};
static_assert(sizeof(MC_PLAY_DATA_HEAD) == 0x24, "MC_PLAY_DATA_HEAD is 0x24 bytes");
static_assert(offsetof(MC_PLAY_DATA_HEAD, clear_num) == 0x10, "MC_PLAY_DATA_HEAD.clear_num");
static_assert(offsetof(MC_PLAY_DATA_HEAD, chapter) == 0x14, "MC_PLAY_DATA_HEAD.chapter");
static_assert(offsetof(MC_PLAY_DATA_HEAD, area) == 0x18, "MC_PLAY_DATA_HEAD.area");
static_assert(offsetof(MC_PLAY_DATA_HEAD, b04) == 0x1c, "MC_PLAY_DATA_HEAD.b04");
static_assert(offsetof(MC_PLAY_DATA_HEAD, b02) == 0x1d, "MC_PLAY_DATA_HEAD.b02");
static_assert(offsetof(MC_PLAY_DATA_HEAD, player) == 0x1e, "MC_PLAY_DATA_HEAD.player");
static_assert(offsetof(MC_PLAY_DATA_HEAD, difficulty) == 0x1f, "MC_PLAY_DATA_HEAD.difficulty");
static_assert(offsetof(MC_PLAY_DATA_HEAD, finder_flg) == 0x20, "MC_PLAY_DATA_HEAD.finder_flg");
static_assert(offsetof(MC_PLAY_DATA_HEAD, data_flg) == 0x21, "MC_PLAY_DATA_HEAD.data_flg");

// Soft reset off / on while a screen holds the card (SYS_WRK.sreset_ng).
void MemoryCardSoftResetLock(void);                                          // FF3 0x00202270
void MemoryCardSoftResetUnlock(void);                                        // FF3 0x002022a8

// Paths (buffers of 55 bytes; directory names 21)
void MemoryCardSetFilePath(char *path_name, int dir_label, int file_label);  // FF3 0x002022d8
void MemoryCardSetDirName(char *dir_name, int dir_label);                    // FF3 0x00202cc0
void MemoryCardMakeSearchDirPath(char *path_name, int dir_label);            // FF3 0x00202d30: "<dir>/*"
void MemoryCardSetIconSysPath(char *path_name, int dir_label);               // FF3 0x00202d80
void MemoryCardSetIconFilePath(char *icon_path, int dir_label, int icon_type); // FF3 0x00203018

// Save data
void SetMemoryCardSaveDataToBuff(void *addr, int dir_label, int file_label); // FF3 0x00202438
void DevelopMemoryCardLoadData(void *addr, int dir_label, int file_label);   // FF3 0x00202518
int  GetMemoryCardDirSize(int dir_label);                                    // FF3 0x002025a0
int  GetMemoryCardDirSizeCluster(int dir_label);                             // FF3 0x00202658: 1 KB clusters
int  GetMemoryCardDataSize(int dir_label, int file_label);                   // FF3 0x00202760: + 4 (checksum)
int  GetMemoryCardDataFileNum(int dir_label);                                // FF3 0x002029f0
int  GetMemoryCardAllFileNum(int dir_label);                                 // FF3 0x00202a08

// Staging buffer (system heap) and the icon model's load
void *GetDataMemoryArea(int size);                                           // FF3 0x00202928
void  LiberateDataMemoryArea(void **data_addr);                              // FF3 0x00202960: frees, clears
void  MemoryCardDataLoadReq(void *addr, int data_label);                     // FF3 0x002029b0
int   MemoryCardDataLoadWait(void);                                          // FF3 0x002029d0: 1 when loaded

// Play-data head
void MemoryCardPlayDataHeadInit(void);                                       // FF3 0x00202a28
void MemoryCardPlayDataHeadClear(int slot);                                  // FF3 0x00202a50
void SetMemoryCardPlayDataHead(int slot);                                    // FF3 0x00202a80
PLAY_TIME *GetMemoryCardPlayDataPlayTime(int slot);  // FF3 0x00202b68
int  GetMemoryCardPlayDataB04(int slot);             // FF3 0x00202b88: -1 none
int  GetMemoryCardPlayDataB02(int slot);             // FF3 0x00202ba8
int  GetMemoryCardPlayDataPlayer(int slot);          // FF3 0x00202bc8
int  GetMemoryCardPlayDataDifficulty(int slot);      // FF3 0x00202be8
int  GetMemoryCardPlayDataClearNum(int slot);        // FF3 0x00202c08
int  GetMemoryCardPlayDataChapter(int slot);         // FF3 0x00202c28
int  GetMemoryCardPlayDataArea(int slot);            // FF3 0x00202c48
int  GetMemoryCardPlayDataFinderFlg(int slot);       // FF3 0x00202c68
int  GetMemoryCardPlayDataFlg(int slot);             // FF3 0x00202c88: 1 a save in the slot
int  SetSave_PlayDataHead(void **addr);              // FF3 0x00202ca8: save callback

// icon.sys and the icon model
sceMcIconSys MemoryCardSetIconSysData(int dir_label);                        // FF3 0x00202e30
void MemoryCardSetIconTitleName(char *title, int dir_label);                 // FF3 0x00202f18
void MemoryCardSetIconFileName(char *icon_name, int dir_label, int icon_type); // FF3 0x00202fa8
int  GetIconDataLabel(int dir_label, int icon_type);                         // FF3 0x00203100: file number
int  GetIconDataSize(int dir_label, int icon_type);                          // FF3 0x00203128

#endif /* _SYSTEM_MC_PRG_MC_SET_DATA_H */
