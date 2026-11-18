/* ==========================================================================
 *  common/cmn_caption.h
 *
 *  Button-guide captions: one line of pad-button icons and command words
 *  ("x Confirm  /\ Return", "L1 R1 Move Page", ...) drawn from the textures
 *  of CAPTION_PK4 (tim2/caption.pk4: page 0, 128x64, the pad icons; page 1,
 *  256x256, the words). 25 caption lines, each a list of (picture, x, y).
 *
 *  FF3-only (no FF2 match), FF3 0x13e6a8..0x13efa0, between
 *  common/cmn_fade_layer.c and the cat control; the name is descriptive.
 *  Nothing in the retail ELF calls these functions (no jal, data word or
 *  address pair for any of them): the game draws its guides through the
 *  CAPTION_PHF layouts instead. Kept whole all the same.
 * ======================================================================== */
#ifndef _COMMON_CMN_CAPTION_H
#define _COMMON_CMN_CAPTION_H

#include "sdk/scetypes.h"

#define CAPTION_LINE_NUM    25      // lines of the caption table (rdata 0x34f318)
#define CAPTION_SPR_NUM     33      // pictures of the sprite table (bss 0x3910c8)

// Allocate CAPTION_PK4's size from the system heap and queue its load
// (wait with IsLoadEndAll).
void CmnCaptionLoadReq(void);                   // FF3 0x0013e7a8
// Send the pack's textures to VRAM (nothing when it is not loaded).
void CmnCaptionSendVram(void);                  // FF3 0x0013e810
// Free the pack.
void CmnCaptionRelease(void);                   // FF3 0x0013e848
// Draw caption line no (-1: none) with the given alpha (0x80 opaque).
void CmnCaptionDisp(int no, u_char alpha);      // FF3 0x0013e6a8

#endif /* _COMMON_CMN_CAPTION_H */
