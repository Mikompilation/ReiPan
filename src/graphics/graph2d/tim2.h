/* ==========================================================================
 *  graphics/graph2d/tim2.h
 *
 *  TIM2 texture sends (FF3 0x25c368..0x25cc20; FF2 graphics/graph2d/tim2.c,
 *  without FF2's Tim2Load / Tim2Get readers, which are not in FF3's run; plus
 *  PK4SendVram and
 *  Tim2PackSend). Every send builds FF3's BITBLTBUF/TRXPOS/TRXREG/TRXDIR
 *  packet and image transfer; on PC the texels also go straight to the GS
 *  model through ReiPan_GsUpload, since nothing walks the DMA chain.
 *
 *  FF3 changed the argument order from FF2's (tim2, tbp, offset) to
 *  (tim2, offset, tbp): `offset` is added to the TBP/CBP stored in the
 *  picture's TEX0, a `tbp` / `cbp` >= 0 replaces it.
 *
 *  TIM2 data is passed as host pointers (PS2: EE addresses).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH2D_TIM2_H
#define _GRAPHICS_GRAPH2D_TIM2_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// A TIM2 picture header (Sony's TIM2 layout; file data, no pointers).
struct TIM2_PICTUREHEADER               /* 0x30 */
{
    /* 0x00 */ u_int   TotalSize;
    /* 0x04 */ u_int   ClutSize;
    /* 0x08 */ u_int   ImageSize;
    /* 0x0c */ u_short HeaderSize;
    /* 0x0e */ u_short ClutColors;
    /* 0x10 */ u_char  PictFormat;
    /* 0x11 */ u_char  MipMapTextures;
    /* 0x12 */ u_char  ClutType;
    /* 0x13 */ u_char  ImageType;
    /* 0x14 */ u_short ImageWidth;
    /* 0x16 */ u_short ImageHeight;
    /* 0x18 */ u_long  GsTex0;
    /* 0x20 */ u_long  GsTex1;
    /* 0x28 */ u_int   GsTexaFbaPabe;
    /* 0x2c */ u_int   GsTexClut;
};
static_assert(sizeof(TIM2_PICTUREHEADER) == 0x30, "TIM2_PICTUREHEADER is 0x30 bytes");
static_assert(offsetof(TIM2_PICTUREHEADER, ClutSize) == 0x04, "TIM2_PICTUREHEADER.ClutSize");
static_assert(offsetof(TIM2_PICTUREHEADER, ImageSize) == 0x08, "TIM2_PICTUREHEADER.ImageSize");
static_assert(offsetof(TIM2_PICTUREHEADER, HeaderSize) == 0x0c, "TIM2_PICTUREHEADER.HeaderSize");
static_assert(offsetof(TIM2_PICTUREHEADER, ClutColors) == 0x0e, "TIM2_PICTUREHEADER.ClutColors");
static_assert(offsetof(TIM2_PICTUREHEADER, ImageWidth) == 0x14, "TIM2_PICTUREHEADER.ImageWidth");
static_assert(offsetof(TIM2_PICTUREHEADER, ImageHeight) == 0x16, "TIM2_PICTUREHEADER.ImageHeight");
static_assert(offsetof(TIM2_PICTUREHEADER, GsTex0) == 0x18, "TIM2_PICTUREHEADER.GsTex0");

// GS buffer width of a w-texel-wide image of mode psm (64-texel units; PSMT8 /
// PSMT4 rounded up to even), 0 for other modes. Unreferenced. FF3 0x0025c300.
int Tim2CalcBufWidth(int psm, int w);

// The first picture header of a TIM2 file: +0x10 when its alignment byte
// (+5) is 0, +0x80 when 1, else nullptr. FF3 0x0025c368.
TIM2_PICTUREHEADER *Tim2GetFirstPicture(void *tim2);

// TEX0 of the first picture with TBP0 and CBP moved by `offset` (0 when the
// file is bad). PK: of member `no` of a PK2.
u_long GetTex0RegPK(void *pk2, int no, u_int offset);           // FF3 0x0025c390
u_long GetTex0RegTM(void *tim2, u_int offset);                  // FF3 0x0025c3b8

// Image / clut of a TIM2 file's first picture to VRAM.
void MakeTim2Direct(void *tim2, int offset, int tbp);           // FF3 0x0025c440
void MakeClutDirect(void *tim2, int offset, int cbp);           // FF3 0x0025c5d0
// The clut of `mono` sent in place of tim2's (size, CBP and colours from tim2).
void MakeClutDirect_ChrMono(void *tim2, void *mono, int offset, int cbp);  // FF3 0x0025c738

// Every / one TIM2 of a PK2 (count +0, offsets +0x10).
void PK2SendVram(void *pk2, int offset, int tbp, int cbp);              // FF3 0x0025c8c0
void PK2SendVramOne(void *pk2, int no, int offset, int tbp, int cbp);   // FF3 0x0025c960

// Image and clut of one TIM2 file. FF3 0x0025ca00 (FF2's took no tbp/cbp).
void MakeTim2SendPacket_3Dpkt(void *tim2, int offset, int tbp, int cbp);
// Image of tim2, clut of mono. FF3 0x0025ca48.
void MakeTim2SendPacket_Mono(void *tim2, void *mono, int offset, int tbp, int cbp);

// Every / one TIM2 of a PK4 (common/pk4.h).
void PK4SendVram(void *pk4, int offset, int tbp, int cbp);              // FF3 0x0025caa8
void Tim2PackSend(void *pk4, int no, int offset, int tbp, int cbp);     // FF3 0x0025cb60

#endif /* _GRAPHICS_GRAPH2D_TIM2_H */
