/* ==========================================================================
 *  graphics/graph3d/gra3dMisc.c
 *
 *  High-level gra3d services between the renderer, the shadow pass and the
 *  map / player code:
 *    - prelighting the shown rooms again, or clearing it (gra3dPrelight /
 *      gra3dClearPrelight),
 *    - shadows of runtime models, whole or block by block
 *      (gra3dDrawSGDShadow / ...EveryObject),
 *    - a character's light set: the room's with the player's flashlights and
 *      self-reflection light folded in (gra3dGenerateLightDataToChar /
 *      _SetLightToShadow),
 *    - the projector spot light of the cine projector rooms
 *      (gra3dSetSpecialLight .. gra3dGetProjectorSpotId).
 *
 *  Ported from MioPan's gra3dMisc.c (FF2 gra3dMisc.c), checked against FF3
 *  0x1dfb90..0x1e0330. FF3 differences: gra3dPrelightScene and gra3dPrelight
 *  go through MapDrawPrelightRoom (which owns the header / light / SGD pass
 *  FF2 did here), gra3dPrelight takes a flag and does both room buffers;
 *  gra3dCalcShadowLight, gra3dSetObjectIdDrawNoShadow and
 *  gra3dDrawSGDShadowCharacter are gone (the shadow light comes from the
 *  player model); the player's lights live in the player model; the special
 *  light is one setter, and the projector's slot and room come from the cine
 *  projector code. No asserts.
 * ======================================================================== */

#include "graphics/graph3d/gra3dMisc.h"

#include <algorithm>                    // std::fill

#include "graphics/graph3d/g3dCore.h"           // g3dSetGsRegister
#include "graphics/graph3d/g3dLight.h"          // G3DLIGHT
#include "graphics/graph3d/g3dLightEx.h"        // gra3dSetLightIntens
#include "graphics/graph3d/gra3d.h"             // GRA3DLIGHTDATA, gra3dSetLightData
#include "graphics/graph3d/gra3dSGD.h"          // _gra3dDrawSGD
#include "graphics/graph3d/gra3dSGDData.h"      // sgdCalcBoneCoordinate, sgdGetProcUnit
#include "graphics/graph3d/gra3dShadow.h"       // gra3dshadow*
#include "graphics/graph3d/sgd_types.h"         // SGDFILEHEADER, SgdPtr
#include "ingame/ingame.h"                      // CharPlayerModel
#include "ingame/cine_film.h"                   // CineGetProjectorAreaNo, CineGetProjectorSpotId
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/map/MapDraw.h"                 // MapDrawGetLightPtr*
#include "ingame/map/MapLight.h"                // MapDrawPrelightRoom
#include "ingame/map/MapLoad.h"                 // MapLoadGetBuffID, MapLoadCheckDrawFlg, MapLoadCheckLoadEnd
#include "ingame/plyr/player.h"                 // GetPlyrAreaNo, PLYR_WRK
#include "reipan/os/reipan_rand.h"              // ReiPan_Rand, REIPAN_RAND_MAXF
#include "sdk/libgraph.h"                       // SCE_GS_TEXFLUSH

// rdata 0x3293c0: the unit matrix gra3dDrawSGDShadow hands the shadow pass.
static sceVu0FMATRIX s_matUnit = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

// rdata 0x35a4d0: the default light (FF2 g_NullLight: no type, position w 1,
// falloff 1); the static initializer copies it into s_ProjectorSpot.
static const G3DLIGHT s_NullLight = {
    { 0.0f, 0.0f, 0.0f, 0.0f },         // vDiffuse
    { 0.0f, 0.0f, 0.0f, 0.0f },         // vSpecular
    { 0.0f, 0.0f, 0.0f, 0.0f },         // vAmbient
    { 0.0f, 0.0f, 0.0f, 1.0f },         // vPosition
    { 0.0f, 0.0f, 0.0f, 0.0f },         // vDirection
    INVALID_G3DLIGHTTYPE,               // Type
    0.0f, 0.0f, 0.0f, 0.0f,             // fAngleInside, fAngleOutside, fMaxRange, fMinRange
    1.0f,                               // fFalloff
    { 0.0f, 0.0f },
};

static int      s_bSpecialLightActive;          // sdata 0x370c64
// bss 0x3ee8b0. FF3 0x001e02b0 (__static_initialization_and_destruction_0,
// run by the global constructor 0x1e0310) copies s_NullLight into it.
static G3DLIGHT s_ProjectorSpot = s_NullLight;

// The player model, whose lights the character light sets take.
static CharPlayerModel *_PlyrMdl(void)
{
    return (CharPlayerModel *)ingame_wrk.plyr_wrk->mdl;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001dfb90

void gra3dPrelightScene(int area)
{
    if (area >= 0)
        MapDrawPrelightRoom(MapLoadGetBuffID(area), 0);
}

// FF3 0x001dfbc8

void gra3dPrelight(int clear)
{
    int buff = MapLoadGetBuffID(GetPlyrAreaNo());

    if (MapLoadCheckDrawFlg(buff) != 0)
    {
        MapDrawPrelightRoom(buff, clear);
        if (MapLoadCheckLoadEnd(buff ^ 1) != 0)
            MapDrawPrelightRoom(buff ^ 1, clear);
    }
}

// FF3 0x001dfc58

void gra3dClearPrelight(void)
{
    gra3dPrelight(1);
}

// ──────────────────────────────────────────────────────────────────────
// A runtime (not preset) model casts its shadow: box avBBWorld is already in
// the world, so the unit matrix. FF3 0x001dfc70.

void gra3dDrawSGDShadow(SGDFILEHEADER *pSGDTop, SGDCOORDINATE *pCoord, G3DLIGHT *pLight, float (*avBBWorld)[4])
{
    if (pSGDTop != nullptr && (pSGDTop->ucModelType & 1) == 0)
    {
        gra3dshadowSetBoundingBox(avBBWorld, s_matUnit);
        gra3dshadowSetLight(pLight);
        gra3dshadowDrawSGD(pSGDTop, pCoord, -1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every block of a runtime shadow model that has a bounding box casts its
// own shadow (blocks 1 .. n-2: block 0 holds the textures, the last the
// skinned meshes). The model is drawn once first. FF3 0x001dfd00 (FF2 took
// the coordinates from the model and skipped one object id).

void gra3dDrawSGDShadowEveryObject(SGDFILEHEADER *pShadowModel, SGDCOORDINATE *pCoord, G3DLIGHT *pLight)
{
    u_int              i;
    SGDPROCUNITHEADER *pPUHead;

    if (pShadowModel == nullptr || pCoord == nullptr || (pShadowModel->ucModelType & 1) != 0)
        return;

    sgdCalcBoneCoordinate(pCoord, pShadowModel->uiNumBlock - 1);
    g3dSetGsRegister(0, SCE_GS_TEXFLUSH, 1);
    _gra3dDrawSGD(pShadowModel, SRT_REALTIME, pCoord, 0);
    gra3dshadowSetLight(pLight);

    for (i = 1; i < pShadowModel->uiNumBlock - 1; i++)
    {
        pPUHead = SgdPtr<SGDPROCUNITHEADER>(pShadowModel->ProcUnitHeads()[i]);
        pPUHead = sgdGetProcUnit(pPUHead, SGDPU_BOUNDING_BOX, 0);
        if (pPUHead != nullptr)
        {
            // the box's 8 corners follow the unit header
            gra3dshadowSetBoundingBox((float (*)[4])(pPUHead + 1), pCoord->matCoord);
            gra3dshadowDrawSGD(pShadowModel, pCoord, (int)i);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// pLDSrc into pLDDest with the player's lights: the flashlight (scaled by
// pData) into the point or spot flashlight slot by its type, the
// self-reflection light, the second flashlight when it is a point light;
// their three slots switched as pData says (a light of type INVALID turns
// its slots off). FF3 0x001dfe08.
// callcheck: memcpy (the struct copy *pLDDest = *pLDSrc)

void gra3dGenerateLightDataToChar(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc,
                                  GRA3DEMULATIONLIGHTDATACREATIONDATA *pData)
{
    CharPlayerModel *mdl          = _PlyrMdl();
    int              flashlightId = GRA3D_LID_SPOT_FLASHLIGHT;
    int              bEnableSelfreflection;
    int              bEmulateSelfreflection;
    int              bEnableFlashlight;
    int              bEmulateFlashlight;
    int              bEnableFlashlight2;
    int              bEmulateFlashlight2;
    G3DLIGHT        *pDest;
    int              j;

    *pLDDest = *pLDSrc;                 // memcpy 0x1610

    // PC: no player model -- the title's room backdrop (outgame/title_room.c)
    // draws a room with nobody in it, and the house's map objects lit as an
    // NPC (PlyrCameraMapObjDraw) still come here. Her lights count as off,
    // the path FF3 takes for lights of type INVALID; FF3 always has her.
    const int fl_type  = mdl != nullptr ? (int)mdl->fl.Type  : INVALID_G3DLIGHTTYPE;
    const int fl2_type = mdl != nullptr ? (int)mdl->fl2.Type : INVALID_G3DLIGHTTYPE;

    if (fl_type == G3DLIGHT_POINT)
        flashlightId = GRA3D_LID_POINT_FLASHLIGHT;

    bEnableSelfreflection  = pData->bEnableSelfreflection;
    bEmulateSelfreflection = pData->bEmulateSelfreflection;
    bEnableFlashlight      = pData->bEnableFlashlight;
    bEmulateFlashlight     = pData->bEmulateFlashlight;
    bEnableFlashlight2     = pData->bEnableFlashlight2;
    bEmulateFlashlight2    = pData->bEmulateFlashlight2;

    if (fl_type == INVALID_G3DLIGHTTYPE)
    {
        bEnableSelfreflection  = 0;
        bEmulateSelfreflection = 0;
        bEnableFlashlight      = 0;
        bEmulateFlashlight     = 0;
    }
    if (fl2_type == INVALID_G3DLIGHTTYPE)
    {
        bEnableFlashlight2  = 0;
        bEmulateFlashlight2 = 0;
    }

    if (bEnableFlashlight != 0)
    {
        pDest  = &pLDDest->aLight[flashlightId];
        *pDest = mdl->fl;
        gra3dSetLightIntens(pDest, pDest->afPad0[0] * pData->fAngleScale);
        for (j = 0; j < 4; j++)
            pDest->vDiffuse[j] *= pData->fDiffuseScale;
    }

    if (bEnableSelfreflection != 0)
        pLDDest->aLight[GRA3D_LID_POINT_SELFREFLECTION] = mdl->reflectionlight;

    if (fl2_type == G3DLIGHT_POINT && bEnableFlashlight2 != 0)
        pLDDest->aLight[GRA3D_LID_POINT_FLASHLIGHT2] = mdl->fl2;

    {
        GRA3DLIGHTSTATUS &rS = pLDDest->aStatus[GRA3D_LID_POINT_SELFREFLECTION];
        rS.bEnable                    = bEnableSelfreflection;
        rS.bEnableToChar              = 1;
        rS.bEnableToShadow            = 1;
        rS.bEmulateToDirectionalLight = bEmulateSelfreflection;
        rS.bEnableToPrelight          = 0;
    }
    {
        GRA3DLIGHTSTATUS &rS = pLDDest->aStatus[flashlightId];
        rS.bEnable                    = bEnableFlashlight;
        rS.bEnableToChar              = 1;
        rS.bEnableToShadow            = 1;
        rS.bEmulateToDirectionalLight = bEmulateFlashlight;
        rS.bEnableToPrelight          = 0;
    }
    {
        GRA3DLIGHTSTATUS &rS = pLDDest->aStatus[GRA3D_LID_POINT_FLASHLIGHT2];
        rS.bEnable                    = bEnableFlashlight2;
        rS.bEnableToChar              = 1;
        rS.bEnableToShadow            = 1;
        rS.bEmulateToDirectionalLight = bEmulateFlashlight2;
        rS.bEnableToPrelight          = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The player's room light set, made for a character as pData says, with the
// projector spot switched on in its slot while active, becomes the live
// light set. FF3 0x001e0058.

void _SetLightToShadow(GRA3DEMULATIONLIGHTDATACREATIONDATA *pData)
{
    GRA3DLIGHTDATA  LD;
    GRA3DLIGHTDATA *pLDSrc;
    int             lightId;

    pLDSrc = (GRA3DLIGHTDATA *)MapDrawGetLightPtr(GetPlyrAreaNo(), 0);
    gra3dGenerateLightDataToChar(&LD, pLDSrc, pData);

    if (gra3dIsSpecialLightActive() != 0)
    {
        lightId = gra3dGetProjectorSpotId();
        if (lightId != INVALID_GRA3DLIGHTID)
        {
            LD.aLight[lightId]          = gra3dGetProjectorSpot();
            LD.aStatus[lightId].bEnable = 1;
        }
    }

    gra3dSetLightData(&LD, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001e0138 (FF2 had gra3dStartSpecialLight / gra3dEndSpecialLight).

void gra3dSetSpecialLight(int active)
{
    s_bSpecialLightActive = active;
}

// FF3 0x001e0140

int gra3dIsSpecialLightActive(void)
{
    return s_bSpecialLightActive;
}

// ──────────────────────────────────────────────────────────────────────
// While the projector runs and the player is in its room: the projector
// slot of the room's light set (bank 0) with ranges 2800 / 2600 and a
// flickering diffuse, every channel 0.4 .. 0.45. FF3 0x001e0148.
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)

void gra3dUpdateSpecialLight(void)
{
    int             lightId;
    int             area;
    GRA3DLIGHTDATA *pLD;
    float           f;

    if (s_bSpecialLightActive == 0)
        return;
    lightId = gra3dGetProjectorSpotId();
    if (lightId == INVALID_GRA3DLIGHTID)
        return;

    area = CineGetProjectorAreaNo();
    if (area != GetPlyrAreaNo())
        return;

    pLD = (GRA3DLIGHTDATA *)MapDrawGetLightPtr4BuffBank(MapLoadGetBuffID(area), 0);

    s_ProjectorSpot           = pLD->aLight[lightId];
    s_ProjectorSpot.fMaxRange = 2800.0f;
    s_ProjectorSpot.fMinRange = 2600.0f;

    // rand() (ReiPan_Rand: the ROM's range); sdata 0x370c70 / 0x370c74 / 0x370c78,
    // 0.05 is 0x3d4ccccb in the ROM
    f = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 0.04999998f + 0.4f;
    std::fill(s_ProjectorSpot.vDiffuse, s_ProjectorSpot.vDiffuse + 4, f);
}

// FF3 0x001e0288

G3DLIGHT &gra3dGetProjectorSpot(void)
{
    return s_ProjectorSpot;
}

// FF3 0x001e0298 (FF2 picked the slot by room itself).

int gra3dGetProjectorSpotId(void)
{
    return CineGetProjectorSpotId();
}
