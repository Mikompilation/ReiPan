/* ==========================================================================
 *  graphics/graph3d/g3dCore.c
 *
 *  The g3d core: it owns the single G3DCOREOBJECT (s_pObject) that mirrors
 *  the renderer's fixed-function state -- render/global state words, global
 *  ambient, material, the five transforms, the nine light slots and the bound
 *  texture -- and keeps the VU1 memory image (Vu1Mem) in sync as state is
 *  pushed in. g3dCalcVertexColor is the EE-side mirror of the VU1 lighting
 *  (prelighting bakes vertex colours with it).
 *
 *  FF3 0x1cb890..0x1cced0, ported from MioPan's g3dCore.c (FF2) and checked
 *  against FF3 function by function. FF3 differences: no asserts, no GS
 *  register file in the object (g3dSetGsRegister* forward to gs_reg.c's
 *  cache), spot cosines through Vu0Cosf, and g3dCalcVertexColor reloads the
 *  light status (g3dSetLightStatus) before each group's accumulator.
 *
 *  The lighting kernels (_Vu0LoadMaterial*, _Vu0CalcColorData*,
 *  _Vu0LoadColorData*, _Vu0LoadColorCoeff*, _Vu0CalcVertexColor*Light*) are
 *  called through tables indexed by the lighting type (data 0x359c80..
 *  0x359ce0). Eighteen are inline functions of FF2's g3dLight.h whose
 *  out-of-line copies sit in FF3's linkonce run 0x2ba590..0x2bb108 (no Ghidra
 *  functions there); the three point-light accumulators are statics inside
 *  this file's range (0x1ccaf8 / 0x1ccc00 / 0x1ccd48). All 21 are
 *  instruction-identical to FF2's. On the EE they exchange data through
 *  fixed VU0 registers; here the two register sets travel as explicit
 *  arguments (G3DVU0MATERIALREGS / G3DVU0LIGHTREGS, MioPan's model).
 * ======================================================================== */

#include <math.h>                       // sqrtf
#include <string.h>

#include "graphics/graph3d/g3dCore.h"
#include "graphics/graph3d/g3dTexture.h"
#include "graphics/gs_reg.h"
#include <libgraph.h>                   // SCE_GS_TEX0_1, SCE_GS_TEX1_1, SCE_GS_XYOFFSET_1

// unassigned@001ef370 (math helpers), not defined yet:
float Vu0Cosf(float x);                                     // at 0x001ef838

LPFUNC_SETGSREGISTER    IG3DCompatible::s_pFuncSetGsRegister    = g3dSetGsRegister;     /* sdata 0x370a3c */
LPFUNC_SETGSREGISTERS   IG3DCompatible::s_pFuncSetGsRegisters   = g3dSetGsRegisters;    /* sdata 0x370a40 */
LPFUNC_GETGSREGISTERREF IG3DCompatible::s_pFuncGetGsRegisterRef = g3dGetGsRegisterRef;  /* sdata 0x370a44 */
LPFUNC_SETTRANSFORM     IG3DCompatible::s_pFuncSetTransform     = g3dSetTransform;      /* sdata 0x370a48 */
LPFUNC_GETTRANSFORMREF  IG3DCompatible::s_pFuncGetTransformRef  = g3dGetTransformRef;   /* sdata 0x370a4c */

// CVu0Matrix's hooks (sdata 0x370aa8 / 0x370aac) start null and only gra3dInit
// sets them; their owning object is not settled (between g3dGeom.c's and
// gra3d.c's sdata).
LPFUNC_VU0LOADMATRIX              CVu0Matrix::s_pFuncLoadMatrix;                        /* sdata 0x370aa8 */
LPFUNC_VU0APPLYMATRIXWITHOUTTRANS CVu0Matrix::s_pFuncApplyMatrixWithoutTrans;           /* sdata 0x370aac */

static G3DCOREOBJECT *s_pObject;        // sdata 0x370a24

/* --------------------------------------------------------------------------
 *  The VU0 register file, made explicit (see the file comment).
 *  layout: host -- PC-only stand-ins for VU0 registers, never in EE memory.
 * ------------------------------------------------------------------------ */
struct G3DVU0MATERIALREGS               // _g3dCalcColor's chain
{
    sceVu0FVECTOR vf3;                  // Material.vDiffuse
    sceVu0FVECTOR vf4;                  // Material.vAmbient
    sceVu0FVECTOR vf5;                  // Material.vSpecular; .w = fPower
};

// layout: host -- PC-only stand-ins for VU0 registers, never in EE memory.
struct G3DVU0LIGHTREGS                  // g3dCalcVertexColor's chain
{
    sceVu0FVECTOR vf7;                  // unit world-space vertex normal
    sceVu0FVECTOR vf11;                 // unit camera->vertex direction (phong)
    u_int         t5;                   // Light.status.lAS (g3dSetLightStatus)

    sceVu0FVECTOR avf8_10[G3D_MAX_LIGHT_PER_TYPE];  // unit light->vertex
    sceVu0FVECTOR vf15;                 // xyz: per-light distance attenuation
    sceVu0FVECTOR vf16;                 // xyz: per-light spot cone ramp
    u_int         t0;                   // skip mask of vf15
    u_int         t1;                   // skip mask of vf16

    sceVu0FVECTOR vf17;                 // xyz: per-light N.L (_Vu0LoadColorCoeff*)
    sceVu0FVECTOR vf18;                 // xyz: per-light N.H
    u_int         t3;                   // skip mask of vf17
    u_int         t4;                   // skip mask of vf18

    sceVu0FVECTOR avAmbient[G3D_MAX_LIGHT_PER_TYPE];    // vf25..vf27 (_Vu0LoadColorData*)
    sceVu0FVECTOR avDiffuse[G3D_MAX_LIGHT_PER_TYPE];    // vf28..vf30
    sceVu0FVECTOR avSpecular[G3D_MAX_LIGHT_PER_TYPE];   // vf31, vf21, vf22
};

typedef void (*LPFUNC_LOADMATERIAL)(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat);
typedef void (*LPFUNC_CALCCOLORDATA)(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL);
typedef void (*LPFUNC_LOADCOLOR)(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD);
typedef void (*LPFUNC_LOADCOLORCOEFF)(G3DVU0LIGHTREGS *r);
typedef void (*LPFUNC_CALCVERTEXCOLOR)(float *vDestColor, const G3DVU0LIGHTREGS *r);

static void _Vu0LoadMaterialConstant(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat);
static void _Vu0LoadMaterialLambert(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat);
static void _Vu0LoadMaterialPhong(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat);
static void _Vu0CalcColorDataConstant(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL);
static void _Vu0CalcColorDataLambert(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL);
static void _Vu0CalcColorDataPhong(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL);
static void _Vu0LoadColorDataConstant(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD);
static void _Vu0LoadColorDataLambert(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD);
static void _Vu0LoadColorDataPhong(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD);
static void _Vu0LoadColorCoeffConstant(G3DVU0LIGHTREGS *r);
static void _Vu0LoadColorCoeffLambert(G3DVU0LIGHTREGS *r);
static void _Vu0LoadColorCoeffPhong(G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorDirectinalLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorDirectinalLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorDirectinalLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorPointLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorPointLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorPointLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorSpotLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorSpotLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r);
static void _Vu0CalcVertexColorSpotLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r);

// The kernel tables, indexed by G3DLIGHTINGTYPE (data 0x359c80..0x359ce0).
static LPFUNC_CALCCOLORDATA   s_apfCalcColorData[NUM_G3DLIGHTINGTYPE] =                     // 0x359c80
{
    _Vu0CalcColorDataConstant, _Vu0CalcColorDataLambert, _Vu0CalcColorDataPhong,
};
static LPFUNC_LOADMATERIAL    s_apfLoadMaterial[NUM_G3DLIGHTINGTYPE] =                      // 0x359c90
{
    _Vu0LoadMaterialConstant, _Vu0LoadMaterialLambert, _Vu0LoadMaterialPhong,
};
static LPFUNC_LOADCOLOR       s_apfLoadColor[NUM_G3DLIGHTINGTYPE] =                         // 0x359ca0
{
    _Vu0LoadColorDataConstant, _Vu0LoadColorDataLambert, _Vu0LoadColorDataPhong,
};
static LPFUNC_LOADCOLORCOEFF  s_apfLoadColorCoeff[NUM_G3DLIGHTINGTYPE] =                    // 0x359cb0
{
    _Vu0LoadColorCoeffConstant, _Vu0LoadColorCoeffLambert, _Vu0LoadColorCoeffPhong,
};
static LPFUNC_CALCVERTEXCOLOR s_apfCalcVertexColorDirectionalLight[NUM_G3DLIGHTINGTYPE] =    // 0x359cc0
{
    _Vu0CalcVertexColorDirectinalLightConstant, _Vu0CalcVertexColorDirectinalLightLambert,
    _Vu0CalcVertexColorDirectinalLightPhong,
};
static LPFUNC_CALCVERTEXCOLOR s_apfCalcVertexColorPointLight[NUM_G3DLIGHTINGTYPE] =          // 0x359cd0
{
    _Vu0CalcVertexColorPointLightConstant, _Vu0CalcVertexColorPointLightLambert,
    _Vu0CalcVertexColorPointLightPhong,
};
static LPFUNC_CALCVERTEXCOLOR s_apfCalcVertexColorSpotLight[NUM_G3DLIGHTINGTYPE] =           // 0x359ce0
{
    _Vu0CalcVertexColorSpotLightConstant, _Vu0CalcVertexColorSpotLightLambert,
    _Vu0CalcVertexColorSpotLightPhong,
};

/* --------------------------------------------------------------------------
 *  Small VU0 helpers (inlined in the ROM).
 * ------------------------------------------------------------------------ */
static inline void _CopyVector(float *vDst, const float *vSrc)
{
    vDst[0] = vSrc[0];
    vDst[1] = vSrc[1];
    vDst[2] = vSrc[2];
    vDst[3] = vSrc[3];
}

// sqc2 vf0: (0, 0, 0, 1). Disabled light slots keep w == 1.
static inline void _ClearVector(float *vDst)
{
    vDst[0] = 0.0f;
    vDst[1] = 0.0f;
    vDst[2] = 0.0f;
    vDst[3] = 1.0f;
}

static inline float _Dot3(const float *v0, const float *v1)
{
    return v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2];
}

// vrsqrt Q, vf0w, |v|^2 / vmul.w vf13, vf12, vf0 / vmulq.xyz: xyz normalised,
// w copied. PC: a zero vector gives 0 instead of the VU's huge finite value.
static inline void _NormalizeKeepW(float *vDst, const float *vSrc)
{
    float fLen2 = _Dot3(vSrc, vSrc);
    float q     = (fLen2 > 0.0f) ? 1.0f / sqrtf(fLen2) : 0.0f;

    vDst[0] = vSrc[0] * q;
    vDst[1] = vSrc[1] * q;
    vDst[2] = vSrc[2] * q;
    vDst[3] = vSrc[3];
}

// cfc2 t, vc1 / andi 0xee after a vector term: per xyz lane the zero and sign
// flags, i.e. "<= 0" -- 0x88 for x, 0x44 for y, 0x22 for z.
static u_int _Vu0FlagsLEZ(const float *v)
{
    u_int uiFlags = 0;

    if (v[0] <= 0.0f) uiFlags |= 0x88;
    if (v[1] <= 0.0f) uiFlags |= 0x44;
    if (v[2] <= 0.0f) uiFlags |= 0x22;

    return uiFlags;
}

static inline float _Clamp01(float f)
{
    if (f < 0.0f) return 0.0f;
    if (f > 1.0f) return 1.0f;
    return f;
}

/* --------------------------------------------------------------------------
 *  _g3dCalcColor (FF2 _CalcColor)                               FF3 0x001cb890
 *
 *  Re-derive the per-light colours in the VU1 image (Color = ColorOrigin x
 *  material) for the current lighting type: load the material once, then fold
 *  each light group.
 * ------------------------------------------------------------------------ */
static void _g3dCalcColor(void)
{
    G3DVU0MATERIALREGS r;
    G3DVU1COLOR       *pColor       = &s_pObject->Vu1Mem.Color;
    const G3DVU1COLOR *pColorOrigin = &s_pObject->Vu1Mem.ColorOrigin;
    int                iLightingType = s_pObject->auiGlobalState[G3DGS_LIGHTINGTYPE];

    s_apfLoadMaterial[iLightingType](&r, &s_pObject->Vu1Mem.Material);

    s_apfCalcColorData[iLightingType](&r, &pColor->dir,   &pColorOrigin->dir);
    s_apfCalcColorData[iLightingType](&r, &pColor->point, &pColorOrigin->point);
    s_apfCalcColorData[iLightingType](&r, &pColor->spot,  &pColorOrigin->spot);
}

/* --------------------------------------------------------------------------
 *  _CalcAmbient                                                 FF3 0x001cb928
 *
 *  Calc.vAmbient = ambient * material ambient + material emissive, the
 *  constant term of every lit vertex. Both steps are .xyz; the sqc2 stores
 *  the whole quadword, so Calc.vAmbient.w ends up as the global ambient's w.
 * ------------------------------------------------------------------------ */
static void _CalcAmbient(void)
{
    const float *vAmb = s_pObject->vAmbient;
    const G3DMATERIAL &rMat = s_pObject->Vu1Mem.Material;
    float *vDst = s_pObject->Vu1Mem.Calc.vAmbient;
    int    c;

    for (c = 0; c < 3; c++)
    {
        vDst[c] = vAmb[c] * rMat.vAmbient[c] + rMat.vEmissive[c];
    }
    vDst[3] = vAmb[3];
}

/* --------------------------------------------------------------------------
 *  _ApplyLightDirectional                                       FF3 0x001cb970
 *
 *  Per directional slot: the enable bit (lAS bits 0..2), then the light's
 *  colours into ColorOrigin.dir and its normalised direction, or vf0 in all
 *  four when it is off.
 * ------------------------------------------------------------------------ */
static void _ApplyLightDirectional(void)
{
    G3DVU1MEMLAYOUT &rVu1 = s_pObject->Vu1Mem;
    int              i;

    for (i = 0; i < G3D_NUM_LIGHT_DIRECTIONAL; i++)
    {
        int bEnable = s_pObject->aLightData[i].bEnable;

        if (bEnable == 0)
        {
            rVu1.Light.status.lAS &= ~(u_long)(1 << i);
        }
        else
        {
            rVu1.Light.status.lAS |= (u_long)(1 << i);
        }

        if (bEnable != 0)
        {
            const G3DLIGHT *pL = &s_pObject->aLightData[i].L;

            _CopyVector(rVu1.ColorOrigin.dir.avAmbient[i],  pL->vAmbient);
            _CopyVector(rVu1.ColorOrigin.dir.avDiffuse[i],  pL->vDiffuse);
            _CopyVector(rVu1.ColorOrigin.dir.avSpecular[i], pL->vSpecular);
            _NormalizeKeepW(rVu1.Light.dir.avDirection[i], pL->vDirection);
        }
        else
        {
            _ClearVector(rVu1.ColorOrigin.dir.avAmbient[i]);
            _ClearVector(rVu1.ColorOrigin.dir.avDiffuse[i]);
            _ClearVector(rVu1.ColorOrigin.dir.avSpecular[i]);
            _ClearVector(rVu1.Light.dir.avDirection[i]);
        }
    }
}

/* --------------------------------------------------------------------------
 *  _ApplyLightPoint                                             FF3 0x001cba78
 *
 *  Point slots 3..5 (lAS bits 4..6): colours, position, max range and
 *  1 / (max - min) -- the pair g3dCalcVertexColor's linear ramp uses.
 * ------------------------------------------------------------------------ */
static void _ApplyLightPoint(void)
{
    G3DVU1MEMLAYOUT &rVu1 = s_pObject->Vu1Mem;
    int              i;

    for (i = 0; i < G3D_NUM_LIGHT_POINT; i++)
    {
        int iId     = G3D_START_LIGHT_POINT + i;
        int bEnable = s_pObject->aLightData[iId].bEnable;

        if (bEnable == 0)
        {
            rVu1.Light.status.lAS &= ~(u_long)(1 << (i + 4));
        }
        else
        {
            rVu1.Light.status.lAS |= (u_long)(1 << (i + 4));
        }

        if (bEnable == 0)
        {
            _ClearVector(rVu1.ColorOrigin.point.avAmbient[i]);
            _ClearVector(rVu1.ColorOrigin.point.avDiffuse[i]);
            _ClearVector(rVu1.ColorOrigin.point.avSpecular[i]);
            _ClearVector(rVu1.Light.point.avPosition[i]);
            rVu1.Light.point.vMaxRange[i]             = 0.0f;
            rVu1.Light.point.vMax_Min_InverseRange[i] = 0.0f;
        }
        else
        {
            const G3DLIGHT *pL = &s_pObject->aLightData[iId].L;

            _CopyVector(rVu1.ColorOrigin.point.avAmbient[i],  pL->vAmbient);
            _CopyVector(rVu1.ColorOrigin.point.avDiffuse[i],  pL->vDiffuse);
            _CopyVector(rVu1.ColorOrigin.point.avSpecular[i], pL->vSpecular);
            _CopyVector(rVu1.Light.point.avPosition[i],       pL->vPosition);
            rVu1.Light.point.vMax_Min_InverseRange[i] = 1.0f / (pL->fMaxRange - pL->fMinRange);
            rVu1.Light.point.vMaxRange[i]             = pL->fMaxRange;
        }
    }
}

/* --------------------------------------------------------------------------
 *  _ApplyLightSpot                                              FF3 0x001cbbb0
 *
 *  Spot slots 6..8 (lAS bits 8..10): colours, position, normalised direction,
 *  max range, 1 / (max - min), cos(outside) and 1 / (cos(inside) -
 *  cos(outside)). FF3 takes the cosines with Vu0Cosf (FF2: cosf).
 * ------------------------------------------------------------------------ */
static void _ApplyLightSpot(void)
{
    G3DVU1MEMLAYOUT &rVu1 = s_pObject->Vu1Mem;
    int              i;

    for (i = 0; i < G3D_NUM_LIGHT_SPOT; i++)
    {
        int iId     = G3D_START_LIGHT_SPOT + i;
        int bEnable = s_pObject->aLightData[iId].bEnable;

        if (bEnable == 0)
        {
            rVu1.Light.status.lAS &= ~(u_long)(1 << (i + 8));
        }
        else
        {
            rVu1.Light.status.lAS |= (u_long)(1 << (i + 8));
        }

        if (bEnable == 0)
        {
            _ClearVector(rVu1.ColorOrigin.spot.avAmbient[i]);
            _ClearVector(rVu1.ColorOrigin.spot.avDiffuse[i]);
            _ClearVector(rVu1.ColorOrigin.spot.avSpecular[i]);
            _ClearVector(rVu1.Light.spot.avPosition[i]);
            _ClearVector(rVu1.Light.spot.avDirection[i]);
            rVu1.Light.spot.vCosOutside[i]           = 0.0f;
            rVu1.Light.spot.vMax_Min_InverseRange[i] = 0.0f;
            rVu1.Light.spot.vMaxRange[i]             = 0.0f;
            rVu1.Light.spot.vCosIn_Out_Inverse[i]    = 0.0f;
        }
        else
        {
            const G3DLIGHT *pL = &s_pObject->aLightData[iId].L;
            float           fCosOutside;
            float           fCosInside;

            _CopyVector(rVu1.ColorOrigin.spot.avAmbient[i],  pL->vAmbient);
            _CopyVector(rVu1.ColorOrigin.spot.avDiffuse[i],  pL->vDiffuse);
            _CopyVector(rVu1.ColorOrigin.spot.avSpecular[i], pL->vSpecular);
            _CopyVector(rVu1.Light.spot.avPosition[i],       pL->vPosition);
            _NormalizeKeepW(rVu1.Light.spot.avDirection[i],  pL->vDirection);

            rVu1.Light.spot.vMaxRange[i]             = pL->fMaxRange;
            rVu1.Light.spot.vMax_Min_InverseRange[i] = 1.0f / (pL->fMaxRange - pL->fMinRange);

            fCosOutside = Vu0Cosf(pL->fAngleOutside);
            fCosInside  = Vu0Cosf(pL->fAngleInside);
            rVu1.Light.spot.vCosOutside[i]        = fCosOutside;
            rVu1.Light.spot.vCosIn_Out_Inverse[i] = 1.0f / (fCosInside - fCosOutside);
        }
    }
}

/* --------------------------------------------------------------------------
 *  g3dInitialize                                                FF3 0x001cbd90
 *
 *  Bind the core to the caller's object and prime it: lambert lighting,
 *  lighting/colour-vertex/specular on, null material (power 1), zero ambient,
 *  the nine slots disabled with their type's default light, and identity in
 *  all five transforms. (The ROM's opening lqc2 of 0x329510 is dead.)
 * ------------------------------------------------------------------------ */
void g3dInitialize(G3DCREATIONDATA *pCD)
{
    int i;

    s_pObject = pCD->pObj;

    s_pObject->auiRenderState[G3DRS_LIGHTING]               = 1;
    s_pObject->auiRenderState[G3DRS_COLORVERTEX]            = 1;
    s_pObject->auiRenderState[G3DRS_SPECULARENABLE]         = 1;
    s_pObject->auiRenderState[G3DRS_DIFFUSEMATERIALSOURCE]  = 0;
    s_pObject->auiRenderState[G3DRS_SPECULARMATERIALSOURCE] = 0;
    s_pObject->auiRenderState[G3DRS_AMBIENTMATERIALSOURCE]  = 0;
    s_pObject->auiRenderState[G3DRS_EMISSIVEMATERIALSOURCE] = 0;
    s_pObject->auiGlobalState[G3DGS_LIGHTATTENUATIONTYPE]   = 0;
    s_pObject->auiGlobalState[G3DGS_LIGHTINGTYPE]           = G3DLT_LAMBERT;

    memset(&s_pObject->Material, 0, sizeof(G3DMATERIAL));
    s_pObject->Material.fPower = 1.0f;

    _ClearVector(s_pObject->vAmbient);      // sqc2 vf0: (0, 0, 0, 1)

    for (i = 0; i < NUM_G3DLIGHTINDEX; i++)
    {
        int iLightType;

        s_pObject->aLightData[i].bEnable = 0;

        if (i < G3D_START_LIGHT_POINT)
        {
            iLightType = G3DLIGHT_DIRECTIONAL;
        }
        else if (i < G3D_START_LIGHT_SPOT)
        {
            iLightType = G3DLIGHT_POINT;
        }
        else if (i < NUM_G3DLIGHTINDEX)
        {
            iLightType = G3DLIGHT_SPOT;
        }
        else
        {
            iLightType = INVALID_G3DLIGHTTYPE;
        }

        g3dutilSetLightDefault(&s_pObject->aLightData[i].L, iLightType);
    }

    for (i = 0; i < NUM_G3DTRANSFORMSTATETYPE; i++)
    {
        sceVu0UnitMatrix(s_pObject->amatTransform[i]);  // the ROM copies g_matUnit (0x3293c0)
    }
}

/* --------------------------------------------------------------------------
 *  g3dSetRenderState                                            FF3 0x001cbf10
 *
 *  Store a render-state word; the fog colour (0x00BBGGRR) also goes to the
 *  GS FOGCOL register. Unreferenced in FF3.
 * ------------------------------------------------------------------------ */
int g3dSetRenderState(int state, u_int uiValue)
{
    s_pObject->auiRenderState[state] = uiValue;

    if (state == G3DRS_FOGCOLOR)
    {
        g3dSetGsRegister((u_long)(uiValue & 0xff)
                         | ((u_long)((uiValue >> 8) & 0xff) << 8)
                         | ((u_long)((uiValue >> 16) & 0xff) << 16),
                         SCE_GS_FOGCOL, 1);
    }

    return 1;
}

// FF3 0x001cbf90 -- unreferenced in FF3.
u_int *g3dGetRenderStateRef(int state)
{
    return &s_pObject->auiRenderState[state];
}

// FF3 0x001cbfa0 -- unreferenced in FF3.
int g3dSetGlobalState(int state, u_int uiValue)
{
    s_pObject->auiGlobalState[state] = uiValue;

    return 1;
}

// FF3 0x001cbfb8
u_int *g3dGetGlobalStateRef(int state)
{
    return &s_pObject->auiGlobalState[state];
}

/* --------------------------------------------------------------------------
 *  g3dSetAmbient                                                FF3 0x001cbfd0
 *
 *  The global ambient, kept in the object and in the VU1 global block; then
 *  Calc.vAmbient is refreshed.
 * ------------------------------------------------------------------------ */
void g3dSetAmbient(float *vAmbient)
{
    _CopyVector(s_pObject->vAmbient, vAmbient);
    _CopyVector(s_pObject->Vu1Mem.Global.vAmbient, vAmbient);

    _CalcAmbient();
}

// FF3 0x001cc008
float *g3dGetAmbientRef(void)
{
    return s_pObject->vAmbient;
}

/* --------------------------------------------------------------------------
 *  g3dSetTransform                                              FF3 0x001cc018
 *
 *  Store the matrix and propagate it into the VU1 transform block: VIEW also
 *  becomes matCamera, and VIEW / PROJECTION rebuild matWorldScreen =
 *  projection * view. Always returns 1.
 * ------------------------------------------------------------------------ */
int g3dSetTransform(int state, float (*mat)[4])
{
    G3DVU1TRANSFORM *rT;

    sceVu0CopyMatrix(s_pObject->amatTransform[state], mat);

    rT = &s_pObject->Vu1Mem.Transform;

    switch (state)
    {
        case G3DTS_VIEW:
            sceVu0CopyMatrix(rT->matCamera, mat);
            sceVu0MulMatrix(rT->matWorldScreen, s_pObject->amatTransform[G3DTS_PROJECTION], mat);
            break;

        case G3DTS_PROJECTION:
            sceVu0MulMatrix(rT->matWorldScreen, mat, s_pObject->amatTransform[G3DTS_VIEW]);
            break;

        case G3DTS_WORLD:
            sceVu0CopyMatrix(rT->matLocalWorld, mat);
            break;

        case G3DTS_WORLD1:
            sceVu0CopyMatrix(rT->matLocalWorld1, mat);
            break;

        case G3DTS_WORLDCLIP:
            sceVu0CopyMatrix(rT->matWorldClip, mat);
            break;
    }

    return 1;
}

// FF3 0x001cc1f0
float (*g3dGetTransformRef(int state))[4]
{
    return s_pObject->amatTransform[state];
}

/* --------------------------------------------------------------------------
 *  g3dSetMaterial                                               FF3 0x001cc208
 *
 *  Copy the material into the object and the VU1 material block, put the
 *  diffuse alpha into Calc.vMisc.x, then rebuild the ambient term and the
 *  per-light colours.
 * ------------------------------------------------------------------------ */
int g3dSetMaterial(const G3DMATERIAL *pMaterial)
{
    s_pObject->Material        = *pMaterial;
    s_pObject->Vu1Mem.Material = s_pObject->Material;

    s_pObject->Vu1Mem.Calc.vMisc[0] = s_pObject->Material.vDiffuse[3];

    _CalcAmbient();
    _g3dCalcColor();

    return 1;
}

// FF3 0x001cc2a0
G3DMATERIAL *g3dGetMaterialRef(void)
{
    return &s_pObject->Material;
}

// FF3 0x001cc2b0
int g3dLightEnable(int iLightId, int bEnable)
{
    s_pObject->aLightData[iLightId].bEnable = bEnable;

    return 1;
}

// FF3 0x001cc2c8
int g3dIsLightEnable(int iLightId)
{
    return s_pObject->aLightData[iLightId].bEnable;
}

// FF3 0x001cc2e0 -- a plain 7-quadword copy (FF2's type/range checks are gone).
int g3dSetLight(int iLightId, const G3DLIGHT *pLight)
{
    s_pObject->aLightData[iLightId].L = *pLight;

    return 1;
}

// FF3 0x001cc330
G3DLIGHT *g3dGetLightRef(int iLightId)
{
    return &s_pObject->aLightData[iLightId].L;
}

/* --------------------------------------------------------------------------
 *  g3dApplyLight                                                FF3 0x001cc348
 *
 *  Push all nine slots into the VU1 image, load the light status for the
 *  kernels and rebuild the per-light colours.
 * ------------------------------------------------------------------------ */
int g3dApplyLight(void)
{
    _ApplyLightDirectional();
    _ApplyLightPoint();
    _ApplyLightSpot();

    g3dSetLightStatus(&s_pObject->Vu1Mem.Light.status);
    _g3dCalcColor();

    return 1;
}

/* --------------------------------------------------------------------------
 *  g3dSetTexture                                                FF3 0x001cc390
 *
 *  Bind a texture to a stage; a non-null one is preloaded (vtable slot +8)
 *  and its TEX0_1 / TEX1_1 values go to the GS.
 * ------------------------------------------------------------------------ */
int g3dSetTexture(int iStage, CTexture *pTexture)
{
    GS_REG_SET aGPA[2];

    s_pObject->Resource.apTexture[iStage] = pTexture;

    if (pTexture != NULL)
    {
        pTexture->PreLoad();

        aGPA[0].data = pTexture->GetTextureDataRef().lTex0;
        aGPA[0].reg  = SCE_GS_TEX0_1;
        aGPA[1].data = pTexture->GetTextureDataRef().lTex1;
        aGPA[1].reg  = SCE_GS_TEX1_1;
        g3dSetGsRegisters(aGPA, 2, 1);
    }

    return 1;
}

/* --------------------------------------------------------------------------
 *  The per-vertex staging blocks g3dCalcVertexColor inlines (MioPan's split).
 * ------------------------------------------------------------------------ */

// World position (w: the vertex's own w, `vadd.xyz`) and the unit world
// normal (w 0) through amatTransform[WORLD].
static void _Vu0CalcVertexPositionNormal(G3DVU0LIGHTREGS *r, float *vPos,
                                         const float *vVertex, const float *vNormal)
{
    const float (*matWorld)[4] = s_pObject->amatTransform[G3DTS_WORLD];
    sceVu0FVECTOR vNrm;
    float         q;
    int           c;

    for (c = 0; c < 3; c++)
    {
        vPos[c] = vVertex[0] * matWorld[0][c] + vVertex[1] * matWorld[1][c]
                + vVertex[2] * matWorld[2][c] + matWorld[3][c];
        vNrm[c] = vNormal[0] * matWorld[0][c] + vNormal[1] * matWorld[1][c]
                + vNormal[2] * matWorld[2][c];
    }
    vPos[3] = vVertex[3];
    vNrm[3] = 0.0f;

    // PC: vrsqrt of 0 is a huge finite value on the VU; 0 keeps the host finite.
    q = _Dot3(vNrm, vNrm);
    q = (q > 0.0f) ? 1.0f / sqrtf(q) : 0.0f;

    for (c = 0; c < 4; c++)
    {
        r->vf7[c] = vNrm[c] * q;
    }
}

// Unit camera->vertex direction (phong only): position - view row 3.
static void _Vu0CalcEyeDirection(G3DVU0LIGHTREGS *r, const float *vPos)
{
    const float *vEye = s_pObject->amatTransform[G3DTS_VIEW][3];
    float        q;
    int          c;

    for (c = 0; c < 3; c++)
    {
        r->vf11[c] = vPos[c] - vEye[c];
    }

    q = _Dot3(r->vf11, r->vf11);
    q = (q > 0.0f) ? 1.0f / sqrtf(q) : 0.0f;

    for (c = 0; c < 3; c++)
    {
        r->vf11[c] *= q;
    }
}

// vf8..vf10 = -avDirection[i] (`vsub.xyz` from vf0, w kept).
static void _Vu0SetupDirectionalLights(G3DVU0LIGHTREGS *r, const sceVu0FVECTOR *avDirection)
{
    int i;
    int c;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        for (c = 0; c < 3; c++)
        {
            r->avf8_10[i][c] = -avDirection[i][c];
        }
        r->avf8_10[i][3] = avDirection[i][3];
    }
}

// Point and spot groups: per light the unit light->vertex direction and the
// distance, then vf15.xyz = clamp((max - d) / (max - min), 0, 1) with its
// "<= 0" mask in t0 (out of range).
static void _Vu0SetupPositionalLights(G3DVU0LIGHTREGS *r, const float *vPos,
                                      const sceVu0FVECTOR *avPosition,
                                      const float *vMaxRange,
                                      const float *vMax_Min_InverseRange)
{
    float vDistance[G3D_MAX_LIGHT_PER_TYPE];
    int   i;
    int   c;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        float d;

        for (c = 0; c < 3; c++)
        {
            r->avf8_10[i][c] = vPos[c] - avPosition[i][c];
        }

        d            = sqrtf(_Dot3(r->avf8_10[i], r->avf8_10[i]));
        vDistance[i] = d;

        if (d > 0.0f)
        {
            float q = 1.0f / d;

            for (c = 0; c < 3; c++)
            {
                r->avf8_10[i][c] *= q;
            }
        }
    }

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        r->vf15[i] = _Clamp01((vMaxRange[i] - vDistance[i]) * vMax_Min_InverseRange[i]);
    }
    r->vf15[3] = 0.0f;

    r->t0 = _Vu0FlagsLEZ(r->vf15);
}

// Spot cone: vf16.xyz = (dir . light->vertex - cos out) * 1/(cos in - cos out);
// mask from the product (outside the cone drops the light), then a high
// clamp only.
static void _Vu0CalcSpotCone(G3DVU0LIGHTREGS *r, const sceVu0FVECTOR *avDirection,
                             const float *vCosOutside, const float *vCosIn_Out_Inverse)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        r->vf16[i] = (_Dot3(avDirection[i], r->avf8_10[i]) - vCosOutside[i]) * vCosIn_Out_Inverse[i];
    }
    r->vf16[3] = 0.0f;

    r->t1 = _Vu0FlagsLEZ(r->vf16);

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        if (r->vf16[i] > 1.0f) r->vf16[i] = 1.0f;
    }
}

/* --------------------------------------------------------------------------
 *  g3dCalcVertexColor                                           FF3 0x001cc420
 *
 *  The EE mirror of the vertex lighting: world position and normal, the eye
 *  direction for phong, vDest = vColor, then for each light group with an
 *  enabled light (lAS & 0x7 / 0x70 / 0x700) load its colours, stage its
 *  geometry, take the coefficients, reload the light status
 *  (g3dSetLightStatus, FF3 only) and accumulate. Last the constant ambient
 *  (.xyz) and a clamp of all four lanes to [0, 1].
 * ------------------------------------------------------------------------ */
// callcheck: g3dSetLightStatus
void g3dCalcVertexColor(float *vDest, const float *vVertex, const float *vNormal, const float *vColor)
{
    G3DVU1MEMLAYOUT &rLO = s_pObject->Vu1Mem;
    G3DVU1LIGHT     &rL  = rLO.Light;
    G3DVU0LIGHTREGS  r;
    sceVu0FVECTOR    vPos;
    sceVu0FVECTOR    vWork;
    int              iLightingType = s_pObject->auiGlobalState[G3DGS_LIGHTINGTYPE];
    int              c;

    memset(&r, 0, sizeof(r));

    _Vu0CalcVertexPositionNormal(&r, vPos, vVertex, vNormal);

    if (iLightingType == G3DLT_PHONG)
    {
        _Vu0CalcEyeDirection(&r, vPos);
    }

    _CopyVector(vDest, vColor);

    // PC: the ROM calls g3dSetLightStatus here, which only loads lAS into
    // the t5 register the accumulators test; r.t5 is that register.
    if (rL.status.lAS & 0x7)
    {
        s_apfLoadColor[iLightingType](&r, &rLO.Color.dir);
        _Vu0SetupDirectionalLights(&r, rL.dir.avDirection);
        s_apfLoadColorCoeff[iLightingType](&r);
        r.t5 = (u_int)rL.status.lAS;                        // g3dSetLightStatus
        s_apfCalcVertexColorDirectionalLight[iLightingType](vWork, &r);

        for (c = 0; c < 4; c++) vDest[c] += vWork[c];
    }

    if (rL.status.lAS & 0x70)
    {
        s_apfLoadColor[iLightingType](&r, &rLO.Color.point);
        _Vu0SetupPositionalLights(&r, vPos, rL.point.avPosition,
                                  rL.point.vMaxRange, rL.point.vMax_Min_InverseRange);
        s_apfLoadColorCoeff[iLightingType](&r);
        r.t5 = (u_int)rL.status.lAS;                        // g3dSetLightStatus
        s_apfCalcVertexColorPointLight[iLightingType](vWork, &r);

        for (c = 0; c < 4; c++) vDest[c] += vWork[c];
    }

    if (rL.status.lAS & 0x700)
    {
        s_apfLoadColor[iLightingType](&r, &rLO.Color.spot);
        _Vu0SetupPositionalLights(&r, vPos, rL.spot.avPosition,
                                  rL.spot.vMaxRange, rL.spot.vMax_Min_InverseRange);
        _Vu0CalcSpotCone(&r, rL.spot.avDirection, rL.spot.vCosOutside,
                         rL.spot.vCosIn_Out_Inverse);
        s_apfLoadColorCoeff[iLightingType](&r);
        r.t5 = (u_int)rL.status.lAS;                        // g3dSetLightStatus
        s_apfCalcVertexColorSpotLight[iLightingType](vWork, &r);

        for (c = 0; c < 4; c++) vDest[c] += vWork[c];
    }

    for (c = 0; c < 3; c++)
    {
        vDest[c] += rLO.Calc.vAmbient[c];
    }

    for (c = 0; c < 4; c++)
    {
        vDest[c] = _Clamp01(vDest[c]);                      // vmaxx 0 / vminibcx 1.0 (sdata 0x370a30)
    }
}

/* --------------------------------------------------------------------------
 *  GS register forwarding: tail jumps into gs_reg.c's cache.
 * ------------------------------------------------------------------------ */

// FF3 0x001cc9b0
int g3dSetGsRegister(u_long data, int reg, int mode)
{
    return SetGsRegCached(data, reg, mode);
}

// FF3 0x001cc9c8
int g3dSetGsRegisters(const GS_REG_SET *regs, int num, int mode)
{
    return SetGsRegsCached(regs, num, mode);
}

// FF3 0x001cc9e0
u_long *g3dGetGsRegisterRef(int reg)
{
    return GetGsRegCached(reg);
}

/* --------------------------------------------------------------------------
 *  g3dCalcGsPrimitiveCoord                                      FF3 0x001cc9f8
 *
 *  Screen coordinate -> GS XYZ: xy to 12.4 (vftoi4) plus XYOFFSET_1's OFX /
 *  OFY (16-bit adds), z = (int)(z * -65535 + 65535) (sdata 0x370a34/38).
 * ------------------------------------------------------------------------ */
void g3dCalcGsPrimitiveCoord(sceGsXyz *pGsXyz, const float *vScreenCoord)
{
    u_long *plXyoffset = &GetGsRegCache()[SCE_GS_XYOFFSET_1];
    int     iv[4];

    _ftoi4(iv, vScreenCoord);

    pGsXyz->X = (u_short)((u_short)*plXyoffset + (u_short)iv[0]);
    pGsXyz->Y = (u_short)((u_short)(*plXyoffset >> 32) + (u_short)iv[1]);
    pGsXyz->Z = (u_int)(int)(vScreenCoord[2] * -65535.0f + 65535.0f);
}

/* --------------------------------------------------------------------------
 *  g3dCalcScreenCoord                                           FF3 0x001cca88
 *
 *  The inverse: subtract OFX / OFY, then vitof4 (divide by 16) all four
 *  lanes, z included. The ROM leaves the w lane's integer uninitialised;
 *  0 here.
 * ------------------------------------------------------------------------ */
void g3dCalcScreenCoord(float *vScreenCoord, const sceGsXyz *pGsXyz)
{
    u_long *plXyoffset = &GetGsRegCache()[SCE_GS_XYOFFSET_1];
    int     iv[4];
    int     c;

    iv[0] = (int)((u_int)pGsXyz->X - (u_int)(u_short)*plXyoffset);
    iv[1] = (int)((u_int)pGsXyz->Y - (u_int)(u_short)(*plXyoffset >> 32));
    iv[2] = (int)(u_int)pGsXyz->Z;
    iv[3] = 0;

    for (c = 0; c < 4; c++)
    {
        vScreenCoord[c] = (float)iv[c] / 16.0f;
    }
}

/* ==========================================================================
 *  The lighting kernels (instruction-identical to FF2's).
 *
 *  Register map: vf0 (0,0,0,1); vf3/vf4/vf5 material diffuse / ambient /
 *  specular (vf5.w = fPower); vf7 unit normal; vf8..vf10 unit light->vertex;
 *  vf11 unit camera->vertex; vf15/vf16 attenuation / cone; vf17/vf18 N.L /
 *  N.H; vf25..27, vf28..30, vf31/21/22 per-light ambient / diffuse /
 *  specular; t0/t1/t3/t4 skip masks; t5 the light status.
 * ======================================================================== */

static void _Vu0MulColorXYZ(float *vDst, const float *vSrc, const float *vMaterial)
{
    vDst[0] = vSrc[0] * vMaterial[0];
    vDst[1] = vSrc[1] * vMaterial[1];
    vDst[2] = vSrc[2] * vMaterial[2];
    vDst[3] = vSrc[3];
}

// linkonce 0x2ba590: `jr ra` -- constant shading uses no material.
static void _Vu0LoadMaterialConstant(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat)
{
    (void)r;
    (void)pMat;
}

// linkonce 0x2ba598
static void _Vu0LoadMaterialLambert(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat)
{
    _CopyVector(r->vf3, pMat->vDiffuse);
    _CopyVector(r->vf4, pMat->vAmbient);
}

// linkonce 0x2ba5a8. fPower rides in vf5.w; no later use reads it.
static void _Vu0LoadMaterialPhong(G3DVU0MATERIALREGS *r, const G3DMATERIAL *pMat)
{
    _CopyVector(r->vf3, pMat->vDiffuse);
    _CopyVector(r->vf4, pMat->vAmbient);
    _CopyVector(r->vf5, pMat->vSpecular);
    r->vf5[3] = pMat->fPower;
}

// linkonce 0x2ba5c0: flat shading takes the lights' raw diffuse colour into
// the group's single array (no material multiply).
static void _Vu0CalcColorDataConstant(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL)
{
    int i;

    (void)r;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _CopyVector(pCD->avAmbient[i], pL->avDiffuse[i]);
    }
}

// linkonce 0x2ba5e0
static void _Vu0CalcColorDataLambert(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _Vu0MulColorXYZ(pCD->avAmbient[i], pL->avAmbient[i], r->vf4);
        _Vu0MulColorXYZ(pCD->avDiffuse[i], pL->avDiffuse[i], r->vf3);
    }
}

// linkonce 0x2ba630
static void _Vu0CalcColorDataPhong(const G3DVU0MATERIALREGS *r, G3DVU1COLOR_GROUP *pCD, const G3DVU1COLOR_GROUP *pL)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _Vu0MulColorXYZ(pCD->avAmbient[i],  pL->avAmbient[i],  r->vf4);
        _Vu0MulColorXYZ(pCD->avDiffuse[i],  pL->avDiffuse[i],  r->vf3);
        _Vu0MulColorXYZ(pCD->avSpecular[i], pL->avSpecular[i], r->vf5);
    }
}

// linkonce 0x2ba6a8
static void _Vu0LoadColorDataConstant(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _CopyVector(r->avAmbient[i], pCD->avAmbient[i]);
    }
}

// linkonce 0x2ba6b8
static void _Vu0LoadColorDataLambert(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _CopyVector(r->avAmbient[i], pCD->avAmbient[i]);
        _CopyVector(r->avDiffuse[i], pCD->avDiffuse[i]);
    }
}

// linkonce 0x2ba6d8
static void _Vu0LoadColorDataPhong(G3DVU0LIGHTREGS *r, const G3DVU1COLOR_GROUP *pCD)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        _CopyVector(r->avAmbient[i],  pCD->avAmbient[i]);
        _CopyVector(r->avDiffuse[i],  pCD->avDiffuse[i]);
        _CopyVector(r->avSpecular[i], pCD->avSpecular[i]);
    }
}

// linkonce 0x2ba700: `jr ra` -- constant shading has no dot products.
static void _Vu0LoadColorCoeffConstant(G3DVU0LIGHTREGS *r)
{
    (void)r;
}

// linkonce 0x2ba708: vf17 = -(light->vertex . N) = N.L, mask in t3; no clamp.
static void _Vu0LoadColorCoeffLambert(G3DVU0LIGHTREGS *r)
{
    int i;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        r->vf17[i] = -_Dot3(r->avf8_10[i], r->vf7);
    }
    r->vf17[3] = 0.0f;

    r->t3 = _Vu0FlagsLEZ(r->vf17);
}

// linkonce 0x2ba770: the lambert term, then the Blinn term vf18 = -N .
// normalize(eye + light->vertex), mask in t4 before the high clamp.
// ROM bug kept: the third half-vector is built from vf9 (light 1), not vf10.
static void _Vu0LoadColorCoeffPhong(G3DVU0LIGHTREGS *r)
{
    int i;

    _Vu0LoadColorCoeffLambert(r);

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        sceVu0FVECTOR vHalf;
        int           iDir = (i == 2) ? 1 : i;
        int           c;

        for (c = 0; c < 3; c++)
        {
            vHalf[c] = r->vf11[c] + r->avf8_10[iDir][c];
        }
        vHalf[3] = 0.0f;
        _NormalizeKeepW(vHalf, vHalf);

        r->vf18[i] = -_Dot3(r->vf7, vHalf);
    }
    r->vf18[3] = 0.0f;

    r->t4 = _Vu0FlagsLEZ(r->vf18);
    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        if (r->vf18[i] > 1.0f) r->vf18[i] = 1.0f;
    }
}

// Status bit of each light of a group in lAS, and the flag pair each light
// index tests (x is the high bit of each nibble).
enum _VU0LIGHTGROUP
{
    _VU0GROUP_DIRECTIONAL = 0,
    _VU0GROUP_POINT       = 1,
    _VU0GROUP_SPOT        = 2,
};

static const u_int s_auiEnableBit[3][G3D_MAX_LIGHT_PER_TYPE] =
{
    { 0x001, 0x002, 0x004 },
    { 0x010, 0x020, 0x040 },
    { 0x100, 0x200, 0x400 },
};
static const u_int s_auiCompMask[G3D_MAX_LIGHT_PER_TYPE] = { 0x88, 0x44, 0x22 };

// The nine accumulators are one three-times-unrolled loop: per enabled light
// (and, positional, in range; spot, inside the cone) ambient + diffuse * N.L
// (unless N.L <= 0) + specular * N.H (unless N.H <= 0), times attenuation and
// cone, each light capped at 1, the sum capped at 1. The ROM stores vf19.w,
// which no kernel writes (a stale register); 0 here.
static void _Vu0AccumulateLightGroup(float *vDestColor, const G3DVU0LIGHTREGS *r,
                                     int iGroup, int bDiffuse, int bSpecular)
{
    sceVu0FVECTOR vSum = { 0.0f, 0.0f, 0.0f, 0.0f };
    int           i;
    int           c;

    for (i = 0; i < G3D_MAX_LIGHT_PER_TYPE; i++)
    {
        float vLight[3];
        u_int uiMask = s_auiCompMask[i];

        if ((r->t5 & s_auiEnableBit[iGroup][i]) == 0)             continue;
        if (iGroup != _VU0GROUP_DIRECTIONAL && (r->t0 & uiMask))  continue;
        if (iGroup == _VU0GROUP_SPOT        && (r->t1 & uiMask))  continue;

        for (c = 0; c < 3; c++) vLight[c] = r->avAmbient[i][c];

        if (bDiffuse && (r->t3 & uiMask) == 0)
        {
            for (c = 0; c < 3; c++) vLight[c] += r->avDiffuse[i][c] * r->vf17[i];
        }
        if (bSpecular && (r->t4 & uiMask) == 0)
        {
            for (c = 0; c < 3; c++) vLight[c] += r->avSpecular[i][c] * r->vf18[i];
        }

        if (iGroup != _VU0GROUP_DIRECTIONAL)
        {
            for (c = 0; c < 3; c++) vLight[c] *= r->vf15[i];
        }
        if (iGroup == _VU0GROUP_SPOT)
        {
            for (c = 0; c < 3; c++) vLight[c] *= r->vf16[i];
        }

        for (c = 0; c < 3; c++)
        {
            if (vLight[c] > 1.0f) vLight[c] = 1.0f;
            vSum[c] += vLight[c];
        }
    }

    for (c = 0; c < 3; c++)
    {
        if (vSum[c] > 1.0f) vSum[c] = 1.0f;
    }

    _CopyVector(vDestColor, vSum);
}

// linkonce 0x2ba908
static void _Vu0CalcVertexColorDirectinalLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_DIRECTIONAL, 0, 0);
}

// linkonce 0x2ba9c0
static void _Vu0CalcVertexColorDirectinalLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_DIRECTIONAL, 1, 0);
}

// linkonce 0x2baab8
static void _Vu0CalcVertexColorDirectinalLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_DIRECTIONAL, 1, 1);
}

// FF3 0x001ccaf8
static void _Vu0CalcVertexColorPointLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_POINT, 0, 0);
}

// FF3 0x001ccc00
static void _Vu0CalcVertexColorPointLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_POINT, 1, 0);
}

// FF3 0x001ccd48
static void _Vu0CalcVertexColorPointLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_POINT, 1, 1);
}

// linkonce 0x2babe8
static void _Vu0CalcVertexColorSpotLightConstant(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_SPOT, 0, 0);
}

// linkonce 0x2bad60
static void _Vu0CalcVertexColorSpotLightLambert(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_SPOT, 1, 0);
}

// linkonce 0x2baf18
static void _Vu0CalcVertexColorSpotLightPhong(float *vDestColor, const G3DVU0LIGHTREGS *r)
{
    _Vu0AccumulateLightGroup(vDestColor, r, _VU0GROUP_SPOT, 1, 1);
}
