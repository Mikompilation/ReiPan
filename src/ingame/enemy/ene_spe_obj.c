// ingame/enemy/ene_spe_obj.c -- the region-section list helpers and the
// real world's special SEs, FF3 0x001985c0..0x00199150. FF3-only (no FF2
// counterpart), written from the FF3 disassembly. GCC 3 emitted the
// object's constructor twice (C1/C2: 0x1987c8 / 0x198770); one body here.

#include "ingame/enemy/ene_spe_obj.h"

#include <math.h>                       // sqrtf
#include <new>                          // placement new
#include <string.h>                     // memset

#include "common/cpp_heap.h"            // operator_new, operator_delete
#include "common/vu0math.h"             // GetRandValF
#include "ingame/camera/cam_rail.h"     // CamCirclePos
#include "ingame/ingame_wrk.h"          // ingame_wrk.plyr_wrk
#include "ingame/map/MapLoad.h"         // MapLoadGetBuffID, MapLoadGetOffsetVector
#include "ingame/map/RegDat.h"          // MB_OUT_SECTION, MB_OUT_RECT, RegDat*
#include "ingame/plyr/player.h"         // PLYR_WRK pos / floor
#include "ingame/sound/snd_ctrl.h"      // SndSePlay, SndSeFadeStop, SndSeVolFade, SndSeIsPlaying, SndSeBankIsLoaded
#include "ingame/story_door.h"          // MapAreaNoGet, MapAreaSideGet
#include "system/eeiop/cd_file_dat.h"   // SYS_REAL_*_BD_BD
#include "system/eeiop/snd_buffer.h"    // SndBufSetPosition

// ──────────────────────────────────────────────────────────────────────
// Data

#define ENE_SPE_LIST_MAX    0x18        // EneSpeObjTblMain's region list (0x60 bytes of stack on the EE)
#define ENE_SPE_VOL         100         // SndSePlay volume (EneSpeObjCtrl fades it at once)
#define ENE_SPE_PITCH       0x1000      // SndSePlay pitch: unchanged
#define ENE_SPE_STOP_FADE   15          // SndSeFadeStop frames of Release / StopAll

#define SE_REAL1(n)         ((u_int)SYS_REAL_1_BD_BD << 16 | (n))
#define SE_REAL2(n)         ((u_int)SYS_REAL_2_BD_BD << 16 | (n))

// FF3 data 0x324060 (9 x 0x48). Rows 0 / 1 are one-shot SEs that travel
// 300 frames (group 0: one of the two at a time); the others loop in place.
static ENE_SPE_OBJ_DAT ene_spe_obj_dat[ENE_SPE_DAT_NUM] =
{
    // se, group, label, rate, ofs, loop, -, time, move, speed, angle, fade, far, min, near, delay, side, started, -
    { SE_REAL1(2), 0, 1019,  20.0f, {  38.0f, -20.0f, 300.0f }, 0, {}, 0.0f, 300.0f, 1.0f,  0.0f, 60.0f, 200.0f, 20.0f, 100.0f, 120.0f, ENE_SPE_SIDE_0,   0, {} },
    { SE_REAL1(3), 0, 1019,  20.0f, { 112.0f, -20.0f, 300.0f }, 0, {}, 0.0f, 300.0f, 1.0f, 50.0f, 60.0f, 200.0f, 20.0f, 100.0f, 120.0f, ENE_SPE_SIDE_0,   0, {} },
    { SE_REAL1(4), 1, 4009,   0.0f, { 152.0f, -25.0f,  86.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  50.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_ANY, 0, {} },
    { SE_REAL2(0), 2, 1020, 100.0f, {  89.0f, -19.0f, 126.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  50.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_ANY, 0, {} },
    { SE_REAL2(1), 3, 1021,  20.0f, {  41.0f, -19.0f, 121.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  50.0f, 20.0f,  20.0f, 120.0f, ENE_SPE_SIDE_1,   0, {} },
    { SE_REAL2(2), 4, 4009,   5.0f, { 154.0f, -19.0f, 120.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  50.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_0,   0, {} },
    { SE_REAL2(3), 5,    2, 100.0f, {  71.0f, -19.0f,  49.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  50.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_ANY, 0, {} },
    { SE_REAL2(4), 6, 1019, 100.0f, {  90.0f, -39.0f, 259.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  70.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_ANY, 0, {} },
    { SE_REAL2(5), 7, 1019,  20.0f, {  90.0f, -39.0f, 259.0f }, 1, {}, 0.0f,   0.0f, 0.0f,  0.0f, 10.0f,  70.0f, 20.0f,  20.0f,   0.0f, ENE_SPE_SIDE_ANY, 0, {} },
};

static sceVu0FVECTOR ene_spe_obj_origin = { 0.0f, 0.0f, 0.0f, 1.0f };    // rdata 0x329500

// The table, bss 0x3e0068. FF3 0x00199108 (the file's
// __static_initialization_and_destruction_0, EneSpeObjTblStaticInit)
// constructs it at start-up from FF3 0x00199130 (_GLOBAL__I_, the .ctors
// entry at 0x2e9130); the C++ static does it here.
static ENE_SPE_OBJ_TBL ene_spe_obj_tbl;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001985c0

int RegSecListHasLabel(int label, MB_OUT_SECTION **list, int num)
{
    for (int i = 0; i < num; i++)
    {
        if ((int)list[i]->labelID == label)
        {
            return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001985f8: max is not checked.

int RegSecListAddUnique(MB_OUT_SECTION **list, int max, int num, MB_OUT_SECTION *rec)
{
    (void)max;

    for (int i = 0; i < num; i++)
    {
        if (list[i] == rec)
        {
            return num;
        }
    }

    list[num] = rec;
    return num + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198640: every resident slot RegDatGetBuffIDG hit, every one of
// its rectangles of type that holds pos.

int RegSecListGet4Pos(float *pos, int floor, MB_OUT_SECTION **list, int max, int type)
{
    int  num;
    int  n;
    int *hit;

    if (RegDatGetBuffIDG(floor, pos) == -1)
    {
        return -1;
    }

    num = 0;
    n = RegDatGetHitNum();
    hit = RegDatGetHitList();

    for (; n > 0; n--, hit++)
    {
        int          buff = *hit;
        MB_OUT_RECT *rec = RegDatGetVecPtr(buff, type);
        int          vn;

        if (rec == nullptr)
        {
            continue;
        }

        for (vn = RegDatGetVecNum(buff, type); vn > 0; vn--)
        {
            if (RegDatCheckHitRect(rec, pos) != 0)
            {
                num = RegSecListAddUnique(list, max, num,
                                          (MB_OUT_SECTION *)RegDatGetStPtr(buff, rec->reg_id));
            }
            rec = RegDatNextVec(rec);
        }
    }

    return num;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001987c8 (EneSpeObjCtor; FF3 0x00198770 is the same body). se,
// delay and vel are left as they are; Start sets them.

ENE_SPE_OBJ::ENE_SPE_OBJ()
{
    active = 0;
    dat = nullptr;
    time = 0.0f;
    step = 1.0f;                                            /* rdata 0x370098 */
    move = 0.0f;
    pos[0] = pos[1] = pos[2] = pos[3] = 0.0f;               /* data 0x329530 */
    memset(&s3s, 0, sizeof(s3s));
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198820

int ENE_SPE_OBJ::Start(ENE_SPE_OBJ_DAT *d)
{
    if (active != 0)
    {
        return -1;
    }

    dat = d;
    se = -1;
    active = 1;

    MapLoadGetOffsetVector(pos, MapLoadGetBuffID(MapAreaNoGet()));
    pos[0] += d->ofs[0];
    pos[1] += d->ofs[1];
    pos[3] = 1.0f;
    pos[2] += d->ofs[2];

    CamCirclePos(d->speed, d->angle / 100.0f, vel, ene_spe_obj_origin);
    vel[3] = 0.0f;

    delay = GetRandValF(d->delay);

    time = d->time;
    if (time == 0.0f)
    {
        time = -1.0f;
    }
    move = d->move;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198938

void ENE_SPE_OBJ::Stop(int fade)
{
    if (se >= 0)
    {
        SndSeFadeStop(se, fade);
    }
    dat = nullptr;
    active = 0;
    se = -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001989e8 (returns 0, unused).

void ENE_SPE_OBJ::SePlay()
{
    s3s.pos = pos;
    se = SndSePlay(dat->se, ENE_SPE_VOL, ENE_SPE_PITCH, 0, &s3s, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198980: 100 within near, then linearly down to min at far, 0
// beyond far.

static float EneSpeObjDistVol(float near_dist, float far_dist, float min_vol, float d)
{
    float v = 0.0f;

    if (d < far_dist)
    {
        v = 100.0f;                                         /* rdata 0x3700a8 */
        if (near_dist < d)
        {
            v = (100.0f - min_vol) * (1.0f - (d - near_dist) / (far_dist - near_dist)) + min_vol;
        }
    }
    return v;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198a38: list / num are the player's regions (EneSpeObjTblMain).
// While the delay runs the player must stay in the row's region; once it
// plays, a looped row restarts the SE when it ends and stops when the
// player leaves, a one-shot row ends with its SE.

int ENE_SPE_OBJ::Ctrl(MB_OUT_SECTION **list, int num)
{
    ENE_SPE_OBJ_DAT *d = dat;
    PLYR_WRK        *plyr;
    float            dx, dy, dz;
    int              ended;

    if (delay >= 0.0f)
    {
        delay -= step;
        if (RegSecListHasLabel(d->label, list, num) == 0)
        {
            Stop(0);
            return 0;
        }
        if (delay < 0.0f)
        {
            SePlay();
        }
        return 0;
    }

    ended = 0;
    if (se < 0 || SndSeIsPlaying(se) == 0)
    {
        ended = 1;
        se = -1;
    }

    if (time == 0.0f)
    {
        Stop((int)d->fade);
        return 0;
    }
    if (time > 0.0f)
    {
        time -= step;
        if (time < 0.0f)
        {
            time = 0.0f;
        }
    }

    if (d->loop != 0)
    {
        if (ended)
        {
            SePlay();
        }
        else if (RegSecListHasLabel(d->label, list, num) == 0)
        {
            Stop((int)d->fade);
        }
    }

    if (se < 0)
    {
        Stop(0);
        return -1;
    }

    if (move > 0.0f)
    {
        pos[0] += vel[0];                                   // vadd.xyzw
        pos[1] += vel[1];
        pos[2] += vel[2];
        pos[3] += vel[3];
        SndBufSetPosition(se, pos);
        move -= step;
    }

    // vsub.xyz / vmul.xyz, x + y + z (hazards.md "VU0 macros that read vf1"), vsqrt
    plyr = ingame_wrk.plyr_wrk;
    dx = plyr->pos[0] - pos[0];
    dy = plyr->pos[1] - pos[1];
    dz = plyr->pos[2] - pos[2];

    SndSeVolFade(se, (int)((EneSpeObjDistVol(d->near_dist, d->far_dist, d->min_vol,
                                             sqrtf(dx * dx + dy * dy + dz * dz)) * 127.0f) / 100.0f), 0);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198c98: the row's side matches and no active object plays a row
// of its group.

int ENE_SPE_OBJ_TBL::CanStart(ENE_SPE_OBJ_DAT *d)
{
    int ok = 1;

    if (d->side == ENE_SPE_SIDE_1)
    {
        ok = (MapAreaSideGet() ^ 1) == 0;
    }
    else if (d->side == ENE_SPE_SIDE_0)
    {
        ok = MapAreaSideGet() == 0;
    }

    if (!ok)
    {
        return 0;
    }

    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        if (obj[i]->active != 0 && obj[i]->dat->group == d->group)
        {
            return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198d50

ENE_SPE_OBJ *ENE_SPE_OBJ_TBL::GetFree()
{
    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        if (obj[i]->active == 0)
        {
            return obj[i];
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198d88

ENE_SPE_OBJ_TBL *GetEneSpeObjTbl(void)
{
    return &ene_spe_obj_tbl;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198d98. A second Start without Release leaks the objects (FF3
// too).
// callcheck: EneSpeObjCtor (the new-expression)

void ENE_SPE_OBJ_TBL::Start()
{
    ready = 0;
    on = 1;

    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        obj[i] = new (operator_new(sizeof(ENE_SPE_OBJ))) ENE_SPE_OBJ;  // FF3 operator_new(0x50)
    }
    for (int i = 0; i < ENE_SPE_DAT_NUM; i++)
    {
        ene_spe_obj_dat[i].started = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198e28 (no destructor: operator_delete only).

void ENE_SPE_OBJ_TBL::Release()
{
    if (on == 0)
    {
        return;
    }

    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        if (obj[i]->active != 0)
        {
            obj[i]->Stop(ENE_SPE_STOP_FADE);
        }
        operator_delete(obj[i]);
        obj[i] = nullptr;
    }
    for (int i = 0; i < ENE_SPE_DAT_NUM; i++)
    {
        ene_spe_obj_dat[i].started = 0;
    }
    on = 0;
    ready = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198ed0 (Ghidra had the wrong FF2 match EventExeRelease).

void ENE_SPE_OBJ_TBL::StopAll()
{
    if (on == 0)
    {
        return;
    }

    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        if (obj[i]->active != 0)
        {
            obj[i]->Stop(ENE_SPE_STOP_FADE);
        }
    }
    for (int i = 0; i < ENE_SPE_DAT_NUM; i++)
    {
        ene_spe_obj_dat[i].started = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00198f58: rows whose region the player is in (rectangle type 2)
// roll their chance once per entry; a num of -1 (no slot under the player)
// counts as "in" for every row, as on the EE. GetFree cannot fail: the
// rows have eight groups and CanStart allows one object per group.

int ENE_SPE_OBJ_TBL::Main()
{
    MB_OUT_SECTION *list[ENE_SPE_LIST_MAX];
    PLYR_WRK       *plyr;
    int             num;

    if (on == 0)
    {
        return 0;
    }

    if (ready == 0)
    {
        if (SndSeBankIsLoaded(SYS_REAL_1_BD_BD) == 0 ||
            SndSeBankIsLoaded(SYS_REAL_2_BD_BD) == 0 ||
            SndSeBankIsLoaded(SYS_REAL_3_BD_BD) == 0)
        {
            return 0;
        }
        ready = 1;
    }

    plyr = ingame_wrk.plyr_wrk;
    num = RegSecListGet4Pos(plyr->pos, plyr->floor, list, ENE_SPE_LIST_MAX, 2);

    for (int i = 0; i < ENE_SPE_DAT_NUM; i++)
    {
        ENE_SPE_OBJ_DAT *d = &ene_spe_obj_dat[i];
        int              j;

        for (j = 0; j < num; j++)
        {
            if (d->label == (int)list[j]->labelID)
            {
                break;
            }
        }

        if (j == num)
        {
            d->started = 0;
            continue;
        }
        if (d->started == 1)
        {
            continue;
        }

        d->started = 1;
        if (GetRandValF(100.0f) <= d->rate && CanStart(d) != 0)
        {
            GetFree()->Start(d);
        }
    }

    for (int i = 0; i < ENE_SPE_OBJ_NUM; i++)
    {
        if (obj[i]->active != 0)
        {
            obj[i]->Ctrl(list, num);
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00199108 (EneSpeObjTblStaticInit), run by _GLOBAL__I_ 0x00199130.

ENE_SPE_OBJ_TBL::ENE_SPE_OBJ_TBL()
{
    on = 0;
    ready = 0;
}
