/* ==========================================================================
 *  graphics/graph3d/g3dLightEx.h
 *
 *  Light emulation helpers (FF2 g3dLightEx.c, FF3 0x1ce0f8..0x1cec20): any
 *  light seen from one position, folded into a directional light; blending
 *  such lights; the box tests and power sort that pick lights per mesh.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DLIGHTEX_H
#define _GRAPHICS_GRAPH3D_G3DLIGHTEX_H

#include <stddef.h>                     // offsetof

#include <libvu0.h>                     // sceVu0FVECTOR

#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT

// One light reduced to a direction and a strength (FF2 layout). FF3: 0x30
// bytes (gra3dEmulateLightDataSub clears 39 of them with memset 0x750 and
// sorts them with a 0x30 stride), fLength at +0x20 (the sort key and the
// "> 0 counts" test). vDiffuse +0x00 / vDirection +0x10 confirmed by
// _ConvertEmulateDirectionallightDataToG3DLIGHT 0x1ce180.
#define G3D_MAX_BLENDLIGHT 6            // g3dBlendLight's stack array (FF2 g3dLight.h)
struct G3D_EMULATE_DIRECTIONALLIGHT_DATA    /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR vDiffuse;
    /* 0x10 */ sceVu0FVECTOR vDirection;
    /* 0x20 */ float         fLength;
    /* 0x24 */ int           aiPad[3];

    // std::sort order: strongest first.
    // layout: host -- an empty comparator, never stored.
    struct greater
    {
        bool operator()(const G3D_EMULATE_DIRECTIONALLIGHT_DATA &a,
                        const G3D_EMULATE_DIRECTIONALLIGHT_DATA &b) const
        {
            return a.fLength > b.fLength;
        }
    };
};
static_assert(sizeof(G3D_EMULATE_DIRECTIONALLIGHT_DATA) == 0x30, "G3D_EMULATE_DIRECTIONALLIGHT_DATA is 0x30 bytes");
static_assert(offsetof(G3D_EMULATE_DIRECTIONALLIGHT_DATA, vDirection) == 0x10, "G3D_EMULATE_DIRECTIONALLIGHT_DATA.vDirection");
static_assert(offsetof(G3D_EMULATE_DIRECTIONALLIGHT_DATA, fLength) == 0x20, "G3D_EMULATE_DIRECTIONALLIGHT_DATA.fLength");

// One light's power reaching a bounding box, for the power sort (FF3
// 0x1ce8c8 writes +0 power, +4 light, +8 index; 0xc-byte stride).
// layout: host -- pLight is a pointer (8 bytes on the host, 4 on the EE).
struct LIGHTCOMPAREDATA                 /* 0xc on the EE */
{
    /* 0x0 */ float     fPower;
    /* 0x4 */ G3DLIGHT *pLight;
    /* 0x8 */ int       iIndex;

    // std::sort order: strongest first.
    // layout: host -- an empty comparator, never stored.
    struct greater
    {
        bool operator()(const LIGHTCOMPAREDATA &a, const LIGHTCOMPAREDATA &b) const
        {
            return a.fPower > b.fPower;
        }
    };
};
static_assert(offsetof(LIGHTCOMPAREDATA, fPower) == 0x0, "LIGHTCOMPAREDATA.fPower");

// ---- g3dLightEx.c --------------------------------------------------------
// pLight seen from vPos as a directional light. FF3 0x001ce4a8.
void g3dEmulateDirectionalLight(G3D_EMULATE_DIRECTIONALLIGHT_DATA *pEDD, G3DLIGHT *pLight, float *vPos);
// fLength * |diffuse.xyz| of pLight emulated at vPos. FF3 0x001ce620.
float g3dCalcLightPower(G3DLIGHT *pLight, float *vPos);
// pLight emulated at vPos, written into pDest as a directional light
// (iLightType is not read). FF3 0x001ce670.
void gra3dEmulateDirectionalLight(G3DLIGHT *pDest, G3DLIGHT *pLight, int iLightType, float *vPos);
// Three directional lights from iNumEmulated emulated ones. FF3 0x001ce6a8.
void g3dGenerateDirectionalLightByEmulatedData(G3DLIGHT *aDest, G3D_EMULATE_DIRECTIONALLIGHT_DATA *aSrc,
                                               int iNumEmulated);
// Can pLight reach the bounding box avBB (8 corners + centre)? Directional:
// always; point / spot: their box tests. FF3 0x001ce870.
int  g3dIsBBLightingup(G3DLIGHT *pLight, float (*avBB)[4]);
// Intensity into afPad0[0]; a spot light's cone angles follow from it.
// FF3 0x001ce430.
void gra3dSetLightIntens(G3DLIGHT *pLight, float fIntens);
// aCD[i] = power of apLightSrc[i] reaching avBB (0 when it cannot), sorted
// strongest first. No FF3 callers. FF3 0x001ce8c8.
void g3dSortLightForBoundingBoxByPowerOrder(int iNum, LIGHTCOMPAREDATA *aCD, G3DLIGHT **apLightSrc,
                                            float (*avBB)[4]);
// iNumSrc (<= G3D_MAX_BLENDLIGHT) lights emulated at vTarget and blended into
// one directional light. FF3 0x001cea78.
void g3dBlendLight(G3DLIGHT *pLight, G3DLIGHT **apSrc, int iNumSrc, float *vTarget);

#endif /* _GRAPHICS_GRAPH3D_G3DLIGHTEX_H */
