/* ==========================================================================
 *  ingame/map/MapObjMdl.c
 *
 *  The map object model handle (see MapObjMdl.h). FF3 0x203310..0x203828;
 *  FF3-only C++, written from the FF3 disassembly (Ghidra's decompilation
 *  drops the arguments of the virtual calls). Once inferred as the tail of
 *  FF2's mc_write.c.
 * ======================================================================== */
#include "ingame/map/MapObjMdl.h"

#include <new>                                  // placement new on FF3's C++ heap (operator_new)
#include "common/cpp_heap.h"                    // operator_new / operator_delete
#include "common/pk4.h"                         // Pk4GetData
#include "common/vu0math.h"                     // NormalizeAngle
#include "graphics/graph3d/sgd_types.h"         // SGDCOORDINATE
#include "graphics/gs_reg.h"                    // SetGsRegCached
#include "graphics/mmanage.h"                   // ModelManager, MdlPk4GetPart
#include "graphics/model_builder.h"             // the builders, ModelDirector
#include "graphics/model_object.h"              // ModelObject::SetAlphaAll / GetCoord
#include "ingame/photo/finder_photo.h"          // GetAlbumSaveDisp
#include "ingame/plyr/plyr_gauge.h"             // PlyrGaugeTimeStep
#include "sdk/libgraph.h"                       // SCE_GS_ZBUF_1
#include "sdk/libvu0.h"                         // sceVu0FMATRIX, sceVu0*Matrix*

// The speed Build starts a cloth model's cloth motions at (sdata 0x371154, 1.0).
#define MAP_OBJ_MDL_CLOTH_SPEED 1.0f

// ZBUF_1 around a gs draw (Draw): the buffer at 0x118, PSMZ16S; z writes
// masked, then back on.
#define MAP_OBJ_MDL_ZBUF_NOWRITE 0x10a000118ULL
#define MAP_OBJ_MDL_ZBUF_WRITE   0x00a000118ULL

// ──────────────────────────────────────────────────────────────────────
// The object's alpha (MapObjMain: the furniture row's alpha).
// FF3 0x00203310: ModelObject::SetAlphaAll as a tail call.

void MAP_OBJ_MDL::SetAlpha(u_char alpha)
{
    mot->ani_ctrl->SetAlphaAll(alpha);
}

// ──────────────────────────────────────────────────────────────────────
// The world matrix into the model (ModelManager::SetMatrix).
// FF3 0x00203330.
// callcheck: SetMatrix (mot vtable +0x08)

int MAP_OBJ_MDL::SetMatrix(float *matrix)
{
    mot->SetMatrix(matrix);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The world matrix from a rotation (x, then y, then z; each angle through
// NormalizeAngle twice) and a position, without scale.
// FF3 0x00203360.

void MAP_OBJ_MDL::SetPosRot(const float *pos, const float *rot)
{
    sceVu0FMATRIX m;

    sceVu0UnitMatrix(m);                // the ROM rotates the unit matrix at data 0x3293c0
    sceVu0RotMatrixX(m, m, NormalizeAngle(NormalizeAngle(rot[0])));
    sceVu0RotMatrixY(m, m, NormalizeAngle(NormalizeAngle(rot[1])));
    sceVu0RotMatrixZ(m, m, NormalizeAngle(NormalizeAngle(rot[2])));
    m[3][0] += pos[0];                  // FF3: vadd.xyz on row 3
    m[3][1] += pos[1];
    m[3][2] += pos[2];
    SetMatrix(&m[0][0]);
}

// ──────────────────────────────────────────────────────────────────────
// Draws the model: its draw request, then the draw. With gs != 0 (the put's
// MAP_PUT_GS flag) z writes are off for it. flags is not read.
// FF3 0x00203430.

int MAP_OBJ_MDL::Draw(u_int flags, u_int gs)
{
    (void)flags;

    if (gs != 0)
        SetGsRegCached(MAP_OBJ_MDL_ZBUF_NOWRITE, SCE_GS_ZBUF_1, 1);
    mot->ReqDraw();
    mot->Draw();
    if (gs != 0)
        SetGsRegCached(MAP_OBJ_MDL_ZBUF_WRITE, SCE_GS_ZBUF_1, 1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Bone motion `motion` from frame 0 without blending, at speed times the
// player's time step (slow while the slow-mode gauge runs).
// FF3 0x002034a8.
// callcheck: PlayMotion (mot vtable +0x0c)

int MAP_OBJ_MDL::PlayMotion(int motion, float speed)
{
    speed *= PlyrGaugeTimeStep(GetAlbumSaveDisp());
    mot->PlayMotion(motion, 0, 0.0f, speed);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every motion's speed. FF3 0x00203518.
// callcheck: SetSpeed (mot vtable +0x24)

int MAP_OBJ_MDL::SetSpeed(float speed)
{
    mot->SetSpeed(speed);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Holds motion `motion`'s first pose: stopped at frame 0, then the model's
// own world matrix (coordinate 0's matLocalWorld, EE coord + 0x40) set
// again so the bones follow it (the doors: MapLBuffLoadDoor,
// MapDoorRoomChange). FF3 0x00203548.
// callcheck: PlayMotion (mot vtable +0x0c); SetMatrix (mot vtable +0x08)

int MAP_OBJ_MDL::SetPose(int motion)
{
    SetSpeed(0.0f);
    mot->PlayMotion(motion, 0, 0.0f, 0.0f);
    mot->SetMatrix(&mot->ani_ctrl->GetCoord(0)->matLocalWorld[0][0]);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Builds the model from its PK4s and keeps them for Rebuild: with a motion
// PK4 a MotionModelBuilder over its first member, else one when the model
// PK4 has cloth (cls) or chains (ccs), else a NormalModelBuilder. The
// director runs the builder, its result becomes mot, stopped (speed 0);
// cloth starts at once. Returns MAP_OBJ_MDL_* (-1 is never reached: one of
// cls / ccs is set on that path).
// FF3 0x002035d0.
// callcheck: MotionModelBuilderCtor, NormalModelBuilderCtor (new-expressions); ModelDirectorConstruct (dir->Construct); GetResult (builder vtable +0x14); SetSpeed (mot vtable +0x24); PlayCloth (mot vtable +0x14)

int MAP_OBJ_MDL::Build(void *mdl_pk4, void *mot_pk4)
{
    void         **holder;
    void          *cls;
    void          *ccs;
    ModelDirector *dir;
    int            kind;

    this->mot_pk4 = mot_pk4;
    this->mdl_pk4 = mdl_pk4;
    kind = -1;

    // PC: FF3 allocates one 4-byte word holding the PK4's address
    holder  = (void **)operator_new(sizeof(void *));
    *holder = mdl_pk4;
    cls = MdlPk4GetPart(holder, MDL_PK4_CLS);
    ccs = MdlPk4GetPart(holder, MDL_PK4_CCS);

    if (mot_pk4 != nullptr)
    {
        void *bmd = Pk4GetData(mot_pk4, 0);

        kind    = MAP_OBJ_MDL_MOTION;
        builder = new (operator_new(sizeof(MotionModelBuilder))) MotionModelBuilder(holder, bmd, nullptr);
    }
    else if (cls != nullptr || ccs != nullptr)
    {
        builder = new (operator_new(sizeof(MotionModelBuilder))) MotionModelBuilder(holder, nullptr, nullptr);
        if (cls != nullptr)
            kind = MAP_OBJ_MDL_CLOTH;
        else if (ccs != nullptr)
            kind = MAP_OBJ_MDL_CHAIN;
    }
    else
    {
        kind    = MAP_OBJ_MDL_PLAIN;
        builder = new (operator_new(sizeof(NormalModelBuilder))) NormalModelBuilder(holder);
    }

    dir = new (operator_new(sizeof(ModelDirector))) ModelDirector(builder);
    dir->Construct();
    mot = builder->GetResult();
    mot->SetSpeed(0.0f);
    if (cls != nullptr)
        mot->PlayCloth(-1, 0, 0.0f, MAP_OBJ_MDL_CLOTH_SPEED);

    operator_delete(dir);
    if (holder != nullptr)
        operator_delete(holder);
    return kind;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the builder (and with it mot); the PK4s stay for Rebuild.
// FF3 0x002037b0.

void MAP_OBJ_MDL::Release()
{
    if (builder != nullptr)
    {
        // FF3: the builder's vtable +4, its deleting destructor
        builder->~ModelBuilder();
        operator_delete(builder);
    }
    mot     = nullptr;
    builder = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Builds the model again from its PK4s (MapObjDoorMdlRebuildAll).
// FF3 0x002037f8: Build as a tail jump.

void MAP_OBJ_MDL::Rebuild()
{
    Release();
    Build(mdl_pk4, mot_pk4);
}
