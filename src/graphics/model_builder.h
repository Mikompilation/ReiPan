/* ==========================================================================
 *  graphics/model_builder.h
 *
 *  FF3's model builders (C++, RTTI names, no FF2 counterpart): a builder
 *  turns a loaded model PK4 into the motion object a GameModel draws
 *  (graphics/game_model.h). ModelDirector runs a builder's three steps --
 *  BuildObject (the ModelObject over the SGDs, the collision shapes),
 *  BuildTexture (MotionModelBuilder: the ccs object and the bone, shape and
 *  cloth motions) and BuildMotion (the ModelManager / MotionModelManager that
 *  becomes GameModel.mot) -- and GetResult hands the object out. The builder
 *  owns everything it made; GameModel deletes the builder with the model.
 *
 *  Also the model's collision shapes (ModelCollision) over the PK4's cld
 *  member: spheres, capsules and planes on the model's SGD coordinates that
 *  push a body (MDL_CLD_BODY) out of them.
 *
 *  FF3 0x20df28..0x20f388; ModelBuilder's own inline code and the shape
 *  tests sit at 0x2bd118..0x2bd7b0.
 * ======================================================================== */
#ifndef _GRAPHICS_MODEL_BUILDER_H
#define _GRAPHICS_MODEL_BUILDER_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

class ModelManager;                          // graphics/mmanage.h
typedef ModelManager MDL_MOT;
class ModelObject;                      // graphics/motion/motion.h
class BmdData;
class BmdMotion;
class ShpData;
class ShpMotion;
class ClothData;
class ClothMotion;
class ModelCcs;                         // graphics/mmanage.h

// ══════════════════════════════════════════════════════════════════════
// Collision shapes (the model PK4's "cld" member)
// ══════════════════════════════════════════════════════════════════════

// A shape of the cld file, also the builder's heap copy of it.
struct MDL_CLD_SHAPE                    /* 0x30 */
{
    /* 0x00 */ int   type;              // 0 sphere (p[0]), 1 capsule (p[0]..p[1]), 2 plane (normal p[0] through p[1])
    /* 0x04 */ short coord[2];          // the SGD coordinate p[0] / p[1] move with
    /* 0x08 */ float size;              // radius (sphere, capsule)
    /* 0x0c */ float push;              // > 1e-5: a hit also pushes the body back along its move by this
    /* 0x10 */ sceVu0FVECTOR p[2];      // local points; the copy sets w = 1, a plane's normal gets y and z negated
};
static_assert(offsetof(MDL_CLD_SHAPE, coord) == 0x04, "MDL_CLD_SHAPE.coord");
static_assert(offsetof(MDL_CLD_SHAPE, size) == 0x08, "MDL_CLD_SHAPE.size");
static_assert(offsetof(MDL_CLD_SHAPE, push) == 0x0c, "MDL_CLD_SHAPE.push");
static_assert(offsetof(MDL_CLD_SHAPE, p) == 0x10, "MDL_CLD_SHAPE.p");
static_assert(sizeof(MDL_CLD_SHAPE) == 0x30, "MDL_CLD_SHAPE is 0x30 bytes");

// The cld member of a model PK4.
struct MDL_CLD_HEAD                     /* 0x10 + 0x30 * num */
{
    /* 0x00 */ int           num;
    /* 0x04 */ u_int         mask;      // collides with bodies whose mask shares a bit
    /* 0x08 */ int           _08;
    /* 0x0c */ int           _0c;
    /* 0x10 */ MDL_CLD_SHAPE shape[1];  // [num]
};
static_assert(offsetof(MDL_CLD_HEAD, mask) == 0x04, "MDL_CLD_HEAD.mask");
static_assert(offsetof(MDL_CLD_HEAD, shape) == 0x10, "MDL_CLD_HEAD.shape");

// A shape's points in the world, recomputed for each test.
struct MDL_CLD_WORLD                    /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR p[2];
    /* 0x20 */ int   hit;               // cleared each frame (ClearHit); set elsewhere
    /* 0x24 */ int   _24[3];
};
static_assert(offsetof(MDL_CLD_WORLD, hit) == 0x20, "MDL_CLD_WORLD.hit");
static_assert(sizeof(MDL_CLD_WORLD) == 0x30, "MDL_CLD_WORLD is 0x30 bytes");

// What collides with the shapes: a chain point of a ModelCcs (mmanage.h;
// ModelCollisionSet::HitCheck over the registry) or a cloth point of a
// ClothMotion (graphics/motion/motion.h, ClothMotion::Calc 0x2156c8).
struct MDL_CLD_BODY                     /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR pos;       // moved out of the shapes
    /* 0x10 */ sceVu0FVECTOR prev;      // pos of the step before (ModelCcs::Update)
    /* 0x20 */ sceVu0FVECTOR move;      // velocity; pushed back along by the shape's push
    /* 0x30 */ sceVu0FVECTOR force;     // added to move every step
    /* 0x40 */ int   off;               // != 0: no collision (a pinned point)
    /* 0x44 */ int   bone;              // ClothMotion: which of its two bones the point follows (0 / 1); chains: not read
    /* 0x48 */ u_int mask;
    /* 0x4c */ float r;                 // radius
};
static_assert(offsetof(MDL_CLD_BODY, move) == 0x20, "MDL_CLD_BODY.move");
static_assert(offsetof(MDL_CLD_BODY, off) == 0x40, "MDL_CLD_BODY.off");
static_assert(offsetof(MDL_CLD_BODY, bone) == 0x44, "MDL_CLD_BODY.bone");
static_assert(offsetof(MDL_CLD_BODY, mask) == 0x48, "MDL_CLD_BODY.mask");
static_assert(offsetof(MDL_CLD_BODY, r) == 0x4c, "MDL_CLD_BODY.r");
static_assert(offsetof(MDL_CLD_BODY, force) == 0x30, "MDL_CLD_BODY.force");
static_assert(sizeof(MDL_CLD_BODY) == 0x50, "MDL_CLD_BODY is 0x50 bytes");

// The model's collision shapes (no vtable; FF3 operator new(0x14)): heap
// copies of the cld shapes and their world points, on the model's SGD
// coordinates. ModelManager registers it (mmanage.h ModelCollisionSet).
// layout: host -- the pointers are 4 bytes on the EE; EE offsets in the comments.
class ModelCollision
{
public:
    // FF3 0x0020efd8 (the builder's; identical twin 0x0020eef0). coord: EE
    // address of the SGDCOORDINATEs (ModelObject::GetCoord), 0: none.
    ModelCollision(u_int coord, MDL_CLD_HEAD *cld);
    ~ModelCollision();                  // FF3 0x0020f140 (identical twin 0x0020f0c0)

    int  HitCheck(MDL_CLD_BODY *body);  // FF3 0x0020f1c0: 1 when a shape pushed body
    void ClearHit();                    // FF3 0x0020f270
    void CalcWorld(MDL_CLD_SHAPE *shape, MDL_CLD_WORLD *world);    // FF3 0x0020f2b0
    int  HitShape(MDL_CLD_BODY *body, int no);  // FF3 0x002bd160: shape no; 1 when it pushed body

    /* 0x00 */ MDL_CLD_SHAPE *shape;    // [num], heap
    /* 0x04 */ int            num;
    /* 0x08 */ u_int          mask;
    /* 0x0c */ u_int          coord;    // EE address of the SGDCOORDINATEs, 0: none
    /* 0x10 */ MDL_CLD_WORLD *world;    // [num], heap
};
static_assert(offsetof(ModelCollision, num) == sizeof(void *), "ModelCollision.num (EE +0x04)");

// ══════════════════════════════════════════════════════════════════════
// Builders
// ══════════════════════════════════════════════════════════════════════

// RTTI ModelBuilder (vtable 0x342050): the interface. Its code is inline,
// emitted at 0x2bd118..0x2bd160 and 0x2ac730 (stubs in lib/unassigned_stubs.c).
// layout: host -- the vptr is 4 bytes on the EE, 8 here.
class ModelBuilder
{
public:
    virtual ~ModelBuilder();            // FF3 0x002bd118 (deleting 0x002bd128)
    virtual void BuildObject();         // FF3 0x002bd148: vtable +0x08, empty
    virtual void BuildTexture();        // FF3 0x002ac730: vtable +0x0c, empty
    virtual void BuildMotion();         // FF3 0x002bd150: vtable +0x10, empty
    virtual MDL_MOT *GetResult();       // FF3 0x002bd158: vtable +0x14, nullptr
};

// RTTI NormalModelBuilder : ModelBuilder (vtable 0x342030; FF3 operator
// new(0x28)): a model without bones -- the ModelObject, the collision shapes
// and a ModelManager. PssModelBuilder (MapSky.h) derives from it.
// layout: host -- EE offsets in the comments.
class NormalModelBuilder : public ModelBuilder
{
public:
    // FF3 0x0020dfd0 (complete; base-object twin 0x0020df28). pk4: FF3's
    // one-word heap object holding the model PK4 (MdlPk4GetPart).
    explicit NormalModelBuilder(void *const *pk4);
    ~NormalModelBuilder() override;     // FF3 0x0020e110 (deleting 0x0020e1a8; base 0x0020e078)
    void BuildObject() override;        // FF3 0x0020e240
    void BuildMotion() override;        // FF3 0x0020e368
    MDL_MOT *GetResult() override;      // FF3 0x002ac738 (inline): result

    /* 0x04 */ MDL_MOT        *result;      // the ModelManager (BuildMotion)
    /* 0x08 */ ModelObject    *obj;         // BuildObject
    /* 0x0c */ void           *mpk;         // the PK4's members (mmanage.h MDL_PK4_PART)
    /* 0x10 */ void           *flgs;
    /* 0x14 */ void           *tpk;
    /* 0x18 */ void           *mono;
    /* 0x1c */ void           *mlt;
    /* 0x20 */ ModelCollision *collision;   // BuildObject, with a cld member
    /* 0x24 */ MDL_CLD_HEAD   *cld;
};
static_assert(offsetof(NormalModelBuilder, result) == sizeof(void *), "NormalModelBuilder.result (EE +0x04)");

// RTTI MotionModelBuilder : NormalModelBuilder (vtable 0x342010; FF3
// operator new(0x54)): adds the ccs object and the bone / shape / cloth
// motions, and a MotionModelManager over them.
// layout: host -- EE offsets in the comments.
class MotionModelBuilder : public NormalModelBuilder
{
public:
    // FF3 0x0020e448 (complete; base-object twin 0x0020e3b0): also the PK4's
    // ccs and cls members, the motion PK4 (mot) and the shape data (shp).
    MotionModelBuilder(void *const *pk4, void *mot, void *shp);
    ~MotionModelBuilder() override;     // FF3 0x0020e680 (deleting 0x0020e820; base 0x0020e4e0)
    void BuildTexture() override;       // FF3 0x0020e9c8
    void BuildMotion() override;        // FF3 0x0020ed58

    /* 0x28 */ ModelCcs     *ccs_obj;       // over ccs
    /* 0x2c */ BmdMotion    *bmd_mot;       // over mot
    /* 0x30 */ ShpMotion   **shp_mot;       // [shp_data->GetNum()]
    /* 0x34 */ ClothMotion **cloth_mot;     // [cloth_data->GetNum()]
    /* 0x38 */ BmdData      *bmd_data;
    /* 0x3c */ ShpData      *shp_data;
    /* 0x40 */ ClothData    *cloth_data;
    /* 0x44 */ void         *ccs;           // PK4 member "ccs"
    /* 0x48 */ void         *mot;           // the motion PK4 (one BMD per skeleton)
    /* 0x4c */ void         *shp;           // the shape data
    /* 0x50 */ void         *cls;           // PK4 member "cls": cloth
};

// The director (FF3 allocates it with operator new(4)): runs a builder's
// steps in order.
// layout: host -- one pointer (4 bytes on the EE).
class ModelDirector
{
public:
    explicit ModelDirector(ModelBuilder *builder) : builder(builder) {}
    void Construct();                   // FF3 0x0020ee10: BuildObject, BuildTexture, BuildMotion

    /* 0x00 */ ModelBuilder *builder;
};
static_assert(sizeof(ModelDirector) == sizeof(void *), "ModelDirector is one pointer");

#endif /* _GRAPHICS_MODEL_BUILDER_H */
