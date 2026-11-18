/* ==========================================================================
 *  ingame/event/prg/ev_ladder.h
 *
 *  The ladder list (FF3-only, no FF2 counterpart; the file name is
 *  descriptive): EventDataLoad loads HASIGO_ECB (3ddata/room/, an ECB table
 *  of 0x90-byte LADDER_DAT rows) to EE 0xfcd000, PlyrLadderStartBottom /
 *  Top look a ladder up by id. One object, sdata gp-0x4d0 (GetLadderList),
 *  built by the file's static initializer. FF3 0x199150..0x1992d0.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_LADDER_H
#define _INGAME_EVENT_PRG_EV_LADDER_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// A ladder record, one ECB row of HASIGO_ECB. PLYR_WRK.ladder points at it
// while the player climbs (ingame/plyr/plyr_sub.c).
struct LADDER_DAT                       /* 0x90 */
{
    /* 0x00 */ short         id;            // LadderList::GetData's key
    /* 0x02 */ u_char        mh_bottom;     // map hit object at the foot (PlyrSetMhNo when getting off)
    /* 0x03 */ u_char        mh_top;        // and at the top
    /* 0x04 */ u_char        cam_a_plyr;    // 1: camera a looks at PLYR_WRK._180 instead of cam_a_tgt
    /* 0x05 */ u_char        cam_b_plyr;
    /* 0x06 */ u_char        height;        // (height - 6) / 2 steps
    /* 0x07 */ u_char        _07;
    /* 0x08 */ u_short       cam_switch;    // camera a below step (cam_switch - 2) / 2, b above
    /* 0x0a */ u_char        _0a[6];
    /* 0x10 */ sceVu0FVECTOR pos_bottom;    // where the climb starts from below
    /* 0x20 */ sceVu0FVECTOR rot_bottom;
    /* 0x30 */ sceVu0FVECTOR pos_top;       // and from above
    /* 0x40 */ sceVu0FVECTOR rot_top;
    /* 0x50 */ sceVu0FVECTOR cam_a_pos;
    /* 0x60 */ sceVu0FVECTOR cam_a_tgt;
    /* 0x70 */ sceVu0FVECTOR cam_b_pos;
    /* 0x80 */ sceVu0FVECTOR cam_b_tgt;
};
static_assert(sizeof(LADDER_DAT) == 0x90, "LADDER_DAT is 0x90 bytes");
static_assert(offsetof(LADDER_DAT, mh_top) == 0x03, "LADDER_DAT.mh_top");
static_assert(offsetof(LADDER_DAT, height) == 0x06, "LADDER_DAT.height");
static_assert(offsetof(LADDER_DAT, cam_switch) == 0x08, "LADDER_DAT.cam_switch");
static_assert(offsetof(LADDER_DAT, pos_bottom) == 0x10, "LADDER_DAT.pos_bottom");
static_assert(offsetof(LADDER_DAT, rot_top) == 0x40, "LADDER_DAT.rot_top");
static_assert(offsetof(LADDER_DAT, cam_b_tgt) == 0x80, "LADDER_DAT.cam_b_tgt");

// 8 bytes on the EE, no RTTI (no virtuals), no destructor.
// layout: host -- ecb is 4 bytes on the EE: ecb at EE +0x00, load_id +0x04.
class LadderList
{
public:
    LadderList();                       // FF3 0x00199158 (C1; C2 0x00199150): ecb nullptr

    void        Load();                 // FF3 0x00199160: HASIGO_ECB to EE 0xfcd000
    void        Release();              // FF3 0x00199190: ecb nullptr (the memory is the event area's)
    LADDER_DAT *GetData(short id);      // FF3 0x001991a8: the row with that id, nullptr: none
    int         IsLoadEnd();            // FF3 0x00199250

    void *ecb;                          // EE +0x00: the ECB table (EE 0xfcd000), nullptr when released
    int   load_id;                      // EE +0x04: LoadReqGetAddr's request id
};
static_assert(offsetof(LadderList, ecb) == 0, "LadderList.ecb is first");
static_assert(offsetof(LadderList, load_id) == sizeof(void *), "LadderList.load_id follows ecb");

LadderList *GetLadderList(void);        // FF3 0x00199268: sdata gp-0x4d0

#endif /* _INGAME_EVENT_PRG_EV_LADDER_H */
