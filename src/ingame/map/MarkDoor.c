// ingame/map/MarkDoor.c -- CMarkDoor, the map screen's door marks.
// FF3 0x0010f150..0x0010f820 (+ the vtable destructors and an inline helper
// at 0x002abf80..0x002abfe0); FF3-only (the class file sorts between MarkDat
// and MarkEvent; open-items.md "0x10bbb0..0x116fa0").
#include "ingame/map/MarkDoor.h"

#include "ingame/map/DoorStat.h"        // DoorGetMarkDoors, DoorStatGetLock, DoorStatIsKnown
#include "ingame/map/MapRec.h"          // MapRecGet (view)
#include "sdk/vu0_intrin.h"             // sceVu0FVECTOR
#include "system/b2d/B2dCtl.h"        // B2D_HOOK_ARGS
#include "system/b2d/BpCtl.h"           // B2dDrawSpriteDirect, B2dDrawQuadDirect
#include "system/b2d/BpTex.h"           // BpTexSetFunc
#include "ingame/camera/cam_rail.h"     // CamSegCrossXZ

#define MARK_DOOR_RGB_GRAY  0x80808080u // the default
#define MARK_DOOR_RGB_OPEN  0x50e69680u // a door has bit 3 and none is locked (light blue)

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010f170 (MarkDoorCtor; FF3 0x0010f150 is the same body). Both
// tail-jump to CMarkDoor::Clear (callcheck sees no call).

CMarkDoor::CMarkDoor()
{
    Clear();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002abfb0 (the deleting one: 0x002abfc0).

CMarkDoor::~CMarkDoor()
{
}

// ──────────────────────────────────────────────────────────────────────
// The record of a door mark. FF3 0x0010f190 (the map item hook 0x001f7580's).

MARK_DOOR_REC *CMarkDoor::Find(int mark)
{
    for (int i = 0; i < num; i++)
    {
        if (rec[i].mark == mark)
            return &rec[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Records a textured quad: xy and uv are four points each.
// FF3 0x0010f220.

void CMarkDoor::AddQuad(int layer, int mark, int handle, int tex,
                     const int *xy, const int *uv, int alpha, u_int rgb, u_int flags)
{
    // PC: FF3 does not check (an 81st record runs into the CMarkEvent behind).
    if (num >= MARK_DOOR_REC_MAX)
        return;

    MARK_DOOR_REC *r = &rec[num];

    r->layer  = layer;
    r->mark   = mark;
    r->type   = MARK_DOOR_QUAD;
    r->handle = handle;
    r->tex    = tex;
    r->alpha  = alpha;
    r->rgb    = rgb;
    r->flags  = flags;
    for (int i = 0; i < 4; i++)
    {
        r->quad.xy[i * 2]     = xy[i * 2];
        r->quad.xy[i * 2 + 1] = xy[i * 2 + 1];
        r->quad.uv[i * 2]     = uv[i * 2];
        r->quad.uv[i * 2 + 1] = uv[i * 2 + 1];
    }
    num++;
}

// ──────────────────────────────────────────────────────────────────────
// Records a sprite: the B2d sprite hook's words (sx, sy 0x400 = 1.0).
// FF3 0x0010f2b8 (the map sprite hook 0x001f7978's).

void CMarkDoor::AddSprite(int layer, int mark, int handle, int tex,
                       int u, int v, int w, int h, int x, int y, int sx, int sy,
                       int alpha, u_int rgb, u_int flags)
{
    // PC: FF3 does not check.
    if (num >= MARK_DOOR_REC_MAX)
        return;

    MARK_DOOR_REC *r = &rec[num];

    r->layer  = layer;
    r->mark   = mark;
    r->spr.sy = sy;
    r->handle = handle;
    r->tex    = tex;
    r->alpha  = alpha;
    r->rgb    = rgb;
    r->flags  = flags;
    r->spr.u  = u;
    r->spr.v  = v;
    r->spr.w  = w;
    r->spr.h  = h;
    r->spr.x  = x;
    r->spr.y  = y;
    r->type   = MARK_DOOR_SPRITE;
    r->spr.sx = sx;
    num++;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010f340.

void CMarkDoor::Clear()
{
    num = 0;
}

// ──────────────────────────────────────────────────────────────────────
// A door mark's colour: light blue when one of its doors has been tried
// (DOOR_STAT_KNOWN) and none of them is locked, else gray.
// FF3 0x0010f348.

u_int MarkDoorGetRgb(int mark)
{
    int ids[16];
    int locked  = 0;
    int flagged = 0;
    int n       = DoorGetMarkDoors((short)mark, ids);

    for (int i = 0; i < n; i++)
    {
        if (DoorStatGetLock(ids[i]) != 0)
            locked = 1;
        if (DoorStatIsKnown(ids[i]) != 0)
            flagged = 1;
    }

    if (flagged && !locked)
        return MARK_DOOR_RGB_OPEN;
    return MARK_DOOR_RGB_GRAY;
}

// ──────────────────────────────────────────────────────────────────────
// An int x, y point as a float vector {x, 0, y, 1} (the XZ plane of the
// segment test). FF3 0x002abf80 (an inline function out of line).

static void MarkDoorPointToVec(float *out, const int *p)
{
    out[1] = 0.0f;
    out[0] = (float)p[0];
    out[3] = 1.0f;                      // data 0x36b6d0
    out[2] = (float)p[1];
}

// ──────────────────────────────────────────────────────────────────────
// The edge function of p against a -> b (negative: p on the inner side of
// a clockwise screen quad). FF3 0x0010f7e0.

static float MarkDoorEdgeFunc(const int *p, const int *a, const int *b)
{
    return (float)((a[0] - p[0]) * (b[1] - a[1]) - (a[1] - p[1]) * (b[0] - a[0]));
}

// ──────────────────────────────────────────────────────────────────────
// 1 when quad a's first corner is strictly inside quad b.
// FF3 0x0010f758.

static int MarkDoorQuadCornerIn(const int *a, const int *b)
{
    for (int i = 0; i < 4; i++)
    {
        if (MarkDoorEdgeFunc(a, &b[i * 2], &b[((i + 1) & 3) * 2]) >= 0.0f)
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Draws the recorded marks: the texture is set when it changes, the
// colour comes from the doors' state, sprites are drawn when they overlap
// the map window (MapRec's view), quads when they cross it.
// FF3 0x0010f3e8 (the map draw 0x001f87b0's).

void CMarkDoor::Draw()
{
    CMapRec   *map  = MapRecGet();
    const int *view = map->view;
    int        handle = -1;
    int        tex    = -1;

    for (int i = 0; i < num; i++)
    {
        MARK_DOOR_REC *r   = &rec[i];
        u_int          rgb = r->rgb;

        if (r->mark != MARK_DOOR_NONE)
            rgb = MarkDoorGetRgb(r->mark);

        if (r->handle != handle || r->tex != tex)
        {
            B2D_HOOK_ARGS set = {};

            handle = r->handle;
            tex    = r->tex;
            set.a0 = handle;
            set.a1 = tex;
            BpTexSetFunc(&set);
        }

        if (r->type == MARK_DOOR_SPRITE)
        {
            int x  = r->spr.x;
            int y  = r->spr.y;
            int w  = r->spr.w * r->spr.sx / 1024;
            int h  = r->spr.h * r->spr.sy / 1024;
            int x0 = view[0] < x ? view[0] : x;
            int y0 = view[1] < y ? view[1] : y;
            int x1 = x + w < view[0] + view[2] ? view[0] + view[2] : x + w;
            int y1 = y + h < view[1] + view[3] ? view[1] + view[3] : y + h;

            // The two rects overlap (the span of both is no wider than the sum).
            if (x1 - x0 <= view[2] + w && y1 - y0 <= view[3] + h)
            {
                B2D_HOOK_ARGS draw = {};

                draw.a0       = r->tex;
                draw.a1       = r->spr.u;
                draw.a2       = r->spr.v;
                draw.a3       = r->spr.w;
                draw.t0       = r->spr.h;
                draw.t1       = r->spr.x;
                draw.t2       = r->spr.y;
                draw.t3       = r->spr.sx;
                draw.stack[0] = r->spr.sy;
                draw.stack[1] = r->alpha;
                draw.stack[2] = rgb;
                draw.stack[3] = r->flags;
                B2dDrawSpriteDirect(&draw);
            }
        }
        else
        {
            const int *xy = r->quad.xy;
            int        win[8] =
            {
                view[0],           view[1],
                view[0],           view[1] + view[3],
                view[0] + view[2], view[1] + view[3],
                view[0] + view[2], view[1],
            };
            sceVu0FVECTOR a0, a1, b0, b1, out;
            int           hit = 0;

            // Every edge pair. Both segments come from the mark's own points
            // on the EE too (the window quad was probably meant for b), and
            // the EE's inner loop has no bound: it only leaves on a crossing,
            // which an edge and its neighbour (a shared corner) give. PC:
            // bounded at four, then the corner tests the EE has after the
            // loops but never reaches.
            for (int e = 0; e < 4 && !hit; e++)
            {
                MarkDoorPointToVec(a0, &xy[e * 2]);
                MarkDoorPointToVec(a1, &xy[((e + 1) & 3) * 2]);
                for (int f = 0; f < 4 && !hit; f++)
                {
                    MarkDoorPointToVec(b0, &xy[f * 2]);
                    MarkDoorPointToVec(b1, &xy[((f + 1) & 3) * 2]);
                    hit = CamSegCrossXZ(out, a0, a1, b0, b1) != 0;
                }
            }
            if (!hit)
                hit = MarkDoorQuadCornerIn(xy, win) || MarkDoorQuadCornerIn(win, xy);

            if (hit)
            {
                B2D_HOOK_ARGS draw = {};

                draw.a0 = r->tex;
                draw.a1 = (intptr_t)r->quad.xy;
                draw.a2 = (intptr_t)r->quad.uv;
                draw.a3 = r->alpha;
                draw.t0 = rgb;
                draw.t1 = r->flags;
                B2dDrawQuadDirect(&draw);
            }
        }
    }
}
