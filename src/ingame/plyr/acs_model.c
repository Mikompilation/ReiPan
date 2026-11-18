/* ==========================================================================
 *  ingame/plyr/acs_model.c
 *
 *  AcsModel, a player's accessory model: a GameModel loaded from
 *  A000_PK4 + n that follows one coordinate of the player model and fades
 *  in and out (PlyrAcsFadeCtrl). FF3 0x121000..0x1211f0, FF3's own class
 *  (RTTI AcsModel : GameModel).
 * ======================================================================== */
#include "ingame/plyr/acs_model.h"

#include "graphics/graph3d/sgd_types.h"                 // SGDCOORDINATE
#include "system/eeiop/cd_file_dat.h"                   // A000_PK4

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// SI -> PS axis flip (data 0x329400; gra3d.c keeps the same matrix as
// s_matConvertPrelight).
alignas(16) static float acs_mat_flip[4][4] =
{
    { 1.0f,  0.0f,  0.0f, 0.0f },
    { 0.0f, -1.0f,  0.0f, 0.0f },
    { 0.0f,  0.0f, -1.0f, 0.0f },
    { 0.0f,  0.0f,  0.0f, 1.0f },
};

// ──────────────────────────────────────────────────────────────────────
// The constructor: a GameModel with no coordinate, not fading, fully shown.
// FF3 0x00121048 (complete object; 0x00121000 is the identical base-object
// constructor, unreferenced).
// callcheck: GameModelCtorBase (the base class constructor)

AcsModel::AcsModel()
{
    coord     = nullptr;
    alpha_spd = 0.0f;
    alpha     = ACS_MODEL_ALPHA_MAX;
}

// ──────────────────────────────────────────────────────────────────────
// Load accessory model no.
// FF3 0x00121090.

int AcsModel::Load(short no)
{
    return GameModel::Load((short)(no + A000_PK4));
}

// ──────────────────────────────────────────────────────────────────────
// The model's world matrix: its coordinate's local-world matrix with the
// axis flip, or the unit matrix while it hangs on nothing.
// FF3 0x001210b8 (vtable +0x1c; VU0 macro code).

void AcsModel::GetWorldMatrix(sceVu0FMATRIX out)
{
    if (coord == nullptr)
    {
        sceVu0UnitMatrix(out);                          // FF3 copies the unit matrix 0x3293c0
        return;
    }
    sceVu0MulMatrix(out, coord->matLocalWorld, acs_mat_flip);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00121170.

void AcsModel::SetCoord(SGDCOORDINATE *coord)
{
    this->coord = coord;
}

// ──────────────────────────────────────────────────────────────────────
// One fade step: alpha += alpha_spd, clamped to 0..128, to the model.
// FF3 0x00121178.

void AcsModel::FadeStep()
{
    float alpha;

    if (alpha_spd == 0.0f)
        return;

    alpha = this->alpha + alpha_spd;
    this->alpha = alpha;
    if (alpha < 0.0f)
        alpha = 0.0f;
    else if (ACS_MODEL_ALPHA_MAX < alpha)
        alpha = ACS_MODEL_ALPHA_MAX;
    this->alpha = alpha;
    SetAlpha((int)alpha & 0xff);
}
