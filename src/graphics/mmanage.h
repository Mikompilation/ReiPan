/* ==========================================================================
 *  graphics/mmanage.h
 *
 *  FF3 0x20f388..0x210818 (file name kept from FF2's link order; FF2's
 *  mmanage.c shares only the shape of the forwarding wrappers):
 *  - the model collision registry (data 0x32d6c0): the ModelCollisions of
 *    the models alive, tested against every chain point;
 *  - ModelCcs: the chain physics over a model PK4's "ccs" member (hair,
 *    ribbons: points on links, pushed by gravity, wind and collisions, written
 *    into the model's SGD coordinates);
 *  - MdlPk4GetPart: a model PK4's members by name;
 *  - RTTI ModelManager: what a model builder makes (GameModel.mot), over the
 *    ModelObject and its collision shapes. MotionModelManager derives from it
 *    (graphics/motion_model_manager.c, the next file).
 * ======================================================================== */
#ifndef _GRAPHICS_MMANAGE_H
#define _GRAPHICS_MMANAGE_H

#include <stddef.h>                 // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"             // sceVu0FVECTOR

class  ModelObject;                 // graphics/motion/motion.h
class  BmdMotion;                   // graphics/motion/motion.h
typedef BmdMotion MOT_CTRL;         // the motion control (graphics/motion/motion.h)
class  ModelCollision;              // graphics/model_builder.h
struct MDL_CLD_BODY;                // graphics/model_builder.h
struct G3DLIGHT;                    // graphics/graph3d/g3dLight.h

// ══════════════════════════════════════════════════════════════════════
// The collision registry
// ══════════════════════════════════════════════════════════════════════

#define MODEL_COLLISION_SET_NUM 8

// The ModelCollisions the chain points collide with (no vtable; one static
// instance, data 0x32d6c0, built and destroyed by the file's static
// initializer 0x20f648). ModelManager adds its collision in its constructor
// and removes it in its destructor; MapLoadRoomMission adds and removes the
// room model's as the room buffer is shown and hidden. FF2's MissionCtrl*
// names sat on these functions (wrong matches).
// layout: host -- 8 EE words; the slots are pointers here.
class ModelCollisionSet
{
public:
    ModelCollisionSet();                    // FF3 0x0020f3a0 (base-object twin 0x0020f388): Init
    ~ModelCollisionSet();                   // FF3 0x0020f3d0 (base-object twin 0x0020f3b8): Term

    void Init();                            // FF3 0x0020f3e8: empties every slot (IngameInit)
    void Term();                            // FF3 0x0020f410: empties every slot (IngameEnd)
    int  FindFree();                        // FF3 0x0020f438: first empty slot, -1 when full
    int  Find(ModelCollision *cld);         // FF3 0x0020f468: cld's slot, -1 when absent or null
    int  Add(ModelCollision *cld);          // FF3 0x0020f4a0: 1 when cld went into a free slot
    int  Remove(ModelCollision *cld);       // FF3 0x0020f510: 1 when cld's slot was emptied
    // FF3 0x0020f560: ModelCollision::HitCheck(body) of every set (ClearHit
    // first when clear); 1 when one pushed body. 0 without a body.
    int  HitCheck(MDL_CLD_BODY *body, int clear);
    void ClearHit();                        // FF3 0x0020f5f8: ModelCollision::ClearHit of every set (IngameMain)

    /* 0x00 */ ModelCollision *slot[MODEL_COLLISION_SET_NUM];
};

extern ModelCollisionSet model_collision_set;           // data 0x32d6c0

// ══════════════════════════════════════════════════════════════════════
// Model PK4 members
// ══════════════════════════════════════════════════════════════════════

// The members of a model PK4, by name (table data 0x32d6e0).
enum MDL_PK4_PART
{
    MDL_PK4_MPK = 0,                    // "mpk"
    MDL_PK4_TPK = 1,                    // "tpk": the model's textures (a PK4 of TIM2s)
    MDL_PK4_MLT = 2,                    // "mlt"
    MDL_PK4_CCS = 3,                    // "ccs": the chains (MDL_CCS_HEAD, ModelCcs)
    MDL_PK4_CLD = 4,                    // "cld"
    MDL_PK4_CLS = 5,                    // "cls": cloth (ClothData)
    MDL_PK4_MONO = 6,                   // "mono" (TextureModelObject +0x14)
    MDL_PK4_FLGS = 7,                   // "flgs" (ModelObject +0x0c)
};

// FF3 0x002104c0: the data of member `part` of the model PK4 *pk4, or 0.
// FF3 passes a one-word heap object holding the PK4's address.
void *MdlPk4GetPart(void *const *pk4, int part);

// ══════════════════════════════════════════════════════════════════════
// ModelCcs: chain physics
// ══════════════════════════════════════════════════════════════════════

// A point of the ccs member (file data, 0x30).
struct MDL_CCS_POINT                    /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR pos;       // rest position, in coordinate 0's space
    /* 0x10 */ sceVu0FVECTOR force;     // added to the velocity every step (gravity)
    /* 0x20 */ int           pinned;    // != 0: stays at pos, no collision
    /* 0x24 */ u_int         mask;      // collides with ModelCollisions sharing a bit
    /* 0x28 */ float         r;         // radius
    /* 0x2c */ int           _2c;
};
static_assert(sizeof(MDL_CCS_POINT) == 0x30, "MDL_CCS_POINT is 0x30 bytes");
static_assert(offsetof(MDL_CCS_POINT, force) == 0x10, "MDL_CCS_POINT.force");
static_assert(offsetof(MDL_CCS_POINT, pinned) == 0x20, "MDL_CCS_POINT.pinned");
static_assert(offsetof(MDL_CCS_POINT, mask) == 0x24, "MDL_CCS_POINT.mask");
static_assert(offsetof(MDL_CCS_POINT, r) == 0x28, "MDL_CCS_POINT.r");

// The ccs member's head: the point count, the damping, then the points.
// Point i drives SGD coordinate i (the links i -> i + 1 set coordinates 1..n-1).
struct MDL_CCS_HEAD                     /* 0x10 + num * 0x30 */
{
    /* 0x00 */ u_short       num;
    /* 0x02 */ u_short       _02;
    /* 0x04 */ float         _04;
    /* 0x08 */ float         damp;      // the velocity is scaled by it every step
    /* 0x0c */ int           _0c;
    /* 0x10 */ MDL_CCS_POINT point[1];  // [num]
};
static_assert(offsetof(MDL_CCS_HEAD, damp) == 0x08, "MDL_CCS_HEAD.damp");
static_assert(offsetof(MDL_CCS_HEAD, point) == 0x10, "MDL_CCS_HEAD.point");

// A link between two points and its rest length (heap, 0xc).
struct MDL_CCS_LINK                     /* 0xc */
{
    /* 0x00 */ short a;
    /* 0x02 */ short b;
    /* 0x04 */ float len;
    /* 0x08 */ int   _08;
};
static_assert(sizeof(MDL_CCS_LINK) == 0xc, "MDL_CCS_LINK is 0xc bytes");
static_assert(offsetof(MDL_CCS_LINK, len) == 0x04, "MDL_CCS_LINK.len");

// Wind kinds (ModelCcs::SetWind; MapObjSetWindCloth passes the furniture
// row's wind strength). Kind 4 has no gusts: the last point is moved off the
// first every step instead.
enum
{
    MDL_CCS_WIND_HANG = 4,
};

// The model builder's object over the ccs PK4 member (MotionModelBuilder::
// BuildTexture; no vtable; FF3 operator new(0x60)). The points are bodies
// the registered ModelCollisions push (MDL_CLD_BODY, model_builder.h).
// layout: host -- the pointers are 4 bytes on the EE; EE offsets in the comments.
class ModelCcs
{
public:
    // FF3 0x0020f7b8 (complete; base-object twin 0x0020f700). coord: EE
    // address of the model's SGDCOORDINATEs (ModelObject::GetCoord).
    ModelCcs(u_int coord, void *ccs);
    ~ModelCcs();                        // FF3 0x0020f8c8 (base-object twin 0x0020f870): frees the arrays

    void  Update();                     // FF3 0x0020f920: one step, then the coordinates (MotionModelManager::SetMatrix)
    void  SetCoordinate();              // FF3 0x0020fd08
    void  Reset();                      // FF3 0x0020ffe0: points and links from the data
    void  CalcWind();                   // FF3 0x00210158
    float GetWindPower();               // FF3 0x002102a0
    void  SetWindDir();                 // FF3 0x00210318: a random horizontal direction
    void  SetWind(int kind);            // FF3 0x002103e0
    void  StopWind();                   // FF3 0x002104b8

    /* 0x00 */ u_int          coord;    // EE address of the SGDCOORDINATEs
    /* 0x04 */ u_int          _04[3];
    /* 0x10 */ int            wind;     // != 0: CalcWind runs
    /* 0x14 */ int            wind_kind;
    /* 0x18 */ u_int          _18[2];
    /* 0x20 */ sceVu0FVECTOR  wind_dir;
    /* 0x30 */ float          wind_power;
    /* 0x34 */ u_int          wind_cnt; // frame in the gust period
    /* 0x38 */ u_int          wind_period;
    /* 0x3c */ u_int          _3c;
    /* 0x40 */ MDL_CCS_HEAD  *data;
    /* 0x44 */ MDL_CLD_BODY  *point;    // [point_num], system heap
    /* 0x48 */ MDL_CCS_LINK  *link;     // [link_num], system heap
    /* 0x4c */ short          point_num;
    /* 0x4e */ short          link_num; // point_num - 1
    /* 0x50 */ int            stop;     // != 0: Update does nothing
    /* 0x54 */ int            reset;    // != 0: Update resets first
};
static_assert(offsetof(ModelCcs, wind) == 0x10, "ModelCcs.wind");
static_assert(offsetof(ModelCcs, wind_dir) == 0x20, "ModelCcs.wind_dir");
static_assert(offsetof(ModelCcs, wind_period) == 0x38, "ModelCcs.wind_period");
static_assert(offsetof(ModelCcs, data) == 0x40, "ModelCcs.data");

// ══════════════════════════════════════════════════════════════════════
// ModelManager
// ══════════════════════════════════════════════════════════════════════

// RTTI ModelManager (vtable 0x3420e0, 21 slots in the order declared below;
// FF3 operator new(0xc)): what NormalModelBuilder::BuildMotion makes, over the
// ModelObject and its collision shapes. Most slots are empty or return 0
// here (inline bodies 0x2bd7b0..0x2bd838); MotionModelManager (vtable
// 0x342080, graphics/motion_model_manager.h) overrides them with its bone,
// shape and cloth motions -- the slot names say what its overrides do; "no"
// is a shape or cloth motion's index, < 0 for all of them where noted.
// MDL_MOT is its old stand-in name (a typedef now).
// layout: host -- the vptr and pointers are 4 bytes on the EE; EE offsets in the comments.
class ModelManager
{
public:
    // FF3 0x002105c0 (complete; base-object twin 0x00210578): registers
    // collision (model_collision_set) when there is one.
    ModelManager(ModelObject *obj, ModelCollision *collision);
    virtual ~ModelManager();            // FF3 0x00210640: vtable +0x00 (deleting +0x04 0x00210678; base 0x00210608)

    // FF3 0x00210768, vtable +0x08: the world matrix (times diag(1, -1, -1, 1))
    // into the object's coordinates, then its bone coordinates.
    virtual void SetMatrix(float *matrix);
    // vtable +0x0c: bone motion no from frame, blending over blend frames
    // when != 0 (MotScrHdlFrame, MapDBuff's anime)
    virtual void PlayMotion(int no, int blend, float frame, float speed);      // FF3 0x002bd7b0: empty
    // vtable +0x10: VertexMotion::Play(loop, stop_end, reverse, frame, speed)
    // of shape motion no (MotScrHdlPlay, the face channels)
    virtual void PlayShape(int no, int loop, int stop_end, int reverse, float frame, float speed);  // FF3 0x002bd7b8: empty
    // vtable +0x14: cloth motion no (< 0: all) from frame (GameModel::Build:
    // -1, 0, 0.0, 1.0 when the PK4 has a cls member); arg is not read
    virtual void PlayCloth(int no, int arg, float frame, float speed);         // FF3 0x002bd7c0: empty
    // vtable +0x18: cloth motion no (< 0: all) back to rest, on the next
    // step when defer != 0 (the model resets pass -1)
    virtual void ResetCloth(int no, int defer);                                // FF3 0x002bd7c8: empty
    // vtable +0x1c: wind on cloth motion no (< 0: all): MapObjSetWind passes
    // -1, a direction vector, a period and a strength
    virtual void SetClothWind(int no, const float *dir, int period, float power);  // FF3 0x002bd7d0: empty
    virtual void StopClothWind(int no); // FF3 0x002bd7d8, vtable +0x20: wind off (no < 0: all); empty
    virtual void SetSpeed(float speed); // FF3 0x002bd7e0, vtable +0x24: every motion's speed (GameAnimeModel::SetAnimeSpeed); empty
    virtual void SetFrame(float frame); // FF3 0x002bd7e8, vtable +0x28: every motion's frame (no host caller yet); empty
    virtual void StopShape(int no);     // FF3 0x002bd7f0, vtable +0x2c: stop shape motion no (PlyrCameraFaceUpdate); empty
    virtual void StopShapeAll(void);    // FF3 0x002bd7f8, vtable +0x30: stop every shape motion (no host caller yet); empty
    virtual void WriteShapeAll(void);   // FF3 0x002bd800, vtable +0x34: every shape motion's vertices into the meshes; empty
    virtual int  IsMotionEnd(void);     // FF3 0x002bd808, vtable +0x38: the bone motion has ended (MotScrHdl's "done" test); 0
    virtual int  IsShapeEnd(int no);    // FF3 0x002bd810, vtable +0x3c: shape motion no has ended (the face channels); 0
    virtual MOT_CTRL *GetBoneMotion(void);  // FF3 0x002bd818, vtable +0x40: the bone motion (motion control); nullptr
    virtual ModelCcs *GetCcs(void);     // FF3 0x002bd820, vtable +0x44: the chains (MapObjSetWindCloth); nullptr
    virtual int  GetShapeNum(void);     // FF3 0x002bd828, vtable +0x48: the number of shape motions; 0
    virtual void SetShapeLoop(int no, int loop);    // FF3 0x002bd830, vtable +0x4c (MotScrHdlPlay modes 2/3, loop 1: sets VertexMotion.hold, the frame frozen); empty
    virtual ModelCollision *GetCollision(void); // FF3 0x002bd838, vtable +0x50: collision (MapLoadRoomMission)

    // Forwarders to the object (FF3 jumps to ModelObject's member with this->obj).
    void Draw();                                    // FF3 0x002106c0 (MapObjMdlDraw, GameModel::DrawMdl)
    void DrawAlpha();                               // FF3 0x002106d8 (no callers)
    void ExecPrelight(float *trans, float *rot);    // FF3 0x002106f0 (MapDrawInitRoom, MapObjRegist)
    void ExecPrelightMatrix(float *matrix);         // FF3 0x00210708 (CMapDBuff::ReqMdl, MapObjReqMdlRoom)
    void ClearPrelight();                           // FF3 0x00210720 (MapDrawPrelightRoom, MapObjReqMdlRoom)
    void DrawShadow(G3DLIGHT *light);               // FF3 0x00210738 (MapDrawPlyrShadow)
    void ReqDraw();                                 // FF3 0x00210750 (MapDrawRoom, MapObjMdlDraw)

    /* 0x04 */ ModelObject    *ani_ctrl;    // the object (ani_ctrl->GetSgd(0) is the model's SGD)
    /* 0x08 */ ModelCollision *collision;   // registered while the manager lives; may be nullptr
};
static_assert(offsetof(ModelManager, ani_ctrl) == sizeof(void *), "ModelManager.ani_ctrl (EE +0x04)");
static_assert(offsetof(ModelManager, collision) == 2 * sizeof(void *), "ModelManager.collision (EE +0x08)");
typedef ModelManager MDL_MOT;

// The model space -> world matrix flip (data 0x329400): y and z negated.
// ModelManager::SetMatrix and MotionModelManager::SetMatrix apply it.
extern sceVu0FMATRIX mdl_flip_yz;

#endif /* _GRAPHICS_MMANAGE_H */
