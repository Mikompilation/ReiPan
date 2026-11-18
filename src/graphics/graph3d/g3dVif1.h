/* ==========================================================================
 *  graphics/graph3d/g3dVif1.h
 *
 *  VIF1 register shadow of the g3d core (FF2 g3dVif1.c): snapshot the VIF1
 *  registers, push a batch of register-set commands as a VIF1 packet, and
 *  UNPACK a quadword buffer into VU1 memory.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DVIF1_H
#define _GRAPHICS_GRAPH3D_G3DVIF1_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// One VIF1 command with up to four data words (FF2 layout; gra3dInit hands
// g3dVif1SetRegister six of them from data 0x35a3c0, 0x14 bytes apart).
struct G3DVIF1CMDDATA                   /* 0x14 */
{
    /* 0x00 */ u_int uiCmd;
    /* 0x04 */ u_int auiSubPacket[4];
};
static_assert(sizeof(G3DVIF1CMDDATA) == 0x14, "G3DVIF1CMDDATA is 0x14 bytes");
static_assert(offsetof(G3DVIF1CMDDATA, auiSubPacket) == 0x04, "G3DVIF1CMDDATA.auiSubPacket");

// Most sub-packet words one register command carries (STROW / STCOL).
#define G3D_MAX_VIF1SUBPACKETLENGTH  4

// Copy the 22 VIF1 hardware registers into the shadow. FF3 0x1cfa28.
void g3dVif1Init(void);
// Apply iNum register-set commands to the shadow and send them as one VIF1
// packet (behind NOP, FLUSH). FF3 0x1cfa60.
void g3dVif1SetRegister(const G3DVIF1CMDDATA *aCmd, int iNum);
// UNPACK V4-32 iQWSize quadwords from pData to VU1 address iVu1MemAddress;
// returns 1. FF3 0x1cfbe0.
int  g3dVif1Unpack(int iVu1MemAddress, const void *pData, int iQWSize);

#endif /* _GRAPHICS_GRAPH3D_G3DVIF1_H */
