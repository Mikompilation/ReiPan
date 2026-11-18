/* ==========================================================================
 *  ingame/map/LightCtl.h
 *
 *  CLightCtl (FF3 RTTI, vtable 0x341008): the blinking room lights. It
 *  loads 3ddata/room/meimetu.ecb (MEIMETU_ECB), plays its brightness
 *  sequences with a step sequencer and a LERP_F, and scales the lights of
 *  area 0x12's room whose names contain "meimetu" (明滅, blinking). FF3-only
 *  class file 0x0010ce28..0x0010d8b8 (between CGameClear and CLoad; FF3's
 *  class files link alphabetically). MapPlayStartReset (0x11d478) news one
 *  into the static CClassPtr<CLightCtl> 0x38e6d0 every map start.
 * ======================================================================== */
#ifndef _INGAME_MAP_LIGHTCTL_H
#define _INGAME_MAP_LIGHTCTL_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#include "graphics/graph3d/gra3d.h"     // GRA3DLIGHTDATA, NUM_GRA3DLIGHTID
#include "common/cmn_util.h"            // STEP_SEQ
#include "ingame/map/MarkEvent.h"       // LERP_F

// ──────────────────────────────────────────────────────────────────────
// CLightCtl
// ──────────────────────────────────────────────────────────────────────

// meimetu.ecb: an ECB table (EcbGetNum / EcbGetData) of 5-float records.
// rec[0] < 0 is a marker: -1.0 starts a sequence, -2.0 ends the table;
// otherwise one blink: rise from 20 to level rec[0] in rec[1] frames, hold
// it rec[2] frames, fall to 20 in rec[3], stay low rec[4].
struct LIGHT_CTL_REC                    /* 0x14 */
{
    /* 0x00 */ float level;             // percent of the light's own diffuse / range; < 0 marker
    /* 0x04 */ float rise;              // frames
    /* 0x08 */ float hold;
    /* 0x0c */ float fall;
    /* 0x10 */ float low;
};
static_assert(sizeof(LIGHT_CTL_REC) == 0x14, "LIGHT_CTL_REC is 0x14 bytes (meimetu.ecb record size)");

#define LIGHT_CTL_SEQ_START  (-1.0f)    // data 0x36b69c / 0x36b6a4
#define LIGHT_CTL_LOW        20.0f      // data 0x36b68c / 0x36b690 / 0x36b694 / 0x36b6a0
#define LIGHT_CTL_AREA       0x12       // the only area whose lights blink

// Area data member 0 (LightCtlGetNameDat): a 0x20-byte name per light id
// after a 0x10-byte head (only ids 3..37 are read).
struct LIGHT_NAME_DAT
{
    /* 0x00 */ u_char head[0x10];
    /* 0x10 */ char   name[NUM_GRA3DLIGHTID][0x20];
};
static_assert(offsetof(LIGHT_NAME_DAT, name) == 0x10, "LIGHT_NAME_DAT.name");
static_assert(sizeof(((LIGHT_NAME_DAT *)0)->name[0]) == 0x20, "LIGHT_NAME_DAT name stride");

// 0x7c bytes on the EE (MapPlayStartReset's operator new).
// layout: host -- the vptr and ecb are 4 bytes on the EE: num at EE +0x04,
// rec +0x08, ecb +0x0c, load_id +0x10, seq +0x14 (0x54), lerp +0x68.
class CLightCtl
{
public:
    CLightCtl();                        // FF3 0x0010d140 (LightCtlCtor)
    virtual ~CLightCtl();               // EE vtable +0x00 0x0010d250 (base 0x0010d200), deleting +0x04 0x0010d2a0

    LIGHT_CTL_REC *GetRec();            // FF3 0x0010ce58
    void LoadReq();                     // FF3 0x0010d310: loads meimetu.ecb into ctl->ecb
    void PickSeq();                     // FF3 0x0010d330
    void Start();                       // FF3 0x0010d448: counts the sequences, picks one at random
    int IsLoadEnd();                    // FF3 0x0010d4e0
    void Update();                      // FF3 0x0010d530: one frame of the blink
    void Apply(GRA3DLIGHTDATA *dat, int area);  // FF3 0x0010d588: area 0x12's lights from dat, the "meimetu" ones scaled

    int      num;                       // EE +0x04: sequences in the table (-1.0 markers)
    int      rec;                       // EE +0x08: the current record, -1 until CLightCtl::Start
    void    *ecb;                       // EE +0x0c: meimetu.ecb, a system-heap buffer
    int      load_id;                   // EE +0x10: LoadReqGetAddr's request id
    STEP_SEQ seq;                       // EE +0x14: LightCtlStepHold / Fall / Low / Next; owner this
    LERP_F   lerp;                      // EE +0x68: the brightness in percent (cur)
};
static_assert(offsetof(CLightCtl, rec) == offsetof(CLightCtl, num) + 4, "CLightCtl.rec follows num");
static_assert(offsetof(CLightCtl, lerp) == offsetof(CLightCtl, seq) + sizeof(STEP_SEQ), "CLightCtl.lerp follows seq");

CLightCtl      *GetLightCtl(void);                  // FF3 0x0010d2f8: the last one constructed (sdata gp-0x7e68), nullptr after its dtor
GRA3DLIGHTDATA *LightCtlGetLightData(void);         // FF3 0x0010d300: bss 0x378400, the set CLightCtl::Apply last wrote
int   LightCtlIsRoomDrawn(void);                    // FF3 0x0010d4f8: area 0x12's room loaded and drawn

#endif /* _INGAME_MAP_LIGHTCTL_H */
