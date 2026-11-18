/* ==========================================================================
 *  reipan/debug/reipan_warp.h
 *
 *  PC debug tool, the ImGui "Debug -> Start point / snapshots..." window:
 *  begin the game at a chosen point to debug events, NPCs and rooms without
 *  playing up to them. None of this exists in FF3.
 *
 *    - Warp: a new game that starts in a chosen chapter (ingame_wrk.b01:
 *      the chapter's own start event runs, FF3's chapter select), area,
 *      world (dream manor / real-world house), side and player. It leaves
 *      play the way pause -> quit does, starts the game from the title the
 *      way New Game does (Title_Mode -> Story_Main -> Story_Start ->
 *      Story_NowLoading) and sets the game work just before
 *      Story_NowLoading picks the start area. The area survives through
 *      FF3's own leftover hook, event mode 2 (EventSetMode: a new game keeps
 *      the start area); modes 1 / 2 also load the developers' event file
 *      EVENT_DB_OBJ, whose one change opens the debug start event 1746
 *      instead of 1060 (optional here).
 *    - Teleport: within play, to another area or beside one of the cat's
 *      position labels, as an event's AREA_CHANGE + PLYR_POS_SET.
 *    - Snapshots: the save blocks of a game file (the card's manifest and
 *      checksum, ReiPan's host layout) written to the user folder during
 *      play and loaded back like a card load. Unlike the card's gather they
 *      skip the blocks whose save callback is still a stub, which end a card
 *      save early (block 3 is the first), and they carry what the stubbed
 *      blocks 27 (ingame_wrk) and 28 (the player) would; the port's own card
 *      saves do not load back yet and are not offered here.
 *    - Live views: the phase, area, world and chapter; any event's state
 *      with open / start buttons; the cat's work with a few buttons.
 *    - The event browser's moves (ReiPan_WarpGoTo): a teleport within the
 *      world, or into the other one as a snapshot load from memory with the
 *      area and world changed.
 *
 *  Everything runs on the game thread: ReiPan_WarpFrame at the top of every
 *  frame (GameSystemFrame) does the requested work between two frames;
 *  ReiPan_WarpDrawWindow is the window, drawn at the end of the frame by
 *  reipan_ui.cpp. For scripted runs (build.py --warp / --snapshot) the
 *  environment asks at boot: REIPAN_WARP (a warp, see ReadEnvironment),
 *  REIPAN_SNAPSHOT (a snapshot load) and REIPAN_SNAP_SKIP (save blocks a
 *  load leaves alone, to bisect a block that breaks the game).
 * ======================================================================== */
#ifndef REIPAN_WARP_H
#define REIPAN_WARP_H

void ReiPan_WarpFrame(void);                // game thread, top of every frame
void ReiPan_WarpDrawWindow(bool *open);     // the Debug menu's window (open: its toggle)

// ── For the event browser (reipan/debug/reipan_events.cpp) ──────────────
// The jobs the window starts, asked for by an area and a spot in it.

#define REIPAN_WARP_KEEP_ROT     (-1000)    // ReiPanWarpSpot.rot: keep her facing
#define REIPAN_WARP_ROOM_CENTRE  (-32768)   // ReiPanWarpSpot.x: the middle of the room's largest outline rectangle

// Where to put the player: on an event rectangle (label >= 0, PLYR_AREAIN's
// label: the centre of its first rectangle, on the floor of its file), else
// at a room position as AREA_CHANGE + PLYR_FLOOR_CHANGE + PLYR_POS_SET.
struct ReiPanWarpSpot
{
    int area;
    int label;                  // -1: the room position below
    int mh;                     // map hit object (PLYR_FLOOR_CHANGE), -1 keep
    int x, z;                   // from the room origin (PLYR_POS_SET)
    int rot;                    // facing in degrees (PLYR_ROT_SET)
    int look;                   // 1: face (look_x, look_z) instead (PLYR_INTO_SIGHT's point)
    int look_x, look_z;         // from the room origin
};

int         ReiPan_WarpIdle(void);      // no warp, load, move or teleport under way
int         ReiPan_WarpInGame(void);    // Story_Main on the phase stack
int         ReiPan_WarpAtTitle(void);   // Title_Mode on the phase stack
int         ReiPan_WarpInPlay(void);    // Story_Normal, nothing requested
const char *ReiPan_WarpStatus(void);    // the window's status line
int         ReiPan_WarpWorldOf(int area);   // 0 dream (the manor), 1 real (the house)
void        ReiPan_WarpChapter(int chapter);   // a new game in the chapter, at its own start event
// A new game in the chapter that starts at spot (its area's world, side
// 0 day / 1 night): settle() runs in Story_NowLoading once the game work is
// set and before any event opens, the player is put on the spot when the
// room is in, and the screen fades in (a start event that would have is
// expected to be settled).
void        ReiPan_WarpJump(int chapter, const ReiPanWarpSpot *spot, int side, void (*settle)(void));
// In play. Within the current world a teleport (the room loads as
// AREA_CHANGE does); into the other world the game is reloaded from memory
// there, as a snapshot load, after that world's EVENT_REBOOT (7 waking up,
// 8 falling asleep), which the scripts' own world changes run.
void        ReiPan_WarpGoTo(const ReiPanWarpSpot *spot);

#endif /* REIPAN_WARP_H */
