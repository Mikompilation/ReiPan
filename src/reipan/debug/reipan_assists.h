/* ==========================================================================
 *  reipan/debug/reipan_assists.h
 *
 *  PC test aid, ImGui Assists > Unlocks > "Unlock everything": puts the game
 *  in the state a fully played save would be in, so the post-clear content
 *  can be tested without earning it. None of this exists in FF3.
 *
 *  What it sets, each through the works the game itself reads:
 *
 *    - every difficulty on the start menu's select (title_item_open 0..3:
 *      Hard after a clear, Nightmare after a Hard clear);
 *    - one clear when there is none (FINDER_STATE_WRK.cnt), so New Game
 *      opens the start menu and the game counts as cleared -- every memo
 *      and soul-list entry listed, the clear-count events open -- and both
 *      endings' gallery movies (_04 / _08). A fresh New Game also closes
 *      the start menu's Continue (_0c), as a clear save has it; none is
 *      put while a warp or event-browser job starts a game, which would
 *      stop in the start menu;
 *    - every bonus-shop entry bought (FINDER_SLOT_WRK 0..32 = 2): costumes,
 *      accessories, both gallery picture sets, lenses and camera functions,
 *      the last two given to the lens work in game as the shop's purchase
 *      does (StartMenuOptionApplyBought);
 *    - every extra-mission group open (MISSION_SAVE.group_open).
 *
 *  IngameInit wipes the start menu's works and the missions on every game
 *  start, and a card load replaces them, so it is put back at the top of
 *  every frame while ticked (ReiPan_AssistsFrame, GameSystemFrame). A save
 *  made meanwhile keeps it; unticking stops it and leaves what is set.
 *
 *  For scripted runs REIPAN_UNLOCK_ALL=1 ticks it at boot.
 * ======================================================================== */
#ifndef REIPAN_ASSISTS_H
#define REIPAN_ASSISTS_H

void ReiPan_AssistsFrame(void);             // game thread, top of every frame
int  ReiPan_AssistsUnlockAll(void);         // the menu's tick
void ReiPan_AssistsSetUnlockAll(int on);

#endif /* REIPAN_ASSISTS_H */
