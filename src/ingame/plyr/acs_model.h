/* ==========================================================================
 *  ingame/plyr/acs_model.h
 *
 *  AcsModel (RTTI; AcsModel : GameModel, 0x28 bytes, vtable 0x3412a0): a
 *  player's accessory model (file A000_PK4 + n), hung on one of the player
 *  model's coordinates and faded by PlyrAcsFadeCtrl. FF3 0x121000..0x1211f0;
 *  the deleting destructor is inline code at 0x2ac9b0. FF3-only file (name
 *  descriptive).
 * ======================================================================== */
#ifndef _INGAME_PLYR_ACS_MODEL_H
#define _INGAME_PLYR_ACS_MODEL_H

#include <stddef.h>                     // offsetof

#include "graphics/game_model.h"      // GameModel
#include "sdk/libvu0.h"                 // sceVu0FMATRIX

struct SGDCOORDINATE;                   // graphics/graph3d/sgd_types.h

// FF3 allocates it with operator new(0x28) (PlyrMdlCreate,
// PlyrCameraAcsNew). Its destructors are GameModel's (implicit here; FF3
// emits them inline at 0x2ac988, deleting 0x2ac9b0).
// layout: host -- derived from GameModel; EE offsets in the comments.
class AcsModel : public GameModel
{
public:
    AcsModel();                         // FF3 0x00121048 (base-object twin 0x00121000)

    void GetWorldMatrix(sceVu0FMATRIX out) override;   // FF3 0x001210b8: vtable +0x1c
    int  Load(short no);                // FF3 0x00121090: model file A000_PK4 + no
    void SetCoord(SGDCOORDINATE *coord);  // FF3 0x00121170
    void FadeStep();                    // FF3 0x00121178: alpha += alpha_spd, clamped

    /* 0x1c */ SGDCOORDINATE *coord;    // the player model's coordinate it hangs on (AcsModel::SetCoord)
    /* 0x20 */ float alpha_spd;         // added to alpha by AcsModel::FadeStep (PlyrAcsFadeCtrl sets it)
    /* 0x24 */ float alpha;             // 0..128, handed to the model (GameModel::SetAlpha)
};
static_assert(offsetof(AcsModel, coord) == sizeof(GameModel), "AcsModel.coord (EE +0x1c)");
static_assert(offsetof(AcsModel, alpha_spd) == offsetof(AcsModel, coord) + sizeof(SGDCOORDINATE *),
              "AcsModel.alpha_spd (EE +0x20)");
static_assert(offsetof(AcsModel, alpha) == offsetof(AcsModel, alpha_spd) + 4, "AcsModel.alpha (EE +0x24)");

#define ACS_MODEL_ALPHA_MAX 128.0f


#endif /* _INGAME_PLYR_ACS_MODEL_H */
