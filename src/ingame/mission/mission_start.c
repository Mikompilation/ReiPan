/* ==========================================================================
 *  ingame/mission/mission_start.c
 *
 *  The start of an extra mission, FF3 0x1ae918..0x1aedd0 (FF3-only; see
 *  mission_start.h): phase 31 Story_Mission_St, the "mission start" screen
 *  over the frozen room before play.
 *
 *  FF3 reads its singletons through gp-relative cached pointers (sdata
 *  gp-0xca4 the phase controller, gp-0xc60 the system work, gp-0xab0 the
 *  finder-mode work); nothing depends on the cache, so calls go to the
 *  getters directly. FF3 0x001aedb0 (MissionStartAtexitGPhaseCtrl) is not
 *  written: the compiler's atexit thunk for the GetGPhaseCtrl static
 *  (GPhaseCtrlTerm), ending the file.
 * ======================================================================== */
#include "ingame/mission/mission_start.h"

#include "graphics/graph2d/fade.h"          // FadeInReq, GetFadeState, FadeMain
#include "ingame/event/prg/ev_command.h"    // AreaChangeEnd
#include "ingame/ingame.h"                  // IngameMain, IngameDrawSub
#include "ingame/mission/mission_mode.h"    // FINDER_MODE_WRK, GetFinderModeWrk
#include "ingame/sound/snd_ctrl.h"          // SndBgm*
#include "main/gphase.h"                    // GetGPhaseCtrl
#include "system/b2d/BpCtl.h"               // BpCtl*, BpPlayLabel
#include "system/eeiop/cd_file_dat.h"       // MISSION_START_PHF, BGM_06_START_STR_STR
#include "system/os/system.h"               // GetSysWrk, GetPALMode

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define MISSION_START_BGM_SLOT      1
#define MISSION_START_PHASE_NORMAL  0x20    // 32 Story_Normal
#define MISSION_START_SKIP          0xfffffffcu // IngameMain: only the player (bit 0) and the events (bit 1) run

// one_Story_Mission_St's steps (GPHASE_NODE::state_cnt).
enum MISSION_START_STEP
{
    MISSION_START_FADE_REQ  = 0,        // request the fade-in
    MISSION_START_FADE_WAIT = 1,        // until the fade-in has ended
    MISSION_START_HOLD      = 2,        // two seconds (state_wrk counts the frames)
    MISSION_START_PLAY      = 3,        // play the label and the BGM
    MISSION_START_PLAYING   = 4,        // until the label has played
};

static const char mission_start_label[] = "mission_start";  /* rodata 0x358940 */

// PAL 25, NTSC 30 frames: one second (inlined in every FF3 use).
static int MissionStartSecond(void)
{
    return GetPALMode() != 0 ? 25 : 30;
}

static int MissionStartFile(void)
{
    return MISSION_START_PHF + GetSysWrk()->GetLanguage();
}

// ══════════════════════════════════════════════════════════════════════
// Phase 31 Story_Mission_St
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Preloads the BGM and loads the layout once; done when both are ready.
// FF3 0x001ae918.
// callcheck: GetLanguage, GetSysWrk (in MissionStartFile)

int init_Story_Mission_St(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0) {
        SndBgmPreload(BGM_06_START_STR_STR, 100, 0x1000, 0, nullptr, 0, MISSION_START_BGM_SLOT);
        BpCtlLoadReq(MissionStartFile(), nullptr);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    BpCtlMain();
    if (BpCtlIsDataReady(BpCtlGetHandleByFileNo(MissionStartFile())) == 0) {
        return 0;
    }
    return (SndBgmIsPreloaded(MISSION_START_BGM_SLOT) != 0) ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────
// Fades in, holds two seconds, then plays "mission_start" with the BGM;
// when the label has played, on to Story_Normal. The room stands still
// under it (IngameMain runs only the player and the events).
// FF3 0x001aea18.
// callcheck: GetLanguage, GetPALMode, GetSysWrk (in MissionStartFile, MissionStartSecond)

int one_Story_Mission_St(void)
{
    GPHASE_NODE *leaf = GetGPhaseCtrl()->leaf;
    int          done = 0;
    int          cnt;

    switch (leaf->state_cnt) {
    case MISSION_START_FADE_REQ:
        FadeInReq(0, 0, 0, (u_int)MissionStartSecond());
        leaf->state_cnt = MISSION_START_FADE_WAIT;
        break;
    case MISSION_START_FADE_WAIT:
        if (GetFadeState() != 0) {
            leaf->state_cnt = MISSION_START_HOLD;
        }
        break;
    case MISSION_START_HOLD:
        leaf->state_wrk = (short)((short)leaf->state_wrk + 1);
        cnt = (short)leaf->state_wrk;
        if (cnt >= MissionStartSecond() * 2) {      /* FF3: 60 NTSC, 50 PAL */
            leaf->state_cnt = MISSION_START_PLAY;
        }
        break;
    case MISSION_START_PLAY:
        BpPlayLabel(BpCtlGetHandleByFileNo(MissionStartFile()), mission_start_label);
        SndBgmStartPreloaded(MISSION_START_BGM_SLOT);
        leaf->state_cnt = MISSION_START_PLAYING;
        break;
    case MISSION_START_PLAYING:
        done = (BpCtlIsLoadEnd(MissionStartFile()) != 0);
        break;
    default:
        break;
    }

    IngameMain(MISSION_START_SKIP);
    IngameDrawSub(0);
    BpCtlMain();
    FadeMain();
    if (!done) {
        return 0;
    }
    GetGPhaseCtrl()->SetNext(MISSION_START_PHASE_NORMAL, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Fades the BGM out over a second, frees the layout, ends the area change
// and starts the mission's clocks.
// FF3 0x001aed20.
// callcheck: GetLanguage, GetPALMode, GetSysWrk (in MissionStartFile, MissionStartSecond)

int end_Story_Mission_St(void)
{
    SndBgmStop(MissionStartSecond(), MISSION_START_BGM_SLOT);
    BpCtlRelease(BpCtlGetHandleByFileNo(MissionStartFile()));
    AreaChangeEnd(1);
    GetFinderModeWrk()->stop = 0;
    return 1;
}
