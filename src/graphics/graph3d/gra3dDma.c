/* ==========================================================================
 *  graphics/graph3d/gra3dDma.c
 *
 *  gra3d-layer wrappers over the g3dDma packet builder: the VIF1 UNPACK code
 *  of a VU1 upload, the VU1 micro-program load, and four ready-made "call
 *  micro subroutine" packets that differ only in MSCAL / MSCALF and the order
 *  of the FLUSH / MSCNT codes each call site needs.
 *
 *  Ported from MioPan's gra3dDma.c (FF2 gra3dDma.c). FF3's six functions
 *  (0x1ddf48..0x1de0d8) are FF2's in FF2 order; FF2's
 *  gra3dSetVif1Code_CallMicroSubroutine2 is gone, and the micro-program load
 *  tail-calls FF3's one-argument g3dDmaLoadVu1MicroProgram.
 *
 *  PC: the packets go into the recorded VIF1 chain (g3dDma.c); no VU1 runs
 *  them. The micro-program chains are ELF data the port does not load, so
 *  the load hands g3dDma nullptr (hazards.md "ELF data addresses are not in
 *  the emulated EE RAM").
 * ======================================================================== */

#include "graphics/graph3d/gra3dDma.h"

#include "graphics/graph3d/g3dDma.h"

// VIF1 opcodes (command byte in bits 31..24).
#define VIF1_STCYCLE_4_4    0x01000404          // STCYCLE CL=4 WL=4
#define VIF1_FLUSH          0x11000000          // FLUSH
#define VIF1_MSCAL          0x14000000          // MSCAL
#define VIF1_MSCALF         0x15000000          // MSCALF
#define VIF1_MSCNT          0x17000000          // MSCNT
#define VIF1_UNPACK         0x60                // UNPACK command bits

// { NOP, NOP, STCYCLE 4/4, UNPACK iCommand iSize -> iAddress }.
// FF3 0x001ddf48.
int gra3dSetVif1Code_Unpack(u_int *dest, int iAddress, int iSize, int iCommand)
{
    dest[0] = 0;
    dest[1] = 0;
    dest[3] = (u_int)iAddress | (u_int)iSize << 0x10 | ((u_int)iCommand | VIF1_UNPACK) << 0x18;
    dest[2] = VIF1_STCYCLE_4_4;
    return 1;
}

// FF3 0x001ddf80: j g3dDmaLoadVu1MicroProgram with a0 (and a1) untouched.
int gra3dDmaLoadVu1MicroProgram(u_int pMPG, int bImmediately)
{
    (void)pMPG;
    (void)bImmediately;
    // PC: the program at pMPG is ELF data that is not in the emulated EE RAM
    // (and no VU1 runs it); nullptr matches g3dDma's empty s_pMPGOld, so
    // nothing is appended.
    return g3dDmaLoadVu1MicroProgram(nullptr);
}

// { FLUSH, MSCAL uiVu1Addr >> 3, FLUSH, MSCNT }. FF3 0x001ddf98.
void gra3dCallMicroSubroutine1(u_int uiVu1Addr)
{
    u_int *pQW;

    pQW = (u_int *)g3dDmaOpenPacket();
    pQW[1] = uiVu1Addr >> 3 | VIF1_MSCAL;
    pQW[2] = VIF1_FLUSH;
    pQW[3] = VIF1_MSCNT;
    pQW[0] = VIF1_FLUSH;
    g3dDmaClosePacket(pQW + 4);
}

// { MSCAL uiVu1Addr >> 3, MSCNT, FLUSH, MSCNT }. FF3 0x001ddfe8.
void gra3dCallMicroSubroutine2(u_int uiVu1Addr)
{
    u_int *pQW;

    pQW = (u_int *)g3dDmaOpenPacket();
    pQW[0] = uiVu1Addr >> 3 | VIF1_MSCAL;
    pQW[2] = VIF1_FLUSH;
    pQW[3] = VIF1_MSCNT;
    pQW[1] = VIF1_MSCNT;
    g3dDmaClosePacket(pQW + 4);
}

// { MSCALF uiVu1Addr >> 3, FLUSH, MSCNT, MSCNT }. FF3 0x001de038.
void gra3dCallMicroSubroutine3(u_int uiVu1Addr)
{
    u_int *pQW;

    pQW = (u_int *)g3dDmaOpenPacket();
    pQW[0] = uiVu1Addr >> 3 | VIF1_MSCALF;
    pQW[1] = VIF1_FLUSH;
    pQW[3] = VIF1_MSCNT;
    pQW[2] = VIF1_MSCNT;
    g3dDmaClosePacket(pQW + 4);
}

// { MSCAL uiVu1Addr >> 3, MSCNT, FLUSH, MSCNT }: the same packet as
// gra3dCallMicroSubroutine2. FF3 0x001de088.
void gra3dCallMicroSubroutine4(u_int uiVu1Addr)
{
    u_int *pQW;

    pQW = (u_int *)g3dDmaOpenPacket();
    pQW[0] = uiVu1Addr >> 3 | VIF1_MSCAL;
    pQW[2] = VIF1_FLUSH;
    pQW[3] = VIF1_MSCNT;
    pQW[1] = VIF1_MSCNT;
    g3dDmaClosePacket(pQW + 4);
}
