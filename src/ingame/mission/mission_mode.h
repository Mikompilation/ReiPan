/* ==========================================================================
 *  ingame/mission/mission_mode.h
 *
 *  The running extra mission, FF3 0x1aa9a8..0x1ab180 (FF3-only; file name
 *  from the link order, pinned 2026-10-20): FINDER_MODE_WRK, its counters
 *  (score, shots, ghosts, clocks), the in-game mission HUD and the clear's
 *  rank. FinderModeWrkStart (ingame/menu/start_menu.c) resets it when a
 *  mission is picked; end_Story_Mission_St starts its clocks; the mission
 *  end phases (ingame/mission/mission_end.c) read and record it.
 * ======================================================================== */
#ifndef _INGAME_MISSION_MISSION_MODE_H
#define _INGAME_MISSION_MISSION_MODE_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/mission/mission_list.h"    // FINDER_BANK_ENTRY, MISSION_RECORD

// FINDER_BANK_ENTRY.type: the value a mission counts (GetValue) and which
// way is better (Compare).
enum
{
    MISSION_TYPE_BEST_SHOT = 0,         // best single shot's points (higher is better)
    MISSION_TYPE_SCORE     = 1,         // the points of all shots (higher)
    MISSION_TYPE_TIME      = 2,         // play time (lower)
    MISSION_TYPE_KILLS     = 3,         // ghosts defeated (higher)
    MISSION_TYPE_SHOTS     = 4,         // shots that scored (lower)
    MISSION_TYPE_LEVEL     = 5,         // highest ghost level shot (higher)
    MISSION_TYPE_TIME_348  = 6,         // time with the player's _348 at 2 (lower)
};

// Compare's answers.
#define MISSION_CMP_BETTER  0           // value beats ref, or there is no ref yet
#define MISSION_CMP_EQUAL   1
#define MISSION_CMP_WORSE   2           // or no value

// Finder-mode work (singleton bss 0x424ad0; getter GetFinderModeWrk FF3
// 0x002b8ce0 in mission_end.c's run): the running extra mission.
struct FINDER_MODE_WRK
{
    FINDER_BANK_ENTRY *GetDef();        // FF3 0x001aa9a8 (FinderBankGetEntry): MissionGetDef(group, no)
    MISSION_RECORD    *GetRecord();     // FF3 0x001aa9c8 (FinderModeGetRecord): MissionGetRecord(group, no)
    int  Compare(int value, int ref, int type); // FF3 0x001aa9e8 (FinderModeCompare): MISSION_CMP_*
    int  IsTimeUp();                    // FF3 0x001aaa38 (FinderModeIsTimeUp): the definition's limit reached, in whole seconds
    void Main();                        // FF3 0x001aab78 (FinderModeWrkMain): the clocks, once a frame
    void Draw();                        // FF3 0x001aac48 (FinderModeWrkDraw): the HUD: the value, and the time left
    void AddScore(int add);             // FF3 0x001aae20 (FinderModeAddScore): a scoring shot
    void AddKill();                     // FF3 0x001aaeb0 (FinderModeAddKill): a ghost defeated (EneDamageApply)
    void SetMaxLevel(int level);        // FF3 0x001aaef0 (FinderModeSetMaxLevel): level = max(level, this), 0..999
    int  GetValue();                    // FF3 0x001aaf38 (FinderModeGetValue): the value the definition's type counts
    int  GetRank();                     // FF3 0x001ab000 (FinderModeGetRank): MISSION_STATE_NONE when not cleared, else 5 - the first rank met, 0 if none
    void RecordResult();                // FF3 0x001ab0a8 (FinderModeRecordResult): a clear's rank and best value into its MISSION_RECORD

    /* 0x00 */ int    mission_grp;  // CHECK_MISSION: mission_grp * 5 + mission_no
    /* 0x04 */ int    mission_no;
    /* 0x08 */ int    score;        // MISSION_TYPE_SCORE, 0..999999
    /* 0x0c */ int    best_shot;    // MISSION_TYPE_BEST_SHOT
    /* 0x10 */ int    time;         // frames played, 0..9:59:59 (MISSION_COUNTER_IS_MORE)
    /* 0x14 */ int    kills;        // MISSION_TYPE_KILLS, 0..999
    /* 0x18 */ int    shots;        // MISSION_TYPE_SHOTS, 0..999
    /* 0x1c */ int    max_level;    // MISSION_TYPE_LEVEL, 0..999
    /* 0x20 */ int    time_348;     // frames with ingame_wrk.plyr_wrk->_348 == 2
    /* 0x24 */ int    hud_fade;     // HUD fade, 0 (shown) .. 50 (PAL) / 60 (hidden); a shot sets it to the top
    /* 0x28 */ int    result;       // EV_MISSION_CLEAR: 1 (cleared); the mission menu adds its points then
    /* 0x2c */ int    stop;         // non-zero: the clocks stand; 1 at the start, 0 from end_Story_Mission_St
};
static_assert(sizeof(FINDER_MODE_WRK) == 0x30, "FINDER_MODE_WRK is 0x30 bytes");
static_assert(offsetof(FINDER_MODE_WRK, mission_no) == 0x04, "FINDER_MODE_WRK.mission_no");
static_assert(offsetof(FINDER_MODE_WRK, score) == 0x08, "FINDER_MODE_WRK.score");
static_assert(offsetof(FINDER_MODE_WRK, best_shot) == 0x0c, "FINDER_MODE_WRK.best_shot");
static_assert(offsetof(FINDER_MODE_WRK, time) == 0x10, "FINDER_MODE_WRK.time");
static_assert(offsetof(FINDER_MODE_WRK, kills) == 0x14, "FINDER_MODE_WRK.kills");
static_assert(offsetof(FINDER_MODE_WRK, shots) == 0x18, "FINDER_MODE_WRK.shots");
static_assert(offsetof(FINDER_MODE_WRK, max_level) == 0x1c, "FINDER_MODE_WRK.max_level");
static_assert(offsetof(FINDER_MODE_WRK, time_348) == 0x20, "FINDER_MODE_WRK.time_348");
static_assert(offsetof(FINDER_MODE_WRK, hud_fade) == 0x24, "FINDER_MODE_WRK.hud_fade");
static_assert(offsetof(FINDER_MODE_WRK, result) == 0x28, "FINDER_MODE_WRK.result");
static_assert(offsetof(FINDER_MODE_WRK, stop) == 0x2c, "FINDER_MODE_WRK.stop");
FINDER_MODE_WRK *GetFinderModeWrk(void);    // FF3 0x002b8ce0

#endif /* _INGAME_MISSION_MISSION_MODE_H */
