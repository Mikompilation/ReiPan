/* ==========================================================================
 *  ingame/enemy/random_ghost.c
 *
 *  The random ghosts (see random_ghost.h). FF3-only, FF3 0x24da20..0x24f398.
 *
 *  FF3 0x0024f370 is a compiler atexit thunk (CandleControl's destructor 0x2ad380
 *  on GetCandleControl's static 0x41e540, this file's copy); the host runs
 *  the static's destructor itself.
 * ======================================================================== */
#include "ingame/enemy/random_ghost.h"

#include <math.h>                               // sqrtf
#include <string.h>                             // memset

#include "common/vu0math.h"                     // GetRandValIByF, RotLimitChk, RotVectorZXY
#include "ingame/candle_ctrl.h"                 // CandleControl, GetCandleControl
#include "ingame/enemy/ene_main.h"              // ENE_WRK, ene_wrk, SearchEneWrkNo, EneGetWrk, EneGetMinLevel
#include "ingame/enemy/enemy.h"                 // EneLoadStart, EneLoadReq, EneRelease, EneReleaseReq, EnemyStart, EneDat*
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/item/prg/item.h"               // ItemGetCnt
#include "ingame/map/map_bgm.h"                 // map_bgmFadeOut, map_bgmFadeIn
#include "ingame/map/map_rectangle.h"           // MrecIsInEvent
#include "ingame/plyr/player.h"                 // PLYR_WRK, PLYR_FLG_PZL
#include "ingame/plyr/unit_ctl.h"               // GetTrgtRotY
#include "ingame/sound/snd_ctrl.h"              // SndConvVolume
#include "ingame/story_menu.h"                  // IsLockTblOn
#include "reipan/os/reipan_rand.h"              // ReiPan_Rand, REIPAN_RAND_MAXF
#include "sdk/libvu0.h"                         // sceVu0AddVector
#include "system/eeiop/stream_auto.h"           // StreamAutoPlayPitch, StreamAutoFadeOut
#include "system/os/system.h"                   // GetPALMode

#include "ingame/enemy/random_ghost_dat.inc"    // rand_ghost_set (data 0x330c60), rand_ghost_zone_tbl (data 0x3329c0)

#define RAND_GHOST_CANDLE_ENE   0x10a           // the type-0 enemy loaded while the candle is out
#define RAND_GHOST_ZONE_END     0x7fffffff      // RAND_GHOST_ZONE.label ending a list
#define PLYR_FLG2_2000000       0x2000000       // PLYR_WRK.flags2 bit: no candle ghost (not traced)

RAND_GHOST_WRK  rand_ghost_wrk;                         // bss 0x3f7a78
RAND_GHOST_AREA rand_ghost_area[RAND_GHOST_AREA_NUM];   // bss 0x3f7ab0

// Appearance-kind weights in percent (sdata 0x372080): timer, door, camera, zone.
static const u_char rand_ghost_kind_weight[4] = { 20, 15, 15, 50 };

static void  RandGhostAreaRelease(short area);
static void  RandGhostReleaseCheck(short area);
static void  RandGhostKindSet(void);
static ENE_WRK *RandGhostFindEne(const RAND_GHOST_ROW *row);
static void  RandGhostSelect(void);
static int   RandGhostLiveRowNum(void);
static int   RandGhostRowNum(void);
static int   RandGhostLiveNum(void);
static void  RandGhostAppear(void);
static const RAND_GHOST_ZONE *RandGhostGetZoneList(int area);
static int   RandGhostFollowCheck(void);
static void  RandGhostBgmStart(void);
static void  RandGhostZoneCheck(void);
static void  RandGhostZoneNearest(void);
static s_char RandGhostZoneNum(short area);

// GetCandleControl through the file's cache (FF3: sdata gp-0xb90, the inline
// getter's copy).
static CandleControl *RandGhostCandle(void)
{
    static CandleControl *candle;

    if (candle == nullptr)
        candle = GetCandleControl();
    return candle;
}

// The ghost set of the current area, nullptr when it has none.
static const RAND_GHOST_ROW *RandGhostCurSet(void)
{
    if (rand_ghost_wrk.area < 0 || rand_ghost_area[rand_ghost_wrk.area].set < 0)
        return nullptr;
    return rand_ghost_set[rand_ghost_area[rand_ghost_wrk.area].set];
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0024da20
void RandGhostWrkClear(void)
{
    memset(rand_ghost_area, 0, sizeof(rand_ghost_area));
    RandGhostAllOff();
    memset(&rand_ghost_wrk.info, 0, sizeof(rand_ghost_wrk.info));
    rand_ghost_wrk.fired     = 0;
    rand_ghost_wrk.stream    = -1;
    rand_ghost_wrk.area      = -1;
    rand_ghost_wrk.reroll    = 0;
    rand_ghost_wrk.adpcm     = -1;
    rand_ghost_wrk.follow      = nullptr;
    rand_ghost_wrk.candle_id = -1;
    RandGhostAllOff();
}

// FF3 0x0024daa0
void RandGhostSetAll(void)
{
    RandGhostKindSet();
    RandGhostSelect();
}

// Each frame (EnemyMain): the timer counts down while fewer than two ghosts
// act and the player is not crawling; at 0 the first frame checks whether
// the next ghost may follow the last one, then lets the timer kind appear,
// else maybe starts its voice stream; from then on the zones are watched. Then the candle-out
// enemy and the release checks.
// callcheck: GetCandleControl (through RandGhostCandle, the file's cache)
// FF3 0x0024dac0
void RandGhostMain(void)
{
    PLYR_WRK *plyr;
    short     area;
    float     step;
    int       n;
    int       i;

    if (IsLockTblOn(0x17) || IsLockTblOn(8) || IsLockTblOn(9))
        return;
    area = rand_ghost_wrk.area;
    if (area < 0)
        return;

    plyr = ingame_wrk.plyr_wrk;
    if (rand_ghost_area[area].off == 0 && rand_ghost_area[area].set >= 0)
    {
        step = RandGhostCandle()->out != 0 ? 2.5f : 1.0f;

        if (RandGhostLiveRowNum() < RandGhostRowNum())
        {
            if (rand_ghost_wrk.reroll == 1)
            {
                RandGhostSetAll();
                RandGhostTimeSet(-1);
                rand_ghost_wrk.reroll = 0;
            }
            else if (0.0f <= rand_ghost_wrk.info.timer - step)
            {
                if (RandGhostLiveNum() < 2 && (plyr->flags & PLYR_FLG_PZL) == 0)
                    rand_ghost_wrk.info.timer -= step;
            }
            else
            {
                rand_ghost_wrk.info.timer = 0.0f;
                if (rand_ghost_wrk.fired == 0)
                {
                    rand_ghost_wrk.fired = 1;
                    if (RandGhostFollowCheck() == 1 && RandGhostAppearReq(RAND_GHOST_KIND_TIMER) != 1)
                        RandGhostBgmStart();
                }
                RandGhostZoneCheck();
            }
        }
        else
        {
            rand_ghost_wrk.reroll = 1;
        }

        if (RandGhostCandle()->out == 0)
        {
            if (SearchEneWrkNo(0, RAND_GHOST_CANDLE_ENE) == -1)
                rand_ghost_wrk.candle_id = -1;
        }
        else if (rand_ghost_wrk.candle_id == -1 && (plyr->flags & PLYR_FLG_PZL) == 0
                 && (plyr->flags2 & PLYR_FLG2_2000000) == 0)
        {
            rand_ghost_wrk.candle_id = RAND_GHOST_CANDLE_ENE;
            RandGhostReleaseAll();
            if (EneLoadStart(0, rand_ghost_wrk.candle_id) == 0)
                rand_ghost_wrk.candle_id = -2;
        }
    }

    RandGhostReleaseCheck(area);

    // The last appearance is forgotten once no ghost of this area acts.
    n = 0;
    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state == ENE_STATE_ACT && ene->hp != 0 && ene->type == 1 && ene->rg.area == area)
            n++;
    }
    if (n == 0 && rand_ghost_wrk.follow != nullptr)
        rand_ghost_wrk.follow = nullptr;
}

// FF3 0x0024dd98
void RandGhostAreaChange(short area)
{
    if (rand_ghost_wrk.area == area)
        return;

    rand_ghost_wrk.area = area;
    RandGhostAreaRelease(area);
    RandGhostSetAll();
    rand_ghost_wrk.follow = nullptr;
    if (rand_ghost_wrk.candle_id == -2)
        rand_ghost_wrk.candle_id = -1;
    rand_ghost_wrk.fired = 0;
    if (rand_ghost_wrk.adpcm != -1)
    {
        StreamAutoFadeOut(rand_ghost_wrk.stream, 30);
        rand_ghost_wrk.stream = -1;
        map_bgmFadeIn(30);
        rand_ghost_wrk.adpcm = -1;
        RandGhostTimeSet(-1);
    }
}

// On an area change: ghosts still loading go; an acting once-row ghost
// counts the change and goes after its life; any other acting ghost goes
// unless its row lists the new area.
// The ROM's area scan reuses the slot counter: after the first acting
// ghost of an ordinary row the counter restarts at 6 while the slot
// pointer goes on, so later slots are checked up to slot index + 10.
// FF3 0x0024de48
static void RandGhostAreaRelease(short area)
{
    ENE_WRK *ene = ene_wrk;
    int      i = 0;
    int      n;

    do
    {
        // PC: the ROM's pointer runs on past ene_wrk[15] when the counter
        // jumped back (it reads the .data after it); stop at the array end.
        if (ene >= &ene_wrk[ENE_WRK_MAX])
            break;

        if (ene->type == 1)
        {
            if ((u_int)(ene->state - 1) < 2)
            {
                EneRelease(ene->alg.idx, 1);
            }
            else if (ene->state == ENE_STATE_ACT && ene->hp != 0)
            {
                if (ene->rg.row->once == 1)
                {
                    if (++ene->rg.cnt >= ene->rg.life)
                        EneRelease(ene->alg.idx, 1);
                }
                else
                {
                    for (i = 0, n = 0; i < 5; i++)
                    {
                        if (area == ene->rg.row->area[i])
                            n++;
                    }
                    if (n == 0)
                        EneRelease(ene->alg.idx, 1);
                }
            }
        }
        i++;
        ene++;
    } while (i < ENE_WRK_MAX);
}

// Every frame: acting ghosts of a switched-off area, or of a level above
// the weakest live enemy's, go; so do the others while the player crawls.
// The candle-out enemy goes with the area, the crawl or flags2 0x2000000.
// FF3 0x0024df58
static void RandGhostReleaseCheck(short area)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    s_char    level = EneGetMinLevel();
    int       i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->state != ENE_STATE_ACT || ene->hp == 0 || ene->type != 1)
            continue;
        if (rand_ghost_area[ene->rg.area].off == 1 || rand_ghost_area[area].off == 1
            || level < (int)ene->rg.row->level || (plyr->flags & PLYR_FLG_PZL) != 0)
        {
            EneRelease(ene->alg.idx, 1);
        }
    }

    if (rand_ghost_wrk.candle_id >= 0
        && (rand_ghost_area[area].off == 1 || (plyr->flags & PLYR_FLG_PZL) != 0
            || (plyr->flags2 & PLYR_FLG2_2000000) != 0))
    {
        EneReleaseReq(0, rand_ghost_wrk.candle_id);
        rand_ghost_wrk.candle_id = -1;
    }
}

// FF3 0x0024e0f8
void RandGhostReleaseAll(void)
{
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].type == 1)
            EneRelease(ene_wrk[i].alg.idx, 1);
    }
}

// FF3 0x0024e158
void RandGhostCandleRelease(void)
{
    if (EneGetWrk(0, RAND_GHOST_CANDLE_ENE) != nullptr)
        EneReleaseReq(0, RAND_GHOST_CANDLE_ENE);
}

// FF3 0x0024e190
void RandGhostCntReset(ENE_WRK *ene)
{
    ene->rg.cnt = 1;
}

// time -1: 2700..4500 frames (PAL 2250..3750) at random. With the candle
// out always 600 (PAL 500). Re-arms the timer kind.
// callcheck: GetCandleControl (through RandGhostCandle, the file's cache)
// FF3 0x0024e1a0
void RandGhostTimeSet(int time)
{
    int lo, hi;

    if (RandGhostCandle()->out == 0)
    {
        if (time == -1)
        {
            lo = GetPALMode() != 0 ? 2250 : 2700;
            hi = (GetPALMode() != 0 ? 2250 : 2700) + (GetPALMode() != 0 ? 1500 : 1800);
            time = (int)((float)lo + (float)(hi - lo) * ((float)ReiPan_Rand() / REIPAN_RAND_MAXF));   // callcheck: rand (ReiPan_Rand: the EE's range)
        }
        rand_ghost_wrk.info.timer = (float)time;
    }
    else
    {
        rand_ghost_wrk.info.timer = GetPALMode() != 0 ? 500.0f : 600.0f;
    }
    rand_ghost_wrk.fired = 0;
}

// Rolls the appearance kind: the camera kind needs item 8 and the zone kind
// zones in the area, else their weight goes to the timer kind. The candle
// out: always the timer kind.
// callcheck: GetCandleControl (through RandGhostCandle, the file's cache)
// FF3 0x0024e2c0
static void RandGhostKindSet(void)
{
    u_char weight[4];
    s_char r = (s_char)GetRandValIByF(100);
    int    sum;
    int    i;

    if (RandGhostCandle()->out != 0)
    {
        rand_ghost_wrk.info.kind = RAND_GHOST_KIND_TIMER;
        return;
    }

    for (i = 0; i < 4; i++)
        weight[i] = rand_ghost_kind_weight[i];
    if (*ItemGetCnt(8) == 0)
    {
        weight[0] += weight[2];
        weight[2] = 0;
    }
    if (RandGhostZoneNum(rand_ghost_wrk.area) == 0)
    {
        weight[0] += weight[3];
        weight[3] = 0;
    }

    sum = 0;
    for (i = 0; i < 4; i++)
    {
        sum += weight[i];
        if (r < sum)
        {
            rand_ghost_wrk.info.kind = i;
            return;
        }
    }
}

// The acting (or releasing) ghost spawned from row, nullptr: none.
// FF3 0x0024e3c0
static ENE_WRK *RandGhostFindEne(const RAND_GHOST_ROW *row)
{
    ENE_WRK *found = nullptr;
    u_int    i;

    for (i = 0; i < ENE_WRK_MAX && found == nullptr; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if ((u_int)(ene->state - 3) < 2 && ene->type == 1 && ene->rg.row == row)
            found = ene;
    }
    return found;
}

// Picks the next ghost from the area's set by weight among the rows whose
// ghost is not out yet (ghosts 0x15 / 0x1a weigh w * (int)(w * 2.5) with the
// candle out), loads it (EneLoadReq) and gives its work the info block.
// callcheck: GetCandleControl (through RandGhostCandle, the file's cache)
// FF3 0x0024e478
static void RandGhostSelect(void)
{
    const RAND_GHOST_ROW *set;
    int    weight[4];
    int    total;
    int    sum;
    int    area = rand_ghost_wrk.area;
    s_char r;
    short  id;
    int    i;

    if (area >= 0)
    {
        if (rand_ghost_area[area].set < 0)
            return;

        set   = rand_ghost_set[rand_ghost_area[area].set];
        total = 0;
        for (i = 0; i < RAND_GHOST_SET_ROW; i++)
        {
            const RAND_GHOST_ROW *row = &set[i];

            if (RandGhostFindEne(row) == nullptr && row->id != -1 && row->id_zone != -1)
            {
                short mdl  = EneDatGetMdlId(1, row->id);
                short mdlz = EneDatGetMdlId(1, row->id_zone);

                weight[i] = row->weight;
                if ((mdl == 0x15 || mdl == 0x1a || mdlz == 0x15 || mdlz == 0x1a)
                    && RandGhostCandle()->out != 0)
                {
                    weight[i] = weight[i] * (int)((float)weight[i] * 2.5f);
                }
            }
            else
            {
                weight[i] = 0;
            }
            total += weight[i];
        }

        if (total != 0)
        {
            r   = (s_char)GetRandValIByF(total);
            sum = 0;
            for (i = 0; i < RAND_GHOST_SET_ROW; i++)
            {
                sum += weight[i];
                if (r < sum)
                {
                    ENE_WRK *ene;

                    rand_ghost_wrk.info.area   = area;
                    rand_ghost_wrk.info.cnt    = 1;
                    rand_ghost_wrk.info.life   = (int)(((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 3.0f + 2.0f);   // callcheck: rand (ReiPan_Rand: the EE's range)
                    rand_ghost_wrk.info.row_no = i;
                    rand_ghost_wrk.info.row    = &set[i];
                    id = RandGhostGetId(&rand_ghost_wrk);
                    EneLoadReq(1, id);
                    ene = EneGetWrk(1, id);
                    if (ene == nullptr)
                        return;
                    ene->rg.zone   = rand_ghost_wrk.info.zone;
                    ene->rg.kind   = rand_ghost_wrk.info.kind;
                    ene->rg.area   = rand_ghost_wrk.info.area;
                    ene->rg.life   = rand_ghost_wrk.info.life;
                    ene->rg.cnt    = rand_ghost_wrk.info.cnt;
                    ene->rg.row_no = rand_ghost_wrk.info.row_no;
                    ene->rg.row    = rand_ghost_wrk.info.row;
                    return;
                }
            }
            return;
        }
    }
    rand_ghost_wrk.info.row_no = -1;
}

// FF3 0x0024e760
static int RandGhostLiveRowNum(void)
{
    const RAND_GHOST_ROW *set = RandGhostCurSet();
    int n = 0;
    int i;

    if (set == nullptr)
        return 0;
    for (i = 0; i < RAND_GHOST_SET_ROW; i++)
    {
        if (RandGhostFindEne(&set[i]) != nullptr)
            n++;
    }
    return n;
}

// FF3 0x0024e7f0
static int RandGhostRowNum(void)
{
    const RAND_GHOST_ROW *set = RandGhostCurSet();
    int n = 0;
    int i;

    if (set == nullptr)
        return 0;
    for (i = 0; i < RAND_GHOST_SET_ROW; i++)
    {
        if (set[i].weight != 0)
            n++;
    }
    return n;
}

// FF3 0x0024e860
static int RandGhostLiveNum(void)
{
    int n = 0;
    int i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *ene = &ene_wrk[i];

        if (ene->type == 1 && ene->state == ENE_STATE_ACT && ene->hp != 0)
            n++;
    }
    return n;
}

// FF3 0x0024e8b0
short RandGhostGetId(const RAND_GHOST_WRK *wrk)
{
    const RAND_GHOST_ROW *row;
    short id = -1;

    if (wrk != nullptr && (row = wrk->info.row) != nullptr)
    {
        if (wrk->info.kind == RAND_GHOST_KIND_ZONE)
        {
            id = row->id_zone;
            if (id == -1)
                id = row->id;
        }
        else
        {
            id = row->id;
        }
    }
    return id;
}

// Starts the selected ghost (not while lock 0x17 is on, except the door
// kind) unless its level is above the weakest live enemy's; then the next
// round is rolled.
// FF3 0x0024e8f8
static void RandGhostAppear(void)
{
    if (IsLockTblOn(0x17) && rand_ghost_wrk.info.kind != RAND_GHOST_KIND_DOOR)
        return;

    if (rand_ghost_wrk.info.row_no != -1)
    {
        short    id = RandGhostGetId(&rand_ghost_wrk);
        int      no = SearchEneWrkNo(1, id);
        ENE_WRK *ene;

        if (EneGetMinLevel() < (int)rand_ghost_wrk.info.row->level)
        {
            if (no != -1)
                EneRelease(no, 1);
        }
        else
        {
            RandGhostZoneNearest();
            ene = EneGetWrk(1, id);
            if (ene != nullptr)
            {
                ene->rg.zone = rand_ghost_wrk.info.zone;
                if (no >= 0)
                {
                    EnemyStart(no);
                    rand_ghost_wrk.follow = rand_ghost_wrk.info.row->follow;
                }
            }
        }
    }
    RandGhostTimeSet(-1);
    RandGhostSetAll();
}

// FF3 0x0024ea08
int RandGhostAppearReq(u_char kind)
{
    if (rand_ghost_wrk.area >= 0 && rand_ghost_area[rand_ghost_wrk.area].off == 0
        && rand_ghost_wrk.info.kind == kind && rand_ghost_wrk.info.timer <= 0.0f)
    {
        RandGhostAppear();
        return 1;
    }
    return 0;
}

// The start position by kind, at the player's height: the timer kind a
// spot 60..100 away in one of 36 directions, door 40 and camera 16 units
// ahead of the player, zone the zone's point.
// FF3 0x0024ea88
void RandGhostSetPos(ENE_WRK *ene)
{
    PLYR_WRK     *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR v;
    sceVu0FVECTOR rot;

    if (ene->rg.row_no == -1)
        return;

    switch (ene->rg.kind)
    {
    case RAND_GHOST_KIND_TIMER:
        v[0]   = 0.0f;
        v[1]   = 0.0f;
        v[2]   = ((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 40.0f + 60.0f;    // callcheck: rand (ReiPan_Rand: the EE's range)
        v[3]   = 0.0f;
        rot[0] = 0.0f;
        rot[1] = (float)(int)(((float)ReiPan_Rand() / REIPAN_RAND_MAXF) * 36.0f + 0.0f) * 0.17453292f - 3.1415927f;
        break;
    case RAND_GHOST_KIND_DOOR:
    case RAND_GHOST_KIND_CAM:
        v[0]   = 0.0f;
        v[1]   = 0.0f;
        v[2]   = ene->rg.kind == RAND_GHOST_KIND_DOOR ? 40.0f : 16.0f;
        v[3]   = 0.0f;
        rot[0] = 0.0f;
        rot[1] = plyr->rot[1];
        break;
    case RAND_GHOST_KIND_ZONE:
        ene->pos[0] = ene->rg.zone->pos[0];
        ene->pos[1] = ene->rg.zone->pos[1];
        ene->pos[2] = ene->rg.zone->pos[2];
        ene->pos[3] = ene->rg.zone->pos[3];
        return;
    default:
        return;
    }
    rot[3] = 0.0f;
    rot[2] = 0.0f;
    RotLimitChk(&rot[1]);
    RotVectorZXY(v, v, rot);
    sceVu0AddVector(v, plyr->pos, v);
    ene->pos[0] = v[0];
    ene->pos[1] = plyr->pos[1];
    ene->pos[2] = v[2];
    ene->pos[3] = v[3];
}

// Facing the player, or the zone's angle.
// FF3 0x0024ec80
void RandGhostSetRot(ENE_WRK *ene)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    float     rot;

    if (ene->rg.row_no == -1)
        return;

    if ((u_int)ene->rg.kind < RAND_GHOST_KIND_ZONE)
    {
        rot = GetTrgtRotY(ene->pos, plyr->pos);
        RotLimitChk(&rot);
    }
    else if (ene->rg.kind == RAND_GHOST_KIND_ZONE)
    {
        rot = (ene->rg.zone->rot * 3.1415927f) / 180.0f;
        RotLimitChk(&rot);
    }
    else
    {
        return;
    }
    ene->rot[3] = 0.0f;
    ene->rot[1] = rot;
    ene->rot[0] = 0.0f;
    ene->rot[2] = 0.0f;
}

// FF3 0x0024ed40
static const RAND_GHOST_ZONE *RandGhostGetZoneList(int area)
{
    if (area < 0)
        return nullptr;
    return rand_ghost_zone_tbl[area];
}

// After a ghost appeared, the next selected one may follow it by the first
// one's row: follow[next row] percent (c * (int)(c * 2.5) for ghosts 0x15 /
// 0x1a while the candle is out). Else the next ghost is dropped and the
// round rolled again, 0.
// callcheck: GetCandleControl (through RandGhostCandle, the file's cache)
// FF3 0x0024ed68
static int RandGhostFollowCheck(void)
{
    u_char r = (u_char)GetRandValIByF(100);

    if (rand_ghost_wrk.follow != nullptr)
    {
        short    id  = RandGhostGetId(&rand_ghost_wrk);
        int      no  = SearchEneWrkNo(1, id);
        ENE_WRK *ene = EneGetWrk(1, id);
        u_char   c   = rand_ghost_wrk.follow[rand_ghost_wrk.info.row_no];
        u_char   chance = c;

        if (ene != nullptr && (ene->cmn->id == 0x15 || ene->cmn->id == 0x1a)
            && RandGhostCandle()->out != 0)
        {
            chance = (u_char)(c * (int)((float)c * 2.5f));
        }
        if (chance < r)
        {
            // PC: the ROM releases slot -1 when the next ghost has no work.
            if (no >= 0)
                EneRelease(no, 1);
            RandGhostTimeSet(-1);
            RandGhostSetAll();
            return 0;
        }
    }
    return 1;
}

// With no enemy acting, by the row's chance: the selected ghost's voice
// stream over the map BGM ducked to silence.
// FF3 0x0024ee88
static void RandGhostBgmStart(void)
{
    u_char r = (u_char)GetRandValIByF(100);
    int    n = 0;
    short  id;
    int    i;

    for (i = 0; i < ENE_WRK_MAX; i++)
    {
        if (ene_wrk[i].state == ENE_STATE_ACT && ene_wrk[i].hp != 0)
            n++;
    }
    if (n != 0)
        return;
    // PC: with no row selected yet the ROM reads the chance from address 5.
    if (rand_ghost_wrk.info.row == nullptr)
        return;
    if (r > rand_ghost_wrk.info.row->bgm_chance)
        return;

    id = RandGhostGetId(&rand_ghost_wrk);
    if (rand_ghost_wrk.adpcm != -1)
        return;
    rand_ghost_wrk.adpcm = EneDatGetAdpcmNo(1, id);
    if (rand_ghost_wrk.adpcm != -1)
    {
        map_bgmFadeOut(0x3c, 0);
        rand_ghost_wrk.stream = StreamAutoPlayPitch(rand_ghost_wrk.adpcm, rand_ghost_wrk.adpcm + 1, 0xd, 1, 0,
                                                    SndConvVolume(100), 0x3c, nullptr, 0x1000);
    }
}

// The ghost's stream fades out and the map BGM comes back (the file stays
// latched until the next area change).
// FF3 0x0024ef88
void RandGhostBgmStop(void)
{
    if (rand_ghost_wrk.stream != -1)
    {
        StreamAutoFadeOut(rand_ghost_wrk.stream, 30);
        rand_ghost_wrk.stream = -1;
        map_bgmFadeIn(30);
    }
}

// The player inside one of the area's zone rectangles: the zone kind.
// FF3 0x0024efe8
static void RandGhostZoneCheck(void)
{
    const RAND_GHOST_ZONE *zone = RandGhostGetZoneList(rand_ghost_wrk.area);
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;

    if (zone == nullptr)
        return;
    for (; zone->label != RAND_GHOST_ZONE_END; zone++)
    {
        if (zone->label != -1 && MrecIsInEvent(plyr->pos, zone->label, plyr->floor))
        {
            rand_ghost_wrk.info.zone = zone;
            RandGhostAppearReq(RAND_GHOST_KIND_ZONE);
            return;
        }
    }
}

// The zone nearest the player on the ground plane (x / z; under 9999).
// FF3 0x0024f0b8
static void RandGhostZoneNearest(void)
{
    const RAND_GHOST_ZONE *zone = RandGhostGetZoneList(rand_ghost_wrk.area);
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    float     best = 9999.0f;

    if (zone == nullptr)
        return;
    for (; zone->label != RAND_GHOST_ZONE_END; zone++)
    {
        float dx, dz, d;

        if (zone->label == -1)
            continue;
        dx = plyr->pos[0] - zone->pos[0];
        dz = plyr->pos[2] - zone->pos[2];
        d  = sqrtf(dx * dx + dz * dz);
        if (d < best)
        {
            best = d;
            rand_ghost_wrk.info.zone = zone;
        }
    }
}

// FF3 0x0024f180
static s_char RandGhostZoneNum(short area)
{
    const RAND_GHOST_ZONE *zone = RandGhostGetZoneList(area);
    int n = 0;

    if (zone == nullptr)
        return 0;
    for (; zone->label != RAND_GHOST_ZONE_END; zone++)
    {
        if (zone->label != -1)
            n++;
    }
    return (s_char)n;
}

// FF3 0x0024f1f8
void RandGhostTimeReset(void)
{
    rand_ghost_wrk.info.kind = RAND_GHOST_KIND_TIMER;
    RandGhostTimeSet(GetPALMode() != 0 ? 500 : 600);
}

// FF3 0x0024f228
void RandGhostEneRelease(ENE_WRK *ene)
{
    if (ene->type == 0 && rand_ghost_wrk.candle_id == ene->id && rand_ghost_wrk.candle_id >= 0)
        rand_ghost_wrk.candle_id = -1;
}

// FF3 0x0024f260
void RandGhostInfoCopy(RAND_GHOST_INFO *dst, const RAND_GHOST_INFO *src)
{
    *dst = *src;
}

// FF3 0x0024f2a8
void RandGhostOn(int area, int set)
{
    if (area < RAND_GHOST_AREA_NUM)
    {
        rand_ghost_area[area].set = set;
        rand_ghost_area[area].off = 0;
        RandGhostSetAll();
    }
}

// FF3 0x0024f2e8
void RandGhostOff(int area)
{
    if (area < RAND_GHOST_AREA_NUM)
    {
        rand_ghost_area[area].off = 1;
        rand_ghost_area[area].set = -1;
    }
}

// FF3 0x0024f318
void RandGhostAllOff(void)
{
    int i;

    for (i = 0; i < RAND_GHOST_AREA_NUM; i++)
    {
        rand_ghost_area[i].off = 1;
        rand_ghost_area[i].set = -1;
    }
}

// FF3 0x0024f350
void RandGhostReq(void)
{
}

// FF3 0x0024f358
int RandGhostSaveGet(void **addr)
{
    *addr = rand_ghost_area;
    return sizeof(rand_ghost_area);
}
