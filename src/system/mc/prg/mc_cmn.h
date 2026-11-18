/* ==========================================================================
 *  system/mc/prg/mc_cmn.h
 *
 *  Helpers shared by the memory-card modules (FF2 mc_cmn.c, FF3
 *  0x200ae8..0x200bd0): the non-blocking sync, the free size the PS2
 *  browser shows, and the card file's checksum.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CMN_H
#define _SYSTEM_MC_PRG_MC_CMN_H

// sceMcSync(1): 1 the request finished (*result holds its answer), 0 still
// running, -1 nothing was in flight.
int  MemoryCardExeEndSync(int *result);                         // FF3 0x00200ae8

// Free clusters less the two the browser keeps, never below 0.
int  GetMemoryCardFreeSizeForBrowser(void);                     // FF3 0x00200b08

// Signed byte sum of size bytes (the card file's checksum).
int  CalcMemoryCardDataCheckSum(char *data_addr, int size);     // FF3 0x00200b30
// The sum's four bytes, little-endian, at addr (unaligned).
void SetMemoryCardDataCheckSum(char *addr, int check_sum);      // FF3 0x00200b60
// Reads them back (FF3-only, unreferenced).
int  GetMemoryCardDataCheckSum(char *addr);                     // FF3 0x00200b98

#endif /* _SYSTEM_MC_PRG_MC_CMN_H */
