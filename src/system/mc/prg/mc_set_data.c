/* ==========================================================================
 *  system/mc/prg/mc_set_data.c
 *
 *  Memory-card paths, sizes, save-data marshalling and the play-data head
 *  (FF3 0x202270..0x203160). From MioPan system/mc/prg/mc_set_data.c (FF2),
 *  checked against FF3 function by function. FF3 differences:
 *
 *    - no MemoryCardAssert / MemoryCardWarning / printf (retail), so the
 *      default branches are empty;
 *    - albums hold two files (the header named after the directory, and
 *      "Zero3Album<n>"), so the per-directory counts are {7,2,2,2,2,2};
 *    - save callbacks return the block size (MC_SET_SAVE_FUNC);
 *    - the gather (SetMemoryCardSaveDataToBuff) stops at the first block whose
 *      address is null *or* whose size is -1; the other two walkers keep FF2's
 *      "both" test;
 *    - the staging buffer comes from the system heap, and the play-data head
 *      is a plain struct per slot with FF3's own fields (player, difficulty,
 *      finder state, map area).
 * ======================================================================== */
#include "system/mc/prg/mc_set_data.h"

#include <string.h>

#include "common/heapctrl.h"
#include "ingame/ingame_wrk.h"              // ingame_wrk, IngameWrkGetDifficulty
#include "ingame/menu/start_menu.h"          // GetFinderStateWrk, FUN_001a5e60
#include "ingame/story_door.h"              // MapAreaNoGet
#include "system/eeiop/cd_file_dat.h"       // ZERO3GAME_ICO, ZERO3ALBUM0_ICO
#include "system/eeiop/cddat.h"             // GetFileSize
#include "system/mc/dat/save_data.h"        // the save manifests
#include "system/mc/prg/mc_cmn.h"           // Calc/SetMemoryCardDataCheckSum
#include "system/os/eecdvd.h"               // LoadReq, IsLoadEndAll
#include "system/os/sys_heap.h"             // GetSystemHeapWrkP
#include "system/os/system.h"               // GetSysWrk

// ══════════════════════════════════════════════════════════════════════
// Data
// ══════════════════════════════════════════════════════════════════════

// The manifest of each file of a directory: game data (dir_label 0) has the
// system file, the play-data head and the five save slots; every album
// (dir_label 1..5) the header and the photos.
static MC_SET_SAVE_FUNC *game_save_data[8] =        /* data 0x32b4e0 */
{
    save_system_data,                               // 0 "Zero3System"
    save_play_data_head,                            // 1 "BESLES-53825ZERO-BD"
    save_game_data,                                 // 2..6 "Zero3Play0".."Zero3Play4"
    save_game_data,
    save_game_data,
    save_game_data,
    save_game_data,
    nullptr
};

static MC_SET_SAVE_FUNC *album_save_data[3] =       /* data 0x32b500 */
{
    save_album_info,
    save_album_data,
    nullptr
};

static const char *game_dir_name[6] =               /* data 0x32b510 */
{
    "BESLES-53825ZERO-BD",
    "BESLES-53825ZERO-BA1",
    "BESLES-53825ZERO-BA2",
    "BESLES-53825ZERO-BA3",
    "BESLES-53825ZERO-BA4",
    "BESLES-53825ZERO-BA5",
};

static const char *game_file_name[8] =              /* data 0x32b528 */
{
    "Zero3System",
    "BESLES-53825ZERO-BD",
    "Zero3Play0",
    "Zero3Play1",
    "Zero3Play2",
    "Zero3Play3",
    "Zero3Play4",
    nullptr
};

static const char *album1_file_name[2] = { "BESLES-53825ZERO-BA1", "Zero3Album0" };  /* sdata 0x371128 */
static const char *album2_file_name[2] = { "BESLES-53825ZERO-BA2", "Zero3Album1" };  /* sdata 0x371130 */
static const char *album3_file_name[2] = { "BESLES-53825ZERO-BA3", "Zero3Album2" };  /* sdata 0x371138 */
static const char *album4_file_name[2] = { "BESLES-53825ZERO-BA4", "Zero3Album3" };  /* sdata 0x371140 */
static const char *album5_file_name[2] = { "BESLES-53825ZERO-BA5", "Zero3Album4" };  /* sdata 0x371148 */

// Data files per directory, and every directory entry (+ icon model and icon.sys).
static const int data_file_num[6] = { 7, 2, 2, 2, 2, 2 };   /* rodata 0x35bb70 */
static const int dir_file_num[6]  = { 9, 4, 4, 4, 4, 4 };   /* rodata 0x35bb88 */

// icon.sys's colours and lights, as the ROM's bytes (rodata 0x35ba10..0x35bac0;
// the floats are truncated, -0.2 is 0xbe4ccccc, so they stay bit patterns).
static const u_int mc_bgcolor[4][4] =                       /* rodata 0x35ba10 */
{
    { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0xb0, 0x40, 0x40, 0 },
};
static const u_int mc_lightdir[3][4] =                      /* rodata 0x35ba50: (-0.2 0.5 0.5) (0.3 -0.4 -0.1) (0.3 -0.5 0.5) */
{
    { 0xbe4ccccc, 0x3f000000, 0x3f000000, 0 },
    { 0x3e999999, 0xbecccccc, 0xbdcccccc, 0 },
    { 0x3e999999, 0xbf000000, 0x3f000000, 0 },
};
static const u_int mc_lightcol[3][4] =                      /* rodata 0x35ba80: (0.4 0.4 0.4) (0.25 0.3 0.3) (0.15 0.2 0.2) */
{
    { 0x3ecccccc, 0x3ecccccc, 0x3ecccccc, 0 },
    { 0x3e800000, 0x3e999999, 0x3e999999, 0 },
    { 0x3e199999, 0x3e4ccccc, 0x3e4ccccc, 0 },
};
static const u_int mc_ambient[4] = { 0x3e999999, 0x3e999999, 0x3e999999, 0 };  /* rodata 0x35bab0: 0.3 */

// icon.sys title, Shift-JIS, two rows split at byte 28 (OffsLF):
// "Project Zero 3" and the directory's own line.
static const char mc_icon_title[0x20] =                     /* rodata 0x35bf20 */
    "\x82\x6f\x82\x92\x82\x8f\x82\x8a\x82\x85\x82\x83\x82\x94\x81\x40"
    "\x82\x79\x82\x85\x82\x92\x82\x8f\x81\x40\x82\x52";
static const char mc_icon_sub_title[6][0x40] =              /* rodata 0x35bf40 */
{
    "\x82\x66\x82\x81\x82\x8d\x82\x85\x82\x63\x82\x81\x82\x94\x82\x81",                  // GameData
    "\x82\x60\x82\x8c\x82\x82\x82\x95\x82\x8d\x82\x63\x82\x81\x82\x94\x82\x81\x82\x50",  // AlbumData1
    "\x82\x60\x82\x8c\x82\x82\x82\x95\x82\x8d\x82\x63\x82\x81\x82\x94\x82\x81\x82\x51",  // AlbumData2
    "\x82\x60\x82\x8c\x82\x82\x82\x95\x82\x8d\x82\x63\x82\x81\x82\x94\x82\x81\x82\x52",  // AlbumData3
    "\x82\x60\x82\x8c\x82\x82\x82\x95\x82\x8d\x82\x63\x82\x81\x82\x94\x82\x81\x82\x53",  // AlbumData4
    "\x82\x60\x82\x8c\x82\x82\x82\x95\x82\x8d\x82\x63\x82\x81\x82\x94\x82\x81\x82\x54",  // AlbumData5
};

// The icon model's file name per directory, for view / copy / delete.
static const char mc_icon_name[6][3][31] =                  /* rodata 0x35bcf0 */
{
    { "zero-b-game.ico",   "zero-b-game.ico",   "zero-b-game.ico"   },
    { "zero-b-album0.ico", "zero-b-album0.ico", "zero-b-album0.ico" },
    { "zero-b-album1.ico", "zero-b-album1.ico", "zero-b-album1.ico" },
    { "zero-b-album2.ico", "zero-b-album2.ico", "zero-b-album2.ico" },
    { "zero-b-album3.ico", "zero-b-album3.ico", "zero-b-album3.ico" },
    { "zero-b-album4.ico", "zero-b-album4.ico", "zero-b-album4.ico" },
};

// The icon model's file number per directory and icon type. Every album
// shows the album-0 model.
static const int icon_data_label[6][3] =                    /* rodata 0x35c0c0 */
{
    { ZERO3GAME_ICO,   ZERO3GAME_ICO,   ZERO3GAME_ICO   },
    { ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO },
    { ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO },
    { ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO },
    { ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO },
    { ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO, ZERO3ALBUM0_ICO },
};

// The five save slots' heads: the only block of "BESLES-53825ZERO-BD".
static MC_PLAY_DATA_HEAD mc_play_data_head[5];              /* bss 0x3f5df0 */

static void SetMemoryCardSaveDataInfo(MC_SAVE_DATA *buff, int dir_label, int file_label);

// ══════════════════════════════════════════════════════════════════════
// Soft-reset lock (MemoryCardExeInit / MemoryCardEnd)
// ══════════════════════════════════════════════════════════════════════

static SYS_WRK *mc_sys_wrk;                 // sdata 0x372890 (gp-0xc60): GetSysWrk, fetched once

// FF2 called SoftResetLock (main.c); FF3 sets the flag itself.
// FF3 0x00202270; FF2 MemoryCardSoftResetLock 0x001e17c8.
void MemoryCardSoftResetLock(void)
{
    if (mc_sys_wrk == nullptr)
        mc_sys_wrk = GetSysWrk();
    mc_sys_wrk->sreset_ng = 1;
}

// FF3 0x002022a8; FF2 MemoryCardSoftResetUnlock 0x001e17e8.
void MemoryCardSoftResetUnlock(void)
{
    if (mc_sys_wrk == nullptr)
        mc_sys_wrk = GetSysWrk();
    mc_sys_wrk->sreset_ng = 0;
}

// ══════════════════════════════════════════════════════════════════════
// Paths
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// "/<dir>/<file>" into a 55-byte path. An unknown dir_label leaves "/<dir>/".
// FF3 0x002022d8; from MioPan mc_set_data.c:164 (no assert).

void MemoryCardSetFilePath(char *path_name, int dir_label, int file_label)
{
    char fname[55]    = "/";
    char dir_name[21] = "";

    memset(path_name, 0, 55);

    MemoryCardSetDirName(dir_name, dir_label);

    strcat(fname, dir_name);
    strcat(fname, "/");

    switch (dir_label)
    {
    case 0:
        strcat(fname, game_file_name[file_label]);
        break;
    case 1:
        strcat(fname, album1_file_name[file_label]);
        break;
    case 2:
        strcat(fname, album2_file_name[file_label]);
        break;
    case 3:
        strcat(fname, album3_file_name[file_label]);
        break;
    case 4:
        strcat(fname, album4_file_name[file_label]);
        break;
    case 5:
        strcat(fname, album5_file_name[file_label]);
        break;
    default:
        break;
    }

    strcpy(path_name, fname);
}

// ══════════════════════════════════════════════════════════════════════
// Save data
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Gather every block of one file into addr0 and put the checksum of all
// but the last four bytes behind them.
// FF3 0x00202438; from MioPan mc_set_data.c:221. FF3 stops at a null address
// or a -1 size (FF2: both); no pointer fixups yet (see MioPan's PORT note:
// blocks holding live pointers need them once those owners exist).

void SetMemoryCardSaveDataToBuff(void *addr, int dir_label, int file_label)
{
    char *addr0 = (char *)addr;
    MC_SAVE_DATA save_data_buff[100];
    int size;
    int i;

    size = GetMemoryCardDataSize(dir_label, file_label);

    SetMemoryCardSaveDataInfo(save_data_buff, dir_label, file_label);

    for (i = 0; i < 100; i++)
    {
        char *addr1 = (char *)save_data_buff[i].addr;
        int j;

        if (addr1 == nullptr || save_data_buff[i].size == -1)
        {
            break;
        }

        for (j = 0; j < save_data_buff[i].size; j++)
        {
            *addr0++ = *addr1++;
        }
    }

    SetMemoryCardDataCheckSum(addr0, CalcMemoryCardDataCheckSum((char *)addr, size - 4));
}

// ──────────────────────────────────────────────────────────────────────
// Scatter a file read from the card back over its blocks.
// FF3 0x00202518; from MioPan mc_set_data.c:279 (no warning, no probe).

void DevelopMemoryCardLoadData(void *addr, int dir_label, int file_label)
{
    const char *addr0 = (const char *)addr;
    MC_SAVE_DATA save_data_buff[100];
    int i;

    SetMemoryCardSaveDataInfo(save_data_buff, dir_label, file_label);

    for (i = 0; i < 100; i++)
    {
        char *addr1 = (char *)save_data_buff[i].addr;
        int j;

        if (addr1 == nullptr && save_data_buff[i].size == -1)
        {
            return;
        }

        for (j = 0; j < save_data_buff[i].size; j++)
        {
            *addr1++ = *addr0++;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// What a whole directory costs in bytes: icon.sys, each data file, the icon
// model, 5 KB (game) or 2 KB (album) and 2 KB for the directory. A dir_label
// outside 0..5 counts icon.sys and the directory only.
// FF3 0x002025a0; from MioPan mc_set_data.c:350 (albums have two files).

int GetMemoryCardDirSize(int dir_label)
{
    int dir_size = sizeof(sceMcIconSys);
    int i;

    if (dir_label == 0)
    {
        for (i = 0; i < 7; i++)
        {
            dir_size += GetMemoryCardDataSize(0, i);
        }

        dir_size += GetIconDataSize(0, 0) + 1024 * 5;
    }
    else if (dir_label >= 0 && dir_label <= 5)
    {
        for (i = 0; i < 2; i++)
        {
            dir_size += GetMemoryCardDataSize(dir_label, i);
        }

        dir_size += GetIconDataSize(dir_label, 0) + 1024 * 2;
    }

    return dir_size + 1024 * 2;
}

// ──────────────────────────────────────────────────────────────────────
// The same in 1 KB clusters, every term rounded up (MemoryCardCheckEmpty /
// MemoryCardCheckDirBroken compare it with the card).
// FF3 0x00202658; from MioPan mc_set_data.c:383 (albums have two files).

int GetMemoryCardDirSizeCluster(int dir_label)
{
    int dir_size = (sizeof(sceMcIconSys) + 1023) / 1024;
    int i;

    if (dir_label == 0)
    {
        for (i = 0; i < 7; i++)
        {
            dir_size += (GetMemoryCardDataSize(0, i) + 1023) / 1024;
        }

        dir_size += (GetIconDataSize(0, 0) + 1023) / 1024 + 5;
    }
    else if (dir_label >= 0 && dir_label <= 5)
    {
        for (i = 0; i < 2; i++)
        {
            dir_size += (GetMemoryCardDataSize(dir_label, i) + 1023) / 1024;
        }

        dir_size += (GetIconDataSize(dir_label, 0) + 1023) / 1024 + 2;
    }

    return dir_size + 2;
}

// ──────────────────────────────────────────────────────────────────────
// A file's size: its blocks plus the 4-byte checksum.
// FF3 0x00202760; from MioPan mc_set_data.c:423.

int GetMemoryCardDataSize(int dir_label, int file_label)
{
    MC_SAVE_DATA save_data_buff[100];
    int size = 0;
    int i;

    SetMemoryCardSaveDataInfo(save_data_buff, dir_label, file_label);

    for (i = 0; i < 100; i++)
    {
        if (save_data_buff[i].addr == nullptr && save_data_buff[i].size == -1)
        {
            break;
        }

        size += save_data_buff[i].size;
    }

    return size + 4;
}

// ──────────────────────────────────────────────────────────────────────
// Run a file's manifest into buff: one {addr, size} per callback, then
// {nullptr, -1}. FF3 first fills all 100 entries with {nullptr, -1}, so a
// missing manifest also reads as an empty list (FF2 left the stack as it was).
// FF3 0x002027d0; from MioPan mc_set_data.c:452.

static void SetMemoryCardSaveDataInfo(MC_SAVE_DATA *buff, int dir_label, int file_label)
{
    MC_SET_SAVE_FUNC *list;
    char data_end = 0;
    int i;

    for (i = 99; i >= 0; i--)
    {
        buff[i].addr = nullptr;
        buff[i].size = -1;
    }

    for (i = 0; i < 100 && !data_end; i++)
    {
        if (dir_label == 0)
        {
            list = game_save_data[file_label];
        }
        else if (dir_label >= 0 && dir_label < 6)
        {
            list = album_save_data[file_label];
        }
        else
        {
            continue;
        }

        if (list == nullptr)
        {
            data_end = 1;
        }
        else if (list[i] == nullptr)
        {
            buff->addr = nullptr;
            data_end   = 1;
            buff->size = -1;
        }
        else
        {
            buff->size = list[i](&buff->addr);
            buff++;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// Staging buffer and the icon model's load
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202928; FF2 GetDataMemoryArea (mem_utilGetMem), now the system heap.

void *GetDataMemoryArea(int size)
{
    return heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202960; FF2 LiberateDataMemoryArea, which took the pointer itself.

void LiberateDataMemoryArea(void **data_addr)
{
    if (*data_addr != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), *data_addr, 0, 0, 0);
        *data_addr = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002029b0; from MioPan mc_set_data.c:530.

void MemoryCardDataLoadReq(void *addr, int data_label)
{
    LoadReq(data_label, addr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002029d0; from MioPan mc_set_data.c:535.

int MemoryCardDataLoadWait(void)
{
    return IsLoadEndAll() != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002029f0; from MioPan mc_set_data.c:551 (static table, no assert).

int GetMemoryCardDataFileNum(int dir_label)
{
    return data_file_num[dir_label];
}

// ──────────────────────────────────────────────────────────────────────
// Directory entries including "." and "..".
// FF3 0x00202a08; from MioPan mc_set_data.c:563 (static table, no assert).

int GetMemoryCardAllFileNum(int dir_label)
{
    return dir_file_num[dir_label] + 2;
}

// ══════════════════════════════════════════════════════════════════════
// Play-data head
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202a28; from MioPan mc_set_data.c:578.

void MemoryCardPlayDataHeadInit(void)
{
    memset(mc_play_data_head, 0, sizeof(mc_play_data_head));
}

// ──────────────────────────────────────────────────────────────────────
// One slot emptied (after a save to it failed).
// FF3 0x00202a50; FF3 only.

void MemoryCardPlayDataHeadClear(int slot)
{
    memset(&mc_play_data_head[slot], 0, sizeof(mc_play_data_head[slot]));
}

// ──────────────────────────────────────────────────────────────────────
// Snapshot the running game into a slot's head, at save.
// FF3 0x00202a80; FF2 SetMemoryCardPlayDataHead with FF3's fields. FF3 caches
// GetFinderStateWrk() in a shared static (sdata 0x372870); calling the getter
// is the same.

void SetMemoryCardPlayDataHead(int slot)
{
    MC_PLAY_DATA_HEAD *head = &mc_play_data_head[slot];

    head->play_time  = ingame_wrk.play_time;
    head->b04        = (s_char)ingame_wrk.b04;
    head->b02        = (s_char)ingame_wrk.b02;
    head->player     = ingame_wrk.b00;
    head->difficulty = (u_char)IngameWrkGetDifficulty(1);
    head->clear_num  = GetFinderStateWrk()->FUN_001a5e60();
    head->chapter    = ingame_wrk._28;
    head->area       = MapAreaNoGet();
    head->data_flg   = 1;
    head->finder_flg = (s_char)GetFinderStateWrk()->_0c;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202b68; FF2 GetMemoryCardPlayDataPlayTime returned the struct.

PLAY_TIME *GetMemoryCardPlayDataPlayTime(int slot)
{
    return &mc_play_data_head[slot].play_time;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202b88.

int GetMemoryCardPlayDataB04(int slot)
{
    return mc_play_data_head[slot].b04;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202ba8.

int GetMemoryCardPlayDataB02(int slot)
{
    return mc_play_data_head[slot].b02;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202bc8.

int GetMemoryCardPlayDataPlayer(int slot)
{
    return mc_play_data_head[slot].player;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202be8.

int GetMemoryCardPlayDataDifficulty(int slot)
{
    return mc_play_data_head[slot].difficulty;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202c08.

int GetMemoryCardPlayDataClearNum(int slot)
{
    return mc_play_data_head[slot].clear_num;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202c28.

int GetMemoryCardPlayDataChapter(int slot)
{
    return mc_play_data_head[slot].chapter;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202c48.

int GetMemoryCardPlayDataArea(int slot)
{
    return mc_play_data_head[slot].area;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202c68.

int GetMemoryCardPlayDataFinderFlg(int slot)
{
    return mc_play_data_head[slot].finder_flg;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202c88; from MioPan mc_set_data.c:595.

int GetMemoryCardPlayDataFlg(int slot)
{
    return mc_play_data_head[slot].data_flg;
}

// ──────────────────────────────────────────────────────────────────────
// The play-data head file's only block (save_play_data_head).
// FF3 0x00202ca8; FF2 SetSave_PlayDataHead.

int SetSave_PlayDataHead(void **addr)
{
    *addr = mc_play_data_head;
    return sizeof(mc_play_data_head);
}

// ══════════════════════════════════════════════════════════════════════
// Names, icon.sys and the icon model
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202cc0; from MioPan mc_set_data.c:636 (no assert; FF3 tests only
// the upper bound).

void MemoryCardSetDirName(char *dir_name, int dir_label)
{
    memset(dir_name, 0, 21);

    if (dir_label < 6)
    {
        strcpy(dir_name, game_dir_name[dir_label]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// "<dir>/*", sceMcGetDir's pattern.
// FF3 0x00202d30; from MioPan mc_set_data.c:652.

void MemoryCardMakeSearchDirPath(char *path_name, int dir_label)
{
    memset(path_name, 0, 55);
    MemoryCardSetDirName(path_name, dir_label);
    strcat(path_name, "/*");
}

// ──────────────────────────────────────────────────────────────────────
// "/<dir>/icon.sys".
// FF3 0x00202d80; from MioPan mc_set_data.c:659.
// callcheck: memset (the two array initialisers; the path itself is not cleared)

void MemoryCardSetIconSysPath(char *path_name, int dir_label)
{
    char fname[55]    = "/";
    char dir_name[21] = "";

    MemoryCardSetDirName(dir_name, dir_label);

    strcat(fname, dir_name);
    strcat(fname, "/");
    strcat(fname, "icon.sys");

    strcpy(path_name, fname);
}

// ──────────────────────────────────────────────────────────────────────
// The icon.sys the card browser reads.
// FF3 0x00202e30; from MioPan mc_set_data.c:683.

sceMcIconSys MemoryCardSetIconSysData(int dir_label)
{
    sceMcIconSys icon;

    memset(&icon, 0, sizeof(icon));

    // Five bytes into the four of Head: the NUL lands on Reserv1 (zero anyway).
    strcpy((char *)&icon, "PS2D");

    MemoryCardSetIconTitleName((char *)icon.TitleName, dir_label);

    icon.OffsLF    = 28;
    icon.TransRate = 0x60;

    memcpy(icon.BgColor,    mc_bgcolor,  sizeof(mc_bgcolor));
    memcpy(icon.LightDir,   mc_lightdir, sizeof(mc_lightdir));
    memcpy(icon.LightColor, mc_lightcol, sizeof(mc_lightcol));
    memcpy(icon.Ambient,    mc_ambient,  sizeof(mc_ambient));

    MemoryCardSetIconFileName((char *)icon.FnameView, dir_label, 0);
    MemoryCardSetIconFileName((char *)icon.FnameCopy, dir_label, 1);
    MemoryCardSetIconFileName((char *)icon.FnameDel,  dir_label, 2);

    return icon;
}

// ──────────────────────────────────────────────────────────────────────
// The two-row title (68 bytes): the game, then the directory's line.
// FF3 0x00202f18; from MioPan mc_set_data.c:716 (unsigned bound, no assert).

void MemoryCardSetIconTitleName(char *title, int dir_label)
{
    char title_name[68] = "";

    memset(title, 0, 68);

    strcpy(title_name, mc_icon_title);

    if ((u_int)dir_label < 6)
    {
        strcat(title_name, mc_icon_sub_title[dir_label]);
    }

    strcpy(title, title_name);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00202fa8; from MioPan mc_set_data.c:736.

void MemoryCardSetIconFileName(char *icon_name, int dir_label, int icon_type)
{
    memset(icon_name, 0, 31);
    strcpy(icon_name, mc_icon_name[dir_label][icon_type]);
}

// ──────────────────────────────────────────────────────────────────────
// "/<dir>/<icon model>".
// FF3 0x00203018; from MioPan mc_set_data.c:742.

void MemoryCardSetIconFilePath(char *icon_path, int dir_label, int icon_type)
{
    char fname[55]    = "/";
    char dir_name[21] = "";

    memset(icon_path, 0, 55);

    MemoryCardSetDirName(dir_name, dir_label);

    strcat(fname, dir_name);
    strcat(fname, "/");
    strcat(fname, mc_icon_name[dir_label][icon_type]);

    strcpy(icon_path, fname);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00203100; from MioPan mc_set_data.c:761.

int GetIconDataLabel(int dir_label, int icon_type)
{
    return icon_data_label[dir_label][icon_type];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00203128; from MioPan mc_set_data.c:766.

int GetIconDataSize(int dir_label, int icon_type)
{
    return GetFileSize(GetIconDataLabel(dir_label, icon_type));
}
