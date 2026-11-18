/* ==========================================================================
 *  common/cmn_fade_layer.h
 *
 *  The common fade layer (FF3-only, FF3 0x13dba8..0x13e648 plus the
 *  CWrkVariable<int,0,128> instances 0x2ad3e0/0x2ad438): one global object
 *  (GetCmnFadeLayer, bss 0x391080) that draws an animation label of the
 *  per-language Bp layout 0x11bd + GetLanguage() over menus and scenes, faded
 *  in and out through a 0..0x80 alpha. Callers pick the label (capCtl::Show
 *  / Switch / ShowFadeIn / Hide), optionally tie its alpha to a B2d item
 *  (capCtl::SetItem*), and call capCtl::Update each frame; BpCtlMain
 *  draws it (capCtl::Draw). With no label up, an "idle" label fades in
 *  while the player can act in phase 0x20 (capCtl::IdleShow/Hide).
 *  Labels are the one-letter animation names "A".."Z" (no "S").
 * ======================================================================== */
#ifndef _COMMON_CMN_FADE_LAYER_H
#define _COMMON_CMN_FADE_LAYER_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/map/MarkEvent.h"       // LERP_F
#include "system/b2d/B2dCtl.h"          // B2D_ITEM

#define CMN_FADE_LAYER_FILE 0x11bd      // Bp layout file, + GetLanguage()

// A value that moves by 'delta' per Work() and stays in [MIN, MAX] (FF2's
// common/variable.h template CWrkVariable<T,Min,Max>, 2 * sizeof(T) bytes).
// FF3 emitted the methods out of line per instance (linkonce): each one used
// is an explicit specialization below or in ingame/photo/fndr_parts.h, with
// its own address.
template <typename T, int MIN, int MAX>
struct CWrkVariable
{
    void Work();                        // val += delta, clamped to [MIN, MAX]
    int  LoopWork();                    // val += delta, past a bound restarts from the other one; 1 then
    int  GetState();                    // CWRK_VAR_STATE

    /* 0x00 */ T val;
    /* 0x04 */ T delta;
};
typedef CWrkVariable<int, 0, 128> CWRK_VAR_INT_0_128;
static_assert(sizeof(CWrkVariable<int, 0, 128>) == 0x8, "CWrkVariable<int,0,128> is 8 bytes");
static_assert(offsetof(CWRK_VAR_INT_0_128, delta) == 0x4, "CWrkVariable.delta");

// CWrkVariable::GetState
enum CWRK_VAR_STATE
{
    CWRK_VAR_AT_MAX     = 0,            // delta > 0, val == MAX
    CWRK_VAR_AT_MIN     = 1,            // delta <= 0, val == MIN
    CWRK_VAR_RISING     = 2,
    CWRK_VAR_FALLING    = 3,
};

template <> void CWrkVariable<int, 0, 128>::Work();         // FF3 0x002ad3e0 (CWrkVariable_int_0_128_Work)
template <> int  CWrkVariable<int, 0, 128>::GetState();     // FF3 0x002ad438 (CWrkVariable_int_0_128_GetState)

// capCtl.flags
#define CMN_FADE_FLG_OUT    0x1         // fading out (Hide); then the queued label, if FLG_IN
#define CMN_FADE_FLG_IN     0x2         // fading in

// FF3 RTTI name capCtl (caption control: it draws caption.phf); Ghidra names
// its methods CmnFadeLayer* (CmnFadeLayerShow is capCtl::Show).
// The layer: 0x48 bytes on the EE, vtable 0x341560 holding only the two
// destructors (complete 0x2ad408, deleting 0x2ad418).
// layout: host -- the vptr (EE 4 bytes) and item (an EE pointer at +0x14) are
// 8 bytes here; EE offsets in the comments.
class capCtl
{
public:
    capCtl();                   // FF3 0x0013dbf0 (CmnFadeLayerCtor; copy 0x0013dba8)
    virtual ~capCtl();          // FF3 0x002ad408 (deleting: 0x002ad418)

    void Reset();                           // FF3 0x0013dc38: idle label off
    void Init();                            // FF3 0x0013dc80: no label, no item, speed 0x1e
    void Load();                            // FF3 0x0013dd38: starts loading the layout
    void Release();                         // FF3 0x0013dd88
    void Show(int label);                   // FF3 0x0013dde0
    void Term();                            // FF3 0x0013de38: = Init
    void Update();                          // FF3 0x0013de50: per frame
    void Draw();                            // FF3 0x0013e120: from BpCtlMain
    void Switch(int label, int speed);      // FF3 0x0013e268
    void Hide(int speed);                   // FF3 0x0013e2a0
    void ShowFadeIn(int label, int speed);  // FF3 0x0013e2b0
    void SetItemHandle(B2D_ITEM *item);     // FF3 0x0013e2d0
    void SetItem(int handle, int id);       // FF3 0x0013e308
    void SetItemByName(int handle, const char *name);  // FF3 0x0013e360
    void IdleShow(int label);               // FF3 0x0013e3b8
    void IdleHide();                        // FF3 0x0013e408

    /* 0x04 */ u_int        flags;          // CMN_FADE_FLG_*
    /* 0x08 */ int          alpha;          // drawn alpha: base * fade / 0x80 (capCtl::Update)
    /* 0x0c */ int          base_alpha;     // base when no item is set (phase 12 drives it)
    /* 0x10 */ int          speed;          // fade step per frame (0x1e after init)
    /* 0x14 */ B2D_ITEM    *item;           // when set, the base is its part 10 (alpha)
    /* 0x18 */ int          label;          // label shown, -1: none
    /* 0x1c */ int          next_label;     // label queued by capCtl::Switch, -1: none
    /* 0x20 */ CWRK_VAR_INT_0_128 fade;     // 0..0x80
    /* 0x28 */ int          idle_label;     // -1: none
    /* 0x2c */ LERP_F       idle_fade;      // idle label alpha 0..127 over 10 frames
    /* 0x40 */ int          idle_fading;    // idle_fade started and not reset
    /* 0x44 */ int          idle_off;       // 1: idle label hidden or hiding

private:
    void DrawIdle();                        // FF3 0x0013df90
};
static_assert(offsetof(capCtl, flags) == sizeof(void *) + 0x00, "capCtl.flags (EE 0x04)");
static_assert(offsetof(capCtl, alpha) == sizeof(void *) + 0x04, "capCtl.alpha (EE 0x08)");
static_assert(offsetof(capCtl, base_alpha) == sizeof(void *) + 0x08, "capCtl.base_alpha (EE 0x0c)");
static_assert(offsetof(capCtl, speed) == sizeof(void *) + 0x0c, "capCtl.speed (EE 0x10)");
static_assert(offsetof(capCtl, item) == sizeof(void *) + 0x10, "capCtl.item (EE 0x14)");
static_assert(offsetof(capCtl, label) == offsetof(capCtl, item) + sizeof(void *),
              "capCtl.label (EE 0x18)");
static_assert(offsetof(capCtl, next_label) == offsetof(capCtl, label) + 0x04,
              "capCtl.next_label (EE 0x1c)");
static_assert(offsetof(capCtl, fade) == offsetof(capCtl, label) + 0x08,
              "capCtl.fade (EE 0x20)");
static_assert(offsetof(capCtl, idle_label) == offsetof(capCtl, label) + 0x10,
              "capCtl.idle_label (EE 0x28)");
static_assert(offsetof(capCtl, idle_fade) == offsetof(capCtl, label) + 0x14,
              "capCtl.idle_fade (EE 0x2c)");
static_assert(offsetof(capCtl, idle_off) == offsetof(capCtl, label) + 0x2c,
              "capCtl.idle_off (EE 0x44)");

capCtl *GetCmnFadeLayer(void);

int  CmnFadeLayerIsReady(void);                     // the layout is loaded

#endif /* _COMMON_CMN_FADE_LAYER_H */
