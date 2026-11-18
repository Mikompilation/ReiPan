// ingame/photo/photo_comp.c -- photo storage (FF2 photo_make.c's storage
// half; FF3 0x228718..0x2289a8, see photo_comp.h).
//
// Ported from MioPan's photo_make.c and checked against FF3 function by
// function. FF3 differences from FF2: none in the logic; the debug printfs
// are gone and the DCT pass's quality counter runs 1..4 as a counted loop.
// The work areas are EE constants on both sides; ReiPan_GetHostPointer
// resolves them.

#include "ingame/photo/photo_comp.h"

#include <string.h>                             // memset, memcpy

#include "ingame/photo/photo_make.h"            // PHOTO_AREA
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer
#include "system/compress/compress.h"           // CompressFile, ExpandFile, photo_expand
#include "system/encodes/encodes.h"             // SlideEncode, SlideDecode

// The bar an encoding has to clear (sdata 0x371870), and the DCT pass's
// worst quality.
#define PHOTO_COMPRESS_RATE 0.274f
#define PHOTO_QUALITY_MAX   4

// ──────────────────────────────────────────────────────────────────────
// The LZSS header wrappers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00228718

float SlideEncodeHeader(u_char *base, u_char *addrs, int max_size)
{
    SLIDE_ENCODE_HEADER *sheader = (SLIDE_ENCODE_HEADER *)addrs;

    sheader->type = 0;
    sheader->size = SlideEncode(base, addrs + sizeof(SLIDE_ENCODE_HEADER), max_size);

    return (float)sheader->size / (float)max_size;
}

// FF3 0x00228770

void SlideDecodeHeader(u_char *base, u_char *addrs)
{
    SlideDecode(base + sizeof(SLIDE_ENCODE_HEADER), addrs, ((SLIDE_ENCODE_HEADER *)base)->size);
}

// ──────────────────────────────────────────────────────────────────────
// Encode / decode
// ──────────────────────────────────────────────────────────────────────

// The picture at addri encoded behind it (addri + PHOTO_RAW_SIZE) and filed
// in slot n of addro: LZSS when it gets under the bar, else the DCT codec at
// quality 1..4, else type 2 (the slot decodes to black). A failed pass only
// costs time: each writes into the same scratch area.
// FF3 0x00228790

void CompressData(uintptr_t addri, uintptr_t addro, int n)
{
    u_char *in  = (u_char *)ReiPan_GetHostPointer(addri);
    u_char *out = (u_char *)ReiPan_GetHostPointer(addro);
    SLIDE_ENCODE_HEADER *sheader = (SLIDE_ENCODE_HEADER *)(in + PHOTO_RAW_SIZE);
    float rate;
    int quality;

    rate = SlideEncodeHeader(in, (u_char *)sheader, PHOTO_RAW_SIZE);
    if (rate > PHOTO_COMPRESS_RATE)
    {
        for (quality = 1; quality <= PHOTO_QUALITY_MAX; quality++)
        {
            memset(sheader, 0xff, PHOTO_ENCODE_SIZE);
            rate = CompressFile((u_int *)in, (char *)sheader, PHOTO_ENCODE_SIZE, (char)quality);
            if (rate < PHOTO_COMPRESS_RATE)
            {
                break;
            }
        }
        if (quality > PHOTO_QUALITY_MAX)
        {
            sheader->type = 2;
        }
    }

    memcpy(out + n * PHOTO_ONE_SIZE, sheader, PHOTO_ONE_SIZE);
}

// Slot n of addri (0xff: the one ReqPhotoExpand asked for) decoded into
// addro. LZSS and blank finish here and post the end; DCT runs thirty
// slices per call and ExpandFile posts it when the picture is done.
// FF3 0x00228878

void UncompressData(uintptr_t addri, int n, uintptr_t addro)
{
    u_char *in  = (u_char *)ReiPan_GetHostPointer(addri);
    u_char *out = (u_char *)ReiPan_GetHostPointer(addro);
    u_char *base;
    int i;

    if (n == 0xff)
    {
        n = GetPhotoExpandNo();
    }

    base = in + n * PHOTO_ONE_SIZE;

    switch (((SLIDE_ENCODE_HEADER *)base)->type)
    {
    case 0:
        SlideDecodeHeader(base, out);
        break;
    case 1:
        for (i = 0; i < 30; i++)
        {
            ExpandFile((char *)base, (u_int *)out);
        }
        return;
    case 2:
        memset(out, 0, PHOTO_ENCODE_SIZE);
        break;
    default:
        return;
    }

    photo_expand.sta = 2;
    photo_expand.cnt = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The fixed-address wrappers
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00228968

void CompPhotoFromWorkArea(int n)
{
    CompressData(PHOTO_WORK_AREA, PHOTO_AREA, n);
}

// FF3 0x00228990

void UncompressPhoto(u_char n)
{
    ReqPhotoExpand(n);
}
