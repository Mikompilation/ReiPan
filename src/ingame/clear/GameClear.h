/* ==========================================================================
 *  ingame/clear/GameClear.h
 *
 *  CGameClear (FF3 RTTI, vtable 0x340ff8): the game-clear sequence of phase
 *  42 (0x2a). After the ending movie (0x15, or 0x16 for the other ending) it
 *  shows the results page of GAMECLEAR_PHF over the outgame backdrop
 *  (difficulty, rank by play time, dream count, play time, best score,
 *  collection count) and then the unlocked rewards ("omake") lists, under
 *  BGM_04_CLEAR. Mission clear / failed (modes 4 / 5) skip all of it.
 *  FF3-only class file 0x0010bba0..0x0010ce28 (FF3's class files link
 *  alphabetically: CExtraMissionShelt, CGameClear, CLightCtl).
 *  Phase 42's init (0x00258a68) news one (0x10 bytes), its one (0x00258ad8)
 *  runs CGameClear::Run and BpCtlMain, its end (0x00258aa0) deletes it.
 * ======================================================================== */
#ifndef _INGAME_CLEAR_GAMECLEAR_H
#define _INGAME_CLEAR_GAMECLEAR_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

struct CB2dBase;                        // system/b2d/B2dBase.h
struct STEP_SEQ;                        // common/cmn_util.h (the step sequencer)

// GameClearSetMode's mode.
#define GAME_CLEAR_MODE_NONE            (-1)    // GAME_CLEAR_ at no difficulty 0..3
#define GAME_CLEAR_MODE_MISSION_CLEAR   4       // EV_MISSION_CLEAR
#define GAME_CLEAR_MODE_MISSION_FAILED  5       // EV_MISSION_FAILED

// CGameClear.rewards: which reward lists have an open reward.
#define GAME_CLEAR_REWARD_LIST1         0x1     // rewards 0..5
#define GAME_CLEAR_REWARD_LIST2         0x2     // rewards 6..9

// 0x10 bytes on the EE.
// layout: host -- the vptr and the two pointers are 4 bytes each on the EE:
// layout at EE +0x04, seq +0x08, rewards +0x0c.
class CGameClear
{
public:
    CGameClear();                       // FF3 0x0010cae8 (GameClearCtor)
    virtual ~CGameClear();              // EE vtable +0x00 0x0010cc28 (base 0x0010cbd8), deleting +0x04 0x0010cc78

    int Run();                          // FF3 0x0010ccd0: one frame; 1 when finished (fade layer cleared)

    CB2dBase *layout;                   // EE +0x04: GAMECLEAR_PHF + GetLanguage()
    STEP_SEQ *seq;                      // EE +0x08: the 12 steps; owner this
    u_int     rewards;                  // EE +0x0c: GAME_CLEAR_REWARD_*, set by the results step
};
static_assert(offsetof(CGameClear, seq) == offsetof(CGameClear, layout) + sizeof(CB2dBase *), "CGameClear.seq follows layout");
static_assert(offsetof(CGameClear, rewards) == offsetof(CGameClear, seq) + sizeof(STEP_SEQ *), "CGameClear.rewards follows seq");

// GAME_CLEAR_ (ev_command.c) and the mission events.
void GameClearKeepFinderFlags(void);            // FF3 0x0010cd28: FINDER_STATE_WRK _04 / _08 before the clear sets them
int  GameClearGetKeptFinderFlag(u_int i);       // FF3 0x0010cd68: _04 (i even) or _08 (i odd) as kept
void GameClearSetMode(int mode, int ending);    // FF3 0x0010cd80: mode 0..3 the difficulty, GAME_CLEAR_MODE_*; ending: movie 0x16
int  GameClearGetMode(void);                    // FF3 0x0010cd90
void GameClearRequest(void);                    // FF3 0x0010cd98: phase 42
void GameClearCountCollection(void);            // FF3 0x0010cdc0: the count / total item 0x417 prints

#endif /* _INGAME_CLEAR_GAMECLEAR_H */
