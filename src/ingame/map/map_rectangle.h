/* ==========================================================================
 *  ingame/map/map_rectangle.h
 *
 *  The registration-rectangle queries (FF2 map_rectangle.c), FF3
 *  0x1ff6e8..0x1ffe18, reconstructed. The run before it is the map menu
 *  (ingame/story_menu_map.c) and before that map_hit_check.c.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAP_RECTANGLE_H
#define _INGAME_MAP_MAP_RECTANGLE_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// ---- map_rectangle.c proper (FF3 0x1ff6e8..0x1ffe18; FF2 map_rectangle.c) ----
// The layer between RegDat's registration rectangles and the systems that ask
// "what is at this position": the camera rectangles, event labels and the
// footstep record. FF3 dropped FF2's hit/door rectangle caches (map_buf_ctrl)
// and the debug draws; it added MrecFindRectStat with a callback and the
// alternate camera sets.

// A registration stat record as MrecFindRectStat hands it to its callback
// (RegDatGetStPtr's result: a RegDat row). Only +8 read so far.
// layout: EE (file data)
struct MREC_HIT
{
    /* 0x00 */ u_char _00[8];           // FF2 MB_OUT_SECTION
    /* 0x08 */ int    id;               // kind 0x12: the door id; kinds 1 / 8 / 20..23: MDAT_CAM.type
};
static_assert(offsetof(MREC_HIT, id) == 0x08, "MREC_HIT.id");

// A camera rectangle's stat record (rectangle types 1 / 8 and their alternate
// sets 20..23; FF2 MDAT_CAM). MrecSetCameraInfo copies the first 0x38 bytes
// into its latch; map_camera.h's MAP_CAM_DATA reads the same words.
// layout: EE (file data)
struct MDAT_CAM
{
    /* 0x00 */ u_char head[8];          // FF2 MB_OUT_SECTION; +4 the word PlyrMapCamCheck reads
    /* 0x08 */ int    type;             // follow mode 0..9, >= 100 a rail camera (MrecGetCameraType)
    /* 0x0c */ float  Pos[3];           // eye
    /* 0x18 */ float  View[3];          // target
    /* 0x24 */ float  Rrg;              // fov, <= 0: keep (MrecGetCameraPrj)
    /* 0x28 */ float  RotZ;             // roll
    /* 0x2c */ float  Asobi;            // follow margin, < 0: do not follow
    /* 0x30 */ int    draw_en0;         // IngameSetDrawEnable
    /* 0x34 */ int    draw_en1;
};
static_assert(sizeof(MDAT_CAM) == 0x38, "MDAT_CAM size (MrecSetCameraInfo's copy)");
static_assert(offsetof(MDAT_CAM, type) == 0x08, "MDAT_CAM.type");
static_assert(offsetof(MDAT_CAM, Pos) == 0x0c, "MDAT_CAM.Pos");
static_assert(offsetof(MDAT_CAM, View) == 0x18, "MDAT_CAM.View");
static_assert(offsetof(MDAT_CAM, Rrg) == 0x24, "MDAT_CAM.Rrg");
static_assert(offsetof(MDAT_CAM, RotZ) == 0x28, "MDAT_CAM.RotZ");
static_assert(offsetof(MDAT_CAM, Asobi) == 0x2c, "MDAT_CAM.Asobi");
static_assert(offsetof(MDAT_CAM, draw_en0) == 0x30, "MDAT_CAM.draw_en0");
static_assert(offsetof(MDAT_CAM, draw_en1) == 0x34, "MDAT_CAM.draw_en1");

float    *MrecGetTestRect(void);        // FF3 0x001ff6e8: the rectangle MrecFindRectStat is testing (gp+0x68: 4 corners, x/z at +0/+8 of each 16 bytes)
MREC_HIT *MrecFindRectStat(short floor, float *pos, int kind, int (*cb)(MREC_HIT *hit));  // FF3 0x001ff6f0: the kind's rectangles containing pos; cb 0 stops (returns that hit), no cb: the first hit
int       MrecSetCameraSet(int set);    // FF3 0x001ff8f8: CAM_CHANGE_TYPE / the area's cam_type: picks the alternate camera rectangles (0x3f5200); returns 0
int       MrecInitCameraInfo(void);     // FF3 0x001ff908: clears the camera latch; returns 1
int       MrecIsCameraChange(void);     // FF3 0x001ff948: the last MrecSetCameraInfo changed records (unreferenced)
int       MrecIsCameraHit(void);        // FF3 0x001ff958: the last MrecSetCameraInfo found a record (unreferenced)

// The camera rectangles (the map camera's queries). type: the rectangle kind
// (1 the ordinary camera, 8 battle, 9 four-corner). The latched record is what
// MrecGetCamera* read.
struct MB_OUT_RECT;                     // ingame/map/RegDat.h
struct MDAT_CAM_SP;
int           MrecCheckHitRect(short floor, float *pos, int type);           // FF3 0x001ff828: pos on floor is in a rectangle of type
int           MrecCheckOnSameCamRect(void *stat, short floor, float *pos, int type);   // FF3 0x001ffc20: pos is still in the rectangle of record stat (of kind type)
MDAT_CAM     *MrecGetCameraInfo(void);                                       // FF3 0x001ff938: the latch (never nullptr in FF3)
int           MrecGetCameraType(void);                                       // FF3 0x001ff968: its follow mode 0..9, >= 100 a rail camera
float         MrecGetCameraPrj(void);                                        // FF3 0x001ff998: fov, <= 0: keep
float         MrecGetCameraAsobi(void);                                      // FF3 0x001ff9c8: the follow margin, < 0: do not follow
int           MrecGetCameraRotZ(float *roll);                                // FF3 0x001ff9f8: 1 with a record (roll = its RotZ)
int           MrecGetCameraPos(float *pos);                                  // FF3 0x001ffa38: 1 with a record (pos = its eye, w 1)
int           MrecGetCameraInterest(float *pos);                             // FF3 0x001ffa98: 1 with a record (pos = its target, w 1)
int           MrecSetCameraInfo(short floor, float *pos, int type);          // FF3 0x001ffb28: latch the record at pos; 1 when it changed
MDAT_CAM_SP  *MrecGetCameraSpInfo(MB_OUT_RECT **rect, void *stat, short floor, float *pos);    // FF3 0x001ffc08: the four-corner record at pos and its rectangle

int  MrecIsInEventSub(float *pos, int buff_id, int label);   // FF3 0x001ffc40: pos is in one of label's rectangles in RegDat slot buff_id
int  MrecIsInEvent(float *pos, int label, short floor);      // FF3 0x001ffcc0: 1 when pos (on floor) is inside one of label's rectangles
int  MrecGetSeNo(void);                 // FF3 0x001ffd68: the latched footstep record's sound number, -1: none
int  MrecSetSEInfo(float *pos, short floor);   // FF3 0x001ffd80: latch the footstep record at pos; -1 without a current room, else 0

void MrecReset(void);                   // FF3 0x001ffdf0: camera latch, footstep record and tested rectangle cleared (MapPlayStartReset)

#endif /* _INGAME_MAP_MAP_RECTANGLE_H */
