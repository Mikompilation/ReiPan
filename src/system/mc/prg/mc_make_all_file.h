/* ==========================================================================
 *  system/mc/prg/mc_make_all_file.h
 *
 *  Fill a fresh card directory: every data file, the icon, icon.sys (FF2
 *  mc_make_all_file.c, FF3 0x201828..0x201a58). Minimal so far: what
 *  reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_MAKE_ALL_FILE_H
#define _SYSTEM_MC_PRG_MC_MAKE_ALL_FILE_H

// buff_addr: a staging buffer at least as large as the biggest file of the directory.
void MemoryCardAllFileMakeInit(int port, int slot, int dir_label, void *buff_addr, int buff_size);  // FF3 0x00201828
int  MemoryCardAllFileMakeMain(void);   // FF3 0x00201890: 1 all made, 0 busy, < 0 error

#endif /* _SYSTEM_MC_PRG_MC_MAKE_ALL_FILE_H */
