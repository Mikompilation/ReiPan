/* ==========================================================================
 *  system/mc/prg/mc_icon_sys.h
 *
 *  Write icon.sys into a card directory (FF2 mc_icon_sys.c, FF3
 *  0x201420..0x201558). Minimal so far: what reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_ICON_SYS_H
#define _SYSTEM_MC_PRG_MC_ICON_SYS_H

void MemoryCardIconSysInit(int port, int slot, int dir_label);  // FF3 0x00201420
int  MemoryCardIconSysMain(void);       // FF3 0x002014a8: 1 written, 0 busy, < 0 error

#endif /* _SYSTEM_MC_PRG_MC_ICON_SYS_H */
