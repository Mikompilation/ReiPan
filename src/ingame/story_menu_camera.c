/* ==========================================================================
 *  ingame/story_menu_camera.c
 *
 *  Phase 64 StoryMenu_Camera; see story_menu_camera.h.
 *  FF3 0x208140..0x208370.
 *
 *  init makes the camera menu and a sequencer of five steps: load, wait
 *  until ready and open, one frame, run until a page ends the menu and
 *  close, wait until closed and go back to StoryMenu_Top. one runs the
 *  sequencer and draws. The steps work on the two statics, not on the
 *  sequencer's owner.
 * ======================================================================== */
#include "ingame/story_menu_camera.h"

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::Update / SetItemHandle
#include "common/cmn_util.h"            // STEP_SEQ, STEP_*
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/menu/anim_2d.h"        // LayoutAnimMain
#include "ingame/menu/menuCamMain.h"    // CmenuCamMain
#include "ingame/photo/n_plyr_camera.h" // PlyrCameraSetEc
#include "system/b2d/BpCtl.h"           // BpCtlMain

// sdata 0x3711c8 / 0x3711cc
static CmenuCamMain *story_cam_menu;
static STEP_SEQ     *story_cam_seq;

// ──────────────────────────────────────────────────────────────────────
// The steps.

// FF3 0x00208140

static int StoryMenuCameraStepLoad(STEP_SEQ *seq)
{
    story_cam_menu->Load();
    return STEP_NEXT;
}

// FF3 0x00208168

static int StoryMenuCameraStepOpen(STEP_SEQ *seq)
{
    int ready = story_cam_menu->IsReady();

    if (ready != 0)
    {
        story_cam_menu->Open();
    }
    return ready != 0 ? STEP_NEXT : STEP_CONT;
}

// FF3 0x002081a0

static int StoryMenuCameraStepWait(STEP_SEQ *seq)
{
    return STEP_NEXT;
}

// FF3 0x002081a8

static int StoryMenuCameraStepRun(STEP_SEQ *seq)
{
    int end = story_cam_menu->Run();

    if (end != 0)
    {
        story_cam_menu->Close();
    }
    return end != 0 ? STEP_NEXT : STEP_CONT;
}

// FF3 0x002081e0

static int StoryMenuCameraStepClose(STEP_SEQ *seq)
{
    int closed;

    story_cam_menu->UpdateHud();
    closed = story_cam_menu->IsClosed();
    if (closed != 0)
    {
        GetCmnFadeLayer()->SetItemHandle(nullptr);
        GPhaseCtrlRequest(0x39, 0);     // 57 StoryMenu_Top
    }
    return closed != 0 ? STEP_NEXT : STEP_CONT;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00208238
// callcheck: operator_new, MenuCamMainCtor, StepSeqInit (new CmenuCamMain, new STEP_SEQ)

int init_StoryMenu_Camera(void)
{
    static const STEP_FUNC steps[] =
    {
        StoryMenuCameraStepLoad, StoryMenuCameraStepOpen, StoryMenuCameraStepWait,
        StoryMenuCameraStepRun, StoryMenuCameraStepClose,
        nullptr,
    };

    story_cam_menu = new CmenuCamMain;
    story_cam_seq = new STEP_SEQ;
    story_cam_seq->Add(steps);
    return 1;
}

// FF3 0x002082d0
// callcheck: CmnFadeLayerUpdate (capCtl::Update)

int one_StoryMenu_Camera(void)
{
    int ret;

    LayoutAnimMain();
    ret = story_cam_seq->Run();
    GetCmnFadeLayer()->Update();
    BpCtlMain();
    story_cam_menu->Draw();
    return ret;
}

// The statics keep their pointers, as in FF3.
// FF3 0x00208320
// callcheck: operator_delete (delete story_cam_menu through its virtual destructor, delete story_cam_seq)

int end_StoryMenu_Camera(void)
{
    if (story_cam_menu != nullptr)
    {
        delete story_cam_menu;
    }
    delete story_cam_seq;
    PlyrCameraSetEc(0);
    PlyrCameraSetEc(1);
    return 1;
}
