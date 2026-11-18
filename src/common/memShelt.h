/* ==========================================================================
 *  common/memShelt.h
 *
 *  CmemShelt (FF3 RTTI, vtable 0x3410a0): a "memory shelter", a buffer on
 *  the system heap that keeps a copy of some work while it is replaced and
 *  gives it back later. CExtraMissionShelt (ingame/mission/ExtraMissionShelt.h)
 *  keeps the story game's card data in one while an extra mission runs, and
 *  one per save block a mission's results carry back over it (CarryStore /
 *  CarryRestore). FF3 0x00110368..0x001105f0; FF3-only, no FF2 file.
 * ======================================================================== */
#ifndef _COMMON_MEMSHELT_H
#define _COMMON_MEMSHELT_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#define MEM_SHELT_ALL   (-1)            // CmemShelt::Store / CmemShelt::Restore: the whole buffer

// 0x10 bytes on the EE.
// layout: host -- the vptr and buf are 4 bytes on the EE: buf is at EE +4,
// size +8, used +0xc.
class CmemShelt
{
public:
    CmemShelt();                        // FF3 0x00110398 (MemSheltCtor; twin 0x00110378)
    CmemShelt(int size);                // FF3 0x00110400 (MemSheltCtorSize; twin 0x001103b8)
    virtual ~CmemShelt();               // EE vtable +0x00 0x00110470 (base 0x00110448), deleting +0x04 0x00110498

    void Clear();                       // FF3 0x00110368: buf, size, used 0 (no free)
    void Free();                        // FF3 0x001104d0
    void Alloc(int size);               // FF3 0x00110528: a new buffer (the old one freed), used 0
    void Store(const void *src, int n); // FF3 0x00110590: n bytes of src in; MEM_SHELT_ALL: size
    void Restore(void *const *dst, int n);  // FF3 0x001105b8: n bytes out to *dst; MEM_SHELT_ALL: size
    void SetUsed(int n);                // FF3 0x001105e8: after filling buf directly

    void *buf;                          // EE +0x04: system heap block, nullptr none
    int   size;                         // EE +0x08: its bytes
    int   used;                         // EE +0x0c: bytes stored (CmemShelt::Store / CmemShelt::SetUsed)
};
static_assert(offsetof(CmemShelt, size) == offsetof(CmemShelt, buf) + sizeof(void *), "CmemShelt.size follows buf");
static_assert(offsetof(CmemShelt, used) == offsetof(CmemShelt, size) + 4, "CmemShelt.used follows size");

#endif /* _COMMON_MEMSHELT_H */
