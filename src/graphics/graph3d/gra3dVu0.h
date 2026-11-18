/* ==========================================================================
 *  graphics/graph3d/gra3dVu0.h
 *
 *  The VU0 constant registers (FF3 0x1e6ae8..0x1e6b98, after gra3dTRI2.c as
 *  FF2's gra3dVu0.c is). FF2's file emitted only fixed_array templates; FF3's
 *  holds the vf1 / vf2 setup and a two-entry table that shadows them.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DVU0_H
#define _GRAPHICS_GRAPH3D_GRA3DVU0_H

#include "sdk/libvu0.h"                         // sceVu0FVECTOR

// The constant-vector table, entry idx - 1 for vf1 / vf2 (data 0x328fa0):
// (1, 1, 1, -1) and (1, 0, 0, 0) until Vu0SetConstVector changes one.
extern sceVu0FVECTOR g_avVu0ConstVector[2];

void   Vu0InitConstRegisters(void);
void   Vu0SetConstVector(int idx, float *v);
void   Vu0GetConstVector(int idx, float *v);
float *Vu0GetConstVectorPtr(int idx);

#endif /* _GRAPHICS_GRAPH3D_GRA3DVU0_H */
