/* ==========================================================================
 *  graphics/graph3d/g3dDma.c
 *
 *  The g3d core's VIF1 DMA packet builder: an open / add / copy / close model
 *  over the frame's VIF1 chain, a flush, and the VU1 micro-program load and
 *  micro-subroutine call / continue kicks.
 *
 *  Ported from MioPan's g3dDma.c (FF2 0x19f620..0x19fd98). FF3's nine
 *  functions (0x1cced0..0x1cd17c) are FF2's without the asserts and the
 *  s_bOpened flag, and go through system.c's PACKET_BUF_MGR (cached in
 *  s_pPacketBufMgr) where FF2 called dmaVif1GetPacket / SetPacket / AddRefTag
 *  / Kick. FF3 dropped g3dDmaCancelPacket, both g3dDmaSetGsRegister overloads,
 *  g3dDmaSetGsRegisters, _DmaSend and the immediate (bImmediately) paths of
 *  the VU1 kicks. 0x1cd178 is an unreferenced empty function (jr ra; nop)
 *  with no FF2 counterpart at that place; not reconstructed.
 *
 *  PC: the packet buffer lives in emulated EE RAM; packets are written
 *  through host pointers and the manager is handed EE addresses
 *  (ReiPan_GetPs2Address), as g2d_draw.c's StartDmaDirectTrans pair does.
 *  The chain is recorded, not walked (system.c PACKET_BUF_MGR::Kick).
 * ======================================================================== */

#include "graphics/graph3d/g3dDma.h"

#include "reipan/reipan_memory.h"               // ReiPan_GetHostPointer / ReiPan_GetPs2Address
#include "sdk/libgraph.h"                       // sceGsSyncPath
#include "system/os/system.h"                   // PACKET_BUF_MGR, GetPacketBufMgr

// VIF1 opcodes (command byte in bits 31..24).
#define VIF1_FLUSH  0x11000000                  // FLUSH
#define VIF1_MSCAL  0x14000000                  // MSCAL
#define VIF1_MSCNT  0x17000000                  // MSCNT

// GetPacketBufMgr's object, fetched by each packet call on first use
// (sdata 0x372850).
static PACKET_BUF_MGR *s_pPacketBufMgr;

// Walk a VU micro-program DMA chain (CNT/REF-style tags, each followed by its
// QWC quadwords) and reference each tag's payload from the open chain, until
// an END tag (id 7). FF3 0x001cced0; from MioPan g3dDma.c.
static void _AppendVUProgTag(const u_int *pDMATag)
{
    const u_char *pDSCT;

    pDSCT = (const u_char *)pDMATag;
    if ((*(const u_int *)pDSCT & 0x70000000) != 0x70000000)
    {
        while (true)
        {
            g3dDmaAddPacket(pDSCT + 0x10, (int)*(const u_short *)pDSCT);

            // past this tag and its QWC quadwords
            pDSCT = pDSCT + ((u_int)*(const u_short *)pDSCT * 0x10 + 0x10);

            if ((*(const u_int *)pDSCT & 0x70000000) == 0x70000000)
            {
                break;
            }
        }
    }
}

// Where the next packet's data goes: one quadword past the buffer's write
// pointer (the tag slot g3dDmaClosePacket fills). FF3 0x001ccf48.
void *g3dDmaOpenPacket(void)
{
    if (s_pPacketBufMgr == nullptr)
    {
        s_pPacketBufMgr = GetPacketBufMgr();
    }
    return ReiPan_GetHostPointer(s_pPacketBufMgr->GetPacket());
}

// Close the open packet at pPacket with a CNT tag, no DIRECT, no FLUSH.
// FF3 0x001ccf78.
int g3dDmaClosePacket(const void *pPacket)
{
    if (s_pPacketBufMgr == nullptr)
    {
        s_pPacketBufMgr = GetPacketBufMgr();
    }
    s_pPacketBufMgr->SetPacket((u_int)ReiPan_GetPs2Address(pPacket), 0, 0);
    return 1;
}

// A REF tag to iQWSize quadwords at pPacket, no DIRECT, no FLUSH.
// FF3 0x001ccfc8.
int g3dDmaAddPacket(const void *pPacket, int iQWSize)
{
    if (s_pPacketBufMgr == nullptr)
    {
        s_pPacketBufMgr = GetPacketBufMgr();
    }
    s_pPacketBufMgr->AddRefTag((u_int)ReiPan_GetPs2Address(pPacket), iQWSize, 0, 0);
    return 1;
}

// Kick the chain and wait for the GS path to drain. FF3 0x001cd028.
int g3dDmaFlush(void)
{
    if (s_pPacketBufMgr == nullptr)
    {
        s_pPacketBufMgr = GetPacketBufMgr();
    }
    s_pPacketBufMgr->Kick();
    sceGsSyncPath(0, 0);
    return 1;
}

// Copy iQWSize quadwords from pPacket into a new packet, so the source can be
// reused once this returns. FF3 0x001cd068; from MioPan g3dDma.c.
int g3dDmaCopyPacket(const void *pPacket, int iQWSize)
{
    const u_int *pSrc;
    u_int       *pDest;
    int          i;

    pDest = (u_int *)g3dDmaOpenPacket();
    pSrc  = (const u_int *)pPacket;

    for (i = iQWSize; i > 0; i = i - 1)
    {
        pDest[0] = pSrc[0];
        pDest[1] = pSrc[1];
        pDest[2] = pSrc[2];
        pDest[3] = pSrc[3];
        pSrc  = pSrc + 4;
        pDest = pDest + 4;
    }

    g3dDmaClosePacket(pDest);
    return 1;
}

// The micro-program loaded last (sdata 0x370a50).
static const u_int *s_pMPGOld;

// Append the micro-program's chain unless it is resident already.
// FF3 0x001cd0c8; from MioPan g3dDma.c.
int g3dDmaLoadVu1MicroProgram(const u_int *pMPG)
{
    if (s_pMPGOld != pMPG)
    {
        _AppendVUProgTag(pMPG);
        s_pMPGOld = pMPG;
    }
    return 1;
}

// One packet: NOP, NOP, FLUSH, MSCAL uiMSAddr >> 3. FF3 0x001cd100; from
// MioPan g3dDma.c.
int g3dDmaCallVu1MicroSubroutine(u_int uiMSAddr)
{
    u_int *pQW;

    pQW = (u_int *)g3dDmaOpenPacket();
    pQW[0] = 0;
    pQW[1] = 0;
    pQW[3] = uiMSAddr >> 3 | VIF1_MSCAL;
    pQW[2] = VIF1_FLUSH;
    g3dDmaClosePacket(pQW + 4);
    return 1;
}

// { NOP, NOP, FLUSH, MSCNT } (data 0x359cf0).
alignas(16) static const u_int s_qwVif1Code_ContinueVu1MicroSubroutine[4] =
{
    0, 0, VIF1_FLUSH, VIF1_MSCNT
};

// FF3 0x001cd150; from MioPan g3dDma.c.
// PC: the quadword is host data, so its REF tag holds EE address 0; the
// chain is not walked, and nothing calls this.
int g3dDmaContinueVu1MicroSubroutine(void)
{
    g3dDmaAddPacket(s_qwVif1Code_ContinueVu1MicroSubroutine, 1);
    return 1;
}
