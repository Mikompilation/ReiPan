// ingame/map/LightCtl.c -- CLightCtl, the blinking ("meimetu") room lights.
// FF3 0x0010ce28..0x0010d8b8; FF3-only (the class file sorts between
// GameClear and Load; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/map/LightCtl.h"

#include <string.h>                     // strstr

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/ecb.h"               // EcbGetNum / EcbGetData
#include "graphics/graph3d/gra3d.h"     // gra3dSetLight, gra3dLightEnable, gra3dApplyLight
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetFreeMemAddr, MapLoadGetRoomData, MapLoadCheckDrawFlg
#include "reipan/os/reipan_rand.h"      // ReiPan_Rand, REIPAN_RAND_MAXF
#include "system/eeiop/cd_file_dat.h"   // MEIMETU_ECB
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReqGetAddr, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The last CLightCtl constructed (sdata gp-0x7e68).
static CLightCtl *light_ctl;

// The light set CLightCtl::Apply last handed to gra3d (bss 0x378400).
static GRA3DLIGHTDATA light_data;

static int  LightCtlStepHold(STEP_SEQ *seq);
static int  LightCtlStepFall(STEP_SEQ *seq);
static int  LightCtlStepLow(STEP_SEQ *seq);
static int  LightCtlStepNext(STEP_SEQ *seq);

// ──────────────────────────────────────────────────────────────────────
// Area area's light names (area data member 0). FF3 0x0010ce28.

static LIGHT_NAME_DAT *LightCtlGetNameDat(int area)
{
    return (LIGHT_NAME_DAT *)MapLoadGetRoomData(MapLoadGetFreeMemAddr(MapLoadGetBuffID(area)), 0);
}

// ──────────────────────────────────────────────────────────────────────
// The current record. FF3 0x0010ce58.

LIGHT_CTL_REC *CLightCtl::GetRec()
{
    return (LIGHT_CTL_REC *)EcbGetData(ecb, rec);
}

// ──────────────────────────────────────────────────────────────────────
// The steps. Each waits for the running lerp, then starts the next one and
// moves on (STEP_SEQ::Run: STEP_NEXT); LightCtlStepNext jumps back to the
// first.

// Hold the level. FF3 0x0010ce78.
static int LightCtlStepHold(STEP_SEQ *seq)
{
    CLightCtl     *ctl  = (CLightCtl *)seq->owner;
    int            done = ctl->lerp.frames <= ctl->lerp.time;
    LIGHT_CTL_REC *rec;

    if (done)
    {
        rec = ctl->GetRec();
        ctl->lerp.Set(rec->level, rec->level, rec->hold);
    }
    return done;
}

// Fall to the low level. FF3 0x0010cee0.
static int LightCtlStepFall(STEP_SEQ *seq)
{
    CLightCtl     *ctl  = (CLightCtl *)seq->owner;
    int            done = ctl->lerp.frames <= ctl->lerp.time;
    LIGHT_CTL_REC *rec;

    if (done)
    {
        rec = ctl->GetRec();
        ctl->lerp.Set(rec->level, LIGHT_CTL_LOW, rec->fall);
    }
    return done;
}

// Stay low. FF3 0x0010cf50.
static int LightCtlStepLow(STEP_SEQ *seq)
{
    CLightCtl     *ctl  = (CLightCtl *)seq->owner;
    int            done = ctl->lerp.frames <= ctl->lerp.time;
    LIGHT_CTL_REC *rec;

    if (done)
    {
        rec = ctl->GetRec();
        ctl->lerp.Set(LIGHT_CTL_LOW, LIGHT_CTL_LOW, rec->low);
    }
    return done;
}

// The next record: a marker (a sequence's end) picks a new sequence at
// random, a blink rises to its level. Then back to LightCtlStepHold.
// FF3 0x0010cfc0.
static int LightCtlStepNext(STEP_SEQ *seq)
{
    CLightCtl     *ctl = (CLightCtl *)seq->owner;
    LIGHT_CTL_REC *rec;

    if (!(ctl->lerp.frames <= ctl->lerp.time))
    {
        return STEP_CONT;
    }

    ctl->rec++;
    rec = ctl->GetRec();
    if (rec->level < 0.0f)
    {
        ctl->PickSeq();
    }
    else
    {
        ctl->lerp.Set(LIGHT_CTL_LOW, rec->level, rec->rise);
    }
    seq->Jump(LightCtlStepHold);
    return STEP_NEXT;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d140 (LightCtlCtor; FF3 0x0010d080 is its twin, copying the
// same step list from 0x34c3c0). Registers itself for GetLightCtl and allocates the
// buffer meimetu.ecb loads into (CLightCtl::LoadReq).
// callcheck: StepSeqInit, LerpFClear (seq's and lerp's constructors, implicit)

CLightCtl::CLightCtl()
{
    // PC: FF3 copies only the four steps to the stack (rodata 0x34c3d0) and
    // STEP_SEQ::Add runs on into the saved s0 above them until a zero word, so
    // num can end up 5 with a junk fifth step. Step 3 always jumps back to
    // step 0, so the fifth never runs; here the list is terminated.
    static const STEP_FUNC steps[] = {
        LightCtlStepHold, LightCtlStepFall, LightCtlStepLow, LightCtlStepNext, nullptr,
    };

    rec = -1;
    seq.Add(steps);
    num       = 0;
    seq.owner = this;
    light_ctl = this;
    ecb       = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(MEIMETU_ECB), 0, 0, 0);
    // PC: FF3 leaves load_id to CLightCtl::LoadReq.
    load_id = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d250 (LightCtlDtor; FF3 0x0010d200 is the same body, the
// deleting one FF3 0x0010d2a0 adds operator delete).

CLightCtl::~CLightCtl()
{
    heapCtrlFree(GetSystemHeapWrkP(), ecb, 0, 0, 0);
    ecb       = nullptr;
    light_ctl = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d2f8.

CLightCtl *GetLightCtl(void)
{
    return light_ctl;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d300 (MapDoorModeNow sets it as the opened door's light data).

GRA3DLIGHTDATA *LightCtlGetLightData(void)
{
    return &light_data;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d310 (MapPlayStartReset, right after the ctor).

void CLightCtl::LoadReq()
{
    LoadReqGetAddr(MEIMETU_ECB, ecb, &load_id);
}

// ──────────────────────────────────────────────────────────────────────
// Starts one of the ctl->num sequences at random: the record after the
// k-th marker. Nothing changes when k comes out as num (rand() at its
// maximum). FF3 0x0010d330.
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)

void CLightCtl::PickSeq()
{
    int            i = 0;
    int            k = 0;
    int            n;
    int            pick;
    float          r;
    LIGHT_CTL_REC *rec;

    // PC: ReiPan_Rand is the ROM's 31-bit rand(); the host's is 15-bit.
    r    = (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
    pick = (int)((float)num * r + 0.0f);
    n    = EcbGetNum(ecb);

    for (i = 0; i < n; i++)
    {
        rec = (LIGHT_CTL_REC *)EcbGetData(ecb, i);
        if (rec->level == LIGHT_CTL_SEQ_START)
        {
            if (k == pick)
            {
                this->rec = i + 1;
                rec      = (LIGHT_CTL_REC *)EcbGetData(ecb, this->rec);
                lerp.Set(LIGHT_CTL_LOW, rec->level, rec->rise);
                return;
            }
            k++;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Once meimetu.ecb is in: count its sequences and start one
// (MapPlayStartIsReady). FF3 0x0010d448.

void CLightCtl::Start()
{
    int n = EcbGetNum(ecb);
    int i;

    num = 0;
    for (i = 0; i < n; i++)
    {
        if (((LIGHT_CTL_REC *)EcbGetData(ecb, i))->level == LIGHT_CTL_SEQ_START)
        {
            num++;
        }
    }
    PickSeq();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d4e0.

int CLightCtl::IsLoadEnd()
{
    return ::IsLoadEnd(load_id);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d4f8.

int LightCtlIsRoomDrawn(void)
{
    int buff = MapLoadGetBuffID(LIGHT_CTL_AREA);

    if (buff < 0)
    {
        return 0;
    }
    return MapLoadCheckDrawFlg(buff) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// One frame: the lerp, then the step that waits on it. FF3 0x0010d530
// (the FF2 match FurnCtlGetMotAddrEx was wrong).

void CLightCtl::Update()
{
    if (LightCtlIsRoomDrawn() != 0 && rec != -1)
    {
        lerp.Update();
        seq.Run();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Copies dat to light_data and, for every light of area's room whose name
// holds "meimetu", scales its diffuse colour (xyz) and ranges by the
// brightness percent and hands it to gra3d. Area 0x12 only, and only once
// a sequence runs. FF3 0x0010d588 (callers CMapDBuff::Draw and 0x1183e0).

void CLightCtl::Apply(GRA3DLIGHTDATA *dat, int area)
{
    LIGHT_NAME_DAT *names;
    G3DLIGHT       *dst;
    const G3DLIGHT *src;
    float           scale;
    int             id;

    if (LightCtlIsRoomDrawn() == 0 || area != LIGHT_CTL_AREA || rec == -1)
    {
        return;
    }

    scale = lerp.cur / 100.0f;     // data 0x36b6b0
    names = LightCtlGetNameDat(LIGHT_CTL_AREA);
    light_data = *dat;

    // FF3 runs two loops: points 3..26, then spots 30..37 (not the whole
    // GRA3D_START/END_LIGHT_POINT / _SPOT ranges).
    for (id = GRA3D_START_LIGHT_POINT; id < 0x26; id++)
    {
        if (id == 0x1b)
        {
            id = GRA3D_START_LIGHT_SPOT;
        }
        if (strstr(names->name[id], "meimetu") == nullptr)
        {
            continue;
        }

        src  = &dat->aLight[id];
        dst  = &light_data.aLight[id];
        *dst = *src;
        dst->fMaxRange   = src->fMaxRange * scale;      // +0x5c
        dst->fMinRange   = src->fMinRange * scale;      // +0x60
        dst->vDiffuse[0] = src->vDiffuse[0] * scale;    // vmulx.xyz: w kept
        dst->vDiffuse[1] = src->vDiffuse[1] * scale;
        dst->vDiffuse[2] = src->vDiffuse[2] * scale;
        gra3dSetLight(id, dst);
        gra3dLightEnable(id, 1);
        light_data.aStatus[id].bEnable = 1;
    }
    gra3dApplyLight();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010d898 (_GLOBAL__I_, .ctors 0x2e90f8) only jumps to the empty
// __static_initialization_and_destruction_0(1, 0xffff) at 0x0010d890:
// nothing to construct.
