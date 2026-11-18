/* ==========================================================================
 *  graphics/graph3d/g3dTexture.h
 *
 *  The g3d texture resource (FF2 g3dTexture.cpp): CTexture holds the GS
 *  TEX0/TEX1 pair g3dSetTexture binds, plus an optional image it uploads in
 *  PreLoad. FF3's CTexture (RTTI typeinfo 0x341e68, vtable 0x341e58: +0/+4
 *  the destructors, +8 PreLoad = 0x1cf838) sits in the unassigned run after
 *  g3dSprite.cpp (0x1cf838 PreLoad, 0x1cf870 probably Create).
 *  Minimal so far: what g3dSetTexture uses.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DTEXTURE_H
#define _GRAPHICS_GRAPH3D_G3DTEXTURE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/sce_gs.h"                 // sceGsLoadImage

enum G3DRESOURCETYPE
{
    G3DRTYPE_TEXTURE        = 1,
    G3DRTYPE_VERTEXBUFFER   = 2,
    G3DRTYPE_INDEXBUFFER    = 3,
    INVALID_G3DRESOURCETYPE = 0x7fffffff,
};

// The GS register values a texture binds (FF2 union G3DTEXTUREDATA; only the
// raw-word view is used so far).
struct G3DTEXTUREDATA                   /* 0x10 */
{
    /* 0x0 */ u_long lTex0;
    /* 0x8 */ u_long lTex1;
};
static_assert(sizeof(G3DTEXTUREDATA) == 0x10, "G3DTEXTUREDATA is 0x10 bytes");
static_assert(offsetof(G3DTEXTUREDATA, lTex1) == 0x8, "G3DTEXTUREDATA.lTex1");

// Common base of the g3d GS resources. FF3 (GCC 3 ABI): vptr at +0,
// m_Type at +4. layout: host -- the vptr is 8 bytes.
class IG3DResource
{
protected:
    int m_Type;                         // G3DRESOURCETYPE

public:
    virtual ~IG3DResource() {}
    virtual void PreLoad(void) {}
};

// layout: host -- vptr. FF3 g3dSetTexture reads m_TextureData at +0x70 (TEX0)
// and +0x78 (TEX1); m_gsLoadImage's 16-byte alignment puts it at 0x10 on the
// EE and on the hosts alike, so m_TextureData keeps 0x70.
class CTexture : public IG3DResource
{
public:                                         // FF2: private (public here for the asserts)
    alignas(16) sceGsLoadImage m_gsLoadImage;   // 0x10, FF2, unverified
    G3DTEXTUREDATA  m_TextureData;              // 0x70
    u_char         *m_pbyData;                  // EE 0x80, FF2, unverified
    int             m_iSize;                    // EE 0x84, FF2, unverified

    G3DTEXTUREDATA &GetTextureDataRef(void)
    {
        return m_TextureData;
    }

    // FF3 0x1cf838 (not reconstructed; nothing constructs a CTexture yet).
    virtual void PreLoad(void);
};
static_assert(offsetof(CTexture, m_gsLoadImage) == 0x10, "CTexture.m_gsLoadImage");
static_assert(offsetof(CTexture, m_TextureData) == 0x70, "CTexture.m_TextureData");

#endif /* _GRAPHICS_GRAPH3D_G3DTEXTURE_H */
