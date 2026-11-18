/* ==========================================================================
 *  system/mc/prg/mc_check_empty.h
 *
 *  Is there room on the card? (FF2 mc_check_empty.c, FF3 0x2008e8..0x200968)
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CHECK_EMPTY_H
#define _SYSTEM_MC_PRG_MC_CHECK_EMPTY_H

// 1: the free clusters hold a whole dir_label directory.
int MemoryCardCheckEmpty(int dir_label);                    // FF3 0x002008e8
// 1: they do once the listed (damaged) directory is deleted.
int MemoryCardCheckEmptyBroken(int dir_label);              // FF3 0x00200918

#endif /* _SYSTEM_MC_PRG_MC_CHECK_EMPTY_H */
