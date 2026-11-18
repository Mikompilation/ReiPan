// FILE: ingame/map/map_rectangle.c
//
// Registration-rectangle queries (FF2 map_rectangle.c, FF3 0x1ff6e8..0x1ffe18):
// the layer between RegDat's rectangle tables and the systems that ask "what
// is at this position". Camera rectangles latch a camera record the map camera
// reads back field by field; event labels answer "is the player in this area";
// the footstep record names the floor's step sound.
//
// What FF3 changed from FF2:
//  - FF2's per-buffer hit / door rectangle caches (map_buf_ctrl, map_rec_info,
//    the line-cross and near tests), the stair headings and the debug wire
//    draws are gone; the wall collision lives in map_hit_check.c (MrecWallPushOut).
//  - MrecFindRectStat walks every slot of the hit list for one rectangle kind
//    and lets a callback decide which hit wins; the door code (story_door.c)
//    and the camera latch both use it, and MrecGetTestRect hands the callback
//    the corners of the rectangle under test.
//  - The camera latch copies the record (0x38 bytes) instead of keeping a
//    pointer, so MrecGetCameraInfo never returns nullptr and the getters'
//    "no record" defaults are dead code (kept as the ROM has them).
//  - Camera kinds 1 and 8 have two alternate rectangle sets each (20 / 21 and
//    22 / 23), chosen by MrecSetCameraSet (CAM_CHANGE_TYPE, an area's
//    cam_type).
//  - The footstep record comes from the current room's hit data, not from a
//    RegDat type-4 rectangle with priorities.

#include "ingame/map/map_rectangle.h"

#include "common/reipan_stub.h"
#include "ingame/map/RegDat.h"                  // rectangle and stat lookups
#include "ingame/map/hit_check_base.h"          // HcBaseIsInRectXZ
#include "ingame/map/MapLoad.h"                 // MapLoadGetCurRoomAddr, MapLoadGetRoomData
#include "ingame/map/MapRcb.h"                  // RcbGetSeObj

#include <string.h>                             // memset

// The camera latch (bss 0x3f5200, 0x4c bytes on the EE; MrecInitCameraInfo
// clears it).
// layout: host -- two pointers; never in a file, only cleared as a whole
struct MREC_CAM_CTRL
{
    int       set;                      // 0x00: the alternate camera set (MrecSetCameraSet)
    int       change_flg;               // 0x04: the last MrecSetCameraInfo changed records
    int       hit_flg;                  // 0x08: the last MrecSetCameraInfo found a record
    MDAT_CAM *found;                    // 0x0c: the callback's pick
    MDAT_CAM *last;                     // 0x10: the record latched last
    MDAT_CAM  info;                     // 0x14: its copy, what the getters read
};

// bss 0x3f5200
static MREC_CAM_CTRL mrec_cam;

// sdata 0x371100: the latched footstep record.
static RCB_OBJ *mrec_se_info;               // the footstep se_ object MrecSetSEInfo latched (FF2 MDAT_SE)

// sdata 0x373558 (gp+0x68): the corners of the rectangle MrecFindRectStat is
// testing, for its callback.
static float *mrec_test_rect;

// The alternate rectangle kinds of camera kinds 1 and 8 (data 0x35b9b0 / 0x35b9c0).
static const int mrec_cam_type_tbl1[3] = { 1, 20, 21 };
static const int mrec_cam_type_tbl8[3] = { 8, 22, 23 };

// FF3 0x001ff6e8
float *MrecGetTestRect(void)
{
    return mrec_test_rect;
}

// FF3 0x001ff6f0
// Every rectangle of kind in every RegDat slot of the hit list at pos. A hit's
// stat record goes to cb: 0 stops and returns it, non-zero keeps scanning.
// Without cb the first hit is returned; after the scan, the last hit (or nullptr).
MREC_HIT *MrecFindRectStat(short floor, float *pos, int kind, int (*cb)(MREC_HIT *hit))
{
    MREC_HIT    *hit = nullptr;
    MB_OUT_RECT *rec;
    int         *list;
    int          list_num;
    int          rec_num;
    int          i;
    int          j;

    if (RegDatGetBuffIDG(floor, pos) == -1)
    {
        return nullptr;
    }

    list     = RegDatGetHitList();
    list_num = RegDatGetHitNum();

    for (i = 0; i < list_num; i++, list++)
    {
        rec_num = RegDatGetVecNum(*list, kind);
        rec     = RegDatGetVecPtr(*list, kind);

        for (j = 0; j < rec_num; j++)
        {
            mrec_test_rect = &rec->vec[0][0];

            if (HcBaseIsInRectXZ(pos, mrec_test_rect) != 0)
            {
                hit = static_cast<MREC_HIT *>(RegDatGetStPtr(*list, rec->reg_id));

                if (cb == nullptr || cb(hit) == 0)
                {
                    return hit;
                }
            }

            rec = RegDatNextVec(rec);
        }
    }

    return hit;
}

// FF3 0x001ff828
int MrecCheckHitRect(short floor, float *pos, int type)
{
    return RegDatGetStat(floor, pos, type) != nullptr;
}

// FF3 0x001ff848
// The rectangle kind MrecSetCameraInfo really searches for type.
static int MrecGetCameraDataType(int type, int set)
{
    if (type == 1)
    {
        return mrec_cam_type_tbl1[set % 3];
    }
    if (type == 8)
    {
        return mrec_cam_type_tbl8[set % 3];
    }
    return type;
}

// FF3 0x001ff8f8
int MrecSetCameraSet(int set)
{
    mrec_cam.set = set;
    return 0;
}

// FF3 0x001ff908
int MrecInitCameraInfo(void)
{
    memset(&mrec_cam, 0, sizeof(mrec_cam));
    return 1;
}

// FF3 0x001ff938
MDAT_CAM *MrecGetCameraInfo(void)
{
    return &mrec_cam.info;
}

// FF3 0x001ff948
int MrecIsCameraChange(void)
{
    return mrec_cam.change_flg;
}

// FF3 0x001ff958
int MrecIsCameraHit(void)
{
    return mrec_cam.hit_flg;
}

// FF3 0x001ff968
int MrecGetCameraType(void)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    return (cam != nullptr) ? cam->type : -1;
}

// FF3 0x001ff998
float MrecGetCameraPrj(void)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    return (cam != nullptr) ? cam->Rrg : -1.0f;     // lit 0x371104
}

// FF3 0x001ff9c8
float MrecGetCameraAsobi(void)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    return (cam != nullptr) ? cam->Asobi : -1.0f;   // lit 0x371108
}

// FF3 0x001ff9f8
int MrecGetCameraRotZ(float *roll)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    if (cam != nullptr)
    {
        *roll = cam->RotZ;
    }
    return cam != nullptr;
}

// FF3 0x001ffa38
int MrecGetCameraPos(float *pos)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    if (cam != nullptr)
    {
        pos[0] = cam->Pos[0];
        pos[1] = cam->Pos[1];
        pos[2] = cam->Pos[2];
        pos[3] = 1.0f;                              // lit 0x37110c
    }
    return cam != nullptr;
}

// FF3 0x001ffa98
int MrecGetCameraInterest(float *pos)
{
    MDAT_CAM *cam = MrecGetCameraInfo();

    if (cam != nullptr)
    {
        pos[0] = cam->View[0];
        pos[1] = cam->View[1];
        pos[2] = cam->View[2];
        pos[3] = 1.0f;                              // lit 0x371110
    }
    return cam != nullptr;
}

// FF3 0x001ffaf8
// MrecSetCameraInfo's pick: the record latched last wins at once (no cut while
// the player stays inside it); otherwise the first record hit is kept.
static int MrecCameraHitCallback(MREC_HIT *hit)
{
    MDAT_CAM *cam = reinterpret_cast<MDAT_CAM *>(hit);

    if (cam == mrec_cam.last)
    {
        mrec_cam.found = cam;
        return 0;
    }
    if (mrec_cam.found == nullptr)
    {
        mrec_cam.found = cam;
    }
    return 1;
}

// FF3 0x001ffb28
// Latch the camera record at pos. Returns 1 when it differs from the last one
// (a cut); on a miss the latch keeps the last record.
int MrecSetCameraInfo(short floor, float *pos, int type)
{
    mrec_cam.found      = nullptr;
    mrec_cam.change_flg = 0;
    mrec_cam.hit_flg    = 0;

    MrecFindRectStat(floor, pos, MrecGetCameraDataType(type, mrec_cam.set), MrecCameraHitCallback);

    if (mrec_cam.found == nullptr)
    {
        return 0;
    }

    mrec_cam.hit_flg = 1;
    if (mrec_cam.found == mrec_cam.last)
    {
        return 0;
    }

    mrec_cam.change_flg = 1;
    mrec_cam.info       = *mrec_cam.found;
    mrec_cam.last       = mrec_cam.found;
    return mrec_cam.change_flg;
}

// FF3 0x001ffc08
MDAT_CAM_SP *MrecGetCameraSpInfo(MB_OUT_RECT **rect, void *stat, short floor, float *pos)
{
    return static_cast<MDAT_CAM_SP *>(RegDatGetRectAndStat(rect, stat, floor, pos, 9));
}

// FF3 0x001ffc20
int MrecCheckOnSameCamRect(void *stat, short floor, float *pos, int type)
{
    return RegDatCheckSameRectStat(stat, floor, pos, type) != 0;
}

// FF3 0x001ffc40
int MrecIsInEventSub(float *pos, int buff_id, int label)
{
    MB_OUT_RECT *rec;

    if (RegDatVecFind4Label(buff_id, label) < 0)
    {
        return 0;
    }

    rec = RegDatVecNextFind(buff_id);
    while (rec != nullptr)
    {
        if (HcBaseIsInRectXZ(pos, &rec->vec[0][0]) != 0)
        {
            return 1;
        }
        rec = RegDatVecNextFind(buff_id);
    }

    return 0;
}

// FF3 0x001ffcc0
// FF2 tried the hit list only when the slot was ambiguous; FF3 always walks it
// and answers 0 when pos is in no slot.
int MrecIsInEvent(float *pos, int label, short floor)
{
    int *list;
    int  num;
    int  i;

    if (RegDatGetBuffIDG(floor, pos) < 0)
    {
        return 0;
    }

    list = RegDatGetHitList();
    num  = RegDatGetHitNum();

    for (i = 0; i < num; i++)
    {
        if (MrecIsInEventSub(pos, list[i], label) != 0)
        {
            return 1;
        }
    }

    return 0;
}

// FF3 0x001ffd68
int MrecGetSeNo(void)
{
    return (mrec_se_info != nullptr) ? mrec_se_info->prm[0] : -1;
}

// FF3 0x001ffd80
// The footstep record of the current room's hit data at pos; a miss keeps the
// record latched before.
int MrecSetSEInfo(float *pos, short floor)
{
    MAP_ROOM_DAT *room = MapLoadGetCurRoomAddr();
    RCB_OBJ      *se;

    if (room == nullptr)
    {
        return -1;
    }

    se = RcbGetSeObj(MapLoadGetRoomData(room, 3), pos, floor);
    if (se != nullptr)
    {
        mrec_se_info = se;
    }
    return 0;
}

// FF3 0x001ffdf0
void MrecReset(void)
{
    MrecInitCameraInfo();
    mrec_se_info   = nullptr;
    mrec_test_rect = nullptr;
}
