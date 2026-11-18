/* ==========================================================================
 *  ingame/map/MarkDoor.h
 *
 *  CMarkDoor (FF3 RTTI, vtable 0x341058): the door marks of the map screen.
 *  The map's sprite hook (FF3 0x001f7978, m_plyr_camera.c) records the
 *  door items of a floor layout here instead of drawing them; the map draw
 *  (FF3 0x001f87b0) draws them afterwards, coloured by the door's state and
 *  clipped to the map window. FF3 0x0010f150..0x0010f820 (+ 0x002abf80..
 *  0x002abfe0); FF3-only, no FF2 file.
 * ======================================================================== */
#ifndef _INGAME_MAP_MARKDOOR_H
#define _INGAME_MAP_MARKDOOR_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// The map screen object (bss 0x3f3360) holds a CMarkDoor at +0x90 and its
// CMarkEvent at +0x1e98: 0x1e08 bytes, 80 records.
#define MARK_DOOR_REC_MAX   80

#define MARK_DOOR_NONE      (-100)      // MARK_DOOR_REC.mark of a mark without doors: rgb as given

enum MARK_DOOR_TYPE
{
    MARK_DOOR_SPRITE = 0,               // CMarkDoor::AddSprite: a B2d sprite (spr)
    MARK_DOOR_QUAD   = 1,               // CMarkDoor::AddQuad: a textured quad (quad)
};

// One mark, 0x60 bytes. The words after flags are the B2d sprite hook's or
// quad hook's arguments (BpCtl.h, B2dDrawSpriteDirect / B2dDrawQuadDirect).
// layout: EE (ints only)
struct MARK_DOOR_REC                    /* 0x60 */
{
    /* 0x00 */ int   layer;             // the map layer being drawn (the hook's sdata gp+0x28)
    /* 0x04 */ int   mark;              // door mark id (the door ECB rows with this +0xe), MARK_DOOR_NONE
    /* 0x08 */ int   type;              // MARK_DOOR_TYPE
    /* 0x0c */ int   handle;            // BpTexSetFunc a0
    /* 0x10 */ int   tex;               // BpTexSetFunc a1; the draws' a0
    /* 0x14 */ int   alpha;
    /* 0x18 */ u_int rgb;               // used as is for MARK_DOOR_NONE, else MarkDoorGetRgb
    /* 0x1c */ u_int flags;             // the draw flags (blend, 0x1000 / 0x2000 mirror)
    union
    {
        struct
        {
            /* 0x20 */ int u, v, w, h;
            /* 0x30 */ int x, y;
            /* 0x38 */ int sx, sy;      // 0x400 = 1.0
        } spr;
        struct
        {
            /* 0x20 */ int xy[8];       // four screen points
            /* 0x40 */ int uv[8];       // their texels
        } quad;
    };
};
static_assert(sizeof(MARK_DOOR_REC) == 0x60, "MARK_DOOR_REC is 0x60 bytes");
static_assert(offsetof(MARK_DOOR_REC, flags) == 0x1c, "MARK_DOOR_REC.flags");
static_assert(offsetof(MARK_DOOR_REC, spr.x) == 0x30, "MARK_DOOR_REC.spr.x");
static_assert(offsetof(MARK_DOOR_REC, spr.sy) == 0x3c, "MARK_DOOR_REC.spr.sy");
static_assert(offsetof(MARK_DOOR_REC, quad.uv) == 0x40, "MARK_DOOR_REC.quad.uv");

// 0x1e08 bytes on the EE.
// layout: host -- the vptr is 4 bytes on the EE: num is at EE +4, rec +8.
class CMarkDoor
{
public:
    CMarkDoor();                        // FF3 0x0010f170 (MarkDoorCtor; twin 0x0010f150)
    virtual ~CMarkDoor();               // EE vtable +0x00 0x002abfb0, deleting +0x04 0x002abfc0

    MARK_DOOR_REC *Find(int mark);      // FF3 0x0010f190: nullptr if none
    void AddQuad(int layer, int mark, int handle, int tex, const int *xy, const int *uv, int alpha, u_int rgb, u_int flags);  // FF3 0x0010f220
    void AddSprite(int layer, int mark, int handle, int tex, int u, int v, int w, int h, int x, int y, int sx, int sy, int alpha, u_int rgb, u_int flags);  // FF3 0x0010f2b8
    void Clear();                       // FF3 0x0010f340
    void Draw();                        // FF3 0x0010f3e8

    int           num;                  // EE +0x04: records in use
    MARK_DOOR_REC rec[MARK_DOOR_REC_MAX];   // EE +0x08
};
static_assert(offsetof(CMarkDoor, rec) == offsetof(CMarkDoor, num) + 4, "CMarkDoor.rec follows num");
static_assert(sizeof(((CMarkDoor *)nullptr)->rec) == 0x1e00, "CMarkDoor.rec is 0x1e00 bytes");

u_int MarkDoorGetRgb(int mark);                                 // FF3 0x0010f348: by the state of the mark's doors

#endif /* _INGAME_MAP_MARKDOOR_H */
