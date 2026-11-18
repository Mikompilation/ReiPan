/* ==========================================================================
 *  system/mc/prg/mc_check_card.h
 *
 *  sceMcGetInfo() wrapper and the "is a usable PS2 card still in the slot"
 *  watch (FF2 mc_check_card.c, FF3 0x200168..0x200460). The card's facts --
 *  port, slot, type, free clusters, format flag -- are cached here and read
 *  back through the accessors below.
 * ======================================================================== */
#ifndef _SYSTEM_MC_PRG_MC_CHECK_CARD_H
#define _SYSTEM_MC_PRG_MC_CHECK_CARD_H

// Forget the card: port and slot -1 (MemoryCardExeInit).
void MemoryCardInfoCtrlInit(void);                      // FF3 0x00200168

// Card info of (port, slot): Init, then Main every frame until it returns 1
// (a formatted PS2 card) or < 0: -1 swapped, -2 unformatted (only on the
// second look: a card being inserted reads unformatted for a moment), -5,
// -10 rejected four times, -0x14 not a PS2 card / other.
void MemoryCardGetCardInfoInit(int port, int slot);     // FF3 0x002001b0
int  MemoryCardGetCardInfoMain(void);                   // FF3 0x002001f8
int  MemoryCardGetCardInfoReq(int port, int slot);      // FF3 0x00200378: sceMcGetInfo

// The same query, re-armed automatically, for a page that polls the card
// every frame: 0 nothing new, 1 the query finished (re-armed), < 0 as above.
void MemoryCardCheckEveryFrameInit(int port, int slot); // FF3 0x002001d8
int  MemoryCardCheckEveryFrameMain(void);               // FF3 0x002003a8

// Cached facts. Port and slot answer -1 unless 0 or 1.
int  GetAccessMemoryCardPort(void);                     // FF3 0x002003f0
int  GetAccessMemoryCardSlot(void);                     // FF3 0x00200408
int  GetAccessMemoryCardType(void);                     // FF3 0x00200420: 2 = PS2 card
int  GetAccessMemoryCardFreeCluster(void);              // FF3 0x00200430
int  GetAccessMemoryCardFormat(void);                   // FF3 0x00200440: 1 = formatted
void MemoryCardSetAccessPort(int port);                 // FF3 0x00200450: without a query

#endif /* _SYSTEM_MC_PRG_MC_CHECK_CARD_H */
