/* ==========================================================================
 *  graphics/graph3d/sgd_types.h
 *
 *  SGD ("scene graph data") model file structures (FF2 sgd_types.h /
 *  gra3dTypes.h): the file header, the per-block chains of process units
 *  (vertex/normal, mesh, material, coordinate, bounding box, GS image, TRI2
 *  texture units) and the data that hangs off them.
 *
 *  SGD data comes from files and lives in emulated EE memory. sgdRemap
 *  (gra3dSGDData.c) turns the file's offsets into absolute EE addresses, as on
 *  the PS2, so every pointer field here is a u_int EE address: follow it with
 *  SgdPtr<T>() (ReiPan_GetHostPointer) and store one with SgdAddr(). Layouts
 *  are FF2's, with the offsets FF3 code touches checked against FF3 (gra3dSGD.c
 *  0x1e0330..0x1e3850); fields FF3 does not touch keep FF2's names.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_SGD_TYPES_H
#define _GRAPHICS_GRAPH3D_SGD_TYPES_H

#include <stddef.h>                     // offsetof

#include <libvu0.h>                     // sceVu0FMATRIX

#include "sdk/scetypes.h"
#include "sdk/sce_gs.h"                 // sceGsLoadImage (SGDTRI2FILEHEADER)
#include "reipan/reipan_memory.h"       // PC: ReiPan_GetHostPointer / ReiPan_GetPs2Address

// SGDFILEHEADER.uiVersionId of the files FF3 draws (gra3dExecPrelightMatrix
// and _gra3dDrawSGD test it, as FF2's SGD_VALID_VERSIONID).
#define SGD_VALID_VERSIONID     0x1050

// PC: an EE address inside SGD data -> host pointer (0 stays null).
template <class T> static inline T *SgdPtr(u_int addr)
{
    return addr != 0 ? (T *)ReiPan_GetHostPointer(addr) : nullptr;
}

// PC: host pointer into EE memory -> the EE address SGD data stores.
static inline u_int SgdAddr(const void *p)
{
    return p != nullptr ? (u_int)ReiPan_GetPs2Address(p) : 0;
}

// Three packed floats (vertex, normal, colour in the preset streams).
typedef float VECTOR3[3];

// _gra3dDrawSGD's render type (FF2 SGDRENDERTYPE).
enum SGDRENDERTYPE
{
    SRT_REALTIME         = 0,
    SRT_PRELIGHTING      = 1,
    SRT_MAPSHADOW        = 2,
    SRT_CLEARPRELIGHTING = 3,
    NUM_SGDRENDERTYPE    = 4,
};

// SGDPROCUNITHEADER.iCategory (FF2 ProcUnitType).
enum SGDPROCUNITTYPE
{
    SGDPU_VUVN         = 0,             // vertex / normal block of the meshes after it
    SGDPU_MESH         = 1,
    SGDPU_MATERIAL     = 2,
    SGDPU_COORDINATE   = 3,
    SGDPU_BOUNDING_BOX = 4,
    SGDPU_GS_IMAGE     = 5,
    SGDPU_TRI2         = 10,            // embedded TRI2 texture file
    SGDPU_END          = 11,
    SGDPU_INVALID      = 12,
    SGDPU_MONOTONE_TRI2 = 13,           // the TRI2 used while monotone drawing is on
    SGDPU_STACK_TRI2   = 14,
    SGDPU_MIRROR_BLOCK = 0x40,          // FF3, in block 0: the block holding the mirror mesh (gra3dMirror.c)
};

// Mesh vector types of the preset (pre-lit) streams (FF2 iMT_*).
#define iMT_0   0x10                    // interleaved vertex + normal
#define iMT_2   0x12                    // interleaved, textured
#define iMT_2F  0x32                    // one normal per strip, then the vertices

// One node of a model's coordinate tree (FF2 layout, 0xe0; FF3 copies it with
// memcpy 0xe0 in gra3dsgdSetCoordinate).
struct SGDCOORDINATE                    /* 0xe0 */
{
    /* 0x00 */ sceVu0FMATRIX matCoord;          // FF2, unverified
    /* 0x40 */ sceVu0FMATRIX matLocalWorld;     // read by every gra3dSGD.c walker
    /* 0x80 */ sceVu0FMATRIX _matWork;          // FF2, unverified
    /* 0xc0 */ sceVu0FVECTOR vRot;              // FF2, unverified
    /* 0xd0 */ u_int         pParent;           // EE address of the parent node
    /* 0xd4 */ u_int         bCalc;             // gra3dExecPrelightMatrix clears it
    /* 0xd8 */ u_int         edge_check;        // BoundingBoxCalcP: the block crosses the guard band
    /* 0xdc */ int           bInViewvolume;     // CheckBoundingBox; the map-shadow pass tests it
};
static_assert(sizeof(SGDCOORDINATE) == 0xe0, "SGDCOORDINATE is 0xe0 bytes");
static_assert(offsetof(SGDCOORDINATE, matLocalWorld) == 0x40, "SGDCOORDINATE.matLocalWorld");
static_assert(offsetof(SGDCOORDINATE, pParent) == 0xd0, "SGDCOORDINATE.pParent");
static_assert(offsetof(SGDCOORDINATE, bCalc) == 0xd4, "SGDCOORDINATE.bCalc");
static_assert(offsetof(SGDCOORDINATE, edge_check) == 0xd8, "SGDCOORDINATE.edge_check");
static_assert(offsetof(SGDCOORDINATE, bInViewvolume) == 0xdc, "SGDCOORDINATE.bInViewvolume");

// The file header (FF2 layout, 0x20). The block heads follow at +0x18: one
// EE address per block, uiNumBlock of them (block 0 holds the textures, the
// last one the skinned meshes drawn after the others).
struct SGDFILEHEADER                    /* 0x20 */
{
    /* 0x00 */ u_int    uiVersionId;            // SGD_VALID_VERSIONID
    /* 0x04 */ u_char   ucMapFlag;              // FF2: set while remapped, unverified
    /* 0x05 */ u_char   ucModelType;            // bit 0: preset (pre-lit) model
    /* 0x06 */ u_short  usNumMaterial;          // FF2, unverified
    /* 0x08 */ u_int    pCoord;                 // EE address of SGDCOORDINATE[]
    /* 0x0c */ u_int    pMaterial;              // EE address of SGDMATERIAL[] (they end at pVectorInfo)
    /* 0x10 */ u_int    pVectorInfo;            // EE address of the PHEAD
    /* 0x14 */ u_int    uiNumBlock;
    /* 0x18 */ u_int    apProcUnitHead;         // first of uiNumBlock block heads (EE addresses)
    /* 0x1c */ int      aiPad[1];

    // The block-head table. PC: computed from the object's address, not
    // apProcUnitHead's declared size, so indexing past [0] stays defined (MioPan
    // sgd_types.h: GCC -O3 deleted stores through a [1] array here).
    u_int *ProcUnitHeads()
    {
        return (u_int *)((u_char *)this + offsetof(SGDFILEHEADER, apProcUnitHead));
    }
};
static_assert(sizeof(SGDFILEHEADER) == 0x20, "SGDFILEHEADER is 0x20 bytes");
static_assert(offsetof(SGDFILEHEADER, ucModelType) == 0x05, "SGDFILEHEADER.ucModelType");
static_assert(offsetof(SGDFILEHEADER, pCoord) == 0x08, "SGDFILEHEADER.pCoord");
static_assert(offsetof(SGDFILEHEADER, pMaterial) == 0x0c, "SGDFILEHEADER.pMaterial");
static_assert(offsetof(SGDFILEHEADER, pVectorInfo) == 0x10, "SGDFILEHEADER.pVectorInfo");
static_assert(offsetof(SGDFILEHEADER, uiNumBlock) == 0x14, "SGDFILEHEADER.uiNumBlock");
static_assert(offsetof(SGDFILEHEADER, apProcUnitHead) == 0x18, "SGDFILEHEADER.apProcUnitHead");

// One section of a PHEAD seen as FF2's SGDVECTORADDRESS: sgdRemap and
// sgdRemapInverse walk HeaderSections of these (stride 0x10, from PHEAD + 4)
// and convert the fields the size allows (> 0: vertex, > 1: normal, > 2: list).
struct SGDVECTORADDRESS                 /* 0x10 */
{
    /* 0x0 */ u_int uiSize;                     // fields in use
    /* 0x4 */ u_int pvVertex;                   // offset on disc, EE address after sgdRemap
    /* 0x8 */ u_int pvNormal;
    /* 0xc */ u_int pVertexList;
};
static_assert(sizeof(SGDVECTORADDRESS) == 0x10, "SGDVECTORADDRESS is 0x10 bytes");
static_assert(offsetof(SGDVECTORADDRESS, pVertexList) == 0xc, "SGDVECTORADDRESS.pVertexList");

// The vertex pools of a model (FF2 PHEAD, at SGDFILEHEADER.pVectorInfo):
// unique, common and weighted (skinned) vertices / normals with their lists.
struct PHEAD                            /* 0x34 */
{
    /* 0x00 */ u_int HeaderSections;            // FF2, unverified
    /* 0x04 */ u_int UniqHeaderSize;            // FF2, unverified
    /* 0x08 */ u_int pUniqVertex;               // FF2, unverified
    /* 0x0c */ u_int pUniqNormal;               // FF2, unverified
    /* 0x10 */ u_int pUniqList;                 // FF2, unverified
    /* 0x14 */ u_int CommonHeaderSize;          // FF2, unverified
    /* 0x18 */ u_int pCommonVertex;             // FF2, unverified
    /* 0x1c */ u_int pCommonNormal;             // FF2, unverified
    /* 0x20 */ u_int pCommonList;               // FF2, unverified
    /* 0x24 */ u_int WeightedHeaderSize;        // FF2, unverified
    /* 0x28 */ u_int pWeightedVertex;           // EE address: pairs of bone-space vertices
    /* 0x2c */ u_int pWeightedNormal;           // EE address: pairs of bone-space normals
    /* 0x30 */ u_int pWeightedList;             // EE address of the _VERTEXLIST pair, 0 when not skinned

    // The sections as an array (HeaderSections of them; a fourth one would
    // run past this struct, so it is computed from the object's address).
    SGDVECTORADDRESS *Address()
    {
        return (SGDVECTORADDRESS *)((u_char *)this + offsetof(PHEAD, UniqHeaderSize));
    }
};
static_assert(sizeof(PHEAD) == 0x34, "PHEAD is 0x34 bytes");
static_assert(offsetof(PHEAD, pWeightedVertex) == 0x28, "PHEAD.pWeightedVertex");
static_assert(offsetof(PHEAD, pWeightedNormal) == 0x2c, "PHEAD.pWeightedNormal");
static_assert(offsetof(PHEAD, pWeightedList) == 0x30, "PHEAD.pWeightedList");
static_assert(offsetof(PHEAD, UniqHeaderSize) == 0x04, "PHEAD.UniqHeaderSize (section 0's uiSize)");
static_assert(offsetof(PHEAD, pUniqVertex) == 0x08, "PHEAD.pUniqVertex");
static_assert(offsetof(PHEAD, pUniqNormal) == 0x0c, "PHEAD.pUniqNormal");
static_assert(offsetof(PHEAD, pUniqList) == 0x10, "PHEAD.pUniqList");
static_assert(offsetof(PHEAD, pCommonList) == 0x20, "PHEAD.pCommonList");

// One run of a weighted vertex list: usNumVector vectors skinned across the
// bone pair (sCoordId0, sCoordId1).
struct _ONELIST                         /* 0x8 */
{
    /* 0x0 */ short  sCoordId0;
    /* 0x2 */ short  sCoordId1;
    /* 0x4 */ u_short usNumVector;
    /* 0x6 */ u_short vOff;                     // FF2, unverified
};
static_assert(sizeof(_ONELIST) == 0x8, "_ONELIST is 8 bytes");
static_assert(offsetof(_ONELIST, sCoordId1) == 0x2, "_ONELIST.sCoordId1");
static_assert(offsetof(_ONELIST, usNumVector) == 0x4, "_ONELIST.usNumVector");

// iNumList runs follow the count. PHEAD.pWeightedList holds two lists back to
// back: the vertices', then the normals' (it starts where the first ends).
struct _VERTEXLIST                      /* 0x4 + 8 * iNumList */
{
    /* 0x0 */ int iNumList;

    _ONELIST *List()
    {
        return (_ONELIST *)((u_char *)this + sizeof(int));
    }
    _VERTEXLIST *Next()                         // the list after this one
    {
        return (_VERTEXLIST *)&List()[iNumList];
    }
};
static_assert(sizeof(_VERTEXLIST) == 0x4, "_VERTEXLIST header is 4 bytes");

// A VUVN unit's per-vertex entry: where its vertex and normal quadwords are.
struct _VECTORADDRESS                   /* 0x8 */
{
    /* 0x0 */ u_int pVertex;                    // EE address (a weighted vertex is two quadwords)
    /* 0x4 */ u_int pNormal;                    // EE address
};
static_assert(sizeof(_VECTORADDRESS) == 0x8, "_VECTORADDRESS is 8 bytes");
static_assert(offsetof(_VECTORADDRESS, pNormal) == 0x4, "_VECTORADDRESS.pNormal");

// Which gra3d lights a material's VU1 colours were last built for (FF2
// GRA3DMATERIALINDEXCACHE, kept here because SGDMATERIAL embeds it; gra3d's
// s_aMaterialCache[3] at bss 0x3ee460 holds the current three).
struct GRA3DMATERIALINDEXCACHE          /* 0x10 */
{
    /* 0x0 */ int bEnable;                      // gra3dIsLightTypeEnable of the type
    /* 0x4 */ int aiIndex[3];                   // per VU1 column: the light's group index
};
static_assert(sizeof(GRA3DMATERIALINDEXCACHE) == 0x10, "GRA3DMATERIALINDEXCACHE is 0x10 bytes");
static_assert(offsetof(GRA3DMATERIALINDEXCACHE, aiIndex) == 0x4, "GRA3DMATERIALINDEXCACHE.aiIndex");

// A model material (FF2 layout, 0xb0; FF3 gra3dSetMaterial reads uiPrimType,
// the four colours and aCache, and sets iCacheStatus; SetMaterialDataVU keeps
// its VU1 packet in iTagAddressOld / iSizeOld). No pointers: usable in place
// in EE memory.
struct SGDMATERIAL                      /* 0xb0 */
{
    /* 0x00 */ u_int                   uiPrimType;      // 1: SGD primitive (192 / 43 colour scales)
    /* 0x04 */ char                    strTexName[12];  // FF2, unverified
    /* 0x10 */ sceVu0FVECTOR           vAmbient;
    /* 0x20 */ sceVu0FVECTOR           vDiffuse;        // w: alpha (gra3dSetSGDMaterialAlpha)
    /* 0x30 */ sceVu0FVECTOR           vSpecular;
    /* 0x40 */ sceVu0FVECTOR           vEmission;
    /* 0x50 */ int                     iCacheStatus;    // >= 0: iTagAddressOld is valid
    /* 0x54 */ u_int                   iTagAddressOld;  // EE address of the material packet (& 0x0fffffff)
    /* 0x58 */ int                     iSizeOld;        // its size in quadwords
    /* 0x5c */ int                     iPad;
    /* 0x60 */ GRA3DMATERIALINDEXCACHE aCache[3];       // per G3DLIGHTTYPE
    /* 0x90 */ int                     aiPad[8];
};
static_assert(sizeof(SGDMATERIAL) == 0xb0, "SGDMATERIAL is 0xb0 bytes");
static_assert(offsetof(SGDMATERIAL, vAmbient) == 0x10, "SGDMATERIAL.vAmbient");
static_assert(offsetof(SGDMATERIAL, vDiffuse) == 0x20, "SGDMATERIAL.vDiffuse");
static_assert(offsetof(SGDMATERIAL, vSpecular) == 0x30, "SGDMATERIAL.vSpecular");
static_assert(offsetof(SGDMATERIAL, vEmission) == 0x40, "SGDMATERIAL.vEmission");
static_assert(offsetof(SGDMATERIAL, iCacheStatus) == 0x50, "SGDMATERIAL.iCacheStatus");
static_assert(offsetof(SGDMATERIAL, iTagAddressOld) == 0x54, "SGDMATERIAL.iTagAddressOld");
static_assert(offsetof(SGDMATERIAL, iSizeOld) == 0x58, "SGDMATERIAL.iSizeOld");
static_assert(offsetof(SGDMATERIAL, aCache) == 0x60, "SGDMATERIAL.aCache");

// ---- process-unit descriptors (the 8-byte union of SGDPROCUNITHEADER) ------

struct SGDVUVNDESC                      /* 0x8 */
{
    /* 0x0 */ short  sNumVertex;
    /* 0x2 */ short  sNumNormal;
    /* 0x4 */ u_char ucSize;                    // quadwords of the preloaded VUVN data
    /* 0x5 */ u_char ucVectorType;              // 2 / 3: weighted (one / per-vertex bone pair)
    /* 0x6 */ u_char aucPad[2];
};
static_assert(sizeof(SGDVUVNDESC) == 0x8, "SGDVUVNDESC is 8 bytes");
static_assert(offsetof(SGDVUVNDESC, sNumNormal) == 0x2, "SGDVUVNDESC.sNumNormal");
static_assert(offsetof(SGDVUVNDESC, ucSize) == 0x4, "SGDVUVNDESC.ucSize");
static_assert(offsetof(SGDVUVNDESC, ucVectorType) == 0x5, "SGDVUVNDESC.ucVectorType");

struct SGDVUMESHDESC                    /* 0x8 */
{
    /* 0x0 */ int    iTagSize;                  // quadwords of the mesh packet
    /* 0x4 */ u_char ucPad0;
    /* 0x5 */ u_char ucMeshType;                // realtime: & 0xd3 picks the VU1 routine; preset: iMT_*
    /* 0x6 */ u_char ucNumMesh;                 // strips
    /* 0x7 */ u_char ucPad1;
};
static_assert(sizeof(SGDVUMESHDESC) == 0x8, "SGDVUMESHDESC is 8 bytes");
static_assert(offsetof(SGDVUMESHDESC, ucMeshType) == 0x5, "SGDVUMESHDESC.ucMeshType");
static_assert(offsetof(SGDVUMESHDESC, ucNumMesh) == 0x6, "SGDVUMESHDESC.ucNumMesh");

struct SGDVUMATERIALDESC                /* 0x8 */
{
    /* 0x0 */ u_int pMat;                       // EE address of the SGDMATERIAL (an index on disc)
    /* 0x4 */ int   iPad;
};
static_assert(sizeof(SGDVUMATERIALDESC) == 0x8, "SGDVUMATERIALDESC is 8 bytes");

struct SGDCOORDINATEDESC                /* 0x8 */
{
    /* 0x0 */ int iCoordId0;
    /* 0x4 */ int iCoordId1;                    // != 0: the meshes after it are skinned
};
static_assert(sizeof(SGDCOORDINATEDESC) == 0x8, "SGDCOORDINATEDESC is 8 bytes");
static_assert(offsetof(SGDCOORDINATEDESC, iCoordId1) == 0x4, "SGDCOORDINATEDESC.iCoordId1");

struct SGDBOUNDINGBOXDESC               /* 0x8 */
{
    /* 0x0 */ int iCoordId;
    /* 0x4 */ int iPad;
};
static_assert(sizeof(SGDBOUNDINGBOXDESC) == 0x8, "SGDBOUNDINGBOXDESC is 8 bytes");

struct SGDGSIMAGEDESC                   /* 0x8 */
{
    /* 0x0 */ int iQWordSize;
    /* 0x4 */ int iPad;
};
static_assert(sizeof(SGDGSIMAGEDESC) == 0x8, "SGDGSIMAGEDESC is 8 bytes");

// FF3: SGDPU_MIRROR_BLOCK's descriptor (gra3dsgdGetMirrorBlock).
struct SGDMIRRORBLOCKDESC               /* 0x8 */
{
    /* 0x0 */ int iBlock;                       // index into SGDFILEHEADER's block heads
    /* 0x4 */ int iPad;
};
static_assert(sizeof(SGDMIRRORBLOCKDESC) == 0x8, "SGDMIRRORBLOCKDESC is 8 bytes");

struct SGDTEXTUREIMAGEDESC              /* 0x8 */
{
    /* 0x0 */ int iNumTexture;
    /* 0x4 */ int iPaddingSize;                 // bytes from the unit's data to its TRI2 header
};
static_assert(sizeof(SGDTEXTUREIMAGEDESC) == 0x8, "SGDTEXTUREIMAGEDESC is 8 bytes");
static_assert(offsetof(SGDTEXTUREIMAGEDESC, iPaddingSize) == 0x4, "SGDTEXTUREIMAGEDESC.iPaddingSize");

// One process unit of a block's chain; its data follows (pPUHead + 1).
struct SGDPROCUNITHEADER                /* 0x10 */
{
    /* 0x0 */ u_int pNext;                      // EE address of the next unit, 0 ends the chain
    /* 0x4 */ int   iCategory;                  // SGDPROCUNITTYPE
    union                                       /* 0x8 */
    {
        SGDVUVNDESC         VUVNDesc;
        SGDVUMESHDESC       VUMeshDesc;
        SGDVUMATERIALDESC   VUMaterialDesc;
        SGDCOORDINATEDESC   CoordDesc;
        SGDBOUNDINGBOXDESC  BoundingBoxDesc;
        SGDGSIMAGEDESC      GSImageDesc;
        SGDTEXTUREIMAGEDESC TexDesc;
        SGDMIRRORBLOCKDESC  MirrorBlockDesc;
    };
};
static_assert(sizeof(SGDPROCUNITHEADER) == 0x10, "SGDPROCUNITHEADER is 0x10 bytes");
static_assert(offsetof(SGDPROCUNITHEADER, iCategory) == 0x4, "SGDPROCUNITHEADER.iCategory");
static_assert(offsetof(SGDPROCUNITHEADER, VUVNDesc) == 0x8, "SGDPROCUNITHEADER.VUVNDesc");

// ---- process-unit data -----------------------------------------------------

// A realtime VUVN unit's data: the VIF code and VU1 addresses, copied as two
// quadwords in front of the vertex / normal pairs; _VECTORADDRESS[sNumVertex]
// follow at pPUHead + 3.
struct SGDVUVNDATA                      /* 0x20 */
{
    /* 0x00 */ u_int qwVif1Code[4];
    /* 0x10 */ u_int uiVTop;                    // FF2, unverified
    /* 0x14 */ u_int uiPTop;                    // FF2, unverified
    /* 0x18 */ u_int uiWTop;                    // FF2, unverified
    /* 0x1c */ u_int uiNumMesh;                 // FF2, unverified
};
static_assert(sizeof(SGDVUVNDATA) == 0x20, "SGDVUVNDATA is 0x20 bytes");

// The same 0x20 bytes seen as a weighted vertex: the bone pair in bytes
// 0x1c / 0x1d (VUVN header for vector type 2, each vertex for type 3).
struct SGDVUVNDATA_WEIGHTED             /* 0x20 */
{
    /* 0x00 */ u_char auc0[28];
    /* 0x1c */ u_char ucBoneId0;
    /* 0x1d */ u_char ucBoneId1;
    /* 0x1e */ u_char auc1[2];
};
static_assert(sizeof(SGDVUVNDATA_WEIGHTED) == 0x20, "SGDVUVNDATA_WEIGHTED is 0x20 bytes");
static_assert(offsetof(SGDVUVNDATA_WEIGHTED, ucBoneId0) == 0x1c, "SGDVUVNDATA_WEIGHTED.ucBoneId0");

struct SGDMESHVERTEXDATA_TYPE2          /* 0x18 */
{
    /* 0x00 */ VECTOR3 vVertex;
    /* 0x0c */ VECTOR3 vNormal;
};
static_assert(sizeof(SGDMESHVERTEXDATA_TYPE2) == 0x18, "SGDMESHVERTEXDATA_TYPE2 is 0x18 bytes");
static_assert(offsetof(SGDMESHVERTEXDATA_TYPE2, vNormal) == 0xc, "SGDMESHVERTEXDATA_TYPE2.vNormal");

// A preset VUVN unit's data: 0x28 bytes of VIF codes, then the packed
// vertices (iMT_0 / iMT_2: SGDMESHVERTEXDATA_TYPE2[]; iMT_2F: sNumNormal
// normals, then the vertices, VECTOR3 each).
struct SGDVUVNDATA_PRESET               /* 0x28 + data */
{
    /* 0x00 */ u_int aui[10];

    SGDMESHVERTEXDATA_TYPE2 *Type2()
    {
        return (SGDMESHVERTEXDATA_TYPE2 *)((u_char *)this + 0x28);
    }
    const SGDMESHVERTEXDATA_TYPE2 *Type2() const
    {
        return (const SGDMESHVERTEXDATA_TYPE2 *)((const u_char *)this + 0x28);
    }
    VECTOR3 *Type2F()
    {
        return (VECTOR3 *)((u_char *)this + 0x28);
    }
    const VECTOR3 *Type2F() const
    {
        return (const VECTOR3 *)((const u_char *)this + 0x28);
    }
};
static_assert(sizeof(SGDVUVNDATA_PRESET) == 0x28, "SGDVUVNDATA_PRESET header is 0x28 bytes");

// A preset mesh unit's data head: word offsets to its ST and colour streams
// (the colour offset counts from the unit header, the ST one from +0xc).
struct SGDVUMESHDATA_PRESET             /* 0x10 + data */
{
    /* 0x00 */ short asPad0[2];
    /* 0x04 */ short sOffsetToST;
    /* 0x06 */ short sOffsetToPrim;
    /* 0x08 */ int   aiPad1[2];
};
static_assert(sizeof(SGDVUMESHDATA_PRESET) == 0x10, "SGDVUMESHDATA_PRESET head is 0x10 bytes");
static_assert(offsetof(SGDVUMESHDATA_PRESET, sOffsetToST) == 0x4, "SGDVUMESHDATA_PRESET.sOffsetToST");
static_assert(offsetof(SGDVUMESHDATA_PRESET, sOffsetToPrim) == 0x6, "SGDVUMESHDATA_PRESET.sOffsetToPrim");

// One strip of a preset colour stream: a VIF UNPACK code (NUM in byte 2), then
// NUM vertex colours. A colour whose first word's low half is 1 is a marker the
// prelight passes leave alone (they bump a computed colour off it).
struct _SGDVUMESHCOLORDATA              /* 0x4 + 0xc * NUM */
{
    /* 0x0 */ u_char  aucVifUnpack[4];          // [2]: NUM

    VECTOR3 *Color()
    {
        return (VECTOR3 *)((u_char *)this + 4);
    }
};
static_assert(sizeof(_SGDVUMESHCOLORDATA) == 0x4, "_SGDVUMESHCOLORDATA head is 4 bytes");

// ST streams (gra3dChangeST). Realtime meshes: SGDVUMESHPOINTNUM[ucNumMesh] at
// pPUHead + 0x40, a 0xc-byte register set, then per strip a VIF code and
// uiPointNum ST pairs. Preset meshes: per strip a VIF UNPACK code (NUM in
// byte 2) and NUM ST pairs.
struct SGDVUMESHPOINTNUM                /* 0x8 */
{
    /* 0x0 */ u_int uiReg;                      // FF2, unverified
    /* 0x4 */ int   uiPointNum;
};
static_assert(sizeof(SGDVUMESHPOINTNUM) == 0x8, "SGDVUMESHPOINTNUM is 8 bytes");
static_assert(offsetof(SGDVUMESHPOINTNUM, uiPointNum) == 0x4, "SGDVUMESHPOINTNUM.uiPointNum");

struct SGDVUMESHSTREGSET                /* 0xc */
{
    /* 0x0 */ u_int auiVifCode[3];
};
static_assert(sizeof(SGDVUMESHSTREGSET) == 0xc, "SGDVUMESHSTREGSET is 0xc bytes");

struct SGDVUMESHST                      /* 0x8 */
{
    /* 0x0 */ float fS;
    /* 0x4 */ float fT;
};
static_assert(sizeof(SGDVUMESHST) == 0x8, "SGDVUMESHST is 8 bytes");

struct SGDVUMESHSTDATA                  /* 0x4 + 8 * n */
{
    /* 0x0 */ u_char aucVifUnpack[4];           // preset: [2] NUM

    SGDVUMESHST *ST()
    {
        return (SGDVUMESHST *)((u_char *)this + 4);
    }
};
static_assert(sizeof(SGDVUMESHSTDATA) == 0x4, "SGDVUMESHSTDATA head is 4 bytes");

// An embedded TRI2 texture file (FF2 layout): VIF codes, then one VIF1 DIRECT
// packet of GS load-image descriptors, each a sceGsLoadImage followed by its
// giftag1.NLOOP texel quadwords (gra3dTRI2.c). LoadTRI2Files sets
// uiVif1Code_FLUSH before handing it over.
struct SGDTRI2FILEHEADER                /* 0x70 */
{
    /* 0x00 */ u_int  uiVif1Code_NOP0;
    /* 0x04 */ u_int  uiVif1Code_NOP1;
    /* 0x08 */ u_int  uiVif1Code_FLUSH;
    /* 0x0c */ u_int  uiVif1Code_DIRECT;        // low half: quadwords of the image packet
    /* 0x10 */ sceGsLoadImage gsli;             // the first descriptor

    // The DIRECT packet's quadword count (FF3 reads the halfword at +0xc).
    u_int GetTRI2Size() const
    {
        return (u_short)uiVif1Code_DIRECT;
    }
};
static_assert(sizeof(SGDTRI2FILEHEADER) == 0x70, "SGDTRI2FILEHEADER is 0x70 bytes");
static_assert(offsetof(SGDTRI2FILEHEADER, uiVif1Code_FLUSH) == 0x8, "SGDTRI2FILEHEADER.uiVif1Code_FLUSH");
static_assert(offsetof(SGDTRI2FILEHEADER, uiVif1Code_DIRECT) == 0xc, "SGDTRI2FILEHEADER.uiVif1Code_DIRECT");
static_assert(offsetof(SGDTRI2FILEHEADER, gsli) == 0x10, "SGDTRI2FILEHEADER.gsli");
static_assert(offsetof(sceGsLoadImage, bitbltbuf) == 0x10, "sceGsLoadImage.bitbltbuf");
static_assert(offsetof(sceGsLoadImage, trxreg) == 0x30, "sceGsLoadImage.trxreg");
static_assert(offsetof(sceGsLoadImage, giftag1) == 0x50, "sceGsLoadImage.giftag1");

// GS memory a run of TRI2 files covers (FF2 layout; RebuildTRI2Files zeroes
// 0x14 bytes, sets uiMinAddress to -1 and stores the page count at +0x10).
struct TRI2SIZEDATA                     /* 0x14 */
{
    /* 0x00 */ u_int uiMaxAddress;
    /* 0x04 */ u_int uiMinAddress;
    /* 0x08 */ u_int uiVRAMTexSize;             // quadwords of the image at uiMaxTbp
    /* 0x0c */ u_int uiMaxTbp;                  // highest DBP seen
    /* 0x10 */ u_int uiPageSize;                // gra3dCalcVRAMPageSize of the span
};
static_assert(sizeof(TRI2SIZEDATA) == 0x14, "TRI2SIZEDATA is 0x14 bytes");
static_assert(offsetof(TRI2SIZEDATA, uiMinAddress) == 0x04, "TRI2SIZEDATA.uiMinAddress");
static_assert(offsetof(TRI2SIZEDATA, uiVRAMTexSize) == 0x08, "TRI2SIZEDATA.uiVRAMTexSize");
static_assert(offsetof(TRI2SIZEDATA, uiMaxTbp) == 0x0c, "TRI2SIZEDATA.uiMaxTbp");
static_assert(offsetof(TRI2SIZEDATA, uiPageSize) == 0x10, "TRI2SIZEDATA.uiPageSize");

#endif /* _GRAPHICS_GRAPH3D_SGD_TYPES_H */
