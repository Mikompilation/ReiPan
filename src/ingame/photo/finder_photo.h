/* ==========================================================================
 *  ingame/photo/finder_photo.h
 *
 *  The photo mode work (FF3-only, FF3 0x1b57d8..0x1b6f20 plus the inline
 *  instances 0x2b94b0..0x2b9610): one global object, bss 0x3e7918, 0x510
 *  bytes on the EE, built at start-up by its constructor (0x2b9510) and set up
 *  by IngameInit (PhotoModeWrkInit). It holds the camera's sub-works -- the
 *  per-slot screen markers, the HP gauge, the shutter charger, the film, the
 *  filament, the lenses, the album-save notice and four more whose roles are
 *  still open (their code: ingame/photo/fndr_parts.c, and
 *  ingame/photo/fndr_effect.c for +0x180 / +0x1e8 / +0x288) -- and the state
 *  finder_photo.c itself keeps:
 *    - the camera mode: 0 the finder (SE bank SYS_FINDER_BD), 1 the "real"
 *      camera (SYS_REAL_FINDER_BD) with a reduced HUD;
 *    - the view fade: 0x80 while the player work's flag 0x800000 (camera up)
 *      is set, else a fade; the draws show the viewfinder parts when it is
 *      non-zero and the HUD otherwise;
 *    - two HUD fades that rise on events (low HP, the pad_key[18] action,
 *      the +0x244 gauge) and hold 0x8c frames;
 *    - the zoom (lens function 6), and a hand-print overlay drawn from
 *      TOUSYU_TEGATA_PK4.
 *  The viewfinder camera (finder_view.c) and the HUD parts (finder_view_hud.c)
 *  are the next two object files. File names are descriptive: FF3-new files
 *  in the slot between FF2's adjacent finder_camera.c and fly_ctrl.c (the
 *  link order is alphabetical by basename).
 * ======================================================================== */
#ifndef _INGAME_PHOTO_FINDER_PHOTO_H
#define _INGAME_PHOTO_FINDER_PHOTO_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "ingame/plyr/plyr_gauge.h"     // PLYR_GAUGE
#include "common/cmn_fade_layer.h"      // CWRK_VAR_INT_0_128
#include "ingame/photo/fndr_parts.h"    // the sub-works (film, filament, lenses, ...)
#include "ingame/photo/fndr_effect.h"   // the sub-works +0x180, +0x1e8, +0x288

// PHOTO_MODE_WRK.mode
enum PHOTO_MODE
{
    PHOTO_MODE_FINDER = 0,              // SE bank SYS_FINDER_BD_BD
    PHOTO_MODE_REAL   = 1,              // SE bank SYS_REAL_FINDER_BD_BD
};

#define PHOTO_ENE_MARK_NUM  16          // one per slot of the 16 x 0x700 work 0x2fa3a0

// +0x180 / +0x1e8 / +0x288: PHOTO_WRK_180 / PHOTO_WRK_1E8 / PHOTO_WRK_288 in
// ingame/photo/fndr_effect.h.

// The photo mode work, bss 0x3e7918.
// layout: host -- tegata_buf is an EE pointer (4 bytes at +0x4e0); the
// offsets up to it match the ROM, the later fields follow it (EE offsets in
// the comments).
struct PHOTO_MODE_WRK                   /* 0x510 on the EE */
{
    PHOTO_MODE_WRK();                   // FF3 0x002b9510 (PhotoModeWrkCtor)

    void Init();                           // FF3 0x001b57d8 (PhotoModeWrkInit)
    void LoadRestore();                    // FF3 0x001b58b0 (PhotoModeWrkLoadRestore): reloads the mode's SE bank
    void ClearFlag();                      // FF3 0x001b58e0 (PhotoModeWrkClearFlag): mode = PHOTO_MODE_FINDER, no bank change
    void Main();                           // FF3 0x001b58e8 (PhotoModeWrkMain)
    void HudFadeInit();                    // FF3 0x001b59e0 (PhotoModeHudFadeInit)
    void HudFadeMain();                    // FF3 0x001b5a10 (PhotoModeHudFadeMain)
    void FinderMain();                     // FF3 0x001b5ba8 (PhotoModeWrkFinderMain)
    void RealMain();                       // FF3 0x001b5cd0 (PhotoModeWrkRealMain)
    void Draw();                           // FF3 0x001b5da0 (PhotoModeWrkDraw)
    void FinderDraw();                     // FF3 0x001b5de8 (PhotoModeWrkFinderDraw)
    void RealDraw();                       // FF3 0x001b61e0 (PhotoModeWrkRealDraw)
    void DrawNearEnemy(float x, float y, int alpha);   // FF3 0x001b6418 (PhotoModeDrawNearEnemy; the floats in f12 / f13, the object in a0)
    void ZoomCtrl();                       // FF3 0x001b6548 (PhotoModeZoomCtrl)
    void ZoomReset();                      // FF3 0x001b6628 (PhotoModeZoomReset)
    float GetZoom();                       // FF3 0x001b6640 (PhotoModeGetZoom)
    u_char *GetModePtr();                 // FF3 0x001b6648 (PhotoModeGetModePtr)
    void SetMode(u_char mode, int force);  // FF3 0x001b6658 (PhotoModeSetMode)
    u_char GetMode();                      // FF3 0x001b66f8 (PhotoModeGetMode)
    u_char GetFilmPower();                 // FF3 0x001b6700 (PhotoModeGetFilmPower)
    void SetSub220Rate();                  // FF3 0x001b6730 (PhotoModeSetSub220Rate)
    void TegataLoad();                     // FF3 0x001b6930 (PhotoModeTegataLoad)
    void TegataFadeIn(u_char frames);      // FF3 0x001b69a8 (PhotoModeTegataFadeIn)
    void TegataFadeOut(u_char frames);     // FF3 0x001b69e0 (PhotoModeTegataFadeOut)
    void TegataMain();                     // FF3 0x001b6a18 (PhotoModeTegataMain)
    int TegataIsReady();                   // FF3 0x001b6a30 (PhotoModeTegataIsReady)
    void TegataRelease();                  // FF3 0x001b6a90 (PhotoModeTegataRelease)
    void TegataDraw(float x, float y, int alpha);      // FF3 0x001b6ae8 (PhotoModeTegataDraw)
    int IsFinderReady();                   // FF3 0x001b6d38 (PhotoModeIsFinderReady)
    void SetViewFade(u_char frames);       // FF3 0x001b6d88 (PhotoModeSetViewFade)
    int GetViewAlpha();                    // FF3 0x001b6da0 (PhotoModeGetViewAlpha): 0..0x80

    /* 0x000 */ PHOTO_ENE_MARK     ene_mark[PHOTO_ENE_MARK_NUM];   // GetPhotoEneMark
    /* 0x180 */ PHOTO_WRK_180      w180;            // GetPhotoModeWrk180
    /* 0x1d0 */ HUD_HP_WRK         hp;              // GetHudHpWrk
    /* 0x1e8 */ PHOTO_WRK_1E8      w1e8;            // GetPhotoModeWrk1e8
    /* 0x220 */ PHOTO_CHARGER      charger;          // GetPhotoModeWrk220
    /* 0x240 */ FILM_WRK           film;            // GetFilmWrk
    /* 0x244 */ FILAMENT_WRK       filament;          // GetPhotoModeWrk244
    /* 0x274 */ PHOTO_WRK_274      w274;            // GetPhotoModeWrk274
    /* 0x288 */ PHOTO_WRK_288      w288;            // GetPhotoModeWrk288
    /* 0x3c0 */ PLYR_GAUGE         album_save_disp; // GetAlbumSaveDisp: player 1's slow-mode gauge (plyr_gauge.c; FF2 name kept)
    /* 0x3cc */ LENS_WRK           lens;            // GetLensWrk
    /* 0x4bc */ PHOTO_WRK_4BC      w4bc;            // GetPhotoModeWrk4bc
    /* 0x4d0 */ int                _4d0;            // 0 (PhotoModeWrkInit)
    /* 0x4d4 */ float              zoom;            // 0.7..1.3, 1.0 without the zoom lens
    /* 0x4d8 */ u_char             mode;            // PHOTO_MODE
    /* 0x4d9 */ u_char             _4d9[3];
    /* 0x4dc */ int                tegata_ready;    // 1 once the TOUSYU_TEGATA_PK4 load ended
    /* 0x4e0 */ void              *tegata_buf;      // system-heap buffer of TOUSYU_TEGATA_PK4
    /* 0x4e4 */ int                tegata_load_id;  // LoadReq id, -1: none
    /* 0x4e8 */ s_char             tegata_level;    // 0..90
    /* 0x4e9 */ s_char             tegata_step;     // per frame, +-90 / frames
    /* 0x4ea */ u_char             _4ea[2];
    /* 0x4ec */ CWRK_VAR_INT_0_128 hud_fade[2];     // 0: HP and HUD, 1: the +0x244 gauge
    /* 0x4fc */ int                hud_hold[2];     // frames left before hud_fade[i] fades out
    /* 0x504 */ CWRK_VAR_INT_0_128 view_fade;       // camera up / down fade
    /* 0x50c */ u_char             view_fade_frames;// 0: no fade running
    /* 0x50d */ u_char             _50d[3];
};
static_assert(offsetof(PHOTO_MODE_WRK, w180) == 0x180, "PHOTO_MODE_WRK.w180");
static_assert(offsetof(PHOTO_MODE_WRK, hp) == 0x1d0, "PHOTO_MODE_WRK.hp");
static_assert(offsetof(PHOTO_MODE_WRK, w1e8) == 0x1e8, "PHOTO_MODE_WRK.w1e8");
static_assert(offsetof(PHOTO_MODE_WRK, charger) == 0x220, "PHOTO_MODE_WRK.charger");
static_assert(offsetof(PHOTO_MODE_WRK, film) == 0x240, "PHOTO_MODE_WRK.film");
static_assert(offsetof(PHOTO_MODE_WRK, filament) == 0x244, "PHOTO_MODE_WRK.filament");
static_assert(offsetof(PHOTO_MODE_WRK, w274) == 0x274, "PHOTO_MODE_WRK.w274");
static_assert(offsetof(PHOTO_MODE_WRK, w288) == 0x288, "PHOTO_MODE_WRK.w288");
static_assert(offsetof(PHOTO_MODE_WRK, album_save_disp) == 0x3c0, "PHOTO_MODE_WRK.album_save_disp");
static_assert(offsetof(PHOTO_MODE_WRK, lens) == 0x3cc, "PHOTO_MODE_WRK.lens");
static_assert(offsetof(PHOTO_MODE_WRK, w4bc) == 0x4bc, "PHOTO_MODE_WRK.w4bc");
static_assert(offsetof(PHOTO_MODE_WRK, zoom) == 0x4d4, "PHOTO_MODE_WRK.zoom");
static_assert(offsetof(PHOTO_MODE_WRK, mode) == 0x4d8, "PHOTO_MODE_WRK.mode");
static_assert(offsetof(PHOTO_MODE_WRK, tegata_ready) == 0x4dc, "PHOTO_MODE_WRK.tegata_ready");
static_assert(offsetof(PHOTO_MODE_WRK, tegata_buf) == 0x4e0, "PHOTO_MODE_WRK.tegata_buf (8-aligned on the host too)");
static_assert(offsetof(PHOTO_MODE_WRK, tegata_load_id) == 0x4e0 + sizeof(void *), "PHOTO_MODE_WRK.tegata_load_id follows tegata_buf");
static_assert(offsetof(PHOTO_MODE_WRK, hud_fade) == offsetof(PHOTO_MODE_WRK, tegata_load_id) + 0x08, "PHOTO_MODE_WRK.hud_fade (EE 0x4ec)");
static_assert(offsetof(PHOTO_MODE_WRK, hud_hold) == offsetof(PHOTO_MODE_WRK, tegata_load_id) + 0x18, "PHOTO_MODE_WRK.hud_hold (EE 0x4fc)");
static_assert(offsetof(PHOTO_MODE_WRK, view_fade) == offsetof(PHOTO_MODE_WRK, tegata_load_id) + 0x20, "PHOTO_MODE_WRK.view_fade (EE 0x504)");
static_assert(offsetof(PHOTO_MODE_WRK, view_fade_frames) == offsetof(PHOTO_MODE_WRK, tegata_load_id) + 0x28, "PHOTO_MODE_WRK.view_fade_frames (EE 0x50c)");

// The work and its parts (getters 0x1b6780..0x1b6840).
PHOTO_MODE_WRK  *GetPhotoModeWrk(void);
PHOTO_ENE_MARK  *GetPhotoEneMark(void);
PHOTO_WRK_180   *GetPhotoModeWrk180(void);
HUD_HP_WRK      *GetHudHpWrk(void);
PHOTO_WRK_1E8   *GetPhotoModeWrk1e8(void);
PHOTO_CHARGER   *GetPhotoModeWrk220(void);
FILM_WRK        *GetFilmWrk(void);
FILAMENT_WRK   *GetPhotoModeWrk244(void);
PHOTO_WRK_274   *GetPhotoModeWrk274(void);
PHOTO_WRK_288   *GetPhotoModeWrk288(void);
PLYR_GAUGE *GetAlbumSaveDisp(void);
LENS_WRK        *GetLensWrk(void);
PHOTO_WRK_4BC   *GetPhotoModeWrk4bc(void);

// IngameInit / IngameLoadRestore / IngameMain / IngameDraw.


// Save-data getters (table at data 0x332b4c): the block's address in *out,
// its size as the result.
int PhotoModeSaveGetLens(void **out);
int PhotoModeSaveGetMode(void **out);
int PhotoModeSaveGetFilm(void **out);
int PhotoModeSaveGetAlbumSaveDisp(void **out);

// The hand-print overlay (TOUSYU_TEGATA_PK4).

int  PhotoModeSePlay(int no, int vol, int pitch);       // SE 'no' of the mode's bank; -1: bank not loaded

#endif /* _INGAME_PHOTO_FINDER_PHOTO_H */
