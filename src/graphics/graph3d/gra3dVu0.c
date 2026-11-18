/* ==========================================================================
 *  graphics/graph3d/gra3dVu0.c
 *
 *  The VU0 constant registers (FF3 0x1e6ae8..0x1e6b98). In FF2 this object
 *  (gra3dVu0.c, after gra3dTRI2.c) emitted only fixed_array templates; FF3's
 *  has four small functions:
 *    - Vu0InitConstRegisters (gra3dInit) loads vf1 = (1, 1, 1, -1) and
 *      vf2 = (1, 0, 0, 0) from a stack copy of rodata 0x35aa40. The macro
 *      code of g3dGeom.c / gra3dMirror.c reads vf1 as "sum xyz" in dot
 *      products (vmaddz.x vf, vf1, vf);
 *    - a table at data 0x328fa0 with the same two vectors, written by
 *      Vu0SetConstVector (gra3dDrawMirrorView flips entry 2 to (-1, 0, 0, 0)
 *      and back around the mirrored draw) and read by Vu0GetConstVector /
 *      Vu0GetConstVectorPtr, which nothing in FF3 calls. Set does not load
 *      the register: no FF3 code reads the table, so it only records.
 *
 *  PC: there is no VU0 register file; every reader of vf1 / vf2 writes the
 *  constant out (g3dGeom.c, gra3dMirror.c), so Vu0InitConstRegisters has
 *  nothing to load. The table is kept as FF3 has it.
 * ======================================================================== */

#include "graphics/graph3d/gra3dVu0.h"

#include <string.h>                             // memcpy

// FF3 data 0x328fa0 (EE: 16-byte aligned, two quadwords).
sceVu0FVECTOR g_avVu0ConstVector[2] = {
    { 1.0f, 1.0f, 1.0f, -1.0f },
    { 1.0f, 0.0f, 0.0f,  0.0f },
};
static_assert(sizeof(g_avVu0ConstVector) == 0x20, "g_avVu0ConstVector is two quadwords");

// Load vf1 = (1, 1, 1, -1) and vf2 = (1, 0, 0, 0) (rodata 0x35aa40, copied
// to the stack, then lqc2). PC: no VU0 registers; the readers inline them.
// FF3 0x001e6ae8.
void Vu0InitConstRegisters(void)
{
}

// Copy v into the table entry for vf<idx>; only idx 1 and 2 are stored.
// FF3 0x001e6b18.
void Vu0SetConstVector(int idx, float *v)
{
    if ((unsigned int)(idx - 1) < 2) {
        memcpy(g_avVu0ConstVector[idx - 1], v, sizeof(sceVu0FVECTOR));
    }
}

// Copy the table entry for vf<idx> to v; only idx 1 and 2 are read.
// No callers in FF3. FF3 0x001e6b48.
void Vu0GetConstVector(int idx, float *v)
{
    if ((unsigned int)(idx - 1) < 2) {
        memcpy(v, g_avVu0ConstVector[idx - 1], sizeof(sceVu0FVECTOR));
    }
}

// The table entry for vf<idx>, unchecked (FF3 computes 0x328f90 + idx * 16).
// No callers in FF3. FF3 0x001e6b78.
float *Vu0GetConstVectorPtr(int idx)
{
    return g_avVu0ConstVector[idx - 1];
}
