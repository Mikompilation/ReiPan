/* ==========================================================================
 *  ingame/mission/mission_mode.c
 *
 *  The running extra mission, FF3 0x1aa9a8..0x1ab180 (FF3-only; FF2 has no
 *  counterpart): FINDER_MODE_WRK's methods. The shots (plyr_shot.c) and
 *  the enemy deaths (ene_main.c) feed its counters, IngameMain runs its
 *  clocks, IngameDrawSub draws the mission HUD (the counted value and the
 *  time left, top right), and the mission end (mission_end.c) ranks the
 *  result and records it in the save block.
 *
 *  Times are frames; the HUD and the ranks count whole seconds (25 fps on
 *  PAL, else 30). FF3 asks GetPALMode at every use; this file does too.
 * ======================================================================== */
#include "ingame/mission/mission_mode.h"

#include "graphics/graph2d/message.h"       // SetCurColorRGB
#include "ingame/ingame_wrk.h"              // ingame_wrk.plyr_wrk
#include "ingame/menu/start_menu.h"         // StartMenuMissionDrawValue
#include "ingame/mission/mission_list.h"    // MissionGetDef, MissionGetRecord
#include "ingame/plyr/player.h"             // PLYR_WRK, PLYR_MODE_7, PlyrGet348
#include "main/gphase.h"                    // GetGPhaseCtrl
#include "system/os/system.h"               // GetPALMode

// FF3 0x002b90f8: the out-of-line clamp several files share (puzzle.c has
// the same copy; its home is not known yet).
static int MissionModeClamp(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int MissionModeFps(void)
{
    return GetPALMode() != 0 ? 25 : 30;
}

// ──────────────────────────────────────────────────────────────────────
// The mission's definition and its save record.
// FF3 0x001aa9a8

FINDER_BANK_ENTRY *FINDER_MODE_WRK::GetDef()
{
    return MissionGetDef(mission_grp, mission_no);
}

// FF3 0x001aa9c8 (the matcher had named it FF2 FinderBankIsLoopSnd)
MISSION_RECORD *FINDER_MODE_WRK::GetRecord()
{
    return MissionGetRecord(mission_grp, mission_no);
}

// ──────────────────────────────────────────────────────────────────────
// value against ref for a mission of this type: times and the shot count
// are better when lower, the rest when higher. No value (-1) is worse, no
// ref (-1) is beaten by any value.
// FF3 0x001aa9e8

int FINDER_MODE_WRK::Compare(int value, int ref, int type)
{
    int better;

    if (value == -1)
        return MISSION_CMP_WORSE;
    if (ref == -1)
        return MISSION_CMP_BETTER;
    if (value == ref)
        return MISSION_CMP_EQUAL;
    if (type == MISSION_TYPE_TIME || type == MISSION_TYPE_SHOTS || type == MISSION_TYPE_TIME_348)
        better = value < ref;
    else
        better = ref < value;
    return better ? MISSION_CMP_BETTER : MISSION_CMP_WORSE;
}

// ──────────────────────────────────────────────────────────────────────
// EV_MISSION_CLEAR: the mission has a time limit and the clock has
// reached it (both in whole seconds); the clear then counts as failed.
// FF3 0x001aaa38
// callcheck: GetPALMode (MissionModeFps)

int FINDER_MODE_WRK::IsTimeUp()
{
    int limit = GetDef()->limit;
    int sec;

    if (limit == -1)
        return 0;
    sec = time / MissionModeFps();
    return limit / MissionModeFps() <= sec;
}

// ──────────────────────────────────────────────────────────────────────
// The HUD shows unless a scripted view runs (plyr _0e0 bit 0), the player
// is in the death scene, or the phase is the mission's start (31), end
// (50) or result (77).
// FF3 0x001aaad8 (FF3 caches the GPhaseCtrl pointer in sbss gp-0xca4)

static int FinderModeIsHudShown(void)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;

    if ((plyr->_0e0 & 1) != 0 || plyr->mode == PLYR_MODE_7)
        return 0;
    switch (GetGPhaseCtrl()->leaf->id) {
    case 0x1f:                          // Story_Mission_St
    case 0x32:                          // Story_Mission_End
    case 0x4d:                          // Story_Mission_Result
        return 0;
    default:
        return 1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// IngameMain, once a frame: once end_Story_Mission_St has started them,
// the clocks run up to 9:59:59; time_348 only while the player's _348 is
// 2.
// FF3 0x001aab78
// callcheck: FUN_002b90f8, GetPALMode (MissionModeClamp, MissionModeFps)

void FINDER_MODE_WRK::Main()
{
    int max;

    if (stop != 0)
        return;
    max = MissionModeFps() * (9 * 3600 + 59 * 60 + 59);    /* FF3 sums h, m, s */
    time = MissionModeClamp(time + 1, 0, max);
    if (PlyrGet348() == 2)
        time_348 = MissionModeClamp(time_348 + 1, 0, max);
}

// ──────────────────────────────────────────────────────────────────────
// IngameDrawSub: the mission HUD, top right at x 563 -- the value the
// mission counts (y 24) and, for a mission with a limit, the time left in
// whole seconds (y 47), in the mission menu's grey. hud_fade runs down
// while the HUD may show, up while not (a shot puts it at the top); in
// its lower half the HUD fades in, alpha 0x80 at 0.
// FF3 0x001aac48
// callcheck: FUN_002b90f8 (MissionModeClamp)

void FINDER_MODE_WRK::Draw()
{
    static const float value_pos[2] = { 563.0f, 24.0f };   /* sdata 0x370368 */
    static const float limit_pos[2] = { 563.0f, 47.0f };   /* sdata 0x370370 */
    FINDER_BANK_ENTRY *def;
    float pos[2];
    int   top;
    int   half;
    int   alpha;
    int   left;

    if (FinderModeIsHudShown() == 0)
        hud_fade++;
    else
        hud_fade--;
    top  = GetPALMode() != 0 ? 0x32 : 0x3c;
    half = GetPALMode() != 0 ? 0x19 : 0x1e;
    hud_fade = MissionModeClamp(hud_fade, 0, top);
    if (hud_fade >= half)
        return;

    def = GetDef();
    SetCurColorRGB(0x92, 0x99, 0x93);
    alpha = MissionModeClamp(0x80 - (hud_fade << 7) / (top - half), 0, 0x80);
    pos[0] = value_pos[0];
    pos[1] = value_pos[1];
    StartMenuMissionDrawValue(def->type, GetValue(), pos, alpha);
    if (def->limit != -1) {
        pos[0] = limit_pos[0];
        pos[1] = limit_pos[1];
        left = def->limit / MissionModeFps() - time / MissionModeFps();
        if (left < 0)
            left = 0;
        StartMenuMissionDrawValue(MISSION_TYPE_TIME, left * MissionModeFps(), pos, alpha);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A shot that scored (PlyrShotScore, PlyrShotSubjectObj): the points add
// up, the best single shot is kept, the shot is counted, and the HUD
// hides to fade back in.
// FF3 0x001aae20
// callcheck: FUN_002b90f8 (MissionModeClamp)

void FINDER_MODE_WRK::AddScore(int add)
{
    score = MissionModeClamp(score + add, 0, 999999);
    if (best_shot < add)
        best_shot = add;
    shots = MissionModeClamp(shots + 1, 0, 999);
    hud_fade = GetPALMode() != 0 ? 0x32 : 0x3c;
}

// EneDamageApply: a ghost is defeated.
// FF3 0x001aaeb0
// callcheck: FUN_002b90f8 (MissionModeClamp)
void FINDER_MODE_WRK::AddKill()
{
    kills = MissionModeClamp(kills + 1, 0, 999);
}

// PlyrShotJudge: the level of the ghost in the shot.
// FF3 0x001aaef0 (the matcher had named it FF2 PlyrCamCondChk)
// callcheck: FUN_002b90f8 (MissionModeClamp)
void FINDER_MODE_WRK::SetMaxLevel(int level)
{
    if (max_level < level)
        max_level = MissionModeClamp(level, 0, 999);
}

// ──────────────────────────────────────────────────────────────────────
// The value the mission counts; times are cut to whole seconds.
// FF3 0x001aaf38
// callcheck: GetPALMode (MissionModeFps)

int FINDER_MODE_WRK::GetValue()
{
    int v;

    switch (GetDef()->type) {
    case MISSION_TYPE_BEST_SHOT: return best_shot;
    case MISSION_TYPE_SCORE:     return score;
    case MISSION_TYPE_TIME:      v = time;     break;
    case MISSION_TYPE_KILLS:     return kills;
    case MISSION_TYPE_SHOTS:     return shots;
    case MISSION_TYPE_LEVEL:     return max_level;
    case MISSION_TYPE_TIME_348:  v = time_348; break;
    default:                     return 0;
    }
    return v - v % MissionModeFps();
}

// ──────────────────────────────────────────────────────────────────────
// The result's rank: MISSION_STATE_NONE without a clear, else 5 for the
// first (hardest) threshold the value meets, down to 1 for the fifth, 0
// below them all.
// FF3 0x001ab000

int FINDER_MODE_WRK::GetRank()
{
    FINDER_BANK_ENTRY *def = GetDef();
    int value = GetValue();
    int i;

    if (result == 0)
        return MISSION_STATE_NONE;
    for (i = 0; i < MISSION_RANK_NUM; i++) {
        if (Compare(value, def->rank[i], def->type) != MISSION_CMP_WORSE)
            return MISSION_RANK_NUM - i;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// init_Story_Mission_End: a clear's rank replaces a lower one (or none),
// its value a worse best.
// FF3 0x001ab0a8

void FINDER_MODE_WRK::RecordResult()
{
    FINDER_BANK_ENTRY *def = GetDef();
    MISSION_RECORD    *rec = GetRecord();
    int rank;
    int value;

    if (result == 0)
        return;
    rank  = GetRank();
    value = GetValue();
    if (rec->state == MISSION_STATE_NONE || rec->state < rank)
        rec->state = rank;
    if (Compare(value, rec->best, def->type) != MISSION_CMP_WORSE)
        rec->best = value;
}

// FF3 0x001ab160 (MissionModeGPhaseCtrlAtExit) is compiler output: the
// atexit thunk of the GPhaseCtrl singleton (GPhaseCtrlTerm(0x41e468)) that
// this file's inline GetGPhaseCtrl instantiates; implicit for the C++
// static.
