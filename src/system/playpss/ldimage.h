/* ==========================================================================
 *  system/playpss/ldimage.h
 *
 *  The GS side of the movie player (FF2 playpss.a(ldimage.o), FF3
 *  0x273568..0x273d40): DMA chains that walk the IPU's RGB32 macroblocks into
 *  GS memory. FF2's two exports, plus FF3's setLoadTexTags: the same picture
 *  as a chain for the VIF1 packet buffer (the map sky / title backdrop movie).
 * ======================================================================== */
#ifndef _SYSTEM_PLAYPSS_LDIMAGE_H
#define _SYSTEM_PLAYPSS_LDIMAGE_H

#include "sdk/libmpeg.h"                // sceIpuRGB32

// Build the PATH3 chain for a width x height picture at `image`, landing at
// (x, y) of the GS block vram_adrs (buffer width 640). Returns one past the
// END tag, which playPssSetPacket hands back so a caller can splice it.
// PC: also arms the direct picture path at vram_adrs (reipan_video.h).
u_int *setLoadImageTags(u_int *tags, sceIpuRGB32 *image, int x, int y,
                        int width, int height, int vram_adrs);      // FF3 0x00273568

// Send a chain setLoadImageTags built down PATH3.
void   loadImage(u_int *tags);                                       // FF3 0x002739f8

// FF3-only: the same picture as a chain for the VIF1 packet buffer (every DMA
// tag carries VIF NOP + DIRECT, no END tag), landing at (x, y) of the GS block
// tbp with a buffer width of the picture's own width rounded up to 64.
// Returns the chain's size in bytes; the caller copies it into its packet.
// PC: also arms the direct picture path at tbp (reipan_video.h).
int    setLoadTexTags(u_int *tags, sceIpuRGB32 *image, int x, int y,
                      int width, int height, int tbp);              // FF3 0x00273d08

#endif /* _SYSTEM_PLAYPSS_LDIMAGE_H */
