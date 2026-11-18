/* ==========================================================================
 *  system/mc/prg/mc_make.h
 *
 *  Make a whole save directory on the card (FF2 mc_make.c, FF3
 *  0x2016e8..0x201828): mkdir (mc_make_dir.c), then every file in it
 *  (mc_make_all_file.c). Used by the game-data and album save screens.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_MAKE_H
#define _SYSTEM_MC_PRG_MC_MAKE_H

// dir_label: 0 game data, 1..5 albums (MemoryCardSetDirName).
void MemoryCardNewMakeInit(int port, int slot, int dir_label, void *buff_addr, int buff_size);  // FF3 0x002016e8
int  MemoryCardNewMakeMain(void);       // FF3 0x00201710: 1 made, 0 busy, < 0 error

#endif /* _SYSTEM_MC_PRG_MC_MAKE_H */
