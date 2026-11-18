/* ==========================================================================
 *  graphics/motion_model_manager.h
 *
 *  FF3 0x210818..0x2113d0: RTTI MotionModelManager : ModelManager (vtable
 *  0x342080), what MotionModelBuilder::BuildMotion makes -- the model's
 *  motion object (GameModel.mot) when the model PK4 has a bone motion,
 *  shape (vertex) motions, cloth or chains. FF3-only C++ (FF2's link order
 *  has morph.c here; its MorphDell matched the constructor by mistake).
 * ======================================================================== */
#ifndef _GRAPHICS_MOTION_MODEL_MANAGER_H
#define _GRAPHICS_MOTION_MODEL_MANAGER_H

#include <stddef.h>                 // offsetof

#include "graphics/mmanage.h"       // ModelManager, ModelCcs

class ShpMotion;                    // graphics/motion/motion.h
class ClothMotion;                  // graphics/motion/motion.h

// The motions of one model and its chains, stepped by SetMatrix (vtable
// +0x08) every frame the model is placed. FF3 operator new(0x24). The
// destructor leaves the motions and the chains to the builder that made
// them (MotionModelBuilder's destructor deletes them).
// layout: host -- the vptr and pointers are 4 bytes on the EE; EE offsets in the comments.
class MotionModelManager : public ModelManager
{
public:
    // FF3 0x002108b8 (complete; base-object twin 0x00210818).
    MotionModelManager(ModelObject *obj, ModelCcs *ccs, ModelCollision *collision,
                       BmdMotion *bmd_mot, ShpMotion **shp_mot, short shp_num,
                       ClothMotion **cloth_mot, short cloth_num);
    ~MotionModelManager() override;     // FF3 0x00210980 (deleting 0x002109a8; base 0x00210958)

    void SetMatrix(float *matrix) override;                                     // FF3 0x00210f50, vtable +0x08
    void PlayMotion(int no, int blend, float frame, float speed) override;      // FF3 0x00210a18, +0x0c
    void PlayShape(int no, int loop, int stop_end, int reverse, float frame, float speed) override;  // FF3 0x00210b48, +0x10
    void PlayCloth(int no, int arg, float frame, float speed) override;         // FF3 0x00210bd0, +0x14
    void ResetCloth(int no, int defer) override;                                // FF3 0x00210cc0, +0x18
    void SetClothWind(int no, const float *dir, int period, float power) override;  // FF3 0x00210dc8, +0x1c
    void StopClothWind(int no) override;                                        // FF3 0x00210ec0, +0x20
    void SetSpeed(float speed) override;                                        // FF3 0x00211170, +0x24
    void SetFrame(float frame) override;                                        // FF3 0x00211200, +0x28
    void StopShape(int no) override;                                            // FF3 0x002112c8, +0x2c
    void StopShapeAll(void) override;                                           // FF3 0x00211300, +0x30
    void WriteShapeAll(void) override;                                          // FF3 0x00211368, +0x34
    int  IsMotionEnd(void) override;                                            // FF3 0x002109e0, +0x38
    int  IsShapeEnd(int no) override;                                           // FF3 0x00210af8, +0x3c
    MOT_CTRL *GetBoneMotion(void) override { return bmd_mot; }                  // FF3 0x002bd840, +0x40 (inline)
    ModelCcs *GetCcs(void) override { return ccs; }                             // FF3 0x002bd848, +0x44 (inline)
    int  GetShapeNum(void) override { return shp_num; }                         // FF3 0x002bd850, +0x48 (inline)
    void SetShapeLoop(int no, int loop) override;                               // FF3 0x00210b98, +0x4c
    // +0x50 GetCollision: ModelManager's (0x002bd838).

    /* 0x0c */ ModelCcs     *ccs;           // the chains; may be nullptr
    /* 0x10 */ BmdMotion    *bmd_mot;       // the bone motion; may be nullptr
    /* 0x14 */ ShpMotion   **shp_mot;       // [shp_num]; may be nullptr
    /* 0x18 */ short         shp_num;
    /* 0x1c */ ClothMotion **cloth_mot;     // [cloth_num], entries may be nullptr; may be nullptr
    /* 0x20 */ short         cloth_num;
};
static_assert(offsetof(MotionModelManager, ccs) == 3 * sizeof(void *), "MotionModelManager.ccs (EE +0x0c)");
static_assert(offsetof(MotionModelManager, shp_num) == 6 * sizeof(void *), "MotionModelManager.shp_num (EE +0x18)");

#endif /* _GRAPHICS_MOTION_MODEL_MANAGER_H */
