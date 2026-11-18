/* ==========================================================================
 *  system/mc/prg/mc_close.h
 *
 *  sceMcClose() step machine (FF2 mc_close.c, FF3 0x200968..0x200ae8).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CLOSE_H
#define _SYSTEM_MC_PRG_MC_CLOSE_H

void MemoryCardFileCloseInit(int fd);                   // FF3 0x00200968
int  MemoryCardFileCloseMain(void);                     // FF3 0x00200988: 1 closed, 0 busy, < 0 (-2, -4, -5, -10, -0x14)
int  MemoryCardFileCloseReq(int fd);                    // FF3 0x00200ad0: sceMcClose

#endif /* _SYSTEM_MC_PRG_MC_CLOSE_H */
