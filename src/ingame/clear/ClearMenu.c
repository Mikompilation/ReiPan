/* ==========================================================================
 *  ingame/clear/ClearMenu.c
 *
 *  Phases 43 / 70, the menu after the game clear (see ClearMenu.h).
 *  FF3-only, FF3 0x14c7c8..0x14cd30.
 * ======================================================================== */
#include "ingame/clear/ClearMenu.h"

#include "album/prg/album.h"            // AlbumSetIngame
#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl::Update
#include "graphics/graph2d/fade.h"      // FadeInReq, FadeMain
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/menu/anim_2d.h"        // LayoutAnimMain
#include "ingame/sound/snd_ctrl.h"      // SndBgmStopAll, SndAllBufStop
#include "main/gphase.h"                // GetGPhaseCtrl, GetGPhaseTree
#include "main/gphase_tree.h"           // GPHASE_TREE::Reset
#include "outgame/outgame_bg.h"         // OutgameBg*
#include "outgame/sel_menu.h"           // SelMenu*, SEL_MENU_ROW
#include "system/b2d/BpCtl.h"           // BpCtlMain
#include "system/eeiop/cd_file_dat.h"   // OUTGAME_BG_PSS
#include "system/eeiop/stream_auto.h"   // StreamAutoAllStop, StreamAutoSetExclusiveMode

#define CLEAR_MENU_PHASE_ALBUM  8       // phase 8 (the album)
#define CLEAR_MENU_PHASE_TITLE  5       // phase 5 Title_Mode
#define CLEAR_MENU_PHASE_SAVE   0x45    // phase 69 Story_SaveGame_Mc

// data 0x350aa0: save, album, title {item, message kind, text, help, result}.
static const SEL_MENU_ROW clear_menu_list[] = {
    { 0x41b, 0x69, 0, 4, CLEAR_MENU_PHASE_SAVE },
    { 0x3f0, 0x6c, 1, 4, CLEAR_MENU_PHASE_ALBUM },
    { 0x3f1, 0x6c, 2, 5, CLEAR_MENU_PHASE_TITLE },
    { -1,    -1,  -1, 0, 0 },
};

static int clear_menu_step;             // sbss 0x372eb0 (gp-0x640)
static int clear_menu_ret;              // sbss 0x372eb4: the phase it was entered from

// Fades in, starts the backdrop movie and waits for it; the steps count
// in the node's state_cnt (0, 3, 4, 5, 6).
// FF3 0x0014c7e0

int init_Story_ClearMenu(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        FadeInReq(0, 0, 0, 0);
        GetGPhaseCtrl()->leaf->state_cnt = 3;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt == 3)
    {
        OutgameBgStart(0, OUTGAME_BG_PSS, -1);
        GetGPhaseCtrl()->leaf->state_cnt = 4;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt == 4)
    {
        if (OutgameBgIsLoaded() == 0)
            return 0;
        if (OutgameBgLoadWait() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 5;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt == 5)
    {
        if (OutgameBgIsReady() == 0)
            return 0;
        GetGPhaseCtrl()->leaf->state_cnt = 6;
    }
    clear_menu_ret  = GetGPhaseCtrl()->from;
    clear_menu_step = 0;
    SelMenuSetList(clear_menu_list);
    return 1;
}

// FF3 0x0014c9b8

int end_Story_ClearMenu(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        OutgameBgStop();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 1;
    if (OutgameBgIsStopped() == 0)
        return 0;
    OutgameBgRelease();
    GetGPhaseCtrl()->leaf->state_cnt = 2;
    return 1;
}

// Steps: 0 init, 1 load, 2 in, 3 select, 4 the title confirm, 5 out and
// the request; 6 / 7 (back from the save / the album) reset the phase tree
// and start over.
// FF3 0x0014ca90

int one_Story_ClearMenu(void)
{
    int res;
    int next;

    LayoutAnimMain();
    if (clear_menu_step == 6)
    {
        GetGPhaseTree()->Reset();
        clear_menu_step = 0;
    }
    if (clear_menu_step == 7)
    {
        GetGPhaseTree()->Reset();
        clear_menu_step = 0;
    }
    if (clear_menu_step == 0)
    {
        SelMenuInit();
        clear_menu_step = 1;
    }
    if (clear_menu_step == 1 && SelMenuLoad() != 0)
        clear_menu_step = 2;
    if (clear_menu_step == 2 && SelMenuIsInEnd() != 0)
        clear_menu_step = 3;
    if (clear_menu_step == 3 && SelMenuSelect() != 0)
    {
        if (SelMenuGetResult() == CLEAR_MENU_PHASE_TITLE)
        {
            clear_menu_step = 4;
            return 0;
        }
        clear_menu_step = 5;
    }
    if (clear_menu_step == 4)
    {
        res = SelMenuConfirm();
        if (res < 0)
            clear_menu_step = 3;
        if (res > 0)
        {
            StreamAutoAllStop();
            SndBgmStopAll();
            SndAllBufStop();
            StreamAutoSetExclusiveMode(0, 1);
            clear_menu_step = 5;
        }
    }
    if (clear_menu_step != 5)
        return 0;

    if (SelMenuIsOut() == 0)
        return 0;
    SelMenuRelease();
    res = SelMenuGetResult();
    if (res == CLEAR_MENU_PHASE_ALBUM)
    {
        GPhaseCtrlRequest(CLEAR_MENU_PHASE_ALBUM, 1);
        AlbumSetIngame(1, 0);
        clear_menu_step = 7;
        return 1;
    }
    if (res < 9)
    {
        next = res == CLEAR_MENU_PHASE_TITLE ? CLEAR_MENU_PHASE_TITLE : clear_menu_ret;
    }
    else
    {
        if (res == CLEAR_MENU_PHASE_SAVE)
        {
            GPhaseCtrlRequest(CLEAR_MENU_PHASE_SAVE, 1);
            clear_menu_step = 6;
            return 1;
        }
        next = clear_menu_ret;
    }
    GPhaseCtrlRequest((short)next, 0);
    return 1;
}

// FF3 0x0014ccb0

void pre_Story_ClearMenu(void)
{
    OutgameBgDraw();
}

// FF3 0x0014ccc8

void after_Story_ClearMenu(void)
{
    GetCmnFadeLayer()->Update();
    BpCtlMain();
    FadeMain();
}

// Phase 70 does nothing.
// FF3 0x0014c7c8

int init_Story_ClearMenu_Main(void)
{
    return 1;
}

// FF3 0x0014c7d8

int one_Story_ClearMenu_Main(void)
{
    return 1;
}

// FF3 0x0014c7d0

int end_Story_ClearMenu_Main(void)
{
    return 1;
}
