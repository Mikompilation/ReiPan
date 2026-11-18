/* ==========================================================================
 *  common/pk4.h
 *
 *  FF3's pack accessors (FF3 0x1eff18..0x1f0138, no FF2 counterpart; the
 *  file name is a guess). Two pack formats, both raw file data:
 *
 *    PK2: +0 count, +0x10 offset table (PK2SendVram's format).
 *    PK4: +0 "PK4\0", +4 headed flag, +8 count, +0x10 offset table. When headed (> 0)
 *         every member starts with { header size, size, name[] } and its
 *         data follows the header.
 *
 *  Offsets are from the start of the pack. All functions take and return
 *  host pointers (PS2: EE addresses); null packs and out-of-range members
 *  give 0 / null.
 * ======================================================================== */
#ifndef _COMMON_PK4_H
#define _COMMON_PK4_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

struct PK4_HEADER                       /* 0x10 + 4 * num */
{
    /* 0x00 */ int  magic;              // "PK4\0" (LANG_SL_PK4: unheaded, 3 TIM2s)
    /* 0x04 */ int  headed;             // > 0: members carry a PK4_MEMBER header
    /* 0x08 */ int  num;
    /* 0x0c */ int  pad0c;
    /* 0x10 */ int  offset[1];          // [num], from the pack start
};
static_assert(offsetof(PK4_HEADER, headed) == 0x04, "PK4_HEADER.headed");
static_assert(offsetof(PK4_HEADER, num) == 0x08, "PK4_HEADER.num");
static_assert(offsetof(PK4_HEADER, offset) == 0x10, "PK4_HEADER.offset");

struct PK4_MEMBER                       /* header of a headed member */
{
    /* 0x00 */ int  head_size;          // the data starts here
    /* 0x04 */ int  size;
    /* 0x08 */ char name[1];            // NUL-terminated
};
static_assert(offsetof(PK4_MEMBER, size) == 0x04, "PK4_MEMBER.size");
static_assert(offsetof(PK4_MEMBER, name) == 0x08, "PK4_MEMBER.name");

void *Pk2GetData(void *pk2, int no);            // FF3 0x001eff18
int   Pk4GetOffset(void *pk4, int no);          // FF3 0x001eff50
int   Pk4GetHeadSize(void *pk4, int no);        // FF3 0x001eff60
int   Pk4GetSize(void *pk4, int no);            // FF3 0x001effc8
char *Pk4GetName(void *pk4, int no);            // FF3 0x001f0030
void *Pk4GetData(void *pk4, int no);            // FF3 0x001f0098

#endif /* _COMMON_PK4_H */
