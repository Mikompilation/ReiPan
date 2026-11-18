/* ==========================================================================
 *  graphics/graph3d/g3dLight.c
 *
 *  Per-light evaluation helpers of the g3d core: defaulting a G3DLIGHT, the
 *  spotlight cone falloff, and the distance / total attenuation a light
 *  applies at a world-space vertex (the CPU-side light model the effect and
 *  shadow code use; the VU kernels in g3dCore.c do the per-vertex work).
 *
 *  Ported from MioPan's g3dLight.c (FF2 0x1a2570..0x1a2a00); FF3's functions
 *  (0x1cde18..0x1ce180) are FF2's minus the asserts/warnings, with two
 *  changes: cosines go through Vu0Cosf, and the spotlight falloff is always
 *  the linear ramp (FF2's fFalloff != 1 powf branch and the dead acosf are
 *  gone; fFalloff is still defaulted to 1 but no longer read here).
 * ======================================================================== */

#include "graphics/graph3d/g3dLight.h"

#include <math.h>                               // sqrtf

#include "common/vu0math.h"                     // Vu0Cosf
#include "graphics/graph3d/g3dCore.h"           // g3dGetGlobalStateRef, G3DGS_LIGHTATTENUATIONTYPE
#include "graphics/graph3d/g3dGeom.h"           // g3dCalcAngle

#define G3D_HALF_PI 1.5707964f                  // rdata 0x370a6c

// ──────────────────────────────────────────────────────────────────────
// Loads the light activity word into the register the VU0 lighting kernels
// test (ld t7, 8(a0); add t5, zero, t7). No memory effect; on the PC the
// callers in g3dCore.c read pLS->lAS themselves.
// FF3 0x001cde18; FF2 g3dSetLightStatus 0x1a2570 (empty there).

void g3dSetLightStatus(G3DVU1LIGHTSTATUS *pLS)
{
    (void)pLS;
}

// ──────────────────────────────────────────────────────────────────────
// Reset pLight to the null light (all colours zero, position (0, 0, 0, 1),
// no range or cone, fFalloff 1) and stamp the requested type.
// FF3 0x001cde28; FF2 g3dutilSetLightDefault 0x1a2580 (g_NullLight inlined).

void g3dutilSetLightDefault(G3DLIGHT *pLight, int iLightType)
{
    for (int i = 0; i < 4; i++)
    {
        pLight->vDiffuse[i]   = 0.0f;
        pLight->vSpecular[i]  = 0.0f;
        pLight->vAmbient[i]   = 0.0f;
        pLight->vPosition[i]  = 0.0f;
        pLight->vDirection[i] = 0.0f;
    }
    pLight->vPosition[3]  = 1.0f;
    pLight->fAngleInside  = 0.0f;
    pLight->fAngleOutside = 0.0f;
    pLight->fMaxRange     = 0.0f;
    pLight->fMinRange     = 0.0f;
    pLight->fFalloff      = 1.0f;
    pLight->afPad0[0]     = 0.0f;
    pLight->afPad0[1]     = 0.0f;
    pLight->Type          = iLightType;         // the template's INVALID_G3DLIGHTTYPE is overwritten
}

// ──────────────────────────────────────────────────────────────────────
// Cone falloff of a spot light at vVertexPosition: 1 inside the inner cone,
// 0 outside the outer cone (or when the cone is inverted), linear in the
// cosine between. Clamps fAngleOutside into [0, PI/2] in place. Other light
// types return 1.
// FF3 0x001cde70; FF2 g3dCalcSpotlightFalloff 0x1a25c8.

float g3dCalcSpotlightFalloff(G3DLIGHT *pLight, float *vVertexPosition)
{
    float fOutside;
    float fCosPhi;
    float fCosTheta;
    float fCosAlpha;

    if (pLight->Type != G3DLIGHT_SPOT)
    {
        return 1.0f;
    }

    fOutside = pLight->fAngleOutside;
    if (fOutside < pLight->fAngleInside)
    {
        return 0.0f;
    }

    // g3dClampf(fOutside, 0, PI/2)
    if (fOutside < 0.0f)
    {
        fOutside = 0.0f;
    }
    else if (G3D_HALF_PI < fOutside)
    {
        fOutside = G3D_HALF_PI;
    }
    pLight->fAngleOutside = fOutside;

    fCosPhi   = Vu0Cosf(fOutside);
    fCosTheta = Vu0Cosf(pLight->fAngleInside);
    fCosAlpha = Vu0Cosf(g3dCalcAngle(pLight->vDirection, pLight->vPosition, vVertexPosition));

    if (fCosAlpha < fCosPhi)
    {
        return 0.0f;
    }
    if (fCosTheta <= fCosAlpha)
    {
        return 1.0f;
    }
    return (fCosAlpha - fCosPhi) / (fCosTheta - fCosPhi);
}

// ──────────────────────────────────────────────────────────────────────
// Distance attenuation of pLight at vVertexPosition: 1 within fMinRange,
// 0 beyond fMaxRange, and between a linear or squared ramp chosen by the
// G3DGS_LIGHTATTENUATIONTYPE global state (other values give 0). Directional
// lights never attenuate.
// The distance is g3dxVu0Length3: vsub.xyz; vmul.xyz; vadday.x;
// vmaddz.x vf1 (= x + y + 1 * z); vsqrt.
// FF3 0x001cdf68; FF2 g3dCalcLightDistanceAttenuation 0x1a2740.

float g3dCalcLightDistanceAttenuation(G3DLIGHT *pLight, float *vVertexPosition)
{
    float fDx;
    float fDy;
    float fDz;
    float fDistance;
    float fScope;
    float f;
    u_int uiType;

    if (pLight->Type == G3DLIGHT_DIRECTIONAL)
    {
        return 1.0f;
    }

    fDx       = pLight->vPosition[0] - vVertexPosition[0];
    fDy       = pLight->vPosition[1] - vVertexPosition[1];
    fDz       = pLight->vPosition[2] - vVertexPosition[2];
    fDistance = sqrtf(fDx * fDx + fDy * fDy + fDz * fDz);

    if (!(fDistance <= pLight->fMaxRange))      // c.le.S: a NaN distance gives 0
    {
        return 0.0f;
    }
    if (fDistance <= pLight->fMinRange)
    {
        return 1.0f;
    }

    fDistance = fDistance - pLight->fMinRange;
    fScope    = pLight->fMaxRange - pLight->fMinRange;

    uiType = *g3dGetGlobalStateRef(G3DGS_LIGHTATTENUATIONTYPE);
    if (uiType == 0)
    {
        return (fScope - fDistance) / fScope;
    }
    if (uiType == 1)
    {
        f = (fScope - fDistance) / fScope;
        return f * f;
    }
    return 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Total attenuation of pLight at vVertex: directional 1, point the distance
// attenuation, spot distance * cone falloff, the invalid type 0; anything
// else 1 (FF2 asserted there).
// FF3 0x001ce050; FF2 g3dCalcLightAttenuation 0x1a28d0.

float g3dCalcLightAttenuation(G3DLIGHT *pLight, float *vVertex)
{
    switch (pLight->Type)
    {
    case G3DLIGHT_POINT:
        return g3dCalcLightDistanceAttenuation(pLight, vVertex);
    case G3DLIGHT_SPOT:
        return g3dCalcLightDistanceAttenuation(pLight, vVertex) * g3dCalcSpotlightFalloff(pLight, vVertex);
    case INVALID_G3DLIGHTTYPE:
        return 0.0f;
    default:                                    // G3DLIGHT_DIRECTIONAL and unknown types
        return 1.0f;
    }
}
