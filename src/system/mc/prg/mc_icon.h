/* ==========================================================================
 *  system/mc/prg/mc_icon.h
 *
 *  Write a save directory's icon model to the card (FF2 mc_icon.c, FF3
 *  0x2012a0..0x201420): loaded from the disc into a system-heap buffer,
 *  then written as a new file.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_ICON_H
#define _SYSTEM_MC_PRG_MC_ICON_H

void MemoryCardIconInit(int port, int slot, int dir_label, int icon_type);  // FF3 0x002012a0
int  MemoryCardIconMain(void);          // FF3 0x002012c8: 1 written, 0 busy, < 0 error (buffer freed either way)
void LiberateMemoryCardIconDataMem(void);   // FF3 0x00201408: frees the icon buffer (MemoryCardEnd)

#endif /* _SYSTEM_MC_PRG_MC_ICON_H */
