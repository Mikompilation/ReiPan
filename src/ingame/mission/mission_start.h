/* ==========================================================================
 *  ingame/mission/mission_start.h
 *
 *  The start of an extra mission, FF3 0x1ae918..0x1aedd0 (FF3-only; no FF2
 *  match -- FF2's init/one/end_Story_Mission_St lived in ingame/ingame.c
 *  and ran the mission itself; the file name is kept from link order,
 *  which the code fits: it follows mission_end.c and only serves the
 *  mission mode).
 *
 *  Phase 31 Story_Mission_St (child of Story_Main, requested by the event
 *  command EV_MISSION_START): loads MISSION_START_PHF (+ language) and
 *  preloads BGM_06_START_STR_STR on BGM slot 1, fades in over the frozen
 *  game, waits two seconds, plays the layout's "mission_start" label with
 *  the BGM and, when the label has played, goes on to phase 32
 *  Story_Normal. Its end stops the BGM, frees the layout and starts the
 *  mission's clocks (FINDER_MODE_WRK::stop = 0).
 * ======================================================================== */
#ifndef _INGAME_MISSION_MISSION_START_H
#define _INGAME_MISSION_MISSION_START_H

// Phase 31 Story_Mission_St.
int  init_Story_Mission_St(void);       // FF3 0x001ae918
int  one_Story_Mission_St(void);        // FF3 0x001aea18
int  end_Story_Mission_St(void);        // FF3 0x001aed20

#endif // _INGAME_MISSION_MISSION_START_H
