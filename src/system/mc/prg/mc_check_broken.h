/* ==========================================================================
 *  system/mc/prg/mc_check_broken.h
 *
 *  Is what is on the card whole? (FF2 mc_check_broken.c, FF3
 *  0x1fffd0..0x200168). Despite the names, each answers 1 when the data is
 *  *not* broken.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CHECK_BROKEN_H
#define _SYSTEM_MC_PRG_MC_CHECK_BROKEN_H

// 1: the listed directory (MemoryCardCheckMain) has the file count and the
// cluster size a dir_label directory should have.
int MemoryCardCheckDirBroken(int dir_label);                // FF3 0x001fffd0
// 1: the loaded file's checksum (its last 4 bytes) matches, or the file is a
// fresh one (sum 0, checksum -1).
int MemoryCardCheckFileBroken(void *data_addr, int size);   // FF3 0x00200030
// 1: the loaded file is a fresh one (all zero, checksum -1): nothing to load.
int MemoryCardCheckNewFileLoad(void *data_addr, int size);  // FF3 0x002000b0

#endif /* _SYSTEM_MC_PRG_MC_CHECK_BROKEN_H */
