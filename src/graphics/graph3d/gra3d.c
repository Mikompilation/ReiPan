/* ==========================================================================
 *  graphics/graph3d/gra3d.c
 *
 *  The gra3d layer over the g3d core (see gra3d.h). FF3 0x1da418..0x1dd8b0
 *  plus its std::sort / GetStaticInstance<> instances 0x2bbe98..0x2bcab0.
 *  Ported from MioPan graphics/graph3d/gra3d.c (FF2) and checked function by
 *  function against FF3; FF2's asserts and debug prints are gone. What FF3
 *  changed: the light bank's ranges (3 / 27 / 9) and a 0x20-byte status,
 *  the camera keeps its world->view matrix, gra3dApplyCamera takes a
 *  bStabilize flag, the two light emulators share one body, GS registers go
 *  through gs_reg.c's cache (g3dSetGsRegister*), and gra3dInit also sets the
 *  draw parameters and the VU0 constant registers.
 *
 *  It also builds the VU1 light and material blocks (g3dSetVu1LightData,
 *  gra3dCalcVu1MaterialData*) that the SGD walker (gra3dSGD.c) sends. PC: no
 *  VU1 runs them; the last of each is mirrored for gra3dSnapshotVu1Lighting,
 *  which the host renderer bridge (reipan_graph3d.cpp) lights meshes with,
 *  and gra3dApplyCamera / gra3dApplyFog hand the camera and fog to it.
 * ======================================================================== */
#include "graphics/graph3d/gra3d.h"

#include <math.h>                               // tanf, sqrtf
#include <string.h>                             // memcpy, memset, memcmp
#include <algorithm>                            // std::fill, std::sort, std::min, std::max_element

#include "graphics/graph3d/g3dCamera.h"         // g3dCalcView{Screen,Clip}Matrix*
#include "graphics/graph3d/g3dCore.h"           // g3dInitialize, IG3DCompatible, CVu0Matrix
#include "graphics/graph3d/g3dDma.h"            // g3dDmaCopyPacket
#include "graphics/graph3d/g3dGeom.h"           // g3dMatrixSet*
#include "graphics/graph3d/g3dLight.h"          // g3dutilSetLightDefault
#include "graphics/graph3d/g3dLightEx.h"        // g3dEmulateDirectionalLight
#include "graphics/graph3d/g3dVif1.h"           // g3dVif1Init, g3dVif1SetRegister
#include "graphics/graph3d/gra3dBoundingBox.h"  // gra3dbbIsInViewvolume
#include "graphics/graph3d/gra3dDma.h"          // gra3dSetVif1Code_Unpack
#include "graphics/graph3d/gra3dSGD.h"          // gra3dsgdInit, _gra3dDrawSGD
#include "graphics/graph3d/gra3dSGDData.h"      // sgdCalcCoordinate
#include "graphics/graph3d/gra3dShadow.h"       // gra3dshadowInit
#include "graphics/graph3d/gra3dVu0.h"          // Vu0InitConstRegisters
#include "graphics/gs_reg.h"                    // GS_REG_SET
#include "system/os/system.h"                   // GetPALMode

#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer (scratchpad)
#include "reipan/rendering/reipan_graph3d.h"    // PC: ReiPan_Graph3dApplyCamera
#include "reipan/rendering/reipan_renderer.h"   // PC: ReiPan_RendererGetViewExtend, ReiPan_RendererSetFog

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@001ef370 (math helpers after the pad driver)
float NormalizeAngle(float fAngle);


// ──────────────────────────────────────────────────────────────────────
// Module data
// ──────────────────────────────────────────────────────────────────────

// PC: the EE scratchpad (0x70000000..0x70003fff) is emulated memory.
#define GRA3D_SPR_LAYOUT            0x70000000u     // GRA3DSCRATCHPADLAYOUT
#define GRA3D_SPR_MAPSHADOW         0x70000440u     // GRA3DSCRATCHPADLAYOUT_MAPSHADOW
#define GRA3D_SPR_COREOBJECT        0x700005e0u     // G3DCOREOBJECT
#define GRA3D_SPR_PRELIGHT_MATRIX   0x70003900u     // gra3dExecPrelight's work matrix

static int                    s_bEnableMonotoneDraw;        /* sdata 0x370b14 */
static int                    s_bUseScratchpad = 1;         /* sdata 0x370b18 */
static int                    s_iDrawParam;                 /* sdata 0x370b1c */
static float                  s_fDrawParam = 1.3f;          /* sdata 0x370b20 */
static GRA3DSCRATCHPADLAYOUT *s_pScratchpadLayout;          /* sdata 0x370b24 */
static int                    s_bFogEnable;                 /* sbss  0x373354 (gp-0x19c) */

static GRA3DCAMERA      s_Camera;                           /* bss 0x3eb570 */
static G3DFOG           s_Fog;                              /* bss 0x3eb790 */
static int              s_ivFogColor[4];                    /* bss 0x3eb7a0 */
static float            clip_value[4];                      /* bss 0x3eb7b0 */
static G3DLIGHT         s_aLight[NUM_GRA3DLIGHTID];         /* bss 0x3eb7c0 */
static GRA3DLIGHTSTATUS s_aLightStatus[NUM_GRA3DLIGHTID];   /* bss 0x3ec8d0 (FF2 s_LightManage) */
static G3DLIGHT         s_WorkaLight[NUM_GRA3DLIGHTID];     /* bss 0x3ecdb0 */
static GRA3DLIGHTSTATUS s_aWorkLightStatus[NUM_GRA3DLIGHTID]; /* bss 0x3edec0 (FF2 s_WorkLightManage) */
alignas(16) static float s_lmDiffuseLight[3][4];   /* bss 0x3ee3a0 */
alignas(16) static float s_lmSpecularLight[3][4];  /* bss 0x3ee3d0 */
alignas(16) static float s_lmDiffuseColor[3][4];   /* bss 0x3ee400 */
alignas(16) static float s_lmSpecularColor[3][4];  /* bss 0x3ee430 */

// The VU1 material state, one object in FF3's bss order (0x3ee460..0x3ee710).
// It has to stay one object: _ClearMaterialData clears 0x150 bytes from a
// 0x80-byte cache, so the point clear also zeroes the spot cache and the head
// of the directional packet, and the spot clear the directional packet and
// the head of the point packet (as on the EE; MioPan gra3d.c has the story).
// The packets are scratch: gra3dCalcVu1MaterialData* copy them out.
struct GRA3DVU1MATERIALPACKET           /* 0x80 */
{
    /* 0x00 */ u_int                      qwVif1Code[4];
    /* 0x10 */ GRA3DVU1MATERIALDATA_POINT Data;     // directional: the same 0x70 bytes
};
static_assert(sizeof(GRA3DVU1MATERIALPACKET) == 0x80, "GRA3DVU1MATERIALPACKET is 0x80 bytes");

struct GRA3DVU1MATERIALDIRPACKET        /* 0x80 */
{
    /* 0x00 */ u_int                            qwVif1Code[4];
    /* 0x10 */ GRA3DVU1MATERIALDATA_DIRECTIONAL Data;
};
static_assert(sizeof(GRA3DVU1MATERIALDIRPACKET) == 0x80, "GRA3DVU1MATERIALDIRPACKET is 0x80 bytes");

struct GRA3DVU1MATERIALSTATE            /* 0x2b0 */
{
    /* 0x000 */ GRA3DMATERIALINDEXCACHE     aMaterialCache[3];          // bss 0x3ee460
    /* 0x030 */ GRA3DVU1MATERIALCACHE_POINT aVu1MaterialCache_Point[1]; // bss 0x3ee490
    /* 0x0b0 */ GRA3DVU1MATERIALCACHE_SPOT  aVu1MaterialCache_Spot[1];  // bss 0x3ee510
    /* 0x130 */ GRA3DVU1MATERIALDIRPACKET   MaterialPacketDirectional;  // bss 0x3ee590
    /* 0x1b0 */ GRA3DVU1MATERIALPACKET      MaterialPacketPoint;        // bss 0x3ee610
    /* 0x230 */ GRA3DVU1MATERIALPACKET      MaterialPacketSpot;         // bss 0x3ee690
};
static_assert(sizeof(GRA3DVU1MATERIALSTATE) == 0x2b0, "GRA3DVU1MATERIALSTATE is 0x2b0 bytes");
static_assert(offsetof(GRA3DVU1MATERIALSTATE, aVu1MaterialCache_Point) == 0x030, "bss 0x3ee490");
static_assert(offsetof(GRA3DVU1MATERIALSTATE, aVu1MaterialCache_Spot) == 0x0b0, "bss 0x3ee510");
static_assert(offsetof(GRA3DVU1MATERIALSTATE, MaterialPacketDirectional) == 0x130, "bss 0x3ee590");
static_assert(offsetof(GRA3DVU1MATERIALSTATE, MaterialPacketPoint) == 0x1b0, "bss 0x3ee610");
static_assert(offsetof(GRA3DVU1MATERIALSTATE, MaterialPacketSpot) == 0x230, "bss 0x3ee690");

alignas(16) static GRA3DVU1MATERIALSTATE s_Vu1Material;     /* bss 0x3ee460 */
static u_int s_uiMaterialPrimType;                          /* sbss 0x373350 (gp-0x1a0) */

// PC: host mirrors of the VU1 light and material images (MioPan gra3d.c port
// additions), for gra3dSnapshotVu1Lighting. g3dSetVu1LightData and
// gra3dCalcVu1MaterialData* build these blocks into DMA packets the PC never
// runs, so the last of each is kept here. The material packets in
// s_Vu1Material cannot be read later: _ClearMaterialData's overrun wipes them
// (see above), so they are mirrored where they are copied out.
alignas(16) static GRA3DVU1LIGHTDATA                s_Vu1LightImage;
alignas(16) static GRA3DVU1MATERIALDATA_DIRECTIONAL s_Vu1MaterialDirImage;
alignas(16) static GRA3DVU1MATERIALDATA_POINT       s_Vu1MaterialPointImage;
alignas(16) static GRA3DVU1MATERIALDATA_SPOT        s_Vu1MaterialSpotImage;

// Cached DMA packets update VU1 memory on PS2; mirror that replay for the host renderer.
void gra3dRestoreVu1MaterialData(const GRA3DVU1MATERIALDATA_DIRECTIONAL *directional,
                               const GRA3DVU1MATERIALDATA_POINT *point,
                               const GRA3DVU1MATERIALDATA_SPOT *spot)
{
    s_Vu1MaterialDirImage = *directional;
    if (point != nullptr)
    {
        s_Vu1MaterialPointImage = *point;
    }
    if (spot != nullptr)
    {
        s_Vu1MaterialSpotImage = *spot;
    }
}

// The default camera (data 0x328850): identity matrices, a zero coordinate
// (gra3dApplyCamera's stabilize normalises zero rows: VU0 gives 0 there).
static const GRA3DCAMERA s_CameraDefault =
{
    0.7683707f,         // fFov (0x3f44b3f1)
    0.5f,               // fNearZ
    3050.0f,            // fFarZ
    1.0f,               // fAspectX
    0.875f,             // fAspectY
    2048.0f,            // fCenterX
    2048.0f,            // fCenterY
    0.5f,               // fZmin
    65535.0f,           // fZmax
    PT_PERSPECTIVE,     // type
    { 0, 0 },
    { 0.0f, 0.0f, 0.0f, 1.0f },     // vTarget
    { 0.0f, 0.0f, 0.0f, 1.0f },     // vPositionOld
    { 0.0f, 0.0f, 0.0f, 1.0f },     // vTargetOld
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },   // matCoord
};

// The default VU1 image (data 0x328400, 0x440 bytes; all zero after +0x90):
// VIF code, VF01 (1,1,1,-1), VF02 (1,0,0,0), the double-buffer addresses and
// the four primitive GIF tags.
static const u_int s_aScratchpadLayoutDefault[sizeof(GRA3DSCRATCHPADLAYOUT) / 4] =
{
    0x00000000, 0x11000000, 0x01000404, 0xfeef0000,     // +0x000 VIF code
    0x3f800000, 0x3f800000, 0x3f800000, 0xbf800000,     // +0x010 _vVF1
    0x3f800000, 0x00000000, 0x00000000, 0x00000000,     // +0x020 _vVF2
    0x00000060, 0x00000230, 0x00000000, 0x00000000,     // +0x030 DB addresses
    0x00000000, 0x20264000, 0x00000041, 0x00000000,     // +0x040 gtTRISTRIP_NOTEXTURE
    0x00000000, 0x302e4000, 0x00000412, 0x00000000,     // +0x050 gtTRISTRIP_TEXTURE
    0x00008000, 0x00000000, 0x00000000, 0x00000000,     // +0x060 gtEOP
    0x00008000, 0x2026c000, 0x00000041, 0x00000000,     // +0x070 gtTRIFAN_NOTEXTURE
    0x00008000, 0x302ec000, 0x00000412, 0x00000000,     // +0x080 gtTRIFAN_TEXTURE
};

// VIF1 registers gra3dInit sets (data 0x35a3c0).
static const G3DVIF1CMDDATA s_aVif1CmdData[6] =
{
    { 0x03000060, { 0, 0, 0, 0 } },
    { 0x020001d0, { 0, 0, 0, 0 } },
    { 0x01000404, { 0, 0, 0, 0 } },
    { 0x20000000, { 0, 0, 0, 0 } },
    { 0x05000000, { 0, 0, 0, 0 } },
    { 0x30000000, { 0, 0, 0x3f800000, 0x3f800000 } },
};

// GS registers gra3dSetGsRegisterDefault restores (data 0x35a350).
static const GS_REG_SET s_aGsRegisterDefault[5] =
{
    { 0x0000000000000044ull, 0x42 },    // ALPHA_1
    { 0x0000000000000060ull, 0x14 },    // TEX1_1
    { 0x0000000000000000ull, 0x08 },    // CLAMP_1
    { 0x000000000005001bull, 0x47 },    // TEST_1
    { 0x000000000a000118ull, 0x4e },    // ZBUF_1
};

static float clip_volume[4]  = { 1920.0f, 1792.0f, 256.0f, 16777000.0f };      /* data 0x35a3a0 */
static float clip_volumev[4] = { 320.0f, 224.0f, 0.099999994f, 16777000.0f };  /* data 0x35a3b0 */

static const LPFUNC_VIEWSCREENMATRIX s_apViewScreenMatrixFunc[NUM_G3DCAMPROJECTIONTYPE] =  /* sdata 0x370bd0 */
{
    g3dCalcViewScreenMatrixPerspective,
    g3dCalcViewScreenMatrixOrtho,
};
static const LPFUNC_VIEWCLIPMATRIX s_apViewClipMatrixFunc[NUM_G3DCAMPROJECTIONTYPE] =      /* sdata 0x370bd8 */
{
    g3dCalcViewClipMatrixPerspective,
    g3dCalcViewClipMatrixOrtho,
};

// SI -> PS axis flip gra3dExecPrelightMatrix applies (data 0x329400).
alignas(16) static float s_matConvertPrelight[4][4] =
{
    { 1.0f,  0.0f,  0.0f, 0.0f },
    { 0.0f, -1.0f,  0.0f, 0.0f },
    { 0.0f,  0.0f, -1.0f, 0.0f },
    { 0.0f,  0.0f,  0.0f, 1.0f },
};
// The unit matrix gra3dExecPrelight rotates (data 0x3293c0).
alignas(16) static float s_matUnit[4][4] =
{
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

// PC: VU0's vf4..vf7, which _Vu0LoadMatrix_4_5_6_7 fills and
// _Vu0ApplyMatrixWithoutTrans_4_5_6_7 reads (as MioPan).
alignas(16) static float s_matVu0_4567[4][4];

static void _ModifyScratchpad(void);
static void _ModifyFogParam(void);
static int     _Gra3dSetGsRegisterForAutoState(u_long data, int reg, int mode);
static int     _Gra3dSetGsRegistersForAutoState(const GS_REG_SET *regs, int num, int mode);
static u_long *_Gra3dGetGsRegisterRefForAutoState(int reg);
static void _ResetCameraTarget(void);
static void _UpdateLight_Directional(void);
static void _UpdateLight_Point(void);
static void _UpdateLight_Spot(void);
static void _Vu0LoadMatrix_4_5_6_7(float (*mat)[4]);
static void _Vu0ApplyMatrixWithoutTrans_4_5_6_7(float *vDest, float *vSrc);

// G3DLIGHTTYPE of a light slot (inlined by FF3 into _InitLight and
// utilSetGRA3DLIGHTDATADefault).
static int _GetLightType(u_int iLightId)
{
    if (iLightId <= GRA3D_END_LIGHT_DIRECTIONAL)
    {
        return G3DLIGHT_DIRECTIONAL;
    }
    if (iLightId - GRA3D_START_LIGHT_POINT < GRA3D_NUM_LIGHT_POINT)
    {
        return G3DLIGHT_POINT;
    }
    if (iLightId - GRA3D_START_LIGHT_SPOT < GRA3D_NUM_LIGHT_SPOT)
    {
        return G3DLIGHT_SPOT;
    }
    return INVALID_G3DLIGHTTYPE;
}

// The inlined VU0 normalise: all four lanes times rsqrt(x*x + y*y + z*z).
// PC: VU0's rsqrt of 0 is the largest float, which leaves a zero xyz at 0;
// a zero vector stays zero here.
static void _Vu0NormalizeXYZW(float *vDest, const float *vSrc)
{
    float fLen2 = vSrc[0] * vSrc[0] + vSrc[1] * vSrc[1] + vSrc[2] * vSrc[2];
    float q     = (fLen2 > 0.0f) ? 1.0f / sqrtf(fLen2) : 0.0f;

    vDest[0] = vSrc[0] * q;
    vDest[1] = vSrc[1] * q;
    vDest[2] = vSrc[2] * q;
    vDest[3] = vSrc[3] * q;
}

static void _CopyVector(float *vDest, const float *vSrc)
{
    vDest[0] = vSrc[0];
    vDest[1] = vSrc[1];
    vDest[2] = vSrc[2];
    vDest[3] = vSrc[3];
}

// ──────────────────────────────────────────────────────────────────────
// Monotone colours
// ──────────────────────────────────────────────────────────────────────

// When monotone draw is on, an RGB float colour becomes its grey (r+g+b)/3.
// The std::fill is FF3's instance std::fill<float*,float> (FF3 0x002bbe98).
// FF3 0x001da418; from MioPan gra3d.c.
static void _MakeColorToMonotone(float *v)
{
    if (s_bEnableMonotoneDraw != 0)
    {
        float fAverage = (v[0] + v[1] + v[2]) / 3.0f;       // sdata 0x370b28

        std::fill(v, v + 3, fAverage);
    }
}

// The integer overload (gra3dSetFogColor).
// FF3 0x001da478; from MioPan gra3d.c.
static void _MakeColorToMonotone(int *v)
{
    if (s_bEnableMonotoneDraw != 0)
    {
        int iAverage = (v[0] + v[1] + v[2]) / 3;

        v[2] = iAverage;
        v[0] = iAverage;
        v[1] = iAverage;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Init
// ──────────────────────────────────────────────────────────────────────

// Light types off, every status cleared, every slot a default light of its
// range's type.
// FF3 0x001da4c0; from MioPan gra3d.c (FF3 clears status words 0..4).
static void _InitLight(void)
{
    int i;

    for (i = G3DLIGHT_DIRECTIONAL; i < NUM_G3DLIGHTTYPE; i++)
    {
        gra3dEnableLightType(i, 0);
    }

    for (i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        s_aLightStatus[i].bEnable                    = 0;
        s_aLightStatus[i].bEnableToChar              = 0;
        s_aLightStatus[i].bEnableToShadow            = 0;
        s_aLightStatus[i].bEmulateToDirectionalLight = 0;
        s_aLightStatus[i].bEnableToPrelight          = 0;
    }

    for (u_int id = 0; id < NUM_GRA3DLIGHTID; id++)
    {
        g3dutilSetLightDefault(&s_aLight[id], _GetLightType(id));
    }
}

// Fog range 0..0 out to 1e7, fog on.
// FF3 0x001da5a0; from MioPan gra3d.c.
static void _InitFog(void)
{
    s_Fog.fMin  = 0.0f;
    s_Fog.fMax  = 0.0f;
    s_Fog.fNear = 0.0f;
    s_Fog.fFar  = 10000000.0f;                              // sdata 0x370b2c

    gra3dEnableFog(1);
}

// The default camera, applied (stabilised, fixed up).
// FF3 0x001da5e0; from MioPan gra3d.c (FF3 passes bStabilize 1).
static void _InitCamera(void)
{
    memcpy(&s_Camera, &s_CameraDefault, sizeof(GRA3DCAMERA));
    gra3dApplyCamera(nullptr, 1, 1);
}

// ──────────────────────────────────────────────────────────────────────
// Light data records
// ──────────────────────────────────────────────────────────────────────

// Copy a light set, moving every non-directional light by vPosition.
// FF3 0x001da618; from MioPan gra3d.c (FF3 adds all four lanes).
void gra3dLightDataAddOffsetPosition(GRA3DLIGHTDATA *pDest, const GRA3DLIGHTDATA *pSrc, const float *vPosition)
{
    memcpy(pDest, pSrc, sizeof(GRA3DLIGHTDATA));

    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        G3DLIGHT &rL = pDest->aLight[i];

        if (rL.Type != G3DLIGHT_DIRECTIONAL)
        {
            for (int c = 0; c < 4; c++)
            {
                rL.vPosition[c] += vPosition[c];
            }
        }
    }
}

// FF3 0x001da680; from MioPan gra3d.c.
void gra3dLightEnableAll(int bEnable)
{
    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        gra3dLightEnable(i, bEnable);
    }
}

// Save the switches and the enabled lights.
// FF3 0x001da6d8; from MioPan gra3d.c.
void gra3dLightPushData(void)
{
    memcpy(s_aWorkLightStatus, s_aLightStatus, sizeof(s_aLightStatus));

    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        if (gra3dIsLightEnable(i) != 0)
        {
            s_WorkaLight[i] = *gra3dGetLightRef(i);
        }
    }
}

// Restore what gra3dLightPushData saved.
// FF3 0x001da780; from MioPan gra3d.c.
void gra3dLightPopData(void)
{
    memcpy(s_aLightStatus, s_aWorkLightStatus, sizeof(s_aLightStatus));

    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        if (gra3dIsLightEnable(i) != 0)
        {
            gra3dSetLight(i, &s_WorkaLight[i]);
        }
    }
}

// Install a light set: every status, every enabled light (non-directional
// ones moved by vTrans when given), the ambient; then rebuild the lights.
// FF3 0x001da7f8; from MioPan gra3d.c (FF3 adds all four lanes of vTrans).
void gra3dSetLightData(GRA3DLIGHTDATA *pLightData, float *vTrans)
{
    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        GRA3DLIGHTSTATUS *pS = &pLightData->aStatus[i];

        gra3dSetLightStatus(i, pS);

        if (pS->bEnable != 0)
        {
            G3DLIGHT L = pLightData->aLight[i];

            if (vTrans != nullptr && L.Type != G3DLIGHT_DIRECTIONAL)
            {
                for (int c = 0; c < 4; c++)
                {
                    L.vPosition[c] += vTrans[c];
                }
            }

            gra3dSetLight(i, &L);
        }
    }

    g3dSetAmbient(pLightData->vAmbient);
    gra3dApplyLight();
}

// The inverse: read the bank's switches, enabled lights and ambient into a
// light set. FF3-only.
// FF3 0x001da8f8.
void gra3dGetLightData(GRA3DLIGHTDATA *pLightData)
{
    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        GRA3DLIGHTSTATUS *pS = gra3dGetLightStatusRef(i);

        pLightData->aStatus[i] = *pS;

        if (pS->bEnable != 0)
        {
            pLightData->aLight[i] = *gra3dGetLightRef(i);
        }
    }

    _CopyVector(pLightData->vAmbient, gra3dGetAmbientRef());
}

// Replace the lights flagged "emulate to directional" by at most three
// directional lights as seen from vPosition, strongest first, scaled by
// fMagnification. bChar (gra3dEmulateLightData) also requires bEnableToChar;
// without it (gra3dEmulateLightDataObj) slot 38 always counts, with strength
// 1.01. FF3 merged FF2's two bodies into this one.
// The std::sort below is FF3's instances
// FF3 0x002bbec8 (__introsort_loop), FF3 0x002bc088 (__final_insertion_sort),
// FF3 0x002bc1c0 (__partial_sort), FF3 0x002bc380 (__unguarded_partition),
// FF3 0x002bc498 (__insertion_sort), FF3 0x002bc648 (__unguarded_linear_insert),
// FF3 0x002bc6f0 (__make_heap), FF3 0x002bc828 (__adjust_heap),
// FF3 0x002bc980 (__push_heap / linear insert), FF3 0x002bca70 (element copy);
// Ghidra keeps FF2's mismatched names for some of them.
// callcheck: FUN_002bc088, __introsort_loop
//   (std::sort's __final_insertion_sort and __introsort_loop instances)
// FF3 0x001da9f0; from MioPan gra3d.c gra3dEmulateLightData / Obj.
void gra3dEmulateLightDataSub(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition,
                              float fMagnification, int bChar)
{
    G3D_EMULATE_DIRECTIONALLIGHT_DATA raEmuDirLight[NUM_GRA3DLIGHTID];
    int iNumLightEnable;
    int iNumScale;
    int i;

    memset(raEmuDirLight, 0, sizeof(raEmuDirLight));
    iNumLightEnable = 0;

    _CopyVector(pLDDest->vAmbient, pLDSrc->vAmbient);
    memcpy(pLDDest->aStatus, pLDSrc->aStatus, sizeof(pLDDest->aStatus));

    for (i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        GRA3DLIGHTSTATUS &rS = pLDSrc->aStatus[i];

        if (rS.bEnable != 0 && rS.bEmulateToDirectionalLight != 0
            && (rS.bEnableToChar != 0 || bChar == 0))
        {
            G3D_EMULATE_DIRECTIONALLIGHT_DATA &rE = raEmuDirLight[iNumLightEnable];

            g3dEmulateDirectionalLight(&rE, &pLDSrc->aLight[i], vPosition);

            if (bChar == 0 && i == NUM_GRA3DLIGHTID - 1)
            {
                rE.fLength = 1.01f;                         // sdata 0x370b30
            }
            if (rE.fLength > 0.0f)
            {
                iNumLightEnable++;
            }
        }
    }

    std::sort(raEmuDirLight, raEmuDirLight + iNumLightEnable,
              G3D_EMULATE_DIRECTIONALLIGHT_DATA::greater());

    g3dGenerateDirectionalLightByEmulatedData(pLDDest->aLight, raEmuDirLight, iNumLightEnable);

    iNumScale = std::min(iNumLightEnable, 3);
    for (i = 0; i < iNumScale; i++)
    {
        for (int c = 0; c < 4; c++)
        {
            pLDDest->aLight[i].vDiffuse[c] *= fMagnification;
        }
    }

    for (i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        int bEnable;

        if (pLDSrc->aStatus[i].bEmulateToDirectionalLight == 0 && pLDSrc->aStatus[i].bEnable != 0)
        {
            bEnable = 1;
        }
        else
        {
            bEnable = (0 <= i && i < iNumScale) ? 1 : 0;
        }

        pLDDest->aStatus[i].bEnable = bEnable;
    }
}

// FF3 0x001dacc0; FF2 gra3dEmulateLightData.
void gra3dEmulateLightData(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition, float fMagnification)
{
    gra3dEmulateLightDataSub(pLDDest, pLDSrc, vPosition, fMagnification, 1);
}

// FF3 0x001dacd8; FF2 gra3dEmulateLightDataObj.
void gra3dEmulateLightDataObj(GRA3DLIGHTDATA *pLDDest, GRA3DLIGHTDATA *pLDSrc, float *vPosition, float fMagnification)
{
    gra3dEmulateLightDataSub(pLDDest, pLDSrc, vPosition, fMagnification, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Clip rectangle and bounding boxes
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001dacf0; from MioPan gra3d.c.
void _SetClipValue(float minx, float maxx, float miny, float maxy)
{
    clip_value[0] = minx;
    clip_value[2] = miny;
    clip_value[1] = maxx;
    clip_value[3] = maxy;
}

// Non-zero when the clip rectangle is not the full (-1, 1, -1, 1).
// FF3 0x001dad10; from MioPan gra3d.c.
static int _GetClipValueCheck(void)
{
    float vClip[4];

    vClip[2] = -1.0f;                                       // sdata 0x370b34
    vClip[0] = -1.0f;
    vClip[3] =  1.0f;                                       // sdata 0x370b38
    vClip[1] =  1.0f;

    return memcmp(vClip, clip_value, sizeof(vClip)) != 0;
}

// VU0 CLIP of v against +-|w|: bit 0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z.
static u_int _Vu0ClipFlags(const float *v)
{
    float w = fabsf(v[3]);
    u_int f = 0;

    if (v[0] > w)  f |= 0x01;
    if (v[0] < -w) f |= 0x02;
    if (v[1] > w)  f |= 0x04;
    if (v[1] < -w) f |= 0x08;
    if (v[2] > w)  f |= 0x10;
    if (v[2] < -w) f |= 0x20;
    return f;
}

// Whether a world bounding box (8 corners) can be seen. With the full clip
// rectangle this is gra3dbbIsInViewvolume; with a narrowed one FF3 inlines
// the same transform, keeps the projected corners in avBBClipped and also
// rejects boxes wholly outside the rectangle.
// PC: matClip / matTransform were VU0 vf4..vf7 / vf8..vf11 (as MioPan).
// FF3 0x001dad68; from MioPan gra3d.c (FF2 asserted the default rectangle).
int gra3dIsBBInViewvolume(float (*avBBTransformed)[4], float (*avBBClipped)[4], float (*avBBWorld)[4],
                          float (*matClip)[4], float (*matTransform)[4])
{
    u_int uiClip;
    int   bOutMinX, bOutMaxX, bOutMinY, bOutMaxY;
    int   i;

    if (_GetClipValueCheck() == 0)
    {
        return gra3dbbIsInViewvolume(avBBTransformed, avBBWorld, matClip, matTransform) != 0 ? 1 : 0;
    }

    uiClip = 0x3f;
    for (i = 7; i >= 0; i--)
    {
        const float *v = avBBWorld[i];
        float vC[4];
        float q;

        for (int c = 0; c < 4; c++)
        {
            vC[c] = matClip[0][c] * v[0] + matClip[1][c] * v[1] + matClip[2][c] * v[2] + matClip[3][c];
        }
        if (matTransform != nullptr)
        {
            for (int c = 0; c < 4; c++)
            {
                avBBTransformed[i][c] = matTransform[0][c] * v[0] + matTransform[1][c] * v[1]
                                      + matTransform[2][c] * v[2] + matTransform[3][c];
            }
        }
        q = 1.0f / vC[3];
        for (int c = 0; c < 4; c++)
        {
            avBBClipped[i][c] = vC[c] * q;
        }
        uiClip &= _Vu0ClipFlags(vC);
    }

    if (uiClip != 0)
    {
        return 0;
    }

    bOutMinY = 1;
    bOutMaxY = 1;
    bOutMaxX = 1;
    bOutMinX = 1;
    for (i = 0; i < 8; i++)
    {
        float x = avBBClipped[i][0];
        float y = avBBClipped[i][1];

        if (clip_value[0] < x) bOutMinX = 0;
        if (x < clip_value[1]) bOutMaxX = 0;
        if (clip_value[2] < y) bOutMinY = 0;
        if (y < clip_value[3]) bOutMaxY = 0;
    }

    return (!bOutMinX && !bOutMaxY && !bOutMinY && !bOutMaxX) ? 1 : 0;
}

// A world bounding box against the camera's object frustum.
// PC: FF3 left vf8..vf11 as the previous caller had them and nothing reads
// the transformed corners; no transform is passed (as MioPan).
// FF3 0x001daed0; from MioPan gra3d.c.
int CheckModelBoundingBox(float (*avBBWorld)[4])
{
    GRA3DCAMERA *pCam = gra3dGetCamera();
    float avClipped[8][4];
    float avTransformed[8][4];

    return gra3dIsBBInViewvolume(avTransformed, avClipped, avBBWorld, pCam->matWorldClipObject, nullptr) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Prelight
// ──────────────────────────────────────────────────────────────────────

// Prelight a model placed at vTrans with Euler angles vRot: build the matrix
// in scratchpad 0x70003900 and run the matrix overload.
// FF3 0x001daf20; from MioPan gra3d.c (FF3 wraps the angles with
// NormalizeAngle and rotates the unit matrix, FF2 g_matConvertSI2PS).
void gra3dExecPrelight(SGDFILEHEADER *pSGDHead, float *vTrans, float *vRot)
{
    float (*mat)[4] = (float (*)[4])ReiPan_GetHostPointer(GRA3D_SPR_PRELIGHT_MATRIX);
    sceVu0FVECTOR vRotN;

    vRotN[0] = NormalizeAngle(vRot[0]);
    vRotN[1] = NormalizeAngle(vRot[1]);
    vRotN[2] = NormalizeAngle(vRot[2]);
    vRotN[3] = 0.0f;                                        // PC: FF3 leaves it unset; RotMatrix ignores w

    sceVu0RotMatrix(mat, s_matUnit, vRotN);
    mat[3][0] = vTrans[0];
    mat[3][1] = vTrans[1];
    mat[3][2] = vTrans[2];
    mat[3][3] = 1.0f;                                       // sdata 0x370b3c

    gra3dExecPrelightMatrix(pSGDHead, mat);
}

// Bake the lighting of an SGD model under mat (flipped to PS axes).
// FF3 0x001dafd0; from MioPan gra3d.c gra3dExecPrelight(SGDFILEHEADER *, mat)
// (FF3 converts the matrix here, FF2 did it in the seed).
void gra3dExecPrelightMatrix(SGDFILEHEADER *pSGDHead, float (*mat)[4])
{
    sceVu0FMATRIX matLocalWorld;

    if (pSGDHead->uiVersionId == SGD_VALID_VERSIONID)
    {
        sceVu0MulMatrix(matLocalWorld, mat, s_matConvertPrelight);
        sgdCalcCoordinate(pSGDHead, matLocalWorld);
        _gra3dDrawSGD(pSGDHead, SRT_PRELIGHTING, nullptr, -1);
        ((SGDCOORDINATE *)ReiPan_GetHostPointer(pSGDHead->pCoord))->bCalc = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Camera
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001db098; from MioPan gra3d.c.
GRA3DCAMERA *gra3dGetCamera(void)
{
    return &s_Camera;
}

// FF3 0x001db0a8; from MioPan gra3d.c.
float *gra3dcamGetPosition(void)
{
    return gra3dGetCamera()->matCoord[3];
}

// FF3 0x001db0c8; from MioPan gra3d.c.
float *gra3dcamGetDirection(void)
{
    return gra3dGetCamera()->matCoord[2];
}

// FF3 0x001db0e8; from MioPan gra3d.c.
float *gra3dcamGetTarget(void)
{
    return gra3dGetCamera()->vTarget;
}

// Move the camera, keeping where it was.
// FF3 0x001db108; from MioPan gra3d.c.
void gra3dcamSetPosition(float *vPos)
{
    _CopyVector(s_Camera.vPositionOld, s_Camera.matCoord[3]);
    g3dMatrixSetPosition(s_Camera.matCoord, vPos);
}

// FF3 0x001db138; from MioPan gra3d.c.
void gra3dcamSetPosition(float x, float y, float z)
{
    float v[4];

    v[3] = 1.0f;                                            // sdata 0x370b40
    v[0] = x;
    v[1] = y;
    v[2] = z;
    gra3dcamSetPosition(v);
}

// FF3 0x001db170; from MioPan gra3d.c.
float *gra3dcamGetPositionOld(void)
{
    return s_Camera.vPositionOld;
}

// Put the target back on the forward axis at its current distance.
// FF3 0x001db180; from MioPan gra3d.c.
static void _ResetCameraTarget(void)
{
    float vD[4];
    float vDir[4];
    float fDist;

    for (int c = 0; c < 4; c++)
    {
        vD[c] = s_Camera.matCoord[3][c] - s_Camera.vTarget[c];
    }
    fDist = sqrtf(vD[0] * vD[0] + vD[1] * vD[1] + vD[2] * vD[2]);

    _Vu0NormalizeXYZW(vDir, s_Camera.matCoord[2]);
    for (int c = 0; c < 4; c++)
    {
        s_Camera.vTarget[c] = s_Camera.matCoord[3][c] + vDir[c] * fDist;
    }
}

// Set the orientation rows, keep the target in front.
// FF3 0x001db200; from MioPan gra3d.c.
void gra3dcamSetCoord(float (*mat)[4])
{
    _CopyVector(s_Camera.matCoord[0], mat[0]);
    _CopyVector(s_Camera.matCoord[1], mat[1]);
    _CopyVector(s_Camera.matCoord[2], mat[2]);
    _ResetCameraTarget();
}

// FF3 0x001db248; from MioPan gra3d.c.
void gra3dcamSetTarget(float *vTarget, int bFixUp)
{
    g3dMatrixSetTarget(s_Camera.matCoord, vTarget, bFixUp);
    _CopyVector(s_Camera.vTarget, vTarget);
}

// FF3 0x001db298; from MioPan gra3d.c.
void gra3dcamSetTarget(float x, float y, float z, int bFixUp)
{
    float v[4];

    v[3] = 1.0f;                                            // sdata 0x370b44
    v[0] = x;
    v[1] = y;
    v[2] = z;
    gra3dcamSetTarget(v, bFixUp);
}

// Aim at vTarget with an explicit up vector. FF3-only.
// FF3 0x001db2d8.
void gra3dcamSetTargetUp(float *vTarget, float *vUp)
{
    g3dMatrixSetTargetUp(s_Camera.matCoord, vTarget, vUp);
    _CopyVector(s_Camera.vTarget, vTarget);
}

// FF3 0x001db328; from MioPan gra3d.c.
void gra3dcamSetRoll(float fRad)
{
    g3dMatrixSetRoll(s_Camera.matCoord, fRad);
    _ResetCameraTarget();
}

// FF3 0x001db348; from MioPan gra3d.c.
float gra3dcamGetRoll(void)
{
    return g3dMatrixGetRoll(s_Camera.matCoord);
}

// FF3 0x001db368; from MioPan gra3d.c.
void gra3dcamRotationByAxis(float *vAxis, float fAngle)
{
    g3dMatrixRotationByAxis(s_Camera.matCoord, vAxis, fAngle);
    _ResetCameraTarget();
}

// Enabled slots of a light type in a light set (ambient counts 1).
// FF3 0x001db390; from MioPan gra3d.c (FF3's ranges).
int gra3dGetNumLightEnable(GRA3DLIGHTDATA *pLD, int iLightType)
{
    int iStart;
    int iNum;
    int iRet;

    if (iLightType == G3DLIGHT_AMBIENT)
    {
        return 1;
    }

    switch (iLightType)
    {
    case G3DLIGHT_DIRECTIONAL:
        iStart = GRA3D_START_LIGHT_DIRECTIONAL;
        iNum   = GRA3D_NUM_LIGHT_DIRECTIONAL;
        break;
    case G3DLIGHT_POINT:
        iStart = GRA3D_START_LIGHT_POINT;
        iNum   = GRA3D_NUM_LIGHT_POINT;
        break;
    case G3DLIGHT_SPOT:
        iStart = GRA3D_START_LIGHT_SPOT;
        iNum   = GRA3D_NUM_LIGHT_SPOT;
        break;
    default:
        iStart = 0x7fffffff;
        iNum   = 0;
        break;
    }

    iRet = 0;
    for (int i = iStart; i < iStart + iNum; i++)
    {
        if (pLD->aStatus[i].bEnable != 0)
        {
            iRet++;
        }
    }
    return iRet;
}

// FF3 0x001db450; from MioPan gra3d.c.
int gra3dGetNumLightInitial(GRA3DLIGHTDATA *pLD, int iLightType)
{
    if (iLightType == G3DLIGHT_AMBIENT)
    {
        return 1;
    }
    if (pLD == nullptr)
    {
        return 0;
    }
    return pLD->aiNumInitial[iLightType];
}

// A cleared light set with every slot a default light of its type.
// FF3 0x001db478; from MioPan gra3d.c.
void utilSetGRA3DLIGHTDATADefault(GRA3DLIGHTDATA *pLD)
{
    memset(pLD, 0, sizeof(GRA3DLIGHTDATA));

    for (u_int id = 0; id < NUM_GRA3DLIGHTID; id++)
    {
        g3dutilSetLightDefault(&pLD->aLight[id], _GetLightType(id));
    }
}

// The 3D pass's GS defaults (alpha, tex1, clamp, test, zbuf).
// FF3 0x001db518; from MioPan gra3d.c (FF3 sends them through the cache).
void gra3dSetGsRegisterDefault(void)
{
    GS_REG_SET aGPA[5];

    for (int i = 0; i < 5; i++)
    {
        aGPA[i] = s_aGsRegisterDefault[i];
    }
    g3dSetGsRegisters(aGPA, 5, 1);
}

// v = -(normalised forward axis).
// FF3 0x001db588; from MioPan gra3d.c.
static void _GetCameraDirectionInverse(float *v)
{
    _Vu0NormalizeXYZW(v, s_Camera.matCoord[2]);
    for (int c = 0; c < 4; c++)
    {
        v[c] = v[c] * -1.0f;
    }
}

// Re-normalise the axes, w of the position 1, re-aim at the target.
// FF3 0x001db5d0; from MioPan gra3d.c.
static void _StabilizeCamera(GRA3DCAMERA *pCam, int bFixup)
{
    _Vu0NormalizeXYZW(pCam->matCoord[0], pCam->matCoord[0]);
    _Vu0NormalizeXYZW(pCam->matCoord[1], pCam->matCoord[1]);
    _Vu0NormalizeXYZW(pCam->matCoord[2], pCam->matCoord[2]);
    pCam->matCoord[3][3] = 1.0f;                            // sdata 0x370b48

    g3dMatrixSetTarget(pCam->matCoord, pCam->vTarget, bFixup);
}

// world -> screen for pCam into mat (stabilises pCam and refreshes its
// world -> view on the way).
// FF3 0x001db678; from MioPan gra3d.c (FF3 keeps world -> view in the camera).
void gra3dCalcWorldScreenMatrix(float (*mat)[4], GRA3DCAMERA *pCam, int bFixup)
{
    sceVu0FMATRIX matViewScreen;
    float fScrZ;

    _StabilizeCamera(pCam, bFixup);

    fScrZ = 224.0f / tanf(pCam->fFov * 0.5f);               // sdata 0x370b50, 0x370b4c

    sceVu0CameraMatrix(pCam->matWorldView, pCam->matCoord[3], pCam->matCoord[2], pCam->matCoord[1]);

    s_apViewScreenMatrixFunc[pCam->type](matViewScreen, fScrZ, pCam->fAspectX, pCam->fAspectY,
                                         pCam->fCenterX, pCam->fCenterY, pCam->fZmin, pCam->fZmax,
                                         pCam->fNearZ, pCam->fFarZ);

    sceVu0MulMatrix(mat, matViewScreen, pCam->matWorldView);
}

// Apply pCam (the module camera when null): world -> view, the projections,
// world -> screen / clip (polygon and object), and the VIEW / PROJECTION
// transforms of the core.
// PC (MioPan gra3d.c): the clip volumes grow by however much wider (taller)
// the window is than the PS2's 640x448 frame, so a wide window shows more
// instead of stretching (both factors are 1 at 4:3), and the camera goes to the
// host renderer (ReiPan_Graph3dApplyCamera: world->view and matViewClipObject).
// FF3 0x001db760; from MioPan gra3d.c (FF3: bStabilize, world -> view kept in
// the camera).
void gra3dApplyCamera(GRA3DCAMERA *pCam, int bFixup, int bStabilize)
{
    sceVu0FMATRIX matViewScreen;
    float fScrZ;
    float fExtendX;                                         // PC
    float fExtendY;                                         // PC

    if (pCam == nullptr)
    {
        pCam = &s_Camera;
    }
    if (bStabilize != 0)
    {
        _StabilizeCamera(pCam, bFixup);
    }

    fScrZ = 224.0f / tanf(pCam->fFov * 0.5f);               // sdata 0x370b54, 0x370b58

    sceVu0CameraMatrix(pCam->matWorldView, pCam->matCoord[3], pCam->matCoord[2], pCam->matCoord[1]);

    ReiPan_RendererGetViewExtend(&fExtendX, &fExtendY);      // PC

    s_apViewScreenMatrixFunc[pCam->type](matViewScreen, fScrZ, pCam->fAspectX, pCam->fAspectY,
                                         pCam->fCenterX, pCam->fCenterY, pCam->fZmin, pCam->fZmax,
                                         pCam->fNearZ, pCam->fFarZ);
    s_apViewClipMatrixFunc[pCam->type](pCam->matViewClipPolygon, fScrZ, pCam->fAspectX, pCam->fAspectY,
                                       pCam->fNearZ, pCam->fFarZ, 1920.0f * fExtendX, 1792.0f * fExtendY);   // sdata 0x370b5c/60
    s_apViewClipMatrixFunc[pCam->type](pCam->matViewClipObject, fScrZ, pCam->fAspectX, pCam->fAspectY,
                                       pCam->fNearZ, pCam->fFarZ, 320.0f * fExtendX, 224.0f * fExtendY);     // sdata 0x370b64/54

    sceVu0MulMatrix(pCam->matWorldScreen, matViewScreen, pCam->matWorldView);
    sceVu0MulMatrix(pCam->matWorldClipPolygon, pCam->matViewClipPolygon, pCam->matWorldView);
    sceVu0MulMatrix(pCam->matWorldClipObject, pCam->matViewClipObject, pCam->matWorldView);

    gra3dSetTransform(GRA3DTS_VIEW, pCam->matWorldView);
    gra3dSetTransform(GRA3DTS_PROJECTION, matViewScreen);

    ReiPan_Graph3dApplyCamera(pCam, pCam->matWorldView);    // PC
}

// Replace the module camera wholesale.
// FF3 0x001db930; from MioPan gra3d.c.
void _gra3dSetCameraForce(GRA3DCAMERA *pCamera)
{
    s_Camera = *pCamera;
}

// FF3 0x001db9b0; from MioPan gra3d.c.
void gra3dcamSetFov(float fFov)
{
    s_Camera.fFov = fFov;
}

// FF3 0x001db9c0; from MioPan gra3d.c.
float gra3dcamGetFov(void)
{
    return s_Camera.fFov;
}

// The pixel aspect; PAL lines are 8/7 taller.
// FF3 0x001db9d0; from MioPan gra3d.c.
void gra3dcamSetAspect(float fX, float fY)
{
    s_Camera.fAspectX = fX;
    s_Camera.fAspectY = fY;

    if (GetPALMode() != 0)
    {
        s_Camera.fAspectY = s_Camera.fAspectY * 1.1428571f;    // sdata 0x370b68
    }
}

// FF3 0x001dba20; from MioPan gra3d.c.
void gra3dcamSetDepth(float fMinZ, float fMaxZ)
{
    s_Camera.fZmin = fMinZ;
    s_Camera.fZmax = fMaxZ;
}

// FF3 0x001dba38; from MioPan gra3d.c.
void gra3dcamSetClip(float fNearZ, float fFarZ)
{
    s_Camera.fNearZ = fNearZ;
    s_Camera.fFarZ  = fFarZ;
}

// FF3 0x001dba50; from MioPan gra3d.c.
void gra3dcamSetType(int type)
{
    s_Camera.type = type;
}

// FF3 0x001dba60; from MioPan gra3d.c.
float *_GetClipVolume(void)
{
    return clip_volume;
}

// FF3 0x001dba70; from MioPan gra3d.c.
float *_GetClipVolumeV(void)
{
    return clip_volumev;
}

// ──────────────────────────────────────────────────────────────────────
// Fog
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001dba80; from MioPan gra3d.c.
void gra3dSetFog(float fMin, float fMax, float fNear, float fFar)
{
    s_Fog.fMin  = fMin;
    s_Fog.fMax  = fMax;
    s_Fog.fNear = fNear;
    s_Fog.fFar  = fFar;
}

// FF3 0x001dbaa0; from MioPan gra3d.c.
void gra3dSetFog(G3DFOG *pFog)
{
    s_Fog = *pFog;
}

// The GS fog colour (FOGCOL), grey in monotone mode.
// FF3 0x001dbab0; from MioPan gra3d.c (FF3 sends it through the GS cache).
void gra3dSetFogColor(int r, int g, int b)
{
    s_ivFogColor[3] = 0;
    s_ivFogColor[0] = r;
    s_ivFogColor[1] = g;
    s_ivFogColor[2] = b;

    _MakeColorToMonotone(s_ivFogColor);

    g3dSetGsRegister((u_long)(int64_t)s_ivFogColor[0]
                     | (u_long)(int64_t)s_ivFogColor[1] << 8
                     | (u_long)(int64_t)s_ivFogColor[2] << 16, 0x3d /* FOGCOL */, 1);
}

// The fog range into the VU1 image.
// PC (MioPan gra3d.c): the VU1 turned that block into one fog value per
// vertex; the host renderer gets the same numbers (enable, range, the two
// coefficients, colour) and evaluates the ramp per pixel. This is the one
// point where a fog setting is committed.
// FF3 0x001dbb18; from MioPan gra3d.c.
void gra3dApplyFog(void)
{
    g3dCalcVu1Fog(&s_pScratchpadLayout->Vu1Mem.Fog, &s_Fog);

    ReiPan_RendererSetFog(s_bFogEnable, s_Fog.fMin, s_Fog.fMax, g3dCalcFA(&s_Fog), g3dCalcFB(&s_Fog),
                          s_ivFogColor[0], s_ivFogColor[1], s_ivFogColor[2]);  // PC
}

// FF3 0x001dbb40; from MioPan gra3d.c.
G3DFOG *gra3dGetFogRef(void)
{
    return &s_Fog;
}

// FF3 0x001dbb50; from MioPan gra3d.c.
int gra3dIsFogEnable(void)
{
    return s_bFogEnable;
}

// The fog flag into the FGE bit (bit 52) of the four primitive GIF tags.
// FF3 0x001dbb58; from MioPan gra3d.c.
static void _ModifyFogParam(void)
{
    GRA3DVU1MEMLAYOUT &rVu1Mem = s_pScratchpadLayout->Vu1Mem;
    u_long lFogBit = ((u_long)(int64_t)s_bFogEnable & 1) << 0x34;

    rVu1Mem.gtTRISTRIP_NOTEXTURE[0] = (rVu1Mem.gtTRISTRIP_NOTEXTURE[0] & 0xffefffffffffffffull) | lFogBit;
    rVu1Mem.gtTRISTRIP_TEXTURE[0]   = (rVu1Mem.gtTRISTRIP_TEXTURE[0]   & 0xffefffffffffffffull) | lFogBit;
    rVu1Mem.gtTRIFAN_NOTEXTURE[0]   = (rVu1Mem.gtTRIFAN_NOTEXTURE[0]   & 0xffefffffffffffffull) | lFogBit;
    rVu1Mem.gtTRIFAN_TEXTURE[0]     = (rVu1Mem.gtTRIFAN_TEXTURE[0]     & 0xffefffffffffffffull) | lFogBit;
}

// FF3 0x001dbbe0; from MioPan gra3d.c.
void gra3dEnableFog(int b)
{
    if (s_bFogEnable != b)
    {
        s_bFogEnable = b;
        _ModifyFogParam();
    }
}

// FF3 0x001dbc08; from MioPan gra3d.c.
void g3dCalcVu1Fog(G3DVU1FOG *pVu1Fog, G3DFOG *pFog)
{
    pVu1Fog->fMin = pFog->fMin;
    pVu1Fog->fMax = pFog->fMax;
    pVu1Fog->FA   = g3dCalcFA(pFog);
    pVu1Fog->FB   = g3dCalcFB(pFog);
}

// FF3 0x001dbc60; from MioPan gra3d.c.
float g3dCalcFA(G3DFOG *pFog)
{
    return (((pFog->fMin - pFog->fMax) * (pFog->fFar + pFog->fNear)) / (pFog->fFar - pFog->fNear)
            + pFog->fMin + pFog->fMax) * 0.5f;              // sdata 0x370b6c
}

// FF3 0x001dbca8; from MioPan gra3d.c.
float g3dCalcFB(G3DFOG *pFog)
{
    return (pFog->fFar * pFog->fNear * (pFog->fMax - pFog->fMin)) / (pFog->fFar - pFog->fNear);
}

// FF3 0x001dbce0; from MioPan gra3d.c.
int gra3dIsMonotoneDrawEnable(void)
{
    return s_bEnableMonotoneDraw;
}

// FF3 0x001dbce8; from MioPan gra3d.c.
void gra3dMonotoneDrawEnable(int bEnable)
{
    s_bEnableMonotoneDraw = bEnable;
}

// ──────────────────────────────────────────────────────────────────────
// Light bank
// ──────────────────────────────────────────────────────────────────────

// Zero a GRA3DVU1MATERIALDATA's worth (0x150 bytes) from pMatData; handed a
// 0x80-byte cache inside s_Vu1Material, so it runs on into what follows.
// FF3 0x001dbcf0; from MioPan gra3d.c.
static void _ClearMaterialData(void *pMatData)
{
    memset(pMatData, 0, 0x150);
}

// Fill one of the core's point / spot slots (iIndex 0..2 of the type) with a
// gra3d light (iLightTypeIndex within the type's bank range), or switch the
// slot off when iLightTypeIndex is INVALID_GRA3DLIGHTID.
// FF3 0x001dbd10; from MioPan gra3d.c (FF3's bank ranges).
void gra3dSetValidLightId(int iLightType, int iIndex, int iLightTypeIndex)
{
    int iCoreId;

    if (iLightType == G3DLIGHT_POINT)
    {
        iCoreId = iIndex + 3;
        if (iLightTypeIndex != INVALID_GRA3DLIGHTID)
        {
            g3dSetLight(iCoreId, &s_aLight[GRA3D_START_LIGHT_POINT + iLightTypeIndex]);
            g3dLightEnable(iCoreId, 1);
            return;
        }
    }
    else if (iLightType == G3DLIGHT_SPOT)
    {
        iCoreId = iIndex + 6;
        if (iLightTypeIndex != INVALID_GRA3DLIGHTID)
        {
            g3dSetLight(iCoreId, &s_aLight[GRA3D_START_LIGHT_SPOT + iLightTypeIndex]);
            g3dLightEnable(iCoreId, 1);
            return;
        }
    }
    else
    {
        return;
    }
    g3dLightEnable(iCoreId, 0);
}

// The directional lights' VU1 matrices: per enabled slot its colour, its
// normalised direction (diffuse) and the half vector with the inverted view
// direction (specular), as columns.
// FF3 0x001dbdc0; from MioPan gra3d.c.
static void _UpdateLight_Directional(void)
{
    GRA3DCAMERA *pCam = gra3dGetCamera();
    float vCamDirInverse[4];

    for (int c = 0; c < 4; c++)
    {
        vCamDirInverse[c] = pCam->matCoord[2][c] * -1.0f;
    }

    memset(s_lmDiffuseLight, 0, sizeof(s_lmDiffuseLight));
    memset(s_lmSpecularLight, 0, sizeof(s_lmSpecularLight));
    memset(s_lmDiffuseColor, 0, sizeof(s_lmDiffuseColor));
    memset(s_lmSpecularColor, 0, sizeof(s_lmSpecularColor));

    for (int i = 0; i < GRA3D_NUM_LIGHT_DIRECTIONAL; i++)
    {
        if (s_aLightStatus[i].bEnable != 0)
        {
            G3DLIGHT &rLight = s_aLight[GRA3D_START_LIGHT_DIRECTIONAL + i];
            float vDir[4];
            float vHalf[4];

            _CopyVector(rLight.vSpecular, rLight.vDiffuse);
            _CopyVector(s_lmDiffuseColor[i], rLight.vDiffuse);
            _CopyVector(s_lmSpecularColor[i], rLight.vSpecular);

            _Vu0NormalizeXYZW(vDir, rLight.vDirection);
            g3dMatrixSetColumnXYZ(s_lmDiffuseLight, vDir, i);

            for (int c = 0; c < 4; c++)
            {
                vHalf[c] = vCamDirInverse[c] + vDir[c];
            }
            _Vu0NormalizeXYZW(vHalf, vHalf);
            g3dMatrixSetColumnXYZ(s_lmSpecularLight, vHalf, i);
        }
    }

    s_lmDiffuseColor[0][3] = 1.0f;                          // sdata 0x370b70
}

// Point lights: specular = diffuse, both without alpha.
// FF3 0x001dbfb8; from MioPan gra3d.c (FF3's range).
static void _UpdateLight_Point(void)
{
    for (int i = GRA3D_START_LIGHT_POINT; i <= GRA3D_END_LIGHT_POINT; i++)
    {
        if (s_aLightStatus[i].bEnable != 0)
        {
            G3DLIGHT &rL = s_aLight[i];

            _CopyVector(rL.vSpecular, rL.vDiffuse);
            rL.vDiffuse[3]  = 0.0f;
            rL.vSpecular[3] = 0.0f;
        }
    }
}

// Spot lights: as point lights, and the direction re-normalised.
// FF3 0x001dc008; from MioPan gra3d.c (FF3's range).
static void _UpdateLight_Spot(void)
{
    for (int i = GRA3D_START_LIGHT_SPOT; i <= GRA3D_END_LIGHT_SPOT; i++)
    {
        if (s_aLightStatus[i].bEnable != 0)
        {
            G3DLIGHT &rL = s_aLight[i];

            _CopyVector(rL.vSpecular, rL.vDiffuse);
            rL.vDiffuse[3]  = 0.0f;
            rL.vSpecular[3] = 0.0f;
            _Vu0NormalizeXYZW(rL.vDirection, rL.vDirection);
        }
    }
}

// How many of a type's bank slots are on (any other type: 0; FF2 counted an
// "ambient" range here, FF3 does not).
// FF3 0x001dc080; from MioPan gra3d.c (FF3's ranges).
int gra3dGetNumEnableLight(int iLightType)
{
    int iStart;
    int iNum;

    switch (iLightType)
    {
        case G3DLIGHT_DIRECTIONAL:
            iStart = GRA3D_START_LIGHT_DIRECTIONAL;
            iNum   = GRA3D_NUM_LIGHT_DIRECTIONAL;
            break;
        case G3DLIGHT_POINT:
            iStart = GRA3D_START_LIGHT_POINT;
            iNum   = GRA3D_NUM_LIGHT_POINT;
            break;
        case G3DLIGHT_SPOT:
            iStart = GRA3D_START_LIGHT_SPOT;
            iNum   = GRA3D_NUM_LIGHT_SPOT;
            break;
        default:
            return 0;                       // FF3: start INVALID, count 0
    }

    int iRet = 0;
    for (int i = iStart; i < iStart + iNum; i++)
    {
        if (s_aLightStatus[i].bEnable != 0)
        {
            iRet++;
        }
    }
    return iRet;
}

// ──────────────────────────────────────────────────────────────────────
// VU1 light / material packets
// ──────────────────────────────────────────────────────────────────────

// The core's enabled point lights (slots 3..5) into the VU1 point block, three
// to a group: position, and in its w the attenuation 1 / (range * intensity *
// (r + g + b)) (0 for intensity 0); the range and colours go to the point
// material cache with the group index of each column.
// FF3 0x001dc138; from MioPan gra3d.c.
static void _SetVu1LightData_Point(GRA3DVU1LIGHTDATA_POINT *pVu1LightDataPoint)
{
    GRA3DVU1MATERIALCACHE_POINT *pCache = s_Vu1Material.aVu1MaterialCache_Point;
    int iColInData = 0;
    int gnum       = 0;

    _ClearMaterialData(pCache);
    memset(pVu1LightDataPoint->lmPosition, 0, sizeof(LMATRIX));

    for (int gcount = 0; gcount < 3; gcount++)
    {
        int iLightId = gcount + 3;

        if (g3dIsLightEnable(iLightId) != 0)
        {
            G3DLIGHT *pL = g3dGetLightRef(iLightId);
            float    *vPos = pVu1LightDataPoint->lmPosition[iColInData];

            _CopyVector(vPos, pL->vPosition);
            // afPad0[0]: the intensity (FF2 gra3dSetLightIntens), not fFalloff.
            if (pL->afPad0[0] == 0.0f)
            {
                vPos[3] = 0.0f;
            }
            else
            {
                float fSum = pL->vDiffuse[0] + pL->vDiffuse[1] + pL->vDiffuse[2];

                vPos[3] = 1.0f / (pL->fMaxRange * pL->afPad0[0] * fSum);    // sdata 0x370b74
            }

            pCache[gnum].Data.vPower[iColInData] = pL->fMaxRange;
            _CopyVector(pCache[gnum].Data.lmDiffuse[iColInData], pL->vDiffuse);
            _CopyVector(pCache[gnum].Data.lmSpecular[iColInData], pL->vSpecular);
            pCache[gnum].Index.aiIndex[iColInData] = gcount;

            iColInData++;
            if (iColInData > 2)
            {
                // A full group; the jump past 100 ends the loop (three slots).
                iColInData = 0;
                gnum++;
                if (gnum > 0)
                {
                    gcount = gra3dGetNumEnableLight(G3DLIGHT_POINT) + 100;
                }
            }
        }
    }
}

// The core's enabled spot lights (slots 6..8) into the VU1 spot block, as
// the point lights, plus the cone (vIntens = afPad0[0], cos^2 of the half
// angle; vIntensB = 1 / (1 - it)), the direction, and a "specular" column:
// the direction, taken into the object's frame by wlmtx when given,
// normalised (FF3 also forms direction minus inverted view direction and
// drops it, as FF2).
// FF3 0x001dc2b8; from MioPan gra3d.c.
static void _SetVu1LightData_Spot(GRA3DVU1LIGHTDATA_SPOT *pVu1LightDataSpot, float (*wlmtx)[4])
{
    GRA3DVU1MATERIALCACHE_SPOT *pCache = s_Vu1Material.aVu1MaterialCache_Spot;
    float vCamDirInv[4];
    int   iColInData = 0;
    int   gnum       = 0;

    _GetCameraDirectionInverse(vCamDirInv);

    _ClearMaterialData(pCache);
    memset(pVu1LightDataSpot, 0, sizeof(GRA3DVU1LIGHTDATA_SPOT));

    for (int gcount = 0; gcount < 3; gcount++)
    {
        int iLightId = gcount + 6;

        if (g3dIsLightEnable(iLightId) != 0)
        {
            G3DLIGHT *pL = g3dGetLightRef(iLightId);
            float     dtmp[4];
            float     stmp[4];
            float     vNorm[4];

            _CopyVector(pVu1LightDataSpot->lmPosition[iColInData], pL->vPosition);
            pVu1LightDataSpot->vIntens[iColInData]  = pL->afPad0[0];
            pVu1LightDataSpot->vIntensB[iColInData] = 1.0f / (1.0f - pL->afPad0[0]);    // sdata 0x370b78
            _CopyVector(pVu1LightDataSpot->lmDirection[iColInData], pL->vDirection);

            if (wlmtx == nullptr)
            {
                _CopyVector(dtmp, pL->vDirection);
                _CopyVector(stmp, vCamDirInv);
            }
            else
            {
                CVu0Matrix::s_pFuncLoadMatrix(wlmtx);
                CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(dtmp, pL->vDirection);
                CVu0Matrix::s_pFuncApplyMatrixWithoutTrans(stmp, vCamDirInv);
            }
            for (int c = 0; c < 4; c++)
            {
                stmp[c] = dtmp[c] - stmp[c];        // unused, as FF3
            }
            _Vu0NormalizeXYZW(vNorm, dtmp);
            g3dMatrixSetColumnXYZ(pVu1LightDataSpot->lmSpecular, vNorm, iColInData);

            pCache[gnum].Data.vPower[iColInData] = pL->fMaxRange;
            _CopyVector(pCache[gnum].Data.lmDiffuse[iColInData], pL->vDiffuse);
            _CopyVector(pCache[gnum].Data.lmSpecular[iColInData], pL->vSpecular);
            pCache[gnum].Index.aiIndex[iColInData] = gcount;

            iColInData++;
            if (iColInData > 2)
            {
                iColInData = 0;
                gnum++;
                if (gnum > 0)
                {
                    gcount = gra3dGetNumEnableLight(G3DLIGHT_SPOT) + 100;
                }
            }
        }
    }
}

// The directional light matrices (_UpdateLight_Directional's columns) into
// the VU1 block, turned by mat's rows 0..2 when given: out[r].xyz =
// lm[0] * mat[r].x + lm[1] * mat[r].y + lm[2] * mat[r].z, out[r].w = mat[r].w.
// FF3 0x001dc540; from MioPan gra3d.c.
static void _SetVu1LightData_Directional(GRA3DVU1LIGHTDATA_DIRECTIONAL *pVu1LightDataDirectional, float (*mat)[4])
{
    if (mat == nullptr)
    {
        memcpy(pVu1LightDataDirectional->lmDiffuse, s_lmDiffuseLight, sizeof(LMATRIX));
        memcpy(pVu1LightDataDirectional->lmSpecular, s_lmSpecularLight, sizeof(LMATRIX));
        return;
    }

    for (int r = 0; r < 3; r++)
    {
        float *vD = pVu1LightDataDirectional->lmDiffuse[r];
        float *vS = pVu1LightDataDirectional->lmSpecular[r];

        for (int c = 0; c < 3; c++)
        {
            vD[c] = s_lmDiffuseLight[0][c] * mat[r][0] + s_lmDiffuseLight[1][c] * mat[r][1]
                  + s_lmDiffuseLight[2][c] * mat[r][2];
            vS[c] = s_lmSpecularLight[0][c] * mat[r][0] + s_lmSpecularLight[1][c] * mat[r][1]
                  + s_lmSpecularLight[2][c] * mat[r][2];
        }
        vD[3] = mat[r][3];
        vS[3] = mat[r][3];
    }
}

// Set the VU1 image's VF02 constant (scratchpad layout +0x20) when the layout
// exists. FF3-only: motion code swaps it around a model draw.
// FF3 0x001dc630.
void gra3dSetVu1ConstVF2(float *v)
{
    if (s_pScratchpadLayout != nullptr)
    {
        _CopyVector(s_pScratchpadLayout->Vu1Mem._vVF2, v);
    }
}

// FF3 0x001dc650.
void gra3dGetVu1ConstVF2(float *v)
{
    if (s_pScratchpadLayout != nullptr)
    {
        _CopyVector(v, s_pScratchpadLayout->Vu1Mem._vVF2);
    }
}

// Build a coordinate's VU1 light block. cp1 == null: the lights go into cp0's
// frame: matWork = transpose(cp0's local->world rotation), its rows
// normalised give matSpot (spot directions), and transposed back
// matDirectional (directional lights, and the image's matLocalWorld; the raw
// matrix goes to matLocalWorldNoNormalized). Otherwise both image matrices
// are the unit matrix and nothing is turned.
// FF3 0x001dc670; from MioPan gra3d.c.
void g3dSetVu1LightData(GRA3DVU1LIGHTDATA *pVu1LightData, SGDCOORDINATE *cp0, SGDCOORDINATE *cp1)
{
    GRA3DVU1MEMLAYOUT &rVu1Mem = s_pScratchpadLayout->Vu1Mem;

    if (cp1 == nullptr)
    {
        sceVu0FMATRIX matWork;
        sceVu0FMATRIX matDirectional;
        sceVu0FMATRIX matSpot;

        memcpy(matWork, cp0->matLocalWorld, sizeof(sceVu0FMATRIX));
        _CopyVector(matWork[3], s_matUnit[3]);              // vf0: drop the translation
        sceVu0TransposeMatrix(matWork, matWork);

        // PC: FF3 leaves row 3 uninitialised before the transpose (only the
        // w column of matLocalWorld sees it); the unit row is used here.
        for (int i = 0; i < 3; i++)
        {
            _Vu0NormalizeXYZW(matDirectional[i], matWork[i]);   // w is 0
        }
        _CopyVector(matDirectional[3], s_matUnit[3]);
        sceVu0TransposeMatrix(matDirectional, matDirectional);

        for (int i = 0; i < 3; i++)
        {
            _Vu0NormalizeXYZW(matSpot[i], matWork[i]);
        }
        _CopyVector(matSpot[3], s_matUnit[3]);              // vf0

        memcpy(rVu1Mem.matLocalWorldNoNormalized, cp0->matLocalWorld, sizeof(sceVu0FMATRIX));
        memcpy(rVu1Mem.matLocalWorld, matDirectional, sizeof(sceVu0FMATRIX));

        _SetVu1LightData_Directional(&pVu1LightData->dir, matDirectional);
        _SetVu1LightData_Point(&pVu1LightData->point);
        _SetVu1LightData_Spot(&pVu1LightData->spot, matSpot);
    }
    else
    {
        memcpy(rVu1Mem.matLocalWorldNoNormalized, s_matUnit, sizeof(sceVu0FMATRIX));
        memcpy(rVu1Mem.matLocalWorld, s_matUnit, sizeof(sceVu0FMATRIX));

        _SetVu1LightData_Directional(&pVu1LightData->dir, nullptr);
        _SetVu1LightData_Point(&pVu1LightData->point);
        _SetVu1LightData_Spot(&pVu1LightData->spot, nullptr);
    }

    s_Vu1LightImage = *pVu1LightData;                       // PC: for gra3dSnapshotVu1Lighting
}

// Put the spot / point light-type switches into the EOP GIF tag's spare
// words and send the tag to VU1 address 5.
// FF3 0x001dc920; from MioPan gra3d.c.
void gra3dVu1TransGTEOP(void)
{
    GRA3DVU1MEMLAYOUT &rVu1Mem = s_pScratchpadLayout->Vu1Mem;

    rVu1Mem.gtEOP[1] = (gra3dIsLightTypeEnable(G3DLIGHT_SPOT) != 0);
    rVu1Mem.gtEOP[2] = (gra3dIsLightTypeEnable(G3DLIGHT_POINT) != 0);
    g3dVif1Unpack(5, rVu1Mem.gtEOP, 1);
}

// The VU1 image's head (0x19 quadwords from VU1 address 0, behind its own
// UNPACK code) into the DMA chain.
// FF3 0x001dc970; from MioPan gra3d.c.
void SetVU1Header(void)
{
    gra3dSetVif1Code_Unpack(s_pScratchpadLayout->qwVif1Code0, 0, 0x19, 0x6c);
    g3dDmaCopyPacket(s_pScratchpadLayout, 0x1a);
}

// Whether any of the core's three slots of a type is on.
// FF3 0x001dc9a0; from MioPan gra3d.c.
int gra3dIsLightTypeEnable(int type)
{
    int iStart;

    if (type > G3DLIGHT_SPOT)
    {
        return 0;
    }
    switch (type)
    {
        case G3DLIGHT_DIRECTIONAL: iStart = 0; break;
        case G3DLIGHT_POINT:       iStart = 3; break;
        case G3DLIGHT_SPOT:        iStart = 6; break;
        default:                   iStart = INVALID_GRA3DLIGHTID; break;
    }

    if (g3dIsLightEnable(iStart) != 0)
    {
        return 1;
    }
    if (g3dIsLightEnable(iStart + 1) != 0)
    {
        return 1;
    }
    return g3dIsLightEnable(iStart + 2) != 0;
}

// Empty in FF3 (and FF2's retail path): light types are not switched.
// FF3 0x001dca38; from MioPan gra3d.c.
void gra3dEnableLightType(int type, int bEnable)
{
    (void)type;
    (void)bEnable;
}

// FF3 0x001dca40; from MioPan gra3d.c.
void gra3dSetAmbient(float *vAmbient)
{
    g3dSetAmbient(vAmbient);
}

// FF3 0x001dca58; from MioPan gra3d.c.
float *gra3dGetAmbientRef(void)
{
    return g3dGetAmbientRef();
}

// FF3 0x001dca70; from MioPan gra3d.c.
void gra3dSetLight(int iLightId, G3DLIGHT *pLight)
{
    s_aLight[iLightId] = *pLight;
}

// FF3 0x001dcac0; from MioPan gra3d.c.
G3DLIGHT *gra3dGetLightRef(int iLightId)
{
    return &s_aLight[iLightId];
}

// FF3 0x001dcae0; from MioPan gra3d.c.
void gra3dLightEnable(int iLightId, int bEnable)
{
    s_aLightStatus[iLightId].bEnable = bEnable;
}

// FF3 0x001dcaf8; from MioPan gra3d.c.
int gra3dIsLightEnable(int iLightId)
{
    return s_aLightStatus[iLightId].bEnable;
}

// Whether the prelight pass may use the light. FF3-only.
// FF3 0x001dcb10.
int gra3dIsLightEnableToPrelight(int iLightId)
{
    return s_aLightStatus[iLightId].bEnableToPrelight;
}

// FF3 0x001dcb28; from MioPan gra3d.c.
void gra3dSetLightStatus(int iLightId, GRA3DLIGHTSTATUS *pS)
{
    s_aLightStatus[iLightId] = *pS;
}

// FF3 0x001dcb80; from MioPan gra3d.c.
GRA3DLIGHTSTATUS *gra3dGetLightStatusRef(int iLightId)
{
    return &s_aLightStatus[iLightId];
}

// FF3 0x001dcb98; from MioPan gra3d.c.
void gra3dApplyLight(void)
{
    _UpdateLight_Directional();
    _UpdateLight_Point();
    _UpdateLight_Spot();
}

// The point or spot material cache of a light group (null for other types).
// FF3 0x001dcbc0; from MioPan gra3d.c.
GRA3DVU1MATERIALCACHE_POINT *g3dGetVu1MaterialCache(int type, int iIndex)
{
    if (type == G3DLIGHT_POINT)
    {
        return &s_Vu1Material.aVu1MaterialCache_Point[iIndex];
    }
    if (type == G3DLIGHT_SPOT)
    {
        return &s_Vu1Material.aVu1MaterialCache_Spot[iIndex];
    }
    return nullptr;
}

// dcol = col scaled so that its largest of r, g, b becomes 255; returns the
// divisor (max / 255, or 1 for black). The std::max_element is FF3's
// instance max_element<float const*> (FF3 0x002bc150).
// FF3 0x001dcbf8; from MioPan gra3d.c.
static float SetMaxColor255(float *dcol, float *col)
{
    float fDiv = *std::max_element(col, col + 3) * 0.0039215684f;  // sdata 0x370b7c (0x3b808080)

    if (fDiv == 0.0f)
    {
        fDiv = 1.0f;                                                // sdata 0x370b80
    }

    float q = 1.0f / fDiv;
    for (int c = 0; c < 4; c++)
    {
        dcol[c] = col[c] * q;
    }
    return fDiv;
}

// The directional material block: ambient = (global ambient * material
// ambient + emissive) * 128 for an SGD primitive (else 255), w 255; per
// enabled directional light its colours times the material's, scaled by the
// factors gra3dSetMaterial left in s_lmDiffuseColor[0].w /
// s_lmSpecularColor[1].w; lmDiffuse[0].w and lmSpecular[0].w carry the
// material alpha.
// FF3 0x001dcc90; from MioPan gra3d.c.
void gra3dCalcVu1MaterialDataDirectional(GRA3DVU1MATERIALDATA_DIRECTIONAL *pDirectionalData)
{
    GRA3DVU1MATERIALDATA_DIRECTIONAL &rData = s_Vu1Material.MaterialPacketDirectional.Data;
    float fAmbientScale = 128.0f;                                   // sdata 0x370b84
    float vWork[4];

    if (s_uiMaterialPrimType != 1)
    {
        fAmbientScale = 255.0f;                                     // sdata 0x370b88
    }

    G3DMATERIAL *pMat     = g3dGetMaterialRef();
    float       *vAmbient = gra3dGetAmbientRef();

    for (int c = 0; c < 4; c++)
    {
        vWork[c] = vAmbient[c] * pMat->vAmbient[c] + pMat->vEmissive[c];
    }
    _MakeColorToMonotone(vWork);
    for (int c = 0; c < 3; c++)
    {
        rData.vAmbient[c] = vWork[c] * fAmbientScale;
    }
    rData.vAmbient[3] = 255.0f;                                     // sdata 0x370b88

    for (int i = 0; i < 3; i++)
    {
        if (gra3dIsLightEnable(i) == 0)
        {
            memset(rData.lmDiffuse[i], 0, sizeof(sceVu0FVECTOR));
            memset(rData.lmSpecular[i], 0, sizeof(sceVu0FVECTOR));
        }
        else
        {
            for (int c = 0; c < 4; c++)
            {
                vWork[c] = s_lmDiffuseColor[i][c] * pMat->vDiffuse[c];
            }
            _MakeColorToMonotone(vWork);
            for (int c = 0; c < 4; c++)
            {
                rData.lmDiffuse[i][c] = vWork[c] * s_lmDiffuseColor[0][3];
            }

            for (int c = 0; c < 4; c++)
            {
                vWork[c] = s_lmSpecularColor[i][c] * pMat->vSpecular[c];
            }
            _MakeColorToMonotone(vWork);
            for (int c = 0; c < 4; c++)
            {
                rData.lmSpecular[i][c] = vWork[c] * s_lmSpecularColor[1][3];
            }
        }
    }

    rData.lmDiffuse[0][3]  = pMat->vDiffuse[3];
    rData.lmSpecular[0][3] = pMat->vDiffuse[3];

    s_Vu1MaterialDirImage = rData;                          // PC: for gra3dSnapshotVu1Lighting
    memcpy(pDirectionalData, &rData, sizeof(GRA3DVU1MATERIALDATA_DIRECTIONAL));
}

// The point / spot material block from that type's light-group cache: per
// column diffuse = cache diffuse * material diffuse * (192 | 255), brought to
// a max of 255 by SetMaxColor255 (the divisor goes into vPower with the
// range), specular = cache specular * material specular * (sum of the
// material specular) * (43 | 86) / divisor. The column -> light indices go
// to s_aMaterialCache[type].
// PC: FF3 keeps vWork in scratchpad 0x70003900 for the point type.
static void _CalcVu1MaterialDataPositional(int type, GRA3DVU1MATERIALDATA_POINT *pOut,
                                           GRA3DVU1MATERIALDATA_POINT &rData)
{
    G3DMATERIAL *pMat = g3dGetMaterialRef();
    float fSpecularSum = pMat->vSpecular[0] + pMat->vSpecular[1] + pMat->vSpecular[2];
    float fSpecularScale;
    float fDiffuseScale;
    float vWork[4];

    if (s_uiMaterialPrimType == 1)
    {
        fSpecularScale = 43.0f;                                     // sdata 0x370b90 / 0x370ba0
        fDiffuseScale  = 192.0f;                                    // sdata 0x370b8c / 0x370b9c
    }
    else
    {
        fSpecularScale = 86.0f;                                     // sdata 0x370b98 / 0x370ba8
        fDiffuseScale  = 255.0f;                                    // sdata 0x370b94 / 0x370ba4
    }

    GRA3DMATERIALINDEXCACHE &rIndex = s_Vu1Material.aMaterialCache[type];
    rIndex.bEnable = gra3dIsLightTypeEnable(type);

    for (int i = 0; i < gra3dIsLightTypeEnable(type); i++)
    {
        GRA3DVU1MATERIALCACHE_POINT *pCache = g3dGetVu1MaterialCache(type, i);

        for (int j = 0; j < 3; j++)
        {
            for (int c = 0; c < 4; c++)
            {
                vWork[c] = pCache->Data.lmDiffuse[j][c] * pMat->vDiffuse[c];
            }
            _MakeColorToMonotone(vWork);
            for (int c = 0; c < 4; c++)
            {
                vWork[c] = vWork[c] * fDiffuseScale;
            }
            float fDiv = SetMaxColor255(rData.lmDiffuse[j], vWork);

            for (int c = 0; c < 4; c++)
            {
                vWork[c] = pCache->Data.lmSpecular[j][c] * pMat->vSpecular[c];
            }
            _MakeColorToMonotone(vWork);
            float fScale = (fSpecularSum * fSpecularScale) / fDiv;
            for (int c = 0; c < 4; c++)
            {
                rData.lmSpecular[j][c] = vWork[c] * fScale;
            }

            rData.vPower[j]   = pCache->Data.vPower[j] * fDiv;
            rIndex.aiIndex[j] = pCache->Index.aiIndex[j];
        }
    }

    memcpy(pOut, &rData, sizeof(GRA3DVU1MATERIALDATA_POINT));
}

// callcheck: g3dGetMaterialRef, gra3dIsLightTypeEnable, g3dGetVu1MaterialCache, _MakeColorToMonotone, SetMaxColor255, memcpy
//   (the body is shared with the other type in _CalcVu1MaterialDataPositional)
// FF3 0x001dce88; from MioPan gra3d.c.
void gra3dCalcVu1MaterialDataPoint(GRA3DVU1MATERIALDATA_POINT *pPointData)
{
    _CalcVu1MaterialDataPositional(G3DLIGHT_POINT, pPointData, s_Vu1Material.MaterialPacketPoint.Data);
    s_Vu1MaterialPointImage = s_Vu1Material.MaterialPacketPoint.Data;   // PC: for gra3dSnapshotVu1Lighting
}

// callcheck: g3dGetMaterialRef, gra3dIsLightTypeEnable, g3dGetVu1MaterialCache, _MakeColorToMonotone, SetMaxColor255, memcpy
//   (the body is shared with the other type in _CalcVu1MaterialDataPositional)
// FF3 0x001dd0f0; from MioPan gra3d.c.
void gra3dCalcVu1MaterialDataSpot(GRA3DVU1MATERIALDATA_SPOT *pSpotData)
{
    _CalcVu1MaterialDataPositional(G3DLIGHT_SPOT, pSpotData, s_Vu1Material.MaterialPacketSpot.Data);
    s_Vu1MaterialSpotImage = s_Vu1Material.MaterialPacketSpot.Data;     // PC: for gra3dSnapshotVu1Lighting
}

// Make an SGD material current: mark it cached, keep its primitive type and
// light-index caches, set the colour scales the VU1 material builders use
// (diffuse 192 | 255 in s_lmDiffuseColor[0].w, specular sum * (43 | 86) in
// s_lmSpecularColor[1].w, 255 in s_lmSpecularColor[0].w) and hand the
// colours to the core.
// FF3 0x001dd308; from MioPan gra3d.c (no host colour-scale hook).
void gra3dSetMaterial(SGDMATERIAL *pMat)
{
    G3DMATERIAL mat;
    float       fSpecularScale = 86.0f;                             // sdata 0x370bb4

    pMat->iCacheStatus  = 1;
    s_uiMaterialPrimType = pMat->uiPrimType;
    memcpy(s_Vu1Material.aMaterialCache, pMat->aCache, sizeof(s_Vu1Material.aMaterialCache));

    float fSpecularSum = pMat->vSpecular[0] + pMat->vSpecular[1] + pMat->vSpecular[2];

    s_lmDiffuseColor[0][3] = 255.0f;                                // sdata 0x370bb8
    if (pMat->uiPrimType == 1)
    {
        fSpecularScale         = 43.0f;                             // sdata 0x370bb0
        s_lmDiffuseColor[0][3] = 192.0f;                            // sdata 0x370bac
    }
    s_lmSpecularColor[1][3] = fSpecularSum * fSpecularScale;
    s_lmSpecularColor[0][3] = 255.0f;                               // sdata 0x370bb8

    _CopyVector(mat.vDiffuse, pMat->vDiffuse);
    _CopyVector(mat.vAmbient, pMat->vAmbient);
    _CopyVector(mat.vSpecular, pMat->vSpecular);
    _CopyVector(mat.vEmissive, pMat->vEmission);
    mat.fPower = 1.0f;                                              // sdata 0x370bbc
    mat.aiPad[0] = mat.aiPad[1] = mat.aiPad[2] = 0;                 // PC: FF3 leaves them unset

    g3dSetMaterial(&mat);
}

// ──────────────────────────────────────────────────────────────────────
// Transforms
// ──────────────────────────────────────────────────────────────────────

// WORLDCLIP goes to the VU1 image and the core, WORLDSCREEN only to the VU1
// image, the rest only to the core.
// FF3 0x001dd420; from MioPan gra3d.c.
int gra3dSetTransform(int state, float (*mat)[4])
{
    if (state == GRA3DTS_WORLDCLIP)
    {
        memcpy(s_pScratchpadLayout->Vu1Mem.matWorldClip, mat, sizeof(sceVu0FMATRIX));
    }
    else if (state == GRA3DTS_WORLDSCREEN)
    {
        memcpy(s_pScratchpadLayout->Vu1Mem.matWorldScreen, mat, sizeof(sceVu0FMATRIX));
        return 1;
    }
    return g3dSetTransform(state, mat);
}

// FF3 0x001dd4b0; from MioPan gra3d.c.
float (*gra3dGetTransformRef(int state))[4]
{
    if (state == GRA3DTS_WORLDSCREEN)
    {
        return s_pScratchpadLayout->Vu1Mem.matWorldScreen;
    }
    return g3dGetTransformRef(state);
}

// The prelight vertex colour: vSrcColor (0..255) to 0..1, lit by the core,
// back to 0..255, then the monotone collapse.
// FF3 0x001dd4e8; from MioPan gra3d.c.
void gra3dCalcVertexColor(float *vDest, float *vVertex, float *vNormal, float *vSrcColor)
{
    float vFirst[4];

    for (int c = 0; c < 4; c++)
    {
        vFirst[c] = vSrcColor[c] * (1.0f / 255.0f);                 // sdata 0x370be0 (0x3b808081)
    }
    g3dCalcVertexColor(vDest, vVertex, vNormal, vFirst);
    for (int c = 0; c < 4; c++)
    {
        vDest[c] = vDest[c] * 255.0f;
    }
    _MakeColorToMonotone(vDest);
}

// ──────────────────────────────────────────────────────────────────────
// PC: the VU1 light image for the host renderer (MioPan gra3d.c port
// additions; no FF3 counterpart). See gra3d.h.
// ──────────────────────────────────────────────────────────────────────

// The current VU1 light and material images, one float4 per light: the light
// half from the last g3dSetVu1LightData, the material half from the last
// gra3dCalcVu1MaterialData*. The directional directions are gra3d's world-space
// columns, not the packet's (turned into the object's frame), so the host dots
// them with world normals. A disabled slot carries zero colours and power, so
// only the two light-type enables matter.
void gra3dSnapshotVu1Lighting(GRA3DVU1LIGHTSNAPSHOT *out)
{
    if (out == nullptr)
    {
        return;
    }
    memset(out, 0, sizeof(*out));

    out->aiConfig[0] = ((gra3dIsLightTypeEnable(G3DLIGHT_SPOT) != 0) ? 1 : 0)
                     | ((gra3dIsLightTypeEnable(G3DLIGHT_POINT) != 0) ? 2 : 0);
    out->aiConfig[1] = 1;
    _CopyVector(out->vAmbient, s_Vu1MaterialDirImage.vAmbient);

    for (int i = 0; i < 3; i++)
    {
        out->vSpotBTimes[i]  = s_Vu1MaterialSpotImage.vPower[i];
        out->vPointBTimes[i] = s_Vu1MaterialPointImage.vPower[i];
        out->vSpotIntens[i]  = s_Vu1LightImage.spot.vIntens[i];
        out->vSpotIntensB[i] = s_Vu1LightImage.spot.vIntensB[i];

        // s_lmDiffuseLight / s_lmSpecularLight hold one light per column.
        for (int c = 0; c < 3; c++)
        {
            out->avDirLightDif[i][c] = s_lmDiffuseLight[c][i];
            out->avDirLightSpc[i][c] = s_lmSpecularLight[c][i];
        }

        _CopyVector(out->avDirColDif[i], s_Vu1MaterialDirImage.lmDiffuse[i]);
        _CopyVector(out->avDirColSpc[i], s_Vu1MaterialDirImage.lmSpecular[i]);
        _CopyVector(out->avSpotPos[i], s_Vu1LightImage.spot.lmPosition[i]);
        _CopyVector(out->avSpotDir[i], s_Vu1LightImage.spot.lmDirection[i]);
        _CopyVector(out->avSpotColDif[i], s_Vu1MaterialSpotImage.lmDiffuse[i]);
        _CopyVector(out->avSpotColSpc[i], s_Vu1MaterialSpotImage.lmSpecular[i]);
        _CopyVector(out->avPointPos[i], s_Vu1LightImage.point.lmPosition[i]);
        _CopyVector(out->avPointColDif[i], s_Vu1MaterialPointImage.lmDiffuse[i]);
        _CopyVector(out->avPointColSpc[i], s_Vu1MaterialPointImage.lmSpecular[i]);
    }

    // The vertex alpha is DIRCOLDIF[0].w, which on the PS2 comes from the last
    // material packet uploaded. The PC draws each mesh as the walker reaches
    // it and a cached material block skips the rebuild, so the packet may
    // describe another material: take the live one's alpha (MioPan measured
    // whole body parts vanishing on a stale 0).
    out->avDirColDif[0][3] = g3dGetMaterialRef()->vDiffuse[3];
}

// The point / spot derivation of _SetVu1LightData_Point / _Spot and
// _CalcVu1MaterialDataPositional run over gra3d's whole bank of that type,
// not the three slots _SelectLightByType chose: the fragment lighting mode
// takes every enabled light. Over iMaxLanes, the strongest by power over
// distance to vRefPos survive. Returns the lane count.
int gra3dCalcVu1WideLanes(GRA3DVU1LANE *aLanes, int iMaxLanes, int type, const float *vRefPos)
{
    int iBase;
    int iNum;

    if (aLanes == nullptr || iMaxLanes <= 0)
    {
        return 0;
    }
    if (type == G3DLIGHT_POINT)
    {
        iBase = GRA3D_START_LIGHT_POINT;
        iNum  = GRA3D_NUM_LIGHT_POINT;
    }
    else if (type == G3DLIGHT_SPOT)
    {
        iBase = GRA3D_START_LIGHT_SPOT;
        iNum  = GRA3D_NUM_LIGHT_SPOT;
    }
    else
    {
        return 0;
    }

    G3DMATERIAL *pMat = g3dGetMaterialRef();
    float fSpecularSum = pMat->vSpecular[0] + pMat->vSpecular[1] + pMat->vSpecular[2];
    float fSpecularScale = (s_uiMaterialPrimType == 1) ? 43.0f : 86.0f;
    float fDiffuseScale  = (s_uiMaterialPrimType == 1) ? 192.0f : 255.0f;

    auto rank = [vRefPos](const G3DLIGHT *pL)
    {
        float d2 = 0.0f;
        for (int c = 0; c < 3; c++)
        {
            float d = pL->vPosition[c] - vRefPos[c];
            d2 += d * d;
        }
        float d = sqrtf(d2);
        return (pL->vDiffuse[0] + pL->vDiffuse[1] + pL->vDiffuse[2]) * pL->fMaxRange / (d > 0.0f ? d : 1.0f);
    };

    int iCount = 0;
    for (int i = 0; i < iNum; i++)
    {
        int iId = iBase + i;
        GRA3DVU1LANE *pLane;
        float vWork[4];

        if (gra3dIsLightEnable(iId) == 0)
        {
            continue;
        }
        G3DLIGHT *pL = gra3dGetLightRef(iId);

        if (iCount >= iMaxLanes)
        {
            float fThisPower = rank(pL);
            int   iWeakest   = 0;

            for (int j = 1; j < iMaxLanes; j++)
            {
                if (aLanes[j].fRank < aLanes[iWeakest].fRank)
                {
                    iWeakest = j;
                }
            }
            if (fThisPower <= aLanes[iWeakest].fRank)
            {
                continue;
            }
            pLane = &aLanes[iWeakest];
            pLane->fRank = fThisPower;
        }
        else
        {
            pLane = &aLanes[iCount++];
            pLane->fRank = rank(pL);
        }

        _CopyVector(pLane->vPosition, pL->vPosition);
        if (type == G3DLIGHT_SPOT)
        {
            _CopyVector(pLane->vDirection, pL->vDirection);
            pLane->vParams[1] = pL->afPad0[0];
            pLane->vParams[2] = 1.0f / (1.0f - pL->afPad0[0]);
        }
        else
        {
            memset(pLane->vDirection, 0, sizeof(pLane->vDirection));
            pLane->vParams[1] = 0.0f;
            pLane->vParams[2] = 0.0f;
        }

        for (int c = 0; c < 4; c++)
        {
            vWork[c] = pL->vDiffuse[c] * pMat->vDiffuse[c];
        }
        _MakeColorToMonotone(vWork);
        for (int c = 0; c < 4; c++)
        {
            vWork[c] *= fDiffuseScale;
        }
        float fDiv = SetMaxColor255(pLane->vColDif, vWork);

        for (int c = 0; c < 4; c++)
        {
            vWork[c] = pL->vSpecular[c] * pMat->vSpecular[c];
        }
        _MakeColorToMonotone(vWork);
        float fScale = (fSpecularSum * fSpecularScale) / fDiv;
        for (int c = 0; c < 4; c++)
        {
            pLane->vColSpc[c] = vWork[c] * fScale;
        }

        pLane->vParams[0] = pL->fMaxRange * fDiv;
        pLane->vParams[3] = 0.0f;
        pLane->vColDif[3] = 0.0f;
        pLane->vColSpc[3] = 0.0f;
    }

    for (int i = iCount; i < iMaxLanes; i++)
    {
        memset(&aLanes[i], 0, sizeof(aLanes[i]));
    }
    return iCount;
}

// The VU1's three lighting kernels on the CPU (MioPan's transcription of the
// FF2 microcode, vu1/ff2_00.vsm: directional from PLOOP_TYPE2, spot from
// CalcIntens, point from CalcPoint). vVertex / vNormal are local and go
// through GRA3DTS_WORLD (the upper 3x3 applied to the normal, then
// normalised). vSrcColor and the result are GS 0..255 units; a null source
// seeds from black, as a realtime VUVN packet carries no colour. Not the
// prelight law of gra3dCalcVertexColor, which only the bake uses.
void gra3dCalcVu1VertexColor(float *vDest, const GRA3DVU1LIGHTSNAPSHOT *snap, const float *vVertex,
                             const float *vNormal, const float *vSrcColor)
{
    float (*matWorld)[4] = gra3dGetTransformRef(GRA3DTS_WORLD);
    float vPos[3];
    float vNrm[3];
    float acc[3];

    for (int c = 0; c < 3; c++)
    {
        vPos[c] = vVertex[0] * matWorld[0][c] + vVertex[1] * matWorld[1][c]
                + vVertex[2] * matWorld[2][c] + matWorld[3][c];
        vNrm[c] = vNormal[0] * matWorld[0][c] + vNormal[1] * matWorld[1][c]
                + vNormal[2] * matWorld[2][c];
    }
    float fLen2 = vNrm[0] * vNrm[0] + vNrm[1] * vNrm[1] + vNrm[2] * vNrm[2];
    float q     = (fLen2 > 0.0f) ? (1.0f / sqrtf(fLen2)) : 0.0f;
    for (int c = 0; c < 3; c++)
    {
        vNrm[c] *= q;
    }

    // The seed, greyed when monotone draw is on (MioPan: the PS2 had grey
    // light colours and a re-baked room instead; the same result without
    // depending on the re-bake).
    if (vSrcColor != nullptr)
    {
        float vSeed[4];

        _CopyVector(vSeed, vSrcColor);
        _MakeColorToMonotone(vSeed);
        for (int c = 0; c < 3; c++)
        {
            acc[c] = vSeed[c];
        }
    }
    else
    {
        acc[0] = acc[1] = acc[2] = 0.0f;
    }
    for (int c = 0; c < 3; c++)
    {
        acc[c] += snap->vAmbient[c];
    }

    // Directional: fixed eighth power against the frame's half vector.
    for (int i = 0; i < 3; i++)
    {
        float nd = snap->avDirLightDif[i][0] * vNrm[0] + snap->avDirLightDif[i][1] * vNrm[1]
                 + snap->avDirLightDif[i][2] * vNrm[2];
        float ns = snap->avDirLightSpc[i][0] * vNrm[0] + snap->avDirLightSpc[i][1] * vNrm[1]
                 + snap->avDirLightSpc[i][2] * vNrm[2];

        nd = nd < 0.0f ? 0.0f : nd;
        ns = ns < 0.0f ? 0.0f : ns;
        ns = ns * ns;
        ns = ns * ns;
        ns = ns * ns;
        for (int c = 0; c < 3; c++)
        {
            acc[c] += snap->avDirColDif[i][c] * nd + snap->avDirColSpc[i][c] * ns;
        }
    }

    if (snap->aiConfig[0] & 1)                                  // CalcIntens
    {
        for (int i = 0; i < 3; i++)
        {
            float L[3];

            for (int c = 0; c < 3; c++)
            {
                L[c] = snap->avSpotPos[i][c] - vPos[c];
            }
            float len2 = L[0] * L[0] + L[1] * L[1] + L[2] * L[2];
            if (len2 <= 0.0f)
            {
                continue;
            }
            float invLen2 = 1.0f / len2;

            // The microcode's sense: vDirection dotted with light - vertex, so
            // the cone opens along -vDirection. FF3's realtime spot installs
            // deliver the anti-beam vector (MapDrawSetRoomLight negates the
            // flashlight, FodSetSpotLights stores light - interest), and
            // _IsBBLightingupSpot's `< 0` binds that side. MioPan negates
            // here instead (its installs keep the beam); doing both lit the
            // half behind Rei (open-items.md "The flashlight lit behind Rei").
            float coneDot = snap->avSpotDir[i][0] * L[0] + snap->avSpotDir[i][1] * L[1]
                          + snap->avSpotDir[i][2] * L[2];
            coneDot = coneDot < 0.0f ? 0.0f : coneDot;
            float cone = coneDot * coneDot * invLen2 - snap->vSpotIntens[i];
            cone = cone < 0.0f ? 0.0f : cone;
            cone = cone * snap->vSpotIntensB[i];

            float coef = vNrm[0] * L[0] + vNrm[1] * L[1] + vNrm[2] * L[2];
            coef = coef < 0.0f ? 0.0f : coef;
            coef = coef * snap->vSpotBTimes[i] * invLen2;
            coef = coef > 1.0f ? 1.0f : coef;

            float e = coef * coef;
            e = e * e;
            e = e * e;
            for (int c = 0; c < 3; c++)
            {
                acc[c] += snap->avSpotColDif[i][c] * (coef * cone) + snap->avSpotColSpc[i][c] * cone * e;
            }
        }
    }

    if (snap->aiConfig[0] & 2)                                  // CalcPoint
    {
        for (int i = 0; i < 3; i++)
        {
            float L[3];
            float Ldot[3];

            for (int c = 0; c < 3; c++)
            {
                L[c] = snap->avPointPos[i][c] - vPos[c];
            }
            float len2 = L[0] * L[0] + L[1] * L[1] + L[2] * L[2];
            if (len2 <= 0.0f)
            {
                continue;
            }
            float invLen2 = 1.0f / len2;

            // The microcode's transpose bug (FF2, kept by MioPan): lane 2 reads
            // light 1's y.
            Ldot[0] = L[0];
            Ldot[1] = (i == 2) ? (snap->avPointPos[1][1] - vPos[1]) : L[1];
            Ldot[2] = L[2];

            float coef = vNrm[0] * Ldot[0] + vNrm[1] * Ldot[1] + vNrm[2] * Ldot[2];
            coef = coef < 0.0f ? 0.0f : coef;
            coef = coef * snap->vPointBTimes[i] * invLen2;
            coef = coef > 1.0f ? 1.0f : coef;

            float e = coef * coef;
            e = e * e;
            for (int c = 0; c < 3; c++)
            {
                acc[c] += snap->avPointColDif[i][c] * coef + snap->avPointColSpc[i][c] * e;
            }
        }
    }

    // The microcode's one clamp, against GLOBALAMBIENT.w (255).
    for (int c = 0; c < 3; c++)
    {
        float v = acc[c];

        v = v < 0.0f ? 0.0f : v;
        v = v > snap->vAmbient[3] ? snap->vAmbient[3] : v;
        vDest[c] = v;
    }
    vDest[3] = snap->avDirColDif[0][3];     // the material alpha (0..128), not lit
}

// ──────────────────────────────────────────────────────────────────────
// Scratchpad and init
// ──────────────────────────────────────────────────────────────────────

// Function-local statics: the homes of the three scratchpad objects while
// they are out of scratchpad.
// FF3 0x002bc190 (G3DCOREOBJECT), FF3 0x002bc1a0 (GRA3DSCRATCHPADLAYOUT),
// FF3 0x002bc1b0 (GRA3DSCRATCHPADLAYOUT_MAPSHADOW); FF2 GetStaticInstance<T>.
template <class T>
static T *GetStaticInstance(void)
{
    static T obj;
    return &obj;
}

// Reset the VU1 image to its default (and the fog bit).
// FF3 0x001dd540; from MioPan gra3d.c.
static void _ModifyGra3dScratchpadDefault(void)
{
    memcpy(s_pScratchpadLayout, s_aScratchpadLayoutDefault, sizeof(GRA3DSCRATCHPADLAYOUT));
    _ModifyFogParam();
}

// Move the core object, the VU1 image and the shadow image into scratchpad
// (s_bUseScratchpad) or back to their static homes, and re-init their owners
// on the current copy.
// callcheck: GetStaticInstance
//   (called as GetStaticInstance<T>(), which the parser does not see)
// FF3 0x001dd568; from MioPan gra3d.c (FF3 sizes: core 0xd70 at 0x700005e0).
static void _ModifyScratchpad(void)
{
    G3DCREATIONDATA         g3dCD;
    GRA3DSHADOWCREATIONDATA gra3dshadowCD;

    G3DCOREOBJECT *pCore    = GetStaticInstance<G3DCOREOBJECT>();
    G3DCOREOBJECT *pCoreSpr = (G3DCOREOBJECT *)ReiPan_GetHostPointer(GRA3D_SPR_COREOBJECT);
    if (s_bUseScratchpad == 0)
    {
        memcpy(pCore, pCoreSpr, sizeof(G3DCOREOBJECT));
        g3dCD.pObj = pCore;
    }
    else
    {
        memcpy(pCoreSpr, pCore, sizeof(G3DCOREOBJECT));
        g3dCD.pObj = pCoreSpr;
    }
    g3dInitialize(&g3dCD);

    GRA3DSCRATCHPADLAYOUT *pSL    = GetStaticInstance<GRA3DSCRATCHPADLAYOUT>();
    GRA3DSCRATCHPADLAYOUT *pSLSpr = (GRA3DSCRATCHPADLAYOUT *)ReiPan_GetHostPointer(GRA3D_SPR_LAYOUT);
    if (s_bUseScratchpad == 0)
    {
        memcpy(pSL, pSLSpr, sizeof(GRA3DSCRATCHPADLAYOUT));
        s_pScratchpadLayout = pSL;
    }
    else
    {
        memcpy(pSLSpr, pSL, sizeof(GRA3DSCRATCHPADLAYOUT));
        s_pScratchpadLayout = pSLSpr;
        _ModifyGra3dScratchpadDefault();
    }

    GRA3DSCRATCHPADLAYOUT_MAPSHADOW *pMS    = GetStaticInstance<GRA3DSCRATCHPADLAYOUT_MAPSHADOW>();
    GRA3DSCRATCHPADLAYOUT_MAPSHADOW *pMSSpr =
        (GRA3DSCRATCHPADLAYOUT_MAPSHADOW *)ReiPan_GetHostPointer(GRA3D_SPR_MAPSHADOW);
    if (s_bUseScratchpad == 0)
    {
        memcpy(pMS, pMSSpr, sizeof(GRA3DSCRATCHPADLAYOUT_MAPSHADOW));
        gra3dshadowCD.pSL = pMS;
    }
    else
    {
        memcpy(pMSSpr, pMS, sizeof(GRA3DSCRATCHPADLAYOUT_MAPSHADOW));
        gra3dshadowCD.pSL = pMSSpr;
    }
    gra3dshadowInit(&gra3dshadowCD);
}

// FF3 0x001dd688; from MioPan gra3d.c.
void gra3dUseScratchpad(int b)
{
    if (b != s_bUseScratchpad)
    {
        s_bUseScratchpad = b;
        _ModifyScratchpad();
    }
}

// Two parameters _CalcValidLightIndexByType<4> reads (GameSystemStart sets
// (0, 1.3), as gra3dInit). Provisional names.
// FF3 0x001dd6b8.
void gra3dSetDrawParam(int i, float f)
{
    s_iDrawParam = i;
    s_fDrawParam = f;
}

// FF3 0x001dd6c8.
float gra3dGetDrawParamF(void)
{
    return s_fDrawParam;
}

// FF3 0x001dd6d0.
int gra3dGetDrawParamI(void)
{
    return s_iDrawParam;
}

// Bring gra3d up: scratchpad objects, VIF1, camera, lights, fog, clip
// rectangle, the SGD work buffer (pPacket, iSize bytes), VU0 constants, and
// the IG3DCompatible / CVu0Matrix hooks.
// FF3 0x001dd6d8; from MioPan gra3d.c (FF3 adds gra3dUseScratchpad(1),
// gra3dSetDrawParam and Vu0InitConstRegisters; no atexit, no debug check).
void gra3dInit(void *pPacket, int iSize)
{
    GRA3DSGDCREATIONDATA gra3dsgdCD;

    _ModifyScratchpad();

    g3dVif1Init();
    g3dVif1SetRegister(s_aVif1CmdData, 6);

    _InitCamera();
    _InitLight();
    _InitFog();
    _SetClipValue(-1.0f, 1.0f, -1.0f, 1.0f);                // sdata 0x370bc0 / 0x370bc4

    gra3dUseScratchpad(1);
    gra3dSetDrawParam(0, 1.3f);                             // sdata 0x370bc8

    gra3dsgdCD.vnarray = pPacket;
    gra3dsgdCD.size    = (u_int)iSize >> 4;
    gra3dsgdInit(&gra3dsgdCD);

    Vu0InitConstRegisters();

    IG3DCompatible::s_pFuncSetGsRegister    = _Gra3dSetGsRegisterForAutoState;
    IG3DCompatible::s_pFuncSetGsRegisters   = _Gra3dSetGsRegistersForAutoState;
    IG3DCompatible::s_pFuncGetGsRegisterRef = _Gra3dGetGsRegisterRefForAutoState;
    IG3DCompatible::s_pFuncSetTransform     = gra3dSetTransform;
    IG3DCompatible::s_pFuncGetTransformRef  = gra3dGetTransformRef;

    CVu0Matrix::s_pFuncLoadMatrix              = _Vu0LoadMatrix_4_5_6_7;
    CVu0Matrix::s_pFuncApplyMatrixWithoutTrans = _Vu0ApplyMatrixWithoutTrans_4_5_6_7;
}

// IG3DCompatible's GS register hooks while gra3d is up: straight to the core.
// FF3 0x001dd808; FF2 _Gra3dSetGsRegisterForAutoState.
static int _Gra3dSetGsRegisterForAutoState(u_long data, int reg, int mode)
{
    return g3dSetGsRegister(data, reg, mode);
}

// FF3 0x001dd820; FF2 _Gra3dSetGsRegistersForAutoState.
static int _Gra3dSetGsRegistersForAutoState(const GS_REG_SET *regs, int num, int mode)
{
    return g3dSetGsRegisters(regs, num, mode);
}

// FF3 0x001dd838.
static u_long *_Gra3dGetGsRegisterRefForAutoState(int reg)
{
    return g3dGetGsRegisterRef(reg);
}

// gra3d.c's static initialisation: nothing to construct (FF3's
// __static_initialization_and_destruction_0 is an empty body, called by its
// .ctors entry gra3d_GLOBAL_I with (1, 0xffff)). FF3 0x001dd850, FF3 0x001dd890.

// Load vf4..vf7 (PC: the s_matVu0_4567 mirror).
// FF3 0x001dd858; from MioPan gra3d.c.
static void _Vu0LoadMatrix_4_5_6_7(float (*mat)[4])
{
    memcpy(s_matVu0_4567, mat, sizeof(s_matVu0_4567));
}

// vDest = vSrc.xyz through vf4..vf6 plus vf7 (vSrc's w is not used).
// FF3 0x001dd870; from MioPan gra3d.c.
static void _Vu0ApplyMatrixWithoutTrans_4_5_6_7(float *vDest, float *vSrc)
{
    float v[4];

    for (int c = 0; c < 4; c++)
    {
        v[c] = vSrc[0] * s_matVu0_4567[0][c] + vSrc[1] * s_matVu0_4567[1][c]
             + vSrc[2] * s_matVu0_4567[2][c] + s_matVu0_4567[3][c];
    }
    _CopyVector(vDest, v);
}
