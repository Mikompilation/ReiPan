/* ==========================================================================
 *  outgame/pad_check.c
 *
 *  Phase 1, Boot_PadCheck (see pad_check.h). FF3 0x225a58..0x225dc8. FF2's
 *  version waited for a DUALSHOCK2 and drew its own window; FF3 asks for
 *  pressure mode and shows a layout message instead.
 * ======================================================================== */
#include "outgame/pad_check.h"

#include "common/cmn_fade_layer.h"
#include "graphics/graph2d/font.h"      // FontTexLoadReq, FontTexSendVram
#include "graphics/graph2d/message.h"   // MsgInit, LangDataLoadReq
#include "ingame/menu/anim_2d.h"
#include "ingame/sound/snd_ctrl.h"      // SndSeBankLoad
#include "main/gphase.h"
#include "system/b2d/BpCtl.h"           // BpCtlMain
#include "system/pad/pad.h"
#include "system/os/eecdvd.h"         // LoadReq / LoadReqGetAddr / IsLoadEnd / IsLoadEndAll

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────

// unassigned@001ef150 (pad driver)
int PadIsStable(int pad);               // libpad state 6 (stable) or 2
int PadIsPressMode(int pad);            // scePadInfoPressMode(...) == 1

// ──────────────────────────────────────────────────────────────────────

static int pad_check_msg;               // sdata 0x37377c (gp+0x28c): message anim slot, -1: none

// Layout animation 0xc with message (0x84, 4). FF3 0x00225a58.
int PadErrMsgOpen(void)
{
    int slot = LayoutAnimStart(0xc, 0);

    LayoutMsgSet(0x84, 4);
    LayoutMsgSetType(2);
    LayoutMsgSetArg(2);
    SndSePlay(0x1a9b0002, 100, 0x1000, 0, 0, 0);
    return slot;
}

// FF3 0x00225ac8.
int PadErrMsgIsClosed(int slot)
{
    if (slot < 0)
        return 1;

    GetCmnFadeLayer()->Show(10);
    if (LayoutAnimIsStateChanged(slot) && LayoutAnimGetState(slot) == 4)
        return 1;
    return 0;
}

// FF3 0x00225b30.
void PadErrMsgRelease(int slot)
{
    (void)slot;
}

// Steps: 0 starts the boot data and fade layer loads; 1 waits for the system
// SE bank, the loads and the layer, then sets up messages and layout
// animations; 2 checks pad 0 and opens the message unless it is connected
// (_1a4) and enters pressure mode.
// FF3 0x00225b38.
int init_Boot_PadCheck(void)
{
    switch (GetGPhaseCtrl()->leaf->state_cnt)
    {
    case 0:
        pad_check_msg = -1;
        FontTexLoadReq();
        LangDataLoadReq();
        GetCmnFadeLayer()->Load();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
        return 0;

    case 1:
        if (SndSeBankLoad(-1) != 0)
            return 0;
        BpCtlMain();
        if (!IsLoadEndAll())
            return 0;
        if (!CmnFadeLayerIsReady())
            return 0;
        MsgInit();
        Anim2dInit();
        GetGPhaseCtrl()->leaf->state_cnt = 2;
        return 0;

    case 2:
        if (pad_wrk[0].connected != 0)
        {
            if (!PadIsStable(0))
                return 0;
            if (PadIsPressMode(0))
                return 1;
        }
        pad_check_msg = PadErrMsgOpen();
        GetCmnFadeLayer()->SetItemHandle(nullptr);
        GetCmnFadeLayer()->base_alpha = 0x80;
        return 1;

    default:
        return 0;
    }
}

// FF3 0x00225cc8.
int end_Boot_PadCheck(void)
{
    PadErrMsgRelease(pad_check_msg);
    LayoutAnimResetAll();
    GetCmnFadeLayer()->Term();
    GetCmnFadeLayer()->Release();
    return 1;
}

// Runs the message until it closes, then moves on to phase 2 (LangSel_Main).
// FF3 0x00225d18.
int one_Boot_PadCheck(void)
{
    if (pad_check_msg < 0)
    {
        GetGPhaseCtrl()->SetNext(2, 0);
        return 1;
    }

    FontTexSendVram();
    GetCmnFadeLayer()->Update();
    LayoutAnimMain();
    BpCtlMain();
    if (PadErrMsgIsClosed(pad_check_msg))
        pad_check_msg = -1;
    return 0;
}
