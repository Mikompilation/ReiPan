/* ==========================================================================
 *  graphics/graph3d/gra3dSGDData.h
 *
 *  The SGD data layer (FF2 gra3dSGDData.c): sgdRemap turns a loaded model's
 *  file offsets into EE addresses (sgdRemapInverse back), the bone tree's
 *  local->world matrices, the room light file, process-unit lookup and the
 *  model's bounding box.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_GRA3DSGDDATA_H
#define _GRAPHICS_GRAPH3D_GRA3DSGDDATA_H

#include <stddef.h>                             // offsetof

#include "graphics/graph3d/sgd_types.h"         // SGDFILEHEADER, SGDCOORDINATE, SGDPROCUNITHEADER
#include "graphics/graph3d/gra3d.h"             // GRA3DLIGHTDATA

// A room's light file as loaded (FF2 ZERO2LIGHTDATAFILE, FF3 size): the
// editor header and light names, then the GRA3DLIGHTDATA sgdVerifyLightData
// copies out (FF3 reads it at +0x4f0, 0x1610 bytes).
struct ZERO2LIGHTDATAFILE               /* 0x1b00 */
{
    /* 0x0000 */ int            iSignature;             // FF2, unverified (FF3 does not test it)
    /* 0x0004 */ int            iSizeOfThisFile;        // FF2, unverified
    /* 0x0008 */ int            aiPad[2];
    /* 0x0010 */ char           astrLightName[NUM_GRA3DLIGHTID][32];   // FF2, unverified
    /* 0x04f0 */ GRA3DLIGHTDATA LD;
};
static_assert(sizeof(ZERO2LIGHTDATAFILE) == 0x1b00, "ZERO2LIGHTDATAFILE is 0x1b00 bytes in FF3");
static_assert(offsetof(ZERO2LIGHTDATAFILE, LD) == 0x4f0, "ZERO2LIGHTDATAFILE.LD");

void sgdEnableOptimizeTexture(int b);                       // FF3 0x001e4538 (nothing calls it)

// Offsets -> EE addresses (once: ucMapFlag), and back.
void sgdRemap(SGDFILEHEADER *pSGDHead);                     // FF3 0x001e3c28
void sgdRemapInverse(SGDFILEHEADER *pSGDHead);              // FF3 0x001e42e8

// Invalidate every material's cached VU1 packet (iCacheStatus); FF3 0x001e3f68.
void sgdResetMaterialCache(SGDFILEHEADER *pSGDData);

// Bone matrices: matLocalWorld = matCoord x the parent's, parents first.
void sgdCalcBoneCoordinate(SGDCOORDINATE *pCoord, int iNumBlock);          // FF3 0x001e3fa8
void sgdCalcCoordinate(SGDFILEHEADER *pSGDData, float (*matLocalWorld)[4]); // FF3 0x001e4018
void sgdCalcCoordinateMatrix(SGDCOORDINATE *pCoord);                       // FF3 0x001e4070

// Clear bCalc on a bone (and its parents) so the next calc redoes it.
void sgdClearCoordCalcFlgParents(SGDCOORDINATE *pCoord);                   // FF3 0x001e3850
void sgdClearCoordCalcFlgParentsId(void *pSGDData, int bone_no);          // FF3 0x001e41a8
void sgdClearCoordCalcFlg(void *pSGDData, int bone_no);                   // FF3 0x001e41d0
void sgdClearCoordCalcFlgCoord(SGDCOORDINATE *pCoord);                     // FF3 0x001e41e8

// Fix up a room light file's lights and copy its light set to pRet.
void sgdVerifyLightData(GRA3DLIGHTDATA *pRet, ZERO2LIGHTDATAFILE *pZLD);  // FF3 0x001e40f8

// The iUnitIndex-th unit of category iProcUnitId in a chain, or null.
SGDPROCUNITHEADER *sgdGetProcUnit(SGDPROCUNITHEADER *pPUHead, int iProcUnitId, int iUnitIndex); // FF3 0x001e41f0
// The 8 corners enclosing every bounding-box unit (and the origin).
void sgdGetBoundingBox(SGDFILEHEADER *pFH, float (*avBB)[4]);             // FF3 0x001e4230

#endif /* _GRAPHICS_GRAPH3D_GRA3DSGDDATA_H */
