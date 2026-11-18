/* ==========================================================================
 *  album/prg/album_zoom_view.c
 *
 *  The enlarged photo page's layout (see album_zoom_view.h). FF3
 *  0x12e798..0x12f470, FF3-only: the mount papers' loads, the photo and
 *  marks, the info hooks, and the page's in / out / sort animations. The
 *  hooks are twins of album_view.c's edit page ones, but print in the album
 *  kind's ink colour at the item's own alpha. The matcher's FF2 name for
 *  0x12e8d0 (AlbumViewFrameDisp, a sprite loop) was a low link-order guess.
 * ======================================================================== */
#include "album/prg/album_zoom_view.h"

#include <string.h>                     // strlen, strncmp

#include "album/prg/album.h"            // AlbumInfoGetKind, AlbumPhotoIs*, AlbumDispCtrlInit
#include "album/prg/album_disp.h"       // AlbumDispGetCur*, AlbumPhotoPrint*, AlbumPhotoDisp
#include "album/prg/album_view.h"       // album_layout_obj, AlbumViewUpdatePages
#include "graphics/graph2d/message.h"   // SetCurColorRGB
#include "graphics/graph2d/tim2.h"      // Tim2GetFirstPicture
#include "graphics/gs_reg.h"            // GetGsRegCached, SetGsRegCached
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, SetCurColorPacked, SetDefaultGsDrawState
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dGetDrawItem, B2D_PART_*
#include "system/b2d/B2dSpr.h"          // B2dSprSetDisp
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo, B2dDrawSpriteDirect
#include "system/b2d/BpObj.h"           // BP_OBJ
#include "system/b2d/BpTex.h"           // BpTexGetTim2, BpTexGetTex0
#include "system/eeiop/cd_file_dat.h"   // ALBM_KAKUDAI_PHF, ALBM0_DAISI*_TM2
#include "system/eeiop/fileload.h"      // FileLoadReqEE, FileLoadIsEnd, FileLoadCancel
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define AZV_PAPER_TEX_1     1000        // ALBM_KAKUDAI textures the papers replace
#define AZV_PAPER_TEX_2     0x3ea
#define AZV_PAPER_PRIORITY  5           // FileLoadReqEE priority
#define AZV_ALPHA_IN        0x10        // paper alpha step while both are in ...
#define AZV_ALPHA_OUT       0x20        // ... and while one is not
#define AZV_ALPHA_MAX       0x80
#define AZV_KIND_NUM        9           // album kinds with a paper and an ink colour

// Items of ALBM_KAKUDAI_PHF.
enum
{
    AZV_ITEM_PHOTO     = 0x3fe,         // the photo (AlbumZoomViewDrawPhoto)
    AZV_ITEM_PROTECT   = 0x45e,         // the protect mark
    AZV_ITEM_FLAG3_OFF = 0x468,         // one of these two by AlbumPhotoIsFlag3
    AZV_ITEM_FLAG3_ON  = 0x467,
    AZV_ITEM_ROOM      = 0x41f,
    AZV_ITEM_DATE      = 0x41e,
    AZV_ITEM_SCORE_A   = 0x41d,         // "NUM.." the digits, else the label
    AZV_ITEM_SCORE_B   = 0x41c,
    AZV_ITEM_SUBJECT_1 = 0x41b,         // names end in '1'..'3'
    AZV_ITEM_SUBJECT_2 = 0x41a,
    AZV_ITEM_SUBJECT_3 = 0x417,
    AZV_ITEM_TEX1_A    = 0x418,         // drawn with TEX1_1 0x100000141
    AZV_ITEM_TEX1_B    = 0x419,
};

static const int azv_flag3_item[2] = { AZV_ITEM_FLAG3_OFF, AZV_ITEM_FLAG3_ON };  // sdata 0x36ba48

// The text ink per album kind (built on the stack by FF3).
static const u_char azv_kind_rgb[AZV_KIND_NUM][3] =
{
    { 0x0a, 0x0a, 0x0a }, { 0x03, 0x12, 0x16 }, { 0x1a, 0x0a, 0x06 },
    { 0x1b, 0x14, 0x06 }, { 0x15, 0x1b, 0x11 }, { 0x18, 0x1c, 0x22 },
    { 0x26, 0x1b, 0x26 }, { 0x34, 0x23, 0x1b }, { 0x27, 0x17, 0x19 },
};

// By album side (sdata 0x36ba58..0x36ba6c).
static const char *const azv_label_in[ALBUM_SIDE_NUM]  = { "kakudaiB_in",  "kakudaiA_in"  };
static const char *const azv_label_out[ALBUM_SIDE_NUM] = { "kakudaiB_out", "kakudaiA_out" };
static const char *const azv_label_sort[2] = { "kakudai_sortoff", "kakudai_sorton" };

static ALBUM_ZOOM_PAPER azv_paper[2];           // bss 0x390c60
static u_char           azv_paper_alpha;        // bss 0x390c70: 0 lets a new paper load start
static int              azv_flag3_disp[2] = { 1, 0 };  // sdata 0x36ba40: AZV_ITEM_FLAG3_OFF / _ON shown

static int AlbumZoomViewHandle(void)
{
    return BpCtlGetHandleByFileNo(GetSysWrk()->GetLanguage() + ALBM_KAKUDAI_PHF);
}

// The text item value the info hooks pass on (EE item +0x124).
static int AlbumZoomViewDrawPri(void)
{
    return B2dGetDrawItem()->part[B2D_PART_TEXT].value;
}

// ──────────────────────────────────────────────────────────────────────
// The mount papers
// ──────────────────────────────────────────────────────────────────────

// Loads file over texture tex of the layout layout_file; returns the load id.
// FF3 0x0012e798.
static int AlbumZoomViewPaperLoadReq(int layout_file, int tex, int file)
{
    int handle = BpCtlGetHandleByFileNo(layout_file);

    return FileLoadReqEE(file, BpTexGetTim2(handle, tex), AZV_PAPER_PRIORITY, nullptr, nullptr);
}

// Once loaded, the new TIM2 is sent where the texture it replaced lived.
// FF3 0x0012e7e8.
static int AlbumZoomViewPaperLoadIsEnd(int layout_file, int tex, int id)
{
    int end = FileLoadIsEnd(id);

    if (end)
    {
        int handle = BpCtlGetHandleByFileNo(layout_file);
        Tim2GetFirstPicture(BpTexGetTim2(handle, tex))->GsTex0 = BpTexGetTex0(handle, tex);
    }
    return end != 0;
}

// FF3 0x0012e870.
static void AlbumZoomViewPaperInit(ALBUM_ZOOM_PAPER *paper)
{
    paper->load_id = -1;
    paper->file    = -1;
}

// FF3 0x0012e880.
static void AlbumZoomViewPaperCancel(ALBUM_ZOOM_PAPER *paper)
{
    if (paper->load_id != -1)
    {
        FileLoadCancel(paper->load_id, nullptr, nullptr);
        paper->load_id = -1;
    }
    paper->file = -1;
}

// Per frame: a new file starts loading once nothing is loading and busy is
// 0; 1 when the file asked for is in (not yet on the frame it arrives).
// FF3 0x0012e8d0 (was FF2's AlbumViewFrameDisp, a wrong low match).
static int AlbumZoomViewPaperStep(ALBUM_ZOOM_PAPER *paper, int layout_file, int tex, int file, u_char busy)
{
    int id = paper->load_id;

    if (id == -1)
    {
        if (paper->file == file)
        {
            if (file != -1)
                return 1;
        }
        else if (busy == 0)
        {
            paper->file = file;
            if (file != -1)
            {
                id             = AlbumZoomViewPaperLoadReq(layout_file, tex, file);
                paper->load_id = id;
            }
        }
    }
    if (id != -1 && AlbumZoomViewPaperLoadIsEnd(layout_file, tex, id))
        paper->load_id = -1;
    return 0;
}

// FF3 0x0012e988.
static void AlbumZoomViewPapersInit(void)
{
    AlbumZoomViewPaperInit(&azv_paper[0]);
    AlbumZoomViewPaperInit(&azv_paper[1]);
    azv_paper_alpha = 0;
}

// FF3 0x0012e9c0.
static void AlbumZoomViewPapersCancel(void)
{
    AlbumZoomViewPaperCancel(&azv_paper[0]);
    AlbumZoomViewPaperCancel(&azv_paper[1]);
    azv_paper_alpha = 0;
}

// Both papers of album kind; 1 when both are in. The alpha climbs while
// they are and falls while one is not, and holds a new load back until 0.
// FF3 0x0012e9f8.
// callcheck: GetSysWrk, GetLanguage (twice in FF3)
static int AlbumZoomViewPapersStep(s_char kind)
{
    int layout = GetSysWrk()->GetLanguage() + ALBM_KAKUDAI_PHF;
    int in1    = AlbumZoomViewPaperStep(&azv_paper[0], layout, AZV_PAPER_TEX_1, ALBM0_DAISI1_TM2 + kind, azv_paper_alpha);
    layout     = GetSysWrk()->GetLanguage() + ALBM_KAKUDAI_PHF;
    int in2    = AlbumZoomViewPaperStep(&azv_paper[1], layout, AZV_PAPER_TEX_2, ALBM0_DAISI2_TM2 + kind, azv_paper_alpha);
    int in     = in1 != 0 && in2 != 0;

    if (in)
    {
        azv_paper_alpha = (azv_paper_alpha + AZV_ALPHA_IN < AZV_ALPHA_MAX) ? (u_char)(azv_paper_alpha + AZV_ALPHA_IN) : AZV_ALPHA_MAX;
    }
    else
    {
        azv_paper_alpha = (azv_paper_alpha - AZV_ALPHA_OUT > 0) ? (u_char)(azv_paper_alpha - AZV_ALPHA_OUT) : 0;
    }
    return in;
}

// ──────────────────────────────────────────────────────────────────────
// The photo and its marks
// ──────────────────────────────────────────────────────────────────────

// Item 0x3fe (a text item: a1 w, a2 h, t1 xy, stack[2] alpha): the current
// photo.
// FF3 0x0012eaf0 (no Ghidra function before 2026-10-21: inside 0x12e9f8).
// callcheck: B2dGetDrawItem (AlbumZoomViewDrawPri)
static int AlbumZoomViewDrawPhoto(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumZoomViewDrawPri();

    SetCurColorPacked((u_int)args->t3);
    s_char side = AlbumDispGetCurAlbum();
    s_char no   = AlbumDispGetCurPhoto();
    AlbumPhotoDisp(side, no, xy[0], xy[1], (int)args->a1, (int)args->a2, (u_char)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

static const B2D_HOOK_REC azv_photo_hooks[] =     // built on the stack by FF3
{
    { AlbumZoomViewDrawPhoto, 0, AZV_ITEM_PHOTO, nullptr },
    { nullptr,               -1, -1,             nullptr },
};

// FF3 0x0012eb90.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumZoomViewHandle)
static void AlbumZoomViewSetPhotoHook(void)
{
    B2dSetHookList(AlbumZoomViewHandle(), azv_photo_hooks);
    AlbumDispCtrlInit();
}

// One of the two flag3 items, as the photo's flag says (kept while the
// slot is empty).
// FF3 0x0012ec28.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumZoomViewHandle)
static void AlbumZoomViewShowFlag3(void)
{
    int    handle = AlbumZoomViewHandle();
    s_char side   = AlbumDispGetCurAlbum();
    s_char no     = AlbumDispGetCurPhoto();

    if (AlbumPhotoIsUsed(side, no))
    {
        int flag3 = AlbumPhotoIsFlag3(side, no) != 0;
        azv_flag3_disp[1] = flag3;
        azv_flag3_disp[0] = !flag3;
    }
    for (s_char i = 0; i < 2; i++)
        B2dSprSetDisp(B2dCtlFindItem(handle, azv_flag3_item[i]), azv_flag3_disp[i]);
}

// FF3 0x0012ed40.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumZoomViewHandle)
static void AlbumZoomViewShowProtect(void)
{
    B2D_ITEM *item = B2dCtlFindItem(AlbumZoomViewHandle(), AZV_ITEM_PROTECT);

    B2dSprSetDisp(item, AlbumPhotoIsProtected(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto()));
}

// ──────────────────────────────────────────────────────────────────────
// The photo's info (ALBM_KAKUDAI text items)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012edb8. FF3 tests kind < 9 only; a negative kind (no album)
// would read the stack below the table, so the host skips it too.
static void AlbumZoomViewSetKindColor(s_char kind)
{
    if (kind >= 0 && kind < AZV_KIND_NUM)
        SetCurColorRGB(azv_kind_rgb[kind][0], azv_kind_rgb[kind][1], azv_kind_rgb[kind][2]);
}

// Item 0x41f: the room the photo was taken in.
// FF3 0x0012ee48.
// callcheck: B2dGetDrawItem (AlbumZoomViewDrawPri)
static int AlbumZoomViewDrawRoom(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumZoomViewDrawPri();
    s_char     side = AlbumDispGetCurAlbum();
    s_char     no   = AlbumDispGetCurPhoto();

    AlbumZoomViewSetKindColor(AlbumInfoGetKind(side));
    AlbumPhotoPrintRoom(side, no, xy[0], xy[1], (int)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

// Item 0x41e: the date (a1, the item's w, is the character step).
// FF3 0x0012eed0.
// callcheck: B2dGetDrawItem (AlbumZoomViewDrawPri)
static int AlbumZoomViewDrawDate(const B2D_HOOK_ARGS *args)
{
    const int *xy  = (const int *)args->t1;
    int        pri = AlbumZoomViewDrawPri();
    s_char     side = AlbumDispGetCurAlbum();
    s_char     no   = AlbumDispGetCurPhoto();

    AlbumZoomViewSetKindColor(AlbumInfoGetKind(side));
    AlbumPhotoPrintDate(side, no, xy[0], xy[1], (int)args->a1, (int)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x41d / 0x41c: the score digits ("NUM.." items) or its label.
// FF3 0x0012ef68.
// callcheck: B2dGetDrawItem (AlbumZoomViewDrawPri)
static int AlbumZoomViewDrawScore(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         pri  = AlbumZoomViewDrawPri();
    s_char      side = AlbumDispGetCurAlbum();
    s_char      no   = AlbumDispGetCurPhoto();

    AlbumZoomViewSetKindColor(AlbumInfoGetKind(side));
    if (strncmp(name, "NUM", 3) == 0)       // sdata 0x36ba50
        AlbumPhotoPrintScore(side, no, xy[0], xy[1], (u_char)args->stack[2], pri);
    else
        AlbumPhotoPrintScoreLabel(side, no, xy[0], xy[1], (int)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x41b / 0x41a / 0x417: the photo's subjects, the name's last digit
// ('1'..) picking which.
// FF3 0x0012f048 (no Ghidra function before 2026-10-21: inside 0x12ef68).
// callcheck: B2dGetDrawItem (AlbumZoomViewDrawPri)
static int AlbumZoomViewDrawSubject(const B2D_HOOK_ARGS *args)
{
    const char *name = (const char *)args->a0;
    const int  *xy   = (const int *)args->t1;
    int         pri  = AlbumZoomViewDrawPri();
    s_char      side = AlbumDispGetCurAlbum();
    s_char      no   = AlbumDispGetCurPhoto();

    AlbumZoomViewSetKindColor(AlbumInfoGetKind(side));
    AlbumPhotoPrintSubject(name[strlen(name) - 1] - '1', side, no, xy[0], xy[1], (int)args->stack[2], pri);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x418 / 0x419: the sprite with TEX1_1 0x100000141, the old TEX1
// restored (album_view.c's fade item does the same).
// FF3 0x0012f0f0.
static int AlbumZoomViewDrawTex1Item(const B2D_HOOK_ARGS *args)
{
    u_long tex1 = *GetGsRegCached(0x14);

    SetGsRegCached(0x100000141ULL, 0x14, 1);    // TEX1_1
    int ret = B2dDrawSpriteDirect(args);
    SetGsRegCached(tex1, 0x14, 1);
    return ret;
}

static const B2D_HOOK_REC azv_info_hooks[] =      // ROM 0x34ead0
{
    { AlbumZoomViewDrawRoom,     0, AZV_ITEM_ROOM,      nullptr },
    { AlbumZoomViewDrawDate,     0, AZV_ITEM_DATE,      nullptr },
    { AlbumZoomViewDrawScore,    0, AZV_ITEM_SCORE_A,   nullptr },
    { AlbumZoomViewDrawScore,    0, AZV_ITEM_SCORE_B,   nullptr },
    { AlbumZoomViewDrawSubject,  0, AZV_ITEM_SUBJECT_1, nullptr },
    { AlbumZoomViewDrawSubject,  0, AZV_ITEM_SUBJECT_2, nullptr },
    { AlbumZoomViewDrawSubject,  0, AZV_ITEM_SUBJECT_3, nullptr },
    { AlbumZoomViewDrawTex1Item, 0, AZV_ITEM_TEX1_A,    nullptr },
    { AlbumZoomViewDrawTex1Item, 0, AZV_ITEM_TEX1_B,    nullptr },
    { nullptr,                  -1, -1,                 nullptr },
};

// FF3 copies the ROM list to the stack first.
// FF3 0x0012f1f0.
// callcheck: BpCtlGetHandleByFileNo, GetLanguage, GetSysWrk (AlbumZoomViewHandle)
static void AlbumZoomViewSetInfoHooks(void)
{
    B2dSetHookList(AlbumZoomViewHandle(), azv_info_hooks);
}

// ──────────────────────────────────────────────────────────────────────
// The page's life
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0012f270.
void AlbumZoomViewInit(void)
{
    AlbumZoomViewPapersInit();
}

// Waits for the papers of the shown album's kind, then plays the side's
// "in" and sets the hooks and marks.
// FF3 0x0012f288.
int AlbumZoomViewIn(void)
{
    s_char side = AlbumDispGetCurAlbum();
    int    in   = AlbumZoomViewPapersStep(AlbumInfoGetKind(side));

    if (in)
    {
        album_layout_obj[ALBUM_LAYOUT_KAKUDAI]->In(azv_label_in[side]);
        AlbumZoomViewSetInfoHooks();
        AlbumZoomViewSetPhotoHook();
        AlbumZoomViewShowFlag3();
        AlbumZoomViewShowProtect();
    }
    return in;
}

// FF3 0x0012f320.
int AlbumZoomViewIsInEnd(void)
{
    AlbumZoomViewPapersStep(AlbumInfoGetKind(AlbumDispGetCurAlbum()));
    return album_layout_obj[ALBUM_LAYOUT_KAKUDAI]->IsInEnd() != 0;
}

// FF3 0x0012f360.
void AlbumZoomViewUpdate(void)
{
    AlbumViewUpdatePages(0, 0);
    AlbumZoomViewShowFlag3();
    AlbumZoomViewShowProtect();
}

// FF3 0x0012f390.
int AlbumZoomViewOut(void)
{
    return album_layout_obj[ALBUM_LAYOUT_KAKUDAI]->Out(azv_label_out[AlbumDispGetCurAlbum()]) != 0;
}

// FF3 0x0012f3e0.
void AlbumZoomViewRelease(void)
{
    AlbumZoomViewPapersCancel();
}

// The sort list's animation on ALBM_CMN; 1 (and reset) once played.
// FF3 0x0012f3f8.
int AlbumZoomViewPlaySort(int on)
{
    BP_OBJ *cmn  = album_layout_obj[ALBUM_LAYOUT_CMN];
    int     done = cmn->PlayOnce(azv_label_sort[on]);

    if (done)
        cmn->PlayOnceReset();
    return done != 0;
}
