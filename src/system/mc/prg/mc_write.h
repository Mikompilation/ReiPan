/* ==========================================================================
 *  system/mc/prg/mc_write.h
 *
 *  sceMcWrite wrapper with retries (FF2 mc_write.c, FF3 0x203160..0x203310).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_WRITE_H
#define _SYSTEM_MC_PRG_MC_WRITE_H

void MemoryCardFileWriteInit(int fd, void *data_addr, int size);  // FF3 0x00203160
// 1 written, 0 busy, < 0 error: -2..-5 and -8 as the card reports them, -20
// any other, -10 after four rejected requests (the file is closed first).
int  MemoryCardFileWriteMain(void);                                // FF3 0x00203188
int  MemoryCardFileWriteReq(int fd, void *data_addr, int size);   // FF3 0x002032e8: sceMcWrite

#endif /* _SYSTEM_MC_PRG_MC_WRITE_H */
