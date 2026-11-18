/* ==========================================================================
 *  graphics/graph3d/g3dCore.h
 *
 *  The g3d core (FF2 g3dCore.c): the one core object (render/global state,
 *  nine light slots, ambient, material, five transforms, bound texture and
 *  the VU1 memory image it keeps in sync), the CPU-side vertex lighting
 *  (g3dCalcVertexColor), GS register forwarding, and the two function-pointer
 *  holders the rest of the engine calls through (IG3DCompatible, CVu0Matrix).
 *  FF3 extent 0x1cb890..0x1cced0, plus 18 lighting kernels in the linkonce
 *  run 0x2ba590..0x2bb108 (see g3dCore.c).
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_G3DCORE_H
#define _GRAPHICS_GRAPH3D_G3DCORE_H

#include <stddef.h>                     // offsetof

#include <libvu0.h>                     // sceVu0FVECTOR

#include "sdk/scetypes.h"
#include "sdk/sce_gs.h"                 // sceGsXyz
#include "graphics/gs_reg.h"            // GS_REG_SET
#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT, _LIGHTDATA, G3DVU1LIGHTSTATUS

// The core's transform slots (FF2 names; gra3d adds GRA3DTS_WORLDSCREEN = 5,
// which it keeps itself).
enum G3DTRANSFORMSTATETYPE
{
    G3DTS_VIEW       = 0,
    G3DTS_PROJECTION = 1,
    G3DTS_WORLD      = 2,
    G3DTS_WORLD1     = 3,
    G3DTS_WORLDCLIP  = 4,
    NUM_G3DTRANSFORMSTATETYPE = 5,
};

// auiRenderState[] slots (FF2 names; g3dInitialize sets 0..2 to 1, 3..6 to 0).
enum G3DRENDERSTATETYPE
{
    G3DRS_LIGHTING                = 0,
    G3DRS_COLORVERTEX             = 1,
    G3DRS_SPECULARENABLE          = 2,
    G3DRS_DIFFUSEMATERIALSOURCE   = 3,
    G3DRS_SPECULARMATERIALSOURCE  = 4,
    G3DRS_AMBIENTMATERIALSOURCE   = 5,
    G3DRS_EMISSIVEMATERIALSOURCE  = 6,
    G3DRS_FOGCOLOR                = 7,
    NUM_G3DRENDERSTATETYPE        = 8,
};

// auiGlobalState[] slots. LIGHTINGTYPE indexes the lighting kernel tables
// (data 0x359c80..0x359ce0) in _g3dCalcColor and g3dCalcVertexColor.
enum G3DGLOBALSTATETYPE
{
    G3DGS_LIGHTATTENUATIONTYPE = 0,
    G3DGS_LIGHTINGTYPE         = 1,
    NUM_G3DGLOBALSTATETYPE     = 2,
};

enum G3DLIGHTINGTYPE
{
    G3DLT_CONSTANT      = 0,
    G3DLT_LAMBERT       = 1,
    G3DLT_PHONG         = 2,
    NUM_G3DLIGHTINGTYPE = 3,
};

// The core's current material (FF2 layout; FF3 g3dSetMaterial copies 5
// quadwords, gra3dSetMaterial fills 0x00..0x40 and gra3dCalcVu1MaterialData*
// read the colours).
struct G3DMATERIAL                      /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR vDiffuse;          // w: alpha
    /* 0x10 */ sceVu0FVECTOR vAmbient;
    /* 0x20 */ sceVu0FVECTOR vSpecular;
    /* 0x30 */ sceVu0FVECTOR vEmissive;
    /* 0x40 */ float         fPower;
    /* 0x44 */ int           aiPad[3];
};
static_assert(sizeof(G3DMATERIAL) == 0x50, "G3DMATERIAL is 0x50 bytes");
static_assert(offsetof(G3DMATERIAL, vAmbient) == 0x10, "G3DMATERIAL.vAmbient");
static_assert(offsetof(G3DMATERIAL, vSpecular) == 0x20, "G3DMATERIAL.vSpecular");
static_assert(offsetof(G3DMATERIAL, vEmissive) == 0x30, "G3DMATERIAL.vEmissive");
static_assert(offsetof(G3DMATERIAL, fPower) == 0x40, "G3DMATERIAL.fPower");

// ---- the core's VU1 memory image (FF2 G3DVU1MEMLAYOUT_PACKED) -------------
// FF3 keeps FF2's layout: every offset below was checked against FF3's
// accesses relative to G3DCOREOBJECT.Vu1Mem (+0x690). FF2's second view of
// the same bytes (G3DVU1MEMLAYOUT_DIRECT, flat arrays) is not recreated.

struct G3DVU1DIRECTIONALLIGHT           /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR avDirection[3];    // normalised (_ApplyLightDirectional)
};
static_assert(sizeof(G3DVU1DIRECTIONALLIGHT) == 0x30, "G3DVU1DIRECTIONALLIGHT is 0x30 bytes");

struct G3DVU1POINTLIGHT                 /* 0x50 */
{
    /* 0x00 */ sceVu0FVECTOR avPosition[3];
    /* 0x30 */ sceVu0FVECTOR vMaxRange;             // x/y/z: lights 0..2
    /* 0x40 */ sceVu0FVECTOR vMax_Min_InverseRange; // 1 / (max - min)
};
static_assert(sizeof(G3DVU1POINTLIGHT) == 0x50, "G3DVU1POINTLIGHT is 0x50 bytes");
static_assert(offsetof(G3DVU1POINTLIGHT, vMaxRange) == 0x30, "G3DVU1POINTLIGHT.vMaxRange");
static_assert(offsetof(G3DVU1POINTLIGHT, vMax_Min_InverseRange) == 0x40, "G3DVU1POINTLIGHT.vMax_Min_InverseRange");

struct G3DVU1SPOTLIGHT                  /* 0xa0 */
{
    /* 0x00 */ sceVu0FVECTOR avPosition[3];
    /* 0x30 */ sceVu0FVECTOR avDirection[3];
    /* 0x60 */ sceVu0FVECTOR vMaxRange;
    /* 0x70 */ sceVu0FVECTOR vMax_Min_InverseRange;
    /* 0x80 */ sceVu0FVECTOR vCosOutside;
    /* 0x90 */ sceVu0FVECTOR vCosIn_Out_Inverse;    // 1 / (cos in - cos out)
};
static_assert(sizeof(G3DVU1SPOTLIGHT) == 0xa0, "G3DVU1SPOTLIGHT is 0xa0 bytes");
static_assert(offsetof(G3DVU1SPOTLIGHT, avDirection) == 0x30, "G3DVU1SPOTLIGHT.avDirection");
static_assert(offsetof(G3DVU1SPOTLIGHT, vMaxRange) == 0x60, "G3DVU1SPOTLIGHT.vMaxRange");
static_assert(offsetof(G3DVU1SPOTLIGHT, vMax_Min_InverseRange) == 0x70, "G3DVU1SPOTLIGHT.vMax_Min_InverseRange");
static_assert(offsetof(G3DVU1SPOTLIGHT, vCosOutside) == 0x80, "G3DVU1SPOTLIGHT.vCosOutside");
static_assert(offsetof(G3DVU1SPOTLIGHT, vCosIn_Out_Inverse) == 0x90, "G3DVU1SPOTLIGHT.vCosIn_Out_Inverse");

// Per-light colours of one light type. The lighting model decides how much of
// it is used: constant only the first array, lambert two, phong all three
// (FF2's G3DVU1COLOR_CONSTANT / _LAMBERT / _PHONG are its prefixes).
struct G3DVU1COLOR_GROUP                /* 0x90 */
{
    /* 0x00 */ sceVu0FVECTOR avAmbient[3];
    /* 0x30 */ sceVu0FVECTOR avDiffuse[3];
    /* 0x60 */ sceVu0FVECTOR avSpecular[3];
};
static_assert(sizeof(G3DVU1COLOR_GROUP) == 0x90, "G3DVU1COLOR_GROUP is 0x90 bytes");
static_assert(offsetof(G3DVU1COLOR_GROUP, avDiffuse) == 0x30, "G3DVU1COLOR_GROUP.avDiffuse");
static_assert(offsetof(G3DVU1COLOR_GROUP, avSpecular) == 0x60, "G3DVU1COLOR_GROUP.avSpecular");

struct G3DVU1COLOR                      /* 0x1b0 */
{
    /* 0x000 */ G3DVU1COLOR_GROUP dir;
    /* 0x090 */ G3DVU1COLOR_GROUP point;
    /* 0x120 */ G3DVU1COLOR_GROUP spot;
};
static_assert(sizeof(G3DVU1COLOR) == 0x1b0, "G3DVU1COLOR is 0x1b0 bytes");
static_assert(offsetof(G3DVU1COLOR, point) == 0x90, "G3DVU1COLOR.point");
static_assert(offsetof(G3DVU1COLOR, spot) == 0x120, "G3DVU1COLOR.spot");

struct G3DVU1CALCULATED                 /* 0x20 */
{
    /* 0x00 */ sceVu0FVECTOR vAmbient;  // global ambient * material ambient + emissive
    /* 0x10 */ sceVu0FVECTOR vMisc;     // x: material diffuse alpha
};
static_assert(sizeof(G3DVU1CALCULATED) == 0x20, "G3DVU1CALCULATED is 0x20 bytes");
static_assert(offsetof(G3DVU1CALCULATED, vMisc) == 0x10, "G3DVU1CALCULATED.vMisc");

struct G3DVU1CONSTANT                   /* 0x20 */
{
    /* 0x00 */ sceVu0FVECTOR v0001;
    /* 0x10 */ sceVu0FVECTOR v1111;
};
static_assert(sizeof(G3DVU1CONSTANT) == 0x20, "G3DVU1CONSTANT is 0x20 bytes");

// The fog block of a VU1 memory image (g3dCalcVu1Fog).
struct G3DVU1FOG                        /* 0x10 */
{
    /* 0x0 */ float fMin;
    /* 0x4 */ float fMax;
    /* 0x8 */ float FA;
    /* 0xc */ float FB;
};
static_assert(sizeof(G3DVU1FOG) == 0x10, "G3DVU1FOG is 0x10 bytes");
static_assert(offsetof(G3DVU1FOG, FB) == 0xc, "G3DVU1FOG.FB");

struct G3DVU1GLOBAL                     /* 0x60 */
{
    /* 0x00 */ sceVu0FVECTOR vAmbient;      // g3dSetAmbient's copy
    /* 0x10 */ sceVu0FVECTOR vTemp;
    /* 0x20 */ sceVu0IVECTOR ivFogColor;
    /* 0x30 */ G3DVU1FOG     Fog;
    /* 0x40 */ qword         gtPrimitve;
    /* 0x50 */ qword         gtVertexBuffer;
};
static_assert(sizeof(G3DVU1GLOBAL) == 0x60, "G3DVU1GLOBAL is 0x60 bytes");
static_assert(offsetof(G3DVU1GLOBAL, ivFogColor) == 0x20, "G3DVU1GLOBAL.ivFogColor");
static_assert(offsetof(G3DVU1GLOBAL, Fog) == 0x30, "G3DVU1GLOBAL.Fog");
static_assert(offsetof(G3DVU1GLOBAL, gtVertexBuffer) == 0x50, "G3DVU1GLOBAL.gtVertexBuffer");

struct G3DVU1TRANSFORM                  /* 0x160 */
{
    /* 0x000 */ sceVu0FMATRIX matLocalWorld;    // G3DTS_WORLD
    /* 0x040 */ sceVu0FMATRIX matLocalWorld1;   // G3DTS_WORLD1
    /* 0x080 */ sceVu0FMATRIX matCamera;        // G3DTS_VIEW
    /* 0x0c0 */ sceVu0FMATRIX matWorldScreen;   // projection * view
    /* 0x100 */ sceVu0FMATRIX matWorldClip;     // G3DTS_WORLDCLIP
    /* 0x140 */ sceVu0FVECTOR vVertex;
    /* 0x150 */ sceVu0FVECTOR vNormal;
};
static_assert(sizeof(G3DVU1TRANSFORM) == 0x160, "G3DVU1TRANSFORM is 0x160 bytes");
static_assert(offsetof(G3DVU1TRANSFORM, matLocalWorld1) == 0x40, "G3DVU1TRANSFORM.matLocalWorld1");
static_assert(offsetof(G3DVU1TRANSFORM, matCamera) == 0x80, "G3DVU1TRANSFORM.matCamera");
static_assert(offsetof(G3DVU1TRANSFORM, matWorldScreen) == 0xc0, "G3DVU1TRANSFORM.matWorldScreen");
static_assert(offsetof(G3DVU1TRANSFORM, matWorldClip) == 0x100, "G3DVU1TRANSFORM.matWorldClip");

struct G3DVU1LIGHT                      /* 0x130 */
{
    /* 0x000 */ G3DVU1LIGHTSTATUS      status;
    /* 0x010 */ G3DVU1DIRECTIONALLIGHT dir;
    /* 0x040 */ G3DVU1POINTLIGHT       point;
    /* 0x090 */ G3DVU1SPOTLIGHT        spot;
};
static_assert(sizeof(G3DVU1LIGHT) == 0x130, "G3DVU1LIGHT is 0x130 bytes");
static_assert(offsetof(G3DVU1LIGHT, dir) == 0x10, "G3DVU1LIGHT.dir");
static_assert(offsetof(G3DVU1LIGHT, point) == 0x40, "G3DVU1LIGHT.point");
static_assert(offsetof(G3DVU1LIGHT, spot) == 0x90, "G3DVU1LIGHT.spot");

struct G3DVU1MEMLAYOUT                  /* 0x6e0 */
{
    /* 0x000 */ G3DVU1CONSTANT   Constant;
    /* 0x020 */ G3DVU1TRANSFORM  Transform;
    /* 0x180 */ G3DVU1COLOR      Color;         // ColorOrigin * material (_g3dCalcColor)
    /* 0x330 */ G3DVU1LIGHT      Light;
    /* 0x460 */ G3DMATERIAL      Material;
    /* 0x4b0 */ G3DVU1GLOBAL     Global;
    /* 0x510 */ G3DVU1CALCULATED Calc;
    /* 0x530 */ G3DVU1COLOR      ColorOrigin;   // the lights' own colours (_ApplyLight*)
};
static_assert(sizeof(G3DVU1MEMLAYOUT) == 0x6e0, "G3DVU1MEMLAYOUT is 0x6e0 bytes");
static_assert(offsetof(G3DVU1MEMLAYOUT, Transform) == 0x20, "G3DVU1MEMLAYOUT.Transform");
static_assert(offsetof(G3DVU1MEMLAYOUT, Color) == 0x180, "G3DVU1MEMLAYOUT.Color");
static_assert(offsetof(G3DVU1MEMLAYOUT, Light) == 0x330, "G3DVU1MEMLAYOUT.Light");
static_assert(offsetof(G3DVU1MEMLAYOUT, Material) == 0x460, "G3DVU1MEMLAYOUT.Material");
static_assert(offsetof(G3DVU1MEMLAYOUT, Global) == 0x4b0, "G3DVU1MEMLAYOUT.Global");
static_assert(offsetof(G3DVU1MEMLAYOUT, Calc) == 0x510, "G3DVU1MEMLAYOUT.Calc");
static_assert(offsetof(G3DVU1MEMLAYOUT, ColorOrigin) == 0x530, "G3DVU1MEMLAYOUT.ColorOrigin");

struct G3DVIEWPORT                      /* 0x18, FF2, unverified */
{
    /* 0x00 */ float fX;
    /* 0x04 */ float fY;
    /* 0x08 */ float fWidth;
    /* 0x0c */ float fHeight;
    /* 0x10 */ float fMinZ;
    /* 0x14 */ float fMaxZ;
};
static_assert(sizeof(G3DVIEWPORT) == 0x18, "G3DVIEWPORT is 0x18 bytes");

struct G3DWINDOW                        /* 0x10, FF2, unverified */
{
    /* 0x0 */ float fAspectX;
    /* 0x4 */ float fAspectY;
    /* 0x8 */ float fCenterX;
    /* 0xc */ float fCenterY;
};
static_assert(sizeof(G3DWINDOW) == 0x10, "G3DWINDOW is 0x10 bytes");

class CTexture;                         // graphics/graph3d/g3dTexture.h

// The core's bound resources (g3dSetTexture writes apTexture[0] at core
// +0x680). layout: host -- on the EE three pointers (0xc) padded to 0x10 by
// Vu1Mem's alignment; the host keeps 0x10 by holding the texture as a host
// pointer and the two buffer pointers (FF2; no FF3 access seen) as EE words.
struct G3DRESOURCE                      /* 0x10 here, EE 0xc */
{
    /* 0x0 */ CTexture *apTexture[1];
    /* 0x8 */ u_int     pVertexBuffer;  // EE IG3DVertexBuffer *, FF2, unverified
    /* 0xc */ u_int     pIndexBuffer;   // EE IG3DIndexBuffer *, FF2, unverified
};
static_assert(sizeof(G3DRESOURCE) == 0x10, "G3DRESOURCE fills 0x680..0x690");
static_assert(offsetof(G3DRESOURCE, pVertexBuffer) == 0x8, "G3DRESOURCE.pVertexBuffer");

// The core object. FF3: 0xd70 bytes (FF2 0x1170: FF3 dropped FF2's GS
// register file at +0xd70, GS registers live in gs_reg.c's cache).
// _ModifyScratchpad moves it between its static home and scratchpad
// 0x700005e0 with memcpy 0xd70. Offsets checked against g3dInitialize,
// g3dSetAmbient/Material/Transform/Light/Texture, _ApplyLight*, _g3dCalcColor
// and g3dCalcVertexColor; Viewport and Window are FF2's, unverified.
struct G3DCOREOBJECT                    /* 0xd70 */
{
    /* 0x000 */ u_int           auiRenderState[NUM_G3DRENDERSTATETYPE];
    /* 0x020 */ u_int           auiGlobalState[NUM_G3DGLOBALSTATETYPE];
    /* 0x030 */ _LIGHTDATA      aLightData[NUM_G3DLIGHTINDEX];
    /* 0x4b0 */ sceVu0FVECTOR   vAmbient;
    /* 0x4c0 */ G3DMATERIAL     Material;
    /* 0x510 */ sceVu0FMATRIX   amatTransform[NUM_G3DTRANSFORMSTATETYPE];
    /* 0x650 */ G3DVIEWPORT     Viewport;
    /* 0x668 */ u_int           _pad668[2];
    /* 0x670 */ G3DWINDOW       Window;
    /* 0x680 */ G3DRESOURCE     Resource;
    /* 0x690 */ G3DVU1MEMLAYOUT Vu1Mem;
};
static_assert(sizeof(G3DCOREOBJECT) == 0xd70, "G3DCOREOBJECT is 0xd70 bytes in FF3");
static_assert(offsetof(G3DCOREOBJECT, auiGlobalState) == 0x20, "G3DCOREOBJECT.auiGlobalState");
static_assert(offsetof(G3DCOREOBJECT, aLightData) == 0x30, "G3DCOREOBJECT.aLightData");
static_assert(offsetof(G3DCOREOBJECT, vAmbient) == 0x4b0, "G3DCOREOBJECT.vAmbient");
static_assert(offsetof(G3DCOREOBJECT, Material) == 0x4c0, "G3DCOREOBJECT.Material");
static_assert(offsetof(G3DCOREOBJECT, amatTransform) == 0x510, "G3DCOREOBJECT.amatTransform");
static_assert(offsetof(G3DCOREOBJECT, Viewport) == 0x650, "G3DCOREOBJECT.Viewport");
static_assert(offsetof(G3DCOREOBJECT, Window) == 0x670, "G3DCOREOBJECT.Window");
static_assert(offsetof(G3DCOREOBJECT, Resource) == 0x680, "G3DCOREOBJECT.Resource");
static_assert(offsetof(G3DCOREOBJECT, Vu1Mem) == 0x690, "G3DCOREOBJECT.Vu1Mem");

// g3dInitialize's argument. layout: host -- a pointer (4 bytes on the EE).
struct G3DCREATIONDATA                  /* EE 0x4 */
{
    G3DCOREOBJECT *pObj;
};

typedef int     (*LPFUNC_SETGSREGISTER)(u_long data, int reg, int mode);
typedef int     (*LPFUNC_SETGSREGISTERS)(const GS_REG_SET *regs, int num, int mode);
typedef u_long *(*LPFUNC_GETGSREGISTERREF)(int reg);
typedef int     (*LPFUNC_SETTRANSFORM)(int state, float (*mat)[4]);
typedef float (*(*LPFUNC_GETTRANSFORMREF)(int state))[4];

// GS register / transform hooks (FF2 g3dRenderTarget.h's IG3DCompatible).
// Defaults are the g3d functions below (sdata 0x370a3c..0x370a4c); gra3dInit
// points them at gra3d's own.
// layout: host -- no instance data, only static function pointers.
struct IG3DCompatible
{
    static LPFUNC_SETGSREGISTER    s_pFuncSetGsRegister;     // sdata 0x370a3c
    static LPFUNC_SETGSREGISTERS   s_pFuncSetGsRegisters;    // sdata 0x370a40
    static LPFUNC_GETGSREGISTERREF s_pFuncGetGsRegisterRef;  // sdata 0x370a44
    static LPFUNC_SETTRANSFORM     s_pFuncSetTransform;      // sdata 0x370a48
    static LPFUNC_GETTRANSFORMREF  s_pFuncGetTransformRef;   // sdata 0x370a4c
};

typedef void (*LPFUNC_VU0LOADMATRIX)(float (*mat)[4]);
typedef void (*LPFUNC_VU0APPLYMATRIXWITHOUTTRANS)(float *vDest, float *vSrc);

// The VU0 matrix registers vf4..vf7 behind a function pointer (FF2
// gra3dTypes.h's CVu0Matrix); gra3dInit installs gra3d's loaders.
// layout: host -- no instance data, only static function pointers.
struct CVu0Matrix
{
    static LPFUNC_VU0LOADMATRIX              s_pFuncLoadMatrix;              // sdata 0x370aa8
    static LPFUNC_VU0APPLYMATRIXWITHOUTTRANS s_pFuncApplyMatrixWithoutTrans; // sdata 0x370aac
};

// ---- g3dCore.c -----------------------------------------------------------
void    g3dInitialize(G3DCREATIONDATA *pCD);
int     g3dSetRenderState(int state, u_int uiValue);           // G3DRENDERSTATETYPE
u_int  *g3dGetRenderStateRef(int state);
int     g3dSetGlobalState(int state, u_int uiValue);           // G3DGLOBALSTATETYPE
u_int  *g3dGetGlobalStateRef(int state);
void    g3dSetAmbient(float *vAmbient);
float  *g3dGetAmbientRef(void);
int     g3dSetTransform(int state, float (*mat)[4]);         // G3DTRANSFORMSTATETYPE
float (*g3dGetTransformRef(int state))[4];

// The core's nine light slots: 0..2 directional, 3..5 point, 6..8 spot
// (gra3dSetValidLightId picks which gra3d lights fill them).
int          g3dLightEnable(int iLightId, int bEnable);
int          g3dIsLightEnable(int iLightId);
int          g3dSetLight(int iLightId, const G3DLIGHT *pLight);
G3DLIGHT    *g3dGetLightRef(int iLightId);
int          g3dApplyLight(void);
int          g3dSetMaterial(const G3DMATERIAL *pMaterial);
G3DMATERIAL *g3dGetMaterialRef(void);
int          g3dSetTexture(int iStage, CTexture *pTexture);
void         g3dCalcVertexColor(float *vDest, const float *vVertex, const float *vNormal, const float *vColor);

// Forward to gs_reg.c's register cache (SetGsRegCached / SetGsRegsCached /
// GetGsRegCached).
int     g3dSetGsRegister(u_long data, int reg, int mode);
int     g3dSetGsRegisters(const GS_REG_SET *regs, int num, int mode);
u_long *g3dGetGsRegisterRef(int reg);

// Screen coordinates (xy in pixels, z 0..1) <-> GS XYZ (12.4 xy biased by
// XYOFFSET_1, z = 65535 - 65535 * z).
void    g3dCalcGsPrimitiveCoord(sceGsXyz *pGsXyz, const float *vScreenCoord);
void    g3dCalcScreenCoord(float *vScreenCoord, const sceGsXyz *pGsXyz);

#endif /* _GRAPHICS_GRAPH3D_G3DCORE_H */
