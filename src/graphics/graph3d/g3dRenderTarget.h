/* ==========================================================================
 *  graphics/graph3d/g3dRenderTarget.h
 *
 *  CRenderTarget (FF2 g3dRenderTarget.cpp; RTTI vtable 0x341db8): a GS render
 *  buffer described by one TEX0 (TBP0 / TBW / PSM give the FRAME, the TW / TH
 *  log2 fields the size), a clear colour and a max Z. Begin() points FRAME_1 /
 *  XYOFFSET_1 / SCISSOR_1 at the buffer and saves the old values in its
 *  CAutoGsRegisters<3>; End() puts them back. gra3dShadow.c's shadow texture
 *  is the only instance (bss 0x3ee950).
 *
 *  The two auto-state helpers save a run of GS registers / a transform and
 *  restore it on Pop() or destruction; they call the g3d core through
 *  IG3DCompatible's function pointers (g3dCore.h). Their bodies are linkonce
 *  copies at 0x2bbac0..0x2bbd70. FF3 extent 0x1cec20..0x1cf348 (0x1cf348.. is
 *  CSprite, g3dSprite.h).
 *
 *  layout: host -- every class here has a vtable (4-byte vptr on the EE, 8
 *  here), so the EE offsets are noted per member and not asserted.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DRENDERTARGET_H
#define _GRAPHICS_GRAPH3D_G3DRENDERTARGET_H

#include <string.h>                     // memset
#include <libvu0.h>                     // sceVu0FMATRIX

#include "sdk/scetypes.h"
#include "graphics/gs_reg.h"            // GS_REG_SET
#include "graphics/graph3d/g3dCore.h"   // IG3DCompatible
#include "graphics/graph3d/g3dSprite.h" // G3DCOLOR

// CRenderTarget::Create's argument (FF2 RENDERTARGETCREATIONDATA).
struct RENDERTARGETCREATIONDATA         /* 0x10 */
{
    /* 0x0 */ u_long   gsTex0;          // sceGsTex0 bits
    /* 0x8 */ G3DCOLOR ClearColor;
    /* 0xc */ float    fZMax;
};
static_assert(sizeof(RENDERTARGETCREATIONDATA) == 0x10, "RENDERTARGETCREATIONDATA is 0x10 bytes");
static_assert(offsetof(RENDERTARGETCREATIONDATA, ClearColor) == 0x8, "RENDERTARGETCREATIONDATA.ClearColor");
static_assert(offsetof(RENDERTARGETCREATIONDATA, fZMax) == 0xc, "RENDERTARGETCREATIONDATA.fZMax");

// layout: host -- vtable (EE vptr 4 bytes, host 8).
// One level of save / restore (vtable 0x341df8: +0 dtor 0x2bbae0, +4
// deleting dtor 0x2bbaf0, Pop pure). EE: +4 m_iNumStack.
class IAutoState : public IG3DCompatible
{
public:
    IAutoState() { m_iNumStack = 0; }
    virtual ~IAutoState() {}
    virtual void Pop() = 0;

protected:
    // m_iNumStack < 1: count it, 1 (inlined into Begin).
    int PushStack()
    {
        if (m_iNumStack >= 1)
        {
            return 0;
        }
        m_iNumStack++;
        return 1;
    }
    // m_iNumStack > 0: uncount it, 1. FF3 0x002bbac0 (linkonce).
    int PopStack()
    {
        if (m_iNumStack <= 0)
        {
            return 0;
        }
        m_iNumStack--;
        return 1;
    }

private:
    int m_iNumStack;                    // EE +0x4
};

// layout: host -- vtable (EE vptr 4 bytes, host 8).
// Saves N GS registers on Push and writes them back on Pop / destruction.
// CAutoGsRegisters<3>: vtable 0x341de0 (+0 dtor 0x2bbb10, +4 deleting
// 0x2bbb80, +8 Pop 0x2bbbf0). EE: +8 m_aGPA[N].
template <int N>
class CAutoGsRegisters : public IAutoState
{
public:
    // Inlined into CRenderTarget's ctor (memset of the 0x30 bytes).
    CAutoGsRegisters() { memset(m_aGPA, 0, sizeof(m_aGPA)); }
    // FF3 0x002bbb10 (N = 3): Pop inlined.
    virtual ~CAutoGsRegisters() { Pop(); }

    // FF3 0x002bbbf0 (N = 3).
    virtual void Pop()
    {
        if (PopStack() != 0)
        {
            s_pFuncSetGsRegisters(m_aGPA, N, 1);
        }
    }

    // Remember the registers aGPA names, then write aGPA (inlined into
    // CRenderTarget::Begin).
    void Push(const GS_REG_SET *aGPA)
    {
        int i;

        if (PushStack() != 0)
        {
            for (i = 0; i < N; i++)
            {
                m_aGPA[i].reg  = aGPA[i].reg;
                m_aGPA[i].data = *s_pFuncGetGsRegisterRef((int)aGPA[i].reg);
            }
            s_pFuncSetGsRegisters(aGPA, N, 1);
        }
    }

private:
    GS_REG_SET m_aGPA[N];               // EE +0x8
};

// Saves transform S on Push and restores it on Pop / destruction.
// CAutoTransform<0>: vtable 0x341dc8 (+0 dtor 0x2bbc40, +4 deleting 0x2bbcb0,
// +8 Pop 0x2bbd20). FF3 instantiates it for state 0 (G3DTS_VIEW); nothing in
// FF3 pushes it, only the destructors pop it. EE: +0x10 m_mat.
// layout: host -- vtable (EE vptr 4 bytes, host 8).
template <int S>
class CAutoTransform : public IAutoState
{
public:
    // FF3 0x002bbc40 (S = 0): Pop inlined.
    virtual ~CAutoTransform() { Pop(); }

    // FF3 0x002bbd20 (S = 0).
    virtual void Pop()
    {
        if (PopStack() != 0)
        {
            s_pFuncSetTransform(S, m_mat);
        }
    }

private:
    sceVu0FMATRIX m_mat;                // EE +0x10
};

// layout: host -- vtable (EE vptr 4 bytes, host 8).
// vtable 0x341db8: +0 dtor 0x1ced28, +4 deleting dtor 0x1cedd0 (complete
// twin 0x1cec80). EE size 0xa8.
class CRenderTarget : public IG3DCompatible
{
public:
    CRenderTarget();                    // FF3 0x001cec20
    virtual ~CRenderTarget();           // FF3 0x001ced28

    int  Create(const RENDERTARGETCREATIONDATA *pCD);   // FF3 0x001cee80
    void Begin();                       // FF3 0x001ceea0
    void End();                         // FF3 0x001cf040
    void Clear(G3DCOLOR Color);         // FF3 0x001cf090
    void Clear();                       // FF3 0x001cf258
    void SetWidth(int iWidth);          // FF3 0x001cf270
    void SetHeight(int iHeight);        // FF3 0x001cf2b8
    int  GetWidth();                    // FF3 0x001cf308
    int  GetHeight();                   // FF3 0x001cf328

    // FF2's inline accessor; SetUpShadow / DispShadowSprite read +8.
    u_long &GetGsTex0Ref() { return m_gsTex0; }

private:
    u_long                    m_gsTex0;            // EE +0x08, sceGsTex0 bits
    CAutoGsRegisters<3>       m_AutoGsRegisters;   // EE +0x10
    CAutoTransform<G3DTS_VIEW> m_AutoTransformView; // EE +0x50
    G3DCOLOR                  m_ClearColor;        // EE +0xa0
    float                     m_fZMax;             // EE +0xa4
};

#endif /* _GRAPHICS_GRAPH3D_G3DRENDERTARGET_H */
