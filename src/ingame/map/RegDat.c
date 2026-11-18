// ingame/map/RegDat.c -- map registration data: the resident registration
// files of the loaded rooms and the by-label / by-position lookups over them.
// FF3 0x0011f668..0x00120800; FF2 RegDat.c (MioPan port), in FF2's order.
//
// FF3 against FF2: 32 slots (FF2 8) and 26 record types (FF2 14); the slot
// pointer, the file pointer and the record stride are real functions
// (RegDatGetBuffListPtr, RegDatGetTopAddr, RegDatNextSt) where FF2 inlines
// them; FF2's per-slot position test became its own function
// (RegDatCheckBuffPos) under RegDatGetBuffIDG; RegDatRegist no longer tests
// the "pzb" magic or flips Y / Z; the error prints and the assert are gone.
//
// PC: registration files live in emulated EE memory (the room PK4).
// RegDatRegist stores absolute EE addresses in the header, as the ROM does,
// and RegDatVecTop / RegDatStTop follow them.

#include "ingame/map/RegDat.h"

#include <stdio.h>                              // printf
#include <string.h>                             // memset

#include "ingame/map/hit_check_base.h"          // HcBaseIsInTriXZ
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer, ReiPan_GetPs2Address

static RD_REG_HEAD RegDatBuff[REG_DAT_BUFF_NUM];    /* bss 0x38e6e0 */

// Position-lookup state: RegDatGetBuffIDG rebuilds the hit list on every
// call; the no-register list holds slots the lookups skip (the hidden room
// buffers, MapLoadSetNoRegList / RegListExe).
static int RegDatNoRegNum;                          /* sdata 0x36b8f8 */
static int RegDatHitNum;                            /* sdata 0x36b8fc */
static int RegDatHitList[REG_DAT_BUFF_NUM];         /* bss 0x390460 */
static int RegDatNoRegList[REG_DAT_BUFF_NUM];       /* bss 0x3904e0 */

// PC: the header's two runs, from the EE addresses RegDatRegist stored.
static MB_OUT_RECT *RegDatVecTop(MB_OUT_HEAD *hp)
{
    return (MB_OUT_RECT *)ReiPan_GetHostPointer(hp->reg_vecp);
}

static MB_OUT_SECTION *RegDatStTop(MB_OUT_HEAD *hp)
{
    return (MB_OUT_SECTION *)ReiPan_GetHostPointer(hp->reg_stp);
}

// FF3 0x0011f668
RD_REG_HEAD *RegDatGetBuffListPtr(int buff_id)
{
    return &RegDatBuff[buff_id];
}

// FF3 0x0011f688. The null test can never fire; it is the ROM's.
MB_OUT_HEAD *RegDatGetTopAddr(int buff_id)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    if (hp == nullptr)
    {
        return nullptr;
    }
    return hp->RegDatPtr;
}

// FF3 0x0011f6b8
int RegDatSetTopAddr(int buff_id, MB_OUT_HEAD *addr)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    if (hp == nullptr)
    {
        return -1;
    }
    hp->RegDatPtr = addr;
    return 0;
}

// FF3 0x0011f6f0
MB_OUT_SECTION *RegDatNextSt(MB_OUT_SECTION *st)
{
    return (MB_OUT_SECTION *)((u_char *)st + st->size);
}

// Walks reg_id records down the section run. FF3 drops FF2's null and range
// checks.
// FF3 0x0011f700
static MB_OUT_SECTION *RegDatGetStPtrSub(MB_OUT_HEAD *hp, int reg_id)
{
    MB_OUT_SECTION *dp = RegDatStTop(hp);

    for (int i = reg_id; i > 0; i--)
    {
        dp = RegDatNextSt(dp);
    }
    return dp;
}

// FF3 0x0011f760
void *RegDatGetStPtr(int buff_id, int reg_id)
{
    MB_OUT_HEAD *hp = RegDatGetTopAddr(buff_id);

    // PC: the ROM walks from an empty slot's *(0 + 0x1c) (EE RAM, garbage);
    // that faults here. Only a caller with a stale slot id gets here.
    if (hp == nullptr)
    {
        return nullptr;
    }
    return RegDatGetStPtrSub(hp, reg_id);
}

// FF3 0x0011f740
u_int RegDatGetStLabel(int buff_id, int reg_id)
{
    MB_OUT_SECTION *dp = (MB_OUT_SECTION *)RegDatGetStPtr(buff_id, reg_id);

    // PC: the ROM reads dp->labelID unconditionally (see RegDatGetStPtr).
    if (dp == nullptr)
    {
        return 0;
    }
    return dp->labelID;
}

// FF3 0x0011f790
MB_OUT_RECT *RegDatGetVecPtr(int buff_id, int type)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    if ((u_int)type >= REG_DAT_ST_TYPE_NUM)
    {
        return nullptr;
    }
    return hp->StPtrList[type].dat;
}

// FF3 0x0011f7d0
int RegDatGetVecNum(int buff_id, int type)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    if ((u_int)type >= REG_DAT_ST_TYPE_NUM)
    {
        return -1;
    }
    return hp->StPtrList[type].st_num;
}

// FF3 0x0011f810
MB_OUT_RECT *RegDatGetVecPtrStart(int buff_id)
{
    MB_OUT_HEAD *hp = RegDatGetTopAddr(buff_id);

    return (hp != nullptr) ? RegDatVecTop(hp) : nullptr;
}

// FF3 0x0011f840
int RegDatGetVecNumAll(int buff_id)
{
    MB_OUT_HEAD *hp = RegDatGetTopAddr(buff_id);

    return (hp != nullptr) ? hp->reg_vec_num : -1;
}

// Rectangles are variable-length too; the caller counts.
// FF3 0x0011f870
MB_OUT_RECT *RegDatNextVec(MB_OUT_RECT *rec)
{
    return (MB_OUT_RECT *)((u_char *)rec + rec->size);
}

// A label encodes the area and the record: label / 1000 is the area id,
// label % 1000 the record's index in that area's section run.
// FF3 0x0011f880
int RegDatGetStID4Label(int buff_id, int label)
{
    MB_OUT_HEAD *hp    = RegDatGetTopAddr(buff_id);
    int          st_id = label % 1000;

    if (hp == nullptr)
    {
        return -1;
    }
    if (hp->area_id != label / 1000)
    {
        return -2;
    }
    if (st_id > hp->reg_st_num)
    {
        return -3;
    }
    if (st_id < 0)
    {
        return -3;
    }
    return st_id;
}

// FF3 0x0011f900
void *RegDatGetStPtr4Label(int buff_id, int label)
{
    int st_id = RegDatGetStID4Label(buff_id, label);

    if (st_id < 0)
    {
        return nullptr;
    }
    return RegDatGetStPtr(buff_id, st_id);
}

// RegDatGetStPtr4Label without the slot: the first slot holding the
// label's area (the area scan repeats RegDatBuffID4Label's, as in FF2).
// FF3 0x0011f948
void *RegDatGetStPtr4Label2(int label)
{
    int area = label / 1000;

    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        MB_OUT_HEAD *bp = RegDatGetTopAddr(i);

        if (bp != nullptr && bp->area_id == area)
        {
            int st_id = RegDatGetStID4Label(i, label);

            if (st_id < 0)
            {
                return nullptr;
            }
            return RegDatGetStPtr(i, st_id);
        }
    }
    return nullptr;
}

// As RegDatGetStPtr4Label2, but the record must be of type (its first
// halfword, SecStID). FF3 drops FF2's "RegDat No Type" assert.
// FF3 0x0011f9f8
void *RegDatGetStPtr4Label3(int label, int type)
{
    MB_OUT_SECTION *sp = (MB_OUT_SECTION *)RegDatGetStPtr4Label2(label);

    if (sp == nullptr)
    {
        return nullptr;
    }
    return ((int)sp->SecStID == type) ? sp : nullptr;
}

// Starts a walk over every rectangle bound to label. The cursor lives in the
// slot (LabVecID / LabVecNum / LabVecPtr), so a walk is per slot and not
// reentrant. The rectangle type is the section's own SecStID: rectangles
// are grouped by the type of the record they bind. 0, or -2 .. -5 for each
// way the lookup comes up empty.
// FF3 0x0011fa38
int RegDatVecFind4Label(int buff_id, int label)
{
    MB_OUT_SECTION *sp = (MB_OUT_SECTION *)RegDatGetStPtr4Label(buff_id, label);
    RD_REG_HEAD    *hp;

    if (sp == nullptr)
    {
        return -2;
    }
    hp = RegDatGetBuffListPtr(buff_id);

    hp->LabVecID = RegDatGetStID4Label(buff_id, label);
    if (hp->LabVecID < 0)
    {
        return -3;
    }

    hp->LabVecNum = RegDatGetVecNum(buff_id, sp->SecStID);
    if (hp->LabVecNum <= 0)
    {
        return -4;
    }

    hp->LabVecPtr = RegDatGetVecPtr(buff_id, sp->SecStID);
    return (hp->LabVecPtr != nullptr) ? 0 : -5;
}

// The next rectangle of the walk RegDatVecFind4Label started, nullptr when
// the type's run is exhausted; rectangles bound to other records are skipped.
// FF3 0x0011fae0
MB_OUT_RECT *RegDatVecNextFind(int buff_id)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    while (--hp->LabVecNum >= 0)
    {
        MB_OUT_RECT *rec = hp->LabVecPtr;

        if (rec->reg_id == hp->LabVecID)
        {
            hp->LabVecPtr = RegDatNextVec(rec);
            return rec;
        }
        hp->LabVecPtr = RegDatNextVec(rec);
    }
    return nullptr;
}

// Which slot holds label's area (label / 1000), -1.
// FF3 0x0011fb88
int RegDatBuffID4Label(int label)
{
    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        MB_OUT_HEAD *bp = RegDatGetTopAddr(i);

        if (bp != nullptr && bp->area_id == label / 1000)
        {
            return i;
        }
    }
    return -1;
}

// The first slot with no file, its per-type index wiped for
// RegDatRegistPtrList; -1 when all 32 are taken.
// FF3 0x0011fbf8
static int RegDatGetBuffFreeSpace(void)
{
    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        if (RegDatBuff[i].RegDatPtr == nullptr)
        {
            for (int j = 0; j < REG_DAT_ST_TYPE_NUM; j++)
            {
                RegDatBuff[i].StPtrList[j].dat    = nullptr;
                RegDatBuff[i].StPtrList[j].st_num = 0;
            }
            return i;
        }
    }
    return -1;
}

// Drops the file from the slot; the index and the cursors stay stale until
// the next RegDatRegist into it.
// FF3 0x0011fc60
void RegDatDeleteBuffList(int buff_id)
{
    if ((u_int)buff_id < REG_DAT_BUFF_NUM)
    {
        RegDatBuff[buff_id].RegDatPtr = nullptr;
    }
}

// FF3 0x0011fc88
void RegDatDeleteAllBuffList(void)
{
    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        RegDatDeleteBuffList(i);
    }
}

// The first rectangle whose section is of type RegStID, its index in the
// run into *num (when num is given). The rectangles are sorted by type, so
// the scan gives up once it is past it.
// FF3 0x0011fcc0
static MB_OUT_RECT *RegDatGetVecPtrSub(MB_OUT_HEAD *stp, int RegStID, int *num)
{
    MB_OUT_RECT *vp = RegDatVecTop(stp);

    for (int i = 0; i < stp->reg_vec_num; i++)
    {
        MB_OUT_SECTION *dp = RegDatGetStPtrSub(stp, vp->reg_id);

        if (dp == nullptr)
        {
            return nullptr;
        }
        if ((int)dp->SecStID == RegStID)
        {
            if (num != nullptr)
            {
                *num = i;
            }
            return vp;
        }
        if (RegStID < (int)dp->SecStID)
        {
            return nullptr;
        }
        vp = RegDatNextVec(vp);
    }
    return nullptr;
}

// How many consecutive rectangles carry type RegStID, from where
// RegDatGetVecPtrSub found the first.
// FF3 0x0011fd78
static int RegDatGetVecNumBin(MB_OUT_HEAD *stp, int RegStID)
{
    MB_OUT_RECT *vp;
    int          cnt;
    int          st_num = 0;

    vp = RegDatGetVecPtrSub(stp, RegStID, &cnt);
    if (vp == nullptr)
    {
        return 0;
    }

    for (int i = cnt; i < stp->reg_vec_num; i++)
    {
        MB_OUT_SECTION *dp = RegDatGetStPtrSub(stp, vp->reg_id);

        if (dp == nullptr)
        {
            return 0;
        }
        if ((int)dp->SecStID != RegStID)
        {
            return st_num;
        }
        st_num++;
        vp = RegDatNextVec(vp);
    }
    return st_num;
}

// The per-type index, built once at registration (FF2's RegDatGetVecPtrBin
// is inlined: RegDatGetVecPtrSub without the index).
// FF3 0x0011fe30
static void RegDatRegistPtrList(RD_REG_HEAD *hp)
{
    for (int i = 0; i < REG_DAT_ST_TYPE_NUM; i++)
    {
        hp->StPtrList[i].st_num = RegDatGetVecNumBin(hp->RegDatPtr, i);
        hp->StPtrList[i].dat    = RegDatGetVecPtrSub(hp->RegDatPtr, i, nullptr);
    }
}

// Slides every rectangle by (x, z) and flattens it onto the floor: the
// rectangles are 2D footprints, so y is forced to 0 and w to 1. y is unused,
// in the ROM as here.
// FF3 0x0011fea0
static void RegDatAddOffset(MB_OUT_HEAD *hp, float x, float y, float z)
{
    MB_OUT_RECT *rp = RegDatVecTop(hp);

    (void)y;
    for (int i = 0; i < hp->reg_vec_num; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            rp->vec[j][1]  = 0.0f;
            rp->vec[j][3]  = 1.0f;          /* sdata 0x36b900 */
            rp->vec[j][0] += x;
            rp->vec[j][2] += z;
        }
        rp = RegDatNextVec(rp);
    }
}

// Moves the whole area to (x, y, z): Pos holds the delta while the
// rectangles move, then the new position.
// FF3 0x0011ff58
void RegDatSetOffset(int buff_id, float x, float y, float z)
{
    MB_OUT_HEAD *hp;

    // PC: the ROM has no check; RegListRegist passes RegDatRegist's -2 when
    // all slots are taken, and an empty slot would be a null file.
    if ((u_int)buff_id >= REG_DAT_BUFF_NUM || (hp = RegDatGetTopAddr(buff_id)) == nullptr)
    {
        return;
    }

    hp->Pos[0] = x - hp->Pos[0];
    hp->Pos[1] = y - hp->Pos[1];
    hp->Pos[2] = z - hp->Pos[2];

    RegDatAddOffset(hp, hp->Pos[0], hp->Pos[1], hp->Pos[2]);

    hp->Pos[0] = x;
    hp->Pos[1] = y;
    hp->Pos[2] = z;
}

// Takes a loaded registration file into a free slot: its two run offsets
// become absolute addresses, the rectangles are indexed by type, then the
// file's own offset is baked into them. The slot id, -2 when none is free.
// FF3 0x0011ffe8
int RegDatRegist(MB_OUT_HEAD *mst)
{
    int          id = RegDatGetBuffFreeSpace();
    RD_REG_HEAD *rp;
    u_int        base;

    if (id < 0)
    {
        return -2;
    }

    // PC: the offsets become EE addresses, so the file must be in EE memory.
    base = (u_int)ReiPan_GetPs2Address(mst);
    if (base == 0)
    {
        printf("RegDatRegist: registration file %p is not in EE memory\n", (void *)mst);
        return -2;
    }
    mst->reg_stp  += base;
    mst->reg_vecp += base;

    rp            = RegDatGetBuffListPtr(id);
    rp->RegDatPtr = mst;

    RegDatRegistPtrList(rp);
    RegDatAddOffset(mst, mst->Pos[0], mst->Pos[1], mst->Pos[2]);
    return id;
}

// FF3 0x00120068
void RegDatInit(void)
{
    memset(RegDatBuff, 0, sizeof(RegDatBuff));

    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        RegDatBuff[i].RegDatPtr = nullptr;
        RegDatBuff[i].LabVecID  = -1;
        RegDatBuff[i].LabVecNum = -1;
        RegDatBuff[i].LabVecPtr = nullptr;
    }

    RegDatDeleteAllBuffList();
    RegDatResetNoRegistList();
}

// A point is inside a rectangle when it is inside either triangle of the quad.
// FF3 0x001200c8
int RegDatCheckHitRect(MB_OUT_RECT *mp, const float *vPos)
{
    if (HcBaseIsInTriXZ(vPos, mp->vec[0], mp->vec[1], mp->vec[2]) != 0)
    {
        return 1;
    }
    return HcBaseIsInTriXZ(vPos, mp->vec[0], mp->vec[2], mp->vec[3]) != 0;
}

// FF2 RegDatGetBuffIDG's loop body for one slot: 1 when vPos is in one of
// its room outlines (type 6) on floor kai (-1 any), 0 when not, on another
// floor or on the no-register list, -1 for an empty slot.
// FF3 0x00120150
int RegDatCheckBuffPos(int buff_id, int kai, const float *vPos)
{
    MB_OUT_HEAD *bp = RegDatGetTopAddr(buff_id);
    MB_OUT_RECT *mp;
    int          cnt;

    if (bp == nullptr)
    {
        return -1;
    }
    if (kai != -1 && bp->kai != kai)
    {
        return 0;
    }

    for (int j = 0; j < RegDatNoRegNum; j++)
    {
        if (RegDatNoRegList[j] == buff_id)
        {
            return 0;
        }
    }

    mp = RegDatGetVecPtr(buff_id, RECORD_TYPE_ROOM_OUTLINE);
    if (mp == nullptr)
    {
        return 0;
    }
    cnt = RegDatGetVecNum(buff_id, RECORD_TYPE_ROOM_OUTLINE);
    for (int j = 0; j < cnt; j++)
    {
        if (RegDatCheckHitRect(mp, vPos) != 0)
        {
            return 1;
        }
        mp = RegDatNextVec(mp);
    }
    return 0;
}

// Which resident slot holds vPos: the one id, -1 none, -2 all 32 (impossible
// geometry), -3 several -- then the caller reads the hit list.
// FF3 0x00120250
int RegDatGetBuffIDG(int kai, const float *vPos)
{
    RegDatHitNum = 0;
    for (int i = 0; i < REG_DAT_BUFF_NUM; i++)
    {
        if (RegDatCheckBuffPos(i, kai, vPos) == 1)
        {
            RegDatHitList[RegDatHitNum++] = i;
        }
    }

    if (RegDatHitNum == 1)
    {
        return RegDatHitList[0];
    }
    if (RegDatHitNum < 2)
    {
        return (RegDatHitNum != 0) ? -3 : -1;
    }
    return (RegDatHitNum != REG_DAT_BUFF_NUM) ? -3 : -2;
}

// FF3 0x00120328
void *RegDatGetStat(int kai, float *vPos, int type)
{
    return RegDatGetRectAndStat(nullptr, nullptr, kai, vPos, type);
}

// As RegDatGetRectAndStat2, with the slot found from vPos. Where several
// rooms overlap every candidate slot is tried: without pRectStat the first
// hit wins; with it the search runs until it finds that record and falls
// back to the first hit.
// FF3 0x00120350
void *RegDatGetRectAndStat(MB_OUT_RECT **ppRect, void *pRectStat, int kai, float *vPos, int type)
{
    int   buff_id;
    int   HitNum;
    int  *pHitList;
    void *pRet = nullptr;

    if (ppRect != nullptr)
    {
        *ppRect = nullptr;
    }

    buff_id = RegDatGetBuffIDG(kai, vPos);
    if (buff_id >= 0)
    {
        return RegDatGetRectAndStat2(ppRect, pRectStat, buff_id, vPos, type);
    }
    if (buff_id == -1)
    {
        return nullptr;
    }

    HitNum   = RegDatGetHitNum();
    pHitList = RegDatGetHitList();

    if (pRectStat == nullptr)
    {
        for (int i = 0; i < HitNum; i++)
        {
            pRet = RegDatGetRectAndStat2(ppRect, nullptr, pHitList[i], vPos, type);
            if (pRet != nullptr)
            {
                return pRet;
            }
        }
    }
    else
    {
        for (int i = 0; i < HitNum; i++)
        {
            void *pStat = RegDatGetRectAndStat2(ppRect, pRectStat, pHitList[i], vPos, type);

            if (pStat == pRectStat)
            {
                return pRectStat;
            }
            if (pStat != nullptr && pRet == nullptr)
            {
                pRet = pStat;
            }
        }
    }
    return pRet;
}

// The record whose type rectangle in slot buff_id holds vPos. pStat, when
// one matches, wins outright (what keeps a camera latched to the rectangle
// it uses); otherwise the first hit. *ppRect gets the matching rectangle.
// FF3 0x001204b0
void *RegDatGetRectAndStat2(MB_OUT_RECT **ppRect, void *pStat, int buff_id, float *vPos, int type)
{
    MB_OUT_RECT *mp;
    MB_OUT_RECT *pRetMbOutRect = nullptr;
    void        *pRet          = nullptr;
    int          cnt;

    if (ppRect != nullptr)
    {
        *ppRect = nullptr;
    }

    mp = RegDatGetVecPtr(buff_id, type);
    if (mp == nullptr)
    {
        return nullptr;
    }

    cnt = RegDatGetVecNum(buff_id, type);
    for (int j = 0; j < cnt; j++)
    {
        if (RegDatCheckHitRect(mp, vPos) != 0)
        {
            void *pTmpStat = RegDatGetStPtr(buff_id, mp->reg_id);

            if (pTmpStat != nullptr)
            {
                if (pTmpStat == pStat)
                {
                    pRet          = pTmpStat;
                    pRetMbOutRect = mp;
                    break;
                }
                if (pRet == nullptr)
                {
                    pRet          = pTmpStat;
                    pRetMbOutRect = mp;
                }
            }
        }
        mp = RegDatNextVec(mp);
    }

    if (pRet == nullptr)
    {
        return nullptr;
    }
    if (ppRect != nullptr)
    {
        *ppRect = pRetMbOutRect;
    }
    return pRet;
}

// Non-zero while pStat is still a record covering vPos: the test that stops
// the map camera cutting every frame.
// FF3 0x001205d8
int RegDatCheckSameRectStat(void *pStat, int kai, float *vPos, int type)
{
    int  buff_id;
    int  HitNum;
    int *pHitList;

    if (pStat == nullptr)
    {
        return 0;
    }

    buff_id = RegDatGetBuffIDG(kai, vPos);
    if (buff_id >= 0)
    {
        return RegDatGetRectAndStat2(nullptr, pStat, buff_id, vPos, type) == pStat;
    }
    if (buff_id == -1)
    {
        return 0;
    }

    HitNum   = RegDatGetHitNum();
    pHitList = RegDatGetHitList();
    for (int i = 0; i < HitNum; i++)
    {
        if (RegDatGetRectAndStat2(nullptr, pStat, pHitList[i], vPos, type) == pStat)
        {
            return 1;
        }
    }
    return 0;
}

// FF3 0x001206d0
int *RegDatGetHitList(void)
{
    return RegDatHitList;
}

// FF3 0x001206e0
int RegDatGetHitNum(void)
{
    return RegDatHitNum;
}

// FF3 drops FF2's overflow print; the list is 32 long, one per slot.
// FF3 0x001206e8
void RegDatAddNoRegistList(int buff_id)
{
    RegDatNoRegList[RegDatNoRegNum++] = buff_id;
}

// FF3 0x00120710
void RegDatResetNoRegistList(void)
{
    RegDatNoRegNum = 0;
}

// Arms the slot's section cursor for a walk over every record of type.
// FF3 0x00120718
void RegDatGetStPtrStart(int buff_id, int type)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);

    if (hp == nullptr || hp->RegDatPtr == nullptr)
    {
        return;
    }
    hp->type_id         = type;
    hp->type_search_num = 0;
    hp->type_search_p   = RegDatStTop(hp->RegDatPtr);
}

// FF3 0x00120760
MB_OUT_SECTION *RegDatGetNextStPtr(int buff_id)
{
    RD_REG_HEAD *hp = RegDatGetBuffListPtr(buff_id);
    MB_OUT_HEAD *mp;

    if (hp == nullptr)
    {
        return nullptr;
    }
    mp = hp->RegDatPtr;
    if (mp == nullptr)
    {
        return nullptr;
    }

    while (hp->type_search_num < mp->reg_st_num)
    {
        MB_OUT_SECTION *wp = hp->type_search_p;

        hp->type_search_p = RegDatNextSt(wp);
        hp->type_search_num++;
        if ((int)wp->SecStID == hp->type_id)
        {
            return wp;
        }
    }
    return nullptr;
}
