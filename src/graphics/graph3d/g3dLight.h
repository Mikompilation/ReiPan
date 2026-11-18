/* ==========================================================================
 *  graphics/graph3d/g3dLight.h
 *
 *  Light primitives of the g3d core (FF2 g3dLight.c): the light type and the
 *  0x70-byte G3DLIGHT record. Minimal so far: what reconstructed code uses.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DLIGHT_H
#define _GRAPHICS_GRAPH3D_G3DLIGHT_H

#include <stddef.h>                     // offsetof

#include <libvu0.h>                     // sceVu0FVECTOR

#include "sdk/scetypes.h"                // u_int, u_long

// G3DLIGHT.Type. FF3 assigns them to gra3d's 39 light slots as FF2 did by
// range (gra3d.h GRA3DLIGHTID), with FF3's own range sizes.
enum G3DLIGHTTYPE
{
    G3DLIGHT_DIRECTIONAL     = 0,
    G3DLIGHT_POINT           = 1,
    G3DLIGHT_SPOT            = 2,
    NUM_G3DLIGHTTYPE         = 3,
    G3DLIGHT_AMBIENT         = 3,
    INVALID_G3DLIGHTTYPE     = 0x7fffffff,
};

// One light (FF2 layout). FF3 copies it as 7 quadwords (gra3dSetLight 0x1dca70)
// and tests Type at +0x50 (gra3dLightDataAddOffsetPosition 0x1da618); the
// other scalar fields keep FF2's names, confirmed by g3dLight.c (0x1cde70..
// 0x1ce180): cone angles +0x54/+0x58, ranges +0x5c/+0x60, fFalloff +0x64.
struct G3DLIGHT                         /* 0x70 */
{
    /* 0x00 */ sceVu0FVECTOR vDiffuse;
    /* 0x10 */ sceVu0FVECTOR vSpecular;
    /* 0x20 */ sceVu0FVECTOR vAmbient;
    /* 0x30 */ sceVu0FVECTOR vPosition;
    /* 0x40 */ sceVu0FVECTOR vDirection;
    /* 0x50 */ int           Type;          // G3DLIGHTTYPE, stored as a word
    /* 0x54 */ float         fAngleInside;
    /* 0x58 */ float         fAngleOutside;
    /* 0x5c */ float         fMaxRange;
    /* 0x60 */ float         fMinRange;
    /* 0x64 */ float         fFalloff;      // defaulted to 1; FF3 g3dLight.c no longer reads it
    /* 0x68 */ float         afPad0[2];
};
static_assert(sizeof(G3DLIGHT) == 0x70, "G3DLIGHT is 0x70 bytes");
static_assert(offsetof(G3DLIGHT, vSpecular) == 0x10, "G3DLIGHT.vSpecular");
static_assert(offsetof(G3DLIGHT, vPosition) == 0x30, "G3DLIGHT.vPosition");
static_assert(offsetof(G3DLIGHT, vDirection) == 0x40, "G3DLIGHT.vDirection");
static_assert(offsetof(G3DLIGHT, Type) == 0x50, "G3DLIGHT.Type");
static_assert(offsetof(G3DLIGHT, fAngleInside) == 0x54, "G3DLIGHT.fAngleInside");
static_assert(offsetof(G3DLIGHT, fAngleOutside) == 0x58, "G3DLIGHT.fAngleOutside");
static_assert(offsetof(G3DLIGHT, fMaxRange) == 0x5c, "G3DLIGHT.fMaxRange");
static_assert(offsetof(G3DLIGHT, fMinRange) == 0x60, "G3DLIGHT.fMinRange");
static_assert(offsetof(G3DLIGHT, fFalloff) == 0x64, "G3DLIGHT.fFalloff");

// The g3d core's nine light slots (FF2 names): G3DCOREOBJECT.aLightData[]
// and the VU1 light status bits are laid out by these ranges.
enum G3DLIGHTINDEX
{
    G3DLIDX_DIRECTIONAL_0       = 0,
    G3DLIDX_DIRECTIONAL_2       = 2,
    G3DLIDX_POINT_0             = 3,
    G3DLIDX_POINT_2             = 5,
    G3DLIDX_SPOT_0              = 6,
    G3DLIDX_SPOT_2              = 8,
    NUM_G3DLIGHTINDEX           = 9,
    G3D_START_LIGHT_DIRECTIONAL = 0,
    G3D_NUM_LIGHT_DIRECTIONAL   = 3,
    G3D_START_LIGHT_POINT       = 3,
    G3D_NUM_LIGHT_POINT         = 3,
    G3D_START_LIGHT_SPOT        = 6,
    G3D_NUM_LIGHT_SPOT          = 3,
    G3D_MAX_LIGHT_PER_TYPE      = 3,
};

// One light slot of the core (FF3 g3dLightEnable / g3dSetLight: stride 0x80,
// bEnable at +0x70).
struct _LIGHTDATA                       /* 0x80 */
{
    /* 0x00 */ G3DLIGHT L;
    /* 0x70 */ int      bEnable;
    /* 0x74 */ int      aiPad[3];
};
static_assert(sizeof(_LIGHTDATA) == 0x80, "_LIGHTDATA is 0x80 bytes");
static_assert(offsetof(_LIGHTDATA, bEnable) == 0x70, "_LIGHTDATA.bEnable");

// The VU1 light status word. lAS holds one enable bit per light: bits 0..2
// directional, 4..6 point, 8..10 spot (_ApplyLight*; g3dCalcVertexColor tests
// 0x7 / 0x70 / 0x700). FF2 overlays a bitfield view (G3DVU1LIGHTACTIVITYSTATUS)
// on lAS; FF3 code only uses the word.
struct G3DVU1LIGHTSTATUS                /* 0x10 */
{
    /* 0x0 */ u_int  auiPad[2];
    /* 0x8 */ u_long lAS;
};
static_assert(sizeof(G3DVU1LIGHTSTATUS) == 0x10, "G3DVU1LIGHTSTATUS is 0x10 bytes");
static_assert(offsetof(G3DVU1LIGHTSTATUS, lAS) == 0x8, "G3DVU1LIGHTSTATUS.lAS");

// ---- g3dLight.c ----------------------------------------------------------
// FF3 0x1cde18: `ld t7, 8(a0); add t5, zero, t7` -- loads lAS into the t5
// register the VU0 lighting kernels test. No memory effect.
void g3dSetLightStatus(G3DVU1LIGHTSTATUS *pLS);
void g3dutilSetLightDefault(G3DLIGHT *pLight, int iLightType);    // G3DLIGHTTYPE
float g3dCalcSpotlightFalloff(G3DLIGHT *pLight, float *vVertexPosition);
float g3dCalcLightDistanceAttenuation(G3DLIGHT *pLight, float *vVertexPosition);
float g3dCalcLightAttenuation(G3DLIGHT *pLight, float *vVertex);

#endif /* _GRAPHICS_GRAPH3D_G3DLIGHT_H */
