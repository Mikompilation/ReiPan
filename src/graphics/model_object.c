// graphics/model_object.c -- FF3 0x2113d0..0x2125f8: ModelObject, TextureModelObject and
// MultiTextureModelObject (RTTI), the SGDs of one model PK4 and their draw passes.
// FF3-only (no FF2 file); between the motion managers and motion.c in link order.

#include "graphics/model_object.h"

#include <string.h>                     // memcpy

#include "common/cpp_heap.h"            // operator_new_array / operator_delete_array
#include "common/pk4.h"                 // PK4_HEADER, Pk4GetData
#include "graphics/graph2d/tim2.h"      // PK4SendVram
#include "graphics/graph3d/gra3d.h"     // gra3dGet/SetVu1ConstVF2, gra3dExecPrelight(Matrix), gra3dIsMonotoneDrawEnable
#include "graphics/graph3d/gra3dMisc.h" // gra3dDrawSGDShadow, gra3dDrawSGDShadowEveryObject
#include "graphics/graph3d/gra3dSGD.h"  // _gra3dDrawSGD, gra3dGet/SetSGDMaterialAlpha
#include "graphics/graph3d/gra3dSGDData.h"  // sgdRemap, sgdCalcBoneCoordinate, sgdGetBoundingBox
#include "graphics/graph3d/gra3dShadow.h"   // gra3dshadowAddProjectModel
#include "graphics/graph3d/sgd_types.h" // SGDFILEHEADER, SGDCOORDINATE, SgdPtr
#include "ingame/ingame.h"              // draw_en_371300 / 371304 / 371308
#include "sdk/libvu0.h"                 // sceVu0FVECTOR, sceVu0ApplyMatrix

// data 0x329530: the zero vector (g_v0000), VU1 VF2 for SGDs flagged vf2_zero.
static sceVu0FVECTOR model_object_vf2_zero = { 0.0f, 0.0f, 0.0f, 0.0f };

// ──────────────────────────────────────────────────────────────────────
// The object over model PK4 members mpk / flgs: the SGD count from mpk's
// header (0 without one), then every SGD remapped to absolute addresses.
// FF3 0x00211450 (complete-object constructor; base-object twin 0x002113d0
// has the same body).

ModelObject::ModelObject(void *mpk, void *flgs)
{
    this->mpk  = mpk;
    num        = (mpk != nullptr) ? ((PK4_HEADER *)mpk)->num : 0;
    this->flgs = flgs;
    for (int i = 0; i < num; i++)
        sgdRemap(GetSgd(i));
}

// ──────────────────────────────────────────────────────────────────────
// The opaque pass: begin (textures), the SGDs, end -- all through the vtable.
// FF3 0x002114d0
// callcheck: vtable +0x00 / +0x08 / +0x04 (BeginDraw, DrawSgd, EndDraw)

void ModelObject::Draw()
{
    BeginDraw();
    DrawSgd();
    EndDraw();
}

// ──────────────────────────────────────────────────────────────────────
// The alpha pass, while draw_en_371300 is set.
// FF3 0x00211520
// callcheck: vtable +0x00 / +0x04 (BeginDraw, EndDraw)

void ModelObject::DrawAlpha()
{
    if (draw_en_371300 != 0)
    {
        BeginDraw();
        DrawAlphaSgd();
        EndDraw();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every SGD but the shadow casters whose material alpha is not 0, with VU1
// VF2 zeroed for the vf2_zero ones; VF2 restored after.
// FF3 0x00211578 (vtable +0x08)

void ModelObject::DrawSgd()
{
    alignas(16) sceVu0FVECTOR vf2;
    int n;

    gra3dGetVu1ConstVF2(vf2);
    n = num;
    for (int i = 0; i < n; i++)
    {
        if (GetFlags(i)->shadow != 0)
            continue;

        SGDFILEHEADER *sgd = GetSgd(i);
        if (gra3dGetSGDMaterialAlpha(sgd) == 0)
            continue;

        gra3dSetVu1ConstVF2(GetFlags(i)->vf2_zero != 0 ? model_object_vf2_zero : vf2);
        _gra3dDrawSGD(sgd, SRT_REALTIME, GetCoord(i), -1);
    }
    gra3dSetVu1ConstVF2(vf2);
}

// ──────────────────────────────────────────────────────────────────────
// The SGDs flagged alpha whose material alpha is not 0.
// FF3 0x00211668

void ModelObject::DrawAlphaSgd()
{
    for (int i = 0; i < num; i++)
    {
        if (GetFlags(i)->alpha == 0)
            continue;

        SGDFILEHEADER *sgd = GetSgd(i);
        if (gra3dGetSGDMaterialAlpha(sgd) != 0)
            _gra3dDrawSGD(sgd, SRT_REALTIME, GetCoord(i), -1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// gra3dExecPrelight of every preset (pre-lit) SGD that is not a shadow caster.
// FF3 0x00211710

void ModelObject::ExecPrelight(float *trans, float *rot)
{
    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd;

        if (GetFlags(i)->shadow == 0 && ((sgd = GetSgd(i))->ucModelType & 1) != 0)
            gra3dExecPrelight(sgd, trans, rot);
    }
}

// ──────────────────────────────────────────────────────────────────────
// gra3dExecPrelightMatrix of every preset SGD that is not a shadow caster.
// FF3 0x002117b8

void ModelObject::ExecPrelightMatrix(float *matrix)
{
    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd;

        if (GetFlags(i)->shadow == 0 && ((sgd = GetSgd(i))->ucModelType & 1) != 0)
            gra3dExecPrelightMatrix(sgd, (float (*)[4])matrix);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The prelighting of every preset SGD that is not a shadow caster cleared
// (an SRT_CLEARPRELIGHTING draw).
// FF3 0x00211850

void ModelObject::ClearPrelight()
{
    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd;

        if (GetFlags(i)->shadow == 0 && ((sgd = GetSgd(i))->ucModelType & 1) != 0)
            _gra3dDrawSGD(sgd, SRT_CLEARPRELIGHTING, GetCoord(i), -1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// While draw_en_371304 is set: every shadow-caster SGD casts with light
// (gra3dDrawSGDShadowEveryObject); nothing without a light.
// FF3 0x00211900

void ModelObject::DrawShadow(G3DLIGHT *light)
{
    if (draw_en_371304 == 0)
        return;

    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd = GetSgd(i);

        if (light != nullptr && sgd != nullptr && GetFlags(i)->shadow != 0)
            gra3dDrawSGDShadowEveryObject(sgd, GetCoord(i), light);
    }
}

// ──────────────────────────────────────────────────────────────────────
// As DrawShadow, through gra3dDrawSGDShadow with the caster's world box bb
// (char_model.c's shadow draws).
// FF3 0x002119b8

void ModelObject::DrawShadowBox(G3DLIGHT *light, float (*bb)[4])
{
    if (draw_en_371304 == 0)
        return;

    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd = GetSgd(i);

        if (light != nullptr && sgd != nullptr && GetFlags(i)->shadow != 0)
            gra3dDrawSGDShadow(sgd, GetCoord(i), light, bb);
    }
}

// ──────────────────────────────────────────────────────────────────────
// matrix into the root coordinate (matCoord) of every SGD's coordinate set,
// once per set: SGD 0's, then any SGD whose set differs from it (none, as
// GetCoord gives SGD 0's for every SGD).
// FF3 0x00211a80 (lqc2 / sqc2 copy)

void ModelObject::SetMatrix(float *matrix)
{
    SGDCOORDINATE *coord0 = GetCoord(0);

    for (int i = 0; i < num; i++)
    {
        SGDCOORDINATE *coord = GetCoord(i);

        if (i == 0 || coord0 != coord)
            memcpy(coord->matCoord, matrix, sizeof(sceVu0FMATRIX));
    }
}

// ──────────────────────────────────────────────────────────────────────
// SGD no: member no of mpk.
// FF3 0x00211b28

SGDFILEHEADER *ModelObject::GetSgd(int no)
{
    return (SGDFILEHEADER *)Pk4GetData(mpk, no);
}

// ──────────────────────────────────────────────────────────────────────
// SGD no's flag bytes: member no of flgs.
// FF3 0x00211b40

MODEL_OBJECT_FLAG *ModelObject::GetFlags(int no)
{
    return (MODEL_OBJECT_FLAG *)Pk4GetData(flgs, no);
}

// ──────────────────────────────────────────────────────────────────────
// The coordinate array of SGD 0. FF3 passes 0 to GetSgd whatever no is
// (move a1, zero), so every SGD of a model shares SGD 0's coordinates; the
// per-SGD tests in SetMatrix / CalcBoneCoordinate never fire. Kept.
// FF3 0x00211b58. PC: pCoord is an EE address (sgdRemap), a host pointer here.

SGDCOORDINATE *ModelObject::GetCoord(int no)
{
    (void)no;
    return SgdPtr<SGDCOORDINATE>(GetSgd(0)->pCoord);
}

// ──────────────────────────────────────────────────────────────────────
// SGD no's vertex pools (its PHEAD), for the shape and cloth motions.
// FF3 0x00211b78. PC: a host pointer.

PHEAD *ModelObject::GetVectorInfo(int no)
{
    return SgdPtr<PHEAD>(GetSgd(no)->pVectorInfo);
}

// ──────────────────────────────────────────────────────────────────────
// Every SGD that is neither no_project nor a shadow caster onto the map
// shadow's project-model list (the models shadows fall on), with its
// coordinates.
// FF3 0x00211b98

void ModelObject::ReqDraw()
{
    for (int i = 0; i < num; i++)
    {
        MODEL_OBJECT_FLAG *flag = GetFlags(i);

        if (flag->no_project == 0 && flag->shadow == 0)
            gra3dshadowAddProjectModel(GetSgd(i), GetCoord(i));
    }
}

// ──────────────────────────────────────────────────────────────────────
// in through coordinate coord's local->world matrix into out (coord < 0: 0).
// FF3 0x00211c38 (VU0 vmula / vmadd: sceVu0ApplyMatrix)

void ModelObject::GetCoordPos(float *out, int coord, float *in)
{
    if (coord < 0)
        coord = 0;
    sceVu0ApplyMatrix(out, GetCoord(0)[coord].matLocalWorld, in);
}

// ──────────────────────────────────────────────────────────────────────
// Row row of coordinate coord's local->world matrix into out (coord < 0: 0).
// FF3 0x00211cb8

void ModelObject::GetCoordAxis(float *out, int coord, int row)
{
    if (coord < 0)
        coord = 0;
    memcpy(out, GetCoord(0)[coord].matLocalWorld[row], sizeof(sceVu0FVECTOR));
}

// ──────────────────────────────────────────────────────────────────────
// sgdCalcBoneCoordinate of every SGD's coordinate set, once per set (see
// SetMatrix), over the SGD's block count - 1.
// FF3 0x00211d18

void ModelObject::CalcBoneCoordinate()
{
    SGDCOORDINATE *coord0 = GetCoord(0);

    for (int i = 0; i < num; i++)
    {
        SGDFILEHEADER *sgd   = GetSgd(i);
        SGDCOORDINATE *coord = GetCoord(i);

        if (i == 0 || coord0 != coord)
            sgdCalcBoneCoordinate(coord, (int)sgd->uiNumBlock - 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every SGD's material alpha.
// FF3 0x00211db8

void ModelObject::SetAlphaAll(u_char alpha)
{
    for (int i = 0; i < num; i++)
        SetAlpha(i, alpha);
}

// ──────────────────────────────────────────────────────────────────────
// SGD no's material alpha (nothing past the last SGD).
// FF3 0x00211e20

void ModelObject::SetAlpha(int no, u_char alpha)
{
    if (no < num)
        gra3dSetSGDMaterialAlpha(GetSgd(no), alpha);
}

// ──────────────────────────────────────────────────────────────────────
// The bounding box of the first SGD that is not a shadow caster. No caller
// found in FF3.
// FF3 0x00211e70

void ModelObject::GetBoundingBox(float (*bb)[4])
{
    for (int i = 0; i < num; i++)
    {
        if (GetFlags(i)->shadow == 0)
        {
            sgdGetBoundingBox(GetSgd(i), bb);
            return;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// TextureModelObject

// ──────────────────────────────────────────────────────────────────────
// tpk into VRAM, then mono when monotone drawing is on (neither without tpk).
// FF3 0x00211f08

void TextureModelObject::SendTexture()
{
    if (tpk == nullptr)
        return;

    PK4SendVram(tpk, 0, -1, -1);
    if (gra3dIsMonotoneDrawEnable() != 0 && mono != nullptr)
        PK4SendVram(mono, 0, -1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// Before the SGD draw: the textures into VRAM.
// FF3 0x00211f80 (vtable +0x00)

void TextureModelObject::BeginDraw()
{
    SendTexture();
}

// ══════════════════════════════════════════════════════════════════════
// MultiTextureModelObject

// ──────────────────────────────────────────────────────────────────────
// The texture object plus mlt, and one blend alpha byte per SGD on FF3's
// C++ heap, all 0.
// FF3 0x00212010 (complete-object constructor; base-object twin 0x00211f98)
// callcheck: ModelObjectCtorBase (the base constructor)

MultiTextureModelObject::MultiTextureModelObject(void *mpk, void *flgs, void *tpk, void *mono, void *mlt)
    : TextureModelObject(mpk, flgs, tpk, mono)
{
    this->mlt = mlt;
    tex_frame = (u_char *)operator_new_array(num);
    MultiTextureModelObject::SetTexFrame(0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002120d8 (base-object twin 0x00212088, deleting 0x00212128): the
// blend bytes freed.

MultiTextureModelObject::~MultiTextureModelObject()
{
    if (tex_frame != nullptr)
        operator_delete_array(tex_frame);
}

// ──────────────────────────────────────────────────────────────────────
// Before the SGD draw: the first texture; when blending, the base SGDs drawn
// with it and then the mlt texture loaded for DrawSgd.
// FF3 0x00212178 (vtable +0x00)

void MultiTextureModelObject::BeginDraw()
{
    TextureModelObject::BeginDraw();
    if (IsBlend() != 0)
    {
        DrawBaseSgd();
        SendMultiTexture();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Not blending: ModelObject's pass. Blending (the mlt texture is in VRAM):
// the SGDs from GetBaseNum on, and the multi ones again at material alpha =
// their blend byte (restored after); VF2 as ModelObject::DrawSgd.
// FF3 0x002121c8 (vtable +0x08)

void MultiTextureModelObject::DrawSgd()
{
    alignas(16) sceVu0FVECTOR vf2;
    int base, n;

    if (IsBlend() == 0)
    {
        ModelObject::DrawSgd();
        return;
    }

    gra3dGetVu1ConstVF2(vf2);
    base = GetBaseNum();
    n    = num;
    for (int i = 0; i < n; i++)
    {
        if (GetFlags(i)->shadow != 0)
            continue;
        if (i < base && GetFlags(i)->multi == 0)
            continue;

        SGDFILEHEADER *sgd   = GetSgd(i);
        int            alpha = gra3dGetSGDMaterialAlpha(sgd);
        if (alpha == 0)
            continue;

        gra3dSetVu1ConstVF2(GetFlags(i)->vf2_zero != 0 ? model_object_vf2_zero : vf2);
        if (GetFlags(i)->multi == 0)
        {
            _gra3dDrawSGD(sgd, SRT_REALTIME, GetCoord(i), -1);
        }
        else
        {
            SetAlpha(i, tex_frame[i]);
            _gra3dDrawSGD(sgd, SRT_REALTIME, GetCoord(i), -1);
            SetAlpha(i, (u_char)alpha);
        }
    }
    gra3dSetVu1ConstVF2(vf2);
}

// ──────────────────────────────────────────────────────────────────────
// mlt into VRAM, then mono when monotone drawing is on (neither without mlt).
// FF3 0x00212368

void MultiTextureModelObject::SendMultiTexture()
{
    if (mlt == nullptr)
        return;

    PK4SendVram(mlt, 0, -1, -1);
    if (gra3dIsMonotoneDrawEnable() != 0 && mono != nullptr)
        PK4SendVram(mono, 0, -1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// 1 when draw_en_371308 is set and any SGD's blend byte is not 0.
// FF3 0x002123e0

int MultiTextureModelObject::IsBlend()
{
    if (draw_en_371308 != 0)
    {
        for (int i = 0; i < num; i++)
        {
            if (tex_frame[i] != 0)
                return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The SGDs up to and including the last multi one (0: none is multi).
// FF3 0x00212438

int MultiTextureModelObject::GetBaseNum()
{
    int base = 0;

    for (int i = 0; i < num; i++)
    {
        if (GetFlags(i)->multi != 0)
            base = i + 1;
    }
    return base;
}

// ──────────────────────────────────────────────────────────────────────
// ModelObject::DrawSgd over the first GetBaseNum SGDs (with the first
// texture, before the mlt texture replaces it).
// FF3 0x002124a8

void MultiTextureModelObject::DrawBaseSgd()
{
    alignas(16) sceVu0FVECTOR vf2;
    int base = GetBaseNum();

    gra3dGetVu1ConstVF2(vf2);
    for (int i = 0; i < base; i++)
    {
        if (GetFlags(i)->shadow != 0)
            continue;

        SGDFILEHEADER *sgd = GetSgd(i);
        if (gra3dGetSGDMaterialAlpha(sgd) == 0)
            continue;

        gra3dSetVu1ConstVF2(GetFlags(i)->vf2_zero != 0 ? model_object_vf2_zero : vf2);
        _gra3dDrawSGD(sgd, SRT_REALTIME, GetCoord(i), -1);
    }
    gra3dSetVu1ConstVF2(vf2);
}

// ──────────────────────────────────────────────────────────────────────
// Every SGD's blend byte (GameModel::SetTexFrame).
// FF3 0x00212598 (vtable +0x14)

void MultiTextureModelObject::SetTexFrame(u_char frame)
{
    for (int i = 0; i < num; i++)
        tex_frame[i] = frame;
}

// ──────────────────────────────────────────────────────────────────────
// SGD no's blend byte (GameModel::SetTexFrameOne).
// FF3 0x002125d0 (vtable +0x18)

void MultiTextureModelObject::SetTexFrameOne(int no, u_char frame)
{
    tex_frame[no] = frame;
}

// ──────────────────────────────────────────────────────────────────────
// SGD no's blend byte.
// FF3 0x002125e8 (vtable +0x1c)

int MultiTextureModelObject::GetTexFrame(int no)
{
    return tex_frame[no];
}
