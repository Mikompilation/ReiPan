/* ==========================================================================
 *  ingame/map/MarkEvent.h
 *
 *  CMarkEvent (FF3 RTTI, vtable 0x341068) and what shares its class file:
 *  the 2D map draw flags (event command SET_2DMAP_DRAW, saved as row 58),
 *  the map sprite hook's event-mark checks, and the LERP_F interpolator.
 *  FF3 0x0010f820..0x0010fb80 (+ the destructors 0x002abfe0..0x002ac010);
 *  FF3-only, no FF2 file. LERP_F may be a file of its own (open-items.md).
 * ======================================================================== */
#ifndef _INGAME_MAP_MARKEVENT_H
#define _INGAME_MAP_MARKEVENT_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

// No members: the map screen object (bss 0x3f3360) holds one at +0x1e98,
// right after its CMarkDoor.
// layout: host -- 4 bytes on the EE (the vptr); nothing reads past it.
class CMarkEvent
{
public:
    CMarkEvent();                       // FF3 0x0010f848 (MarkEventCtor)
    virtual ~CMarkEvent();              // EE vtable +0x00 0x002abfe0, deleting +0x04 0x002abff0
};

// 16 flag bytes, bss 0x379a38 (saved: save_game_data row 58), set by the
// event command SET_2DMAP_DRAW for the rows of table 0x2efae8.
int  MarkEventFlagSaveGet(void **addr); // FF3 0x0010f820: {0x379a38, 0x10}
void MarkEventFlagClear(void);          // FF3 0x0010f858: all 0
void MarkEventFlagSet(int id, u_char value);    // FF3 0x0010f890: SET_2DMAP_DRAW (args[1], args[0])

int  MarkEventIsSpecialRoom(void);      // FF3 0x0010f8d8: area 0x2a with the player in map hit 2..9
int  MarkEventSpecialSprite(int floor, int layer, int id, int *x, int *y,
                            int *u, int *v, int *w, int *h);    // FF3 0x0010f918: 1 when it replaced the sprite
int  MarkEventIsDrawn(int floor, int layer, int id);    // FF3 0x0010fa40: the item's SET_2DMAP_DRAW flag, 1 if none

// A linear float interpolator, 0x14 bytes (FF3 0x0010fab0..0x0010fb80; the
// common fade layer embeds one at +0x2c, CLightCtl and CmenuCamStat theirs).
// C++ in FF3: a C1/C2 constructor pair that only its owners' constructors
// call; the members get the object in a0 and the floats in f12..f14.
struct LERP_F                           /* 0x14 */
{
    LERP_F();                                       // FF3 0x0010fac8 (LerpFClear; base-object twin 0x0010fab0): all 0
    void Set(float from, float to, float frames);   // FF3 0x0010fae0 (LerpFSet): cur = from
    void Start(float to, float frames);             // FF3 0x0010faf8 (LerpFStart): from the current value
    int  Update();                                  // FF3 0x0010fb10 (LerpFUpdate): one frame; 1 when at 'to'

    /* 0x00 */ float from;
    /* 0x04 */ float to;
    /* 0x08 */ float cur;
    /* 0x0c */ float frames;
    /* 0x10 */ float time;
};
static_assert(sizeof(LERP_F) == 0x14, "LERP_F is 0x14 bytes");
static_assert(offsetof(LERP_F, cur) == 0x08, "LERP_F.cur");
static_assert(offsetof(LERP_F, time) == 0x10, "LERP_F.time");

#endif /* _INGAME_MAP_MARKEVENT_H */
