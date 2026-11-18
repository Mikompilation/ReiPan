/* ==========================================================================
 *  system/mc/prg/mc_check_broken.c
 *
 *  Directory and file integrity (FF3 0x1fffd0..0x200168). From MioPan
 *  system/mc/prg/mc_check_broken.c (FF2), the same code in FF3 without the
 *  MemoryCardPrint traces.
 * ======================================================================== */
#include "system/mc/prg/mc_check_broken.h"
#include "reipan/save/reipan_save_codec.h"
#include <cstdio>

#include "system/mc/prg/mc_check_dir.h"     // GetMemoryCardCheckDir*
#include "system/mc/prg/mc_cmn.h"           // CalcMemoryCardDataCheckSum
#include "system/mc/prg/mc_set_data.h"      // GetMemoryCardAllFileNum, GetMemoryCardDirSizeCluster

// FF3 0x001fffd0; FF2 MemoryCardCheckDirBroken 0x001df218.
int MemoryCardCheckDirBroken(int dir_label)
{
    int expected = GetMemoryCardDirSizeCluster(dir_label);
    if (dir_label == 0)
    {
        for (int i = 0; i < 5; ++i)
        {
            char name[16];std::snprintf(name, sizeof(name), "Zero3Play%d", i);
            const int size = GetMemoryCardCheckDirEntrySize(name);
            if (size != (int)ReiPanSave::NativeSize() && size != (int)ReiPanSave::Ps2Size)
                return 0;
            expected -= ((int)ReiPanSave::NativeSize() + 1023) / 1024 - (size + 1023) / 1024;
        }
    }
    if (GetMemoryCardAllFileNum(dir_label) == GetMemoryCardCheckDirFileNum()
        && expected == GetMemoryCardCheckDirSize())
    {
        return 1;
    }
    return 0;
}

// FF3 0x00200030; FF2 MemoryCardCheckFileBroken 0x001df280.
int MemoryCardCheckFileBroken(void *data_addr, int size)
{
    int load_check_sum = 0;
    char *check_sum_addr = (char *)&load_check_sum;
    char *addr = (char *)data_addr + size - 4;
    int check_sum = CalcMemoryCardDataCheckSum((char *)data_addr, size - 4);
    int res;
    int i;

    for (i = 3; i >= 0; i--)
        *check_sum_addr++ = *addr++;

    res = (check_sum == load_check_sum);
    // A file MemoryCardAllFileMakeMain created and nothing wrote yet: all
    // zero, -1 in the checksum. Whole, just empty.
    if (check_sum == 0 && load_check_sum == -1)
        res = 1;
    return res;
}

// FF3 0x002000b0; FF2 MemoryCardCheckNewFileLoad 0x001df320.
int MemoryCardCheckNewFileLoad(void *data_addr, int size)
{
    int load_check_sum = 0;
    char *check_sum_addr = (char *)&load_check_sum;
    char *data = (char *)data_addr;
    char *addr;
    int check_sum;
    int res;
    int i;

    size -= 4;
    addr = data + size;
    check_sum = CalcMemoryCardDataCheckSum(data, size);
    for (i = 3; i >= 0; i--)
        *check_sum_addr++ = *addr++;

    res = (check_sum == 0 && load_check_sum == -1);
    for (i = 0; i < size; i++)
    {
        if (data[i] != 0)
            return 0;
    }
    return res;
}
