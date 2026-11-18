/* ==========================================================================
 *  graphics/graph3d/gra3dConst.h
 *
 *  The graph3d engine's file-scope constants (FF2 gra3dConst.c). FF3's
 *  file holds only the writable zero vector's static init (0x1ddec0..
 *  0x1ddf48); the other FF2 constants are inlined by the callers so far.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DCONST_H
#define _GRAPHICS_GRAPH3D_GRA3DCONST_H

// The writable zero vector (data 0x328840), zeroed by the static init.
extern float g_xv0000[4];

#endif /* _GRAPHICS_GRAPH3D_GRA3DCONST_H */
