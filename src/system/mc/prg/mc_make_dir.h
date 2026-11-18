/* ==========================================================================
 *  system/mc/prg/mc_make_dir.h
 *
 *  sceMcMkdir wrapper (FF2 mc_make_dir.c, FF3 0x201a58..0x201bf0): make one
 *  save directory, with up to 4 retries. Called by mc_make.c.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_MAKE_DIR_H
#define _SYSTEM_MC_PRG_MC_MAKE_DIR_H

// name: a bare directory name from MemoryCardSetDirName, no slashes.
void MemoryCardMakeNewDirInit(int port, int slot, char *name);  // FF3 0x00201a58
int  MemoryCardMakeNewDirMain(void);    // FF3 0x00201ab8: 1 made, 0 busy, -10 out of retries, -20 mkdir failed
int  MemoryCardMakeNewDirReq(int port, int slot, char *name);   // FF3 0x00201bd8: sceMcMkdir result

#endif /* _SYSTEM_MC_PRG_MC_MAKE_DIR_H */
