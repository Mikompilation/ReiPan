/* ==========================================================================
 *  ingame/story_game_clear.c
 *
 *  Phase 42, Story_GameClear: owns the CGameClear for the length of the
 *  phase. FF3 0x258a68..0x258b20, FF3's own code.
 * ======================================================================== */
#include "ingame/story_game_clear.h"

#include "common/cmn_fade_layer.h"                      // GetCmnFadeLayer, capCtl::Update
#include "ingame/clear/GameClear.h"                     // CGameClear
#include "system/b2d/BpCtl.h"                           // BpCtlMain

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static CGameClear *story_game_clear;                    // sbss 0x372600 (gp-0xef0)

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00258a68.
// callcheck: operator_new, GameClearCtor (new CGameClear)

int init_Story_GameClear(void)
{
    story_game_clear = new CGameClear();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00258aa0.

int end_Story_GameClear(void)
{
    if (story_game_clear != nullptr)
        delete story_game_clear;                        // vtable +0x04, the deleting dtor
    story_game_clear = nullptr;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the sequence; 1 (the phase ends) when CGameClear::Run is done.
// The layout is drawn only while it runs.
// FF3 0x00258ad8.

int one_Story_GameClear(void)
{
    GetCmnFadeLayer()->Update();
    int done = story_game_clear->Run();
    if (done == 0)
        BpCtlMain();
    return done != 0;
}
