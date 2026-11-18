// system/compress/compress.c -- the photo codec (FF3 0x229990..0x22a928,
// see compress.h).
//
// Ported from MioPan's compress.c (FF2 compress.o) and checked against FF3
// function by function.  The algorithm is Mark Nelson's DCT.C: per colour
// plane, 384x128 pixels -> 16 strips of 8 rows -> 48 blocks of 8x8 per strip;
// block -> ForwardDCT -> quantise -> zig-zag -> OutputCode (run-length zeros
// plus a variable-width code) -> OutputBits, and the reverse coming back.
// `quality` scales the quantiser; CompressData (photo_comp.c) tries 1..4.
//
// FF3 differences from FF2:
//   * the debug printfs are gone ("START COMPRESS", the photo information);
//   * pi is the correctly rounded float (.lit4 0x3718c0 = 0x40490fdb, FF2's
//     was one ulp below), so FF3's basis differs from FF2's in the last bit;
//   * the work areas moved: pixelstrip 0x3f6860, C 0x3f7460, Ct 0x3f7560,
//     quantum 0x3f7660, ExpandFile's bit reader 0x3f7760, the run lengths
//     and ExpandFile's cursors in .sbss at gp+0x2cc..gp+0x2e4.
// The logic is FF2's otherwise.  Two ROM oddities kept from FF2:
//   * ExpandFile writes a four-word header at the start of its output on the
//     first slice, and the R plane's first pixels overwrite it in the same
//     call (out_header, top and output are all output2);
//   * repair_flg is written once per macroblock row and read nowhere.

#include "system/compress/compress.h"

#include <math.h>                       // cosf, sqrtf
#include <stdint.h>                     // uintptr_t

#define N       8

// .lit4 0x3718c0.
#define PI      3.14159265f

// Nelson's ROUND(); both arms are emitted at every use, even after a clamp.
#define ROUND(a)    (((a) < 0) ? (int)((a) - 0.5f) : (int)((a) + 0.5f))

// The standard JPEG zig-zag scan as (row, col) pairs; checked against the
// 128 bytes at 0x32dfd0.
DCT_ROOT ZigZag[64] =                                       // data  0x32dfd0
{
    {0, 0}, {0, 1}, {1, 0}, {2, 0}, {1, 1}, {0, 2}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {4, 0}, {3, 1}, {2, 2}, {1, 3}, {0, 4}, {0, 5},
    {1, 4}, {2, 3}, {3, 2}, {4, 1}, {5, 0}, {6, 0}, {5, 1}, {4, 2},
    {3, 3}, {2, 4}, {1, 5}, {0, 6}, {0, 7}, {1, 6}, {2, 5}, {3, 4},
    {4, 3}, {5, 2}, {6, 1}, {7, 0}, {7, 1}, {6, 2}, {5, 3}, {4, 4},
    {3, 5}, {2, 6}, {1, 7}, {2, 7}, {3, 6}, {4, 5}, {5, 4}, {6, 3},
    {7, 2}, {7, 3}, {6, 4}, {5, 5}, {4, 6}, {3, 7}, {4, 7}, {5, 6},
    {6, 5}, {7, 4}, {7, 5}, {6, 6}, {5, 7}, {6, 7}, {7, 6}, {7, 7}
};

PHOTO_EXPAND photo_expand;                                  // sdata 0x3718b0

// One 8-row strip of one colour plane, 384 pixels wide.
static u_char pixelstrip[8][384];                           // bss   0x3f6860

// The DCT basis, its transpose and the quantiser, rebuilt by InitCompress.
static float  C[8][8];                                      // bss   0x3f7460
static float  Ct[8][8];                                     // bss   0x3f7560
static int    quantum[8][8];                                // bss   0x3f7660

static int    InputRunLength;                               // sbss  gp+0x2cc
static int    OutputRunLength;                              // sbss  gp+0x2d0

// Written by ExpandFile once per macroblock row and never read.
static char   repair_flg;                                   // sbss  gp+0x2d4

static void    InitCompress(char quality);
static void    ForwardDCT(u_char **input, int (*output)[8]);
static void    InverseDCT(int (*input)[8], u_char **output);
static void    WriteDCTData(BIT_FILE *output_file, int (*output_data)[8]);
static u_int  *ReadPixelStrip(u_int *input, u_char (*strip)[384], u_char type);
static int     InputCode(BIT_FILE *input_file);
static void    ReadDCTData(BIT_FILE *input_file, int (*input_data)[8]);
static void    OutputCode(BIT_FILE *output_file, int code);
static u_int  *WritePixelStrip(u_int *output, u_char (*strip)[384], u_char type);
static void    OutputBits(BIT_FILE *bit_file, u_int code, int count);
static int     InputBits(BIT_FILE *bit_file, int bit_count);

// ──────────────────────────────────────────────────────────────────────
// The entry points
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00229990
//
// Encodes all three colour planes in one call, each plane's stream padded
// to 16 bytes.  The ratio's division is unguarded (a zero max_size gives an
// infinity).
float CompressFile(u_int *input, char *output, u_int max_size, char quality)
{
    SLIDE_ENCODE_HEADER *sheader = (SLIDE_ENCODE_HEADER *)output;
    int       row;
    int       col;
    int       i;
    int       j;
    u_char   *input_array[8];
    int       output_array[8][8];
    BIT_FILE  bit_file;
    u_int     size;
    u_int    *addr;
    uintptr_t tmp;                      // PC: an EE address was a u_int

    sheader->type    = 1;
    sheader->quality = quality;

    bit_file.file = output + sizeof(SLIDE_ENCODE_HEADER);
    bit_file.mask = 0x80;
    bit_file.rack = 0;

    InitCompress(quality);

    for (i = 0; i < 3; i++)
    {
        addr            = input;
        OutputRunLength = 0;
        InputRunLength  = 0;
        bit_file.mask   = 0x80;
        bit_file.rack   = 0;

        sheader->sizeRGB[i] = (int)(bit_file.file - output);

        for (row = 0; row < 128; row += 8)
        {
            addr = ReadPixelStrip(addr, pixelstrip, (u_char)i);

            for (col = 0; col < 384; col += 8)
            {
                for (j = 0; j < N; j++)
                {
                    input_array[j] = &pixelstrip[j][col];
                }

                ForwardDCT(input_array, output_array);
                WriteDCTData(&bit_file, output_array);
            }
        }

        // A non-zero code flushes the pending run of zeros.
        OutputCode(&bit_file, 1);
        bit_file.file++;

        tmp = (uintptr_t)bit_file.file;
        if ((tmp & 0xf) != 0)
        {
            tmp = tmp - (tmp & 0xf) + 0x10;
        }
        bit_file.file = (char *)tmp;
    }

    size = (u_int)(bit_file.file - output);
    sheader->size = (int)size;

    return (float)size / (float)max_size;
}

// FF3 0x00229ba8
char CheckPhotoExpandEnd(void)
{
    return photo_expand.sta == 2;
}

// FF3 0x00229bb8
//
// Returns the state as it was and claims it: the caller that sees 0 owns
// this frame's slice of the decode.
u_char GetPhotoExpand(void)
{
    u_char ret = photo_expand.sta;

    if (photo_expand.sta == 0)
    {
        photo_expand.sta = 1;
    }

    return ret;
}

// FF3 0x00229bd8
void InitPhotoExpand(u_char no)
{
    photo_expand.sta = 0;
    photo_expand.cnt = 0;
    photo_expand.no  = no;
}

// FF3 0x00229bf8
void ReqPhotoExpand(u_char no)
{
    InitPhotoExpand(no);
}

// FF3 0x00229c10
u_char GetPhotoExpandNo(void)
{
    return photo_expand.no;
}

// FF3 0x00229c20
//
// Decodes one slice: photo_expand.cnt runs 0..23, three colour planes of
// eight steps, each step two macroblock rows (16 pixel rows).  The cursors
// are statics because they live across calls.
void ExpandFile(char *input, u_int *output2)
{
    static u_int               *out_header;                     // gp+0x2d8
    static SLIDE_ENCODE_HEADER *sheader;                        // gp+0x2dc
    static u_int               *top;                            // gp+0x2e0
    static u_int               *output;                         // gp+0x2e4
    static BIT_FILE             bit_file;                       // bss 0x3f7760
    int      row;
    int      col;
    int      i;
    int      j;
    int      input_array[8][8];
    u_char  *output_array[8];
    u_int    end;

    if (photo_expand.sta == 2)
    {
        return;
    }

    if (photo_expand.cnt == 0)
    {
        output = output2;

        // Overwritten by the R plane's first pixels in this very call.
        out_header    = output2;
        out_header[0] = *(u_int *)input;
        out_header[1] = out_header[2] = out_header[3] = 0;

        sheader = (SLIDE_ENCODE_HEADER *)input;

        bit_file.file = input + sizeof(SLIDE_ENCODE_HEADER);
        bit_file.mask = 0x80;
        bit_file.rack = 0;

        top = output;

        InitCompress((char)sheader->quality);
    }

    i = photo_expand.cnt >> 3;
    if (i < 3)
    {
        // First step of a plane: back to the top of the picture, the reader
        // on that plane's stream.
        if ((photo_expand.cnt & 7) == 0)
        {
            OutputRunLength = 0;
            InputRunLength  = 0;
            output          = top;
            bit_file.file   = input + sheader->sizeRGB[i];
            bit_file.mask   = 0x80;
            bit_file.rack   = 0;
        }

        row = (photo_expand.cnt & 7) * 16;
        end = (u_int)row + 16;

        for (; row < (int)end; row += 8)
        {
            repair_flg = (row >= 120);

            for (col = 0; col < 384; col += 8)
            {
                for (j = 0; j < N; j++)
                {
                    output_array[j] = &pixelstrip[j][col];
                }

                ReadDCTData(&bit_file, input_array);
                InverseDCT(input_array, output_array);
            }

            output = WritePixelStrip(output, pixelstrip, (u_char)i);
        }

        photo_expand.cnt++;
    }

    if (photo_expand.cnt >= 24)
    {
        photo_expand.cnt = 0;
        photo_expand.sta = 2;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The codec
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00229e38
//
// The quantiser and the DCT basis for one quality.  The quantiser ramps by
// (1 + 2i/3 + 2j/3), gentler than Nelson's (1 + i + j).
static void InitCompress(char quality)
{
    u_int i;
    u_int j;
    float val;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            quantum[i][j] = 1 + (1 + (int)(i * 2) / 3 + (int)(j * 2) / 3) * quality;
        }
    }

    val = 1.0f / sqrtf((float)N);
    for (j = 0; j < N; j++)
    {
        C[0][j]  = val;
        Ct[j][0] = val;
    }

    val = sqrtf(2.0f / (float)N);
    for (i = 1; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            C[i][j]  = val * cosf((float)(2 * j + 1) * PI * (float)i * (1.0f / (2.0f * N)));
            Ct[j][i] = C[i][j];
        }
    }
}

// FF3 0x0022a090
//
// 8x8 forward DCT, separable; the pixels are biased to -128..127.
static void ForwardDCT(u_char **input, int (*output)[8])
{
    float temp[8][8];
    float temp1;
    u_int i, j, k;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            temp[i][j] = 0.0f;
            for (k = 0; k < N; k++)
            {
                temp[i][j] += (input[i][k] - 128) * Ct[k][j];
            }
        }
    }

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            temp1 = 0.0f;
            for (k = 0; k < N; k++)
            {
                temp1 += C[i][k] * temp[k][j];
            }
            output[i][j] = ROUND(temp1);
        }
    }
}

// FF3 0x0022a1d0
//
// 8x8 inverse DCT; +128, clamped to 0..255 before the round.
static void InverseDCT(int (*input)[8], u_char **output)
{
    float temp[8][8];
    float temp1;
    u_int i, j, k;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            temp[i][j] = 0.0f;
            for (k = 0; k < N; k++)
            {
                temp[i][j] += input[i][k] * C[k][j];
            }
        }
    }

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < N; j++)
        {
            temp1 = 0.0f;
            for (k = 0; k < N; k++)
            {
                temp1 += Ct[i][k] * temp[k][j];
            }
            temp1 += 128.0f;
            if (temp1 < 0)
            {
                output[i][j] = 0;
            }
            else if (temp1 > 255)
            {
                output[i][j] = 255;
            }
            else
            {
                output[i][j] = (u_char)ROUND(temp1);
            }
        }
    }
}

// FF3 0x0022a360
//
// Quantises a block and writes its 64 coefficients in zig-zag order, so the
// high-frequency zeros arrive in one run.
static void WriteDCTData(BIT_FILE *output_file, int (*output_data)[8])
{
    int   i;
    int   row;
    int   col;
    float result;

    for (i = 0; i < 64; i++)
    {
        row    = ZigZag[i].row;
        col    = ZigZag[i].col;
        result = output_data[row][col] / (float)quantum[row][col];
        OutputCode(output_file, ROUND(result));
    }
}

// FF3 0x0022a450
//
// One 8-row strip of one colour plane out of the packed RGB source.  A
// component of 0 is forced to 1.
static u_int *ReadPixelStrip(u_int *input, u_char (*strip)[384], u_char type)
{
    int   row;
    int   col;
    u_int c;

    for (row = 0; row < N; row++)
    {
        for (col = 0; col < 384; col++)
        {
            c = *input++;

            if (type == 0)
            {
                strip[row][col] = (u_char)(c >> 16);
            }
            else if (type == 1)
            {
                strip[row][col] = (u_char)(c >> 8);
            }
            else if (type == 2)
            {
                strip[row][col] = (u_char)c;
            }

            if (strip[row][col] == 0)
            {
                strip[row][col] = 1;
            }
        }
    }

    return input;
}

// FF3 0x0022a4e0
//
// One coefficient: a 2-bit class, then a run of zeros or a 1..15-bit value.
static int InputCode(BIT_FILE *input_file)
{
    int bit_count;
    int result;

    if (InputRunLength > 0)
    {
        InputRunLength--;
        return 0;
    }

    if ((bit_count = InputBits(input_file, 2)) == 0)
    {
        InputRunLength = InputBits(input_file, 4);
        return 0;
    }

    if (bit_count == 1)
    {
        bit_count = InputBits(input_file, 1) + 1;
    }
    else
    {
        bit_count = InputBits(input_file, 2) + (bit_count << 2) - 5;
    }

    result = InputBits(input_file, bit_count);

    if ((result >> (bit_count - 1)) & 1)
    {
        return result;
    }

    return result - (1 << bit_count) + 1;
}

// FF3 0x0022a5b0
static void ReadDCTData(BIT_FILE *input_file, int (*input_data)[8])
{
    int i;
    int row;
    int col;

    for (i = 0; i < 64; i++)
    {
        row = ZigZag[i].row;
        col = ZigZag[i].col;
        input_data[row][col] = InputCode(input_file) * quantum[row][col];
    }
}

// FF3 0x0022a650
//
// One coefficient.  Zeros accumulate into a run flushed in chunks of at most
// 16 when a non-zero arrives.
static void OutputCode(BIT_FILE *output_file, int code)
{
    int top_of_range;
    int abs_code;
    int bit_count;

    if (code == 0)
    {
        OutputRunLength++;
        return;
    }

    if (OutputRunLength != 0)
    {
        while (OutputRunLength > 0)
        {
            OutputBits(output_file, 0, 2);
            if (OutputRunLength <= 16)
            {
                OutputBits(output_file, OutputRunLength - 1, 4);
                OutputRunLength = 0;
            }
            else
            {
                OutputBits(output_file, 15, 4);
                OutputRunLength -= 16;
            }
        }
    }

    abs_code = (code < 0) ? -code : code;

    top_of_range = 1;
    bit_count    = 1;
    while (abs_code > top_of_range)
    {
        bit_count++;
        top_of_range = ((top_of_range + 1) * 2) - 1;
    }

    if (bit_count < 3)
    {
        OutputBits(output_file, bit_count + 1, 3);
    }
    else
    {
        OutputBits(output_file, bit_count + 5, 4);
    }

    if (code > 0)
    {
        OutputBits(output_file, code, bit_count);
    }
    else
    {
        OutputBits(output_file, code + top_of_range, bit_count);
    }
}

// FF3 0x0022a7a8
//
// Merges one decoded strip into the packed destination: the R pass sets the
// word (with the 0x01 top byte), G and B OR into it, so the planes go in
// order.
static u_int *WritePixelStrip(u_int *output, u_char (*strip)[384], u_char type)
{
    int row;
    int col;

    for (row = 0; row < N; row++)
    {
        for (col = 0; col < 384; col++)
        {
            if (type == 0)
            {
                *output  = 0x01000000;
                *output |= strip[row][col] << 16;
            }
            else if (type == 1)
            {
                *output |= strip[row][col] << 8;
            }
            else if (type == 2)
            {
                *output |= strip[row][col];
            }

            output++;
        }
    }

    return output;
}

// FF3 0x0022a848
static void OutputBits(BIT_FILE *bit_file, u_int code, int count)
{
    u_int mask;

    for (mask = 1u << (count - 1); mask != 0; mask >>= 1)
    {
        if (mask & code)
        {
            bit_file->rack |= bit_file->mask;
        }

        bit_file->mask >>= 1;
        if (bit_file->mask == 0)
        {
            *bit_file->file = (char)bit_file->rack;
            bit_file->rack  = 0;
            bit_file->mask  = 0x80;
            bit_file->file++;
        }
    }
}

// FF3 0x0022a8c0
//
// The byte is read sign-extended (lb); only the masked bits are used.
static int InputBits(BIT_FILE *bit_file, int bit_count)
{
    u_int mask;
    int   return_value;

    mask         = 1u << (bit_count - 1);
    return_value = 0;
    while (mask != 0)
    {
        if (bit_file->mask == 0x80)
        {
            bit_file->rack = (signed char)*bit_file->file++;
        }

        if (bit_file->rack & bit_file->mask)
        {
            return_value |= mask;
        }

        mask >>= 1;
        bit_file->mask >>= 1;
        if (bit_file->mask == 0)
        {
            bit_file->mask = 0x80;
        }
    }

    return return_value;
}
