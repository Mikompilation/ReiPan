/* ==========================================================================
 *  system/mc/prg/mc_open.h
 *
 *  sceMcOpen() step machine (FF2 mc_open.c, FF3 0x201d80..0x201f28).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_OPEN_H
#define _SYSTEM_MC_PRG_MC_OPEN_H

// name is kept, not copied: it must outlive the open. mode: sceMcOpen's bits.
void MemoryCardFileOpenInit(int port, int slot, char *name, int mode);  // FF3 0x00201d80
// 1 open (*fd set), 0 busy, < 0 (-2..-5, -7, -10, -0x14); *fd is -1 meanwhile.
int  MemoryCardFileOpenMain(int *fd);                                   // FF3 0x00201da8
int  MemoryCardFileOpenReq(int port, int slot, char *name, int mode);   // FF3 0x00201f10: sceMcOpen

#endif /* _SYSTEM_MC_PRG_MC_OPEN_H */
