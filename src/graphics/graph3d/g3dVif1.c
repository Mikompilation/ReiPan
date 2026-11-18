/* ==========================================================================
 *  graphics/graph3d/g3dVif1.c
 *
 *  VIF1 register shadow of the g3d core. g3dVif1Init copies the 22 VIF1
 *  hardware registers (0x10003c00, 0x10 apart) into s_Vif1RegisterLayout;
 *  g3dVif1SetRegister applies a list of register-set commands (STCYCLE,
 *  OFFSET, BASE, ITOP, STMOD, MARK, STMASK, STROW, STCOL, FLUSH) to that
 *  shadow and sends them as one VIF1 packet; g3dVif1Unpack builds an UNPACK
 *  V4-32 packet that streams quadwords into VU1 memory.
 *
 *  Ported from MioPan's g3dVif1.c (FF2 0x1a5880..0x1a5db0). FF3's five
 *  functions (0x1cf870..0x1cfc80) are FF2's without the asserts; the command
 *  table is static data (0x359d10) where FF2 built it on the stack, and
 *  g3dVif1SetRegister no longer cancels a packet that ends up empty.
 * ======================================================================== */

#include "graphics/graph3d/g3dVif1.h"

#include "graphics/graph3d/g3dDma.h"            // g3dDmaOpenPacket, g3dDmaClosePacket
#include "sdk/eeregs.h"                         // g_pcport_REG_VIF1_REGISTER_FILE

// The VIF1 register file as g3dVif1Init last read it plus every register
// g3dVif1SetRegister set since (data 0x3eb310). Index = (address -
// 0x10003c00) / 0x10: 3 MARK, 4 CYCLE, 5 MODE, 7 MASK, 9 ITOPS, 10 BASE,
// 11 OFST, 14..17 R0..R3, 18..21 C0..C3.
struct G3DVIF1REGISTERLAYOUT            /* 0x58 */
{
    /* 0x00 */ u_int auiReg[22];
};
static_assert(sizeof(G3DVIF1REGISTERLAYOUT) == 0x58, "G3DVIF1REGISTERLAYOUT is 0x58 bytes");

// One settable register: the VIF code's opcode, the hardware register it
// sets, how many data words follow the code, and the shadow words it updates.
// FF3 entries are 0x1c bytes (4-byte pointers); this one only lives in the
// table and on the stack, never in EE memory, so it holds host pointers.
// layout: host -- pointers; FF3 0x1c bytes, never overlaid on game data.
struct _VIF1CMDINFO
{
    u_int           uiCmd;
    volatile u_int *pqwSubstantialRegister;
    int             iLengthSubPacket;
    u_int          *apiLocalRegister[4];
};

static G3DVIF1REGISTERLAYOUT s_Vif1RegisterLayout;

#define VIF1REG(uiAddress)  (&g_pcport_REG_VIF1_REGISTER_FILE[((uiAddress) - 0x10003c00) / 0x10][0])
#define LOCAL(iReg)         (&s_Vif1RegisterLayout.auiReg[iReg])

// The register-set commands g3dVif1SetRegister understands (data 0x359d10).
static const _VIF1CMDINFO s_aVif1CmdInfo[10] =
{
    { 0x01, VIF1REG(0x10003c40), 0, { LOCAL(4) } },                             // STCYCLE
    { 0x02, VIF1REG(0x10003cb0), 0, { LOCAL(11) } },                            // OFFSET
    { 0x03, VIF1REG(0x10003ca0), 0, { LOCAL(10) } },                            // BASE
    { 0x04, VIF1REG(0x10003cd0), 0, { LOCAL(9) } },                             // ITOP
    { 0x05, VIF1REG(0x10003c50), 0, { LOCAL(5) } },                             // STMOD
    { 0x07, VIF1REG(0x10003c30), 0, { LOCAL(3) } },                             // MARK
    { 0x20, VIF1REG(0x10003c70), 1, { LOCAL(7) } },                             // STMASK
    { 0x30, VIF1REG(0x10003d00), 4, { LOCAL(14), LOCAL(15), LOCAL(16), LOCAL(17) } }, // STROW
    { 0x31, VIF1REG(0x10003d40), 4, { LOCAL(18), LOCAL(19), LOCAL(20), LOCAL(21) } }, // STCOL
    { 0x11, nullptr,             0, { nullptr } },                              // FLUSH
};

#undef LOCAL
#undef VIF1REG

// Copy the table entry whose opcode is pVCD's VIF code (its top byte; a code
// with the interrupt bit set matches none) to *pRet; no match leaves *pRet.
// FF3 0x001cf870; from MioPan g3dVif1.c.
// callcheck: memcpy (the struct copy of the matching entry)
static void _GetVif1CmdInfo(_VIF1CMDINFO *pRet, const G3DVIF1CMDDATA *pVCD)
{
    int i;

    for (i = 0; i < 10; i = i + 1)
    {
        if ((s_aVif1CmdInfo[i].uiCmd & 0x7f) == (pVCD->uiCmd >> 24))
        {
            *pRet = s_aVif1CmdInfo[i];
            return;
        }
    }
}

// Store one command's value in the shadow word(s) pVCI names. Returns 1 for
// a known opcode (FLUSH has no shadow), 0 otherwise.
// FF3 0x001cf910; from MioPan g3dVif1.c.
static int _UpdateValue(_VIF1CMDINFO *pVCI, const G3DVIF1CMDDATA *pVCD)
{
    switch (pVCI->uiCmd & 0x7f)
    {
    case 0x01:      // STCYCLE: CL, WL (the code's two low bytes)
        ((u_char *)pVCI->apiLocalRegister[0])[0] = ((const u_char *)&pVCD->uiCmd)[0];
        ((u_char *)pVCI->apiLocalRegister[0])[1] = ((const u_char *)&pVCD->uiCmd)[1];
        return 1;
    case 0x02:      // OFFSET: 10 bits
        *pVCI->apiLocalRegister[0] = (*pVCI->apiLocalRegister[0] & 0xfffffc00) | (pVCD->uiCmd & 0x3ff);
        return 1;
    case 0x03:      // BASE
    case 0x04:      // ITOP: the low byte into the 10-bit field
        *pVCI->apiLocalRegister[0] = (*pVCI->apiLocalRegister[0] & 0xfffffc00) | (u_char)pVCD->uiCmd;
        return 1;
    case 0x05:      // STMOD: 2 bits
        *pVCI->apiLocalRegister[0] = (*pVCI->apiLocalRegister[0] & 0xfffffffc) | (pVCD->uiCmd & 3);
        return 1;
    case 0x07:      // MARK: 16 bits
        *(u_short *)pVCI->apiLocalRegister[0] = (u_short)pVCD->uiCmd;
        return 1;
    case 0x11:      // FLUSH
        return 1;
    case 0x20:      // STMASK: FF2's code keeps only 2 bits of the data word
        *pVCI->apiLocalRegister[0] = (*pVCI->apiLocalRegister[0] & 0xfffffffc) | (pVCD->auiSubPacket[0] & 3);
        return 1;
    case 0x30:      // STROW: R0..R3
    case 0x31:      // STCOL: C0..C3
        *pVCI->apiLocalRegister[0] = pVCD->auiSubPacket[0];
        *pVCI->apiLocalRegister[1] = pVCD->auiSubPacket[1];
        *pVCI->apiLocalRegister[2] = pVCD->auiSubPacket[2];
        *pVCI->apiLocalRegister[3] = pVCD->auiSubPacket[3];
        return 1;
    default:
        return 0;
    }
}

// Snapshot the VIF1 registers (word 0 of each of the 22 quadwords).
// FF3 0x001cfa28; from MioPan g3dVif1.c.
void g3dVif1Init(void)
{
    int i;

    for (i = 0; i < 22; i = i + 1)
    {
        s_Vif1RegisterLayout.auiReg[i] = g_pcport_REG_VIF1_REGISTER_FILE[i][0];
    }
}

// Build one VIF1 packet: NOP, FLUSH (data 0x370aa0), then each known command
// with its data words, padded with NOPs to the next quadword (a whole one
// when it is aligned already). Only closed when a command went in; FF3 leaves
// an empty packet open where FF2 cancelled it.
// FF3 0x001cfa60; from MioPan g3dVif1.c.
void g3dVif1SetRegister(const G3DVIF1CMDDATA *aCmd, int iNum)
{
    static const u_int s_auiDefaultVif1Code[2] = { 0x00000000, 0x11000000 };
    u_int       *rauiPacket;
    int          iWSize;
    int          iFraction;
    int          i;
    int          j;
    _VIF1CMDINFO VCI = {};                      // PC: FF3 leaves it unset when no entry matches

    if (iNum <= 0)
    {
        return;
    }

    rauiPacket = (u_int *)g3dDmaOpenPacket();
    rauiPacket[0] = s_auiDefaultVif1Code[0];
    rauiPacket[1] = s_auiDefaultVif1Code[1];
    iWSize = 2;

    for (i = 0; i < iNum; i = i + 1)
    {
        _GetVif1CmdInfo(&VCI, &aCmd[i]);
        if (_UpdateValue(&VCI, &aCmd[i]) != 0)
        {
            rauiPacket[iWSize] = aCmd[i].uiCmd;
            iWSize = iWSize + 1;
            for (j = 0; j < VCI.iLengthSubPacket; j = j + 1)
            {
                rauiPacket[iWSize] = aCmd[i].auiSubPacket[j];
                iWSize = iWSize + 1;
            }
        }
    }

    if (iWSize > 2)
    {
        for (iFraction = 4 - (iWSize & 3); iFraction > 0; iFraction = iFraction - 1)
        {
            rauiPacket[iWSize] = 0;
            iWSize = iWSize + 1;
        }
        g3dDmaClosePacket(rauiPacket + iWSize);
    }
}

// One packet: NOP, NOP, STCYCLE CL=4 WL=4, UNPACK V4-32 (num = iQWSize,
// addr = iVu1MemAddress), then the quadwords.
// FF3 0x001cfbe0; from MioPan g3dVif1.c.
int g3dVif1Unpack(int iVu1MemAddress, const void *pData, int iQWSize)
{
    u_int       *rQW;
    const u_int *pSrc;
    int          i;

    rQW = (u_int *)g3dDmaOpenPacket();
    rQW[0] = 0;
    rQW[1] = 0;
    rQW[2] = 0x01000404;
    rQW[3] = (u_int)iVu1MemAddress | (u_int)iQWSize << 16 | 0x6c000000;

    pSrc = (const u_int *)pData;
    for (i = 0; i < iQWSize; i = i + 1)
    {
        rQW[4 + i * 4 + 0] = pSrc[i * 4 + 0];
        rQW[4 + i * 4 + 1] = pSrc[i * 4 + 1];
        rQW[4 + i * 4 + 2] = pSrc[i * 4 + 2];
        rQW[4 + i * 4 + 3] = pSrc[i * 4 + 3];
    }

    g3dDmaClosePacket(rQW + iQWSize * 4 + 4);
    return 1;
}
