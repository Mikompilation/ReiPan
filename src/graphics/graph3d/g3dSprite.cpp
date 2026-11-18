// graphics/graph3d/g3dSprite.cpp -- CSprite, FF3 0x1cf348..0x1cf838 (+ the
// linkonce dtors 0x2bbd70 / 0x2bbd80, inline in the header).
// From MioPan's g3dSprite.cpp, checked against FF3: the GIFTAG words were
// swapped there (FF3 writes the tag to +0x50 and the REGS list to +0x58), and
// FF3 adds the vptr, so the members sit 0x10 higher.

#include <string.h>                             // memset

#include "graphics/graph3d/g3dSprite.h"
#include "graphics/graph3d/g3dCore.h"           // g3dSetTexture, g3dCalcGsPrimitiveCoord
#include "graphics/graph3d/g3dDma.h"            // g3dDmaOpenPacket, g3dDmaClosePacket
#include "sdk/libgraph.h"                       // SCE_GS_*

#define VIF1_FLUSH  0x11000000                  // VIF code FLUSH
#define VIF1_DIRECT 0x50000000                  // VIF code DIRECT, IMMEDIATE = quadwords

// FF3 0x001cf378 (base-object twin 0x001cf348): the vptr and a zeroed header.
CSprite::CSprite()
{
    memset(&m_Header, 0, sizeof(m_Header));
}

// Bake the packet: untextured = RGBAQ + XYZF2 x2 (5 quadwords with the header),
// textured = RGBAQ + (ST, XYZF2) per corner (7). The GIFTAG is NLOOP 1, EOP,
// PRE with PRIM = SPRITE (| TME), PACKED. FF3 0x001cf3a8
int CSprite::Create(const G3DSPRITEDATA *pSpriteData, CTexture *pTexture)
{
    int    iPrim;
    int    iNreg;
    int    iReg1;
    u_long lReg3;
    u_long lReg4;
    float  vScreenCoordLT[4];
    float  vScreenCoordRB[4];

    lReg3 = 0;
    lReg4 = 0;
    m_SpriteData = *pSpriteData;
    m_pTexture   = pTexture;

    if (pTexture == nullptr)
    {
        iPrim           = SCE_GS_PRIM_SPRITE;           // 6
        m_iQWSizePacket = 5;
        iNreg           = 3;
        iReg1           = SCE_GS_XYZF2;                 // 4
    }
    else
    {
        iPrim           = SCE_GS_PRIM_SPRITE | 0x10;    // TME
        m_iQWSizePacket = 7;
        iNreg           = 5;
        iReg1           = SCE_GS_ST;                    // 2
        lReg3           = SCE_GS_ST;
        lReg4           = SCE_GS_XYZF2;
    }

    // NOP, NOP, FLUSH, DIRECT over the rest of the packet.
    m_Header.qwVifCode[0] = 0;
    m_Header.qwVifCode[1] = 0;
    m_Header.qwVifCode[2] = VIF1_FLUSH;
    m_Header.qwVifCode[3] = (m_iQWSizePacket - 1) | VIF1_DIRECT;

    m_Rgbaq.R = (u_char)(m_SpriteData.Color);
    m_Rgbaq.G = (u_char)(m_SpriteData.Color >> 8);
    m_Rgbaq.B = (u_char)(m_SpriteData.Color >> 16);
    m_Rgbaq.A = (u_char)(m_SpriteData.Color >> 24);

    // NLOOP 1, EOP, PRE, PRIM, FLG PACKED, NREG; REGS: RGBAQ, then the corners.
    m_Header.Gt.lTag  = (m_Header.Gt.lTag & 0x7fffffff8000ull)
                      | 0x400000008001ull
                      | (u_long)iPrim << 47
                      | (u_long)iNreg << 60;
    m_Header.Gt.lRegs = (m_Header.Gt.lRegs & 0xfffffffffff00000ull)
                      | SCE_GS_RGBAQ
                      | (u_long)iReg1 << 4
                      | (u_long)SCE_GS_XYZF2 << 8
                      | (u_long)lReg3 << 12
                      | (u_long)lReg4 << 16;

    vScreenCoordLT[0] = m_SpriteData.Rect.fLeft;
    vScreenCoordLT[1] = m_SpriteData.Rect.fTop;
    vScreenCoordLT[2] = m_SpriteData.fZ;
    vScreenCoordLT[3] = 0.0f;
    g3dCalcGsPrimitiveCoord(&m_GsXyzLT, vScreenCoordLT);

    vScreenCoordRB[0] = m_SpriteData.Rect.fRight;
    vScreenCoordRB[1] = m_SpriteData.Rect.fBottom;
    vScreenCoordRB[2] = m_SpriteData.fZ;
    vScreenCoordRB[3] = 0.0f;
    g3dCalcGsPrimitiveCoord(&m_GsXyzRB, vScreenCoordRB);

    return 1;
}

// Bind the texture (or none), copy the header and fill the PACKED items into
// a new packet of the g3d chain; Q is 1.0 (sdata 0x370a9c). FF3 0x001cf5b0
void CSprite::Draw()
{
    PACKETHEADER *pHeader;

    g3dSetTexture(0, m_pTexture);
    pHeader  = (PACKETHEADER *)g3dDmaOpenPacket();
    *pHeader = m_Header;

    if (m_pTexture == nullptr)
    {
        GIFPACKET_WITHOUTTEXTURE *pGifPacket = (GIFPACKET_WITHOUTTEXTURE *)(pHeader + 1);

        pGifPacket->Rgbaq = m_Rgbaq;
        memset(&pGifPacket->XyzfLT, 0, sizeof(pGifPacket->XyzfLT));
        pGifPacket->XyzfLT.X = (u_short)m_GsXyzLT.X;
        pGifPacket->XyzfLT.Y = (u_short)m_GsXyzLT.Y;
        pGifPacket->XyzfLT.Z = (u_int)m_GsXyzLT.Z << 4;
        memset(&pGifPacket->XyzfRB, 0, sizeof(pGifPacket->XyzfRB));
        pGifPacket->XyzfRB.X = (u_short)m_GsXyzRB.X;
        pGifPacket->XyzfRB.Y = (u_short)m_GsXyzRB.Y;
        pGifPacket->XyzfRB.Z = (u_int)m_GsXyzRB.Z << 4;
    }
    else
    {
        GIFPACKET_WITHTEXTURE *pGifPacket = (GIFPACKET_WITHTEXTURE *)(pHeader + 1);

        pGifPacket->Rgbaq = m_Rgbaq;
        memset(&pGifPacket->StLT, 0, sizeof(pGifPacket->StLT));
        pGifPacket->StLT.S = m_SpriteData.StLT.S;
        pGifPacket->StLT.T = m_SpriteData.StLT.T;
        pGifPacket->StLT.Q = 1.0f;
        memset(&pGifPacket->XyzfLT, 0, sizeof(pGifPacket->XyzfLT));
        pGifPacket->XyzfLT.X = (u_short)m_GsXyzLT.X;
        pGifPacket->XyzfLT.Y = (u_short)m_GsXyzLT.Y;
        pGifPacket->XyzfLT.Z = (u_int)m_GsXyzLT.Z << 4;
        memset(&pGifPacket->StRB, 0, sizeof(pGifPacket->StRB));
        pGifPacket->StRB.S = m_SpriteData.StRB.S;
        pGifPacket->StRB.T = m_SpriteData.StRB.T;
        pGifPacket->StRB.Q = 1.0f;
        memset(&pGifPacket->XyzfRB, 0, sizeof(pGifPacket->XyzfRB));
        pGifPacket->XyzfRB.X = (u_short)m_GsXyzRB.X;
        pGifPacket->XyzfRB.Y = (u_short)m_GsXyzRB.Y;
        pGifPacket->XyzfRB.Z = (u_int)m_GsXyzRB.Z << 4;
    }

    g3dDmaClosePacket((qword *)pHeader + m_iQWSizePacket);
}
