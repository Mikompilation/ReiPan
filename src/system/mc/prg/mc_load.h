/* ==========================================================================
 *  system/mc/prg/mc_load.h
 *
 *  Open, read and close one card file (FF2 mc_load.c, FF3
 *  0x201558..0x2016e8).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_LOAD_H
#define _SYSTEM_MC_PRG_MC_LOAD_H

// name (a full path, copied) -> size bytes at data_addr.
void MemoryCardFileLoadInit(int port, int slot, char *name, void *data_addr, int size);  // FF3 0x00201558
int  MemoryCardFileLoadMain(void);      // FF3 0x002015c0: 1 done, 0 busy, < 0 the failing step's error

#endif /* _SYSTEM_MC_PRG_MC_LOAD_H */
