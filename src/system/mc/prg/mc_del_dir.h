/* ==========================================================================
 *  system/mc/prg/mc_del_dir.h
 *
 *  Delete a whole save directory (FF2 mc_del_dir.c, FF3 0x200e68..0x200f98):
 *  empty it with mc_del_all_file.c, then delete the directory entry itself
 *  with mc_del_file.c. Called by album_save.c and game_data_save.c when a
 *  foreign or damaged save directory is replaced.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_DEL_DIR_H
#define _SYSTEM_MC_PRG_MC_DEL_DIR_H

// dir_label: 0 game data, 1..5 albums (MemoryCardSetDirName).
void MemoryCardDirDelInit(int port, int slot, int dir_label);   // FF3 0x00200e68
int  MemoryCardDirDelMain(void);        // FF3 0x00200e88: 1 deleted, 0 busy, <0 error from a step

#endif /* _SYSTEM_MC_PRG_MC_DEL_DIR_H */
