// graphics/graph3d/g3dRenderTarget.cpp -- CRenderTarget, FF3 0x1cec20..0x1cf348
// (+ the auto-state linkonce bodies 0x2bbac0..0x2bbd70, inline in the header).
// From MioPan's g3dRenderTarget.cpp, checked against FF3: the GS calls go
// through IG3DCompatible's pointers, the member destructors pop (RAII), and the
// view transform is never pushed; Clear(G3DCOLOR) ignores its colour.

#include "common/vu0math.h"                     // IntLogBase
#include "graphics/graph3d/g3dRenderTarget.h"
#include "sdk/libgraph.h"                       // SCE_GS_*

// FF3 0x001cec20: the members' vtables and zeroed stacks / saved registers
// (callcheck: memset (inline CAutoGsRegisters ctor)).
CRenderTarget::CRenderTarget()
{
}

// FF3 0x001ced28 (complete twin 0x001cec80, deleting 0x001cedd0): the members
// pop on destruction -- the view transform first, then the GS registers.
// callcheck: PopStack (the members' inline destructors)
CRenderTarget::~CRenderTarget()
{
}

// FF3 0x001cee80
int CRenderTarget::Create(const RENDERTARGETCREATIONDATA *pCD)
{
    m_gsTex0     = pCD->gsTex0;
    m_ClearColor = pCD->ClearColor;
    m_fZMax      = pCD->fZMax;
    return 1;
}

// Draw into the buffer: flush the texture cache, then FRAME_1 (FBP = TBP0 / 32,
// FBW = width / 64, PSM from TEX0), XYOFFSET_1 (the buffer centred on the
// 2048 guard band, 12.4 fixed point) and SCISSOR_1 (the whole buffer), saving
// the old values. FF3 0x001ceea0
void CRenderTarget::Begin()
{
    GS_REG_SET aGPA[3];
    int        iWidth;
    int        iHeight;

    s_pFuncSetGsRegister(0, SCE_GS_TEXFLUSH, 1);
    iWidth  = GetWidth();
    iHeight = GetHeight();

    aGPA[0].data = (m_gsTex0 & 0x3fff) >> 5
                 | (u_long)(iWidth / 64) << 16
                 | ((m_gsTex0 >> 20) & 0x3f) << 24;
    aGPA[0].reg  = SCE_GS_FRAME_1;
    aGPA[1].data = (u_long)((0x800 - iWidth / 2) * 0x10)
                 | (u_long)(0x800 - iHeight / 2) << 36;
    aGPA[1].reg  = SCE_GS_XYOFFSET_1;
    aGPA[2].data = (u_long)(iWidth - 1) << 16
                 | (u_long)(iHeight - 1) << 48;
    aGPA[2].reg  = SCE_GS_SCISSOR_1;
    m_AutoGsRegisters.Push(aGPA);
}

// FF3 0x001cf040
void CRenderTarget::End()
{
    m_AutoGsRegisters.Pop();
}

// Fill the buffer with a flat untextured sprite at m_fZMax, centred on the
// SCISSOR_1 that Begin sets. FF3 never stores Color: the sprite data is
// memset, so the colour is always 0 (black, alpha 0 -- the shadow target's
// clear colour anyway), and the left edge uses the height (FF2 does both;
// the shadow texture is square). FF3 0x001cf090
// (callcheck: CSprite, __floatdisf (the ctor runs on the declaration; the EE's
// u_long -> float helper))
void CRenderTarget::Clear(G3DCOLOR Color)
{
    int           iWidth;
    int           iHeight;
    u_long        ulScissor;
    float         fCenterX;
    float         fCenterY;
    CSprite       sprite;
    G3DSPRITEDATA spritedata;

    (void)Color;
    iWidth    = GetWidth();
    iHeight   = GetHeight();
    ulScissor = (u_long)(iWidth - 1) << 16 | (u_long)(iHeight - 1) << 48;
    fCenterX  = (float)((ulScissor >> 16 & 0x7ff) - (ulScissor & 0x7ff) + 1) * 0.5f;
    fCenterY  = (float)((ulScissor >> 48 & 0x7ff) - (ulScissor >> 32 & 0x7ff) + 1) * 0.5f;

    memset(&spritedata, 0, sizeof(spritedata));
    spritedata.fZ           = m_fZMax;
    spritedata.Rect.fLeft   = fCenterX - (float)(iHeight / 2);
    spritedata.Rect.fTop    = fCenterY - (float)(iHeight / 2);
    spritedata.Rect.fRight  = fCenterX + (float)(iWidth / 2);
    spritedata.Rect.fBottom = fCenterY + (float)(iHeight / 2);
    sprite.Create(&spritedata, nullptr);
    sprite.Draw();
}

// FF3 0x001cf258
void CRenderTarget::Clear()
{
    Clear(m_ClearColor);
}

// FF3 0x001cf270
void CRenderTarget::SetWidth(int iWidth)
{
    u_long uLog = (u_long)IntLogBase(2, iWidth);

    m_gsTex0 = (m_gsTex0 & ~((u_long)0xf << 26)) | (uLog & 0xf) << 26;
}

// FF3 0x001cf2b8
void CRenderTarget::SetHeight(int iHeight)
{
    u_long uLog = (u_long)IntLogBase(2, iHeight);

    m_gsTex0 = (m_gsTex0 & ~((u_long)0xf << 30)) | (uLog & 0xf) << 30;
}

// FF3 0x001cf308
int CRenderTarget::GetWidth()
{
    return 1 << ((u_int)(m_gsTex0 >> 26) & 0xf);
}

// FF3 0x001cf328
int CRenderTarget::GetHeight()
{
    return 1 << ((u_int)(m_gsTex0 >> 30) & 0xf);
}
