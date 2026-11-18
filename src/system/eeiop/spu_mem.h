/* ==========================================================================
 *  system/eeiop/spu_mem.h
 *
 *  SPU2 RAM allocator (FF2 spu_mem.c; FF3 0x270cf8..0x2711f0). Addresses are
 *  SPU RAM offsets carried as int (they go to the IOP in commands), never
 *  EE pointers; 0 means no memory.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_SPU_MEM_H
#define _SYSTEM_EEIOP_SPU_MEM_H

// [0, start) is reserved (the SPU2's own area); size bytes after it are
// managed. FF3 takes both from EEIOP_DEF (FF2's had no arguments).
void SPUMemoryInit(int start, int size);    // 0x270d38

// Best fit, rounded up to 64 bytes. 0 when nothing fits.
int  GetSPUMemory(int size);                // 0x270dc0
int  ReleaseSPUMemory(int adrs);            // 0x270f90: 1 when adrs was found

// Reverb work area: placed to end on a 128 KB boundary, as high as the free
// runs allow. Returns its end address - 1 (what the SPU2's EEA register
// takes and ReleaseSPUEffectMemory matches), 0 when nothing fits.
int  GetSPUEffectMemory(int size);          // 0x270fb0
int  ReleaseSPUEffectMemory(int end_adrs);  // 0x271100: 1 when found

void PrintBufferState(void);                // 0x271120: empty in FF3
int  SPUQueryMaxFreeSize(void);             // 0x271128
int  SPUQueryTotalFreeSize(void);           // 0x271188

#endif /* _SYSTEM_EEIOP_SPU_MEM_H */
