/* ==========================================================================
 *  system/mc/prg/mc_check_empty.c
 *
 *  Free-space checks (FF3 0x2008e8..0x200968). From MioPan
 *  system/mc/prg/mc_check_empty.c (FF2); FF3 asks for the directory size
 *  first.
 * ======================================================================== */
#include "system/mc/prg/mc_check_empty.h"

#include "system/mc/prg/mc_check_card.h"    // GetAccessMemoryCardFreeCluster
#include "system/mc/prg/mc_check_dir.h"     // GetMemoryCardCheckDirSize
#include "system/mc/prg/mc_set_data.h"      // GetMemoryCardDirSizeCluster

// FF3 0x002008e8; FF2 MemoryCardCheckEmpty 0x001dfaa0.
int MemoryCardCheckEmpty(int dir_label)
{
    int need = GetMemoryCardDirSizeCluster(dir_label);

    return !(GetAccessMemoryCardFreeCluster() < need);
}

// FF3 0x00200918; FF2 MemoryCardCheckEmptyBroken 0x001dfad8.
int MemoryCardCheckEmptyBroken(int dir_label)
{
    int empty_size = GetMemoryCardCheckDirSize();

    empty_size += GetAccessMemoryCardFreeCluster();
    return !(empty_size < GetMemoryCardDirSizeCluster(dir_label));
}
