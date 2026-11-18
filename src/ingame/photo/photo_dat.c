// ingame/photo/photo_dat.c -- the photo sequence and phase 33 Story_Photo
// (FF3 0x2274e0..0x228718; FF3-only code in FF2's photo_dat.c link slot,
// doing what FF2's photo.c phase half did; see photo_dat.h).
//
// The work is one object (photo_wrk, bss 0x3f6670); PhotoMain runs the
// step photo_wrk.mode names in photo_step_tbl each frame of phase 33. The
// steps in order (FF2's beats in brackets):
//    0 the next subject: its hint texture is requested (-> 4) or none (-> 1)
//    1 white flash, frame frozen into page 0x3480 (-> 3); a scenery shot
//      draws the frame first (-> 2)          [FF2 PictureInitSub / PicturePre3]
//    2 flash for the scenery shot (-> 3)
//    3 the developed frame (-> 7)            [FF2 PicturePre4]
//    4 hint texture loading (-> 5)           [FF2 mode 4]
//    5 flash with the hint shot (-> 6), 6 its frame (-> 7)
//    7 the frame held cnt frames; with a hint -> 8 (SE 8 + the hint stream), else -> 13
//    8 hint picture fades in (-> 9)          [FF2 PictureHint1]
//    9 hint message window (-> 10, or 13)    [FF2 PictureHint2]
//   10 hint picture fades out, texture freed (-> 13) [FF2 PictureHint3]
//   11 seal-door burn starts (-> 12), 12 waits for it (-> 14) [FF2 PictureToUnlockGhost / PictureUnlockGhost]
//   13 the frame encoded into the album slot (-> 14, or 11 to unlock) [FF2 PictureCapture]
//   14 the subjects marked photographed, 10 frames, then done [FF2 PictureEnd]
// The frame and hint drawing is photo_make.c; the encode (CompPhotoFromWorkArea)
// is photo_comp.c, the file after this one.
//
// FF3 0x002286f8 is not written: a compiler-emitted static-destruction thunk
// (GPhaseCtrlTerm on the controller singleton) that marks the end of the
// file, as 0x2274c0 ends the peeping file before this one.

#include "ingame/photo/photo_dat.h"

#include "graphics/effect/effect.h"            // eff_wrk
#include "graphics/effect/effect_obj.h"        // EffSealDoorBurnLoad / IsEnd / Release
#include "graphics/effect/effect_oth.h"        // ScreenFlashWhite, ScreenFlashDraw
#include "graphics/graph2d/g2d_draw.h"         // LocalCopyLtoL, LocalCopyLtoB
#include "graphics/graph3d/gra3dSGD.h"         // _SetPREVIOUSTRI2PRIM
#include "graphics/gs_reg.h"                   // SetGsRegCached
#include "common/heapctrl.h"                   // heapCtrlMalloc / Free
#include "ingame/story_menu_photo.h"           // DocFlagSet
#include "ingame/story_develop.h"              // DevelopFilmShoot
#include "ingame/photo/photo_file.h"            // InitPhotoWrk
#include "ingame/enemy/ene_main.h"             // EnemyMain
#include "ingame/ingame.h"                     // IngameDrawSub, GPhaseCtrlRequest, SndSePlayStd
#include "ingame/ingame_wrk.h"                 // ingame_wrk
#include "ingame/item/prg/soul_list.h"         // SoulListSetHave
#include "ingame/map/MapObjWrk.h"              // MapObjUpdateRoom
#include "ingame/menu/anim_2d.h"               // LayoutAnim*, Anim2dMsgSet*
#include "ingame/photo/finder_photo.h"         // GetPhotoModeWrk, PhotoModeSePlay
#include "ingame/photo/finder_view.h"          // FinderViewCameraCtrl
#include "ingame/photo/photo_comp.h"           // CompPhotoFromWorkArea
#include "ingame/photo/photo_make.h"           // DispPhotoFrame1, DrawPhotoFrame, DrawPhotoHinttex, ...
#include "ingame/photo/photo_obj.h"            // PhotoObjInit, PhotoObjHoldAlpha
#include "ingame/photo/photo_obj_dat.h"        // PHOTO_OBJ_DAT
#include "ingame/plyr/player.h"                // PLYR_WRK, PlyrClear234, GetPlyrAreaNo
#include "ingame/sound/snd_ctrl.h"             // SndConvVolume
#include "ingame/story_menu.h"                 // LockTblInc / Dec
#include "main/gphase.h"                       // GetGPhaseCtrl
#include "system/b2d/BpCtl.h"                  // BpCtlMain
#include "system/eeiop/cddat.h"                // GetFileSize
#include "system/eeiop/stream_auto.h"          // StreamAutoPlayPitch, StreamAutoIsPlaying
#include "system/os/eecdvd.h"                  // LoadReq, IsLoadEndAll
#include "system/os/sys_heap.h"                // GetSystemHeapWrkP
#include "system/os/system.h"                  // GetSysWrk

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static PHOTO_WRK photo_wrk;             // bss 0x3f6670

// The picture's top-left corner on screen (sdata 0x37182c.. : 128 / 96 in
// every pair); the photo is 384x256 (photo_make.h).
#define PHOTO_FRAME_X   128.0f
#define PHOTO_FRAME_Y   96.0f

// The display buffers alternate on frame parity: page 0 / 0x1180 (FF3
// reads the frame count through a cached GetSysWrk pointer each time).
#define PHOTO_DISP_PAGE ((int)(GetSysWrk()->count & 1) * 0x1180)
#define PHOTO_DRAW_PAGE ((int)((GetSysWrk()->count + 1) & 1) * 0x1180)

static int PhotoStepSubject(void);
static int PhotoStepFlash(void);
static int PhotoStepFurnFlash(void);
static int PhotoStepDevelopStart(void);
static int PhotoStepHintLoadWait(void);
static int PhotoStepHintFlash(void);
static int PhotoStepHintDevelop(void);
static int PhotoStepDevelop(void);
static int PhotoStepHintIn(void);
static int PhotoStepHintMsg(void);
static int PhotoStepHintOut(void);
static int PhotoStepSealBurnStart(void);
static int PhotoStepSealBurnWait(void);
static int PhotoStepCapture(void);
static int PhotoStepEnd(void);

// data 0x35fb20
static const PHOTO_STEP photo_step_tbl[] =
{
    {  0, PhotoStepSubject,       0x2000 },
    {  1, PhotoStepFlash,         0x2000 },
    {  2, PhotoStepFurnFlash,     0x2080 },
    {  3, PhotoStepDevelopStart,  -1     },
    {  4, PhotoStepHintLoadWait,  0x2000 },
    {  5, PhotoStepHintFlash,     0x80   },
    {  6, PhotoStepHintDevelop,   -1     },
    {  7, PhotoStepDevelop,       -1     },
    {  8, PhotoStepHintIn,        -1     },
    {  9, PhotoStepHintMsg,       -1     },
    { 10, PhotoStepHintOut,       -1     },
    { 11, PhotoStepSealBurnStart, 0x80   },
    { 12, PhotoStepSealBurnWait,  0x80   },
    { 13, PhotoStepCapture,       -1     },
    { 14, PhotoStepEnd,           0      },
    { -1, nullptr,                -1     },
};

// ──────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────

// ZBUF_1 written masked, then unmasked (ZBP 0x118, PSMZ 0xa), when the shot
// carries both message ids.
// FF3 0x002274e0

static void PhotoZbufRefresh(void)
{
    if (photo_wrk.def.msg_type >= 0 && photo_wrk.def.msg_name >= 0)
    {
        SetGsRegCached(0x10a000118ULL, 0x4e, 1);
        SetGsRegCached(0x0a000118ULL, 0x4e, 1);
    }
}

// The hint texture file onto the system heap (the old one freed first).
// FF3 0x00227540

static void *PhotoHintTexLoad(void *old, int file)
{
    void *adrs;

    if (old != nullptr)
        heapCtrlFree(GetSystemHeapWrkP(), old, 0, 0, 0);

    adrs = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(file), 0, 0, 0);
    LoadReq(file, adrs);
    return adrs;
}

// FF3 0x002275d0

static void PhotoSetFrameBit(int on)
{
    photo_wrk.bits = (u_char)((photo_wrk.bits & ~PHOTO_BIT_FRAME) | (on & 1));
}

// Clears PHOTO_BIT_END. Name from FF2 (photo_dat.c, which set a
// seal-ghost draw lock there); FF3 clears a bit nothing here reads.
// FF3 0x00227650

void photo_datObjSealGhostDrawLock(void)
{
    photo_wrk.bits &= ~PHOTO_BIT_END;
}

// The white flash of the shot: the counter set to the flash's length (20 /
// 60 frames, short with an ordinary enemy active), the shown frame frozen
// into page 0x3480 and also copied to the draw page.
// FF3 0x00227670
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static void PhotoFlashStart(void)
{
    int fast;

    photo_datObjSealGhostDrawLock();
    fast = (ingame_wrk.plyr_wrk->flags & PLYR_FLG_000020) != 0;
    photo_wrk.cnt = fast ? 0x14 : 0x3c;
    LocalCopyLtoL(3, PHOTO_DISP_PAGE, 0x3480);
    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    ScreenFlashWhite(fast ? 0x13 : 0x3b);
}

// The frame in a style at an alpha: a special shot draws style-3 frames at
// alpha 3, a special subject's hint at 100.
// FF3 0x00227748

static void PhotoFrameDraw(int style, int alpha)
{
    if (photo_wrk.bits & PHOTO_BIT_SPECIAL)
        alpha = 3;
    if ((photo_wrk.bits & PHOTO_BIT_SUBJECT) && photo_wrk.cur->type == 3)
        alpha = 100;
    DispPhotoFrame1(PHOTO_FRAME_X, PHOTO_FRAME_Y, style, alpha);
}

// The frame with the current subject's hint picture over it (step 0 in,
// 1 hold, 2 out); a scenery shot only draws the frame.
// FF3 0x002277a8

static int PhotoHintDraw(int step, int alpha)
{
    if ((photo_wrk.sta & PHOTO_STA_FURN) == 0)
    {
        PhotoFrameDraw(2, alpha);
        if (photo_wrk.bits & PHOTO_BIT_SUBJECT)
        {
            short kind  = (short)photo_wrk.cur->flags;     // FF3 reads the low half
            int   flags = 0;
            int   a;

            if (kind == 0)
                flags = 2;
            else if (kind == 1)
                flags = 3;

            a = PhotoHintAlpha(step, photo_wrk.cnt, 0x80);
            if (photo_wrk.cur->type == 3)
            {
                flags |= 4;
                a = (int)((float)a * 0.8f);                 // sdata 0x371834
            }
            DrawPhotoHinttex(PHOTO_FRAME_X, PHOTO_FRAME_Y, 0x180, 0x100, a, photo_wrk.hint_tex,
                         flags, photo_wrk.cur->type == 3);
        }
    }
    else
    {
        DispPhotoFrame1(PHOTO_FRAME_X, PHOTO_FRAME_Y, 2, 100);
    }
    DrawPhotoFrame(PHOTO_FRAME_X, PHOTO_FRAME_Y);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The steps (photo_step_tbl)
// ──────────────────────────────────────────────────────────────────────

// Step 0: the next photographed subject. One whose record names a hint
// texture (+0x28) has it loaded (step 4); else the flash (step 1). Lock
// 0xd holds the game until step 14; the room objects redraw.
// FF3 0x002278f8

static int PhotoStepSubject(void)
{
    short file = -1;

    if (photo_wrk.hint_no < photo_wrk.def.hint_cnt)
    {
        photo_wrk.cur = PhotoObjGetDat(photo_wrk.def.hint_pict[photo_wrk.hint_no]);
        photo_wrk.hint_no++;
        if ((photo_wrk.sta & PHOTO_STA_FURN) == 0)
            file = (short)photo_wrk.cur->_28;
        photo_wrk.bits |= PHOTO_BIT_SUBJECT;
    }

    if (file < 0)
    {
        photo_wrk.mode = 1;
    }
    else
    {
        photo_wrk.hint_tex = PhotoHintTexLoad(photo_wrk.hint_tex, file);
        photo_wrk.mode = 4;
        photo_wrk.sta |= PHOTO_STA_HINT_LOAD;
    }

    LockTblInc(0xd);
    MapObjUpdateRoom(0, 4);
    MapObjUpdateRoom(1, 4);
    return 0;
}

// Step 1: the flash (as PhotoFlashStart without the copy to the draw page),
// or for a scenery shot the frame's border first and step 2.
// FF3 0x002279d8
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepFlash(void)
{
    if ((photo_wrk.sta & PHOTO_STA_FURN) == 0)
    {
        int fast;

        photo_datObjSealGhostDrawLock();
        fast = (ingame_wrk.plyr_wrk->flags & PLYR_FLG_000020) != 0;
        photo_wrk.cnt = fast ? 0x14 : 0x3c;
        LocalCopyLtoL(3, PHOTO_DISP_PAGE, 0x3480);
        ScreenFlashWhite(fast ? 0x13 : 0x3b);
        photo_wrk.mode = 3;
    }
    else
    {
        DrawSpecialFurnPhoto(PHOTO_FRAME_X, PHOTO_FRAME_Y);
        PhotoSetFrameBit(1);
        photo_wrk.mode = 2;
    }
    return 0;
}

// Step 2: the scenery shot's flash.
// FF3 0x00227ad0

static int PhotoStepFurnFlash(void)
{
    PhotoFlashStart();
    photo_wrk.mode = 3;
    return 0;
}

// Step 3: the developed frame appears.
// FF3 0x00227b00

static int PhotoStepDevelopStart(void)
{
    PhotoFrameDraw(0, 100);
    photo_wrk.mode = 7;
    DrawPhotoFrame(PHOTO_FRAME_X, PHOTO_FRAME_Y);
    return 0;
}

// Step 7: the frame is held cnt frames. Then with a hint (a subject, or a
// scenery shot) SE 8, the subject's stream (+0x2c) and step 8; else the
// capture (step 13).
// FF3 0x00227b50

static int PhotoStepDevelop(void)
{
    PhotoFrameDraw(1, 100);
    if ((photo_wrk.sta & (PHOTO_STA_HINT_LOAD | PHOTO_STA_FURN)) == 0)
        PhotoZbufRefresh();

    if (photo_wrk.cnt-- < 1)
    {
        if ((photo_wrk.sta & (PHOTO_STA_HINT_LOAD | PHOTO_STA_FURN)) == 0)
        {
            photo_wrk.mode = 0xd;
        }
        else
        {
            photo_wrk.mode = 8;
            photo_wrk.cnt = 0x32;
            PhotoModeSePlay(8, 100, 0x1000);
            if (photo_wrk.cur->_2c >= 0)
            {
                photo_wrk.stream = StreamAutoPlayPitch(photo_wrk.cur->_2c, photo_wrk.cur->_2c + 1, 0xd, 1, 0,
                                                       SndConvVolume(100), 0, nullptr, 0x1000);
            }
            photo_wrk.sta &= ~PHOTO_STA_HINT_LOAD;
        }
    }
    DrawPhotoFrame(PHOTO_FRAME_X, PHOTO_FRAME_Y);
    return 0;
}

// Step 8: the hint picture fades in over 50 frames; it stays while the
// hint stream plays.
// FF3 0x00227c78

static int PhotoStepHintIn(void)
{
    PhotoHintDraw(0, photo_wrk.cnt * 2);
    if (photo_wrk.cnt-- < 1)
    {
        if (photo_wrk.stream < 0 || StreamAutoIsPlaying(photo_wrk.stream) == 0)
        {
            photo_wrk.mode = 9;
            photo_wrk.cnt = 0x32;
        }
        else
        {
            photo_wrk.cnt = 0;
        }
    }
    return 0;
}

// Step 9: the hint message (the subject's +0x24 / +0x26 ids) in layout
// window 5 until it closes; a subject without one only waits 50 frames.
// Then the fade out (step 10), or the capture for a scenery / special shot.
// FF3 0x00227cf8

static int PhotoStepHintMsg(void)
{
    int done = 0;

    PhotoHintDraw(1, 0);
    if (photo_wrk.cur->_24[0] < 0)
    {
        done = photo_wrk.cnt < 1;
        photo_wrk.cnt--;
        if (done)
            photo_wrk.cnt = 0x32;
        PhotoZbufRefresh();
    }
    else if (photo_wrk.cnt == 0x32)
    {
        photo_wrk.layout = LayoutAnimStart(5, 0);
        Anim2dMsgSet(photo_wrk.cur->_24[0], photo_wrk.cur->_24[1]);
        Anim2dMsgSetType(2);
        photo_wrk.cnt--;
    }
    else if (LayoutAnimGetState(photo_wrk.layout) == 4)
    {
        done = 1;
        LayoutAnimReset(photo_wrk.layout);
        photo_wrk.cnt = 0x32;
        photo_wrk.layout = -1;
    }

    if (done)
    {
        photo_wrk.mode = 0xd;
        if ((photo_wrk.sta & PHOTO_STA_FURN) == 0)
        {
            photo_wrk.mode = 10;
            if (photo_wrk.bits & PHOTO_BIT_SPECIAL)
                photo_wrk.mode = 0xd;
        }
    }

    LayoutAnimMain();
    BpCtlMain();
    return 0;
}

// Step 10: the hint picture fades out; its texture is freed, then the
// capture.
// FF3 0x00227e20

static int PhotoStepHintOut(void)
{
    PhotoHintDraw(2, 100 - photo_wrk.cnt * 2);
    if (photo_wrk.cnt-- < 1)
    {
        if (photo_wrk.hint_tex != nullptr)
        {
            heapCtrlFree(GetSystemHeapWrkP(), photo_wrk.hint_tex, 0, 0, 0);
            photo_wrk.hint_tex = nullptr;   // PC: FF3 keeps the pointer and PhotoHintTexLoad frees it again
        }
        photo_wrk.mode = 0xd;
    }
    return 0;
}

// Step 11: after cnt frames the seal-door burn (row 0) starts and the photo
// objects' alpha holds.
// FF3 0x00227eb0

static int PhotoStepSealBurnStart(void)
{
    DispPhotoFrame1(PHOTO_FRAME_X, PHOTO_FRAME_Y, 3, 100);
    if (photo_wrk.cnt-- < 1)
    {
        photo_wrk.cnt = 0x32;
        photo_wrk.mode = 0xc;
        EffSealDoorBurnLoad(0);
        PhotoObjHoldAlpha();
    }
    return 0;
}

// Step 12: the burn's end, then step 14.
// FF3 0x00227f38

static int PhotoStepSealBurnWait(void)
{
    if (EffSealDoorBurnIsEnd())
    {
        EffSealDoorBurnRelease();
        photo_wrk.cnt = 10;
        photo_wrk.mode = 0xe;
        eff_wrk._18 = 1;
    }
    return 0;
}

// Step 13: the frame encoded into the album slot: the draw page to EE
// 0x10ef000 (0x180 x 0x80 at 0x80, 0x60), encoded (CompPhotoFromWorkArea), the
// thumbnail at 0x1207000 + adr_no * 0x1000, the page back. Then the burn
// (step 11) for an unlock shot, else step 14.
// FF3 0x00227f98
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepCapture(void)
{
    int adr;

    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    LocalCopyLtoL(5, PHOTO_DRAW_PAGE, 0x2bc0);
    // 0x10ef000 / 0x1207000 are EE work areas (photo_make.c resolves them).
    CopyScreenToBuffer2(0x2bc0, 0x10ef000, 0, 0x80, 0x60, 0x180, 0x80);
    CompPhotoFromWorkArea(photo_wrk.def.adr_no);
    adr = photo_wrk.def.adr_no * 0x1000;
    MakeSmallPhotoV(PHOTO_FRAME_X, PHOTO_FRAME_Y);
    CopyScreenToBuffer2(0x2bc0, adr + 0x1207000, 0, 0, 0, 0x2d, 0xf);
    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    LocalCopyLtoB(0, 0, PHOTO_DRAW_PAGE);
    MapObjUpdateRoom(0, 2);
    MapObjUpdateRoom(1, 2);

    if (photo_wrk.def.unlock_ghost == 0)
    {
        photo_wrk.mode = 0xe;
        eff_wrk._18 = 1;
    }
    else
    {
        photo_wrk.mode = 0xb;
    }
    photo_wrk.cnt = 10;
    return 0;
}

// Step 4: the hint texture's load, then step 5.
// FF3 0x00228188
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepHintLoadWait(void)
{
    if (IsLoadEndAll())
        photo_wrk.mode = 5;
    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    return 0;
}

// Step 5: the flash of a shot with a hint.
// FF3 0x00228200
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepHintFlash(void)
{
    PhotoFlashStart();
    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    photo_wrk.mode = 6;
    return 0;
}

// Step 6: its developed frame (step 3's body; 3 sets mode 7 too).
// FF3 0x00228270
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepHintDevelop(void)
{
    PhotoStepDevelopStart();
    LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
    photo_wrk.mode = 7;
    return 0;
}

// Step 14: the finder view and enemies keep running. On the first frame
// (cnt 10) every subject is marked photographed, with what the current
// record asks (FF3 reads cur, the last subject, for all of them): a film
// developed (+0x30), a finder-camera flag (+0x38), flag 0x3eb3b0 (+0x3c);
// the shot's SE (photo mode +0x4d0); effects run again; lock 0xd off.
// 1 when the 10 frames are over.
// FF3 0x002282e0
// callcheck: GetSysWrk (PHOTO_DISP_PAGE / PHOTO_DRAW_PAGE)

static int PhotoStepEnd(void)
{
    s_char cnt;

    PlyrClear234();
    FinderViewCameraCtrl();
    EnemyMain();

    if (photo_wrk.cnt == 10)
    {
        LocalCopyLtoL(0, PHOTO_DRAW_PAGE, PHOTO_DISP_PAGE);
        photo_wrk.bits |= PHOTO_BIT_END;
        PhotoSetFrameBit(0);

        for (int i = 0; i < photo_wrk.def.hint_cnt; i++)
        {
            PhotoObjSetPhotographed(photo_wrk.def.hint_pict[i]);
            if (photo_wrk.cur->_30 >= 0)
                DevelopFilmShoot((short)photo_wrk.cur->_30, GetPlyrAreaNo(), (char)ingame_wrk._28);
            if (photo_wrk.cur->_38 >= 0)
                DocFlagSet((short)photo_wrk.cur->_38);
            if (photo_wrk.cur->_3c >= 0)
                SoulListSetHave(photo_wrk.cur->_3c);
        }

        if (GetPhotoModeWrk()->_4d0 != 0)
            SndSePlayStd(GetPhotoModeWrk()->_4d0);

        eff_wrk.stop_flg = 0;
        LockTblDec(0xd);
    }
    else
    {
        DispPhotoFrame1(PHOTO_FRAME_X, PHOTO_FRAME_Y, 3, 100);
    }

    cnt = photo_wrk.cnt--;
    return cnt < 1;
}

// ──────────────────────────────────────────────────────────────────────
// Start and phase 33
// ──────────────────────────────────────────────────────────────────────

// IngameInit / the load path: the photo tables start empty.
// FF3 0x002284a0

void PhotoDatInit(void)
{
    InitPhotoWrk();
    PhotoObjDatInit();
    PhotoObjInit();
}

// FF3 0x002284f0

void PhotoSeqReset(void)
{
    photo_wrk.sta = 0;
    photo_wrk.bits &= ~(PHOTO_BIT_FRAME | PHOTO_BIT_SPECIAL);
}

// The shot's definition taken over: step 0 next, the counter 1, the special
// / scenery type flags, no hint texture, stream or subject yet.
// FF3 0x00228518 (Ghidra had FF2's __static_initialization_and_destruction_0,
// a wrong match: PlyrShotRecord calls it with the PHOTO_WRK_DEF).

void PhotoSeqStart(PHOTO_WRK_DEF *def)
{
    if (def->type == PHOTO_TYPE_SPECIAL)
        photo_wrk.bits |= PHOTO_BIT_SPECIAL;
    else if (def->type == PHOTO_TYPE_FURN)
        photo_wrk.sta |= PHOTO_STA_FURN;

    photo_wrk.mode = 0;
    photo_wrk.cnt = 1;
    photo_wrk.bits |= PHOTO_BIT_02 | PHOTO_BIT_END;
    photo_wrk.def = *def;
    photo_wrk.stream = -1;
    photo_wrk.bits &= ~PHOTO_BIT_SUBJECT;
    eff_wrk._18 = 0;
    photo_wrk.hint_tex = nullptr;
    photo_wrk.hint_no = 0;
}

// One step: the table row of photo_wrk.mode, the game drawn first unless
// its draw flags are -1. 1 only from step 14, when it is over; 0 while no
// row matches.
// FF3 0x002285b8

int PhotoMain(void)
{
    int mode = photo_wrk.mode;
    int ret;

    for (const PHOTO_STEP *row = photo_step_tbl; row->mode >= 0; row++)
    {
        if (row->mode != mode)
            continue;

        if (row->draw != -1)
            IngameDrawSub((u_int)row->draw);
        ret = row->func();
        return mode == 0xe ? ret : 0;
    }
    return 0;
}

// Phase 33 init: remembers the requesting phase and freezes the effects.
// FF3 0x00228638

int init_Story_Photo(void)
{
    photo_wrk.back = GetGPhaseCtrl()->from;
    eff_wrk.stop_flg = 1;
    return 1;
}

// FF3 0x00228688

int end_Story_Photo(void)
{
    return 1;
}

// Phase 33 one: a step of the sequence, the screen flash drawn while it
// runs; when it is over, back to the phase that asked for the photo.
// FF3 0x00228690

int one_Story_Photo(void)
{
    if (PhotoMain() == 0)
    {
        ScreenFlashDraw();
        return 0;
    }

    _SetPREVIOUSTRI2PRIM(nullptr);
    GPhaseCtrlRequest(photo_wrk.back, 0);
    return 1;
}
