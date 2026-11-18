/* ==========================================================================
 *  system/mc/prg/mc_format.h
 *
 *  Card format (FF2 mc_format.c, FF3 0x201148..0x20129c).
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_FORMAT_H
#define _SYSTEM_MC_PRG_MC_FORMAT_H

void MemoryCardFormatInit(int port, int slot);      // FF3 0x00201148
int  MemoryCardFormatMain(void);                    // FF3 0x00201168: 1 done, 0 busy, -5 denied, -10/-20 error
int  MemoryCardFormatReq(int port, int slot);       // FF3 0x00201288: sceMcFormat

#endif /* _SYSTEM_MC_PRG_MC_FORMAT_H */
