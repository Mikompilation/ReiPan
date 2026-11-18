/* ==========================================================================
 *  system/mc/prg/mc_check.h
 *
 *  Card query followed by a directory listing (FF2 mc_check.c, FF3
 *  0x1ffe88..0x1fffd0): the probe the boot auto load and the load / save
 *  screens start with.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CHECK_H
#define _SYSTEM_MC_PRG_MC_CHECK_H

// name: a search path ("/<dir>/*", MemoryCardMakeSearchDirPath), copied.
void MemoryCardCheckInit(int port, int slot, char *name);     // FF3 0x001ffe88
// 1 listed (GetMemoryCardCheckDir* read it), 0 busy, < 0 the card query's or
// the listing's error (-2 unformatted, -4 no directory, -6 ..., -0x14).
int  MemoryCardCheckMain(void);                               // FF3 0x001ffee0

#endif /* _SYSTEM_MC_PRG_MC_CHECK_H */
