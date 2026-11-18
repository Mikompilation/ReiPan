/* ==========================================================================
 *  reipan/debug/reipan_events.h
 *
 *  PC debug tool, the Debug menu's "Event browser" (after MioPan's): every
 *  event in the game by Hour, with its conditions, its programs and its live
 *  state, and the way to take the game to any of them. None of this exists
 *  in FF3.
 *
 *  reipan_events.cpp is the engine: the catalogue, read out of the event
 *  file resident at EVENT_DATA_ADDR, and the driver that takes the game to
 *  an event, stepped once a frame by ReiPan_EventsFrame (GameSystemFrame,
 *  after the warp's work). Its moves go through the start point window's
 *  jobs (reipan_warp.h). reipan_events_window.cpp draws the window.
 *
 *  For scripted runs REIPAN_EVENT=<id> asks at boot for a jump to event
 *  <id> (REIPAN_EVENT=go:<id> a teleport, chapter:<n> an Hour's start).
 * ======================================================================== */
#ifndef REIPAN_EVENTS_H
#define REIPAN_EVENTS_H

#include <string>
#include <vector>

void ReiPan_EventsFrame(void);              // game thread, top of every frame
void ReiPan_EventsDrawWindow(bool *open);   // the Debug menu's window (open: its toggle)

namespace ReiPanEvents
{

// ingame_wrk.b01 runs 0..14: the prologue, Hours I..XIII and the Final
// Hour, one chapter event each under ev88.
constexpr int kChapterCount = 15;

constexpr int kKeepRot = -1000;         // Place.rot: no facing asked for

// Where an event's open conditions put the player.
struct Place
{
    int label = -1;                     // event rectangle (PLYR_AREAIN: area_id * 1000 + record), -1 none
    int area  = -1;                     // the game's area (PLYR_ROOMIN, GetPlyrAreaNo), -1 unknown
    int rot   = kKeepRot;               // facing in degrees, from PLYR_ROT
    bool look = false;                  // PLYR_INTO_SIGHT: face (look_x, look_z), from the room origin
    int  look_x = 0, look_z = 0;

    bool Known() const { return area >= 0; }
};

struct Event
{
    int              parent = -1;
    std::vector<int> subs;

    // The chapter tree it sits in (ingame_wrk.b01), -1 for the other trees,
    // which run in every chapter.
    int  chapter = -1;
    // A chapter's start event (the chapter event's first sub-event): what a
    // game started in that chapter runs. A jump waits for it.
    bool entry = false;
    // A leaf that opens on its chapter's start event alone: the chapter's
    // own set-up, never skipped.
    bool intro_leaf = false;
    // Has an init or main program.
    bool has_program = false;
    // CHECK_GAME_WORLD in its first alternative: 0 the dream, 1 the real
    // world, -1 either; CHECK_DAY_FLAG: the map side, 0 day, 1 night.
    int world = -1;
    int side  = -1;

    Place place;                        // its own trigger, from its open conditions
    Place go;                           // where a teleport for it goes (place, else inherited)
    int   room_hint = -1;               // area of the nearest ancestor opening on a lone PLYR_ROOMIN

    std::string trigger;                // "examine, rect 10033 (area 1)"
    std::string content;                // "movie, message"
    std::string search;                 // lower-case text the filter matches
};

struct Catalogue
{
    std::vector<Event> events;          // indexed by event id
    int chapter_event[kChapterCount] = {};  // under ev88, by b01; -1 none
    int chapter_root = -1;              // ev88
    int room_root    = -1;              // ev1060: a group per area
    std::vector<int> other_roots;       // ev0's other sub-events: ev1 and friends
    bool from_memory = false;           // the event file is resident now (else the copy from the last game)
};

// nullptr until a game has loaded the event file; after that the copy taken
// then, so the list stays up at the title.
const Catalogue *GetCatalogue();

// What a request can do from here; `why` gets a sentence when not.
bool CanStartChapter(const char **why);
bool CanTeleport(const char **why);

// A new game in `chapter`, from its start event.
void StartChapter(int chapter);
// A new game in the event's chapter that starts where the event does, with
// everything it waits on settled (an event with no place: the chapter's own
// start, then the player taken to it).
void JumpToEvent(int id);
// The player taken to where the event starts, in the game as it stands.
void TeleportToEvent(int id);

// The ROM's forced state changes on one event. Start now and waiting again
// go through the parked queue (Req_CompulsionSetEventState), so they apply
// in the next EventMain.
void ForceStart(int id);
void ForceEnd(int id);
void Rearm(int id);

bool               Busy();
const std::string &Status();
bool               CanProceed();        // a teleport held back by a running event
void               Proceed();
void               Cancel();

int         StateOf(int id);            // live state, -1 outside a game
int         RunCount(int id);           // times it ran (EVENT_STATE::cnt), -1 outside a game
const char *StateName(int state);
bool        IsArmed(int id);            // waiting, with its parent running: its conditions are polled

// Text of an event's conditions (EV_TBL_OPEN_COND / EV_TBL_CLOSE_COND), of
// its three programs (names only, or every command with its arguments) and
// of a place.
std::string DescribeConditions(int id, int table);
std::string DescribePrograms(int id, bool args);
std::string DescribePlace(const Place &place);

// The same conditions judged now, one group per alternative: live 1 true,
// 0 false, -1 not judged here (outside a game, or a condition whose handler
// does more than read: PUSH_PAD plays its sound, EV_RANDOM draws a number).
struct LiveCond
{
    std::string text;
    int         live;
};
std::vector<std::vector<LiveCond>> JudgeConditions(int id, int table);

Place       PlayerPlace();              // area -1 outside a game
int         CurrentChapter();           // -1 outside a game
int         CurrentWorld();             // -1 outside a game
const char *ChapterLabel(int chapter);  // "Hour II: The Manor of Sleep"

} // namespace ReiPanEvents

#endif /* REIPAN_EVENTS_H */
