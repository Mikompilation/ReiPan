/* ==========================================================================
 *  ingame/mission/ExtraMissionShelt.h
 *
 *  CExtraMissionShelt (FF3 RTTI, vtable 0x340fe8): the story game kept aside
 *  while an extra mission runs. The mission menu makes one and saves the
 *  card data into it before a mission (one_StartMenu_MissionMain); when the
 *  mission ends or is quit, CarryStore keeps what the mission changed (the
 *  mission records, options, soul list, equipment, lens, points); back in
 *  phase 55 init_StartMenu_Main loads the story game back, CarryRestore puts
 *  the kept blocks over it, and the keeper is deleted.
 *  FF3-only class file 0x0010b788..0x0010bba0, the first of the alphabetical
 *  class files (CExtraMissionShelt, CGameClear, CLightCtl, ...).
 * ======================================================================== */
#ifndef _INGAME_MISSION_EXTRAMISSIONSHELT_H
#define _INGAME_MISSION_EXTRAMISSIONSHELT_H

#include <stddef.h>                     // offsetof

class CmemShelt;                        // common/memShelt.h

// 0xc bytes on the EE.
// layout: host -- the vptr and shelt are 4 bytes on the EE, 8 here.
class CExtraMissionShelt
{
public:
    CExtraMissionShelt();               // FF3 0x0010b7f0 (CExtraMissionSheltCtor; twin 0x0010b788)
    virtual ~CExtraMissionShelt();      // EE vtable +0x00 0x0010b898 (base 0x0010b858), deleting +0x04 0x0010b8d8

    void Save();                        // FF3 0x0010b930: the story game into the buffer, mission resets
    void Load();                        // FF3 0x0010baf8: the story game back from the buffer

    // FF3 calls these through the instance (a0) but never reads it.
    static void CarryStore();           // FF3 0x0010bb20: keep what the mission changed
    static void CarryRestore();         // FF3 0x0010bb60: put it back over the reloaded story game

    /* 0x04 */ int        saved;        // 1 once CExtraMissionShelt::Save ran
    /* 0x08 */ CmemShelt *shelt;        // the card data (GetMemoryCardDataSize(0, 2) bytes)
};
static_assert(offsetof(CExtraMissionShelt, saved) == sizeof(void *), "CExtraMissionShelt.saved");
static_assert(offsetof(CExtraMissionShelt, shelt) == 2 * sizeof(void *), "CExtraMissionShelt.shelt");

// The instance (sdata 0x36b620, gp-0x7ed0): set by the constructor, cleared
// by the destructor; nullptr outside an extra mission.
CExtraMissionShelt *GetExtraMissionShelt(void);     // FF3 0x0010b928

#endif /* _INGAME_MISSION_EXTRAMISSIONSHELT_H */
