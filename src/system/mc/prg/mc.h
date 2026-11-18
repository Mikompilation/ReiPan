/* ==========================================================================
 *  system/mc/prg/mc.h
 *
 *  Memory-card subsystem brackets (FF2 mc.c, FF3 0x1ffe18..0x1ffe88).
 *
 *  MemoryCardInit() runs once at boot (InitSystem); MemoryCardExeInit() /
 *  MemoryCardEnd() bracket a screen's use of the card.
 *
 *  ---- the folder's return-code convention -------------------------------
 *
 *  Every Main() in system/mc answers the same way:
 *
 *      1        finished, successfully
 *      0        still working
 *     -1        the card was swapped (only from the card query)
 *     -2        no format
 *     -3        card full / short read
 *     -4        no such entry
 *     -5        access denied
 *     -6        directory not empty
 *     -7        out of file handles
 *     -8        replace failed
 *    -10        the request was rejected four times running -- give up
 *    -0x14      anything else the card reported (also: not a PS2 card)
 *
 *  -1 .. -8 are libmc's own result codes passed through unchanged; -10 and
 *  -0x14 are the folder's.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_H
#define _SYSTEM_MC_PRG_MC_H

bool MemoryCardIsInUse(void); // PC: do not transfer folders while a card screen owns them.

void MemoryCardInit(void);              // FF3 0x001ffe18: sceMcInit
void MemoryCardExeInit(void);           // FF3 0x001ffe30: takes the card for a screen
void MemoryCardEnd(void);               // FF3 0x001ffe58: gives it back

#endif /* _SYSTEM_MC_PRG_MC_H */
