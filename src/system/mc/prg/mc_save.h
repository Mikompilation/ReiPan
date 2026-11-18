/* ==========================================================================
 *  system/mc/prg/mc_save.h
 *
 *  Overwrite an existing card file: open, write, close (FF2 mc_save.c,
 *  FF3 0x2020e0..0x20226c).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_SAVE_H
#define _SYSTEM_MC_PRG_MC_SAVE_H

void MemoryCardFileSaveInit(int port, int slot, char *name, void *data_addr, int size);  // FF3 0x002020e0
// 1 written and closed, 0 busy, < 0 the failing step's error.
int  MemoryCardFileSaveMain(void);                                                       // FF3 0x00202148

#endif /* _SYSTEM_MC_PRG_MC_SAVE_H */
