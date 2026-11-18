/* ==========================================================================
 *  ingame/photo/finder_photo.c
 *
 *  The photo mode work (FF3-only, no FF2 counterpart): FF3 0x1b57d8..0x1b6f20
 *  (the object file ends with its static initializer pair 0x1b6ec8/0x1b6f00)
 *  plus its out-of-line inline instances 0x2b94b0..0x2b9610. See
 *  finder_photo.h for the work's layout and roles.
 * ======================================================================== */
#include "ingame/photo/finder_photo.h"

#include <string.h>                     // memset

#include "ingame/plyr/plyr_gauge.h"
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/effect/effect.h"     // SetEffects
#include "ingame/enemy/ene_main.h"   // ene_wrk
#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, DISP_SPRT, CopySprDToSpr, DispSprD
#include "graphics/graph2d/tim2.h"      // PK4SendVram
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/photo/finder_view.h"         // HudTexSend
#include "ingame/photo/finder_view_hud.h"
#include "ingame/photo/fndr_parts.h"
#include "ingame/plyr/player.h"
#include "ingame/sound/snd_ctrl.h"      // SndSeBank*, SndSePlay
#include "ingame/plyr/unit_ctl.h"        // GetDirTrg
#include "system/eeiop/cd_file_dat.h"   // SYS_FINDER_BD_BD, SYS_REAL_FINDER_BD_BD, TOUSYU_TEGATA_PK4
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq, IsLoadEnd
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPALMode
#include "system/pad/pad_ctrl.h"        // CheckPad*PlaySe, pad_key
#include "ingame/story_menu.h"  // IsLockTblOn, IsLockTblCnt
#include "ingame/item/prg/item.h"      // ItemGetCnt
#include "reipan/rendering/reipan_renderer.h" // PC: ReiPan_RendererDrawFinderMask

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────

// sdata 0x370480 (gp-0x3070): effect 5's value, 0x50 while the camera is
// down, falling by 4 a frame once it is up.
static s_char photo_eff5_val;

// data 0x3707f8: per film type (FILM_WRK.type), PhotoModeGetFilmPower.
static const u_char photo_film_power[5] = { 3, 10, 14, 18, 25 };

// ══════════════════════════════════════════════════════════════════════
// Init, main, draw (IngameInit / IngameMain / IngameDraw)
// ══════════════════════════════════════════════════════════════════════

static int  PhotoTimerCountDown(int *timer);
static void PhotoTegataLevelStep(s_char *level);

// ──────────────────────────────────────────────────────────────────────
// Every sub-work set up, the finder mode (and its SE bank) selected, the
// hand print and the fades cleared.
// FF3 0x001b57d8.

void PHOTO_MODE_WRK::Init()
{
    _4d0 = 0;
    ZoomReset();
    PhotoShakeReset();
    mode = PHOTO_MODE_FINDER;
    SetMode(PHOTO_MODE_FINDER, 1);
    EneMarkInit(ene_mark);
    PhotoWrk180Init(&w180);
    w1e8.Init();
    charger.charge.val = 0;
    PhotoChargerInit(&charger, 1);
    charger.se7_wait = 0;
    charger.se0_wait = 0;
    FilmInit(&film);
    FilamentInit(&filament);
    PhotoWrk274Init(&w274);
    PhotoWrk288Init(&w288);
    PlyrGaugeInit(&album_save_disp);
    lens.Init();
    tegata_ready     = 0;
    tegata_load_id   = -1;
    tegata_buf       = nullptr;
    tegata_level     = 0;
    tegata_step      = 0;
    view_fade.val    = 0;
    view_fade.delta  = 0;
    view_fade_frames = 0;
    HudFadeInit();
}

// ──────────────────────────────────────────────────────────────────────
// After a load: the current mode's SE bank again.
// FF3 0x001b58b0.

void PHOTO_MODE_WRK::LoadRestore()
{
    SetMode(GetMode(), 1);
}

// ──────────────────────────────────────────────────────────────────────
// Back to the finder mode without touching the SE banks (IngameNewGame).
// FF3 0x001b58e0.

void PHOTO_MODE_WRK::ClearFlag()
{
    mode = PHOTO_MODE_FINDER;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame: the mode's sub-works, the HUD fades, the view fade towards
// camera up / down, and effect 5 while the camera is up.
// FF3 0x001b58e8.

void PHOTO_MODE_WRK::Main()
{
    if (mode == PHOTO_MODE_FINDER)
        FinderMain();
    else
        RealMain();

    HudFadeMain();

    u_int frames = view_fade_frames;
    if (frames != 0)
    {
        int full = (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) ? 0x80 : -0x80;

        view_fade.delta = full / (int)frames;
        view_fade.Work();
    }

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
    {
        photo_eff5_val = 0x50;
    }
    else if (photo_eff5_val > 0)
    {
        photo_eff5_val -= 4;
        SetEffects(5, 1, (int)photo_eff5_val);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b59e0.

void PHOTO_MODE_WRK::HudFadeInit()
{
    for (int i = 0; i < 2; i++)
    {
        hud_fade[i].val   = 0;
        hud_fade[i].delta = 0;
        hud_hold[i]       = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// HUD fade 0 (HP gauge and HUD) rises and holds 0x8c frames whenever the
// camera is up, HP is low, the player is hit, or the pad_key[18] action is
// available and pressed; fade 1 (the +0x244 gauge) while its level is up.
// When a hold runs out the fade falls by 5 a frame.
// FF3 0x001b5a10.

void PHOTO_MODE_WRK::HudFadeMain()
{
    PLYR_WRK *plyr  = ingame_wrk.plyr_wrk;
    u_long    flags = plyr->flags;
    int       show  = 1;

    if ((flags & (PLYR_FLG_800000 | PLYR_FLG_000020)) == 0
        && (plyr->hp <= 0 || plyr->hp_max / 5 < plyr->hp)
        && (flags & PLYR_FLG_000100) == 0
        && (PlyrFlashAble() == 0 || CheckPadTrgPlaySe(pad_key[PAD_KEY_18], -1, 0) == 0)
        && (PlyrCamAble() == 0 || CheckPadTrgPlaySe(pad_key[PAD_KEY_18], -1, 0) == 0))
    {
        show = PlyrGetCurNo() == 1 && (ingame_wrk.plyr_wrk->flags & PLYR_FLG_100000) != 0;
    }

    if (show)
    {
        hud_fade[0].delta = 0x1e;
        if (hud_hold[0] == 0)
            hud_hold[0] = 0x8c;
    }

    if (filament.rate > 0.0f)
    {
        hud_fade[1].delta = 0x1e;
        if (hud_hold[1] == 0)
            hud_hold[1] = 0x8c;
    }

    for (int i = 0; i < 2; i++)
    {
        if (PhotoTimerCountDown(&hud_hold[i]))
            hud_fade[i].delta = -5;
        hud_fade[i].Work();
    }
}

// ──────────────────────────────────────────────────────────────────────
// Finder mode, per frame: every sub-work; with the camera up also the
// markers, the film (left / right pick it unless lock 0x15 is on), the lens
// parts and the zoom.
// FF3 0x001b5ba8.

void PHOTO_MODE_WRK::FinderMain()
{
    TegataMain();
    FilamentWork(&filament);
    HudHpWork(&hp);
    PlyrGaugeMain(&album_save_disp);
    lens.Work();
    if (ingame_wrk.plyr_wrk->_250 == 0)
        PhotoChargerWork(&charger, 0);
    w1e8.SeMain();

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
        return;

    EneMarkWork(ene_mark);
    PhotoWrk180Main(&w180);
    FilmWork(&film);
    PhotoWrk274Work(&w274);
    PhotoWrk288Main(&w288);
    PhotoGage4bcWork(&w4bc);
    if (IsLockTblOn(0x15) == 0)
    {
        if (GetDirTrg(0))
            FilmSelectPrev(&film);
        if (GetDirTrg(1))
            FilmSelectNext(&film);
    }
    ZoomCtrl();
}

// ──────────────────────────────────────────────────────────────────────
// Real mode, per frame: the reduced set (the +0x244 gauge only for player 2
// outside ingame_wrk.b03; no lens, film choice or zoom).
// FF3 0x001b5cd0.

void PHOTO_MODE_WRK::RealMain()
{
    TegataMain();
    if (ingame_wrk.b03 == 0 && PlyrGetCurNo() == 2)
        FilamentWork(&filament);
    HudHpWork(&hp);
    PlyrGaugeMain(&album_save_disp);
    if (ingame_wrk.plyr_wrk->_250 == 0)
        PhotoChargerWork(&charger, 1);

    if ((ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000) == 0)
        return;

    EneMarkWork(ene_mark);
    PhotoWrk180Main(&w180);
    FilmWork(&film);
    PhotoWrk288Main(&w288);
}

// ──────────────────────────────────────────────────────────────────────
// The textures of the viewfinder (LANG_TEX pictures) go up while it is
// visible, the two HUD ones always; then the mode's draw.
// FF3 0x001b5da0.

void PHOTO_MODE_WRK::Draw()
{
    HudTexSend(mode != PHOTO_MODE_FINDER);
    if (mode == PHOTO_MODE_FINDER)
        FinderDraw();
    else
        RealDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Finder mode draw. Camera down (view alpha 0): the HP frame and gauge and
// the album-save notice at HUD fade 0, then -- outside ingame_wrk.b03, when
// the player has table entry 8 or is player 2 -- the +0x244 gauge at the
// larger fade and, for player 0, the flash counter. Camera up (unless lock
// 0xd): every viewfinder part around the finder point.
// FF3 0x001b5de8.

void PHOTO_MODE_WRK::FinderDraw()
{
    int alpha = GetViewAlpha();

    if (alpha == 0)
    {
        int   hud  = hud_fade[0].val;
        int   gage = hud_fade[1].val;
        if (gage < hud)
            gage = hud;
        float a_hud  = (float)hud * (1.0f / 128.0f);
        float a_gage = (float)gage * (1.0f / 128.0f);

        HudHpFrameDraw(0.0f, 0.0f, a_hud);
        if (hud > 0)
        {
            HudHpDraw(0.0f, 0.0f, &hp, hud);
            PlyrGaugeDraw(0.0f, 0.0f, &album_save_disp, hud);
        }

        if (ingame_wrk.b03 != 0)
            return;
        if (*ItemGetCnt(8) == 0 && PlyrGetCurNo() != 2)
            return;

        HudGageFrameDraw(0.0f, 0.0f, a_gage);
        FilamentDraw(0.0f, 0.0f, &filament, gage);
        if (PlyrGetCurNo() != 0)
            return;

        int flash = PlyrGetFlashNum();
        int ready = 0;
        if (flash > 0)
            ready = PlyrFlashIsReady() != 0;
        HudFlashCountDraw(0.0f, 0.0f, a_hud, flash, ready);
        return;
    }

    if (IsLockTblCnt(0xd))
        return;

    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    float     a    = (float)alpha * (1.0f / 128.0f);
    float     x    = plyr->finder_x - 320.0f;
    float     y    = plyr->finder_y - 224.0f;
    int       pct  = (int)(a * 100.0f);

    // PC: the viewfinder surround (reipan_renderer.h), a port addition:
    // the world outside the 640 x 448 frame, defocused, darkened and cast
    // cold. Here because the world and its effects are finished
    // (IngameDrawSub) and no viewfinder part is drawn yet, so the frame art
    // lands sharp on top; moved by the same sway (x, y) as that art, so the
    // strip the art uncovers while the camera pans is covered too.
    ReiPan_RendererDrawFinderMask(alpha, x, y);

    TegataDraw(x, y, alpha);
    EneMarkDraw(ingame_wrk.plyr_wrk->finder_x, ingame_wrk.plyr_wrk->finder_y, ene_mark, alpha, 0);
    HudFinderFrameDraw(x, y, a, 0);
    HudHpFrameDraw(x, y, a);
    FilamentDraw(x, y, &filament, pct);
    HudHpDraw(x, y, &hp, pct);
    PlyrGaugeDraw(x, y, &album_save_disp, pct);
    PhotoWrk180Draw(ingame_wrk.plyr_wrk->finder_x, ingame_wrk.plyr_wrk->finder_y, &w180, pct, 0);
    w1e8.Draw(ingame_wrk.plyr_wrk->finder_x, ingame_wrk.plyr_wrk->finder_y, pct);

    int type      = FilmGetType(&film);
    int num       = FilmGetNum(&film);
    int flash     = PlyrGetFlashNum();
    int can_shoot = PhotoChargerIsReady(&charger) != 0 && num > 0;
    int ready     = 0;
    if (flash > 0)
        ready = PlyrFlashIsReady() != 0;
    HudFinderCountDraw(x, y, a, type, num, can_shoot, flash, ready);

    if (lens.GetFunc(LENS_FUNC_274))
        PhotoWrk274Draw(x, y, &w274, alpha);
    DrawNearEnemy(x, y, alpha);
    lens.Draw(ingame_wrk.plyr_wrk->finder_x, ingame_wrk.plyr_wrk->finder_y, alpha);
    PhotoWrk288Draw(x, y, &w288, alpha, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Real mode draw: the HP frame and gauge at HUD fade 0 and, for player 2,
// the +0x244 gauge; with the camera up the reduced viewfinder.
// FF3 0x001b61e0.

void PHOTO_MODE_WRK::RealDraw()
{
    int alpha = GetViewAlpha();

    if (alpha == 0)
    {
        int hud = hud_fade[0].val;

        HudHpFrameDraw(0.0f, 0.0f, (float)hud * (1.0f / 128.0f));
        if (hud > 0)
            HudHpDraw(0.0f, 0.0f, &hp, hud);

        if (ingame_wrk.b03 != 0 || PlyrGetCurNo() != 2)
            return;

        int gage = hud_fade[1].val;
        if (gage < hud)
            gage = hud;
        HudGageFrameDraw(0.0f, 0.0f, (float)gage * (1.0f / 128.0f));
        FilamentDraw(0.0f, 0.0f, &filament, gage);
        return;
    }

    if (IsLockTblCnt(0xd))
        return;

    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    float     a    = (float)alpha * (1.0f / 128.0f);
    float     y    = plyr->finder_y - 224.0f;
    int       pct  = (int)(a * 100.0f);
    float     x    = plyr->finder_x - 320.0f;

    ReiPan_RendererDrawFinderMask(alpha, x, y);   // PC: as in FinderDraw

    EneMarkDraw(plyr->finder_x, plyr->finder_y, ene_mark, alpha, 1);
    HudFinderFrameDraw(x, y, a, 1);
    HudHpFrameDraw(x, y, a);
    HudHpDraw(x, y, &hp, pct);
    PhotoWrk180Draw(ingame_wrk.plyr_wrk->finder_x, ingame_wrk.plyr_wrk->finder_y, &w180, pct, 1);
    PhotoWrk288Draw(x, y, &w288, alpha, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The nearest targetable slot (state 3, its +0x590 part up, closer than
// 10000; the first of equals wins) gets its part drawn; then the lens-4 part.
// FF3 0x001b6418.

void PHOTO_MODE_WRK::DrawNearEnemy(float x, float y, int alpha)
{
    ENE_WRK *near = nullptr;
    float         best = 10000.0f;

    for (int i = 0; i < ENE_WRK_MAX; i++)
    {
        ENE_WRK *slot = &ene_wrk[i];

        if (slot->state == 3 && DamageDispIsOn(&slot->dmg_disp) && slot->dist < best)
        {
            best = slot->dist;
            near = slot;
        }
    }

    if (near != nullptr)
        DamageDispDraw(x, y, &near->dmg_disp, alpha);

    if (lens.GetFunc(LENS_FUNC_4BC) == 0)
        return;
    PhotoGage4bcDraw(x, y, &w4bc, alpha);
}

// ──────────────────────────────────────────────────────────────────────
// With the zoom lens, holding zoom in / out moves the zoom by 0.03 a frame
// within 0.7..1.3; without it the zoom is 1.0.
// FF3 0x001b6548.

void PHOTO_MODE_WRK::ZoomCtrl()
{
    if (lens.GetFunc(LENS_FUNC_ZOOM) == 0)
    {
        zoom = 1.0f;
        return;
    }

    if (CheckPadNowPlaySe(pad_key[PAD_KEY_ZOOM_IN], -1, 0))
    {
        zoom += 0.03f;
        if (zoom > 1.3f)
            zoom = 1.3f;
    }
    else if (CheckPadNowPlaySe(pad_key[PAD_KEY_ZOOM_OUT], -1, 0))
    {
        zoom -= 0.03f;
        if (zoom < 0.7f)
            zoom = 0.7f;
    }
}

// ══════════════════════════════════════════════════════════════════════
// Zoom, mode
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6628.

void PHOTO_MODE_WRK::ZoomReset()
{
    zoom = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6640.

float PHOTO_MODE_WRK::GetZoom()
{
    return zoom;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6648.

u_char *PHOTO_MODE_WRK::GetModePtr()
{
    return &mode;
}

// ──────────────────────────────────────────────────────────────────────
// Each mode has its own SE bank: a mode change releases the old one and
// loads the new one; 'force' reloads the current one (after a load).
// FF3 0x001b6658.

void PHOTO_MODE_WRK::SetMode(u_char mode, int force)
{
    u_char cur = this->mode;

    if (cur != mode)
    {
        if (cur == PHOTO_MODE_FINDER || cur == PHOTO_MODE_REAL)
        {
            SndSeBankRelease(cur == PHOTO_MODE_FINDER ? SYS_FINDER_BD_BD : SYS_REAL_FINDER_BD_BD, 0);
            cur = this->mode;
        }
    }

    if (cur == mode && force == 0)
    {
        this->mode = mode;
        return;
    }

    if (mode == PHOTO_MODE_FINDER || mode == PHOTO_MODE_REAL)
        SndSeBankLoad(mode == PHOTO_MODE_FINDER ? SYS_FINDER_BD_BD : SYS_REAL_FINDER_BD_BD);
    this->mode = mode;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b66f8.

u_char PHOTO_MODE_WRK::GetMode()
{
    return mode;
}

// ──────────────────────────────────────────────────────────────────────
// The loaded film's power in finder mode (FF3 reads the global work's film
// directly), 0 in real mode.
// FF3 0x001b6700.

u_char PHOTO_MODE_WRK::GetFilmPower()
{
    if (mode == PHOTO_MODE_FINDER)
        return photo_film_power[GetPhotoModeWrk()->film.type];
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Sets up the +0x220 sub-work (of the global work) for the mode: finder
// PhotoChargerSetup, real PhotoChargerInit with 8 (PAL) or 10 (NTSC).
// FF3 0x001b6730.

void PHOTO_MODE_WRK::SetSub220Rate()
{
    if (mode == PHOTO_MODE_FINDER)
    {
        PhotoChargerSetup(GetPhotoModeWrk220());
        return;
    }
    PhotoChargerInit(GetPhotoModeWrk220(), GetPALMode() ? 8 : 10);
}

// ══════════════════════════════════════════════════════════════════════
// The work and its parts
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The work, bss 0x3e7918. FF3 0x001b6ec8 (PhotoModeStaticInit, the file's
// __static_initialization_and_destruction_0(1, 0xffff)) runs its
// constructor at start-up, called by the global constructor FF3 0x001b6f00
// (PhotoModeGlobalCtor); the C++ static does both here.

static PHOTO_MODE_WRK photo_mode_wrk;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6780.

PHOTO_MODE_WRK *GetPhotoModeWrk(void)
{
    return &photo_mode_wrk;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6790.

PHOTO_ENE_MARK *GetPhotoEneMark(void)
{
    return photo_mode_wrk.ene_mark;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67a0.

PHOTO_WRK_180 *GetPhotoModeWrk180(void)
{
    return &photo_mode_wrk.w180;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67b0.

HUD_HP_WRK *GetHudHpWrk(void)
{
    return &photo_mode_wrk.hp;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67c0.

PHOTO_WRK_1E8 *GetPhotoModeWrk1e8(void)
{
    return &photo_mode_wrk.w1e8;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67d0.

PHOTO_CHARGER *GetPhotoModeWrk220(void)
{
    return &photo_mode_wrk.charger;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67e0.

FILM_WRK *GetFilmWrk(void)
{
    return &photo_mode_wrk.film;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b67f0.

FILAMENT_WRK *GetPhotoModeWrk244(void)
{
    return &photo_mode_wrk.filament;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6800.

PHOTO_WRK_274 *GetPhotoModeWrk274(void)
{
    return &photo_mode_wrk.w274;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6810.

PHOTO_WRK_288 *GetPhotoModeWrk288(void)
{
    return &photo_mode_wrk.w288;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6820.

PLYR_GAUGE *GetAlbumSaveDisp(void)
{
    return &photo_mode_wrk.album_save_disp;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6830.

LENS_WRK *GetLensWrk(void)
{
    return &photo_mode_wrk.lens;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6840.

PHOTO_WRK_4BC *GetPhotoModeWrk4bc(void)
{
    return &photo_mode_wrk.w4bc;
}

// ══════════════════════════════════════════════════════════════════════
// Save-data getters (entries of the table at data 0x332b4c)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6850.

int PhotoModeSaveGetLens(void **out)
{
    *out = GetLensWrk()->GetSaveData();
    return 0x70;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6888.

int PhotoModeSaveGetMode(void **out)
{
    *out = GetPhotoModeWrk()->GetModePtr();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b68c0.

int PhotoModeSaveGetFilm(void **out)
{
    *out = FilmGetSaveData(GetFilmWrk());
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b68f8.

int PhotoModeSaveGetAlbumSaveDisp(void **out)
{
    *out = PlyrGaugeGetSaveData(GetAlbumSaveDisp());
    return 8;
}

// ══════════════════════════════════════════════════════════════════════
// The hand-print overlay (TOUSYU_TEGATA_PK4)
// ══════════════════════════════════════════════════════════════════════

// data 0x3596a8: the hand print's two pictures (the second flipped).
static const SPRT_DAT photo_tegata_sprt[2] =
{
    { 0x20057c05dd40abc0ull, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 1 },
    { 0x20057c05dd40abc0ull, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 2, 1 },
};

// ──────────────────────────────────────────────────────────────────────
// Starts loading the pack into a system-heap buffer (once).
// FF3 0x001b6930.

void PHOTO_MODE_WRK::TegataLoad()
{
    if (tegata_buf != nullptr)
        return;

    tegata_ready = 0;
    tegata_buf   = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(TOUSYU_TEGATA_PK4), 0, 0, 0);
    if (tegata_buf != nullptr)
        tegata_load_id = LoadReq(TOUSYU_TEGATA_PK4, tegata_buf);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b69a8.

void PHOTO_MODE_WRK::TegataFadeIn(u_char frames)
{
    if (frames == 0)
        frames = 1;
    tegata_step = (s_char)(90 / frames);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b69e0.

void PHOTO_MODE_WRK::TegataFadeOut(u_char frames)
{
    if (frames == 0)
        frames = 1;
    tegata_step = (s_char)(-90 / (int)frames);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6a18.

void PHOTO_MODE_WRK::TegataMain()
{
    PhotoTegataLevelStep(&tegata_level);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6a30.

int PHOTO_MODE_WRK::TegataIsReady()
{
    if (tegata_load_id == -1)
        return 0;
    if (tegata_ready != 1)
    {
        if (IsLoadEnd(tegata_load_id))
            tegata_ready = 1;
        return tegata_ready;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6a90.

void PHOTO_MODE_WRK::TegataRelease()
{
    if (tegata_buf == nullptr)
        return;

    heapCtrlFree(GetSystemHeapWrkP(), tegata_buf, 0, 0, 0);
    tegata_ready   = 0;
    tegata_load_id = -1;
    tegata_buf     = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Both pictures, additive (ALPHA 0x48), 1.75x, rotated 70 and -65 about
// their offsets, at alpha view * level / 128; the first also masks Z.
// FF3 0x001b6ae8.

void PHOTO_MODE_WRK::TegataDraw(float x, float y, int alpha)
{
    SPRT_DAT  spr[2] = { photo_tegata_sprt[0], photo_tegata_sprt[1] };     // FF3 copies them to the stack
    DISP_SPRT ds;
    int       a = (int)((float)(alpha * tegata_level) * (1.0f / 128.0f));

    if (a == 0 || TegataIsReady() == 0)
        return;

    PK4SendVram(tegata_buf, 0, -1, -1);

    CopySprDToSpr(&ds, &spr[0]);
    ds.alphar = 0x48;
    ds.zbuf   = 0x10a000118ull;
    ds.test   = 0x30003;
    ds.alpha  = (u_char)a;
    ds.x      = x + 248.0f;
    ds.crx    = x + 376.0f;
    ds.csx    = ds.crx;
    ds.y      = y + 257.0f;
    ds.cry    = ds.y;
    ds.csy    = ds.y;
    ds.rot    = 70.0f;
    ds.r      = 0x80;
    ds.g      = 0x80;
    ds.b      = 0x80;
    ds.pri    = 0x10;
    ds.z      = 0xfffef;
    ds.scw    = 1.75f;
    ds.sch    = 1.75f;
    DispSprD(&ds);

    CopySprDToSpr(&ds, &spr[1]);
    ds.alphar = 0x48;
    ds.test   = 0x30003;
    ds.alpha  = (u_char)a;
    ds.x      = x + 244.0f;
    ds.crx    = ds.x;
    ds.csx    = ds.x;
    ds.y      = y + 312.0f;
    ds.cry    = ds.y;
    ds.csy    = ds.y;
    ds.rot    = -65.0f;
    ds.r      = 0x80;
    ds.g      = 0x80;
    ds.b      = 0x80;
    ds.pri    = 0x10;
    ds.z      = 0xfffef;
    ds.scw    = 1.75f;
    ds.sch    = 1.75f;
    DispSprD(&ds);
}

// ══════════════════════════════════════════════════════════════════════
// View fade, SE
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// The pad_key[18] action works with the finder: the player has table
// entry 8 and the mode is the finder.
// FF3 0x001b6d38.

int PHOTO_MODE_WRK::IsFinderReady()
{
    if (*ItemGetCnt(8) == 0)
        return 0;
    return GetMode() == PHOTO_MODE_FINDER;
}

// ──────────────────────────────────────────────────────────────────────
// Starts the view fade from 0; PhotoModeWrkMain steps it over 'frames'.
// FF3 0x001b6d88.

void PHOTO_MODE_WRK::SetViewFade(u_char frames)
{
    view_fade.val    = 0;
    view_fade_frames = frames;
    view_fade.delta  = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001b6da0.

int PHOTO_MODE_WRK::GetViewAlpha()
{
    if (ingame_wrk.plyr_wrk->flags & PLYR_FLG_800000)
        return 0x80;
    if (view_fade_frames == 0)
        return 0;
    return view_fade.val;
}

// ──────────────────────────────────────────────────────────────────────
// SE 'no' of the current mode's bank, if that bank is in; -1 otherwise.
// FF3 0x001b6dd8.

int PhotoModeSePlay(int no, int vol, int pitch)
{
    // FF3 builds a 3-word sound position block on the stack (the player's
    // position first) and never passes it on: SndSePlay gets no position.
    void *pos[3];
    memset(pos, 0, sizeof(pos));
    pos[0] = &ingame_wrk.plyr_wrk->pos;

    u_char mode = GetPhotoModeWrk()->GetMode();
    if (mode == PHOTO_MODE_FINDER && SndSeBankIsLoaded(SYS_FINDER_BD_BD))
        return SndSePlay((u_int)no | (SYS_FINDER_BD_BD << 16), vol, pitch, 0, nullptr, 0);
    if (mode != PHOTO_MODE_REAL)
        return -1;
    if (SndSeBankIsLoaded(SYS_REAL_FINDER_BD_BD) == 0)
        return -1;
    return SndSePlay((u_int)no | (SYS_REAL_FINDER_BD_BD << 16), vol, pitch, 0, nullptr, 0);
}

// ══════════════════════════════════════════════════════════════════════
// Constructors and inline instances (FF3's shared area 0x2b94b0..)
// ══════════════════════════════════════════════════════════════════════

// ──────────────────────────────────────────────────────────────────────
// Counts a non-zero timer down; 1 on the step that reaches 0.
// FF3 0x002b94b0.

static int PhotoTimerCountDown(int *timer)
{
    if (*timer != 0 && --*timer == 0)
        return 1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// level += level[1] (the step), kept in 0..90.
// FF3 0x002b94d8.

static void PhotoTegataLevelStep(s_char *level)
{
    int v = level[0] + level[1];

    if (v > 90)
    {
        level[0] = 90;
        return;
    }
    if (v < 0)
    {
        level[0] = 0;
        return;
    }
    level[0] = (s_char)v;
}

// ──────────────────────────────────────────────────────────────────────
// The members' constructors, inlined except +0x1e8's init and the lens's
// (LENS_WRK's constructor runs before this body here).
// FF3 0x002b9510.
// callcheck: LensWrkCtor (lens's constructor, implicit)

PHOTO_MODE_WRK::PHOTO_MODE_WRK()
{
    w180.wave.val   = 0;
    w180.wave.delta = 0;
    w1e8.fade.val   = 0;
    w1e8.fade.delta = 0;
    w1e8.Init();
    charger.charge.val     = 0;
    charger.flash.wait.val = 0;
    filament.master_alp.val   = 0;
    filament.master_alp.delta = 0;
    w288.pulse.wait.val = 0;
    for (int i = 0; i < PHOTO288_ENT_NUM; i++)
        w288.ent[i].wait.val = 0;
    view_fade.delta = 0;
    tegata_level    = 0;
    tegata_step     = 0;
    hud_fade[0].val   = 0;
    hud_fade[0].delta = 0;
    hud_fade[1].val   = 0;
    hud_fade[1].delta = 0;
    hud_hold[0]       = 0;
    hud_hold[1]       = 0;
    view_fade.val     = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002b95b8.

LENS_WRK::LENS_WRK()
{
    for (int i = 0; i < 7; i++)
    {
        icon[i].state = LENS_ICON_OFF;
        icon[i].t     = 0.0f;
        icon[i].rot   = 0.0f;
    }
    sw.state       = 0;
    sw.t           = 0.0f;
    sw.equip       = LENS_NONE;
    label_wait.val = 0;
    se.interval    = 0;
    se.end         = 0;
    se.count       = 0;
}
