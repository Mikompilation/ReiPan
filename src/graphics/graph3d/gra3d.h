/* ==========================================================================
 *  graphics/graph3d/gra3d.h
 *
 *  The gra3d layer over the g3d core (FF2 gra3d.c): the module camera, fog,
 *  the 39-slot light bank, the VU1 memory image kept in scratchpad, transform
 *  and GS register forwarding, and gra3dInit. FF3 0x1da418..0x1dd8b0.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3D_H
#define _GRAPHICS_GRAPH3D_GRA3D_H

#include <stddef.h>                     // offsetof

#include <libvu0.h>                     // sceVu0FVECTOR / sceVu0FMATRIX

#include "sdk/scetypes.h"
#include "graphics/graph3d/g3dCore.h"   // G3DVU1FOG
#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT
#include "graphics/graph3d/sgd_types.h" // SGDFILEHEADER, SGDCOORDINATE, SGDMATERIAL

// GRA3DCAMERA.type: which view->screen / view->clip builders gra3dApplyCamera
// calls (two-entry function tables at sdata 0x370bd0 / 0x370bd8).
enum G3DCAMPROJECTIONTYPE
{
    PT_PERSPECTIVE = 0,
    PT_ORTHO       = 1,
    NUM_G3DCAMPROJECTIONTYPE = 2,
};

// The camera (s_Camera, bss 0x3eb570; _InitCamera copies the default from
// data 0x328850). FF3 grew FF2's 0x1e0 layout by keeping the world->view
// matrix in the object (FF2 built it in scratchpad 0x70003900): every field
// from 0xe0 on moved down 0x40. Checked against FF3 gra3dApplyCamera,
// gra3dCalcWorldScreenMatrix, gra3dcamGet*/Set*, _ResetCameraTarget,
// _InitCamera (size); vTargetOld keeps FF2's name and slot unverified.
// No pointers: the PS2 size holds; the VU0 types give it the EE's 16-byte
// alignment.
struct GRA3DCAMERA                  /* 0x220 */
{
    /* 0x000 */ float         fFov;                     // vertical field of view, radians
    /* 0x004 */ float         fNearZ;
    /* 0x008 */ float         fFarZ;
    /* 0x00c */ float         fAspectX;
    /* 0x010 */ float         fAspectY;                 // gra3dcamSetAspect scales it in PAL
    /* 0x014 */ float         fCenterX;
    /* 0x018 */ float         fCenterY;
    /* 0x01c */ float         fZmin;
    /* 0x020 */ float         fZmax;
    /* 0x024 */ int           type;                     // G3DCAMPROJECTIONTYPE, stored as a word
    /* 0x028 */ int           aiPad[2];
    /* 0x030 */ sceVu0FVECTOR vTarget;                  // gra3dcamGetTarget
    /* 0x040 */ sceVu0FVECTOR vPositionOld;             // gra3dcamGetPositionOld
    /* 0x050 */ sceVu0FVECTOR vTargetOld;               // FF2, unverified
    /* 0x060 */ sceVu0FMATRIX matViewClipPolygon;
    /* 0x0a0 */ sceVu0FMATRIX matViewClipObject;
    /* 0x0e0 */ sceVu0FMATRIX matWorldView;             // FF3: sceVu0CameraMatrix(matCoord[3], [2], [1])
    /* 0x120 */ sceVu0FMATRIX matWorldScreen;           // view->screen * world->view
    /* 0x160 */ sceVu0FMATRIX matWorldClipPolygon;
    /* 0x1a0 */ sceVu0FMATRIX matWorldClipObject;
    /* 0x1e0 */ sceVu0FMATRIX matCoord;                 // [1] up, [2] direction, [3] position
};
static_assert(sizeof(GRA3DCAMERA) == 0x220, "GRA3DCAMERA is 0x220 bytes in FF3");
static_assert(offsetof(GRA3DCAMERA, type) == 0x024, "GRA3DCAMERA.type");
static_assert(offsetof(GRA3DCAMERA, vTarget) == 0x030, "GRA3DCAMERA.vTarget");
static_assert(offsetof(GRA3DCAMERA, vPositionOld) == 0x040, "GRA3DCAMERA.vPositionOld");
static_assert(offsetof(GRA3DCAMERA, matViewClipPolygon) == 0x060, "GRA3DCAMERA.matViewClipPolygon");
static_assert(offsetof(GRA3DCAMERA, matViewClipObject) == 0x0a0, "GRA3DCAMERA.matViewClipObject");
static_assert(offsetof(GRA3DCAMERA, matWorldView) == 0x0e0, "GRA3DCAMERA.matWorldView");
static_assert(offsetof(GRA3DCAMERA, matWorldScreen) == 0x120, "GRA3DCAMERA.matWorldScreen");
static_assert(offsetof(GRA3DCAMERA, matWorldClipPolygon) == 0x160, "GRA3DCAMERA.matWorldClipPolygon");
static_assert(offsetof(GRA3DCAMERA, matWorldClipObject) == 0x1a0, "GRA3DCAMERA.matWorldClipObject");
static_assert(offsetof(GRA3DCAMERA, matCoord) == 0x1e0, "GRA3DCAMERA.matCoord");

// The light bank's 39 slots by type. FF3's ranges (from _InitLight,
// _UpdateLight_*, gra3dGetNumLightEnable): 3 directional, 27 point, 9 spot
// (FF2: 3 / 19 / 17).
enum GRA3DLIGHTID
{
    GRA3D_START_LIGHT_DIRECTIONAL = 0,
    GRA3D_END_LIGHT_DIRECTIONAL   = 2,
    GRA3D_NUM_LIGHT_DIRECTIONAL   = 3,
    GRA3D_START_LIGHT_POINT       = 3,
    GRA3D_END_LIGHT_POINT         = 29,
    GRA3D_NUM_LIGHT_POINT         = 27,
    GRA3D_START_LIGHT_SPOT        = 30,
    GRA3D_END_LIGHT_SPOT          = 38,
    GRA3D_NUM_LIGHT_SPOT          = 9,
    NUM_GRA3DLIGHTID              = 39,
    INVALID_GRA3DLIGHTID          = 0x7fffffff,
};

// Per-slot switches. FF3: 0x20 bytes (FF2 0x10): _InitLight clears words
// 0..4, gra3dSetLightStatus copies 8 words.
struct GRA3DLIGHTSTATUS                 /* 0x20 */
{
    /* 0x00 */ int bEnable;
    /* 0x04 */ int bEnableToChar;               // gra3dEmulateLightData's candidate test
    /* 0x08 */ int bEnableToShadow;             // FF2, unverified
    /* 0x0c */ int bEmulateToDirectionalLight;  // gra3dEmulateLightDataSub's candidate test
    /* 0x10 */ int bEnableToPrelight;           // FF3: gra3dIsLightEnableToPrelight
    /* 0x14 */ int aiPad[3];
};
static_assert(sizeof(GRA3DLIGHTSTATUS) == 0x20, "GRA3DLIGHTSTATUS is 0x20 bytes in FF3");
static_assert(offsetof(GRA3DLIGHTSTATUS, bEnableToChar) == 0x04, "GRA3DLIGHTSTATUS.bEnableToChar");
static_assert(offsetof(GRA3DLIGHTSTATUS, bEmulateToDirectionalLight) == 0x0c, "GRA3DLIGHTSTATUS.bEmulateToDirectionalLight");
static_assert(offsetof(GRA3DLIGHTSTATUS, bEnableToPrelight) == 0x10, "GRA3DLIGHTSTATUS.bEnableToPrelight");

// A whole light set (ambient, 39 lights, their switches). FF3: 0x1610 bytes
// (gra3dLightDataAddOffsetPosition / utilSetGRA3DLIGHTDATADefault), aStatus at
// 0x1120, aiNumInitial at 0x1600 (gra3dGetNumLightInitial).
struct GRA3DLIGHTDATA                   /* 0x1610 */
{
    /* 0x0000 */ sceVu0FVECTOR    vAmbient;
    /* 0x0010 */ G3DLIGHT         aLight[NUM_GRA3DLIGHTID];
    /* 0x1120 */ GRA3DLIGHTSTATUS aStatus[NUM_GRA3DLIGHTID];
    /* 0x1600 */ int              aiNumInitial[3];          // per G3DLIGHTTYPE
    /* 0x160c */ int              aiPad[1];
};
static_assert(sizeof(GRA3DLIGHTDATA) == 0x1610, "GRA3DLIGHTDATA is 0x1610 bytes in FF3");
static_assert(offsetof(GRA3DLIGHTDATA, aLight) == 0x0010, "GRA3DLIGHTDATA.aLight");
static_assert(offsetof(GRA3DLIGHTDATA, aStatus) == 0x1120, "GRA3DLIGHTDATA.aStatus");
static_assert(offsetof(GRA3DLIGHTDATA, aiNumInitial) == 0x1600, "GRA3DLIGHTDATA.aiNumInitial");

// Fog range (s_Fog, bss 0x3eb790).
struct G3DFOG                           /* 0x10 */
{
    /* 0x0 */ float fMin;
    /* 0x4 */ float fMax;
    /* 0x8 */ float fNear;
    /* 0xc */ float fFar;
};
static_assert(sizeof(G3DFOG) == 0x10, "G3DFOG is 0x10 bytes");
static_assert(offsetof(G3DFOG, fFar) == 0xc, "G3DFOG.fFar");

// G3DVU1FOG (the fog block of a VU1 memory image) is in g3dCore.h.

// Transform slots: the core's (G3DTRANSFORMSTATETYPE) plus WORLDSCREEN,
// which gra3d keeps in its VU1 image.
enum GRA3DTRANSFORMSTATETYPE
{
    GRA3DTS_VIEW        = 0,
    GRA3DTS_PROJECTION  = 1,
    GRA3DTS_WORLD       = 2,
    GRA3DTS_WORLD1      = 3,
    GRA3DTS_WORLDCLIP   = 4,
    GRA3DTS_WORLDSCREEN = 5,
    NUM_GRA3DTRANSFORMSTATETYPE = 6,
};

// The VU1 memory image gra3d builds (FF2 GRA3DVU1MEMLAYOUT_DIRECT). FF3
// accesses so far: _vVF2 (gra3dSetVu1ConstVF2 / gra3dGetVu1ConstVF2), the
// four GIF tags whose FOG bit _ModifyFogParam sets (+0x30/+0x40/+0x60/+0x70),
// the EOP tag's light-type flags (gra3dVu1TransGTEOP), matWorldScreen /
// matWorldClip (gra3dSetTransform), the two local->world matrices
// (g3dSetVu1LightData) and the fog block (gra3dApplyFog); the default image
// at data 0x328400 fills the head (VF01/VF02 constants, double-buffer
// addresses, GIF tags) as FF2's.
struct GRA3DVU1MEMLAYOUT                /* 0x430 */
{
    /* 0x000 */ sceVu0FVECTOR _vVF1;
    /* 0x010 */ sceVu0FVECTOR _vVF2;                    // default (1,0,0,0); motion code swaps it per draw
    /* 0x020 */ u_int         auiDBAddress[4];          // FF2 GRA3DVU1DBADDRESS, unverified
    /* 0x030 */ u_long        gtTRISTRIP_NOTEXTURE[2];
    /* 0x040 */ u_long        gtTRISTRIP_TEXTURE[2];
    /* 0x050 */ u_int         gtEOP[4];                 // EOP GIF tag; words 1 / 2 = spot / point lights on
    /* 0x060 */ u_long        gtTRIFAN_NOTEXTURE[2];
    /* 0x070 */ u_long        gtTRIFAN_TEXTURE[2];
    /* 0x080 */ sceVu0FMATRIX matWorldScreen;
    /* 0x0c0 */ sceVu0FMATRIX matWorldClip;
    /* 0x100 */ sceVu0FMATRIX matLocalWorld;            // rotation, rows normalised (g3dSetVu1LightData)
    /* 0x140 */ sceVu0FMATRIX matLocalWorldNoNormalized; // the coordinate's raw local->world
    /* 0x180 */ G3DVU1FOG     Fog;
    /* 0x190 */ u_char        _190[0x430 - 0x190];      // FF2: the light / material blocks
};
static_assert(sizeof(GRA3DVU1MEMLAYOUT) == 0x430, "GRA3DVU1MEMLAYOUT is 0x430 bytes");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, _vVF2) == 0x010, "GRA3DVU1MEMLAYOUT._vVF2");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, gtTRISTRIP_NOTEXTURE) == 0x030, "GRA3DVU1MEMLAYOUT.gtTRISTRIP_NOTEXTURE");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, gtEOP) == 0x050, "GRA3DVU1MEMLAYOUT.gtEOP");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, gtTRIFAN_TEXTURE) == 0x070, "GRA3DVU1MEMLAYOUT.gtTRIFAN_TEXTURE");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, matWorldScreen) == 0x080, "GRA3DVU1MEMLAYOUT.matWorldScreen");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, matWorldClip) == 0x0c0, "GRA3DVU1MEMLAYOUT.matWorldClip");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, matLocalWorld) == 0x100, "GRA3DVU1MEMLAYOUT.matLocalWorld");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, matLocalWorldNoNormalized) == 0x140, "GRA3DVU1MEMLAYOUT.matLocalWorldNoNormalized");
static_assert(offsetof(GRA3DVU1MEMLAYOUT, Fog) == 0x180, "GRA3DVU1MEMLAYOUT.Fog");

// The VU1 image with its leading VIF code (scratchpad 0x70000000 or a static
// copy; _ModifyScratchpad, memcpy 0x440).
struct GRA3DSCRATCHPADLAYOUT            /* 0x440 */
{
    /* 0x000 */ u_int             qwVif1Code0[4];
    /* 0x010 */ GRA3DVU1MEMLAYOUT Vu1Mem;
};
static_assert(sizeof(GRA3DSCRATCHPADLAYOUT) == 0x440, "GRA3DSCRATCHPADLAYOUT is 0x440 bytes");
static_assert(offsetof(GRA3DSCRATCHPADLAYOUT, Vu1Mem) == 0x010, "GRA3DSCRATCHPADLAYOUT.Vu1Mem");

// Three light vectors (one row per light, or three columns of xyz).
typedef sceVu0FVECTOR LMATRIX[3];

// The per-coordinate light block g3dSetVu1LightData builds for VU1
// (FF2 layouts; FF3 offsets checked in g3dSetVu1LightData and
// _SetVu1LightData_*).
struct GRA3DVU1LIGHTDATA_DIRECTIONAL    /* 0x60 */
{
    /* 0x00 */ LMATRIX lmDiffuse;               // light directions as columns
    /* 0x30 */ LMATRIX lmSpecular;              // half vectors as columns
};
static_assert(sizeof(GRA3DVU1LIGHTDATA_DIRECTIONAL) == 0x60, "GRA3DVU1LIGHTDATA_DIRECTIONAL is 0x60 bytes");
static_assert(offsetof(GRA3DVU1LIGHTDATA_DIRECTIONAL, lmSpecular) == 0x30, "GRA3DVU1LIGHTDATA_DIRECTIONAL.lmSpecular");

struct GRA3DVU1LIGHTDATA_POINT          /* 0x40 */
{
    /* 0x00 */ LMATRIX       lmPosition;        // w: 1 / (range * intensity * diffuse sum)
    /* 0x30 */ sceVu0FVECTOR _vDirectionInverse;    // FF2, not written by FF3
};
static_assert(sizeof(GRA3DVU1LIGHTDATA_POINT) == 0x40, "GRA3DVU1LIGHTDATA_POINT is 0x40 bytes");

struct GRA3DVU1LIGHTDATA_SPOT           /* 0xb0 */
{
    /* 0x00 */ LMATRIX       lmPosition;
    /* 0x30 */ sceVu0FVECTOR vIntens;           // per light: cos^2 of the cone half-angle
    /* 0x40 */ sceVu0FVECTOR vIntensB;          // per light: 1 / (1 - vIntens)
    /* 0x50 */ LMATRIX       lmDirection;
    /* 0x80 */ LMATRIX       lmSpecular;        // columns
};
static_assert(sizeof(GRA3DVU1LIGHTDATA_SPOT) == 0xb0, "GRA3DVU1LIGHTDATA_SPOT is 0xb0 bytes");
static_assert(offsetof(GRA3DVU1LIGHTDATA_SPOT, vIntens) == 0x30, "GRA3DVU1LIGHTDATA_SPOT.vIntens");
static_assert(offsetof(GRA3DVU1LIGHTDATA_SPOT, vIntensB) == 0x40, "GRA3DVU1LIGHTDATA_SPOT.vIntensB");
static_assert(offsetof(GRA3DVU1LIGHTDATA_SPOT, lmDirection) == 0x50, "GRA3DVU1LIGHTDATA_SPOT.lmDirection");
static_assert(offsetof(GRA3DVU1LIGHTDATA_SPOT, lmSpecular) == 0x80, "GRA3DVU1LIGHTDATA_SPOT.lmSpecular");

struct GRA3DVU1LIGHTDATA                /* 0x150 */
{
    /* 0x000 */ GRA3DVU1LIGHTDATA_DIRECTIONAL dir;
    /* 0x060 */ GRA3DVU1LIGHTDATA_SPOT        spot;
    /* 0x110 */ GRA3DVU1LIGHTDATA_POINT       point;
};
static_assert(sizeof(GRA3DVU1LIGHTDATA) == 0x150, "GRA3DVU1LIGHTDATA is 0x150 bytes");
static_assert(offsetof(GRA3DVU1LIGHTDATA, spot) == 0x060, "GRA3DVU1LIGHTDATA.spot");
static_assert(offsetof(GRA3DVU1LIGHTDATA, point) == 0x110, "GRA3DVU1LIGHTDATA.point");

// The material blocks gra3dCalcVu1MaterialData* hand to VU1 (0x70 each):
// colours already folded with the material and scaled to GS units.
struct GRA3DVU1MATERIALDATA_DIRECTIONAL /* 0x70 */
{
    /* 0x00 */ sceVu0FVECTOR vAmbient;          // w 255
    /* 0x10 */ LMATRIX       lmDiffuse;         // [0].w: the material's alpha
    /* 0x40 */ LMATRIX       lmSpecular;
};
static_assert(sizeof(GRA3DVU1MATERIALDATA_DIRECTIONAL) == 0x70, "GRA3DVU1MATERIALDATA_DIRECTIONAL is 0x70 bytes");
static_assert(offsetof(GRA3DVU1MATERIALDATA_DIRECTIONAL, lmSpecular) == 0x40, "GRA3DVU1MATERIALDATA_DIRECTIONAL.lmSpecular");

// Point and spot share one layout (FF2 has two identical structs).
struct GRA3DVU1MATERIALDATA_POINT       /* 0x70 */
{
    /* 0x00 */ sceVu0FVECTOR vPower;            // per light: range (cache) / range * colour divisor (packet)
    /* 0x10 */ LMATRIX       lmDiffuse;
    /* 0x40 */ LMATRIX       lmSpecular;
};
static_assert(sizeof(GRA3DVU1MATERIALDATA_POINT) == 0x70, "GRA3DVU1MATERIALDATA_POINT is 0x70 bytes");
static_assert(offsetof(GRA3DVU1MATERIALDATA_POINT, lmDiffuse) == 0x10, "GRA3DVU1MATERIALDATA_POINT.lmDiffuse");
static_assert(offsetof(GRA3DVU1MATERIALDATA_POINT, lmSpecular) == 0x40, "GRA3DVU1MATERIALDATA_POINT.lmSpecular");
typedef GRA3DVU1MATERIALDATA_POINT GRA3DVU1MATERIALDATA_SPOT;

// A light group's colours as _SetVu1LightData_Point / _Spot collected them,
// and which gra3d light each column came from.
struct GRA3DVU1MATERIALCACHE_POINT      /* 0x80 */
{
    /* 0x00 */ GRA3DVU1MATERIALDATA_POINT Data;
    /* 0x70 */ GRA3DMATERIALINDEXCACHE    Index;
};
static_assert(sizeof(GRA3DVU1MATERIALCACHE_POINT) == 0x80, "GRA3DVU1MATERIALCACHE_POINT is 0x80 bytes");
static_assert(offsetof(GRA3DVU1MATERIALCACHE_POINT, Index) == 0x70, "GRA3DVU1MATERIALCACHE_POINT.Index");
typedef GRA3DVU1MATERIALCACHE_POINT GRA3DVU1MATERIALCACHE_SPOT;

// ---- init / scratchpad ---------------------------------------------------
void   gra3dInit(void *pPacket, int iSize);
void   gra3dUseScratchpad(int b);
void   gra3dSetDrawParam(int i, float f);  // two light-selection parameters (provisional names)
float  gra3dGetDrawParamF(void);
int    gra3dGetDrawParamI(void);

// ---- light bank ----------------------------------------------------------
void              gra3dSetLight(int iLightId, G3DLIGHT *pLight);
G3DLIGHT         *gra3dGetLightRef(int iLightId);
void              gra3dLightEnable(int iLightId, int bEnable);
int               gra3dIsLightEnable(int iLightId);
int               gra3dIsLightEnableToPrelight(int iLightId);
void              gra3dLightEnableAll(int bEnable);
void              gra3dSetLightStatus(int iLightId, GRA3DLIGHTSTATUS *pS);
GRA3DLIGHTSTATUS *gra3dGetLightStatusRef(int iLightId);
void              gra3dEnableLightType(int type, int bEnable);
void              gra3dSetAmbient(float *vAmbient);
float            *gra3dGetAmbientRef(void);
void              gra3dApplyLight(void);

void  gra3dLightPushData(void);
void  gra3dLightPopData(void);
void  gra3dSetLightData(GRA3DLIGHTDATA *pLightData, float *vTrans);
void  gra3dGetLightData(GRA3DLIGHTDATA *pLightData);
void  gra3dLightDataAddOffsetPosition(GRA3DLIGHTDATA *pDest, const GRA3DLIGHTDATA *pSrc, const float *vPosition);
void  gra3dEmulateLightDataSub(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition,
                               float fMagnification, int bChar);
void  gra3dEmulateLightData(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition, float fMagnification);
void  gra3dEmulateLightDataObj(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition, float fMagnification);
int   gra3dGetNumLightEnable(GRA3DLIGHTDATA *pLD, int iLightType);
int   gra3dGetNumLightInitial(GRA3DLIGHTDATA *pLD, int iLightType);
void  utilSetGRA3DLIGHTDATADefault(GRA3DLIGHTDATA *pLD);
int   gra3dGetNumEnableLight(int iLightType);              // G3DLIGHTTYPE
void  gra3dSetValidLightId(int iLightType, int iIndex, int iLightTypeIndex);
int   gra3dIsLightTypeEnable(int type);                    // G3DLIGHTTYPE

// ---- VU1 light / material packets ----------------------------------------
void  gra3dSetVu1ConstVF2(float *v);
void  gra3dGetVu1ConstVF2(float *v);
void  g3dSetVu1LightData(GRA3DVU1LIGHTDATA *pVu1LightData, SGDCOORDINATE *cp0, SGDCOORDINATE *cp1);
void  gra3dVu1TransGTEOP(void);
void  SetVU1Header(void);
GRA3DVU1MATERIALCACHE_POINT *g3dGetVu1MaterialCache(int type, int iIndex);   // point or spot
void  gra3dCalcVu1MaterialDataDirectional(GRA3DVU1MATERIALDATA_DIRECTIONAL *pDirectionalData);
void  gra3dCalcVu1MaterialDataPoint(GRA3DVU1MATERIALDATA_POINT *pPointData);
void  gra3dCalcVu1MaterialDataSpot(GRA3DVU1MATERIALDATA_SPOT *pSpotData);
void  gra3dRestoreVu1MaterialData(const GRA3DVU1MATERIALDATA_DIRECTIONAL *directional,
                                const GRA3DVU1MATERIALDATA_POINT *point,
                                const GRA3DVU1MATERIALDATA_SPOT *spot);
void  gra3dSetMaterial(SGDMATERIAL *pMat);
void  gra3dCalcVertexColor(float *vDest, float *vVertex, float *vNormal, float *vSrcColor);

// ---- PC: the VU1 light image for the host renderer -----------------------
// No FF3 counterpart (MioPan gra3d.c / gra3dTypes.h port additions). The PS2
// lit realtime and preset meshes in VU1 microcode from the light and material
// blocks above; the PC has no VU1, so the host renderer bridge
// (reipan_graph3d.cpp) reads the last blocks built through these and lights the
// vertices with the same kernels.

// One positional light, derived as the three-lane VU1 image derives its own,
// without the three-lane cap (gra3dCalcVu1WideLanes). vParams: x bTimes,
// y cos^2 of the cone half-angle, z 1 / (1 - y); fRank is scratch.
// layout: host -- PC only, never in EE memory.
struct GRA3DVU1LANE
{
    float vPosition[4];
    float vDirection[4];
    float vColDif[4];
    float vColSpc[4];
    float vParams[4];
    float fRank;
};

// What the VU1 kernels read while lighting a vertex, one float4 per light.
// Colours are GS 0..255 units with gra3dCalcVu1MaterialData*'s scales.
struct GRA3DVU1LIGHTSNAPSHOT            /* 0x270 */
{
    int   aiConfig[4];          // x: bit 0 spot / bit 1 point type enabled; y: 1
    float vAmbient[4];          // GLOBALAMBIENT, w 255 (the closing clamp)
    float vSpotBTimes[4];
    float vSpotIntens[4];       // cos^2 of the cone half-angle
    float vSpotIntensB[4];      // 1 / (1 - vSpotIntens)
    float vPointBTimes[4];
    float avDirLightDif[3][4];  // world-space light directions, one per light
    float avDirLightSpc[3][4];  // half vectors
    float avDirColDif[3][4];    // [0].w: the material alpha
    float avDirColSpc[3][4];
    float avSpotPos[3][4];
    float avSpotDir[3][4];
    float avSpotColDif[3][4];
    float avSpotColSpc[3][4];
    float avPointPos[3][4];
    float avPointColDif[3][4];
    float avPointColSpc[3][4];
};
static_assert(sizeof(GRA3DVU1LIGHTSNAPSHOT) == 0x270, "GRA3DVU1LIGHTSNAPSHOT is 0x270 bytes");

void  gra3dSnapshotVu1Lighting(GRA3DVU1LIGHTSNAPSHOT *out);
int   gra3dCalcVu1WideLanes(GRA3DVU1LANE *aLanes, int iMaxLanes, int type, const float *vRefPos);
void  gra3dCalcVu1VertexColor(float *vDest, const GRA3DVU1LIGHTSNAPSHOT *snap, const float *vVertex,
                              const float *vNormal, const float *vSrcColor);

// ---- bounding boxes / prelight -------------------------------------------
// PC: matClip / matTransform were the VU0 registers vf4..vf7 / vf8..vf11 the
// caller loaded; matTransform may be null (avBBTransformed is then left alone).
int   gra3dIsBBInViewvolume(float (*avBBTransformed)[4], float (*avBBClipped)[4], float (*avBBWorld)[4],
                            float (*matClip)[4], float (*matTransform)[4]);
int   CheckModelBoundingBox(float (*avBBWorld)[4]);
void  gra3dExecPrelight(SGDFILEHEADER *pSGDHead, float *vTrans, float *vRot);
void  gra3dExecPrelightMatrix(SGDFILEHEADER *pSGDHead, float (*mat)[4]);

// ---- camera --------------------------------------------------------------
GRA3DCAMERA *gra3dGetCamera(void);
float *gra3dcamGetPosition(void);
float *gra3dcamGetDirection(void);
float *gra3dcamGetTarget(void);
float *gra3dcamGetPositionOld(void);
void   gra3dcamSetPosition(float *vPos);
void   gra3dcamSetPosition(float x, float y, float z);
void   gra3dcamSetCoord(float (*mat)[4]);
void   gra3dcamSetTarget(float *vTarget, int bFixUp);
void   gra3dcamSetTarget(float x, float y, float z, int bFixUp);
void   gra3dcamSetTargetUp(float *vTarget, float *vUp);
void   gra3dcamSetRoll(float fRad);
float  gra3dcamGetRoll(void);
void   gra3dcamRotationByAxis(float *vAxis, float fAngle);
void   gra3dcamSetFov(float fFov);
float  gra3dcamGetFov(void);
void   gra3dcamSetAspect(float fX, float fY);
void   gra3dcamSetDepth(float fMinZ, float fMaxZ);
void   gra3dcamSetClip(float fNearZ, float fFarZ);
void   gra3dcamSetType(int type);           // G3DCAMPROJECTIONTYPE
float *_GetClipVolume(void);
float *_GetClipVolumeV(void);
void   gra3dCalcWorldScreenMatrix(float (*mat)[4], GRA3DCAMERA *pCam, int bFixup);
void   gra3dApplyCamera(GRA3DCAMERA *pCam, int bFixup, int bStabilize);
void   _gra3dSetCameraForce(GRA3DCAMERA *pCamera);

// ---- fog / monotone ------------------------------------------------------
void    gra3dSetFog(float fMin, float fMax, float fNear, float fFar);
void    gra3dSetFog(G3DFOG *pFog);
void    gra3dSetFogColor(int r, int g, int b);
void    gra3dApplyFog(void);
G3DFOG *gra3dGetFogRef(void);
int     gra3dIsFogEnable(void);
void    gra3dEnableFog(int b);
void    g3dCalcVu1Fog(G3DVU1FOG *pVu1Fog, G3DFOG *pFog);
float   g3dCalcFA(G3DFOG *pFog);
float   g3dCalcFB(G3DFOG *pFog);
int     gra3dIsMonotoneDrawEnable(void);
void    gra3dMonotoneDrawEnable(int bEnable);

// ---- GS registers / transforms -------------------------------------------
void   gra3dSetGsRegisterDefault(void);
// The model clip rectangle in clip space (-1..1 each; gra3dMirror.c narrows
// it to the mirror). FF3 0x001dacf0.
void   _SetClipValue(float minx, float maxx, float miny, float maxy);
int    gra3dSetTransform(int state, float (*mat)[4]);      // GRA3DTRANSFORMSTATETYPE
float (*gra3dGetTransformRef(int state))[4];

#endif /* _GRAPHICS_GRAPH3D_GRA3D_H */
