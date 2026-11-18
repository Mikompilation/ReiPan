/* ==========================================================================
 *  graphics/graph3d/g3dDma.h
 *
 *  The g3d core's VIF1 DMA packet builder (FF2 g3dDma.c): open a packet in
 *  the frame's VIF1 chain, reference or copy quadwords into it, close it,
 *  flush the chain, and the VU1 micro-program / micro-subroutine kicks.
 *  FF3 0x1cced0..0x1cd17c; FF3 builds on system.c's PACKET_BUF_MGR where FF2
 *  used the dmaVif1 ring.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DDMA_H
#define _GRAPHICS_GRAPH3D_G3DDMA_H

#include "sdk/scetypes.h"

// ---- packet builder -------------------------------------------------------

// Open a packet in the current VIF1 chain: returns where to write it (host
// pointer to the packet buffer). FF3 0x1ccf48.
void *g3dDmaOpenPacket(void);
// Close the packet opened last at pPacket (its end); returns 1. FF3 0x1ccf78.
int g3dDmaClosePacket(const void *pPacket);
// Reference iQWSize quadwords at pPacket from the chain (a REF tag); returns 1.
// FF3 0x1ccfc8.
int g3dDmaAddPacket(const void *pPacket, int iQWSize);
// Send the chain built so far and wait for the GS path; returns 1.
// FF3 0x1cd028.
int g3dDmaFlush(void);
// Copy iQWSize quadwords into a new packet of the current chain; returns 1.
// FF3 0x1cd068.
int g3dDmaCopyPacket(const void *pPacket, int iQWSize);

// ---- VU1 micro program / subroutine kicks ---------------------------------

// Append the VU1 micro-program DMA chain at pMPG (its tags' payloads), unless
// it is the program loaded last; returns 1. FF3 0x1cd0c8 (FF2's bImmediately
// path is gone).
int g3dDmaLoadVu1MicroProgram(const u_int *pMPG);
// A packet FLUSH + MSCAL (uiMSAddr >> 3): run the VU1 micro-subroutine at
// uiMSAddr (VU1 micro memory address); returns 1. FF3 0x1cd100 (FF2's
// deferred branch only; nothing calls it).
int g3dDmaCallVu1MicroSubroutine(u_int uiMSAddr);
// Reference the static FLUSH + MSCNT quadword: resume the VU1 micro-
// subroutine; returns 1. FF3 0x1cd150 (FF2 took an unused pMS; nothing calls it).
int g3dDmaContinueVu1MicroSubroutine(void);

#endif /* _GRAPHICS_GRAPH3D_G3DDMA_H */
