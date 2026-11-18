/* ==========================================================================
 *  ingame/map/BgSe.h
 *
 *  Background (layout) SEs: CBgSe (FF3 RTTI, vtable 0x340f58), FF3
 *  0x00109728..0x00109dd8, FF3-only (no FF2 counterpart; file name inferred
 *  from link order). One static object (bss 0x377f10, GetLayoutSeWrk) keeps
 *  up to four ambient SE channels for the current area: on an area change
 *  LayoutSeSetArea picks the area's entries of the table at 0x2ef3c8 (each
 *  by its chance), loads their SE banks and places them at the area's map
 *  offset; LayoutSeMain starts each one when its bank is loaded and, for an
 *  entry with a region label, once the player stands in that region.
 * ======================================================================== */
#ifndef _INGAME_MAP_BGSE_H
#define _INGAME_MAP_BGSE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include <libvu0.h>                     // sceVu0FVECTOR

struct MB_OUT_SECTION;

#define LAYOUT_SE_CH_NUM    4           // channels of CBgSe
#define LAYOUT_SE_DAT_NUM   34          // entries of layout_se_dat (0x2ef3c8)

// LAYOUT_SE_CH.step
enum LAYOUT_SE_STEP
{
    LAYOUT_SE_STEP_IDLE = 0,            // nothing set (LayoutSeChStop)
    LAYOUT_SE_STEP_LOAD = 1,            // bank requested (LayoutSeChSet), waiting for SndSeBankIsLoaded
    LAYOUT_SE_STEP_WAIT = 2,            // bank loaded, waiting for the player to enter region label
    LAYOUT_SE_STEP_PLAY = 3,            // SndSePlay done
};

// One ambient SE channel (0x30 on the EE and the hosts: CBgSe +0x10, 4 x
// 0x30). Its constructor (FF3 0x00109748, C1/C2 twin 0x00109728) only sets
// the first 0x18 bytes.
struct LAYOUT_SE_CH
{
    LAYOUT_SE_CH();                     // FF3 0x00109748 (LayoutSeChInit; twin 0x00109728 LayoutSeChInitTwin)

    void Set(u_int se_no, int label_no, int s3d_flag, int volume,
             const float *position);    // FF3 0x00109768 (LayoutSeChSet): bank load, then step 1
    void Play();                        // FF3 0x00109808 (LayoutSeChPlay)
    void Main(MB_OUT_SECTION **list, int num);  // FF3 0x001098a8 (LayoutSeChMain): list = the player's regions
    void Stop();                        // FF3 0x00109940 (LayoutSeChStop)

    /* 0x00 */ int           step;      // LAYOUT_SE_STEP
    /* 0x04 */ int           s3d;       // 1: placed at pos (SND_3D_SET), else plain; -1 unset
    /* 0x08 */ int           play_id;   // SndSePlay's id, -1
    /* 0x0c */ int           label;     // region label that starts it (RegSecListHasLabel), -1: none
    /* 0x10 */ int           vol;       // 0..0x7f (LayoutSeChPlay clamps)
    /* 0x14 */ u_int         se;        // bank file << 16 | SE number; -1 unset
    /* 0x18 */ u_char        _018[8];   // never touched
    /* 0x20 */ sceVu0FVECTOR pos;       // world position, w 1
};
static_assert(sizeof(LAYOUT_SE_CH) == 0x30, "LAYOUT_SE_CH is 0x30 bytes");
static_assert(offsetof(LAYOUT_SE_CH, step) == 0x00, "LAYOUT_SE_CH.step");
static_assert(offsetof(LAYOUT_SE_CH, s3d) == 0x04, "LAYOUT_SE_CH.s3d");
static_assert(offsetof(LAYOUT_SE_CH, play_id) == 0x08, "LAYOUT_SE_CH.play_id");
static_assert(offsetof(LAYOUT_SE_CH, label) == 0x0c, "LAYOUT_SE_CH.label");
static_assert(offsetof(LAYOUT_SE_CH, vol) == 0x10, "LAYOUT_SE_CH.vol");
static_assert(offsetof(LAYOUT_SE_CH, se) == 0x14, "LAYOUT_SE_CH.se");
static_assert(offsetof(LAYOUT_SE_CH, pos) == 0x20, "LAYOUT_SE_CH.pos");

// One entry of the ambient SE table (data 0x2ef3c8, 34 x 0x24).
// layout: EE (ROM data)
struct LAYOUT_SE_DAT
{
    /* 0x00 */ int   area;              // MapAreaNoGet's area
    /* 0x04 */ u_int se;                // bank file << 16 | SE number
    /* 0x08 */ int   s3d;               // LAYOUT_SE_CH.s3d (1 in every entry)
    /* 0x0c */ float rate;              // chance in percent (GetRandValF(100) <= rate)
    /* 0x10 */ int   vol;
    /* 0x14 */ int   label;             // LAYOUT_SE_CH.label
    /* 0x18 */ float ofs[3];            // position relative to the area's map offset
};
static_assert(sizeof(LAYOUT_SE_DAT) == 0x24, "LAYOUT_SE_DAT is 0x24 bytes");
static_assert(offsetof(LAYOUT_SE_DAT, se) == 0x04, "LAYOUT_SE_DAT.se");
static_assert(offsetof(LAYOUT_SE_DAT, s3d) == 0x08, "LAYOUT_SE_DAT.s3d");
static_assert(offsetof(LAYOUT_SE_DAT, rate) == 0x0c, "LAYOUT_SE_DAT.rate");
static_assert(offsetof(LAYOUT_SE_DAT, vol) == 0x10, "LAYOUT_SE_DAT.vol");
static_assert(offsetof(LAYOUT_SE_DAT, label) == 0x14, "LAYOUT_SE_DAT.label");
static_assert(offsetof(LAYOUT_SE_DAT, ofs) == 0x18, "LAYOUT_SE_DAT.ofs");

// 0xd0 bytes on the EE (bss 0x377f10): the vptr, the area, padding to the
// 16-aligned channels.
// layout: host -- the vptr is 8 bytes on 64-bit hosts; area then sits at +8
// and the channels stay at +0x10.
class CBgSe
{
public:
    CBgSe();                            // FF3 0x00109a28 (BgSeCtor; base-object twin 0x001099c0 BgSeCtorBase)
    virtual ~CBgSe();                   // EE vtable +0x00 0x00109ab8 BgSeDtor (base 0x00109a90), deleting +0x04 0x00109ae0

    void Main();                        // FF3 0x00109b18 (LayoutSeMain): steps the channels (IngameMain)
    void SetArea(int area_no);          // FF3 0x00109b98 (LayoutSeSetArea): the area's ambient SEs
    void StopAll();                     // FF3 0x00109cc8 (LayoutSeStopAll): every channel, area -1

    int          area;                  // EE +0x04: current area, -1 after StopAll
    LAYOUT_SE_CH ch[LAYOUT_SE_CH_NUM];  // EE +0x10
};

CBgSe *GetLayoutSeWrk(void);            // FF3 0x001099b0: the static CBgSe (bss 0x377f10)

#endif /* _INGAME_MAP_BGSE_H */
