/* ==========================================================================
 *  graphics/motion/motion.h
 *
 *  FF3's motions (FF3 0x2125f8..0x217c90 and the inline virtuals at
 *  0x2bd8c0..0x2be0e0, RTTI): the motion data wrappers (MotionData: BmdData,
 *  ShpData, ClothData) and the motions that play them on a ModelObject
 *  (Motion: BoneMotion -> BmdMotion, VertexMotion -> ShpMotion ->
 *  ShpPhysicsMotion, ClothMotion). MotionModelManager
 *  (graphics/motion_model_manager.h) owns one bone motion and the shape and
 *  cloth motions of a model; the model builder (graphics/model_builder.h)
 *  makes them. FF3-only: FF2's motion.c is the C ANI_CTRL / MOT_CTRL system;
 *  only motQuaternion2Matrix survived (from MioPan).
 *
 *  The data (BMD, shape and cloth packs) is file data in emulated EE memory,
 *  read through host pointers; the BMD key table is relocated in place to EE
 *  addresses, as on the PS2 (SgdPtr / SgdAddr, sgd_types.h).
 * ======================================================================== */
#ifndef _GRAPHICS_MOTION_MOTION_H
#define _GRAPHICS_MOTION_MOTION_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"             // sceVu0FVECTOR, sceVu0FMATRIX

// The object a motion moves (ModelManager.ani_ctrl): graphics/model_object.h.
#include "graphics/model_object.h"

// ──────────────────────────────────────────────────────────────────────
// Data formats
// ──────────────────────────────────────────────────────────────────────

// A BMD (bone motion) entry of a motion PK4: the head, the parent table
// (short[num_bone], from +0x10), the key table (one word per frame, 16-aligned
// after the parents: an offset from the key data, which BmdData::Relocate turns
// into an EE address once), then the key data (16-aligned after the key
// table). Each key holds the bones' rotations (unit quaternions, float[4], or
// four u16 when packed: 8 bytes a bone, the block 16-aligned) and then their
// translations (float[4] a bone).
struct BMD_HEAD                         /* 0x10 + parents */
{
    /* 0x00 */ char   magic[4];         // "BMD\0" (sdata 0x371320; BmdMotion::FindBmd)
    /* 0x04 */ short  num_frame;        // keys, one a frame (BmdMotion::GetFrameNum)
    /* 0x06 */ short  _06;              // not read
    /* 0x08 */ short  num_bone;         // the builder takes the entry whose count is the SGD's uiNumBlock - 2
    /* 0x0a */ s_char half_rate;        // != 0: keys every second frame (BmdMotion::IsHalfRate)
    /* 0x0b */ u_char packed;           // != 0: rotations as four u16 (BmdData::UnpackQuat)
    /* 0x0c */ u_char mode_type0;       // the key mode of data type 0 (BmdData::GetKeyData)
    /* 0x0d */ u_char mode_trans;       // 2: keys > 0 hold the root bones' translations only
    /* 0x0e */ u_char _0e;              // not read
    /* 0x0f */ u_char relocated;        // set once the key table holds addresses
    /* 0x10 */ short  parent[1];        // [num_bone]: the parent bone, -1 for a root
};
static_assert(offsetof(BMD_HEAD, num_frame) == 0x04, "BMD_HEAD.num_frame");
static_assert(offsetof(BMD_HEAD, num_bone) == 0x08, "BMD_HEAD.num_bone");
static_assert(offsetof(BMD_HEAD, half_rate) == 0x0a, "BMD_HEAD.half_rate");
static_assert(offsetof(BMD_HEAD, packed) == 0x0b, "BMD_HEAD.packed");
static_assert(offsetof(BMD_HEAD, mode_type0) == 0x0c, "BMD_HEAD.mode_type0");
static_assert(offsetof(BMD_HEAD, mode_trans) == 0x0d, "BMD_HEAD.mode_trans");
static_assert(offsetof(BMD_HEAD, relocated) == 0x0f, "BMD_HEAD.relocated");
static_assert(offsetof(BMD_HEAD, parent) == 0x10, "BMD_HEAD.parent");

// BmdData::GetKeyData's data types.
enum
{
    BMD_KEY_TYPE0 = 0,                  // mode from the head (+0xc); no caller seen
    BMD_KEY_ROT   = 1,                  // rotations (quaternions)
    BMD_KEY_TRANS = 2,                  // translations
};

// One shape motion of a shape pack's motion PK4 (member 0): the head, then
// num_track tracks from +0x10, each SHP_TRACK + float[num_frame] weights,
// 16-aligned. A physics motion (type 1) has one track whose head is
// SHP_PHYSICS_TRACK and whose data is SHP_PHYSICS_PARAM.
struct SHP_MOT                          /* 0x10 + tracks */
{
    /* 0x00 */ int num_track;
    /* 0x04 */ int type;                // 0 ShpMotion, 1 ShpPhysicsMotion
    /* 0x08 */ int num_frame;
    /* 0x0c */ int mesh;                // the SGD (ModelObject::GetVectorInfo) the motion deforms
};
static_assert(sizeof(SHP_MOT) == 0x10, "SHP_MOT head");
static_assert(offsetof(SHP_MOT, mesh) == 0x0c, "SHP_MOT.mesh");

struct SHP_TRACK                        /* 0x10 + weights */
{
    /* 0x00 */ int   _00;               // not read
    /* 0x04 */ int   delta;             // the delta set (member of the delta PK4)
    /* 0x08 */ int   _08[2];            // not read
    /* 0x10 */ float weight[1];         // [num_frame]
};
static_assert(offsetof(SHP_TRACK, delta) == 0x04, "SHP_TRACK.delta");
static_assert(offsetof(SHP_TRACK, weight) == 0x10, "SHP_TRACK.weight");

struct SHP_PHYSICS_TRACK                /* 0x10 */
{
    /* 0x00 */ short bone;              // the coordinate whose movement drives it
    /* 0x02 */ short delta[6];          // delta sets for +x, -x, +y, -y, +z, -z (-1: none)
    /* 0x0e */ short _0e;               // not read
};
static_assert(sizeof(SHP_PHYSICS_TRACK) == 0x10, "SHP_PHYSICS_TRACK");
static_assert(offsetof(SHP_PHYSICS_TRACK, delta) == 0x02, "SHP_PHYSICS_TRACK.delta");

struct SHP_PHYSICS_PARAM                /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR point;     // in the bone's space
    /* 0x10 */ sceVu0FVECTOR force;     // added to the movement every step
    /* 0x20 */ float         scale;     // movement / force -> push
    /* 0x24 */ float         spring;    // pull back of the offset
    /* 0x28 */ float         damp;      // velocity kept a step
    /* 0x2c */ float         _2c;       // not read
};
static_assert(sizeof(SHP_PHYSICS_PARAM) == 0x30, "SHP_PHYSICS_PARAM");
static_assert(offsetof(SHP_PHYSICS_PARAM, force) == 0x10, "SHP_PHYSICS_PARAM.force");
static_assert(offsetof(SHP_PHYSICS_PARAM, scale) == 0x20, "SHP_PHYSICS_PARAM.scale");
static_assert(offsetof(SHP_PHYSICS_PARAM, damp) == 0x28, "SHP_PHYSICS_PARAM.damp");

// One delta set of the delta PK4 (member 1): count, then count vectors whose
// w holds the vertex index.
struct SHP_DELTA_VEC                    /* 0x10 */
{
    /* 0x00 */ float d[3];
    /* 0x0c */ int   vertex;
};
static_assert(sizeof(SHP_DELTA_VEC) == 0x10, "SHP_DELTA_VEC");

struct SHP_DELTA                        /* 0x10 + vectors */
{
    /* 0x00 */ int           num;
    /* 0x04 */ int           _04[3];    // not read
    /* 0x10 */ SHP_DELTA_VEC vec[1];    // [num]
};
static_assert(offsetof(SHP_DELTA, vec) == 0x10, "SHP_DELTA.vec");

// ──────────────────────────────────────────────────────────────────────
// Motion data (RTTI MotionData and its subclasses)
// ──────────────────────────────────────────────────────────────────────

// RTTI MotionData (vtable 0x3422d0): a motion file wrapper.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class MotionData
{
public:
    virtual ~MotionData();                      // FF3 0x002bdb68 (deleting 0x002bdb78)
};

// RTTI BmdData : MotionData (vtable 0x3422b0; FF3 operator new(0x14)): one
// BMD entry and its tables.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class BmdData : public MotionData
{
public:
    explicit BmdData(void *data);               // FF3 0x00214238 (base-object twin 0x00214200); data 0: none yet
    ~BmdData() override;                        // FF3 0x002bdb38 (deleting 0x002bdb48)
    void SetData(void *data);                   // FF3 0x00214300: the entry, its parent and key tables, Relocate
    int  Relocate(void *data, u_int base);      // FF3 0x00214270: 1 when it relocated now
    short GetParent(short bone);                // FF3 0x002142d0
    // FF3 0x00214358: key no's data of type for every bone (bone < 0) or
    // one, in buf or straight from the file; returns where it is.
    sceVu0FVECTOR *GetKeyData(sceVu0FVECTOR *buf, short no, short type, short bone);
    void  UnpackQuat(float *out, const u_short *in);        // FF3 0x00214628
    void *GetKeyAddr(short no, short type);     // FF3 0x002146c0

    /* 0x04 */ void     *data;
    /* 0x08 */ BMD_HEAD *head;                  // the same entry
    /* 0x0c */ short    *parent;                // head->parent
    /* 0x10 */ u_int    *key;                   // the key table: EE addresses once relocated
};
static_assert(offsetof(BmdData, data) == sizeof(void *), "BmdData.data (EE +0x04)");

// RTTI ShpData : MotionData (vtable 0x3423b8; FF3 operator new(8)): the
// model's shape (vertex) motions: a PK4 of the motion PK4 and the delta PK4.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class ShpData : public MotionData
{
public:
    explicit ShpData(void *data);               // FF3 0x002179a0
    ~ShpData() override;                        // FF3 0x002be0d0 (deleting 0x002be0e0)
    int   GetNum();                             // FF3 0x002179b8: the number of shape motions
    void *GetMotPack();                         // FF3 0x002179e8
    void *GetDeltaPack();                       // FF3 0x00217a08
    SHP_MOT   *GetMot(int no);                  // FF3 0x00217a28
    SHP_TRACK *GetTrack(int no, int track);     // FF3 0x00217a58
    SHP_PHYSICS_TRACK *GetFirstTrack(int no);   // FF3 0x00217ab0
    float     *GetTrackWeights(int no, int track);  // FF3 0x00217ad0
    SHP_PHYSICS_PARAM *GetPhysicsParam(int no); // FF3 0x00217af0
    SHP_DELTA     *GetDelta(int set);           // FF3 0x00217b10
    SHP_DELTA_VEC *GetDeltaVecs(int set);       // FF3 0x00217b40
    int   GetTrackNum(int no);                  // FF3 0x00217b60
    int   GetType(int no);                      // FF3 0x00217b80: 0 ShpMotion, 1 ShpPhysicsMotion
    int   GetMotFrameNum(int no);               // FF3 0x00217ba0
    int   GetMeshNo(int no);                    // FF3 0x00217bc0
    int   GetTrackDelta(int no, int track);     // FF3 0x00217be0
    float GetTrackWeight(int no, int track, int frame);     // FF3 0x00217c00
    float *GetTrackWeightList(int no, int track);           // FF3 0x00217c30
    int   GetDeltaNum(int set);                 // FF3 0x00217c48
    short GetFrameNum(int no);                  // FF3 0x00217c68

    /* 0x04 */ void *data;
};

// One cloth of a cloth PK4 (ClothData::GetCloth): a grid of w x h points,
// layer times over, one point a vertex of the mesh's unique vertex pool, row
// by row. The head; the parameters (CLOTH_FILE_PARAM) follow at +0x10.
struct CLOTH_HEAD                       /* 0x10 + param */
{
    /* 0x00 */ int _00[3];              // not read
    /* 0x0c */ int mesh;                // the SGD (ModelObject::GetVectorInfo / GetCoord) the cloth is
};
static_assert(offsetof(CLOTH_HEAD, mesh) == 0x0c, "CLOTH_HEAD.mesh");

// A cloth's parameters as stored (ClothData::GetParam); ClothMotion::CopyParam
// rearranges them into CLOTH_PARAM.
struct CLOTH_FILE_PARAM                 /* 0x40 + pins */
{
    /* 0x00 */ int   type;              // CLOTH_PARAM.type
    /* 0x04 */ u_int preset;            // < 3: stiffness, damping and gravity from cloth_preset (data 0x35caa0)
    /* 0x08 */ int   w;                 // points a row
    /* 0x0c */ int   h;                 // rows
    /* 0x10 */ int   bone[2];           // the coordinates the cloth hangs from (the second for types 2 / 5)
    /* 0x18 */ float stiff[2];          // link stiffness by link kind (every link is kind 0)
    /* 0x20 */ float damp;              // velocity kept a step
    /* 0x24 */ float gravity[3];        // added to the force every step
    /* 0x30 */ float angle;             // degrees: bone 1's frame turned about z (types 2 / 5)
    /* 0x34 */ u_int mask;              // the points' collision mask (MDL_CLD_BODY.mask); bit 2: no step while not drawn
    /* 0x38 */ int   _38;               // not read
    /* 0x3c */ int   num_pin;
    /* 0x40 */ int   pin[1];            // [num_pin]: pinned columns, counted from the right
};
static_assert(offsetof(CLOTH_FILE_PARAM, stiff) == 0x18, "CLOTH_FILE_PARAM.stiff");
static_assert(offsetof(CLOTH_FILE_PARAM, gravity) == 0x24, "CLOTH_FILE_PARAM.gravity");
static_assert(offsetof(CLOTH_FILE_PARAM, angle) == 0x30, "CLOTH_FILE_PARAM.angle");
static_assert(offsetof(CLOTH_FILE_PARAM, mask) == 0x34, "CLOTH_FILE_PARAM.mask");
static_assert(offsetof(CLOTH_FILE_PARAM, num_pin) == 0x3c, "CLOTH_FILE_PARAM.num_pin");
static_assert(offsetof(CLOTH_FILE_PARAM, pin) == 0x40, "CLOTH_FILE_PARAM.pin");

// The cloth types (CLOTH_PARAM.type).
enum
{
    CLOTH_TYPE_SHEET      = 0,          // one bone
    CLOTH_TYPE_TUBE       = 1,          // the first and last rows linked (a loop)
    CLOTH_TYPE_TWO_BONE   = 2,          // pinned points of the lower half rows follow bone 1; the layers linked
    CLOTH_TYPE_SHEET_F    = 3,          // types 3..5: as 0..2, and the points follow half the root bone's movement
    CLOTH_TYPE_TUBE_F     = 4,
    CLOTH_TYPE_TWO_BONE_F = 5,
};

// A cloth's parameters as the solver reads them (ClothMotion.param, a 0x40
// heap block made by ClothMotion::CopyParam).
struct CLOTH_PARAM                      /* 0x40 */
{
    /* 0x00 */ int   type;              // CLOTH_TYPE_*
    /* 0x04 */ int   num_vtx;           // the mesh's unique vertices (= the points)
    /* 0x08 */ int   w;
    /* 0x0c */ int   h;
    /* 0x10 */ int   layer;             // num_vtx / (w * h)
    /* 0x14 */ int   bone[2];
    /* 0x1c */ u_int mask;
    /* 0x20 */ float stiff[2];          // by CLOTH_LINK.kind
    /* 0x28 */ float damp;
    /* 0x2c */ float angle;             // degrees
    /* 0x30 */ sceVu0FVECTOR gravity;   // w 0 (a preset's w)
};
static_assert(sizeof(CLOTH_PARAM) == 0x40, "CLOTH_PARAM is 0x40 bytes");
static_assert(offsetof(CLOTH_PARAM, layer) == 0x10, "CLOTH_PARAM.layer");
static_assert(offsetof(CLOTH_PARAM, bone) == 0x14, "CLOTH_PARAM.bone");
static_assert(offsetof(CLOTH_PARAM, mask) == 0x1c, "CLOTH_PARAM.mask");
static_assert(offsetof(CLOTH_PARAM, stiff) == 0x20, "CLOTH_PARAM.stiff");
static_assert(offsetof(CLOTH_PARAM, damp) == 0x28, "CLOTH_PARAM.damp");
static_assert(offsetof(CLOTH_PARAM, angle) == 0x2c, "CLOTH_PARAM.angle");
static_assert(offsetof(CLOTH_PARAM, gravity) == 0x30, "CLOTH_PARAM.gravity");

// The stiffness / damping / gravity presets (data 0x35caa0, 3 of them).
struct CLOTH_PRESET                     /* 0x20 */
{
    /* 0x00 */ float stiff[2];
    /* 0x08 */ float damp;
    /* 0x0c */ float _0c;               // not read
    /* 0x10 */ float gravity[4];
};
static_assert(sizeof(CLOTH_PRESET) == 0x20, "CLOTH_PRESET is 0x20 bytes");
static_assert(offsetof(CLOTH_PRESET, gravity) == 0x10, "CLOTH_PRESET.gravity");

// A spring between two cloth points (ClothMotion.link, ClothMotion::InitLink).
struct CLOTH_LINK                       /* 0xc */
{
    /* 0x0 */ u_short a;                // point indices
    /* 0x2 */ u_short b;
    /* 0x4 */ float   len;              // rest length (the distance at Init)
    /* 0x8 */ int     kind;             // CLOTH_PARAM.stiff index; always 0
};
static_assert(sizeof(CLOTH_LINK) == 0xc, "CLOTH_LINK is 0xc bytes");
static_assert(offsetof(CLOTH_LINK, len) == 0x4, "CLOTH_LINK.len");
static_assert(offsetof(CLOTH_LINK, kind) == 0x8, "CLOTH_LINK.kind");

// The cloth points are collision bodies (graphics/model_builder.h).
struct MDL_CLD_BODY;

// RTTI ClothData : MotionData (vtable 0x342318; FF3 operator new(8)): a PK4,
// one member per cloth (CLOTH_HEAD).
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class ClothData : public MotionData
{
public:
    explicit ClothData(void *data);             // FF3 0x00216570
    ~ClothData() override;                      // FF3 0x002be008 (deleting 0x002be018)
    int   GetNum();                             // FF3 0x00216588: the number of cloth motions
    void *GetCloth(int no);                     // FF3 0x002165a0
    void *GetParam(int no);                     // FF3 0x002165b8: GetCloth + 0x10

    /* 0x04 */ void *data;
};

// ──────────────────────────────────────────────────────────────────────
// Motions (RTTI Motion and its subclasses)
// ──────────────────────────────────────────────────────────────────────

// RTTI Motion (vtable 0x342230), the root of the motions.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class Motion
{
public:
    explicit Motion(ModelObject *obj);          // FF3 0x00212618 (base-object twin 0x002125f8)
    virtual ~Motion();                          // EE vtable +0x00, FF3 0x002bd8c0 (deleting 0x002bd8d0)
    virtual int  Step();                        // EE vtable +0x08, FF3 0x002bd8f0: 1 at the end
    virtual int  Vtbl0C();                      // EE vtable +0x0c, FF3 0x002bd8f8: 0 (no caller seen)
    virtual int  IsEnd();                       // EE vtable +0x10, FF3 0x002bd900: 1

    void  Init(ModelObject *obj);               // FF3 0x00212638
    void  SetFrame(float frame);                // FF3 0x00212658: frame = max(frame, 0)
    float GetStep();                            // FF3 0x00212670: speed * GetFrameTimeScale()

    /* 0x04 */ ModelObject *obj;                // the object the motion moves
    /* 0x08 */ float        frame;              // GameMotionModel::GetMotFrame
    /* 0x0c */ float        speed;              // frames per step (MotionModelManager::SetSpeed)
};

// The root-yaw callback of a BmdMotion (+0x48): the yaw (radians) and the
// argument stored beside it (+0x4c). Nothing in the code read so far sets it.
typedef void (*BMD_YAW_FUNC)(float yaw, void *arg);

// RTTI BoneMotion : Motion (vtable 0x342200). The model's bone motion, the
// one a MotionModelManager hands out as its motion control (MOT_CTRL).
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class BoneMotion : public Motion
{
public:
    BoneMotion(ModelObject *obj, void *mot);    // FF3 0x002126f8 (base-object twin 0x002126a0)
    ~BoneMotion() override;                     // FF3 0x002bd908 (deleting 0x002bd918)
    int  Step() override;                       // FF3 0x00212750, EE vtable +0x08
    int  IsEnd() override;                      // FF3 0x002128d8: frame >= GetFrameNum() - 1 and no blend
    virtual void Pose(float *matrix);           // EE vtable +0x14, FF3 0x002bd938: empty
    virtual void SetMotion(int no);             // EE vtable +0x18, FF3 0x002bd940: empty
    virtual void EndBlend();                    // EE vtable +0x1c, FF3 0x002bd948: empty
    virtual void BlendPose(float *matrix);      // EE vtable +0x20, FF3 0x002bd950: empty
    virtual int  GetFrameNum();                 // EE vtable +0x24, FF3 0x002bd958: 0

    // FF3 0x00212840: one step with the world matrix (Pose, or BlendPose while
    // blending, then Step; a blend that ends goes on from its overshoot).
    int  Update(float *matrix);

    /* 0x10 */ void  *mot;                      // the motion PK4 (members: BMD entries)
    /* 0x14 */ void  *mot_ext;                  // a second PK4 searched when mot's member is no BMD (not set by code read so far)
    /* 0x18 */ int    no;                       // the motion playing (GameMotionModel::GetMotNo)
    /* 0x1c */ float  frame_over;               // frames run past the end (or the blend) on the last step; MotScrHdlFrame starts from it
    /* 0x20 */ float  blend_frame;              // frames blended so far
    /* 0x24 */ int    blend;                    // != 0 while blending (PlayMotion: blend frames != 0)
    /* 0x28 */ int    blend_num;                // blend frames (read as a short)
};

// RTTI BmdMotion : BoneMotion (vtable 0x342270; FF3 operator new(0x50)):
// keyframed bone rotations and translations into the object's coordinates.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class BmdMotion : public BoneMotion
{
public:
    // FF3 0x00212de8 (base-object twin 0x00212cb8): the object, the bone
    // data and the whole motion PK4.
    BmdMotion(ModelObject *obj, BmdData *data, void *mot);
    ~BmdMotion() override;                      // FF3 0x00213000 (base-object 0x00212f18, deleting 0x002130e8)
    void Pose(float *matrix) override;          // FF3 0x002133d8, EE vtable +0x14
    void SetMotion(int no) override;            // FF3 0x00213840, EE vtable +0x18
    void EndBlend() override;                   // FF3 0x00213da0, EE vtable +0x1c
    void BlendPose(float *matrix) override;     // FF3 0x00213a40, EE vtable +0x20
    int  GetFrameNum() override;                // FF3 0x002131f8, EE vtable +0x24

    int   IsHalfRate();                         // FF3 0x002131d0
    int   GetNum();                             // FF3 0x00213218: the motion count (PK4 word +8), 0 without
    short GetKeyFloor(float frame);             // FF3 0x00213230
    short GetKeyNext(float frame);              // FF3 0x00213290
    float GetKeyRatio(float frame, short key);  // FF3 0x002132e0
    short GetBoneNum();                         // FF3 0x002133b8
    void  ResetPose();                          // FF3 0x002137d0: the saved pose = the last one drawn
    BMD_HEAD *FindBmd(int no);                  // FF3 0x00213880
    short GetMotFrameNum(int no);               // FF3 0x00213920
    void  LinkCoords();                         // FF3 0x00213950
    float CalcYaw(float *q0, float *q1);        // FF3 0x00213dc8
    void  CalcBlendYaw();                       // FF3 0x00213f10
    short GetRootBone();                        // FF3 0x00214028
    void  ApplyMotionYaw();                     // FF3 0x002140b8
    void  ApplyBlendYaw();                      // FF3 0x002140e8
    void  CalcMotionYaw(float time);            // FF3 0x00214120

    /* 0x2c */ sceVu0FVECTOR *pose_rot;         // the pose a blend starts from: rotations, a bone each
    /* 0x30 */ sceVu0FVECTOR *pose_trans;       // ... translations
    /* 0x34 */ sceVu0FVECTOR *cur_rot;          // the pose drawn last: rotations
    /* 0x38 */ sceVu0FVECTOR *cur_trans;        // ... translations
    /* 0x3c */ BmdData       *data;
    /* 0x40 */ float          blend_yaw;        // root yaw of the blend (CalcBlendYaw / BlendPose)
    /* 0x44 */ float          motion_yaw;       // root yaw to the motion's end (CalcMotionYaw)
    /* 0x48 */ BMD_YAW_FUNC   yaw_func;
    /* 0x4c */ void          *yaw_arg;
};

// The motion control a motion object hands out (ModelManager::GetBoneMotion;
// MotScrHdlFrame 0x1d3958) is its BmdMotion: MOT_CTRL was its stand-in name.
typedef BmdMotion MOT_CTRL;

// RTTI VertexMotion : Motion (vtable 0x3421d8): the shape and cloth motions.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class VertexMotion : public Motion
{
public:
    VertexMotion(ModelObject *obj, int no);     // FF3 0x00212998 (base-object twin 0x00212940)
    ~VertexMotion() override;                   // FF3 0x002bd960 (deleting 0x002bd970)
    int  Step() override;                       // FF3 0x00212b00, EE vtable +0x08
    // FF3 0x00212bc8: 0 while held; reverse: frame <= 0; else frame >=
    // count - 1 (GetFrameNum of no).
    int  IsEnd() override;
    virtual void Stop();                        // FF3 0x00212ae0, EE vtable +0x14: playing = 0, SetFrame(0)
    virtual void Pose();                        // EE vtable +0x18, FF3 0x002bd990: empty
    virtual int  GetFrameNum(int no);           // EE vtable +0x1c, FF3 0x002bd998: 0

    void Init(ModelObject *obj, int no);        // FF3 0x002129f0
    // FF3 0x00212a40: start playing from frame (counted from the end when
    // reverse), clamped to the frame count.
    void Play(int loop, int stop_end, int reverse, float frame, float speed);
    // FF3 0x00212c40: one step while playing (Pose, Step; Stop at the end when
    // stop_end). 1 when it reached the end.
    int  Update();

    /* 0x10 */ int playing;
    /* 0x14 */ int loop;                        // at the end: back to the start (else hold the last frame)
    /* 0x18 */ int reverse;
    /* 0x1c */ int hold;                        // != 0: frame frozen, never ends (MotionModelManager::SetShapeLoop)
    /* 0x20 */ int stop_end;                    // != 0: Stop at the end
    /* 0x24 */ int no;                          // the shape / cloth motion
};

// RTTI ShpMotion : VertexMotion (vtable 0x342368; FF3 operator new(0x38)):
// weighted vertex deltas onto one mesh, from a base copy of its vertices.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class ShpMotion : public VertexMotion
{
public:
    ShpMotion();                                // FF3 0x00216f70 (base-object twin 0x00216f28)
    ~ShpMotion() override;                      // FF3 0x00217020 (base-object 0x00216fb8, deleting 0x00217088)
    void  Stop() override;                      // FF3 0x00217200: WriteVertex(1), then VertexMotion::Stop
    void  Pose() override;                      // FF3 0x002172e8, EE vtable +0x18
    int   GetFrameNum(int no) override;         // FF3 0x00217430, EE vtable +0x1c
    // EE vtable +0x20 (FF3 0x002be038, inline): Stop through the vtable.
    virtual void StopVirtual() { Stop(); }
    void  Init(ModelObject *obj, ShpData *data, int no);    // FF3 0x002170f8
    int   GetMeshNo();                          // FF3 0x002172c8: the SGD it deforms
    // FF3 0x00217130: nullptr: a heap copy of its mesh's vertices; else
    // shares buf. Returns the buffer it uses.
    sceVu0FVECTOR *SetVertexBuf(sceVu0FVECTOR *buf);
    // FF3 0x00217230: when force or dirty, the base vertices back into its
    // mesh; 1 when written.
    int   WriteVertex(int force);
    // FF3 0x00217460: vtx[vec.vertex] += vec * weight for n vectors.
    void  AddDelta(float weight, sceVu0FVECTOR *vtx, SHP_DELTA_VEC *vec, int n);

    /* 0x28 */ ShpData       *data;
    /* 0x2c */ sceVu0FVECTOR *vbuf;             // the mesh's base vertices
    /* 0x30 */ sceVu0FVECTOR *own;              // vbuf when this motion made it (freed by it)
    /* 0x34 */ int            dirty;            // the mesh vertices hold deltas
};

// RTTI ShpPhysicsMotion : ShpMotion (vtable 0x342338; FF3 operator new(0x70)):
// a damped spring driven by a bone's movement, shown through six delta sets.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class ShpPhysicsMotion : public ShpMotion
{
public:
    ShpPhysicsMotion();                         // FF3 0x00217508 (base-object twin 0x002174a8)
    ~ShpPhysicsMotion() override;               // FF3 0x002be060 (deleting 0x002be088)
    void Pose() override;                       // FF3 0x00217810, EE vtable +0x18
    int  GetFrameNum(int no) override;          // FF3 0x002be0c8, EE vtable +0x1c: 0
    void StopVirtual() override;                // FF3 0x002be0c0, EE vtable +0x20: empty
    void Calc();                                // FF3 0x00217568

    /* 0x40 */ sceVu0FVECTOR offset;            // the spring's offset, each axis in [-1, 1]
    /* 0x50 */ sceVu0FVECTOR prev;              // the bone point last step (world)
    /* 0x60 */ sceVu0FVECTOR vel;
};

// The matrices a ClothMotion keeps from its last step (ClothMotion.mat), a
// pair each (bone 0, bone 1): the bones' world matrices, their rotations, the
// inverses of the world matrices and the inverse rotations (the step stores
// the rotations again there; nothing reads them), then bone 1's matrix in
// bone 0's space (ClothMotion::WriteVertex).
enum
{
    CLOTH_MAT_WORLD = 0,
    CLOTH_MAT_ROT   = 2,
    CLOTH_MAT_INV   = 4,
    CLOTH_MAT_RINV  = 6,
    CLOTH_MAT_BONE1 = 8,
};

// RTTI ClothMotion : VertexMotion (vtable 0x3422e0; FF3 operator new(0x300)):
// a mass-spring cloth on one mesh. The points (MDL_CLD_BODY, one a vertex)
// live in bone 0's space between steps; a step takes them to the world with
// the bones' last matrices, adds wind and gravity, relaxes the links twice
// against the collision registry and takes them back with the new matrices.
// layout: host -- vptr and pointers 4 bytes on the EE; EE offsets in the comments.
class ClothMotion : public VertexMotion
{
public:
    ClothMotion();                              // FF3 0x002148f0 (base-object twin 0x00214758)
    ~ClothMotion() override;                    // FF3 0x00214b50 (base-object 0x00214a88, deleting 0x00214c18)
    void Stop() override;                       // FF3 0x002156a0, EE vtable +0x14: VertexMotion::Stop, Reset
    // FF3 0x002162b8, EE vtable +0x18: settle on the first call, then one
    // step a frame and the mesh written.
    void Pose() override;
    int  GetFrameNum(int no) override;          // FF3 0x002bdb98, EE vtable +0x1c: 0
    void Init(ModelObject *obj, ClothData *data, int arg, int no);  // FF3 0x00214ce8
    void Reset();                               // FF3 0x00216410: the cloth back to its rest shape
    // FF3 0x00216520: wind along dir (copied) with power, gusts every period
    // frames; sets wind.
    void SetWind(const float *dir, int period, float power);

    CLOTH_PARAM *CopyParam();                   // FF3 0x00214e28: a heap CLOTH_PARAM from the file's
    int  CountLink();                           // FF3 0x00215208
    void InitPoint();                           // FF3 0x00214fd0
    void InitLink();                            // FF3 0x00215278
    // FF3 0x002156c8: one solver step; collide != 0: the jump / turn checks
    // and the collision registry (0 while settling).
    void Calc(int collide);
    void CalcWind();                            // FF3 0x002bdd90 (out-of-line inline)
    void CalcLink();                            // FF3 0x002bdba0 (out-of-line inline)
    void WriteVertex();                         // FF3 0x002161d8: the points into the mesh

    /* 0x28 */ sceVu0FVECTOR *vbuf;             // the mesh's rest vertices (Init)
    /* 0x2c */ sceVu0FVECTOR *own;              // freed by the destructor
    /* 0x30 */ ModelObject   *cloth_obj;        // the object again (Init)
    /* 0x34 */ ClothData     *data;
    /* 0x38 */ CLOTH_PARAM   *param;            // CopyParam
    /* 0x3c */ MDL_CLD_BODY  *point;            // [num_point]
    /* 0x40 */ CLOTH_LINK    *link;             // [num_link]
    /* 0x44 */ int            num_point;
    /* 0x48 */ int            num_link;
    /* 0x4c */ int            stepped;          // mat / vec hold a step's (Calc sets it, Reset clears it)
    /* 0x50 */ int            reset_req;        // != 0: Reset on the next step (MotionModelManager::ResetCloth; Calc on a jump or turn)
    /* 0x54 */ int            calc_off;         // != 0: Calc does nothing (no writer seen)
    /* 0x58 */ int            settled;          // Pose ran the settling steps
    /* 0x5c */ int            written;          // WriteVertex ran
    /* 0x60 */ sceVu0FMATRIX  mat[9];           // CLOTH_MAT_*; unit matrices at construction
    // The root bone's world position at the last step: [0] for the jump
    // check, [2] for the follow shift (types 3..5); [1] its direction about
    // the origin on the xz plane, for the turn check. Zero at construction.
    /* 0x2a0 */ sceVu0FVECTOR vec[3];
    /* 0x2d0 */ int           wind;             // != 0: the wind blows (SetWind; MotionModelManager::StopClothWind clears it)
    /* 0x2d4 */ int           _2d4[3];
    /* 0x2e0 */ sceVu0FVECTOR wind_dir;
    /* 0x2f0 */ float         wind_power;
    /* 0x2f4 */ short         wind_period;      // frames a gust cycle, before the random stretch
    /* 0x2f6 */ short         wind_cycle;       // this cycle's frames
    /* 0x2f8 */ short         wind_frame;       // frame in the cycle
    /* 0x2fa */ short         wind_rise;        // frames of the gust's rise
    /* 0x2fc */ short         wind_fall;        // the frame the fall starts (calm before it)
    /* 0x2fe */ short         _2fe;
};

// The motion object a model keeps at +4 (GameModel.mot, graphics/game_model.h,
// made by the model builder) is RTTI ModelManager / MotionModelManager:
// graphics/mmanage.h (MDL_MOT, its old stand-in name, is a typedef there).
#include "graphics/mmanage.h"

// FF3 0x002bd9a0 (an out-of-line inline): spherical interpolation from q0
// (t = 0) to q1 (t = 1) the short way round, into out; returns out.
float *motQuatSlerp(float t, float *out, float *q0, float *q1);

// Unit quaternion (x, y, z, w) -> rotation matrix, row 3 = (0, 0, 0, 1).
// FF3 0x002bb108 (after the g3dCore linkonce kernels; also called by the
// motion code 0x2133d8.. and 0x1de8c8); FF2 motQuaternion2Matrix.
void  motQuaternion2Matrix(float (*m)[4], float *q);

#endif /* _GRAPHICS_MOTION_MOTION_H */
