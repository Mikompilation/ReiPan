/* ==========================================================================
 *  ingame/mission/mission_end.h
 *
 *  The end of an extra mission, FF3 0x1acc18..0x1ae918 (FF3-only; no FF2
 *  match, file name kept from link order, which the code fits: it sits
 *  between mission_list.c and mission_start.c and only serves the mission
 *  mode).
 *
 *  - The mission loading screen's hooks (MISSION_LOAD_PHF + language):
 *    the current mission's texts and its group / number digits.
 *  - Phase 50 Story_Mission_End (child of Story_Main): records the result,
 *    loads the three result layouts -- MISSION_BONUS_PHF (the "every
 *    mission played / topped" notice), MISSION_CLEAR_PHF, MISSION_FAILED_PHF
 *    (+ language) -- while the BGM volume fades out, and on the way out
 *    fades the screen and prepares the retry.
 *  - Phase 77 Story_Mission_Result (its child): one result screen, with
 *    BGM_07_SUCCESS / BGM_08_FAILURE; OK leaves to phase 11, or back to 50
 *    for the next screen while a completion notice is pending.
 * ======================================================================== */
#ifndef _INGAME_MISSION_MISSION_END_H
#define _INGAME_MISSION_MISSION_END_H

#include "sdk/scetypes.h"

// MissionEndGetNotice: the completion notice still to show.
enum MISSION_NOTICE
{
    MISSION_NOTICE_PLAYED = 0,          // MissionIsComplete(0): every mission played
    MISSION_NOTICE_TOP    = 1,          // MissionIsComplete(1): every mission at MISSION_STATE_TOP
    MISSION_NOTICE_NONE   = 3,
};

// MissionEndGetScreen: the result layout phase 77 shows.
enum MISSION_SCREEN
{
    MISSION_SCREEN_BONUS  = 0,          // MISSION_BONUS_PHF: a completion notice
    MISSION_SCREEN_CLEAR  = 1,          // MISSION_CLEAR_PHF: the first screen after a clear
    MISSION_SCREEN_FAILED = 2,          // MISSION_FAILED_PHF
};

// The mission loading screen (CcmLoadingMission::OnReady).
void MissionLoadSetHooks(void);         // FF3 0x001ad0d0

// Phase 50 Story_Mission_End.
int  init_Story_Mission_End(void);      // FF3 0x001ae040
void pre_Story_Mission_End(void);       // FF3 0x001ae310
int  one_Story_Mission_End(void);       // FF3 0x001ae318
void after_Story_Mission_End(void);     // FF3 0x001ae358
int  end_Story_Mission_End(void);       // FF3 0x001ae3d0

// Phase 77 Story_Mission_Result.
int  init_Story_Mission_Result(void);   // FF3 0x001ae598
int  one_Story_Mission_Result(void);    // FF3 0x001ae700
int  end_Story_Mission_Result(void);    // FF3 0x001ae7d0

#endif /* _INGAME_MISSION_MISSION_END_H */
