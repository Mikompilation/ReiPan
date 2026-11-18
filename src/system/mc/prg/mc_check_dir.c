/* ==========================================================================
 *  system/mc/prg/mc_check_dir.c
 *
 *  Directory listing (FF3 0x200460..0x2008e8). From MioPan
 *  system/mc/prg/mc_check_dir.c (FF2), checked against FF3 function by
 *  function: the same code without MemoryCardAssert, plus two FF3-only
 *  lookups by entry name at the end that nothing calls.
 * ======================================================================== */
#include "system/mc/prg/mc_check_dir.h"

#include <stddef.h>
#include <string.h>

#include "sdk/libmc.h"
#include "system/mc/prg/mc_cmn.h"           // MemoryCardExeEndSync

#define MC_DIR_ENTRY_MAX 18

struct MC_CHECK_DIR_CTRL                    /* 0x44, bss 0x3f52c0 */
{
    /* 0x00 */ char step;                   // 0 request, 1 wait, 2 drain a busy card
    /* 0x01 */ char retry_cnt;
    /* 0x04 */ int  port;
    /* 0x08 */ int  slot;
    /* 0x0c */ char name[55];
};
static_assert(sizeof(MC_CHECK_DIR_CTRL) == 0x44, "MC_CHECK_DIR_CTRL is 0x44 bytes");
static_assert(offsetof(MC_CHECK_DIR_CTRL, name) == 0x0c, "MC_CHECK_DIR_CTRL.name");

struct MC_DIR_INFO                          /* 0x4c0, bss 0x3f5340 */
{
    /* 0x000 */ sceMcTblGetDir table[MC_DIR_ENTRY_MAX];
    /* 0x480 */ short          get_filenum;
};
static_assert(sizeof(sceMcTblGetDir) == 0x40, "sceMcTblGetDir is 0x40 bytes");
static_assert(sizeof(MC_DIR_INFO) == 0x4c0, "MC_DIR_INFO is 0x4c0 bytes");
static_assert(offsetof(MC_DIR_INFO, get_filenum) == 0x480, "MC_DIR_INFO.get_filenum");

static MC_CHECK_DIR_CTRL mc_check_dir_ctrl;     // bss 0x3f52c0
static MC_DIR_INFO       mc_dir_info;           // bss 0x3f5340

// FF3 0x00200460; FF2 MemoryCardDirInfoCtrlInit 0x001df778.
void MemoryCardDirInfoCtrlInit(void)
{
    memset(&mc_dir_info, 0, sizeof(mc_dir_info));
}

// FF3 0x00200488; FF2 MemoryCardGetDirInfoInit 0x001df7a0.
void MemoryCardGetDirInfoInit(int port, int slot, char *name)
{
    mc_check_dir_ctrl.step = 0;
    mc_check_dir_ctrl.retry_cnt = 4;
    mc_check_dir_ctrl.port = port;
    mc_check_dir_ctrl.slot = slot;
    memset(mc_check_dir_ctrl.name, 0, sizeof(mc_check_dir_ctrl.name));
    strcpy(mc_check_dir_ctrl.name, name);
}

// On success the result is the entry count.
// FF3 0x002004e8; FF2 MemoryCardGetDirInfoMain 0x001df808.
int MemoryCardGetDirInfoMain(void)
{
    int result = 0;
    int res = 0;
    int mc_res;

    if (mc_check_dir_ctrl.step == 0)
    {
        mc_res = MemoryCardGetDirInfoReq(mc_check_dir_ctrl.port, mc_check_dir_ctrl.slot,
                                         mc_check_dir_ctrl.name);
        if (mc_res == 0)
        {
            mc_check_dir_ctrl.step = 1;
        }
        else if (mc_check_dir_ctrl.retry_cnt < 1)
        {
            res = -10;
        }
        else if (mc_res != sceMcErrNotInit && mc_res != sceMcErrEntryName)
        {
            if (mc_res == sceMcErrBusy)
                mc_check_dir_ctrl.step = 2;
            else
                mc_check_dir_ctrl.retry_cnt--;
        }
    }

    if (mc_check_dir_ctrl.step == 1)
    {
        mc_res = MemoryCardExeEndSync(&result);
        if (mc_res == 1)
        {
            if (result >= 0)
            {
                mc_dir_info.get_filenum = (short)result;
                res = 1;
            }
            else
            {
                switch (result)
                {
                case sceMcResNoFormat:
                case sceMcResNoEntry:
                case sceMcResDeniedPermit:
                case sceMcResNotEmpty:
                    res = result;
                    break;
                default:
                    res = -0x14;
                    break;
                }
            }
        }
        else if (mc_res == -1)
        {
            mc_check_dir_ctrl.step = 0;
        }
    }

    if (mc_check_dir_ctrl.step == 2 && MemoryCardExeEndSync(&result) != 0)
        mc_check_dir_ctrl.step = 0;

    return res;
}

// FF3 0x00200628; FF2 MemoryCardGetDirInfoReq 0x001df990.
int MemoryCardGetDirInfoReq(int port, int slot, char *name)
{
    return sceMcGetDir(port, slot, name, 0, MC_DIR_ENTRY_MAX, mc_dir_info.table);
}

// FF3 0x00200650; FF2 GetMemoryCardCheckDirFileNum 0x001df9b8.
int GetMemoryCardCheckDirFileNum(void)
{
    return mc_dir_info.get_filenum;
}

// FF2 asserted on a position past the end; FF3 leaves the name empty.
// FF3 0x00200660; FF2 GetMemoryCardCheckDirEntryName 0x001df9c8.
void GetMemoryCardCheckDirEntryName(char *name, int data_pos)
{
    memset(name, 0, 55);
    if (data_pos < GetMemoryCardCheckDirFileNum())
        strcpy(name, (char *)mc_dir_info.table[data_pos].EntryName);
}

// The files' clusters (1 KB each, rounded up), one cluster per two entries,
// and two for the directory itself.
// FF3 0x002006d0; FF2 GetMemoryCardCheckDirSize 0x001dfa38.
int GetMemoryCardCheckDirSize(void)
{
    int size = 0;
    int i;

    for (i = 2; i < mc_dir_info.get_filenum; i++)
        size += (mc_dir_info.table[i].FileSizeByte + 1023) >> 10;
    size += (mc_dir_info.get_filenum - 1) / 2;
    return size + 2;
}

// FileSizeByte of the listing entry (from entry 2) whose name matches.
// FF3 0x00200780.
int GetMemoryCardCheckDirEntrySize(char *name)
{
    sceMcTblGetDir *entry = nullptr;
    int i;

    if (name != nullptr)
    {
        for (i = 2; i < mc_dir_info.get_filenum; i++)
        {
            if (strncmp(name, (char *)mc_dir_info.table[i].EntryName, 31) == 0)
            {
                entry = &mc_dir_info.table[i];
                break;
            }
        }
    }
    return entry != nullptr ? (int)entry->FileSizeByte : 0;
}

// The same lookup; the entry's "closed" attribute bit.
// FF3 0x00200830.
int GetMemoryCardCheckDirEntryClosed(char *name)
{
    sceMcTblGetDir *entry = nullptr;
    int i;

    if (name != nullptr)
    {
        for (i = 2; i < mc_dir_info.get_filenum; i++)
        {
            if (strncmp(name, (char *)mc_dir_info.table[i].EntryName, 31) == 0)
            {
                entry = &mc_dir_info.table[i];
                break;
            }
        }
    }
    return entry != nullptr ? (entry->AttrFile & sceMcFileAttrClosed) : 0;
}
