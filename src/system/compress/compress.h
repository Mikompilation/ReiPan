/* ==========================================================================
 *  system/compress/compress.h
 *
 *  compress.o -- the photo codec: an 8x8 DCT, a quality-driven quantiser and
 *  a run-length-coded bit stream, plus the three-byte handshake the album
 *  decodes photos through.  FF3 0x229990..0x22a928 (18 functions), between
 *  photo_obj.c and photo_obj_dat.c.
 *
 *  The algorithm is Mark Nelson's DCT.C from "The Data Compression Book",
 *  adapted to memory buffers and to the game's packed-RGB picture format.
 *  Ported from MioPan's compress.c (FF2) and checked against FF3 function by
 *  function; compress.c lists the differences.
 *
 *  ExpandFile() does two macroblock rows per call and a picture takes
 *  twenty-four of them.  A viewer posts a request with ReqPhotoExpand(), then
 *  asks CheckPhotoExpandEnd() each frame; GetPhotoExpand() moves 0 -> 1 so
 *  only the caller that saw the 0 does the work.
 * ======================================================================== */
#ifndef _SYSTEM_COMPRESS_COMPRESS_H
#define _SYSTEM_COMPRESS_COMPRESS_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

/* --------------------------------------------------------------------------
 *  The 0x20-byte header every stored photo carries.
 *
 *  `type`: 0 lossless (LZSS, `size` is the encoded byte count), 1 lossy (the
 *  DCT codec below), 2 blank (CompressData's "nothing fits" marker).  For a
 *  DCT picture `sizeRGB[c]` is the offset from the header at which colour
 *  plane c's bit stream starts and `quality` the quantiser scale.
 * ------------------------------------------------------------------------ */
typedef struct _SLIDE_ENCODE_HEADER     /* 0x20 */
{
    /* 0x00 */ int size;                // the whole encoded picture
    /* 0x04 */ int type;                // 0 LZSS, 1 DCT, 2 blank
    /* 0x08 */ int pad[2];
    /* 0x10 */ int sizeRGB[3];          // DCT: the planes' offsets
    /* 0x1c */ int quality;             // DCT: 1..4
} SLIDE_ENCODE_HEADER;
static_assert(sizeof(SLIDE_ENCODE_HEADER) == 0x20, "SLIDE_ENCODE_HEADER: data at +0x20");
static_assert(offsetof(SLIDE_ENCODE_HEADER, type) == 0x04, "SLIDE_ENCODE_HEADER.type");
static_assert(offsetof(SLIDE_ENCODE_HEADER, sizeRGB) == 0x10, "SLIDE_ENCODE_HEADER.sizeRGB");
static_assert(offsetof(SLIDE_ENCODE_HEADER, quality) == 0x1c, "SLIDE_ENCODE_HEADER.quality");

/* --------------------------------------------------------------------------
 *  The decode handshake (sdata 0x3718b0 = gp-0x1c40).
 * ------------------------------------------------------------------------ */
typedef struct _PHOTO_EXPAND            /* 0x3 */
{
    /* 0x0 */ u_char sta;               // 0 requested, 1 decoding, 2 decoded
    /* 0x1 */ u_char cnt;               // ExpandFile's slice, 0..23
    /* 0x2 */ u_char no;                // the album slot requested
} PHOTO_EXPAND;
static_assert(sizeof(PHOTO_EXPAND) == 3, "PHOTO_EXPAND: three bytes at gp-0x1c40");
static_assert(offsetof(PHOTO_EXPAND, cnt) == 1, "PHOTO_EXPAND.cnt");
static_assert(offsetof(PHOTO_EXPAND, no) == 2, "PHOTO_EXPAND.no");

/* One entry of the zig-zag scan order: the (row, col) of the n'th
 * coefficient.  Nelson's `struct zigzag`. */
typedef struct _DCT_ROOT                /* 0x2 */
{
    /* 0x0 */ char row;
    /* 0x1 */ char col;
} DCT_ROOT;
static_assert(sizeof(DCT_ROOT) == 2, "DCT_ROOT: (row, col) bytes");

/* Nelson's BIT_FILE with the FILE * replaced by a buffer cursor.
 * layout: host -- `file` is a pointer (EE size 0xc: file 0x0, mask 0x4,
 * rack 0x8); nothing reads it from game data, so only the order is kept. */
typedef struct _BIT_FILE
{
    char  *file;                        // the byte cursor
    u_char mask;                        // walks the byte from 0x80 down
    int    rack;                        // the byte being built or read
} BIT_FILE;

extern PHOTO_EXPAND photo_expand;       // sdata 0x3718b0
extern DCT_ROOT     ZigZag[64];         // data  0x32dfd0

char   CheckPhotoExpandEnd(void);       // FF3 0x00229ba8: sta == 2
u_char GetPhotoExpand(void);            // FF3 0x00229bb8: the state; 0 -> 1
void   InitPhotoExpand(u_char no);      // FF3 0x00229bd8: {0, 0, no}
void   ReqPhotoExpand(u_char no);       // FF3 0x00229bf8: InitPhotoExpand
u_char GetPhotoExpandNo(void);          // FF3 0x00229c10: the slot requested

// Encodes one 384x128 picture (packed 0x00RRGGBB words) into `output`,
// header first; returns the output / max_size ratio.  `max_size` is only the
// ratio's denominator, not a bound on what is written.
float  CompressFile(u_int *input, char *output, u_int max_size, char quality); // FF3 0x00229990
// Decodes one slice of the DCT picture at `input` into `output2`, advancing
// photo_expand; call it until CheckPhotoExpandEnd().
void   ExpandFile(char *input, u_int *output2);                                // FF3 0x00229c20

#endif /* _SYSTEM_COMPRESS_COMPRESS_H */
