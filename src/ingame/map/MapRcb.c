// ingame/map/MapRcb.c -- the room's rcb file: named map objects made of floor
// quads, and the queries over them (floor of a hit area, hit-area switch
// zones, footstep sound areas, player motion zones, wall push-out, segment
// crossings). FF3-only; FF3 0x0011e820..0x0011f668 and 0x002ac870 (the line
// query's quad test, linked after the library).
//
// Every query is RcbForEachQuad with a callback and a 0x40-byte block on the
// stack (RCB_QUERY): the callback reads its input there and leaves its
// result there.
//
// PC: the file lives in emulated EE memory (the room's data, slot 3). RcbInit
// stores absolute EE addresses in it, as the ROM does, and the walks follow
// them through ReiPan_GetHostPointer.

#include "ingame/map/MapRcb.h"

#include <ctype.h>                              // isdigit
#include <math.h>                               // sqrtf
#include <stdio.h>                              // sscanf
#include <string.h>                             // memset, strcmp, strlen, strncmp

#include "ingame/camera/cam_rail.h"              // CamSegCrossXZ
#include "ingame/map/hit_check_base.h"          // HcBaseIsInRectXZ, HcBaseIsNearSegRect
#include "ingame/map/map_hit_check.h"           // MapHitPushOutQuad
#include "reipan/reipan_memory.h"               // PC: ReiPan_GetHostPointer, ReiPan_GetPs2Address


// The block a query hands its callbacks (0x40 bytes on the EE stack).
// layout: host -- stack only; obj / obj2 / list are EE words at 0x30 / 0x34.
struct RCB_QUERY
{
    /* 0x00 */ float          r;            // RcbPushOut: radius; RcbLineHit: the nearest crossing's XZ distance
    /* 0x10 */ alignas(16) float pos[4];    // the point / the segment start
    /* 0x20 */ alignas(16) float pos2[4];   // the segment end, pulled in to the nearest crossing
    /* 0x30 */ RCB_OBJ       *obj;
    /* 0x34 */ RCB_OBJ       *obj2;
    /* 0x30 */ int           *list;         // RcbGetSeNoList
    /* 0x38 */ int            hit;          // found / count
    /* 0x3c */ int            val;          // RcbGetFloor: the floor index
};
static_assert(offsetof(RCB_QUERY, pos) == 0x10, "RCB_QUERY.pos");
static_assert(offsetof(RCB_QUERY, pos2) == 0x20, "RCB_QUERY.pos2");

// RcbForEachQuad's {cb, arg} for RcbForEachQuadCb. layout: host -- stack only.
struct RCB_QUAD_WALK
{
    RCB_QUAD_CB cb;
    void       *arg;
};

// FF3 0x2f1530: the object name prefixes and their kinds.
static const struct
{
    const char *prefix;
    short       kind;
} rcb_kind_tbl[] = {
    { "hit_",    RCB_KIND_HIT },
    { "se_",     RCB_KIND_SE },
    { "pos_",    RCB_KIND_POS },
    { "change_", RCB_KIND_CHANGE },
    { "kai_",    RCB_KIND_KAI },
    { "mot_",    RCB_KIND_MOT },
    { "tourou_", RCB_KIND_TOUROU },
    { nullptr,   0 },
};

static int RcbForEachQuadCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg);
static int RcbFindObjInQuadCb(RCB_OBJ *obj, float **quad, void *arg);
static int RcbAddFloorMhNoCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg);
static int RcbPushOutCb(RCB_OBJ *obj, float **quad, void *arg);
static int RcbLineHitCb(RCB_OBJ *obj, float **quad, void *arg);
static int RcbLineHitQuad(RCB_QUERY *q, const float *quad);
static int RcbFindSeObjCb(RCB_OBJ *obj, float **quad, void *arg);
static int RcbAddSeNoCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg);
static int RcbFindMotObjCb(RCB_OBJ *obj, float **quad, void *arg);
static int RcbGetFloorCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg);

// PC: an EE address stored by RcbInit -> host pointer.
template <typename T>
static T *RcbPtr(u_int addr)
{
    return static_cast<T *>(ReiPan_GetHostPointer(addr));
}

// The callbacks' copy of a quad: its 4 vertices side by side (the ROM's lq / sq loop).
static void RcbCopyQuad(float (*out)[4], float **quad)
{
    for (int i = 0; i < 4; i++)
    {
        memcpy(out[i], quad[i], sizeof(out[i]));
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011e820: each "_" followed by a digit or "-" starts a number.
// The ROM stores every number it finds; past 9 only the result says so.

int RcbGetNamePrm(short *prm, const char *name)
{
    int len = (int)strlen(name);
    int n   = 0;

    for (int i = 0; i < len; i++)
    {
        if (name[i] == '_' && (isdigit((u_char)name[i + 1]) || name[i + 1] == '-'))
        {
            sscanf(&name[i + 1], "%hd", prm);
            prm++;
            n++;
        }
    }
    return (n < 10) ? n : -1;
}

// FF3 0x0011e908

short RcbSetObjKind(RCB_OBJ *obj)
{
    if (obj == nullptr)
    {
        return -2;
    }

    for (int i = 0; rcb_kind_tbl[i].prefix != nullptr; i++)
    {
        if (strncmp(obj->name, rcb_kind_tbl[i].prefix, strlen(rcb_kind_tbl[i].prefix)) == 0)
        {
            obj->kind = rcb_kind_tbl[i].kind;
            RcbGetNamePrm(obj->prm, obj->name);
            return rcb_kind_tbl[i].kind;
        }
    }
    return -1;
}

// FF3 0x0011e9c0

float *RcbGetVertex(RCB_GROUP *grp, int idx)
{
    if (grp == nullptr || idx < 0 || grp->vtx_num <= idx)
    {
        return nullptr;
    }
    return RcbPtr<float>(grp->vtx) + idx * 4;
}

// FF3 0x0011e9f8

int RcbIsRcb(void *rcb)
{
    return rcb != nullptr && strcmp(static_cast<const char *>(rcb), "rcb") == 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011ea28. "Relocated" is the ROM's test: the group pointer lies past
// the file's own address.

int RcbForEachObj(void *rcb, int kind, short val, int idx, RCB_OBJ_CB cb, void *arg)
{
    RCB_HEAD  *hd = static_cast<RCB_HEAD *>(rcb);
    RCB_GROUP *grp;

    if (RcbIsRcb(rcb) == 0)
    {
        return -1;
    }
    if (hd->group <= ReiPan_GetPs2Address(rcb))
    {
        return -2;
    }

    grp = RcbPtr<RCB_GROUP>(hd->group);
    for (int i = 0; i < hd->group_num; i++, grp++)
    {
        RCB_OBJ *obj = RcbPtr<RCB_OBJ>(grp->obj);

        for (int j = 0; j < grp->obj_num; j++, obj++)
        {
            if ((obj->kind == kind || kind == -1) && (idx < 0 || obj->prm[idx] == val))
            {
                if (cb(grp, obj, arg) != 0)
                {
                    return 0;
                }
            }
        }
    }
    return 0;
}

// FF3 0x0011eb68

static int RcbForEachQuadCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg)
{
    RCB_QUAD_WALK *walk = static_cast<RCB_QUAD_WALK *>(arg);
    RCB_QUAD      *quad = RcbPtr<RCB_QUAD>(obj->quad);
    float         *vtx[4];

    for (int i = 0; i < obj->quad_num; i++, quad++)
    {
        for (int k = 0; k < 4; k++)
        {
            vtx[k] = RcbGetVertex(grp, quad->vtx[k]);
        }
        if (walk->cb(obj, vtx, walk->arg) != 0)
        {
            return 1;
        }
    }
    return 0;
}

// FF3 0x0011ec38

int RcbForEachQuad(void *rcb, int kind, short val, int idx, RCB_QUAD_CB cb, void *arg)
{
    RCB_QUAD_WALK walk = { cb, arg };

    RcbForEachObj(rcb, kind, val, idx, RcbForEachQuadCb, &walk);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011ec70 (RoomFileSetHit). Once per load: offsets become EE
// addresses, each object gets its kind and numbers from its name, and the
// vertices move to the room origin, flat on y 0.

int RcbInit(float x, float z, void *rcb)
{
    RCB_HEAD *hd = static_cast<RCB_HEAD *>(rcb);
    u_int     base;

    if (RcbIsRcb(rcb) == 0)
    {
        return -1;
    }

    base = (u_int)ReiPan_GetPs2Address(rcb);
    if (hd->group <= base)
    {
        hd->group += base;

        RCB_GROUP *grp = RcbPtr<RCB_GROUP>(hd->group);
        for (int i = 0; i < hd->group_num; i++, grp++)
        {
            grp->vtx += base;
            grp->obj += base;

            RCB_OBJ *obj = RcbPtr<RCB_OBJ>(grp->obj);
            for (int j = 0; j < grp->obj_num; j++, obj++)
            {
                if (RcbSetObjKind(obj) < 0)
                {
                    return -1;
                }
                obj->quad += base;
            }

            float *v = RcbPtr<float>(grp->vtx);
            for (int k = 0; k < grp->vtx_num; k++, v += 4)
            {
                v[3] = 1.0f;
                v[0] = v[0] + x;
                v[1] = v[1] * 0.0f;
                v[2] = v[2] + z;
            }
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011edd0

static int RcbFindObjInQuadCb(RCB_OBJ *obj, float **quad, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    alignas(16) float rect[4][4];

    RcbCopyQuad(rect, quad);
    if (HcBaseIsInRectXZ(q->pos, rect[0]) == 0)
    {
        return 0;
    }
    q->obj = obj;
    q->hit = 1;
    return 1;
}

// FF3 0x0011ee48

RCB_OBJ *RcbFindObj(void *rcb, float *pos, int kind, short val, int idx)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    memcpy(q.pos, pos, sizeof(q.pos));
    RcbForEachQuad(rcb, kind, val, idx, RcbFindObjInQuadCb, &q);
    return q.obj;
}

// FF3 0x0011eee0 (unreferenced, with its query)

static int RcbAddFloorMhNoCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    int        no;

    if (obj->prm[1] != q->val)
    {
        return 0;
    }

    no = obj->prm[0];
    for (int i = 0; i < q->hit; i++)
    {
        if (q->list[i] == no)
        {
            return 0;
        }
    }
    q->list[q->hit] = no;
    q->hit++;
    return 0;
}

// FF3 0x0011ef58 (unreferenced): the hit areas on floor index floor.

int RcbGetFloorMhNoList(void *rcb, int floor, int *list)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    q.list = list;
    q.val  = floor;
    RcbForEachObj(rcb, RCB_KIND_KAI, 0, -1, RcbAddFloorMhNoCb, &q);
    return q.hit;
}

// FF3 0x0011efd0

static int RcbPushOutCb(RCB_OBJ *obj, float **quad, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    alignas(16) float rect[4][4];

    RcbCopyQuad(rect, quad);
    q->hit = MapHitPushOutQuad(q->r, q->pos, rect[0]);
    return q->hit;
}

// FF3 0x0011f028 (MrecWallPushOut, MapHitTourouPushOut)

int RcbPushOut(float r, void *rcb, float *out, float *pos, short val, int kind, short idx)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    q.r = r;
    memcpy(q.pos, pos, sizeof(q.pos));
    RcbForEachQuad(rcb, kind, val, idx, RcbPushOutCb, &q);
    memcpy(out, q.pos, sizeof(q.pos));
    return q.hit;
}

// FF3 0x0011f0f0

static int RcbLineHitCb(RCB_OBJ *obj, float **quad, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    alignas(16) float rect[4][4];

    RcbCopyQuad(rect, quad);
    q->hit |= RcbLineHitQuad(q, rect[0]);
    return 0;
}

// FF3 0x002ac870: each edge the segment crosses pulls pos2 in to the
// crossing nearest pos (XZ distance in r; a tie replaces).

static int RcbLineHitQuad(RCB_QUERY *q, const float *quad)
{
    if (HcBaseIsNearSegRect(quad, q->pos, q->pos2) == 0)
    {
        return 0;
    }

    for (int i = 0; i < 4; i++)
    {
        alignas(16) float out[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (CamSegCrossXZ(out, q->pos, q->pos2, &quad[i * 4], &quad[((i + 1) & 3) * 4]) == 0)
        {
            continue;
        }

        float dx = q->pos[0] - out[0];
        float dz = q->pos[2] - out[2];
        float d  = sqrtf(dx * dx + dz * dz);

        if (q->hit == 0 || !(q->r < d))
        {
            q->r = d;
            memcpy(q->pos2, out, sizeof(q->pos2));
            q->hit = 1;
        }
    }
    return q->hit;
}

// FF3 0x0011f158 (FUN_0016d070)

int RcbLineHit(void *rcb, float *out, float *p0, float *p1, short val, short kind, short idx)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    memcpy(q.pos, p0, sizeof(q.pos));
    memcpy(q.pos2, p1, sizeof(q.pos2));
    RcbForEachQuad(rcb, kind, val, idx, RcbLineHitCb, &q);
    if (q.hit == 0)
    {
        return 0;
    }
    memcpy(out, q.pos2, sizeof(q.pos2));
    return q.hit;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0011f230: of the se_ areas under pos, the one with the lowest prm[2].

static int RcbFindSeObjCb(RCB_OBJ *obj, float **quad, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    alignas(16) float rect[4][4];

    RcbCopyQuad(rect, quad);
    if (HcBaseIsInRectXZ(q->pos, rect[0]) != 0)
    {
        q->hit = 1;
        if (q->obj == nullptr || obj->prm[2] < q->obj->prm[2])
        {
            q->obj = obj;
        }
    }
    return 0;
}

// FF3 0x0011f2c0 (MrecSetSEInfo)

RCB_OBJ *RcbGetSeObj(void *rcb, float *pos, short floor)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    memcpy(q.pos, pos, sizeof(q.pos));
    RcbForEachQuad(rcb, RCB_KIND_SE, floor, 1, RcbFindSeObjCb, &q);
    return q.obj;
}

// FF3 0x0011f340

static int RcbAddSeNoCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg)
{
    RCB_QUERY *q  = static_cast<RCB_QUERY *>(arg);
    int        no = obj->prm[0];

    for (int i = 0; i < q->hit; i++)
    {
        if (q->list[i] == no)
        {
            return 0;
        }
    }
    q->list[q->hit] = no;
    q->hit++;
    return q->hit >= 8;
}

// FF3 0x0011f3b8 (foot_seGetSeStat)

int RcbGetSeNoList(void *rcb, int *list)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    q.list = list;
    RcbForEachObj(rcb, RCB_KIND_SE, 0, -1, RcbAddSeNoCb, &q);
    return q.hit;
}

// FF3 0x0011f420

static int RcbFindMotObjCb(RCB_OBJ *obj, float **quad, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);
    alignas(16) float rect[4][4];
    int        n;

    RcbCopyQuad(rect, quad);
    if (HcBaseIsInRectXZ(q->pos, rect[0]) == 0)
    {
        return 0;
    }

    n = q->hit;
    if (n == 0)
    {
        q->obj = obj;
    }
    else if (n == 1)
    {
        q->obj2 = obj;
    }
    q->hit = n + 1;
    return q->hit > 1;
}

// FF3 0x0011f4b8 (PlyrOnMapObj, PlyrGetMapObjDir)

int RcbGetMotObj(void *rcb, int mh_no, float *pos, RCB_OBJ **out)
{
    RCB_QUERY q;
    int       n;

    memset(&q, 0, sizeof(q));
    memcpy(q.pos, pos, sizeof(q.pos));
    RcbForEachQuad(rcb, RCB_KIND_MOT, (short)mh_no, 0, RcbFindMotObjCb, &q);

    n = (q.obj != nullptr) ? q.hit : -1;
    out[0] = q.obj;
    out[1] = q.obj2;
    return n;
}

// FF3 0x0011f560

static int RcbGetFloorCb(RCB_GROUP *grp, RCB_OBJ *obj, void *arg)
{
    RCB_QUERY *q = static_cast<RCB_QUERY *>(arg);

    q->hit = 1;
    q->val = obj->prm[1];
    return 1;
}

// FF3 0x0011f578: floors are numbered from 11 (the RegDat files' 11 / 13 / 15).

short RcbGetFloor(void *rcb, int mh_no)
{
    RCB_QUERY q;

    memset(&q, 0, sizeof(q));
    RcbForEachObj(rcb, RCB_KIND_KAI, (short)mh_no, 0, RcbGetFloorCb, &q);
    return (q.hit != 0) ? (short)(q.val + 11) : 11;
}

// FF3 0x0011f5f0 (PlyrMapPosUpdate)

short RcbUpdateMhNo(void *rcb, int mh_no, float *pos, short *floor)
{
    RCB_OBJ *obj = RcbFindObj(rcb, pos, RCB_KIND_CHANGE, (short)mh_no, 0);

    if (obj != nullptr)
    {
        mh_no = obj->prm[1];
        if (floor != nullptr)
        {
            *floor = RcbGetFloor(rcb, mh_no);
        }
    }
    return (short)mh_no;
}
