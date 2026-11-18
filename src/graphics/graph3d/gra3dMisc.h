/* ==========================================================================
 *  graphics/graph3d/gra3dMisc.h
 *
 *  FF2 gra3dMisc.c: room prelighting, model shadows, a character's light
 *  set, the projector spot light (FF3 0x1dfbc8..0x1e0330).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DMISC_H
#define _GRAPHICS_GRAPH3D_GRA3DMISC_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

struct G3DLIGHT;                        // graphics/graph3d/g3dLight.h
struct GRA3DLIGHTDATA;                  // graphics/graph3d/gra3d.h
struct SGDFILEHEADER;                   // graphics/graph3d/sgd_types.h
struct SGDCOORDINATE;

// The light set slots the player's lights go to (FF2's LID_* names; FF3's
// numbers: the last three point slots and the last spot slot).
enum
{
    GRA3D_LID_POINT_FLASHLIGHT     = 27,    // the flashlight when it is a point light
    GRA3D_LID_POINT_FLASHLIGHT2    = 28,    // the second flashlight
    GRA3D_LID_POINT_SELFREFLECTION = 29,    // the self-reflection light
    GRA3D_LID_SPOT_FLASHLIGHT      = 38,    // the flashlight otherwise
};

// How a character's light set is made from the map's (FF2's layout; FF3
// data 0x35f6d0, the NPC models' set, matches it).
struct GRA3DEMULATIONLIGHTDATACREATIONDATA      /* 0x40 */
{
    /* 0x00 */ float vStaticDirLightColor[4];
    /* 0x10 */ float fAngleScale;
    /* 0x14 */ float fDiffuseScale;
    /* 0x18 */ float fMaplightScale;            // gra3dEmulateLightData's fMagnification
    /* 0x1c */ int   bEnableSelfreflection;
    /* 0x20 */ int   bEmulateSelfreflection;
    /* 0x24 */ int   bEnableFlashlight;
    /* 0x28 */ int   bEmulateFlashlight;
    /* 0x2c */ int   bEnableFlashlight2;
    /* 0x30 */ int   bEmulateFlashlight2;
    /* 0x34 */ int   bEnableStaticDirLight;
    /* 0x38 */ int   aiPad[2];
};
static_assert(sizeof(GRA3DEMULATIONLIGHTDATACREATIONDATA) == 0x40, "GRA3DEMULATIONLIGHTDATACREATIONDATA is 0x40 bytes");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, fAngleScale) == 0x10, "GRA3DEMULATIONLIGHTDATACREATIONDATA.fAngleScale");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, fDiffuseScale) == 0x14, "GRA3DEMULATIONLIGHTDATACREATIONDATA.fDiffuseScale");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, fMaplightScale) == 0x18, "GRA3DEMULATIONLIGHTDATACREATIONDATA.fMaplightScale");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, bEnableSelfreflection) == 0x1c, "GRA3DEMULATIONLIGHTDATACREATIONDATA.bEnableSelfreflection");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, bEmulateFlashlight2) == 0x30, "GRA3DEMULATIONLIGHTDATACREATIONDATA.bEmulateFlashlight2");
static_assert(offsetof(GRA3DEMULATIONLIGHTDATACREATIONDATA, bEnableStaticDirLight) == 0x34, "GRA3DEMULATIONLIGHTDATACREATIONDATA.bEnableStaticDirLight");

// FF3 0x001dfb90: area >= 0: its room buffer prelit again (MapDrawPrelightRoom
// with clear 0) -- after the monochrome mode or a scene changed the lights.
void gra3dPrelightScene(int area);
// FF3 0x001dfbc8: the player's room (when drawn) and the other buffer (when
// loaded) prelit again (clear 0) or their prelighting cleared (clear 1).
void gra3dPrelight(int clear);
// FF3 0x001dfc58: gra3dPrelight(1).
void gra3dClearPrelight(void);

// FF3 0x001dfc70: a runtime model's shadow with pLight, its world box avBBWorld.
void gra3dDrawSGDShadow(SGDFILEHEADER *pSGDTop, SGDCOORDINATE *pCoord, G3DLIGHT *pLight, float (*avBBWorld)[4]);
// FF3 0x001dfd00: every bounding-boxed block of a shadow model casts a shadow.
void gra3dDrawSGDShadowEveryObject(SGDFILEHEADER *pShadowModel, SGDCOORDINATE *pCoord, G3DLIGHT *pLight);

// FF3 0x001dfe08: pLDSrc with the player's flashlights and self-reflection
// light folded in, as pData says, into pLDDest.
void gra3dGenerateLightDataToChar(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc,
                                  GRA3DEMULATIONLIGHTDATACREATIONDATA *pData);
// FF3 0x001e0058: the player's room light set made for a character (plus the
// projector spot when on) becomes the live light set.
void _SetLightToShadow(GRA3DEMULATIONLIGHTDATACREATIONDATA *pData);

// The projector spot light (the cine projector rooms).
void      gra3dSetSpecialLight(int active);    // FF3 0x001e0138
int       gra3dIsSpecialLightActive(void);      // FF3 0x001e0140
void      gra3dUpdateSpecialLight(void);        // FF3 0x001e0148: once a frame while the projector film runs
G3DLIGHT &gra3dGetProjectorSpot(void);          // FF3 0x001e0288
int       gra3dGetProjectorSpotId(void);        // FF3 0x001e0298: its light set slot, INVALID_GRA3DLIGHTID elsewhere

#endif /* _GRAPHICS_GRAPH3D_GRA3DMISC_H */
