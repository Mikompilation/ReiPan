/* ==========================================================================
 *  ingame/event/prg/ev_main.h
 *
 *  The event system's top level (FF3 0x19b768..0x19baf8; FF2 ev_main.c):
 *  init / release of every event subsystem, the event file load, the
 *  per-frame driver and the save hooks. FF3 moved FF2's ev_wrk and the
 *  sister gaze out of this file (ev_change.c holds the event states).
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_MAIN_H
#define _INGAME_EVENT_PRG_EV_MAIN_H

// The event file (EVENT_OBJ, EVENT_DB_OBJ or EV_MISSION(_50)_OBJ) is loaded
// to this EE address; EvGetTblAddr 0x19b2a8 reads its table there.
#define EVENT_DATA_ADDR     0xf82000

void EventSetMode(int mode);            // FF3 0x0019b768: 1 / 2 load EVENT_DB_OBJ; 2 also keeps the start area (IngameNowLoadingMain)
int  EventGetMode(void);                // FF3 0x0019b778
void EventSetStop(int stop);            // FF3 0x0019b788: nonzero skips EventMain (no callers)
void EventInit(void);                   // FF3 0x0019b798
void EventRelease(void);                // FF3 0x0019b808
int  EventDataIsLoadEnd(void);          // FF3 0x0019b868: the event file, ladder list and VCI pack are in (no callers)
void EventDataLoad(int mission);        // FF3 0x0019b8c0: 1 (mission mode) EV_MISSION_OBJ / _50 by PAL mode
void EventOpenConditionClear(void);     // FF3 0x0019b950: EventSetOpenCondition(0)
void EventMain(void);                   // FF3 0x0019b968
void EventAfterLoad(void);              // FF3 0x0019b9e8: after a game file was loaded from the card: offsets back to addresses
void EventEnd(void);                    // FF3 0x0019ba08: before a game file is copied for the card: addresses to offsets
void EventAfterSave(void);              // FF3 0x0019ba28: after the copy (EventAfterLoad)

#endif /* _INGAME_EVENT_PRG_EV_MAIN_H */
