/* ==========================================================================
 *  common/cmn_fade_layer.c
 *
 *  The common fade layer (see cmn_fade_layer.h). FF3-only; FF3
 *  0x13dba8..0x13e6a8, the file ending with its static initializer
 *  (0x13e5d0) and the compiler's tail after it, plus the
 *  CWrkVariable<int,0,128> template instances it uses
 *  (0x2ad3e0, 0x2ad438) from the shared template area.
 * ======================================================================== */
#include "common/cmn_fade_layer.h"

#include "graphics/gs_reg.h"                            // SetGsRegCached
#include "ingame/ingame_wrk.h"                          // ingame_wrk.plyr_wrk
#include "ingame/story_menu_map.h"                      // IsMapEnable
#include "ingame/pause/prg/pause.h"                     // IsPauseEnable
#include "ingame/plyr/player.h"                         // PLYR_WRK
#include "main/gphase.h"                                // GetGPhaseCtrl
#include "system/b2d/B2dSpr.h"                          // B2dSprGetPartData
#include "system/b2d/BpCtl.h"                           // the Bp layout table
#include "system/os/system.h"                           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad.h"                             // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_file_list.h"                // MemoMenuIsEnable
#include "ingame/story_menu.h"                         // IsLockTblOn, IsMenuEnable

static int CmnFadeLayerIsIdleOk(void);

// Animation labels of the layout, data 0x2f2ff8 (strings 0x36bdf0..): the
// label numbers callers pass index this table.
static const char *const cmn_fade_layer_label[25] =
{
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "T", "U", "V", "W", "X", "Y", "Z",
};

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013dbf0 (CmnFadeLayerCtor; 0x0013dba8 CmnFadeLayerCtor2 is the
// identical base-object copy): fade value and idle interpolator cleared, then
// the idle state and the main state reset.
// callcheck: LerpFClear (idle_fade's constructor, implicit)

capCtl::capCtl()
{
    fade.val   = 0;
    fade.delta = 0;
    Reset();
    Init();
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ad408 (no Ghidra function; the vtable's complete destructor:
// only restores the vptr); the deleting variant 0x002ad418 adds operator
// delete.

capCtl::~capCtl()
{
}

// ──────────────────────────────────────────────────────────────────────
// Idle label off: interpolator at 0, nothing to fade in.
// FF3 0x0013dc38.

void capCtl::Reset()
{
    idle_fade.Set(0.0f, 0.0f, 0.0f);
    idle_fading = 0;
    idle_off    = 1;
    idle_label  = -1;
}

// ──────────────────────────────────────────────────────────────────────
// No label, no item, alpha 0, default speed. flags are left as they are.
// FF3 0x0013dc80.

void capCtl::Init()
{
    item       = nullptr;
    next_label = -1;
    speed      = 0x1e;
    alpha      = 0;
    base_alpha = 0;
    label      = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the layout is loaded; the first time it is, clears the Bp entry's
// word +0x20 (meaning unknown).
// FF3 0x0013dca8.

int CmnFadeLayerIsReady(void)
{
    int ready = BpCtlIsDataReady(BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE));

    if (ready == 1)
        BpCtlSetField20(BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE), 0);
    return ready == 1;
}

// ──────────────────────────────────────────────────────────────────────
// Starts loading the layout (last free Bp slot, buffer allocated by Bp).
// FF3 0x0013dd38.

void capCtl::Load()
{
    Reset();
    item = nullptr;
    BpCtlLoadReqLast(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013dd88.

void capCtl::Release()
{
    BpCtlRelease(BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE));
    Reset();
    item = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Shows 'label' at once: the fade jumps to 0x80 (delta 0x100, one Work).
// FF3 0x0013dde0.

void capCtl::Show(int label)
{
    if (CmnFadeLayerIsReady())
    {
        this->label      = label;
        fade.val   = 0;
        fade.delta = 0x100;
        fade.Work();
        flags      = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013de38.

void capCtl::Term()
{
    Init();
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: runs the fade (out: until 0, then the queued label if a switch
// asked for it; in: until 0x80) and sets the drawn alpha from the base (the
// item's part alpha when an item is set) scaled by the fade.
// FF3 0x0013de50. (FF3 inlines GetState in the fade-out branch.)

void capCtl::Update()
{
    int base;

    if (CmnFadeLayerIsReady() == 0)
        return;

    if (item == nullptr)
        base = base_alpha;
    else
        base = B2dSprGetPartData(item, 10);

    if (this->flags & CMN_FADE_FLG_OUT)
    {
        fade.delta = -speed;
        if (fade.GetState() == CWRK_VAR_AT_MIN)
        {
            u_int flags = this->flags;

            label = -1;
            this->flags = flags & ~CMN_FADE_FLG_OUT;
            if ((flags & CMN_FADE_FLG_IN) && next_label != -1)
            {
                label      = next_label;
                next_label = -1;
            }
        }
    }
    else if (this->flags & CMN_FADE_FLG_IN)
    {
        fade.delta = speed;
        if (fade.GetState() == CWRK_VAR_AT_MAX)
            this->flags &= ~CMN_FADE_FLG_IN;
    }

    fade.Work();
    alpha = (base * fade.val) / 0x80;
}

// ──────────────────────────────────────────────────────────────────────
// With no main label up: fades the idle label in (to 127 over 10 frames)
// while the player can act, drops it to 0 otherwise, and draws it.
// FF3 0x0013df90.

void capCtl::DrawIdle()
{
    if (idle_off != 0 && idle_fade.cur == 0.0f)
        return;

    if (CmnFadeLayerIsIdleOk() == 0)
    {
        if (idle_fading)
        {
            idle_fade.Set(0.0f, 0.0f, 0.0f);
            idle_fading = 0;
        }
        return;
    }

    if (idle_off == 0 && idle_fading == 0)
    {
        idle_fade.Start(127.0f, 10.0f);   // data 0x36beb4, 0x36beb8
        idle_fading = 1;
    }
    idle_fade.Update();

    int alpha = (int)idle_fade.cur;
    int label = idle_label;

    if (alpha > 0)
    {
        int handle = BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE);

        B2dCtlSetAlpha(handle, alpha);
        BpPlayLabel(BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE), cmn_fade_layer_label[label]);
        BpSetDrawGsRegs();
        SetGsRegCached(0x100000065, 0x14, 1);           // reg 0x14
        BpCtlDrawOne(handle);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Called by BpCtlMain: draws the label at the layer's alpha, or the idle
// label when no label is up (or the alpha went negative).
// FF3 0x0013e120.

void capCtl::Draw()
{
    int label;

    if (CmnFadeLayerIsReady() == 0)
        return;

    label = this->label;
    if (label == -1)
    {
        label = idle_label;
    }
    else
    {
        int alpha = this->alpha;

        if (alpha >= 0)
        {
            if (alpha < 1)
                return;

            int handle = BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE);

            B2dCtlSetAlpha(handle, alpha);
            BpPlayLabel(BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + CMN_FADE_LAYER_FILE), cmn_fade_layer_label[label]);
            BpSetDrawGsRegs();
            SetGsRegCached(0x100000065, 0x14, 1);       // reg 0x14
            BpCtlDrawOne(handle);
            return;
        }
        label = idle_label;
    }

    if (label != -1)
        DrawIdle();
}

// ──────────────────────────────────────────────────────────────────────
// Fades the current label out at 'speed' and 'label' in after it, unless
// 'label' already shows (and is not fading out).
// FF3 0x0013e268.

void capCtl::Switch(int label, int speed)
{
    if (label != this->label || (flags & CMN_FADE_FLG_OUT))
    {
        this->speed      = speed;
        next_label = label;
        flags      = CMN_FADE_FLG_OUT | CMN_FADE_FLG_IN;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e2a0.

void capCtl::Hide(int speed)
{
    this->speed = speed;
    flags = CMN_FADE_FLG_OUT;
}

// ──────────────────────────────────────────────────────────────────────
// Shows 'label' from alpha 0, fading in at 'speed'.
// FF3 0x0013e2b0.

void capCtl::ShowFadeIn(int label, int speed)
{
    this->label      = label;
    flags      = CMN_FADE_FLG_IN;
    this->speed      = speed;
    fade.delta = 0;
    fade.val   = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e2d0.

void capCtl::SetItemHandle(B2D_ITEM *item)
{
    if (CmnFadeLayerIsReady())
        this->item = item;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e308.

void capCtl::SetItem(int handle, int id)
{
    if (CmnFadeLayerIsReady())
        item = B2dCtlFindItem(handle, id);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e360 (no callers).

void capCtl::SetItemByName(int handle, const char *name)
{
    if (CmnFadeLayerIsReady())
        item = B2dCtlFindItemByName(handle, name);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e3b8.

void capCtl::IdleShow(int label)
{
    idle_label = label;
    idle_fade.Start(127.0f, 10.0f);       // data 0x36bebc, 0x36bec0
    idle_off    = 0;
    idle_fading = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e408.

void capCtl::IdleHide()
{
    idle_fade.Start(0.0f, 10.0f);         // data 0x36bec4
    idle_fading = 1;
    idle_off    = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The idle label may show: in phase 0x20 (current and deepest node), when no
// pause/menu/map/finder button was pressed this frame (each only while that
// screen may open), the current player is free (+0x144 clear, none of flag
// bits 0x1c000000/0x800000/0x200000) and lock entries 2 and 3 are off.
// FF3 0x0013e450.

static int CmnFadeLayerIsIdleOk(void)
{
    GPHASE_CTRL *ctrl = GetGPhaseCtrl();
    PLYR_WRK    *plyr;
    u_long       flags;

    if (ctrl->leaf->id != 0x20 || ctrl->now != 0x20)
        return 0;

    if (IsPauseEnable() && CheckPadTrgPlaySe(pad_key[PAD_KEY_PAUSE], -1, 0))
        return 0;
    if (IsMenuEnable() && CheckPadTrgPlaySe(pad_key[PAD_KEY_MENU], -1, 0))
        return 0;
    if (IsMapEnable() && CheckPadTrgPlaySe(pad_key[PAD_KEY_MAP], -1, 0))
        return 0;
    if (MemoMenuIsEnable(0) && CheckPadTrgPlaySe(pad_key[PAD_KEY_MEMO], -1, 0))
        return 0;

    plyr  = ingame_wrk.plyr_wrk;
    flags = plyr->flags;
    if (plyr->mode != 0 || (flags & 0x1c000000) != 0 || (flags & 0x800000) != 0 || (flags & 0x200000) != 0)
        return 0;

    if (IsLockTblOn(3))
        return 0;
    return IsLockTblOn(2) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// The layer object, bss 0x391080. FF3 0x0013e5d0 (the file's
// __static_initialization_and_destruction_0) constructs it at start-up and
// destroys it at exit; the C++ static does both here. The rest of FF3's
// file is compiler output too: FF3 0x0013e648 (the atexit thunk of the
// GPhaseCtrl singleton, GPhaseCtrlTerm(0x41e468)), FF3 0x0013e668
// (_GLOBAL__I_, the .ctors entry at 0x2e9118) and FF3 0x0013e688
// (_GLOBAL__D_).

static capCtl cmn_fade_layer;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e5c0.

capCtl *GetCmnFadeLayer(void)
{
    return &cmn_fade_layer;
}

// ──────────────────────────────────────────────────────────────────────
// CWrkVariable<int,0,128> instances (FF3's shared template area).
// ──────────────────────────────────────────────────────────────────────

// val += delta, clamped to 0..0x80.
// FF3 0x002ad3e0.

template <>
void CWrkVariable<int, 0, 128>::Work()
{
    int val = this->val + delta;

    if (val < 0)
        val = 0;
    if (val > 0x80)
        val = 0x80;
    this->val = val;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ad438.

template <>
int CWrkVariable<int, 0, 128>::GetState()
{
    if (delta < 1)
        return val != 0 ? CWRK_VAR_FALLING : CWRK_VAR_AT_MIN;
    return val != 0x80 ? CWRK_VAR_RISING : CWRK_VAR_AT_MAX;
}
