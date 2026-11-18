#include "utility2.h"

#include <string.h>                 // memcpy, strlen

// FF3's utility2 object is FF2's without the debug bodies: the assert / warning
// reporters and DumpMemReal compile to bare returns, and Get2Power, the random
// helpers, SetAssertPreMessage and PrintBit are gone from it. In the ROM only
// GetAlignUp and StrToLower have callers.

// ──────────────────────────────────────────────────────────────────────
// Round `a` up to a multiple of 1 << power. The second argument is a shift count:
// GetAlignUp(size, 4) aligns to 16 bytes.
// FF3 0x00268ac0, instruction-for-instruction FF2's 0x0026c900.

unsigned int GetAlignUp(unsigned int a, int power)
{
    unsigned int mask;

    mask = ~(-1 << (power & 0x1f));
    return ((a + mask) >> (power & 0x1f)) << (power & 0x1f);
}

// ──────────────────────────────────────────────────────────────────────
// Offset of the first `chr` in `string`, or -1 when the string ends first.
// FF3 0x00268ae0 (FF2 0x0026c920)

int GetOffsetChar(char *string, char chr)
{
    int ofst;

    for (ofst = 0; string[ofst] != '\0'; ofst++)
    {
        if (string[ofst] == chr)
        {
            return ofst;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00268b10 (FF2 0x0026c960)

int GetStrLen(char *str)
{
    int len;

    len = 0;
    while (str[len] != '\0')
    {
        len++;
    }
    return len;
}

// ──────────────────────────────────────────────────────────────────────
// Offset of the last `chr` in the first `len` bytes of `string`, searching
// backwards from len - 1; -1 when a '\0' or the start comes first.
// FF3 0x00268b48 (FF2 0x0026c998)

int GetOffsetLastChar(char *string, int len, char chr)
{
    int ofst;

    ofst = len - 1;
    while (string[ofst] != chr)
    {
        if (string[ofst] == '\0')
        {
            return -1;
        }
        ofst--;
        if (ofst < 0)
        {
            return -1;
        }
    }
    return ofst;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the text after the last '.' of `filename` equals `c_extent` (given
// without the dot), 0 otherwise or when there is no '.'.
// FF3 0x00268ba8 (FF2 0x0026c9f0)

int FileExtensionMatch(char *filename, char *c_extent)
{
    char *ptr;
    int   ofst;

    ofst = GetOffsetLastChar(filename, GetStrLen(filename), '.');
    if (ofst < 0)
    {
        return 0;
    }

    ptr = filename + ofst + 1;
    while (*ptr == *c_extent)
    {
        if (*ptr == '\0')
        {
            return 1;
        }
        ptr++;
        c_extent++;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The four bytes after the first '.' of `filename` as an int (a packed
// extension tag), 0 when there is no '.'. The ROM reads them with a plain lw,
// which needs the address 4-aligned; memcpy reads the same bytes on any host.
// FF3 0x00268c20 (FF2 0x0026ca68)

int GetExtension(char *filename)
{
    int ofst;
    int ext;

    ofst = GetOffsetChar(filename, '.');
    if (ofst < 0)
    {
        return 0;
    }
    memcpy(&ext, filename + ofst + 1, sizeof(ext));
    return ext;
}

// ──────────────────────────────────────────────────────────────────────
// Next index of a `buf_num`-slot ring.
// FF3 0x00268c60 (FF2 0x0026caa8)

int RingBufAdd(int idx, int buf_num)
{
    idx++;
    if (idx >= buf_num)
    {
        idx = 0;
    }
    return idx;
}

// ──────────────────────────────────────────────────────────────────────
// Distance from `later` forward to `former` around a `num`-slot ring
// (former == later reads as 0, an empty ring).
// FF3 0x00268c78 (FF2 0x0026cab8)

int RingBufCalcDiff(int former, int later, int num)
{
    if (former < later)
    {
        return num - (later - former);
    }
    return former - later;
}

// ──────────────────────────────────────────────────────────────────────
// FF2 printed a hex dump here; FF3's release body is a bare return.
// FF3 0x00268c98 (FF2 0x0026cad8)

void DumpMemReal(unsigned int adrs, int size, char *name)
{
    (void)adrs;
    (void)size;
    (void)name;
}

// ──────────────────────────────────────────────────────────────────────
// Clamp `val` into [lower, upper]. Note the argument order: upper before lower.
// FF3 0x00268ca0 (FF2 0x0026cb98)

float GetClampValF(float val, float upper, float lower)
{
    if (upper < val)
    {
        return upper;
    }
    if (val < lower)
    {
        return lower;
    }
    return val;
}

// ──────────────────────────────────────────────────────────────────────
// The int twin of GetClampValF.
// FF3 0x00268cc8 (FF2 0x0026cbc0)

int GetClampVal(int val, int upper, int lower)
{
    if (upper < val)
    {
        return upper;
    }
    if (val < lower)
    {
        return lower;
    }
    return val;
}

// ──────────────────────────────────────────────────────────────────────
// FF2 installed the warning / assert report hooks here; FF3 stores nothing.
// FF3 0x00268ce8 (FF2 0x0026cbe0)

void SetPrintWarning(void (*func)(char *))
{
    (void)func;
}

// FF3 0x00268cf0 (FF2 0x0026cbe8)

void SetPrintAssert(void (*func)(char *))
{
    (void)func;
}

// ──────────────────────────────────────────────────────────────────────
// The reporters: two variadics with empty bodies (they only spill a1..t3).
// FF2 has three here (PrintWarningReal, SetAssertPreMessage, PrintAssertReal);
// which two FF3 kept is a guess from the order, nothing calls them.
// FF3 0x00268cf8 (FF2 0x0026cbf0)

void PrintWarningReal(char *str, ...)
{
    (void)str;
}

// FF3 0x00268d20 (FF2 0x0026cd10)

void PrintAssertReal(char *str, ...)
{
    (void)str;
}

// ──────────────────────────────────────────────────────────────────────
// Lower-case str in place and return it. The ROM tests the libc ctype
// table's upper-case bit (data 0x3663a8, indexed by the signed char); that
// bit is set for 'A'..'Z' only.
// FF3 0x00268d48 (FF2 0x0026cdf8)

char *StrToLower(char *str)
{
    for (char *p = str; *p != '\0'; p++)
    {
        if (*p >= 'A' && *p <= 'Z')
        {
            *p += 'a' - 'A';
        }
    }
    return str;
}

// ──────────────────────────────────────────────────────────────────────
// The file name after the last '/' of `path_file`.
// PORT: the ROM scans back without a bound, so a path with no '/' reads before
// the string; here the scan stops at the start and returns the whole string.
// FF3 0x00268db0 (FF2 0x0026ce50)

char *GetNameWithoutPath(char *path_file)
{
    int len;

    len = (int)strlen(path_file);
    while (len >= 0 && path_file[len] != '/')
    {
        len--;
    }
    return &path_file[len + 1];
}

// ──────────────────────────────────────────────────────────────────────
// Fold a rotation back into [-PI, PI]. One step, not a modulo: a value more
// than one turn out comes back still out of range. FF3's constants (rodata
// 0x3727c8 / 0x3727cc / 0x3727d0) are the true float PI 0x40490fdb, 2 PI and
// -PI; FF2's object had the truncated 3.1415925f.
// FF3 0x00268e18 (FF2 0x0026cea0)

float RotLimitChk2(float rot)
{
    if (3.14159274f < rot)
    {
        return rot - 6.28318548f;
    }
    if (rot < -3.14159274f)
    {
        return rot + 6.28318548f;
    }
    return rot;
}
