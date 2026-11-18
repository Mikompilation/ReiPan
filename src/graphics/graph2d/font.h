/* ==========================================================================
 *  graphics/graph2d/font.h
 *
 *  FF3's font textures: two PK4 packs loaded once during the boot to fixed
 *  EE addresses and sent to VRAM again before anything draws text.
 *  FONT_TEX_PK4 is the message font (7 TIM2 pages of 512x256: Latin with
 *  accents, digits, pad button symbols, punctuation, some kanji);
 *  FONT_EX_TEX_PK4 holds 3 more pages of kanji, sent by the message windows
 *  that use them. FF2 has no such file.
 *
 *  FF3 0x1c7c38..0x1c7cb8 (between fod.o and foot_se.o in the alphabetical
 *  link order, hence the name).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH2D_FONT_H
#define _GRAPHICS_GRAPH2D_FONT_H

// EE addresses the two packs load to.
#define FONT_TEX_ADRS       0x104d000   // FONT_TEX_PK4, 0x703b0 bytes decoded
#define FONT_EX_TEX_ADRS    0x10be000   // FONT_EX_TEX_PK4, 0x301a0 bytes decoded

// Queue the loads of both packs (wait with IsLoadEndAll).
void FontTexLoadReq(void);              // FF3 0x001c7c38
// Send the font pack's textures to VRAM.
void FontTexSendVram(void);             // FF3 0x001c7c68
// Send the extra font pack's textures to VRAM.
void FontExTexSendVram(void);           // FF3 0x001c7c90

#endif
