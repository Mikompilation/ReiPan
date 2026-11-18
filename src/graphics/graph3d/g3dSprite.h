/* ==========================================================================
 *  graphics/graph3d/g3dSprite.h
 *
 *  CSprite (FF2 g3dSprite.cpp; RTTI typeinfo 0x372a58, vtable 0x341e48): one
 *  screen-space quad drawn as a GS SPRITE. Create() bakes a G3DSPRITEDATA into
 *  a ready GIF packet -- VIF1 FLUSH + DIRECT, a PACKED GIFTAG, the RGBAQ item
 *  and the two corners as GS XYZ -- in the untextured (RGBAQ, XYZF2 x2) or
 *  textured (RGBAQ, ST + XYZF2 per corner) shape; Draw() binds the texture
 *  and copies the packet into the g3d DMA chain. CRenderTarget::Clear is the
 *  only user.
 *
 *  FF3 0x1cf348..0x1cf838 (ctors, Create, Draw) + the linkonce dtors
 *  0x2bbd70 / 0x2bbd80. FF3 gave the class a virtual dtor, so every FF2
 *  offset moves up 0x10.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DSPRITE_H
#define _GRAPHICS_GRAPH3D_G3DSPRITE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/sce_gs.h"                 // sceGsXyz, sceGsSt, sceGifPack*, SCEGIFTAG_EOP
#include "graphics/graph3d/g3dTexture.h" // CTexture

typedef u_int G3DCOLOR;                 // RGBA8, R in the low byte

struct G3DFRECT                         /* 0x10 */
{
    /* 0x0 */ float fLeft;
    /* 0x4 */ float fTop;
    /* 0x8 */ float fRight;
    /* 0xc */ float fBottom;
};
static_assert(sizeof(G3DFRECT) == 0x10, "G3DFRECT is 0x10 bytes");

// CSprite::Create's argument. 16-byte aligned on the EE: CSprite keeps its
// copy at +0x10 after the vptr, and CRenderTarget::Clear memsets 0x30.
struct alignas(16) G3DSPRITEDATA        /* 0x30 */
{
    /* 0x00 */ G3DFRECT Rect;           // screen pixels
    /* 0x10 */ G3DCOLOR Color;
    /* 0x14 */ float    fZ;             // 0 near .. 1 far (g3dCalcGsPrimitiveCoord)
    /* 0x18 */ sceGsSt  StLT;
    /* 0x20 */ sceGsSt  StRB;
};
static_assert(sizeof(G3DSPRITEDATA) == 0x30, "G3DSPRITEDATA is 0x30 bytes");
static_assert(offsetof(G3DSPRITEDATA, Color) == 0x10, "G3DSPRITEDATA.Color");
static_assert(offsetof(G3DSPRITEDATA, fZ) == 0x14, "G3DSPRITEDATA.fZ");
static_assert(offsetof(G3DSPRITEDATA, StLT) == 0x18, "G3DSPRITEDATA.StLT");
static_assert(offsetof(G3DSPRITEDATA, StRB) == 0x20, "G3DSPRITEDATA.StRB");

// The VIF1 code quadword + GIFTAG in front of both packet shapes.
struct PACKETHEADER                     /* 0x20 */
{
    /* 0x00 */ qword         qwVifCode;
    /* 0x10 */ SCEGIFTAG_EOP Gt;
};
static_assert(sizeof(PACKETHEADER) == 0x20, "PACKETHEADER is 0x20 bytes");
static_assert(offsetof(PACKETHEADER, Gt) == 0x10, "PACKETHEADER.Gt");

// PACKED body of an untextured sprite.
struct GIFPACKET_WITHOUTTEXTURE         /* 0x30 */
{
    /* 0x00 */ sceGifPackRgbaq Rgbaq;
    /* 0x10 */ sceGifPackXyzf  XyzfLT;
    /* 0x20 */ sceGifPackXyzf  XyzfRB;
};
static_assert(sizeof(GIFPACKET_WITHOUTTEXTURE) == 0x30, "GIFPACKET_WITHOUTTEXTURE is 0x30 bytes");
static_assert(offsetof(GIFPACKET_WITHOUTTEXTURE, XyzfLT) == 0x10, "GIFPACKET_WITHOUTTEXTURE.XyzfLT");
static_assert(offsetof(GIFPACKET_WITHOUTTEXTURE, XyzfRB) == 0x20, "GIFPACKET_WITHOUTTEXTURE.XyzfRB");

// PACKED body of a textured sprite.
struct GIFPACKET_WITHTEXTURE            /* 0x50 */
{
    /* 0x00 */ sceGifPackRgbaq Rgbaq;
    /* 0x10 */ sceGifPackSt    StLT;
    /* 0x20 */ sceGifPackXyzf  XyzfLT;
    /* 0x30 */ sceGifPackSt    StRB;
    /* 0x40 */ sceGifPackXyzf  XyzfRB;
};
static_assert(sizeof(GIFPACKET_WITHTEXTURE) == 0x50, "GIFPACKET_WITHTEXTURE is 0x50 bytes");
static_assert(offsetof(GIFPACKET_WITHTEXTURE, StLT) == 0x10, "GIFPACKET_WITHTEXTURE.StLT");
static_assert(offsetof(GIFPACKET_WITHTEXTURE, XyzfLT) == 0x20, "GIFPACKET_WITHTEXTURE.XyzfLT");
static_assert(offsetof(GIFPACKET_WITHTEXTURE, StRB) == 0x30, "GIFPACKET_WITHTEXTURE.StRB");
static_assert(offsetof(GIFPACKET_WITHTEXTURE, XyzfRB) == 0x40, "GIFPACKET_WITHTEXTURE.XyzfRB");

// layout: host -- vtable (EE vptr 4 bytes, host 8) and a pointer member; the
// EE offsets are noted per member. EE size 0x90.
class CSprite
{
public:
    CSprite();                          // FF3 0x001cf378 (base twin 0x001cf348)
    virtual ~CSprite() {}               // FF3 0x002bbd70 (linkonce; deleting 0x002bbd80)

    int  Create(const G3DSPRITEDATA *pSpriteData, CTexture *pTexture);  // FF3 0x001cf3a8
    void Draw();                        // FF3 0x001cf5b0

private:
    G3DSPRITEDATA   m_SpriteData;       // EE +0x10
    PACKETHEADER    m_Header;           // EE +0x40
    CTexture       *m_pTexture;         // EE +0x60
    sceGifPackRgbaq m_Rgbaq;            // EE +0x64
    sceGsXyz        m_GsXyzLT;          // EE +0x78
    sceGsXyz        m_GsXyzRB;          // EE +0x80
    int             m_iQWSizePacket;    // EE +0x88, header included
};

#endif /* _GRAPHICS_GRAPH3D_G3DSPRITE_H */
