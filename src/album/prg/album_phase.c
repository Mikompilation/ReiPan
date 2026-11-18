/* ==========================================================================
 *  album/prg/album_phase.c
 *
 *  The album's memory-card phases (FF3 0x128a58..0x128e30, FF3-only; see
 *  album_phase.h): 27 Album_Save and 28 Album_Load. Both run the same
 *  sequence: the card machine's and the screen's init, the screen's "in",
 *  then screen + machine every frame until the machine is finished, the
 *  screen's "out", release, give the card back, and SetNext to the phase
 *  that linked them (GPhaseCtrl.from at init) with its fade layer.
 *  FF3 0x00128e10 is not written: the compiler's static-destruction thunk
 *  (GPhaseCtrlTerm on the GPhaseCtrl singleton), as in candle_ctrl.c.
 * ======================================================================== */
#include "album/prg/album_phase.h"

#include "album/prg/album_edit.h"       // AlbumGetOpenArg
#include "album/prg/album_load.h"       // AlbumLoadCtrlInit / Main / End, album_load_ctrl
#include "album/prg/album_load_scr.h"   // AlbumLoadScr*
#include "album/prg/album_save.h"       // AlbumSaveCtrlInit / Main / End
#include "album/prg/album_save_scr.h"   // AlbumSaveScr*
#include "album/prg/album_view.h"       // AlbumViewUpdatePages, AlbumViewSetFadeLayer
#include "main/gphase.h"

// Phase 27 / 28's step (sbss 0x372c84, a byte).
enum
{
    ALBUM_PHASE_INIT      = 0,      // the machine's and the screen's init
    ALBUM_PHASE_IN        = 1,      // the screen's "in"
    ALBUM_PHASE_IN_WAIT   = 2,
    ALBUM_PHASE_MAIN      = 3,      // screen + machine until the machine returns 1
    ALBUM_PHASE_OUT_WAIT  = 4,      // the screen's "out"
    ALBUM_PHASE_END       = 5,      // card back, return to album_phase_return
};

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

static int    album_load_result;    // sdata 0x36b938
static int    album_phase_return;   // sbss 0x372c80: GetGPhaseCtrl()->from at init
static s_char album_phase_step;     // sbss 0x372c84: ALBUM_PHASE_*

// ──────────────────────────────────────────────────────────────────────
// The load result
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00128a58.
void AlbumLoadResultSet(int loaded)
{
    album_load_result = loaded;
}

// FF3 0x00128a60.
int AlbumLoadResultGet(void)
{
    return album_load_result;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 27, Album_Save
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00128a68.
int init_Album_Save(void)
{
    album_phase_step   = ALBUM_PHASE_INIT;
    album_phase_return = GetGPhaseCtrl()->from;
    return 1;
}

// FF3 0x00128aa8.
int end_Album_Save(void)
{
    return 1;
}

// FF3 0x00128ab8.
int one_Album_Save(void)
{
    if (album_phase_step == ALBUM_PHASE_INIT)
    {
        AlbumSaveCtrlInit();
        AlbumSaveScrInit();
        album_phase_step = ALBUM_PHASE_IN;
    }
    if (album_phase_step == ALBUM_PHASE_IN && AlbumSaveScrIn(AlbumGetOpenArg()))
        album_phase_step = ALBUM_PHASE_IN_WAIT;
    if (album_phase_step == ALBUM_PHASE_IN_WAIT && AlbumSaveScrIsInEnd())
        album_phase_step = ALBUM_PHASE_MAIN;
    if (album_phase_step == ALBUM_PHASE_MAIN)
    {
        AlbumSaveScrMain();
        if (AlbumSaveMain())
        {
            AlbumSaveScrClose();
            album_phase_step = ALBUM_PHASE_OUT_WAIT;
        }
    }
    if (album_phase_step == ALBUM_PHASE_OUT_WAIT && AlbumSaveScrOutWait())
    {
        AlbumSaveScrRelease();
        album_phase_step = ALBUM_PHASE_END;
    }

    if (album_phase_step != ALBUM_PHASE_END)
        return 0;

    AlbumSaveEnd();
    GetGPhaseCtrl()->SetNext((short)album_phase_return, 0);
    AlbumViewSetFadeLayer(album_phase_return);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 28, Album_Load
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00128c28.
int init_Album_Load(void)
{
    album_phase_step   = ALBUM_PHASE_INIT;
    album_phase_return = GetGPhaseCtrl()->from;
    return 1;
}

// FF3 0x00128c68.
int end_Album_Load(void)
{
    return 1;
}

// As one_Album_Save over the load machine, with the album pages kept up
// behind the screen; a finished load (album_load_ctrl.load_done) sets the
// result Album_Open reads.
// FF3 0x00128c78.
int one_Album_Load(void)
{
    AlbumViewUpdatePages(1, 0);

    if (album_phase_step == ALBUM_PHASE_INIT)
    {
        AlbumLoadCtrlInit();
        AlbumLoadScrInit();
        album_phase_step = ALBUM_PHASE_IN;
    }
    if (album_phase_step == ALBUM_PHASE_IN && AlbumLoadScrIn(AlbumGetOpenArg()))
        album_phase_step = ALBUM_PHASE_IN_WAIT;
    if (album_phase_step == ALBUM_PHASE_IN_WAIT && AlbumLoadScrIsInEnd())
        album_phase_step = ALBUM_PHASE_MAIN;
    if (album_phase_step == ALBUM_PHASE_MAIN)
    {
        AlbumLoadScrMain();
        if (AlbumLoadMain())
        {
            AlbumLoadScrClose();
            album_phase_step = ALBUM_PHASE_OUT_WAIT;
        }
    }
    if (album_phase_step == ALBUM_PHASE_OUT_WAIT && AlbumLoadScrOutWait())
    {
        AlbumLoadScrRelease();
        album_phase_step = ALBUM_PHASE_END;
    }

    if (album_phase_step != ALBUM_PHASE_END)
        return 0;

    AlbumLoadEnd();
    if (album_load_ctrl.load_done != 0)
        AlbumLoadResultSet(1);
    GetGPhaseCtrl()->SetNext((short)album_phase_return, 0);
    AlbumViewSetFadeLayer(album_phase_return);
    return 1;
}
