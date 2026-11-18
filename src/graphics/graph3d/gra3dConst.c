/* ==========================================================================
 *  graphics/graph3d/gra3dConst.c
 *
 *  The graph3d engine's file-scope constants (FF2 gra3dConst.c,
 *  0x1b57f8..0x1b59f0). In FF3 the file's code is only the static
 *  initialisation of g_xv0000: __static_initialization_and_destruction_0
 *  (0x1ddec0) zeroes it on (1, 0xffff), called by the .ctors entry 0x1ddef8.
 *  FF2's g_xmatConvert* objects are not built here in FF3.
 *
 *  PC: a zero-initialised global needs no constructor.
 * ======================================================================== */

#include "graphics/graph3d/gra3dConst.h"

// FF3 data 0x328840; zeroed by FF3 0x001ddec0 (called from FF3 0x001ddef8).
float g_xv0000[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
