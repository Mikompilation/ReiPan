// ingame/map/MapDBuff.c -- CMapDBuff, the opened door's second model, and
// the special-event put pair.
// FF3 0x0010dbe8..0x0010e230; FF3-only (the class file sorts between Load
// and MapRec; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/map/MapDBuff.h"

#include <string.h>                     // memset

#include "graphics/graph3d/gra3d.h"     // gra3dSetLightData, GRA3DLIGHTDATA
#include "graphics/mmanage.h"           // ModelManager (ExecPrelightMatrix, SetMatrix)
#include "graphics/motion/motion.h"     // MDL_MOT
#include "ingame/event/prg/ev_change.h" // GetEvDreamPlayerWrk
#include "ingame/map/LightCtl.h"        // GetLightCtl, CLightCtl::Apply
#include "ingame/map/MapDraw.h"         // MapDrawGetLightPtr
#include "ingame/map/MapLight.h"        // MapLightSetPlayerOnly
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetRoomNoNow
#include "ingame/map/MapPut.h"          // MapPut*
#include "ingame/photo/finder_photo.h"  // GetAlbumSaveDisp
#include "ingame/plyr/plyr_gauge.h"     // PlyrGaugeTimeStep
#include "ingame/map/MapDoor.h"  // door table, door motion pack
#include "reipan/reipan_memory.h"       // ReiPan_GetHostPointer
#include "system/eeiop/cd_file_dat.h"   // D000_PK4

// ──────────────────────────────────────────────────────────────────────
// The special-event puts (sbss 0x372b68 gp-0x988, 0x372b6c gp-0x984).

static MAP_PUT *map_sp_appear_put;
static MAP_PUT *map_sp_vanish_put;

// The area whose puts swap at the event.
#define MAP_SP_EVENT_AREA 0x21

// ──────────────────────────────────────────────────────────────────────
// The put's draw callback while a CMapDBuff is on it: the frame's model,
// stepping the pair unless flags has 0x200.
// FF3 0x0010dbe8.

static void MapDBuffDrawFunc(MAP_PUT *put, u_int flags)
{
    (static_cast<CMapDBuff *>(MapPutGetWork(put)))->Draw(((flags >> 9) ^ 1) & 1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010dc78 (MapDBuffCtor; FF3 0x0010dc20 is the same body).
// callcheck: LoadCtor (load is a member: C++ constructs it)

CMapDBuff::CMapDBuff()
{
    mdl[0] = nullptr;
    mdl[1] = nullptr;
    cur    = 0;
    matrix = nullptr;
    put    = nullptr;
    state  = MAP_DBUFF_IDLE;
    memset(&own, 0, sizeof(own));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010dd10 (the base one 0x0010dcd0 is the same body, the deleting
// one 0x0010dd50 adds operator delete); load's destructor follows.
// callcheck: LoadDtor (load's destructor)

CMapDBuff::~CMapDBuff()
{
    own.Release();
}

// ──────────────────────────────────────────────────────────────────────
// Puts the pair on put: put draws through MapDBuffDrawFunc with its own
// model and this one in turn, both starting anime (anime < 0: nothing).
// FF3 0x0010dd90.

void CMapDBuff::Set(MAP_PUT *put, int anime)
{
    float step;

    if (anime < 0)
        return;

    mdl[0] = MapPutGetMdl(put);
    mdl[1] = &own;
    MapPutSetDrawFunc(put, MapDBuffDrawFunc, 0, 0, 0);
    SetMatrix(MapPutGetMatrix(put));
    MapPutSetWork(put, this);
    this->put = put;
    cur = 1;

    step = PlyrGaugeTimeStep(GetAlbumSaveDisp());

    mdl[0]->mot->PlayMotion(anime, 0, 0.0f, step);
    mdl[1]->mot->PlayMotion(anime, 0, 0.0f, step);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010de98.

void CMapDBuff::PutChange()
{
    MapPutChangeObj(put);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010deb0.

void CMapDBuff::LoadReq(int no)
{
    load.ReqAlloc(D000_PK4 + no, -1);
    state = MAP_DBUFF_LOADING;
}

// ──────────────────────────────────────────────────────────────────────
// 0 while the file loads; once it is in, builds the model from it with
// the door anime and is ready.
// FF3 0x0010dee8.

int CMapDBuff::IsReady()
{
    if (state == MAP_DBUFF_LOADING)
    {
        if (load.IsEnd() == 0)
            return 0;

        own.Build(load.addr, ReiPan_GetHostPointer(MapDoorAnimGetAddr()));
        state = MAP_DBUFF_READY;
    }

    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Model i's draw request with the put's room light at matrix.
// FF3 0x0010df70.

void CMapDBuff::ReqMdl(float *matrix, int i)
{
    float mtx[4][4];

    gra3dSetLightData(static_cast<GRA3DLIGHTDATA *>(MapPutGetLitPtr(put)), nullptr);
    memcpy(mtx, matrix, sizeof(mtx));
    mdl[i]->mot->ExecPrelightMatrix(&mtx[0][0]);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010dff8.

void CMapDBuff::DrawMdl(int i)
{
    mdl[i]->Draw(0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010e020.

void CMapDBuff::SetMatrix(float *matrix)
{
    this->matrix = matrix;
}

// ──────────────────────────────────────────────────────────────────────
// Draws model cur. A stepped frame first moves both models to the matrix,
// requests the current one in the put's light and refreshes area 0x12's
// lights, and afterwards flips cur.
// FF3 0x0010e028.

void CMapDBuff::Draw(int step)
{
    void *dat;

    if (matrix == nullptr)
        return;

    if (step != 0)
    {
        mdl[0]->mot->GetBoneMotion();    // result unused
        mdl[0]->mot->SetMatrix(matrix);
        mdl[1]->mot->SetMatrix(matrix);
        ReqMdl(matrix, cur);
        MapLightSetPlayerOnly();

        if (MapLoadGetBuffID(0x12) >= 0)
        {
            dat = MapDrawGetLightPtr(0x12, 0);
            GetLightCtl()->Apply((GRA3DLIGHTDATA *)dat, 0x12);
        }
    }

    DrawMdl(cur);

    if (step != 0)
        cur ^= 1;
}

// ──────────────────────────────────────────────────────────────────────
// Flag 8 (hidden) of a put on or off; FF3 writes it inline (the callers'
// MapPutGetFlag calls are here).

static void MapSpEventPutHide(MAP_PUT *put, int hide)
{
    u_int *flag = MapPutGetFlag(put);

    // PC: MapPutGetFlag gives nullptr for an unset put (and while MapPut.c
    // is stubs); FF3 would write through it.
    if (flag == nullptr)
        return;

    if (hide != 0)
        *flag |= 8;
    else
        *flag &= ~8u;
}

// ──────────────────────────────────────────────────────────────────────
// A put that appears with the special event: hidden until it ran.
// FF3 0x0010e120 (MapDraw).
// callcheck: MapPutGetFlag (in MapSpEventPutHide)

void MapSpEventPutAppear(MAP_PUT *put)
{
    if (GetEvDreamPlayerWrk()[1] == 0)
    {
        map_sp_appear_put = put;
        MapSpEventPutHide(put, 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A put that vanishes with the special event: hidden once it ran.
// FF3 0x0010e168 (MapDraw).
// callcheck: MapPutGetFlag (in MapSpEventPutHide)

void MapSpEventPutVanish(MAP_PUT *put)
{
    if (GetEvDreamPlayerWrk()[1] == 0)
        map_sp_vanish_put = put;
    else
        MapSpEventPutHide(put, 1);
}

// ──────────────────────────────────────────────────────────────────────
// SET_SP_EVENT 0: the event has run; in its area the two puts swap.
// FF3 0x0010e1b8.
// callcheck: MapPutGetFlag (in MapSpEventPutHide)

void MapSpEventSet(void)
{
    if (GetEvDreamPlayerWrk()[1] != 0)
        return;

    GetEvDreamPlayerWrk()[1] = 1;

    if (MapLoadGetRoomNoNow() == MAP_SP_EVENT_AREA)
    {
        MapSpEventPutHide(map_sp_vanish_put, 1);
        MapSpEventPutHide(map_sp_appear_put, 0);
    }
}
