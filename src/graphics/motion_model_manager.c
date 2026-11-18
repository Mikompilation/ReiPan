/* ==========================================================================
 *  graphics/motion_model_manager.c
 *
 *  FF3 0x210818..0x2113d0: RTTI MotionModelManager (see the header). Its
 *  getters +0x40..+0x48 are inline code FF3 emits at 0x2bd840..0x2bd850.
 *  Written from the FF3 disassembly.
 *
 *  FF3 compiles each constructor and destructor twice (complete and
 *  base-object, same body) plus a deleting destructor; one C++ definition
 *  stands for each set.
 * ======================================================================== */
#include "graphics/motion_model_manager.h"

#include <string.h>                     // memcpy, memset

#include "graphics/motion/motion.h"     // ModelObject, BmdMotion, ShpMotion, ClothMotion

// The shape motions' meshes SetMatrix keeps apart (its stack table, 0x400 bytes).
#define MOTION_MODEL_MESH_MAX 256

// FF3 0x002108b8 (base-object twin 0x00210818)
// callcheck: ModelManagerCtorBase (the base-class initializer)

MotionModelManager::MotionModelManager(ModelObject *obj, ModelCcs *ccs, ModelCollision *collision,
                                       BmdMotion *bmd_mot, ShpMotion **shp_mot, short shp_num,
                                       ClothMotion **cloth_mot, short cloth_num)
    : ModelManager(obj, collision)
{
    this->cloth_num = cloth_num;
    this->ccs       = ccs;
    this->bmd_mot   = bmd_mot;
    this->shp_mot   = shp_mot;
    this->shp_num   = shp_num;
    this->cloth_mot = cloth_mot;
}

// FF3 0x00210980 (deleting 0x002109a8, base-object 0x00210958): only
// ModelManager's part; the builder owns the motions.
// callcheck: ModelManagerDtorBase (the base-class destructor)

MotionModelManager::~MotionModelManager()
{
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the model at matrix: without a bone motion ModelManager's
// placement; with one the bone motion's step in world space and the bone
// coordinates. Then each mesh's vertices from the first of its shape motions
// that has any to write, every shape and cloth step, and the chains.
// FF3 0x00210f50 (vtable +0x08).
// callcheck: ModelManagerSetMatrix (the base class's, called directly); BoneMotionUpdate, ModelObjectCalcBoneCoordinate, ShpMotionGetMeshNo, ShpMotionWriteVertex, VertexMotionUpdate, ModelCcsUpdate (members)

void MotionModelManager::SetMatrix(float *matrix)
{
    sceVu0FMATRIX m;
    int           done[MOTION_MODEL_MESH_MAX];
    int           mesh;
    int           i;

    memcpy(m, matrix, sizeof(m));       // PC: the caller's matrix may not be 16-byte aligned
    if (bmd_mot == nullptr)
    {
        ModelManager::SetMatrix(&m[0][0]);
    }
    else
    {
        sceVu0MulMatrix(m, m, mdl_flip_yz);
        bmd_mot->Update(&m[0][0]);
        ani_ctrl->CalcBoneCoordinate();
    }

    if (shp_mot != nullptr)
    {
        // Shapes on one mesh share its vertex buffer: one write each.
        memset(done, 0, sizeof(done));
        for (i = 0; i < shp_num; i++)
        {
            mesh = shp_mot[i]->GetMeshNo();
            if (done[mesh] == 0 && shp_mot[i]->WriteVertex(0) != 0)
                done[mesh] = 1;
        }
        for (i = 0; i < shp_num; i++)
            shp_mot[i]->Update();
    }

    if (cloth_mot != nullptr)
    {
        for (i = 0; i < cloth_num; i++)
            cloth_mot[i]->Update();
    }

    if (ccs != nullptr)
        ccs->Update();
}

// Bone motion no (below the motion count) from frame: blend != 0 blends
// into it over that many frames.
// FF3 0x00210a18 (vtable +0x0c).
// callcheck: BmdMotionGetNum, MotionSetFrame, BmdMotionResetPose (members); SetMotion (bmd_mot vtable +0x18)

void MotionModelManager::PlayMotion(int no, int blend, float frame, float speed)
{
    if (bmd_mot == nullptr || !(no < bmd_mot->GetNum()))
        return;

    bmd_mot->blend_num   = blend;
    bmd_mot->blend       = (blend != 0);
    bmd_mot->blend_frame = 0.0f;
    bmd_mot->SetFrame(frame);
    bmd_mot->no          = no;
    bmd_mot->SetMotion(no);
    bmd_mot->speed = speed;
    bmd_mot->ResetPose();
}

// FF3 0x00210b48 (vtable +0x10). FF3 tests no only against the count.
// callcheck: VertexMotionPlay (member)

void MotionModelManager::PlayShape(int no, int loop, int stop_end, int reverse, float frame, float speed)
{
    if (shp_mot != nullptr && no < shp_num)
        shp_mot[no]->Play(loop, stop_end, reverse, frame, speed);
}

// FF3 0x00210bd0 (vtable +0x14): arg is not read.
// callcheck: VertexMotionPlay (member)

void MotionModelManager::PlayCloth(int no, int arg, float frame, float speed)
{
    int i;

    (void)arg;
    if (cloth_mot == nullptr)
        return;

    if (no < 0)
    {
        for (i = 0; i < cloth_num; i++)
        {
            if (cloth_mot[i] != nullptr)
                cloth_mot[i]->Play(0, 0, 0, frame, speed);
        }
    }
    else if (no < cloth_num && cloth_mot[no] != nullptr)
    {
        cloth_mot[no]->Play(0, 0, 0, frame, speed);
    }
}

// FF3 0x00210cc0 (vtable +0x18).
// callcheck: ClothMotionReset (member)

void MotionModelManager::ResetCloth(int no, int defer)
{
    int i;

    if (cloth_mot == nullptr)
        return;

    if (no < 0)
    {
        for (i = 0; i < cloth_num; i++)
        {
            if (cloth_mot[i] == nullptr)
                continue;
            if (defer != 0)
                cloth_mot[i]->reset_req = 1;
            else
                cloth_mot[i]->Reset();
        }
    }
    else if (no < cloth_num && cloth_mot[no] != nullptr)
    {
        if (defer != 0)
            cloth_mot[no]->reset_req = 1;
        else
            cloth_mot[no]->Reset();
    }
}

// FF3 0x00210dc8 (vtable +0x1c).
// callcheck: ClothMotionSetWind (member)

void MotionModelManager::SetClothWind(int no, const float *dir, int period, float power)
{
    int i;

    if (cloth_mot == nullptr)
        return;

    if (no < 0)
    {
        for (i = 0; i < cloth_num; i++)
        {
            if (cloth_mot[i] != nullptr)
                cloth_mot[i]->SetWind(dir, period, power);
        }
    }
    else if (no < cloth_num && cloth_mot[no] != nullptr)
    {
        cloth_mot[no]->SetWind(dir, period, power);
    }
}

// FF3 0x00210ec0 (vtable +0x20)

void MotionModelManager::StopClothWind(int no)
{
    int i;

    if (cloth_mot == nullptr)
        return;

    if (no < 0)
    {
        for (i = 0; i < cloth_num; i++)
        {
            if (cloth_mot[i] != nullptr)
                cloth_mot[i]->wind = 0;
        }
    }
    else if (no < cloth_num && cloth_mot[no] != nullptr)
    {
        cloth_mot[no]->wind = 0;
    }
}

// FF3 0x00211170 (vtable +0x24)

void MotionModelManager::SetSpeed(float speed)
{
    int i;

    if (bmd_mot != nullptr)
        bmd_mot->speed = speed;
    if (shp_mot != nullptr)
    {
        for (i = 0; i < shp_num; i++)
            shp_mot[i]->speed = speed;
    }
    if (cloth_mot != nullptr)
    {
        for (i = 0; i < cloth_num; i++)
            cloth_mot[i]->speed = speed;
    }
}

// FF3 0x00211200 (vtable +0x28)
// callcheck: MotionSetFrame (member)

void MotionModelManager::SetFrame(float frame)
{
    int i;

    if (bmd_mot != nullptr)
        bmd_mot->SetFrame(frame);
    if (shp_mot != nullptr)
    {
        for (i = 0; i < shp_num; i++)
            shp_mot[i]->SetFrame(frame);
    }
    if (cloth_mot != nullptr)
    {
        for (i = 0; i < cloth_num; i++)
            cloth_mot[i]->SetFrame(frame);
    }
}

// FF3 0x002112c8 (vtable +0x2c). FF3 does not test no.
// callcheck: StopVirtual (shape vtable +0x20)

void MotionModelManager::StopShape(int no)
{
    if (shp_mot != nullptr)
        shp_mot[no]->StopVirtual();
}

// FF3 0x00211300 (vtable +0x30)
// callcheck: StopVirtual (shape vtable +0x20)

void MotionModelManager::StopShapeAll(void)
{
    int i;

    if (shp_mot == nullptr)
        return;
    for (i = 0; i < shp_num; i++)
        shp_mot[i]->StopVirtual();
}

// FF3 0x00211368 (vtable +0x34)
// callcheck: ShpMotionWriteVertex (member)

void MotionModelManager::WriteShapeAll(void)
{
    int i;

    if (shp_mot == nullptr)
        return;
    for (i = 0; i < shp_num; i++)
        shp_mot[i]->WriteVertex(1);
}

// FF3 0x002109e0 (vtable +0x38)
// callcheck: IsEnd (bmd_mot vtable +0x10)

int MotionModelManager::IsMotionEnd(void)
{
    if (bmd_mot == nullptr)
        return 0;
    return bmd_mot->IsEnd();
}

// FF3 0x00210af8 (vtable +0x3c): 1 when there is no such shape motion.
// callcheck: IsEnd (shape vtable +0x10)

int MotionModelManager::IsShapeEnd(int no)
{
    short n = (short)no;

    if (shp_mot == nullptr || !(n < shp_num))
        return 1;
    return shp_mot[n]->IsEnd();
}

// FF3 0x00210b98 (vtable +0x4c)

void MotionModelManager::SetShapeLoop(int no, int loop)
{
    if (shp_mot != nullptr && no < shp_num)
        shp_mot[no]->hold = loop;   // VertexMotion.hold: the frame frozen
}
