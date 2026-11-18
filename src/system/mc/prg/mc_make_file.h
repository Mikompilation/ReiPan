/* ==========================================================================
 *  system/mc/prg/mc_make_file.h
 *
 *  Create a card file and write it (FF2 mc_make_file.c, FF3
 *  0x201bf0..0x201d80). Minimal so far: what reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_MAKE_FILE_H
#define _SYSTEM_MC_PRG_MC_MAKE_FILE_H

// name (a full path, copied) <- size bytes at data_addr.
void MemoryCardMakeNewFileInit(int port, int slot, char *name, void *data_addr, int size);  // FF3 0x00201bf0
int  MemoryCardMakeNewFileMain(void);   // FF3 0x00201c58: 1 written, 0 busy, < 0 error

#endif /* _SYSTEM_MC_PRG_MC_MAKE_FILE_H */
