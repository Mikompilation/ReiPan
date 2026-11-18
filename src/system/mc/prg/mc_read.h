/* ==========================================================================
 *  system/mc/prg/mc_read.h
 *
 *  sceMcRead() step machine (FF2 mc_read.c, FF3 0x201f28..0x2020e0).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_READ_H
#define _SYSTEM_MC_PRG_MC_READ_H

void MemoryCardFileReadInit(int fd, void *data_addr, int size);     // FF3 0x00201f28
// 1 all size bytes read, 0 busy, < 0 (-2..-5, -0x14). A short read (-3) or
// a request rejected four times (1) closes the file first.
int  MemoryCardFileReadMain(void);                                  // FF3 0x00201f50
int  MemoryCardFileReadReq(int fd, void *data_addr, int size);      // FF3 0x002020c8: sceMcRead

#endif /* _SYSTEM_MC_PRG_MC_READ_H */
