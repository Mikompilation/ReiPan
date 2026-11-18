/* ==========================================================================
 *  graphics/graph3d/gra3dDma.h
 *
 *  gra3d's VIF1 code helpers over the g3dDma packet builder (FF2
 *  gra3dDma.c): the UNPACK code of a VU1 upload, the VU1 micro-program load
 *  and four "call micro subroutine" packets. FF3 0x1ddf48..0x1de0d8.
 *
 *  VU1 addresses (uiVu1Addr) are VU1 micro-memory byte addresses (the MSCAL
 *  operand is addr >> 3); pMPG is the EE data address of a micro-program DMA
 *  chain. Both stay integers on PC (open-items.md "VU1 program / subroutine
 *  addresses").
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DDMA_H
#define _GRAPHICS_GRAPH3D_GRA3DDMA_H

#include "sdk/scetypes.h"

// Write a STCYCLE + UNPACK code pair (iSize quadwords to VU1 address
// iAddress, format iCommand | 0x60) into the quadword at dest; returns 1.
// FF3 0x1ddf48.
int gra3dSetVif1Code_Unpack(u_int *dest, int iAddress, int iSize, int iCommand);

// Append the VU1 micro-program at EE address pMPG to the chain unless it is
// loaded already; returns 1. FF3 0x1ddf80 (a tail call; bImmediately is
// passed through and unused, as FF3's g3dDmaLoadVu1MicroProgram takes one).
int gra3dDmaLoadVu1MicroProgram(u_int pMPG, int bImmediately);

// A packet { FLUSH, MSCAL, FLUSH, MSCNT }. FF3 0x1ddf98.
void gra3dCallMicroSubroutine1(u_int uiVu1Addr);
// A packet { MSCAL, MSCNT, FLUSH, MSCNT }. FF3 0x1ddfe8.
void gra3dCallMicroSubroutine2(u_int uiVu1Addr);
// A packet { MSCALF, FLUSH, MSCNT, MSCNT }. FF3 0x1de038.
void gra3dCallMicroSubroutine3(u_int uiVu1Addr);
// A packet { MSCAL, MSCNT, FLUSH, MSCNT } (Subroutine2's twin). FF3 0x1de088.
void gra3dCallMicroSubroutine4(u_int uiVu1Addr);

#endif /* _GRAPHICS_GRAPH3D_GRA3DDMA_H */
