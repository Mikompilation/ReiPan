/* ==========================================================================
 *  graphics/ecb.c
 *
 *  ECB table files (see ecb.h). FF3 0x154cd0..0x154df0; FF3-only, written
 *  from the FF3 disassembly. Every reader takes the loaded file and checks
 *  it for nullptr. 0x154cf0, 0x154d18 and 0x154d98 have no callers (and
 *  EcbGetExt only them); no disc file has an extra block.
 * ======================================================================== */
#include "graphics/ecb.h"

#include "sdk/scetypes.h"                       // u_char

// ──────────────────────────────────────────────────────────────────────
// The extra block: ecb + ext_ofs + ofs; nullptr for no file or no block.
// FF3 0x00154cd0.

void *EcbGetExt(const void *ecb, int ofs)
{
    const ECB_HEAD *head = (const ECB_HEAD *)ecb;

    if (head == nullptr || head->ext_ofs == 0)
    {
        return nullptr;
    }

    return (char *)ecb + head->ext_ofs + ofs;
}

// FF3 0x00154cf0 (no callers).
void *EcbGetExtData(const void *ecb, int ofs)
{
    char *ext = (char *)EcbGetExt(ecb, ofs);

    return ext != nullptr ? ext + 4 : nullptr;
}

// The short at +2 of an extra-block entry; FF3 reads it a byte at a time
// (the entry need not be aligned).
// FF3 0x00154d18 (no callers).
int EcbGetExtNum(const void *ecb, int ofs)
{
    const u_char *ext = (const u_char *)EcbGetExt(ecb, ofs);

    if (ext == nullptr)
    {
        return 0;
    }

    return (short)(ext[2] | (ext[3] << 8));
}

// ──────────────────────────────────────────────────────────────────────
// The head fields.

// FF3 0x00154d68.
int EcbGetNum(const void *ecb)
{
    if (ecb == nullptr)
    {
        return 0;
    }

    return ((const ECB_HEAD *)ecb)->row_num;
}

// FF3 0x00154d80.
int EcbGetSize(const void *ecb)
{
    if (ecb == nullptr)
    {
        return 0;
    }

    return ((const ECB_HEAD *)ecb)->row_size;
}

// FF3 0x00154d98 (no callers).
int EcbGetFileSize(const void *ecb)
{
    if (ecb == nullptr)
    {
        return 0;
    }

    return ((const ECB_HEAD *)ecb)->file_size;
}

// ──────────────────────────────────────────────────────────────────────
// Row no: the rows start right after the head; nullptr out of range.
// FF3 0x00154db0.

void *EcbGetData(const void *ecb, int no)
{
    const ECB_HEAD *head = (const ECB_HEAD *)ecb;

    if (head == nullptr)
    {
        return nullptr;
    }

    if (no < 0 || no >= head->row_num)
    {
        return nullptr;
    }

    return (char *)ecb + sizeof(ECB_HEAD) + no * head->row_size;
}
