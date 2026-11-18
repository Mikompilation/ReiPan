/* ==========================================================================
 *  ingame/map/RegDat.h
 *
 *  Map registration data (FF2 RegDat.c; FF3 0x0011f668..0x00120800). A
 *  room's registration file (a member of its PK4) is a flat block: an
 *  MB_OUT_HEAD, a run of variable-length MB_OUT_SECTION records (the
 *  placements: cameras, fog, doors, lights, ...) and a run of
 *  variable-length MB_OUT_RECT rectangles that bind floor positions to those
 *  records. Up to 32 files are resident at once, one slot each in RegDatBuff
 *  (FF2: 8).
 *
 *  Registration data lives in emulated EE memory (the room's PK4). As on the
 *  PS2, RegDatRegist turns the header's two offsets into absolute EE
 *  addresses; RegDat.c follows them with ReiPan_GetHostPointer.
 * ======================================================================== */
#ifndef _INGAME_MAP_REGDAT_H
#define _INGAME_MAP_REGDAT_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define REG_DAT_BUFF_NUM    32          // resident files (RegDatBuff slots; FF2 8)
#define REG_DAT_ST_TYPE_NUM 26          // section / rectangle types (FF2 14)

// Section / rectangle types FF3 code asks for (FF2's numbering where it
// still holds: 6 room outline, 9 four-corner camera, 5 / 13 fog).
#define RECORD_TYPE_ROOM_OUTLINE        6   // RegDatCheckBuffPos
#define RECORD_TYPE_FOUR_CORNER_CAMERA  9   // MrecGetSpCamera
#define RECORD_TYPE_DOOR                0x12    // MapLBuffLoadDoor's walk (REG_DAT_DOOR)

// The file header, at offset 0 of the block. reg_vecp / reg_stp are file
// offsets on disc; RegDatRegist adds the block's EE address to both.
// layout: EE (file data)
struct MB_OUT_HEAD                      /* 0x30 (FF2) */
{
    /* 0x00 */ char  magic[4];          // FF2 "pzb"; FF3 does not test it
    /* 0x04 */ int   area_id;           // label / 1000 of every record in it
    /* 0x08 */ int   kai;               // floor; RegDatCheckBuffPos's filter, RegListFindLabel's key
    /* 0x0c */ int   pad;
    /* 0x10 */ int   reg_vec_num;       // rectangles
    /* 0x14 */ u_int reg_vecp;          // MB_OUT_RECT run (EE address after RegDatRegist)
    /* 0x18 */ int   reg_st_num;        // section records
    /* 0x1c */ u_int reg_stp;           // MB_OUT_SECTION run (EE address after RegDatRegist)
    /* 0x20 */ float Pos[4];            // the area's offset (x, y, z); [3] FF2, unverified
};
static_assert(sizeof(MB_OUT_HEAD) == 0x30, "MB_OUT_HEAD size");
static_assert(offsetof(MB_OUT_HEAD, area_id) == 0x04, "MB_OUT_HEAD.area_id");
static_assert(offsetof(MB_OUT_HEAD, kai) == 0x08, "MB_OUT_HEAD.kai");
static_assert(offsetof(MB_OUT_HEAD, reg_vec_num) == 0x10, "MB_OUT_HEAD.reg_vec_num");
static_assert(offsetof(MB_OUT_HEAD, reg_vecp) == 0x14, "MB_OUT_HEAD.reg_vecp");
static_assert(offsetof(MB_OUT_HEAD, reg_st_num) == 0x18, "MB_OUT_HEAD.reg_st_num");
static_assert(offsetof(MB_OUT_HEAD, reg_stp) == 0x1c, "MB_OUT_HEAD.reg_stp");
static_assert(offsetof(MB_OUT_HEAD, Pos) == 0x20, "MB_OUT_HEAD.Pos");

// Common preamble of every section (placement) record. SecStID is the
// record type, the numbering the rectangles are grouped by; size covers the
// whole record, so it is also the stride to the next one.
// layout: EE (file data)
struct MB_OUT_SECTION
{
    /* 0x00 */ u_short SecStID;
    /* 0x02 */ u_short size;
    /* 0x04 */ u_int   labelID;         // RegDatGetStLabel
};
static_assert(sizeof(MB_OUT_SECTION) == 0x08, "MB_OUT_SECTION size");
static_assert(offsetof(MB_OUT_SECTION, size) == 0x02, "MB_OUT_SECTION.size");
static_assert(offsetof(MB_OUT_SECTION, labelID) == 0x04, "MB_OUT_SECTION.labelID");

// A door record of a registration file (type 0x12; MapLBuffLoadDoor walks
// them with RegDatGetStPtrStart / RegDatGetNextStPtr).
// layout: EE (file data)
struct REG_DAT_DOOR
{
    /* 0x00 */ MB_OUT_SECTION head;     // SecStID 0x12
    /* 0x08 */ int            door;     // door_stat id
};
static_assert(offsetof(REG_DAT_DOOR, door) == 0x08, "REG_DAT_DOOR.door");

// A rectangle record of a registration file: its four corners (FF2
// MB_OUT_RECT). size is the stride to the next one (RegDatNextVec); the
// rectangles of one file are sorted by their section's type.
// layout: EE (file data)
struct MB_OUT_RECT
{
    /* 0x00 */ u_short RectID;          // FF2, unverified
    /* 0x02 */ u_short size;            // RegDatNextVec
    /* 0x04 */ int     reg_id;          // the section record it binds (RegDatGetStPtr)
    /* 0x08 */ int     pad[2];
    /* 0x10 */ float   vec[4][4];       // corners 0..3 (x, y, z, w); y = 0, w = 1 after RegDatAddOffset
};
static_assert(sizeof(MB_OUT_RECT) == 0x50, "MB_OUT_RECT size");
static_assert(offsetof(MB_OUT_RECT, size) == 0x02, "MB_OUT_RECT.size");
static_assert(offsetof(MB_OUT_RECT, reg_id) == 0x04, "MB_OUT_RECT.reg_id");
static_assert(offsetof(MB_OUT_RECT, vec) == 0x10, "MB_OUT_RECT.vec");

// One corner of a four-corner ("special", rectangle type 9) camera. FF3
// puts the corner it belongs to in front of FF2's MDAT_CAM_SP_ONE.
// layout: EE (file data)
struct MDAT_CAM_SP_ONE                  /* 0x24 */
{
    /* 0x00 */ u_int corner;            // rectangle corner 0..3 this record sits at; > 3: its own index (MapCamSpGetCornerOrder)
    /* 0x04 */ float Cam_Pos[3];
    /* 0x10 */ float View_Pos[3];
    /* 0x1c */ float Proj;              // fov
    /* 0x20 */ float RotZ;              // roll
};
static_assert(sizeof(MDAT_CAM_SP_ONE) == 0x24, "MDAT_CAM_SP_ONE size (MapCamSpGetCornerOrder's stride)");
static_assert(offsetof(MDAT_CAM_SP_ONE, Cam_Pos) == 0x04, "MDAT_CAM_SP_ONE.Cam_Pos");
static_assert(offsetof(MDAT_CAM_SP_ONE, View_Pos) == 0x10, "MDAT_CAM_SP_ONE.View_Pos");
static_assert(offsetof(MDAT_CAM_SP_ONE, Proj) == 0x1c, "MDAT_CAM_SP_ONE.Proj");
static_assert(offsetof(MDAT_CAM_SP_ONE, RotZ) == 0x20, "MDAT_CAM_SP_ONE.RotZ");

// The four-corner camera record (FF2 MDAT_CAM_SP), blended across the
// rectangle by MapCamSpCameraCtrl. Size not settled (the draw-enable words
// IngameSetDrawEnable reads follow at +0xa0, ingame/camera/map_camera.h).
// layout: EE (file data)
struct MDAT_CAM_SP
{
    /* 0x00 */ MB_OUT_SECTION  head;
    /* 0x08 */ int             type;    // 0 / 2 parallel blend, 1 (and others) / 3 diagonal; 2 and 3 aim relative to the player
    /* 0x0c */ MDAT_CAM_SP_ONE st[4];
};
static_assert(offsetof(MDAT_CAM_SP, type) == 0x08, "MDAT_CAM_SP.type");
static_assert(offsetof(MDAT_CAM_SP, st) == 0x0c, "MDAT_CAM_SP.st");

// Per-type index built once at registration time: how many rectangles of
// that type the file holds, and where their run starts (FF2 RD_REG_ST_DAT).
// layout: host -- dat is a pointer (EE 0x8: st_num +0, dat +4)
struct RD_REG_ST_DAT
{
    /* 0x0 */ int          st_num;
    /* 0x4 */ MB_OUT_RECT *dat;
};
static_assert(offsetof(RD_REG_ST_DAT, st_num) == 0x0, "RD_REG_ST_DAT.st_num");

// One resident registration file (FF2 RD_REG_HEAD; EE 0xec, RegDatBuff
// 0x38e6e0 holds 32). LabVec* is the cursor of the RegDatVecFind4Label /
// RegDatVecNextFind walk, type_* the cursor of RegDatGetStPtrStart /
// RegDatGetNextStPtr: one walk of each kind per slot at a time.
// layout: host -- pointer members (EE: StPtrList +0, RegDatPtr +0xd0,
// LabVecID +0xd4, LabVecNum +0xd8, LabVecPtr +0xdc, type_id +0xe0,
// type_search_num +0xe4, type_search_p +0xe8)
struct RD_REG_HEAD
{
    RD_REG_ST_DAT   StPtrList[REG_DAT_ST_TYPE_NUM];
    MB_OUT_HEAD    *RegDatPtr;
    int             LabVecID;
    int             LabVecNum;
    MB_OUT_RECT    *LabVecPtr;
    int             type_id;
    int             type_search_num;
    MB_OUT_SECTION *type_search_p;
};
static_assert(offsetof(RD_REG_HEAD, StPtrList) == 0x0, "RD_REG_HEAD.StPtrList");

// ---- slots ----
RD_REG_HEAD *RegDatGetBuffListPtr(int buff_id);         // FF3 0x0011f668: &RegDatBuff[buff_id] (no range check)
MB_OUT_HEAD *RegDatGetTopAddr(int buff_id);             // FF3 0x0011f688: the file registered in slot buff_id, nullptr
int  RegDatSetTopAddr(int buff_id, MB_OUT_HEAD *addr);  // FF3 0x0011f6b8: 0, -1
void RegDatInit(void);                                  // FF3 0x00120068
int  RegDatRegist(MB_OUT_HEAD *mst);                    // FF3 0x0011ffe8: the slot id, -2 when full
void RegDatDeleteBuffList(int buff_id);                 // FF3 0x0011fc60: frees slot buff_id (RegListDelete)
void RegDatDeleteAllBuffList(void);                     // FF3 0x0011fc88
void RegDatSetOffset(int buff_id, float x, float y, float z);  // FF3 0x0011ff58: moves slot buff_id's rectangles to the area offset (x, y, z)

// ---- section (placement) records ----
// The stat getters return void *: every caller casts to the record the type
// implies (MDAT_FOG, MDAT_CAM_SP, MREC_HIT, ...).
MB_OUT_SECTION *RegDatNextSt(MB_OUT_SECTION *st);       // FF3 0x0011f6f0: st + st->size
void  *RegDatGetStPtr(int buff_id, int reg_id);         // FF3 0x0011f760: section record reg_id of slot buff_id
u_int  RegDatGetStLabel(int buff_id, int reg_id);       // FF3 0x0011f740: its labelID
int    RegDatGetStID4Label(int buff_id, int label);     // FF3 0x0011f880: label % 1000 when label / 1000 is the slot's area; -1 no file, -2 other area, -3 out of range
void  *RegDatGetStPtr4Label(int buff_id, int label);    // FF3 0x0011f900: the record of label in slot buff_id, nullptr (RegListCheck)
void  *RegDatGetStPtr4Label2(int label);                // FF3 0x0011f948: the record of label in the first slot of its area, nullptr
void  *RegDatGetStPtr4Label3(int label, int type);      // FF3 0x0011f9f8: RegDatGetStPtr4Label2 when it is of type, nullptr
int    RegDatBuffID4Label(int label);                   // FF3 0x0011fb88: the slot whose area is label / 1000, -1
void            RegDatGetStPtrStart(int buff_id, int type);    // FF3 0x00120718: starts slot buff_id's walk over the records of type
MB_OUT_SECTION *RegDatGetNextStPtr(int buff_id);               // FF3 0x00120760: the next one, nullptr at the end

// ---- rectangles ----
MB_OUT_RECT *RegDatGetVecPtr(int buff_id, int type);    // FF3 0x0011f790: slot buff_id's first rectangle of type, nullptr
int  RegDatGetVecNum(int buff_id, int type);            // FF3 0x0011f7d0: how many rectangles of type slot buff_id has, -1
MB_OUT_RECT *RegDatGetVecPtrStart(int buff_id);         // FF3 0x0011f810: the slot's whole rectangle run, nullptr
int  RegDatGetVecNumAll(int buff_id);                   // FF3 0x0011f840: its length, -1
int  RegDatCheckHitRect(MB_OUT_RECT *mp, const float *vPos); // FF3 0x001200c8: vPos inside the rectangle (XZ, either triangle)
MB_OUT_RECT *RegDatNextVec(MB_OUT_RECT *rec);           // FF3 0x0011f870: the next rectangle record (rec + rec->size)
int  RegDatVecFind4Label(int buff_id, int label);       // FF3 0x0011fa38: starts the walk over label's rectangles; 0, < 0 nothing to walk
MB_OUT_RECT *RegDatVecNextFind(int buff_id);            // FF3 0x0011fae0: the walk's next rectangle, nullptr at the end

// ---- position lookups ----
// Which resident slot covers vPos (kai -1: any floor). RegDatGetBuffIDG
// fills the hit list: the one id, -1 none, -2 all 32, -3 several (then read
// RegDatGetHitList / RegDatGetHitNum).
int  RegDatCheckBuffPos(int buff_id, int kai, const float *vPos);  // FF3 0x00120150: 1 vPos in slot buff_id's room outline on floor kai, 0 not, -1 empty slot (RegListGetBuffID)
int  RegDatGetBuffIDG(int kai, const float *vPos);      // FF3 0x00120250
int *RegDatGetHitList(void);                            // FF3 0x001206d0: the hit list (bss 0x390460)
int  RegDatGetHitNum(void);                             // FF3 0x001206e0: its count (sdata 0x36b8fc)
void RegDatAddNoRegistList(int buff_id);                // FF3 0x001206e8: slot buff_id stops answering position lookups (RegListExe)
void RegDatResetNoRegistList(void);                     // FF3 0x00120710

// The section record whose type rectangle holds vPos (map_rectangle.c's).
// stat: prefer this record when several match (keeps a camera latched).
void *RegDatGetStat(int kai, float *vPos, int type);    // FF3 0x00120328: nullptr
void *RegDatGetRectAndStat(MB_OUT_RECT **ppRect, void *pRectStat, int kai, float *vPos, int type);    // FF3 0x00120350: as RegDatGetStat, with its rectangle
void *RegDatGetRectAndStat2(MB_OUT_RECT **ppRect, void *pStat, int buff_id, float *vPos, int type);   // FF3 0x001204b0: the same inside slot buff_id
int   RegDatCheckSameRectStat(void *pStat, int kai, float *vPos, int type);   // FF3 0x001205d8: vPos is still in a rectangle of record pStat

#endif /* _INGAME_MAP_REGDAT_H */
