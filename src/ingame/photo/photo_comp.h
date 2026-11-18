/* ==========================================================================
 *  ingame/photo/photo_comp.h
 *
 *  Photo storage (FF2 photo_make.c's storage half; FF3 0x228718..0x2289a8,
 *  an object of its own between photo_dat.c and photo_obj.c -- the drawing
 *  half is photo_make.c at 0x22ada0):
 *    * CompressData encodes the 384x128 PSMCT32 picture into one album slot:
 *      the lossless LZSS (SlideEncode) first, the lossy DCT codec
 *      (CompressFile) at worsening quality when LZSS does not get under
 *      PHOTO_COMPRESS_RATE, a blank slot (type 2) when quality 4 fails too;
 *    * UncompressData decodes a slot by its header type, LZSS and blank in
 *      one call, DCT a slice per frame (ExpandFile) through photo_expand.
 *  The slots are PHOTO_ONE_SIZE apart; each starts with a
 *  SLIDE_ENCODE_HEADER and the encoded data follows it.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_COMP_H
#define _INGAME_PHOTO_PHOTO_COMP_H

#include <stdint.h>                     // uintptr_t

#include "sdk/scetypes.h"
#include "system/compress/compress.h"  // SLIDE_ENCODE_HEADER, PHOTO_EXPAND

#define PHOTO_RAW_SIZE      0x30000     // 384 * 128 * 4
#define PHOTO_ENCODE_SIZE   0x30020     // header + raw
#define PHOTO_ONE_SIZE      0xd360      // one encoded album slot

// The capture the photo sequence encodes (CopyScreenToBuffer2's target); its
// header and scratch area follow it at + PHOTO_RAW_SIZE.
#define PHOTO_WORK_AREA     0x10ef000

// LZSS-encodes base (max_size bytes) behind the header at addrs: type 0, the
// size; returns size / max_size.
float SlideEncodeHeader(u_char *base, u_char *addrs, int max_size);            // FF3 0x00228718
// Decodes the LZSS slot at base into addrs.
void  SlideDecodeHeader(u_char *base, u_char *addrs);                          // FF3 0x00228770
// Encodes the picture at EE addri into slot n of EE addro.
void  CompressData(uintptr_t addri, uintptr_t addro, int n);                   // FF3 0x00228790
// Decodes slot n (0xff: the one requested) of EE addri into EE addro.
void  UncompressData(uintptr_t addri, int n, uintptr_t addro);                 // FF3 0x00228878
// Encodes the photo sequence's capture into PHOTO_AREA slot n.
void  CompPhotoFromWorkArea(int n);                                            // FF3 0x00228968
// Requests slot n from the decoder (photo_expand back to 0).
void  UncompressPhoto(u_char n);                                               // FF3 0x00228990

#endif /* _INGAME_PHOTO_PHOTO_COMP_H */
