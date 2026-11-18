/* ==========================================================================
 *  system/mc/prg/mc_cmn.c
 *
 *  Shared memory-card helpers (FF3 0x200ae8..0x200bd0). From MioPan
 *  system/mc/prg/mc_cmn.c (FF2), the same four functions in FF3, plus the
 *  FF3-only GetMemoryCardDataCheckSum (nothing calls it).
 * ======================================================================== */
#include "system/mc/prg/mc_cmn.h"

#include "sdk/libmc.h"
#include "sdk/scetypes.h"
#include "system/mc/prg/mc_check_card.h"    // GetAccessMemoryCardFreeCluster

// FF3 0x00200ae8; FF2 MemoryCardExeEndSync 0x001dfc88.
int MemoryCardExeEndSync(int *result)
{
    return sceMcSync(1, nullptr, result);
}

// FF3 0x00200b08; FF2 GetMemoryCardFreeSizeForBrowser 0x001dfca8.
int GetMemoryCardFreeSizeForBrowser(void)
{
    int size = GetAccessMemoryCardFreeCluster() - 2;

    if (size < 0)
        size = 0;
    return size;
}

// The EE's char is signed: every byte adds -128..127.
// FF3 0x00200b30; FF2 CalcMemoryCardDataCheckSum 0x001dfcd0.
int CalcMemoryCardDataCheckSum(char *data_addr, int size)
{
    int check_sum = 0;
    int i;

    for (i = 0; i < size; i++)
        check_sum += (s_char)data_addr[i];
    return check_sum;
}

// FF3 0x00200b60; FF2 SetMemoryCardDataCheckSum 0x001dfd00.
void SetMemoryCardDataCheckSum(char *addr, int check_sum)
{
    char *check_sum_addr = (char *)&check_sum;
    int i;

    for (i = 3; i >= 0; i--)
        *addr++ = *check_sum_addr++;
}

// FF3 0x00200b98.
int GetMemoryCardDataCheckSum(char *addr)
{
    int check_sum = 0;
    char *check_sum_addr = (char *)&check_sum;
    int i;

    for (i = 3; i >= 0; i--)
        *check_sum_addr++ = *addr++;
    return check_sum;
}
