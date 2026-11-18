/* ==========================================================================
 *  graphics/game_model.c
 *
 *  GameModel and GameMotionModel (FF3's own C++ classes, RTTI names; no FF2
 *  counterpart): a model PK4 from the shared file cache (plus a motion file
 *  and a second file for GameMotionModel) that a model builder turns into the
 *  motion object at +4 once loaded, and the thin calls the game makes on it
 *  (world matrix, draw, alpha, texture frames, coordinates). FF3
 *  0x1d2a40..0x1d36f0 and GameModel's inline virtuals 0x2ac568..0x2ac590 /
 *  0x2ac980. Written from the FF3 decompilation.
 *
 *  FF3 compiles each constructor and destructor twice (complete and
 *  base-object, same body) plus a deleting destructor; one C++ definition
 *  stands for each set.
 * ======================================================================== */
#include "graphics/game_model.h"

#include <float.h>                      // FLT_MAX
#include <math.h>                       // sqrtf
#include <new>                          // placement new on FF3's C++ heap (operator_new)

#include "common/cpp_heap.h"            // operator_new / operator_delete
#include "common/pk4.h"                 // Pk4GetData
#include "common/vu0math.h"             // Vu0Acosf, RotLimitChk
#include "graphics/graph3d/gra3d.h"     // gra3dSetGsRegisterDefault
#include "graphics/graph3d/sgd_types.h" // SGDCOORDINATE, SgdPtr
#include "graphics/mmanage.h"           // MdlPk4GetPart, ModelManager
#include "graphics/model_builder.h"     // the builders, ModelDirector
#include "graphics/motion/motion.h"     // MDL_MOT, MDL_OBJ (ModelObject), MOT_CTRL
#include "ingame/enemy/random_ghost.h"    // the free hooks
#include "system/eeiop/file_cache.h"    // file_cache

// The reference axes of GetCoordRot (data 0x329450 / 0x329460).
alignas(16) static const float game_model_axis_y[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
alignas(16) static const float game_model_axis_z[4] = { 0.0f, 0.0f, 1.0f, 0.0f };

// The speed of the motion Build starts (sdata 0x370ac8 / 0x370ad4, 1.0).
#define GAME_MODEL_MOT_SPEED 1.0f

// ══════════════════════════════════════════════════════════════════════
// GameModel
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// No motion object, builder or file; the free hooks release enemies.
// FF3 0x001d2a80 (complete object) and FF3 0x001d2a40 (the base-object twin
// the derived constructors call).

GameModel::GameModel()
{
    mot     = nullptr;
    builder = nullptr;
    hook[1] = RandGhostCandleRelease;
    pk4     = nullptr;
    id      = -1;
    hook[0] = RandGhostReleaseAll;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the builder and lets go of the model file. The motion object is
// the builder's.
// FF3 0x001d2b18 (vtable +0x08), FF3 0x001d2ac0 (base object), FF3 0x001d2b70
// (deleting: then operator_delete).
// callcheck: GameModelFileRelease (FileRelease)

GameModel::~GameModel()
{
    if (builder != nullptr)
    {
        // FF3: the builder's vtable +4, its deleting destructor
        builder->~ModelBuilder();
        operator_delete(builder);
    }
    FileRelease(id);
}

// ──────────────────────────────────────────────────────────────────────
// Turns the loaded PK4 into the motion object: a MotionModelBuilder when the
// PK4 has a cls member, else a NormalModelBuilder; the director runs it and
// its result becomes mot. With a cls member motion -1 starts at once.
// FF3 0x001d2bc8 (vtable +0x00).
// callcheck: NormalModelBuilderCtor, MotionModelBuilderCtor (the builders' constructors, new-expressions); GetResult (builder vtable +0x14); PlayCloth (mot vtable +0x14)

void GameModel::Build()
{
    void         **holder;
    ModelDirector *dir;

    // PC: FF3 allocates one 4-byte word for each of these (EE pointers)
    holder  = (void **)operator_new(sizeof(void *));
    *holder = pk4;
    if (MdlPk4GetPart(holder, MDL_PK4_CLS) != nullptr)
        builder = new (operator_new(sizeof(MotionModelBuilder))) MotionModelBuilder(holder, nullptr, nullptr);
    else
        builder = new (operator_new(sizeof(NormalModelBuilder))) NormalModelBuilder(holder);

    dir = new (operator_new(sizeof(ModelDirector))) ModelDirector(builder);
    dir->Construct();
    operator_delete(dir);

    // FF3 deletes holder here and still reads it for the second
    // MdlPk4GetPart below; the host deletes it last.
    mot = builder->GetResult();

    if (MdlPk4GetPart(holder, MDL_PK4_CLS) != nullptr)
        mot->PlayCloth(-1, 0, 0.0f, GAME_MODEL_MOT_SPEED);

    if (holder != nullptr)
        operator_delete(holder);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac568 (vtable +0x04): nothing; GameMotionModel::IsLoaded calls
// it right before Build.

void GameModel::BuildPre()
{
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the model file is in; builds the motion object the first time.
// FF3 0x001d2cf0 (vtable +0x10).
// callcheck: FileCacheIsLoaded (file_cache.IsLoaded)

int GameModel::IsLoaded()
{
    if (file_cache.IsLoaded(id) == 0)
        return 0;
    if (builder == nullptr)
        Build();                        // FF3: vtable +0x00
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac570 (vtable +0x14): always drawn.

int GameModel::IsDrawn()
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The world matrix to the motion object.
// FF3 0x001d3010 (vtable +0x18).
// callcheck: ModelManagerSetMatrix (mot vtable +0x08)

void GameModel::SetWorldMatrix(sceVu0FMATRIX m)
{
    if (mot != nullptr)
        mot->SetMatrix(&m[0][0]);
}

// ──────────────────────────────────────────────────────────────────────
// The unit matrix (FF3 copies the one at data 0x3293c0).
// FF3 0x001d2e60 (vtable +0x1c).

void GameModel::GetWorldMatrix(sceVu0FMATRIX out)
{
    sceVu0UnitMatrix(out);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac980 (vtable +0x20): nothing (PssModel overrides it).

void GameModel::DrawMdlPre()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac578 (vtable +0x24): nothing.

void GameModel::DrawFirst()
{
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac580 (vtable +0x28): nothing.

void GameModel::DrawSecond()
{
}

// ──────────────────────────────────────────────────────────────────────
// in through coordinate coord's matrix into out.
// FF3 0x001d2e90 (vtable +0x2c).

void GameModel::GetCoordPos(float *out, int coord, float *in)
{
    if (mot != nullptr)
        mot->ani_ctrl->GetCoordPos(out, coord, in);
}

// ──────────────────────────────────────────────────────────────────────
// Row axis of coordinate coord's matrix into out.
// FF3 0x001d2eb8 (vtable +0x30).

void GameModel::GetCoordAxis(float *out, int coord, int axis)
{
    if (mot != nullptr)
        mot->ani_ctrl->GetCoordAxis(out, coord, axis);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ac588 (vtable +0x34): joint -> coordinate, 0 for a plain model.

int GameModel::GetJointCoord(int joint)
{
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Asks the file cache for model file id; 1 when it took the request.
// FF3 0x001d3040

int GameModel::Load(short id)
{
    this->id = id;
    pk4 = FileReq(id);
    return pk4 != nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The model's world matrix to its motion object (both through the vtable).
// FF3 0x001d2d50

void GameModel::UpdateMatrix()
{
    alignas(16) sceVu0FMATRIX m;

    GetWorldMatrix(m);                  // FF3: vtable +0x1c
    SetWorldMatrix(m);                  // FF3: vtable +0x18
}

// ──────────────────────────────────────────────────────────────────────
// Draws the motion object with the default GS registers when there is one
// and the model is drawn.
// FF3 0x001d2d98

void GameModel::DrawMdl()
{
    if (mot == nullptr || IsDrawn() == 0)   // FF3: vtable +0x14
        return;

    gra3dSetGsRegisterDefault();
    DrawMdlPre();                       // FF3: vtable +0x20
    mot->Draw();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d2e08 (MapDrawPlyrCameraMdl): vtable +0x24 then +0x28 when
// the model is drawn.

void GameModel::Draw()
{
    if (IsDrawn() != 0)                 // FF3: vtable +0x14
    {
        DrawFirst();                    // FF3: vtable +0x24
        DrawSecond();                   // FF3: vtable +0x28
    }
}

// ──────────────────────────────────────────────────────────────────────
// The angle (-pi..pi) of coordinate coord about axis: its matrix's row axis
// with that component dropped, normalised, against (0, 0, 1) for axis 1 or
// (0, 1, 0) otherwise; the cross product's y picks the sign. 0 without a
// motion object or for coord < 0.
// FF3 0x001d2ee0 (VU0 macro code).

float GameModel::GetCoordRot(int coord, int axis)
{
    alignas(16) sceVu0FVECTOR v;
    const float *ref;
    float        len, q, cross_y, dot, rot, sign;

    if (mot == nullptr || coord < 0)
        return 0.0f;

    ref = (axis == 1) ? game_model_axis_z : game_model_axis_y;
    mot->ani_ctrl->GetCoordAxis(v, coord, axis);
    v[axis] = 0.0f;

    // vrsqrt; the VU saturates 1 / sqrt(0) where the host would make inf
    len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    q   = (len > 0.0f) ? 1.0f / sqrtf(len) : FLT_MAX;
    v[0] *= q;
    v[1] *= q;
    v[2] *= q;

    // vopmula / vopmsub: v x ref; only y is read
    cross_y = v[2] * ref[0] - v[0] * ref[2];
    sign = 1.0f;                        // sdata 0x370acc
    if (0.0f <= cross_y)
        sign = -1.0f;                   // sdata 0x370ad0

    dot = v[0] * ref[0] + v[1] * ref[1] + v[2] * ref[2];
    rot = Vu0Acosf(dot) * sign;
    RotLimitChk(&rot);
    return rot;
}

// ──────────────────────────────────────────────────────────────────────
// The coordinate array of the model's first SGD (index it with
// GetJointCoord), nullptr without a motion object.
// FF3 0x001d3080

SGDCOORDINATE *GameModel::GetCoord(int no)
{
    if (mot != nullptr)
        return mot->ani_ctrl->GetCoord(no);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The material alpha of every SGD of the model.
// FF3 0x001d30b0

void GameModel::SetAlpha(u_char alpha)
{
    if (mot != nullptr)
        mot->ani_ctrl->SetAlphaAll(alpha);
}

// ──────────────────────────────────────────────────────────────────────
// The material alpha of SGD no.
// FF3 0x001d30e0

void GameModel::SetMeshAlpha(int no, u_char alpha)
{
    if (mot != nullptr)
        mot->ani_ctrl->SetAlpha(no, alpha);
}

// ──────────────────────────────────────────────────────────────────────
// Every texture's frame (PlyrMdlFxMain's value, PlyrCameraUpdate's 0).
// FF3 0x001d3110
// callcheck: ModelObject::SetTexFrame through ani_ctrl's vtable (+0x14)

void GameModel::SetTexFrame(u_char frame)
{
    if (mot != nullptr)
        mot->ani_ctrl->SetTexFrame(frame);
}

// ──────────────────────────────────────────────────────────────────────
// Texture no's frame.
// FF3 0x001d3148
// callcheck: ModelObject::SetTexFrameOne through ani_ctrl's vtable (+0x18)

void GameModel::SetTexFrameOne(int no, u_char frame)
{
    if (mot != nullptr)
        mot->ani_ctrl->SetTexFrameOne(no, frame);
}

// ──────────────────────────────────────────────────────────────────────
// Free hook no (0 / 1); EneMdlCreate clears them for some enemies.
// FF3 0x001d3180

void GameModel::SetFreeHook(int no, GAME_MODEL_FREE_HOOK hook)
{
    this->hook[no] = hook;
}

// ──────────────────────────────────────────────────────────────────────
// File no from the file cache; when the cache is full each free hook runs in
// turn and the request is tried again. nullptr for no -1 or when nothing
// helped.
// FF3 0x001d3190
// callcheck: FileCacheReq (file_cache.Req)

void *GameModel::FileReq(short no)
{
    void *buf;
    int   i;

    if (no == -1)
        return nullptr;

    buf = file_cache.Req(no);
    if (buf != nullptr)
        return buf;

    for (i = 0; i < 2; i++)
    {
        if (hook[i] != nullptr)
        {
            hook[i]();
            buf = file_cache.Req(no);
            if (buf != nullptr)
                return buf;
        }
    }
    return buf;
}

// ──────────────────────────────────────────────────────────────────────
// Lets go of file no (none for -1).
// FF3 0x001d3238
// callcheck: FileCacheRelease (file_cache.Release)

void GameModel::FileRelease(short no)
{
    if (no != -1)
        file_cache.Release(no);
}

// ══════════════════════════════════════════════════════════════════════
// GameMotionModel
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// A GameModel without a motion file or second file.
// FF3 0x001d32b0 (complete object) and FF3 0x001d3268 (the base-object twin
// GameAnimeModel's constructor calls).
// callcheck: GameModelCtorBase (the base class constructor)

GameMotionModel::GameMotionModel()
{
    shp_data = nullptr;
    mot_data = nullptr;
    shp_no   = -1;
    mot_no   = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Lets go of the motion file and the second file, then GameModel's part.
// FF3 0x001d3340 (vtable +0x08), FF3 0x001d32f8 (base object), FF3 0x001d3388
// (deleting: then operator_delete).
// callcheck: GameModelFileRelease, GameModelDtorBase (FileRelease; the base class destructor)

GameMotionModel::~GameMotionModel()
{
    FileRelease(mot_no);
    FileRelease(shp_no);
}

// ──────────────────────────────────────────────────────────────────────
// Model file mdl, motion file mot and second file shp (-1: none) from the
// file cache. A refused request lets go of what the earlier ones took and
// answers 0. The numbers stay set either way, so the destructor releases them
// again (the ROM's behaviour).
// FF3 0x001d33d8
// callcheck: GameModelLoad (GameModel::Load)

int GameMotionModel::Load(short mdl, short mot, short shp)
{
    if (GameModel::Load(mdl) == 0)
        return 0;

    mot_no = mot;
    shp_no = shp;

    if (mot != -1)
    {
        mot_data = FileReq(mot);
        if (mot_data == nullptr)
        {
            FileRelease(mdl);
            return 0;
        }
    }
    if (shp != -1)
    {
        shp_data = FileReq(shp);
        if (shp_data == nullptr)
        {
            FileRelease(mdl);
            FileRelease(mot);
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Always a MotionModelBuilder, with the motion PK4's first member and the
// second file's data; the rest as GameModel::Build (here FF3 deletes the
// PK4 holder last).
// FF3 0x001d34b0 (vtable +0x00).
// callcheck: MotionModelBuilderCtor (the builder's constructor, a new-expression); GetResult (builder vtable +0x14); PlayCloth (mot vtable +0x14)

void GameMotionModel::Build()
{
    void         **holder;
    void          *mot_dat;
    ModelDirector *dir;

    // PC: FF3 allocates one 4-byte word for each of these (EE pointers)
    holder  = (void **)operator_new(sizeof(void *));
    *holder = pk4;
    mot_dat = Pk4GetData(mot_data, 0);
    builder = new (operator_new(sizeof(MotionModelBuilder))) MotionModelBuilder(holder, mot_dat, shp_data);

    dir = new (operator_new(sizeof(ModelDirector))) ModelDirector(builder);
    dir->Construct();

    mot = builder->GetResult();

    if (MdlPk4GetPart(holder, MDL_PK4_CLS) != nullptr)
        mot->PlayCloth(-1, 0, 0.0f, GAME_MODEL_MOT_SPEED);

    operator_delete(dir);
    operator_delete(holder);
}

// ──────────────────────────────────────────────────────────────────────
// 1 once the model file and the motion / second files in use are in; the
// first time BuildPre and Build run.
// FF3 0x001d35b8 (vtable +0x10; CharModel's, CharEnemyModel's and
// CharNpcModel's too).
// callcheck: FileCacheIsLoaded (file_cache.IsLoaded)

int GameMotionModel::IsLoaded()
{
    if (file_cache.IsLoaded(id) == 0)
        return 0;
    if (mot_no >= 0 && file_cache.IsLoaded(mot_no) == 0)
        return 0;
    if (shp_no >= 0 && file_cache.IsLoaded(shp_no) == 0)
        return 0;

    if (builder == nullptr)
    {
        BuildPre();                     // FF3: vtable +0x04
        Build();                        // FF3: vtable +0x00
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The motion number the motion control plays, 0 without a motion object.
// FF3 0x001d3670
// callcheck: GetBoneMotion (mot vtable +0x40)

short GameMotionModel::GetMotNo()
{
    if (mot == nullptr)
        return 0;
    return mot->GetBoneMotion()->no;
}

// ──────────────────────────────────────────────────────────────────────
// The motion control's frame, 0 without a motion object.
// FF3 0x001d36b0
// callcheck: GetBoneMotion (mot vtable +0x40)

float GameMotionModel::GetMotFrame()
{
    if (mot == nullptr)
        return 0.0f;
    return mot->GetBoneMotion()->frame;
}
