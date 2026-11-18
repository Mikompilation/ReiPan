/* ==========================================================================
 *  system/mc/prg/mc_del_all_file.h
 *
 *  Empty a card directory (FF2 mc_del_all_file.c, FF3 0x200bd0..0x200e68):
 *  lists it and deletes every entry from index 2 on. Minimal so far: what
 *  reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_DEL_ALL_FILE_H
#define _SYSTEM_MC_PRG_MC_DEL_ALL_FILE_H

// dir_label: 0 game data, 1..5 albums (MemoryCardSetDirName).
void MemoryCardAllFileDelInit(int port, int slot, int dir_label);   // FF3 0x00200bd0
int  MemoryCardAllFileDelMain(void);    // FF3 0x00200bf8: 1 emptied, 0 busy, <0 error

#endif /* _SYSTEM_MC_PRG_MC_DEL_ALL_FILE_H */
