/* ==========================================================================
 *  ingame/map/MapRcb.h
 *
 *  The room's rcb file (FF3-only; MapLoadGetRoomData slot 3, magic "rcb"):
 *  named map objects, each a set of quads on the floor plane. The name's
 *  prefix gives the kind, its _<n> parts the parameters ("kai_2_1": the hit
 *  area 2 is on floor index 1). RcbInit relocates the file in place at load.
 *  FF3 0x0011e820..0x0011f668 and 0x002ac870.
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPRCB_H
#define _INGAME_MAP_MAPRCB_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// The object kinds: the name prefixes of the table at 0x2f1530.
enum RCB_KIND
{
    RCB_KIND_HIT    = 0,                // hit_
    RCB_KIND_SE     = 1,                // se_<sound>_<floor>_<priority>: footstep sound areas
    RCB_KIND_POS    = 2,                // pos_
    RCB_KIND_KAI    = 3,                // kai_<mh_no>_<floor>: the floor of a hit area
    RCB_KIND_CHANGE = 4,                // change_<mh_no>_<new mh_no>: hit-area switch zones
    RCB_KIND_MOT    = 5,                // mot_<mh_no>_<type>_<dir>: player motion zones (PlyrOnMapObj)
    RCB_KIND_TOUROU = 6,                // tourou_ (lanterns)
};

// layout: EE (file data). Offsets in the file, EE addresses after RcbInit.
struct RCB_HEAD
{
    /* 0x00 */ char  magic[4];          // "rcb"
    /* 0x04 */ u_int _04;
    /* 0x08 */ int   group_num;
    /* 0x0c */ u_int group;             // RCB_GROUP[group_num]
};
static_assert(sizeof(RCB_HEAD) == 0x10, "RCB_HEAD size");
static_assert(offsetof(RCB_HEAD, group_num) == 0x08, "RCB_HEAD.group_num");
static_assert(offsetof(RCB_HEAD, group) == 0x0c, "RCB_HEAD.group");

// layout: EE (file data)
struct RCB_GROUP
{
    /* 0x00 */ int   vtx_num;
    /* 0x04 */ u_int vtx;               // sceVu0FVECTOR[vtx_num], in room space after RcbInit
    /* 0x08 */ int   obj_num;
    /* 0x0c */ u_int obj;               // RCB_OBJ[obj_num]
};
static_assert(sizeof(RCB_GROUP) == 0x10, "RCB_GROUP size");
static_assert(offsetof(RCB_GROUP, vtx) == 0x04, "RCB_GROUP.vtx");
static_assert(offsetof(RCB_GROUP, obj_num) == 0x08, "RCB_GROUP.obj_num");
static_assert(offsetof(RCB_GROUP, obj) == 0x0c, "RCB_GROUP.obj");

// layout: EE (file data)
struct RCB_OBJ
{
    /* 0x00 */ char  name[0x20];
    /* 0x20 */ short prm[10];           // the name's _<n> numbers (RcbGetNamePrm)
    /* 0x34 */ int   kind;              // RCB_KIND, from the name (RcbSetObjKind)
    /* 0x38 */ int   quad_num;
    /* 0x3c */ u_int quad;              // RCB_QUAD[quad_num]
};
static_assert(sizeof(RCB_OBJ) == 0x40, "RCB_OBJ size");
static_assert(offsetof(RCB_OBJ, prm) == 0x20, "RCB_OBJ.prm");
static_assert(offsetof(RCB_OBJ, kind) == 0x34, "RCB_OBJ.kind");
static_assert(offsetof(RCB_OBJ, quad_num) == 0x38, "RCB_OBJ.quad_num");
static_assert(offsetof(RCB_OBJ, quad) == 0x3c, "RCB_OBJ.quad");

// layout: EE (file data): one quad, 4 vertex indices into its group.
struct RCB_QUAD
{
    /* 0x00 */ int vtx[4];
};
static_assert(sizeof(RCB_QUAD) == 0x10, "RCB_QUAD size");

// cb(group, obj, arg) of RcbForEachObj, cb(obj, the quad's 4 vertices, arg)
// of RcbForEachQuad; nonzero stops the walk.
typedef int (*RCB_OBJ_CB)(RCB_GROUP *grp, RCB_OBJ *obj, void *arg);
typedef int (*RCB_QUAD_CB)(RCB_OBJ *obj, float **quad, void *arg);

int      RcbGetNamePrm(short *prm, const char *name);  // FF3 0x0011e820: the name's _<n> numbers into prm; the count, -1 when 10+
short    RcbSetObjKind(RCB_OBJ *obj);   // FF3 0x0011e908: kind and prm from the name; -1 unknown prefix, -2 null
float   *RcbGetVertex(RCB_GROUP *grp, int idx);    // FF3 0x0011e9c0: null out of range
int      RcbIsRcb(void *rcb);           // FF3 0x0011e9f8
int      RcbForEachObj(void *rcb, int kind, short val, int idx, RCB_OBJ_CB cb, void *arg);   // FF3 0x0011ea28: kind -1 all, idx < 0 any prm; -1 not rcb, -2 not relocated, else 0
int      RcbForEachQuad(void *rcb, int kind, short val, int idx, RCB_QUAD_CB cb, void *arg); // FF3 0x0011ec38
int      RcbInit(float x, float z, void *rcb);  // FF3 0x0011ec70: relocate, kinds, vertices to the room origin (RoomFileSetHit); -1 bad
RCB_OBJ *RcbFindObj(void *rcb, float *pos, int kind, short val, int idx);  // FF3 0x0011ee48: the first object under pos (XZ)
int      RcbGetFloorMhNoList(void *rcb, int floor, int *list);  // FF3 0x0011ef58: the distinct hit areas on floor index floor (unreferenced); the count
int      RcbPushOut(float r, void *rcb, float *out, float *pos, short val, int kind, short idx);   // FF3 0x0011f028: circle r at pos pushed out of the quads into out; 1 pushed
int      RcbLineHit(void *rcb, float *out, float *p0, float *p1, short val, short kind, short idx); // FF3 0x0011f158: nearest crossing of p0-p1 with the quads' edges into out; 0 none
RCB_OBJ *RcbGetSeObj(void *rcb, float *pos, short floor);  // FF3 0x0011f2c0: the se_ object under pos on floor with the lowest priority prm[2], null
int      RcbGetSeNoList(void *rcb, int *list);  // FF3 0x0011f3b8: the distinct se_ sound numbers, up to 8; the count
int      RcbGetMotObj(void *rcb, int mh_no, float *pos, RCB_OBJ **out);    // FF3 0x0011f4b8: mot_ objects of mh_no under pos, up to 2 in out; the count, -1 none
short    RcbGetFloor(void *rcb, int mh_no); // FF3 0x0011f578: 11 + the kai_ floor of hit area mh_no, 11 when none
short    RcbUpdateMhNo(void *rcb, int mh_no, float *pos, short *floor); // FF3 0x0011f5f0: the hit area after a change_ zone at pos; *floor its floor

#endif /* _INGAME_MAP_MAPRCB_H */
