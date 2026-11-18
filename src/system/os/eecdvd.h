/* ==========================================================================
 *  system/os/eecdvd.h
 *
 *  The game's file-load front end (FF2 system/os/eecdvd.c): LoadReq and
 *  LoadReqGetAddr queue a load of a file number (enum CD_FILE_DAT,
 *  system/eeiop/cd_file_dat.h) to an address; IsLoadEnd / IsLoadEndAll poll
 *  it. FF3 0x259680..0x2597b0. FF3 kept only these four (FF2's language
 *  wrappers and LoadReqSe are gone), loads at priority 5 (FF2: 6) and also
 *  rejects a file with no table entry (size -1).
 * ======================================================================== */
#ifndef _SYSTEM_OS_EECDVD_H
#define _SYSTEM_OS_EECDVD_H

int   LoadReq(int file, void *addr);                    // load id; -1: no entry or empty file
void *LoadReqGetAddr(int file, void *addr, int *id);    // addr + size rounded up to 64; *id = load id or -1
int   IsLoadEndAll(void);                               // 1 when no load is pending
int   IsLoadEnd(int id);                                // 1 when that load finished

#endif /* _SYSTEM_OS_EECDVD_H */
