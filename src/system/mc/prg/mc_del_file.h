/* ==========================================================================
 *  system/mc/prg/mc_del_file.h
 *
 *  sceMcDelete wrapper (FF2 mc_del_file.c, FF3 0x200f98..0x201148): deletes
 *  a file or, once it is empty, a directory. Minimal so far: what
 *  reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_DEL_FILE_H
#define _SYSTEM_MC_PRG_MC_DEL_FILE_H

void MemoryCardFileDelInit(int port, int slot, char *name);  // FF3 0x00200f98
int  MemoryCardFileDelMain(void);       // FF3 0x00200ff8: 1 deleted, 0 busy, <0 error
int  MemoryCardFileDelReq(int port, int slot, char *name);   // FF3 0x00201130

#endif /* _SYSTEM_MC_PRG_MC_DEL_FILE_H */
