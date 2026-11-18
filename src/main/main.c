/* ==========================================================================
 *  main/main.c
 *
 *  The GPhase root: GameSystemStart (phase tree, first phase, per-session
 *  inits), GameSystemFrame (one phase-engine step a frame), GameSystemEnd
 *  (tear-down), and the super phase (phase 0, root of the 120-phase tree).
 *  FF3 0x1d2610..0x1d2a40. FF2 kept these roles in main.c together with main();
 *  FF3's main() is in system/os/system.c, and FF2's init_super bring-up moved
 *  to InitMainSystem (once) and GameSystemStart (per session).
 *
 *  FF3 0x001d29f8 is not written: a compiler-emitted static-destruction thunk
 *  (GPhaseCtrlTerm on the GPhaseCtrl singleton) that marks the end of the file.
 * ======================================================================== */
#include "main/main.h"

#include "main/gphase.h"                                // GPhaseCtrl*, GPhaseTree*, gphase_tree_def

#include "common/cmn_fade_layer.h"                       // GetCmnFadeLayer / capCtl::Reset/Init/Release
#include "graphics/gs_reg.h"                            // SetDefaultGsRegs
#include "graphics/graph3d/gra3d.h"                     // gra3dMonotoneDrawEnable / gra3dSetDrawParam
#include "graphics/movie/movie.h"                       // EndMovie, MovieResetPlaying
#include "ingame/enemy/ene_spe_obj.h"                     // GetEneSpeObjTbl
#include "ingame/event/prg/ev_se.h"                     // ev_seRelease
#include "common/ProcCtl.h"                             // ProcCtlReleaseAll
#include "ingame/map/BgSe.h"                            // GetLayoutSeWrk, CBgSe
#include "ingame/map/MapSky.h"                          // GetMapSky / GetMapSkyData / MapSkyInit
#include "ingame/map/FurnSave.h"                        // FurnSaveFree
#include "ingame/map/MapDraw.h"                         // MapDrawRelease
#include "ingame/map/MapObjWrk.h"                       // MapObjReleaseAll
#include "ingame/map/foot_se.h"                         // foot_seRelease
#include "ingame/story_menu_map.h"                      // StoryMapRelease
#include "ingame/menu/anim_2d.h"                        // LayoutAnimResetAll
#include "ingame/movie_room_menu/prg/movie_projecter.h" // MovieReqInit
#include "ingame/sound/snd_ctrl.h"                      // SndSystemInit / SndSystemEnd
#include "ingame/ingame.h"                              // SndVolume0MuteReset
#include "ingame/ingame_wrk.h"                          // title flags, PlayTimeCount
#include "outgame/option.h"                             // GetOptionWrk / OptionInit / OptionApplySound
#include "outgame/title_menu.h"                         // TitleMovieFlagClear / TitleMenuFlagSet
#include "system/b2d/BpCtl.h"                            // B2dSystemInit/End, BpCtlFrameReset
#include "system/eeiop/ee_iop.h"                        // ee_iopMain
#include "system/eeiop/fileload.h"                      // FileLoadCancelAll
#include "system/os/sys_heap.h"                         // InitCppAndSystemHeap
#include "system/os/system.h"                           // GetSysWrk, SendDMAMain, SetVBlankWaitNum
#include "system/eeiop/file_cache.h"                    // file_cache.ReleaseAll
#include "system/pad/pad.h"                             // PadFrameUpdate / PadLockReset
#include "system/playpss/playpss.h"                     // playPssInit

#include "reipan/debug/reipan_assists.h"                // PC: ReiPan_AssistsFrame
#include "reipan/debug/reipan_events.h"                 // PC: ReiPan_EventsFrame
#include "reipan/debug/reipan_warp.h"                   // PC: ReiPan_WarpFrame
#include "reipan/rendering/reipan_renderer.h"           // PC: ReiPan_RendererBeginFrame

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@00198e28

// ──────────────────────────────────────────────────────────────────────
// Game session
// ──────────────────────────────────────────────────────────────────────

// One phase-engine step: build the active node stack and run it.
// FF3 0x001d2610; FF2 GPhaseSysMain's role.

void GameSystemFrame(void)
{
    // PC: the Debug menu's warp / snapshot and event browser work and the
    // Assists menu's unlocks run between two frames.
    ReiPan_WarpFrame();
    ReiPan_EventsFrame();
    ReiPan_AssistsFrame();
    GetGPhaseCtrl()->Main();
}

// ──────────────────────────────────────────────────────────────────────
// Start a game session: link the phase tree, enter phase 0 (super), then put
// every session-wide subsystem in its initial state (sound, movie, language,
// options, layouts, fade layer, title flags, sky). Runs at boot and after
// each soft reset.
// FF3 0x001d2640; FF2 InitGPhaseSys + parts of init_super.

void GameSystemStart(void)
{
    SYS_WRK        *sys;
    capCtl *layer;

    GetGPhaseTree()->Init(&gphase_tree_def);
    GetGPhaseCtrl()->Init();
    GetGPhaseCtrl()->SetNext(0, 0);   // phase 0: super
    GetGPhaseCtrl()->Change();

    SndSystemInit();
    playPssInit();

    sys = GetSysWrk();
    sys->language = GetSystemLanguage();

    OptionInit(GetOptionWrk());
    B2dSystemInit();

    layer = GetCmnFadeLayer();
    layer->Reset();
    layer->Init();

    gra3dMonotoneDrawEnable(0);
    gra3dSetDrawParam(0, 1.3f);                 // sdata 0x370ac4

    TitleStartFlagClear();
    TitleMovieFlagClear();
    TitleMenuFlagSet();
    MovieResetPlaying();
    SetVBlankWaitNum(2);
    MovieReqInit();
    TitleMenuItemOpenInit();
    OptionApplySound();
    SndVolume0MuteReset();

    GetMapSkyData()->Init();
    GetMapSky()->Init();

    PadLockReset(-1);                           // both pads
}

// ──────────────────────────────────────────────────────────────────────
// End a game session (soft reset): cancel loads, release every session-wide
// subsystem in turn, terminate the phase engine, then rebuild the C++ and
// system heaps so the next GameSystemStart begins from empty memory.
// FF3 0x001d2810; no FF2 counterpart (FF2 reset through a phase).

void GameSystemEnd(void)
{
    FileLoadCancelAll();
    ProcCtlReleaseAll();
    EndMovie();
    GetMapSkyData()->Release();
    GetMapSky()->Reset();
    GetEneSpeObjTbl()->Release();
    GetLayoutSeWrk()->StopAll();
    foot_seRelease();
    ev_seRelease();
    SndSystemEnd();
    StoryMapRelease();
    GetCmnFadeLayer()->Release();
    LayoutAnimResetAll();
    B2dSystemEnd();
    FurnSaveFree();
    MapDrawRelease();
    MapObjReleaseAll();
    MapDrawRelease();                           // FF3 calls it twice
    file_cache.ReleaseAll();

    GetGPhaseCtrl()->Term();
    GetGPhaseTree()->Term();
    InitCppAndSystemHeap();
}

// ──────────────────────────────────────────────────────────────────────
// Phase 0: super
// ──────────────────────────────────────────────────────────────────────

// Nothing to set up: FF2's init_super bring-up is in InitMainSystem and
// GameSystemStart.
// FF3 0x001d2948; FF2 init_super.

int init_super(void)
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Start of every frame: pads, IOP replies, the play clock, default GS state.
// FF3 0x001d2950; FF2 pre_super (FF2 reset draw env / scissor and read pads).

void pre_super(void)
{
    // PC: the renderer's frame starts here, as in MioPan (SendDMAMain ends it).
    ReiPan_RendererBeginFrame();

    PadFrameUpdate();
    ee_iopMain();
    PlayTimeCount();
    SetDefaultGsRegs();
}

// ──────────────────────────────────────────────────────────────────────
// super is current only right after GameSystemStart: hand over to phase 1
// (Boot_PadCheck).
// FF3 0x001d2980; no FF2 body (FF2 set its first phase in InitGPhaseSys).

int one_super(void)
{
    GetGPhaseCtrl()->SetNext(1, 0);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// End of every frame: send the frame (SendDMAMain), then reset the Bp
// per-frame word.
// FF3 0x001d29c0; FF2 after_super (FF2 also ran ee_iopMain and the soft
// reset check here; FF3 moved them to pre_super and main()).

void after_super(void)
{
    SendDMAMain();
    BpCtlFrameReset();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001d29e0; FF2 end_super.

int end_super(void)
{
    return 1;
}
