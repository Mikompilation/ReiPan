/* ==========================================================================
 *  graphics/graph3d/gra3dSGDData.c
 *
 *  The SGD data layer. sgdRemap runs once on a loaded model (ucMapFlag):
 *  the header's coordinate / material / vector-info offsets, the block heads
 *  and every unit's self-relative pNext become absolute EE addresses, a
 *  coordinate's parent index a parent address, a material unit's index its
 *  SGDMATERIAL's address, and a realtime VUVN unit's vertex / normal indices
 *  the addresses of their quadwords (the weighted pools, or gra3dSGD.c's
 *  global blend buffers when the model is skinned through vertex lists).
 *  sgdRemapInverse undoes the header, coordinate, material and chain parts.
 *  Also: the bone tree's matLocalWorld (parents first, bCalc marks done),
 *  the room light file check, process-unit lookup and the model's box.
 *
 *  Ported from MioPan's gra3dSGDData.c (FF2 0x1bbb50..0x1bd460) and checked
 *  function by function against FF3 0x1e3850..0x1e4760 (+ linkonce
 *  0x2bcb80..0x2bcbc0). FF3 changes: no asserts (the bone-id checks with
 *  their endless loops are gone), no sgdGetLocalWorldMatrix and no
 *  sgdClearCoordCalcFlgAll loop, a bCalc clear for one SGDCOORDINATE
 *  (sgdClearCoordCalcFlgCoord), the box merge out of line
 *  (sgdExpandBoundingBox), 0x1610-byte light sets, and sgdRemapInverse no
 *  longer walks the mesh units.
 *
 *  PC: SGD data lives in emulated EE memory and every pointer field is a
 *  u_int EE address (sgd_types.h SgdPtr / SgdAddr), as the PS2 stored it, so
 *  the walkers in gra3dSGD.c follow what sgdRemap writes. MioPan's
 *  self-relative SGDSELF32 fields are not used.
 * ======================================================================== */

#include "graphics/graph3d/gra3dSGDData.h"

#include <stdio.h>                              // printf
#include <string.h>                             // memset, memcpy

#include "graphics/graph3d/g3dCore.h"           // g3dSetGsRegister
#include "graphics/graph3d/g3dDma.h"            // g3dDmaFlush
#include "graphics/graph3d/g3dLight.h"          // G3DLIGHT_POINT
#include "graphics/graph3d/gra3dSGD.h"          // _GetGlobal*Buffer*, _SetPREVIOUSTRI2PRIM, gra3dCalcVRAMPageSize
#include "graphics/graph3d/gra3dTRI2.h"         // gra3dLoadTRI2FileToVRAM, gra3dGetTRI2SizeData, gra3dGenerateTRI2FileFromVRAM
#include "sdk/libvu0.h"                         // sceVu0CopyMatrix, sceVu0MulMatrix, sceVu0CopyVector

#include "reipan/rendering/reipan_graph3d.h"    // PC: ReiPan_Graph3dInvalidateSkinCache

// Offsets below this are file offsets, not yet addresses (FF2 SGD_REMAP_BORDER).
#define SGD_REMAP_BORDER    0x30000000u

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static int s_bEnableOptimizeTexture;            // sdata 0x370cb0 (gp-0x2840)

// data 0x329530: a point light's direction.
static const sceVu0FVECTOR s_v0000 = { 0.0f, 0.0f, 0.0f, 0.0f };

// ──────────────────────────────────────────────────────────────────────
// Bone flags
// ──────────────────────────────────────────────────────────────────────

// Clear bCalc on pCoord and every parent above it.
// FF3 0x001e3850; from MioPan gra3dSGDData.c (FF2's static helper; FF3 tests
// the -1 root marker directly).
void sgdClearCoordCalcFlgParents(SGDCOORDINATE *pCoord)
{
    if (pCoord->pParent != 0)
    {
        if (pCoord->pParent == 0xffffffffu)
        {
            pCoord->bCalc = 0;
            return;
        }
        sgdClearCoordCalcFlgParents(SgdPtr<SGDCOORDINATE>(pCoord->pParent));
    }
    pCoord->bCalc = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Remap helpers
// ──────────────────────────────────────────────────────────────────────

// Repack a TRI2 unit's textures through GS memory (more than two textures
// only, and only when the repacked files fit before the next unit). The
// switch is never turned on in FF3 (sgdEnableOptimizeTexture has no callers).
// FF3 0x001e3890; from MioPan gra3dSGDData.c.
static void RebuildTRI2Files(SGDPROCUNITHEADER *pPUHead)
{
    if (!s_bEnableOptimizeTexture)
    {
        return;
    }

    SGDTEXTUREIMAGEDESC &rTexDesc = pPUHead->TexDesc;
    TRI2SIZEDATA         TRI2SizeData;

    memset(&TRI2SizeData, 0, sizeof(TRI2SizeData));
    TRI2SizeData.uiMinAddress = 0xffffffffu;

    if (rTexDesc.iNumTexture <= 2)
    {
        return;
    }

    _SetPREVIOUSTRI2PRIM(nullptr);
    SGDTRI2FILEHEADER *pTRI2Head =
        (SGDTRI2FILEHEADER *)((u_char *)pPUHead + rTexDesc.iPaddingSize + 0x10);
    gra3dLoadTRI2FileToVRAM(rTexDesc.iNumTexture, pTRI2Head, 1);

    for (int i = 0; i < rTexDesc.iNumTexture; i++)
    {
        u_int nQwc = pTRI2Head->GetTRI2Size();       // DIRECT's quadword count
        gra3dGetTRI2SizeData(&TRI2SizeData, &TRI2SizeData, pTRI2Head);
        pTRI2Head = (SGDTRI2FILEHEADER *)((u_char *)pTRI2Head + nQwc * 0x10 + 0x10);
    }

    g3dSetGsRegister(0, 0x3f, 1);               // TEXFLUSH
    g3dDmaFlush();
    TRI2SizeData.uiPageSize = gra3dCalcVRAMPageSize(TRI2SizeData.uiMaxAddress - TRI2SizeData.uiMinAddress);

    // Room before the next unit (pNext is an address by now; unsigned, as FF3).
    if (TRI2SizeData.uiPageSize * 0x2000 + 0x180 <= pPUHead->pNext - SgdAddr(pPUHead))
    {
        rTexDesc.iNumTexture = gra3dGenerateTRI2FileFromVRAM(
            (SGDTRI2FILEHEADER *)((u_char *)pPUHead + rTexDesc.iPaddingSize + 0x10), &TRI2SizeData);
    }
}

// Coordinate units hold nothing to remap.
// FF3 0x001e39c8; from MioPan gra3dSGDData.c.
static void MappingCoordinateData(SGDPROCUNITHEADER *pPUHead, SGDFILEHEADER *pSGDHead)
{
    (void)pPUHead;
    (void)pSGDHead;
}

// Preset (pre-lit) VUVN data holds no addresses.
// FF3 0x001e39d0; from MioPan gra3dSGDData.c.
static void MappingVUVNDataPreset(SGDPROCUNITHEADER *pVUVN, int mtype, int gloops, int hsize)
{
    (void)pVUVN;
    (void)mtype;
    (void)gloops;
    (void)hsize;
}

// A realtime VUVN unit: each _VECTORADDRESS (after the 0x20-byte VIF header,
// pPUHead + 3) holds a vertex and a normal index on disc and their EE
// addresses after this. Vector type 0: the unique pool; 1: nothing; 2 / 3
// (weighted): the weighted pool (two quadwords per entry), or the global
// blend buffers when the model has vertex lists.
// FF3 0x001e39d8; from MioPan gra3dSGDData.c.
static void MappingVUVNData(SGDPROCUNITHEADER *pPUHead, SGDFILEHEADER *pSGDHead)
{
    PHEAD          *pVectorInfo = SgdPtr<PHEAD>(pSGDHead->pVectorInfo);
    SGDVUVNDESC    &rVUVNDesc   = pPUHead->VUVNDesc;
    _VECTORADDRESS *pVA         = (_VECTORADDRESS *)&pPUHead[3];
    u_int           uiVertex;
    u_int           uiNormal;
    u_int           uiStride;

    switch (rVUVNDesc.ucVectorType)
    {
    case 0:
        uiVertex = pVectorInfo->pUniqVertex;
        uiNormal = pVectorInfo->pUniqNormal;
        uiStride = 0x10;
        break;

    case 1:
        return;

    default:
        if (pVectorInfo->pWeightedList == 0)
        {
            uiVertex = pVectorInfo->pWeightedVertex;
            uiNormal = pVectorInfo->pWeightedNormal;
            uiStride = 0x20;
        }
        else
        {
            uiVertex = SgdAddr(_GetGlobalVertexBuffer());
            uiNormal = SgdAddr(_GetGlobalNormalBuffer());
            uiStride = 0x10;
        }
        break;
    }

    for (int i = 0; i < rVUVNDesc.sNumVertex; i++)
    {
        pVA[i].pVertex = uiVertex + pVA[i].pVertex * uiStride;
        pVA[i].pNormal = uiNormal + pVA[i].pNormal * uiStride;
    }
}

// A mesh unit remaps the VUVN unit before it: preset meshes (& 0x10) unless
// & 0x40, realtime ones unless & 0xc0.
// FF3 0x001e3b28; from MioPan gra3dSGDData.c.
static void MappingMeshData(SGDPROCUNITHEADER *pPUHead, SGDPROCUNITHEADER *pVUVN, SGDFILEHEADER *pSGDHead)
{
    int mtype  = pPUHead->VUMeshDesc.ucMeshType;
    int gloops = pPUHead->VUMeshDesc.ucNumMesh;

    if (mtype & 0x10)
    {
        if (!(mtype & 0x40))
        {
            MappingVUVNDataPreset(pVUVN, mtype, gloops, 0);
        }
    }
    else if (!(mtype & 0xc0))
    {
        MappingVUVNData(pVUVN, pSGDHead);
    }
}

// Give each run of a weighted vertex list its offset into the blend buffer;
// a model needing more than the buffer holds loses its lists (it is then
// drawn from the weighted pools unblended).
// FF3 0x001e3b88; from MioPan gra3dSGDData.c.
// callcheck: FUN_002a1478 (newlib printf)
static void MappingVertexList(_VERTEXLIST *pVL, PHEAD *pVectorInfo)
{
    int size = 0;

    for (int i = 0; i < pVL->iNumList; i++)
    {
        _ONELIST &rList = pVL->List()[i];
        // FF3 reads usNumVector and vOff as one word (lw) before storing vOff:
        // vOff is 0 in a model that was never remapped.
        int vnnum = (int)(rList.usNumVector | ((u_int)rList.vOff << 16));

        rList.vOff = (u_short)size;
        size += vnnum;
    }

    if (_GetGlobalBufferSize() < size)
    {
        printf("VNBuffer Over size %d needs %d\n", _GetGlobalBufferSize(), size);  /* 0x35a6e0 */
        pVectorInfo->pWeightedList = 0;
        pVectorInfo->pUniqList     = 0;
    }
}

// FF2's empty MappingVertexListInverse; nothing in FF3 calls it (kept global
// so it stays, as in the ROM).
// FF3 0x001e3c20; from MioPan gra3dSGDData.c.
void MappingVertexListInverse(_VERTEXLIST *pVL, PHEAD *pVectorInfo)
{
    (void)pVL;
    (void)pVectorInfo;
}

// ──────────────────────────────────────────────────────────────────────
// Remap
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001e3c28; from MioPan gra3dSGDData.c (FF2's absolute addresses, not
// MioPan's self-relative fields; FF3 drops the asserts and the "Illegal SGD
// Data" check).
void sgdRemap(SGDFILEHEADER *pSGDHead)
{
    if (pSGDHead == nullptr)
    {
        return;
    }
    if (pSGDHead->uiVersionId != SGD_VALID_VERSIONID)
    {
        return;
    }
    if (pSGDHead->ucMapFlag != 0)
    {
        return;
    }

    const u_int uiTop = SgdAddr(pSGDHead);     // PC: the header's EE address
    u_int       i;

    ReiPan_Graph3dInvalidateSkinCache(pSGDHead);   // PC: as MioPan, a new model may sit here
    pSGDHead->ucMapFlag = 1;

    if (pSGDHead->pCoord < SGD_REMAP_BORDER && pSGDHead->pCoord != 0)
    {
        pSGDHead->pCoord += uiTop;
    }
    if (pSGDHead->pMaterial < SGD_REMAP_BORDER && pSGDHead->pMaterial != 0)
    {
        pSGDHead->pMaterial += uiTop;
    }

    // Coordinate parents: index -> address, negative -> none.
    if (pSGDHead->pCoord != 0)
    {
        SGDCOORDINATE *pCoord = SgdPtr<SGDCOORDINATE>(pSGDHead->pCoord);

        for (i = 0; i < pSGDHead->uiNumBlock - 1; i++)
        {
            u_int j = pCoord[i].pParent;

            if ((int)j < 0)
            {
                pCoord[i].pParent = 0;
            }
            else if (j < pSGDHead->pCoord)
            {
                pCoord[i].pParent = pSGDHead->pCoord + j * sizeof(SGDCOORDINATE);
            }
        }
    }

    // The vertex pools.
    if (pSGDHead->pVectorInfo != 0)
    {
        pSGDHead->pVectorInfo += uiTop;
        PHEAD *pVectorInfo = SgdPtr<PHEAD>(pSGDHead->pVectorInfo);

        for (i = 0; i < pVectorInfo->HeaderSections; i++)
        {
            SGDVECTORADDRESS &rVA = pVectorInfo->Address()[i];

            if (rVA.uiSize != 0 && rVA.pvVertex != 0)
            {
                rVA.pvVertex += uiTop;
            }
            if (rVA.uiSize > 1 && rVA.pvNormal != 0)
            {
                rVA.pvNormal += uiTop;
            }
            if (rVA.uiSize > 2 && rVA.pVertexList != 0)
            {
                rVA.pVertexList += uiTop;
            }
        }

        // Only the weighted vertex lists are kept, and only with a blend
        // buffer and weighted pools to blend.
        if (pVectorInfo->pUniqList != 0 && pVectorInfo->HeaderSections == 4)
        {
            if (_GetGlobalBufferSize() == 0)
            {
                pVectorInfo->pUniqList     = 0;
                pVectorInfo->pCommonList   = 0;
                pVectorInfo->pWeightedList = 0;
            }
            else if (pVectorInfo->pWeightedVertex == 0 && pVectorInfo->pWeightedNormal == 0)
            {
                pVectorInfo->pWeightedList = 0;
            }
            pVectorInfo->pUniqList = 0;

            _VERTEXLIST *pVL = SgdPtr<_VERTEXLIST>(pVectorInfo->pWeightedList);
            if (pVL != nullptr)
            {
                MappingVertexList(pVL, pVectorInfo);            // the vertices'
                MappingVertexList(pVL->Next(), pVectorInfo);    // the normals'
            }
        }
    }

    // Block heads.
    u_int *apProcUnitHead = pSGDHead->ProcUnitHeads();

    for (i = 0; i < pSGDHead->uiNumBlock; i++)
    {
        if (apProcUnitHead[i] != 0)
        {
            apProcUnitHead[i] += uiTop;
        }
    }

    // Every chain: pNext is relative to its own unit on disc. The last unit
    // (pNext 0) is not looked at.
    for (i = 0; i < pSGDHead->uiNumBlock; i++)
    {
        SGDPROCUNITHEADER *pVUVN   = nullptr;
        SGDPROCUNITHEADER *pPUHead = SgdPtr<SGDPROCUNITHEADER>(apProcUnitHead[i]);

        while (pPUHead != nullptr && pPUHead->pNext != 0)
        {
            pPUHead->pNext += SgdAddr(pPUHead);

            switch (pPUHead->iCategory)
            {
            case SGDPU_VUVN:
                pVUVN = pPUHead;
                break;

            case SGDPU_MESH:
                MappingMeshData(pPUHead, pVUVN, pSGDHead);
                break;

            case SGDPU_MATERIAL:                // material index -> its address
                pPUHead->VUMaterialDesc.pMat =
                    pSGDHead->pMaterial + pPUHead->VUMaterialDesc.pMat * sizeof(SGDMATERIAL);
                break;

            case SGDPU_COORDINATE:
                MappingCoordinateData(pPUHead, pSGDHead);
                break;

            case SGDPU_TRI2:
                RebuildTRI2Files(pPUHead);
                break;

            default:
                break;
            }

            pPUHead = SgdPtr<SGDPROCUNITHEADER>(pPUHead->pNext);
        }
    }
}

// FF3 0x001e3f68; from MioPan gra3dSGDData.c (FF3 counts up).
void sgdResetMaterialCache(SGDFILEHEADER *pSGDData)
{
    SGDMATERIAL *pMaterial = SgdPtr<SGDMATERIAL>(pSGDData->pMaterial);

    for (int i = 0; i < (int)pSGDData->usNumMaterial; i++)
    {
        pMaterial[i].iCacheStatus = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Bone matrices
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001e3fa8; from MioPan gra3dSGDData.c.
void sgdCalcBoneCoordinate(SGDCOORDINATE *pCoord, int iNumBlock)
{
    int i;

    for (i = 0; i < iNumBlock; i++)
    {
        pCoord[i].bCalc = 0;
    }
    for (i = 0; i < iNumBlock; i++)
    {
        sgdCalcCoordinateMatrix(&pCoord[i]);
    }
}

// The root takes matLocalWorld as its matCoord, then the whole tree.
// FF3 0x001e4018; from MioPan gra3dSGDData.c (FF3 copies the matrix inline).
void sgdCalcCoordinate(SGDFILEHEADER *pSGDData, float (*matLocalWorld)[4])
{
    if (pSGDData == nullptr)
    {
        return;
    }

    int            iNumBlock = (int)pSGDData->uiNumBlock;
    SGDCOORDINATE *pCoord    = SgdPtr<SGDCOORDINATE>(pSGDData->pCoord);

    sceVu0CopyMatrix(pCoord->matCoord, matLocalWorld);
    sgdCalcBoneCoordinate(pCoord, iNumBlock - 1);
}

// matLocalWorld = matCoord, or the parent's (computed first) times matCoord.
// FF3 0x001e4070; from MioPan gra3dSGDData.c (sceVu0MulMatrix's argument
// order as FF3: see MioPan's note on the column-major host implementation).
// callcheck: FUN_00282fa0 (sceVu0CopyMatrix)
void sgdCalcCoordinateMatrix(SGDCOORDINATE *pCoord)
{
    if (pCoord == nullptr || pCoord == (SGDCOORDINATE *)(intptr_t)-1 || pCoord->bCalc)
    {
        return;
    }

    if (pCoord->pParent == 0 || pCoord->pParent == 0xffffffffu)
    {
        sceVu0CopyMatrix(pCoord->matLocalWorld, pCoord->matCoord);
    }
    else
    {
        SGDCOORDINATE *pParent = SgdPtr<SGDCOORDINATE>(pCoord->pParent);

        sgdCalcCoordinateMatrix(pParent);
        sceVu0MulMatrix(pCoord->matLocalWorld, pParent->matLocalWorld, pCoord->matCoord);
    }

    pCoord->bCalc = 1;
}

// ──────────────────────────────────────────────────────────────────────
// Light file
// ──────────────────────────────────────────────────────────────────────

// Point lights get no direction, every light a falloff of 1; then the light
// set is copied out.
// FF3 0x001e40f8; from MioPan gra3dSGDData.c (FF3: no checks, 0x1610 bytes).
void sgdVerifyLightData(GRA3DLIGHTDATA *pRet, ZERO2LIGHTDATAFILE *pZLD)
{
    for (int i = 0; i < NUM_GRA3DLIGHTID; i++)
    {
        G3DLIGHT *pLight = &pZLD->LD.aLight[i];

        if (pLight->Type == G3DLIGHT_POINT)
        {
            sceVu0CopyVector(pLight->vDirection, (float *)s_v0000);
        }
        pLight->fFalloff = 1.0f;                // data 0x370cb4
    }

    memcpy(pRet, &pZLD->LD, sizeof(GRA3DLIGHTDATA));
}

// FF2's two-argument sgdClearCoordCalcFlgParents (FF3: no bone-id check).
// Nothing in FF3 calls it.
// FF3 0x001e41a8; from MioPan gra3dSGDData.c.
void sgdClearCoordCalcFlgParentsId(void *pSGDData, int bone_no)
{
    SGDFILEHEADER *pFH = (SGDFILEHEADER *)pSGDData;

    sgdClearCoordCalcFlgParents(&SgdPtr<SGDCOORDINATE>(pFH->pCoord)[bone_no]);
}

// FF3 0x001e41d0; from MioPan gra3dSGDData.c (no bone-id check; no callers).
void sgdClearCoordCalcFlg(void *pSGDData, int bone_no)
{
    SGDFILEHEADER *pFH = (SGDFILEHEADER *)pSGDData;

    SgdPtr<SGDCOORDINATE>(pFH->pCoord)[bone_no].bCalc = 0;
}

// FF3 0x001e41e8 (FF3-only).
void sgdClearCoordCalcFlgCoord(SGDCOORDINATE *pCoord)
{
    pCoord->bCalc = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Lookup
// ──────────────────────────────────────────────────────────────────────

// FF3 0x001e41f0; from MioPan gra3dSGDData.c.
SGDPROCUNITHEADER *sgdGetProcUnit(SGDPROCUNITHEADER *pPUHead, int iProcUnitId, int iUnitIndex)
{
    int iCount = 0;

    for (SGDPROCUNITHEADER *p = pPUHead; p != nullptr; p = SgdPtr<SGDPROCUNITHEADER>(p->pNext))
    {
        if (p->iCategory == iProcUnitId)
        {
            if (iCount == iUnitIndex)
            {
                return p;
            }
            iCount++;
        }
    }
    return nullptr;
}

static void sgdExpandBoundingBox(float (*avBB)[4], float (*avUnit)[4]);

// FF3 0x001e4230; from MioPan gra3dSGDData.c (FF3 merges each unit in
// sgdExpandBoundingBox; the corners follow the 0x10-byte unit header).
void sgdGetBoundingBox(SGDFILEHEADER *pFH, float (*avBB)[4])
{
    memset(avBB, 0, sizeof(float[8][4]));

    u_int *apProcUnitHead = pFH->ProcUnitHeads();

    for (u_int i = 0; i < pFH->uiNumBlock; i++)
    {
        for (SGDPROCUNITHEADER *pPUHead = SgdPtr<SGDPROCUNITHEADER>(apProcUnitHead[i]); pPUHead != nullptr;
             pPUHead = SgdPtr<SGDPROCUNITHEADER>(pPUHead->pNext))
        {
            if (pPUHead->iCategory == SGDPU_BOUNDING_BOX)
            {
                sgdExpandBoundingBox(avBB, (float (*)[4])&pPUHead[1]);
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Inverse remap
// ──────────────────────────────────────────────────────────────────────

// FF2's indexof<T> instances (linkonce).
// FF3 0x002bcb80.
static int indexof_SGDCOORDINATE(u_int uiTop, u_int uiAddr)
{
    return (int)(uiAddr - uiTop) / (int)sizeof(SGDCOORDINATE);
}

// FF3 0x002bcba0.
static int indexof_SGDMATERIAL(u_int uiTop, u_int uiAddr)
{
    return (int)(uiAddr - uiTop) / (int)sizeof(SGDMATERIAL);
}

// Back to file offsets. FF3 leaves the VUVN units' addresses and the vertex
// lists' offsets as they are (FF2's mesh pass is gone).
// FF3 0x001e42e8; from MioPan gra3dSGDData.c (FF2's absolute form). Nothing
// in FF3 calls it.
void sgdRemapInverse(SGDFILEHEADER *pSGDHead)
{
    if (pSGDHead == nullptr)
    {
        return;
    }
    if (pSGDHead->uiVersionId != SGD_VALID_VERSIONID)
    {
        return;
    }
    if (pSGDHead->ucMapFlag == 0)
    {
        return;
    }

    const u_int uiTop = SgdAddr(pSGDHead);
    u_int       i;

    ReiPan_Graph3dInvalidateSkinCache(pSGDHead);   // PC: as MioPan
    pSGDHead->ucMapFlag = 0;

    // Coordinate parents: address -> index, none -> -1.
    if (pSGDHead->pCoord != 0)
    {
        SGDCOORDINATE *pCoord = SgdPtr<SGDCOORDINATE>(pSGDHead->pCoord);

        for (i = 0; i < pSGDHead->uiNumBlock - 1; i++)
        {
            if (pCoord[i].pParent == 0)
            {
                pCoord[i].pParent = 0xffffffffu;
            }
            else
            {
                pCoord[i].pParent = (u_int)indexof_SGDCOORDINATE(pSGDHead->pCoord, pCoord[i].pParent);
            }
        }
        pSGDHead->pCoord -= uiTop;
    }

    // The vertex pools.
    if (pSGDHead->pVectorInfo != 0)
    {
        PHEAD *pVectorInfo = SgdPtr<PHEAD>(pSGDHead->pVectorInfo);

        for (i = 0; i < pVectorInfo->HeaderSections; i++)
        {
            SGDVECTORADDRESS &rVA = pVectorInfo->Address()[i];

            if (rVA.uiSize != 0 && rVA.pvVertex != 0)
            {
                rVA.pvVertex -= uiTop;
            }
            if (rVA.uiSize > 1 && rVA.pvNormal != 0)
            {
                rVA.pvNormal -= uiTop;
            }
            if (rVA.uiSize > 2 && rVA.pVertexList != 0)
            {
                rVA.pVertexList -= uiTop;
            }
        }

        if (pVectorInfo->pUniqList != 0 && pVectorInfo->HeaderSections == 4)
        {
            pVectorInfo->pUniqList     = 0;
            pVectorInfo->pCommonList   = 0;
            pVectorInfo->pWeightedList = 0;
        }
        pSGDHead->pVectorInfo -= uiTop;
    }

    // Every chain, then the block heads.
    u_int *apProcUnitHead = pSGDHead->ProcUnitHeads();

    for (i = 0; i < pSGDHead->uiNumBlock; i++)
    {
        SGDPROCUNITHEADER *pPUHead = SgdPtr<SGDPROCUNITHEADER>(apProcUnitHead[i]);

        while (pPUHead != nullptr && pPUHead->pNext != 0)
        {
            u_int uiNext = pPUHead->pNext;

            if (pPUHead->iCategory == SGDPU_MATERIAL)
            {
                pPUHead->VUMaterialDesc.pMat =
                    (u_int)indexof_SGDMATERIAL(pSGDHead->pMaterial, pPUHead->VUMaterialDesc.pMat);
            }
            pPUHead->pNext = uiNext - SgdAddr(pPUHead);
            pPUHead        = SgdPtr<SGDPROCUNITHEADER>(uiNext);
        }

        if (apProcUnitHead[i] != 0)
        {
            apProcUnitHead[i] -= uiTop;
        }
    }

    if (uiTop < pSGDHead->pMaterial)
    {
        pSGDHead->pMaterial -= uiTop;
    }
}

// FF2's sgdEnableOptimizeTexture (RebuildTRI2Files' switch); nothing in FF3
// calls it.
// FF3 0x001e4538; from MioPan gra3dSGDData.c.
void sgdEnableOptimizeTexture(int b)
{
    s_bEnableOptimizeTexture = b;
}

// Grow the 8 corners of avBB outward by a unit's 8 corners: corner c takes
// the max on each axis whose bit is set in c (1: x, 2: y, 4: z), the min on
// the others. FF3 unrolls the 24 compares.
// FF3 0x001e4540 (FF3-only out-of-line; FF2 inlines it in sgdGetBoundingBox).
static void sgdExpandBoundingBox(float (*avBB)[4], float (*avUnit)[4])
{
    for (int c = 0; c < 8; c++)
    {
        for (int a = 0; a < 3; a++)
        {
            if (c & (1 << a))
            {
                if (avBB[c][a] < avUnit[c][a])
                {
                    avBB[c][a] = avUnit[c][a];
                }
            }
            else if (avUnit[c][a] < avBB[c][a])
            {
                avBB[c][a] = avUnit[c][a];
            }
        }
    }
}
