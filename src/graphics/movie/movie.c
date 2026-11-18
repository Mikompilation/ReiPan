/* ==========================================================================
 *  graphics/movie/movie.c
 *
 *  The PSS movie player's game-side wrapper (see movie.h). playpss does the
 *  decoding; this file owns the buffers around it, the start / per-frame draw
 *  / end of a movie and the frame counter, plus FF3's phase 39
 *  Story_SceneMovie.
 *  FF3 0x217c90..0x218678. Ported from MioPan's movie.c (FF2), turned into
 *  FF3's: FF2's InitMovie allocated each buffer with mem_utilGetMem and
 *  started the stream; FF3 carves the EE buffers out of one BORROW_BUF
 *  (InitMovie), checks them (MovieRsrcsCheckLack) and starts the stream in
 *  StartMovie, so a movie that finds no memory is refused rather than
 *  asserted. FF2's PlayMovie / *WithTitle / MovieCancel are gone; FF3's
 *  MovieDispMain draws the frame over a black screen at the decoded size.
 *  FF2's debug printfs are compiled out of FF3 (iopalloc keeps its unused
 *  description argument).
 * ======================================================================== */
#include "graphics/movie/movie.h"

#include <new>                          // placement new
#include <stdint.h>                     // uintptr_t

#include <libgraph.h>                   // SCE_GS_SET_*

#include "common/cpp_heap.h"            // operator_new, operator_delete
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "common/utility2.h"            // GetAlignUp
#include "graphics/graph2d/g2d_debug.h" // DispSqrRect
#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, DISP_SPRT, CopySprDToSpr, DispSprD
#include "graphics/graph3d/gra3d.h"     // gra3dUseScratchpad
#include "graphics/gs_reg.h"            // ScreenCaptureSave
#include "graphics/scene/scene.h"       // GetFileNoFromSceneNo, SceneDecisionMovie
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"          // title_start_flg
#include "ingame/map/MapLoad.h"         // MapLoadCheckDisp, MapLoadGetFreeArea
#include "ingame/movie_room_menu/prg/movie_projecter.h"  // MovieReqGetSceneNo, MovieReqGetRetPhase, MovieReqDispCapture
#include "reipan/reipan_memory.h"       // ReiPan_GetHostPointer
#include "sdk/sifdev.h"                 // sceSifAllocIopHeap, sceSifFreeIopHeap
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/eeiop/ee_iop_q.h"      // ReqQuerySPUTransCoreGet / Release
#include "system/eeiop/snd.h"           // SndGetGroupVolume
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/os/system.h"           // GetPALMode, SetVBlankWaitNum
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key
#include "system/playpss/my_strfile.h"  // MyStrStart, MyStrRead, MyStrStop
#include "system/playpss/playpss.h"     // playPss*

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// GS local-memory block the decoded frame is loaded into.
#define MOVIE_VRAM_ADRS     0x2bc0

// The EE block InitMovie carves up: the five decoder buffers, the audio ring
// and 0x80 of slack for the alignment.
#define MOVIE_BUF_SIZE      0x2afd80

// sdata 0x371398: the frame as a texture -- TBP 0x2bc0, 640 wide, PSMCT32,
// 1024x512, CLD 1.
static const u_long movie_tex0 = SCE_GS_SET_TEX0(MOVIE_VRAM_ADRS, 10, SCE_GS_PSMCT32, 10, 9, 0, 0, 0, 0, 0, 0, 1);
static const float movie_scr_w = 640.0f;    // sdata 0x3713a0
static const float movie_scr_h = 448.0f;    // sdata 0x3713a4
static const float movie_x     = 1.0f;      // sdata 0x3713a8

// sdata 0x371390 (gp-0x2160) -- -1 in the ROM's image: frames played,
// -1 while no movie is up. EndMovie and MovieDispMain gate on it.
static int iMovieCnt = -1;

static playPssRsrcs rsrcs;              // bss 0x3f60d0
static int          movie_audio_flg;    // sbss 0x3736ac (gp+0x1bc)
static BORROW_BUF  *movie_buf;          // sbss 0x3736b0 (gp+0x1c0): holds rsrcs' EE buffers

// ──────────────────────────────────────────────────────────────────────
// Buffers
// ──────────────────────────────────────────────────────────────────────

// Claim IOP memory or stop dead: a failure means the IOP heap is corrupt.
// FF3 0x00217c90; FF2 iopalloc (its printfs compiled out).

int iopalloc(u_int size, const char *desc)
{
    void *ret;

    (void)desc;
    ret = sceSifAllocIopHeap(size);
    if (ret == nullptr)
    {
        for (;;)
        {
        }
    }
    return (int)(uintptr_t)ret;     // an IOP address
}

// FF3 0x00217cd0.

static void MovieRsrcsClear(playPssRsrcs *rs)
{
    rs->iopZeroBuff = 0;
    rs->mpegBuff    = nullptr;
    rs->rgb32       = nullptr;
    rs->path3tag    = nullptr;
    rs->read_buf    = nullptr;
    rs->demuxBuff   = nullptr;
    rs->audioBuff   = nullptr;
    rs->iopBuff     = 0;
    rs->zeroBuff    = nullptr;
}

// Everything the decoder needs out of one 2.7 MB BORROW_BUF, aligned to
// 128: video ES ring, IPU output, GS chain, read staging, demux, and with
// audio the audio ring, its IOP twin, a silent sector (system heap) and its
// IOP twin. Nothing is set when the buffer could not be had.
// FF3 0x00217cf8; FF2 InitMovie's allocation half.
// callcheck: BorrowBufCtor, GetAlignUp (the new-expression; PC: the 128-byte
// alignment of a host pointer)

void InitMovie(playPssRsrcs *rs, int audio_flg)
{
    u_char *base;

    movie_buf = new (operator_new(sizeof(BORROW_BUF))) BORROW_BUF(MOVIE_BUF_SIZE);
    if (movie_buf->adrs == nullptr)
    {
        return;
    }

    // PC: GetAlignUp(adrs, 7) on a host pointer.
    base = (u_char *)(((uintptr_t)movie_buf->adrs + 0x7f) & ~(uintptr_t)0x7f);

    rs->mpegBuff      = base;
    rs->mpegBuffSize  = 0x13d800;
    rs->rgb32         = (sceIpuRGB32 *)(base + 0x13d800);
    rs->demuxBuffSize = 0x30000;
    rs->path3tag      = (u_int *)(base + 0x255800);
    rs->read_buf      = (char *)(base + 0x26fd00);
    rs->demuxBuff     = base + 0x273d00;
    if (audio_flg != 0)
    {
        rs->audioBuff     = base + 0x2a3d00;
        rs->audioBuffSize = 0xc000;
        rs->iopBuff       = iopalloc(0x6000, "audio buffer");
        rs->iopBuffSize   = 0x6000;
        rs->zeroBuff      = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), 0x800, 0, 0, 0);
        rs->iopZeroBuff   = iopalloc(0x800, "zero buffer");
    }
}

// Gives back what InitMovie took (the audio blocks only with audio_flg).
// FF3 0x00217e10.
// callcheck: BorrowBufDtor (the explicit destructor call)

static void MovieRsrcsFree(playPssRsrcs *rs, int audio_flg)
{
    BORROW_BUF *buf = movie_buf;

    if (buf != nullptr)
    {
        buf->~BORROW_BUF();
        operator_delete(buf);
    }
    movie_buf = nullptr;

    if (audio_flg != 0)
    {
        if (rs->iopBuff != 0)
        {
            sceSifFreeIopHeap((void *)(uintptr_t)rs->iopBuff);
        }
        if (rs->zeroBuff != nullptr)
        {
            heapCtrlFree(GetSystemHeapWrkP(), rs->zeroBuff, 0, 0, 0);
        }
        if (rs->iopZeroBuff != 0)
        {
            sceSifFreeIopHeap((void *)(uintptr_t)rs->iopZeroBuff);
        }
    }
    MovieRsrcsClear(rs);
}

// 1 when a buffer InitMovie should have set is missing (the audio ones only
// with audio_flg), else 0.
// FF3 0x00217ec0.

static int MovieRsrcsCheckLack(playPssRsrcs *rs, int audio_flg)
{
    if (rs->mpegBuff == nullptr || rs->rgb32 == nullptr || rs->demuxBuff == nullptr ||
        rs->path3tag == nullptr || rs->read_buf == nullptr)
    {
        return 1;
    }
    if (audio_flg == 0)
    {
        return 0;
    }
    if (rs->audioBuff == nullptr || rs->iopBuff == 0 || rs->zeroBuff == nullptr ||
        rs->iopZeroBuff == 0)
    {
        return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// BORROW_BUF
// ──────────────────────────────────────────────────────────────────────

// In a game the free room buffer (64-aligned), unless both room buffers are
// shown (then nothing, heap 1); otherwise, or when the map loader has none
// free, size bytes of system heap.
// The identical base-object copy is BorrowBufCtor2 (twin 0x00218310).
// FF3 0x002183e8 (complete object).

BORROW_BUF::BORROW_BUF(u_int size)
{
    adrs = nullptr;
    heap = 1;
    if (title_start_flg.mode == 1 || title_start_flg.mode == 2)
    {
        if (MapLoadCheckDisp(0) != 0 && MapLoadCheckDisp(1) != 0)
        {
            return;
        }
        u_int ee = GetAlignUp(MapLoadGetFreeArea(), 6);
        // PC: the map buffers are EE addresses; 0 stays nullptr.
        adrs = ee != 0 ? (u_char *)ReiPan_GetHostPointer(ee) : nullptr;
        heap = 0;
    }
    if (adrs == nullptr)
    {
        adrs = (u_char *)heapCtrlMalloc(GetSystemHeapWrkP(), size, 0, 0, 0);
        heap = 1;
    }
}

// The identical base-object copy is BorrowBufDtor2 (twin 0x002184c0).
// FF3 0x00218530 (complete object).

BORROW_BUF::~BORROW_BUF()
{
    if (heap != 0 && heap == 1 && adrs != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), adrs, 0, 0, 0);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Playback
// ──────────────────────────────────────────────────────────────────────

// Claims the buffers and opens the stream; the per-scene volume scales the
// BGM group volume, so the option screen's music slider still applies.
// FF3 0x00217f40; FF2 InitMovie's second half.

int StartMovie(int file_no, int vol, int audio_flg)
{
    movie_audio_flg = audio_flg;
    iMovieCnt       = -1;
    MovieRsrcsClear(&rsrcs);
    InitMovie(&rsrcs, movie_audio_flg);
    if (MovieRsrcsCheckLack(&rsrcs, movie_audio_flg) != 0)
    {
        MovieRsrcsFree(&rsrcs, movie_audio_flg);
        return 0;
    }

    if (movie_audio_flg != 0)
    {
        rsrcs.audio_vol_percent = vol * SndGetGroupVolume(1) / 256;    // group 1: BGM
        ReqQuerySPUTransCoreGet(&rsrcs.auto_dma_core);
    }

    // The streamer is asked for the file until it takes it.
    while (MyStrStart(file_no, 10, 8) == 0)
    {
    }

    if (GetPALMode() != 0)
    {
        playPssSetNtsc2Pal(1, MOVIE_VRAM_ADRS);
    }
    playPssStart(MyStrRead, &rsrcs);

    iMovieCnt = 0;
    return 1;
}

// One frame. In PAL the game drops to one field per frame while it plays.
// With skip, a trigger of pad (-1: OK or START) stops the movie. The frame
// is drawn over a black screen, scaled from its decoded size to 640x448.
// FF3 0x00218048.

int MovieDispMain(int skip, int pad)
{
    int ret = 1;

    if (iMovieCnt < 0)
    {
        return ret;
    }

    if (GetPALMode() != 0)
    {
        SetVBlankWaitNum(1);
    }

    if (skip != 0)
    {
        int trg;

        if (pad == -1)
        {
            trg = CheckPadTrgPlaySe((u_short)(pad_key[PAD_KEY_OK] | 0x800), -1, 0);
        }
        else
        {
            trg = CheckPadTrgPlaySe((u_short)pad, -1, 0);
        }
        if (trg != 0)
        {
            playPssStop();
        }
    }

    iMovieCnt++;
    gra3dUseScratchpad(0);

    ret = playPssMain(&rsrcs, MOVIE_VRAM_ADRS);
    if (ret == 0)
    {
        DispSqrRect(0.0f, 0.0f, movie_scr_w, movie_scr_h, 0xe0, 0, 0, 0, 0x80);

        if (playPssAlreadySendImage() != 0)
        {
            // rodata 0x35cb40
            SPRT_DAT dat = { 0, 1, 0, 0x27e, 0x1c0, 0, 0, 0xe0, 0x80, 0, 1 };
            DISP_SPRT ds;
            PLAY_PSS_MPEG_INFO info;

            CopySprDToSpr(&ds, &dat);
            ds.y    = 0.0f;
            ds.tex1 = SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0);            // 0x161
            ds.tex0 = movie_tex0;
            ds.test = SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1);         // 0x30003
            ds.x    = movie_x;
            playPssGetMpegInfo(&info);
            ds.w    = info.width - 2;
            ds.h    = info.height;
            ds.scw  = movie_scr_w / (float)info.width;
            ds.sch  = movie_scr_h / (float)info.height;
            DispSprD(&ds);
        }
    }
    return ret;
}

// Gives everything back and puts the frame rate and the scratchpad back.
// FF3 0x00218250; FF2 EndMovie.

void EndMovie(void)
{
    if (iMovieCnt < 0)
    {
        return;
    }

    SetVBlankWaitNum(2);
    playPssEnd();
    playPssSetNtsc2Pal(0, MOVIE_VRAM_ADRS);
    MyStrStop();
    gra3dUseScratchpad(1);
    if (movie_audio_flg != 0)
    {
        ReqQuerySPUTransCoreRelease(rsrcs.auto_dma_core);
    }
    MovieRsrcsFree(&rsrcs, movie_audio_flg);
    iMovieCnt = -1;
}

// The counter halved in PAL (a shift, so -1 stays -1): the caption table and
// the scene-effect scripts count NTSC frames.
// FF3 0x002182d0; FF2 MovieCountGet.

int MovieCountGet(void)
{
    return GetPALMode() ? iMovieCnt >> 1 : iMovieCnt;
}

// FF3 0x00218300.

void MovieResetPlaying(void)
{
    iMovieCnt = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 39 Story_SceneMovie
// ──────────────────────────────────────────────────────────────────────

// Keeps a copy of the screen (capture type 2, blended) for the scene /
// movie phases to fade over.
// FF3 0x002185a0.

int init_Story_SceneMovie(void)
{
    ScreenCaptureSave(2, 1);
    return 1;
}

// FF3 0x002185c8.

int end_Story_SceneMovie(void)
{
    return 1;
}

// The requested scene goes to phase 67 Story_Movie when it is a movie, else
// 66 Story_Scene; when its file is missing or empty the phase that asked is
// requested back.
// FF3 0x002185d0.

int one_Story_SceneMovie(void)
{
    int   scene_no = MovieReqGetSceneNo();
    short next     = MovieReqGetRetPhase();

    MovieReqDispCapture();
    if (GetFileSize(GetFileNoFromSceneNo(scene_no)) != (u_int)-1 &&
        GetFileSize(GetFileNoFromSceneNo(scene_no)) != 0)
    {
        next = SceneDecisionMovie(scene_no) != 0 ? 0x43 : 0x42;
    }
    GPhaseCtrlRequest(next, 0);
    return 1;
}

// FF3 0x00218668.

void pre_Story_SceneMovie(void)
{
}

// FF3 0x00218670.

void after_Story_SceneMovie(void)
{
}
