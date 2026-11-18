/* ==========================================================================
 *  system/mc/prg/mc_check_dir.h
 *
 *  sceMcGetDir() wrapper and the listing it fills (FF2 mc_check_dir.c, FF3
 *  0x200460..0x2008e8). The listing holds up to 18 entries; like the card's
 *  own, it starts with "." and "..", so the accessors that walk files start
 *  at entry 2.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CHECK_DIR_H
#define _SYSTEM_MC_PRG_MC_CHECK_DIR_H

void MemoryCardDirInfoCtrlInit(void);                           // FF3 0x00200460: clears the listing

// List name (a search path, copied) on (port, slot): Init, then Main every
// frame until 1 (listed) or < 0 (-2, -4, -5, -6, -10, -0x14).
void MemoryCardGetDirInfoInit(int port, int slot, char *name);  // FF3 0x00200488
int  MemoryCardGetDirInfoMain(void);                            // FF3 0x002004e8
int  MemoryCardGetDirInfoReq(int port, int slot, char *name);   // FF3 0x00200628: sceMcGetDir

int  GetMemoryCardCheckDirFileNum(void);                        // FF3 0x00200650: entries, "." ".." included
void GetMemoryCardCheckDirEntryName(char *name, int data_pos);  // FF3 0x00200660: 55-byte name, "" past the end
int  GetMemoryCardCheckDirSize(void);                           // FF3 0x002006d0: clusters the directory takes

// FF3-only lookups by entry name (nothing calls them).
int  GetMemoryCardCheckDirEntrySize(char *name);                // FF3 0x00200780: FileSizeByte, 0 if absent
int  GetMemoryCardCheckDirEntryClosed(char *name);              // FF3 0x00200830: AttrFile & sceMcFileAttrClosed

#endif /* _SYSTEM_MC_PRG_MC_CHECK_DIR_H */
