/* ==========================================================================
 *  album/prg/album_zoom.c
 *
 *  The album's enlarged photo page (see album_zoom.h): phases 53 Album_Zoom,
 *  79 Album_ZoomSort and 106 Album_ZoomConfirm. FF3-only; the page itself
 *  (the mount papers, the photo, its info lines) is album_zoom_view.c
 *  (0x12e798..0x12f470), the sort list and the confirm are twins of
 *  album_edit.c's Album_Sort and Album_Confirm.
 *  FF3 0x00126f48 is not written: the compiler's static-destruction thunk
 *  (GPhaseCtrlTerm on the GPhaseCtrl singleton), as in album_phase.c.
 * ======================================================================== */
#include "album/prg/album_zoom.h"

#include "album/prg/album.h"            // AlbumPhotoProtectToggle
#include "album/prg/album_disp.h"       // AlbumDispGetCurAlbum / Photo, AlbumEditCsrUpdate
#include "album/prg/album_edit.h"       // AlbumKeyCsr*, AlbumEditErase*, AlbumEditSort, album_key_sel
#include "album/prg/album_view.h"       // AlbumViewPageCsrFreeze, AlbumViewSort*, AlbumViewSetFadeLayer
#include "album/prg/album_zoom_view.h"  // AlbumZoomView*
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsg*, Anim2dMsg*
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "main/gphase.h"
#include "system/pad/pad_ctrl.h"        // pad_key, CheckPadTrgPlaySe

#define ALBUM_SE_CANCEL     0x1a9b0001  // as album_edit.c
#define ALBUM_SE_BUZZ       0x1a9b0002
#define ALBUM_SE_OK         0x1a9b0003
#define ALBUM_SE_PROTECT    0x1a9b0005

#define ALBUM_MSG           0x59        // the album's message type

#define ALBUM_ZOOM_KEY_CSR  0x4f        // MenuCsr id of the zoom's sort keys

#define ALBUM_ZOOM_MSG_ERASE      0x18  // ALBUM_MSG ids: erase the photo? (yes / no)
#define ALBUM_ZOOM_MSG_PROTECTED  0x36  //   the photo is protected

// AlbumZoomConfirmReq's mode: a yes / no cursor (its default row, 1 no) or a
// message alone.
#define ALBUM_ZOOM_CONF_CHOICE    1
#define ALBUM_ZOOM_CONF_MESSAGE   2

#define PHASE_ALBUM_MENU          0x1a  // phases (architecture.md "phases")
#define PHASE_ALBUM_ZOOM          0x35
#define PHASE_ALBUM_ZOOM_SORT     0x4f
#define PHASE_ALBUM_ZOOM_CONFIRM  0x6a

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static s_char album_zoom_conf;          // sbss 0x372c68: ALBUM_ZOOM_CONF_*, then the answer (0 yes)
static int    album_zoom_conf_msg;      // sbss 0x372c6c: the confirm's ALBUM_MSG id
static u_char album_zoom_erase;         // sbss 0x372c70: 1 while the erase question is up
static u_char album_zoom_key_mask;      // sbss 0x372c71: enabled sort keys
static int    album_zoom_conf_anim;     // sbss 0x372c74: Album_ZoomConfirm's LayoutAnim slot

// ──────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────

// OK on the enlarged photo: flips its protection, with the SE when it did.
// FF3 0x00126738.
static int AlbumZoomProtectToggle(void)
{
    int r = AlbumPhotoProtectToggle(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto());

    if (r != 0)
        SndSePlay(ALBUM_SE_PROTECT, 100, 0x1000, 0, nullptr, 0);
    return r != 0;
}

// Album_ZoomConfirm on message msg, linked under the caller.
// FF3 0x001267a0.
static void AlbumZoomConfirmReq(s_char msg, s_char mode)
{
    album_zoom_conf_msg = msg;
    album_zoom_conf     = mode;
    GetGPhaseCtrl()->SetNext(PHASE_ALBUM_ZOOM_CONFIRM, 1);
    AlbumViewSetFadeLayer(PHASE_ALBUM_ZOOM_CONFIRM);
}

// ──────────────────────────────────────────────────────────────────────
// Phase 53, Album_Zoom
// ──────────────────────────────────────────────────────────────────────

// The view's init, its "in" once its frames are loaded, the end of the "in".
// FF3 0x001267f8.
int init_Album_Zoom(void)
{
    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        AlbumZoomViewInit();
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }
    if (GetGPhaseCtrl()->leaf->state_cnt == 1 && AlbumZoomViewIn())
        GetGPhaseCtrl()->leaf->state_cnt = 2;

    if (GetGPhaseCtrl()->leaf->state_cnt != 2 || !AlbumZoomViewIsInEnd())
        return 0;
    album_zoom_erase = 0;
    return 1;
}

// FF3 0x00126900.
int end_Album_Zoom(void)
{
    int done = AlbumZoomViewOut();

    if (done)
        AlbumZoomViewRelease();
    return done != 0;
}

// FF3 0x00126930.
void pre_Album_Zoom(void)
{
}

// The enlarged photo: an answered erase question first, then the cursor
// (kept on this side), circle erase (asks, or says it is protected), OK
// protect, cancel back to the menu, square the sort list.
// FF3 0x00126938.
// callcheck: GPhaseCtrlSetNext (GPHASE_CTRL::SetNext)
int one_Album_Zoom(void)
{
    int r;

    AlbumZoomViewUpdate();

    if (album_zoom_erase == 1)
    {
        if (album_zoom_conf == 0)
            AlbumEditEraseExec();
        album_zoom_erase = 0;
    }
    if (album_zoom_erase != 0)
        return 0;

    AlbumViewPageCsrFreeze(0, 1);
    AlbumViewPageCsrFreeze(1, 1);

    if (AlbumEditCsrUpdate(1))
    {
        AlbumViewSelectSlot(AlbumDispGetCurAlbum(), AlbumDispGetCurPhoto());
        return 0;
    }
    if (CheckPadTrgPlaySe(0x0020, -1, 0))                               // circle
    {
        r = AlbumEditEraseReq();
        if (r == 0)
            return 0;
        if (r == 1)
        {
            album_zoom_erase = 1;
            AlbumZoomConfirmReq(ALBUM_ZOOM_MSG_ERASE, ALBUM_ZOOM_CONF_CHOICE);
        }
        else
        {
            AlbumZoomConfirmReq(ALBUM_ZOOM_MSG_PROTECTED, ALBUM_ZOOM_CONF_MESSAGE);
        }
        return 1;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
    {
        AlbumZoomProtectToggle();
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
    {
        GetGPhaseCtrl()->SetNext(PHASE_ALBUM_MENU, 0);
        AlbumViewSetFadeLayer(PHASE_ALBUM_MENU);
        AlbumViewPageCsrFreeze(0, 0);
        AlbumViewPageCsrFreeze(1, 0);
        return 1;
    }
    if (!CheckPadTrgPlaySe(0x0080, ALBUM_SE_OK, 0))                     // square
        return 0;

    GetGPhaseCtrl()->SetNext(PHASE_ALBUM_ZOOM_SORT, 0);
    AlbumViewSetFadeLayer(PHASE_ALBUM_ZOOM_SORT);
    return 1;
}

// FF3 0x00126b20.
void after_Album_Zoom(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Phase 79, Album_ZoomSort
// ──────────────────────────────────────────────────────────────────────

// As album_edit.c's key row check, on the zoom's key mask: -1 no key at
// all, 0 the key under the cursor is off, 1 on.
// FF3 0x00126b28.
static int AlbumZoomKeyRowCheck(void)
{
    if (album_zoom_key_mask == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return -1;
    }
    if (((album_zoom_key_mask >> (album_key_sel & 0x1f)) & 1) == 0)
    {
        SndSePlay(ALBUM_SE_BUZZ, 100, 0x1000, 0, nullptr, 0);
        return 0;
    }
    SndSePlay(ALBUM_SE_OK, 100, 0x1000, 0, nullptr, 0);
    return 1;
}

// FF3 0x00126bd0.
int init_Album_ZoomSort(void)
{
    AlbumViewPageCsrFreeze(0, 0);
    AlbumViewPageCsrFreeze(1, 0);

    if (GetGPhaseCtrl()->leaf->state_cnt == 0)
    {
        u_char mask = (u_char)AlbumMenuCondSortKeys();

        album_zoom_key_mask = mask;
        AlbumKeyCsrInit(ALBUM_ZOOM_KEY_CSR, mask);
        AlbumViewSortCsrSet(album_key_sel);
        AlbumViewSortRowSetRgb(album_zoom_key_mask);
        GetGPhaseCtrl()->leaf->state_cnt = 1;
    }

    if (GetGPhaseCtrl()->leaf->state_cnt != 1)
        return 0;
    return AlbumZoomViewPlaySort(1) != 0;
}

// FF3 0x00126cb8.
int end_Album_ZoomSort(void)
{
    int done = AlbumZoomViewPlaySort(0);

    if (done)
        AlbumKeyCsrRelease(ALBUM_ZOOM_KEY_CSR);
    return done != 0;
}

// FF3 0x00126cf0.
void pre_Album_ZoomSort(void)
{
}

// The key cursor; OK sorts the side by the key (again: the other way) and
// refreshes the enlarged photo, cancel goes back to Album_Zoom.
// FF3 0x00126cf8.
// callcheck: GPhaseCtrlSetNext (GPHASE_CTRL::SetNext)
int one_Album_ZoomSort(void)
{
    int r;

    if (AlbumKeyCsrUpdate(ALBUM_ZOOM_KEY_CSR))
    {
        AlbumViewSortCsrSet(album_key_sel);
        return 0;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], ALBUM_SE_CANCEL, 0))
    {
        GetGPhaseCtrl()->SetNext(PHASE_ALBUM_ZOOM, 0);
        AlbumViewSetFadeLayer(PHASE_ALBUM_ZOOM);
        return 1;
    }
    if (!CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0))
        return 0;

    r = AlbumZoomKeyRowCheck();
    if (r == 0)
        return 0;
    if (r != 1)
        return 1;
    if (AlbumEditSort(album_key_sel))
        AlbumZoomViewUpdate();
    return 0;
}

// FF3 0x00126de0.
void after_Album_ZoomSort(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Phase 106, Album_ZoomConfirm
// ──────────────────────────────────────────────────────────────────────

// A message alone opens the plain message window (layout type 3), a
// question the choice window (type 5) with the cursor on the mode's row.
// FF3 0x00126de8.
int init_Album_ZoomConfirm(void)
{
    LayoutAnimMain();
    AlbumViewPageCsrFreeze(0, 0);
    AlbumViewPageCsrFreeze(1, 0);

    if (album_zoom_conf == ALBUM_ZOOM_CONF_MESSAGE)
    {
        album_zoom_conf_anim = LayoutAnimStart(3, 0);
        LayoutMsgSet(ALBUM_MSG, album_zoom_conf_msg);
        LayoutMsgSetType(2);
    }
    else
    {
        album_zoom_conf_anim = LayoutAnimStart(5, 0);
        Anim2dMsgSet(ALBUM_MSG, album_zoom_conf_msg);
        Anim2dMsgSetMode(1, album_zoom_conf);
    }
    LayoutMsgSetArg(1);
    return 1;
}

// FF3 0x00126e90.
int end_Album_ZoomConfirm(void)
{
    LayoutAnimReset(album_zoom_conf_anim);
    return 1;
}

// When the window has closed (state 4): the answer (the choice cursor, 0
// yes) and back to the phase that asked.
// FF3 0x00126eb8.
// callcheck: GPhaseCtrlSetNext (GPHASE_CTRL::SetNext)
int one_Album_ZoomConfirm(void)
{
    LayoutAnimMain();
    if (LayoutAnimGetState(album_zoom_conf_anim) != 4)
        return 0;

    album_zoom_conf = (s_char)Anim2dMsgGetCsr();
    GetGPhaseCtrl()->SetNext(GetGPhaseCtrl()->from, 0);
    AlbumViewSetFadeLayer(GetGPhaseCtrl()->from);
    return 1;
}
