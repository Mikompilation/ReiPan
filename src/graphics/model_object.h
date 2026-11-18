/* ==========================================================================
 *  graphics/model_object.h
 *
 *  The model object (RTTI ModelObject, vtable 0x3421a0) and its texture
 *  variants: the SGDs of one model PK4 and how they are drawn. A model's
 *  motion object (ModelManager, graphics/mmanage.h) keeps one at +4
 *  (ani_ctrl) and forwards every draw, prelight, shadow and matrix call to
 *  it; the model builders (graphics/model_builder.c) make it. FF3-only (FF2
 *  drew its models through motion.c's ANI_CTRL); code 0x2113d0..0x2125f8.
 * ======================================================================== */
#ifndef _GRAPHICS_MODEL_OBJECT_H
#define _GRAPHICS_MODEL_OBJECT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct G3DLIGHT;                        // graphics/graph3d/g3dLight.h
struct SGDFILEHEADER;                   // graphics/graph3d/sgd_types.h
struct SGDCOORDINATE;
struct PHEAD;

// One SGD's entry of the model PK4 member "flgs" (ModelObject::GetFlags,
// Pk4GetData(flgs, no)): one byte per switch. Size not known (5 bytes read).
struct MODEL_OBJECT_FLAG
{
    /* 0x00 */ u_char no_project;       // set: ReqDraw does not queue it as a shadow project model
    /* 0x01 */ u_char shadow;           // set: a shadow caster -- only DrawShadow / DrawShadowBox take it, every other pass skips it
    /* 0x02 */ u_char alpha;            // set: drawn by DrawAlpha's pass
    /* 0x03 */ u_char multi;            // set: blended with the second texture (MultiTextureModelObject)
    /* 0x04 */ u_char vf2_zero;         // set: drawn with VU1 VF2 = (0, 0, 0, 0)
};
static_assert(sizeof(MODEL_OBJECT_FLAG) == 0x05, "MODEL_OBJECT_FLAG (as far as read)");
static_assert(offsetof(MODEL_OBJECT_FLAG, shadow) == 0x01, "MODEL_OBJECT_FLAG.shadow");
static_assert(offsetof(MODEL_OBJECT_FLAG, alpha) == 0x02, "MODEL_OBJECT_FLAG.alpha");
static_assert(offsetof(MODEL_OBJECT_FLAG, multi) == 0x03, "MODEL_OBJECT_FLAG.multi");
static_assert(offsetof(MODEL_OBJECT_FLAG, vf2_zero) == 0x04, "MODEL_OBJECT_FLAG.vf2_zero");

// RTTI ModelObject (vtable 0x3421a0; FF3 operator new(0x10)): the SGDs of
// the model PK4 member "mpk" (SGD n is GetSgd(n)), each with its flag bytes
// in member "flgs". Virtual slots in EE order; the empty ones are FF3's
// inline functions (linkonce code at 0x2ac7d8.. / 0x2bd858..).
// layout: host -- the vptr and pointers are 4 bytes on the EE; EE offsets in the comments.
class ModelObject
{
public:
    // FF3 0x00211450 (base-object twin 0x002113d0, the same body): mpk and
    // its SGD count, flgs, then sgdRemap of every SGD.
    ModelObject(void *mpk, void *flgs);

    virtual void BeginDraw() {}                     // EE vtable +0x00 (FF3 0x002bd858): before the SGD draw
    virtual void EndDraw() {}                       // EE vtable +0x04 (FF3 0x002ac7d8): after it
    virtual void DrawSgd();                         // EE vtable +0x08, FF3 0x00211578
    virtual ~ModelObject() {}                       // EE vtable +0x0c / +0x10 (FF3 0x002bd860 / deleting 0x002bd870)
    virtual void SetTexFrame(u_char frame) { (void)frame; }                 // EE vtable +0x14 (FF3 0x002ac7e0)
    virtual void SetTexFrameOne(int no, u_char frame) { (void)no; (void)frame; }   // EE vtable +0x18 (FF3 0x002ac7e8)
    virtual int  GetTexFrame(int no) { (void)no; return 0; }               // EE vtable +0x1c (FF3 0x002ac7f0)

    void Draw();                                    // FF3 0x002114d0
    void DrawAlpha();                               // FF3 0x00211520
    void DrawAlphaSgd();                            // FF3 0x00211668
    void ExecPrelight(float *trans, float *rot);    // FF3 0x00211710
    void ExecPrelightMatrix(float *matrix);         // FF3 0x002117b8
    void ClearPrelight();                           // FF3 0x00211850
    void DrawShadow(G3DLIGHT *light);               // FF3 0x00211900
    void DrawShadowBox(G3DLIGHT *light, float (*bb)[4]);   // FF3 0x002119b8
    void SetMatrix(float *matrix);                  // FF3 0x00211a80
    SGDFILEHEADER     *GetSgd(int no);              // FF3 0x00211b28
    MODEL_OBJECT_FLAG *GetFlags(int no);            // FF3 0x00211b40
    SGDCOORDINATE     *GetCoord(int no);            // FF3 0x00211b58: SGD 0's coordinates, whatever no
    PHEAD             *GetVectorInfo(int no);       // FF3 0x00211b78
    void ReqDraw();                                 // FF3 0x00211b98
    void GetCoordPos(float *out, int coord, float *in);     // FF3 0x00211c38
    void GetCoordAxis(float *out, int coord, int row);      // FF3 0x00211cb8
    void CalcBoneCoordinate();                      // FF3 0x00211d18
    void SetAlphaAll(u_char alpha);                 // FF3 0x00211db8
    void SetAlpha(int no, u_char alpha);            // FF3 0x00211e20
    void GetBoundingBox(float (*bb)[4]);            // FF3 0x00211e70
    // Bone turns (IK), defined in graphics/motion/motion.c (their FF3 range):
    // root and mid turned about z so end reaches target; returns 1.
    int  IkLimb(float *target, int root, int mid, int end);    // FF3 0x002165d8
    // From tip's parent up to stop, each joint turned about its axis (0..2)
    // toward target; returns 1.
    int  IkChain(float *target, int tip, int stop, int axis);  // FF3 0x00216c40

    /* 0x04 */ void *mpk;                   // model PK4 member "mpk": a pack of SGDs
    /* 0x08 */ int   num;                   // its SGD count (PK4 header word +8)
    /* 0x0c */ void *flgs;                  // model PK4 member "flgs": MODEL_OBJECT_FLAG per SGD
};
static_assert(offsetof(ModelObject, mpk) == sizeof(void *), "ModelObject.mpk (EE +0x04)");
static_assert(offsetof(ModelObject, num) == 2 * sizeof(void *), "ModelObject.num (EE +0x08)");
static_assert(offsetof(ModelObject, flgs) == 3 * sizeof(void *), "ModelObject.flgs (EE +0x0c)");
typedef ModelObject MDL_OBJ;            // its old stand-in name

// RTTI TextureModelObject : ModelObject (vtable 0x342178; FF3 operator
// new(0x18)): a model PK4 with textures, sent to VRAM before each draw. FF3
// has no constructor of its own -- NormalModelBuilder::BuildObject inlines it
// (ModelObject's base-object constructor, then +0x10 / +0x14, then the vptr).
// layout: host (as ModelObject); EE offsets in the comments.
class TextureModelObject : public ModelObject
{
public:
    TextureModelObject(void *mpk, void *flgs, void *tpk, void *mono)
        : ModelObject(mpk, flgs)
    {
        this->tpk  = tpk;
        this->mono = mono;
    }
    // EE vtable +0x0c / +0x10: FF3 0x002bd890 / deleting 0x002bd8a0 (inline).

    void BeginDraw() override;                      // FF3 0x00211f80
    void SendTexture();                             // FF3 0x00211f08

    /* 0x10 */ void *tpk;                   // model PK4 member "tpk": the textures
    /* 0x14 */ void *mono;                  // model PK4 member "mono": sent too while monotone drawing is on
};
static_assert(offsetof(TextureModelObject, tpk) == 4 * sizeof(void *), "TextureModelObject.tpk (EE +0x10)");
static_assert(offsetof(TextureModelObject, mono) == 5 * sizeof(void *), "TextureModelObject.mono (EE +0x14)");

// RTTI MultiTextureModelObject : TextureModelObject (vtable 0x342150; FF3
// operator new(0x20)): a model PK4 that also has an "mlt" texture. The SGDs
// flagged multi are drawn a second time with it at their own alpha (the
// "texture frame" GameModel::SetTexFrame sets) while blending is on.
// layout: host (as ModelObject); EE offsets in the comments.
class MultiTextureModelObject : public TextureModelObject
{
public:
    // FF3 0x00212010 (base-object twin 0x00211f98).
    MultiTextureModelObject(void *mpk, void *flgs, void *tpk, void *mono, void *mlt);
    ~MultiTextureModelObject() override;            // FF3 0x002120d8 (twin 0x00212088, deleting 0x00212128)

    void BeginDraw() override;                      // FF3 0x00212178
    void DrawSgd() override;                        // FF3 0x002121c8
    void SetTexFrame(u_char frame) override;        // FF3 0x00212598
    void SetTexFrameOne(int no, u_char frame) override;    // FF3 0x002125d0
    int  GetTexFrame(int no) override;              // FF3 0x002125e8

    void SendMultiTexture();                        // FF3 0x00212368
    int  IsBlend();                                 // FF3 0x002123e0
    int  GetBaseNum();                              // FF3 0x00212438
    void DrawBaseSgd();                             // FF3 0x002124a8

    /* 0x18 */ void   *mlt;                 // model PK4 member "mlt": the second texture
    /* 0x1c */ u_char *tex_frame;           // per SGD: the second texture's alpha (operator new[](num))
};
static_assert(offsetof(MultiTextureModelObject, mlt) == 6 * sizeof(void *), "MultiTextureModelObject.mlt (EE +0x18)");
static_assert(offsetof(MultiTextureModelObject, tex_frame) == 7 * sizeof(void *), "MultiTextureModelObject.tex_frame (EE +0x1c)");

#endif /* _GRAPHICS_MODEL_OBJECT_H */
