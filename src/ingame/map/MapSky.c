/* ==========================================================================
 *  ingame/map/MapSky.c
 *
 *  FF3's map sky: the sky dome data (MAP_SKY_DATA, a PssModel showing
 *  shuffled sky movies) and the PSS movie player under it (MAP_SKY), which
 *  the title also uses for its backdrop. See MapSky.h.
 *  FF3 0x11d890..0x11e820 (FF2's MapSky.c sits in the same link slot but is
 *  pak-based sky code; nothing of it survives), and the dome's model classes
 *  (PssModel, PssModelBuilder, DummyTextureModelObject), inline code at
 *  0x2ac590..0x2ac870.
 * ======================================================================== */
#include "ingame/map/MapSky.h"

#include <new>                                         // placement new on FF3's C++ heap (operator_new)
#include <string.h>                                     // memcpy

#include <eeregs.h>                                     // REG_IPU_CMD

#include "common/cpp_heap.h"                            // operator_new / operator_delete
#include "common/heapctrl.h"                            // heapCtrlMalloc / heapCtrlFree
#include "common/pk4.h"                                 // Pk4GetData
#include "common/vu0math.h"                             // GetRandValIByF
#include "graphics/graph2d/tim2.h"                      // Tim2GetFirstPicture, MakeTim2Direct
#include "graphics/graph3d/gra3d.h"                     // gra3dUseScratchpad
#include "graphics/gs_reg.h"                            // SetGsRegsCached
#include "graphics/mmanage.h"                           // MdlPk4GetPart
#include "ingame/map/MapDraw.h"                         // MapDrawGetLightPtr
#include "ingame/plyr/player.h"                         // GetPlyrAreaNo
#include "reipan/reipan_memory.h"                       // ReiPan_GetHostPointer
#include "system/eeiop/cd_file_dat.h"                   // DOME_MDL_PK4, SCENE91xxM_PSS
#include "system/os/sys_heap.h"                         // GetSystemHeapWrkP
#include "system/os/system.h"                           // GetPacketBufMgr
#include "system/playpss/my_strfile.h"                  // MyStrStart / MyStrRead / MyStrStop
#include "system/playpss/pss_loop.h"                    // PssLoop*

#include <libgraph.h>                                   // SCE_GS_SET_*

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The sky movies the dome cycles through (rdata 0x34dcd0; FF3 appends
// SCENE9108M_PSS on the stack). Each is followed by its _SLT and _EFF.
static const int map_sky_movie_tbl[9] =
{
    SCENE9100M_PSS, SCENE9101M_PSS, SCENE9102M_PSS, SCENE9103M_PSS,
    SCENE9104M_PSS, SCENE9105M_PSS, SCENE9106M_PSS, SCENE9107M_PSS,
    SCENE9108M_PSS,
};

// sdata 0x372900 (gp-0xbf0) caches GetMapSky; title_obj.c reads the same
// word through its own copy of the cached getter.
static MAP_SKY *map_sky_p;

// sdata gp-0xca0 caches GetPacketBufMgr (shared with g2d_draw.c / BpCtl.c).
static PACKET_BUF_MGR *map_sky_pkt_mgr;

// The movie streams' ring: 10 buffers of 0x18 sectors.
#define MAP_SKY_RING_BUF_NUM        10
#define MAP_SKY_ONE_BUF_SECTOR      0x18

// IPU SETTH: CSC alpha threshold th0 = 10 while a movie plays, 0 after.
#define MAP_SKY_IPU_SETTH_ON        0x9000000a
#define MAP_SKY_IPU_SETTH_OFF       0x90000000

// The cached getter FF3 inlines at every use.
static MAP_SKY *MapSkyGet(void)
{
    if (map_sky_p == nullptr)
        map_sky_p = GetMapSky();
    return map_sky_p;
}

// The dome model's first texture: its PK4's "tpk" member, picture 0.
// FF3 inlines this (a one-word heap object around the PK4 address,
// operator_new(4) / operator_delete) in MapSkyDataIsReady and MapSkyDataDraw.
void *MAP_SKY_DATA::GetTim2()
{
    void **ref;
    void  *tpk;

    ref  = (void **)operator_new(sizeof(void *));
    *ref = mdl->pk4;
    tpk  = MdlPk4GetPart(ref, MDL_PK4_TPK);
    tpk  = Pk4GetData(tpk, 0);
    operator_delete(ref);
    return tpk;
}

// ──────────────────────────────────────────────────────────────────────
// The sky data
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0011d890

void MAP_SKY_DATA::Init()
{
    playing = 0;
    mdl     = nullptr;
    started = 0;
}

// The dome: a PssModel loading DOME_MDL_PK4. FF3 0x0011d8a0 (PssModel's
// constructor is inline: GameModel's, then the vptr 0x3411c8).
// callcheck: GameModelCtorBase (new PssModel)

void MAP_SKY_DATA::Create()
{
    PssModel *mdl;

    mdl = new (operator_new(sizeof(PssModel))) PssModel;
    this->mdl = mdl;
    mdl->Load(DOME_MDL_PK4);
    playing = 0;
    started = 0;
}

// FF3 0x0011d900
// callcheck: GetMapSky (MapSkyGet, the cached getter FF3 inlines)

void MAP_SKY_DATA::Release()
{
    if (started != 0)
    {
        MapSkyGet()->Reset();
        FreeList();
    }
    if (mdl != nullptr)
    {
        mdl->~PssModel();                         // FF3: through the vtable (+0x0c, 0x2ac590)
        operator_delete(mdl);
        mdl = nullptr;
    }
    playing = 0;
    started = 0;
}

// The nine movie ids in a fresh heap list, -1 terminated, then nine random
// swaps. FF3 0x0011d978

void MAP_SKY_DATA::Shuffle()
{
    int  ids[9];
    int  tmp;
    char a, b;
    char i;

    memcpy(ids, map_sky_movie_tbl, sizeof(ids));

    list = (int *)heapCtrlMalloc(GetSystemHeapWrkP(), 0x28, 0, 0, 0);
    next = list;
    memcpy(list, ids, sizeof(ids));
    next[9] = -1;

    i = 0;
    do
    {
        a = (char)GetRandValIByF(9);
        b = (char)GetRandValIByF(9);
        i++;
        if (a != b)
        {
            tmp           = next[a];
            next[a] = next[b];
            next[b] = tmp;
        }
    } while (i < 9);
}

// FF3 0x0011da98: at the end of the list a new shuffle.

int MAP_SKY_DATA::NextFile()
{
    if (*next < 0)
    {
        FreeList();
        Shuffle();
    }
    return *next++;
}

// FF3 0x0011dae8

void MAP_SKY_DATA::FreeList()
{
    heapCtrlFree(GetSystemHeapWrkP(), list, 0, 0, 0);
    list = nullptr;
    next = nullptr;
}

// Once the dome has loaded: the first movie, on the dome's own texture
// (its TBP), 160x128 from the disc. FF3 0x0011db30
// callcheck: GetMapSky, operator_new, MdlPk4GetPart, Pk4GetData, operator_delete
// (MapSkyGet and GetTim2, inline in FF3)

int MAP_SKY_DATA::IsReady()
{
    TIM2_PICTUREHEADER *pic;
    u_long              tex0;

    if (started == 0)
    {
        if (mdl->IsLoaded() == 0)               // FF3: through the vtable (+0x10)
            return 0;

        pic  = Tim2GetFirstPicture(GetTim2());
        tex0 = pic->GsTex0;
        Shuffle();
        MapSkyGet()->MovieSet(NextFile(), (int)(tex0 & 0x3fff), 160, 128, 0, 0);
        playing = 0;
        started = 1;
    }
    return 1;
}

// Per frame: wait for the movie, play it, and at its end the next one.
// FF3 0x0011dc38 (Ghidra had the FF2 match MapSkyRegist).
// callcheck: GetMapSky (MapSkyGet)

int MAP_SKY_DATA::Proc()
{
    if (started == 0)
        return 1;

    if (playing == 0)
    {
        if (MapSkyGet()->MovieIsReady() != 0)
            playing = 1;
        if (playing == 0)
            return 0;
    }

    if (MapSkyGet()->MovieProc(0) != 0)
    {
        MapSkyGet()->MovieChange(NextFile());
        playing = 0;
    }
    return 0;
}

// The dome at pos: the movie frame (or, without one, the dome's own
// texture) into its TBP, then the model under a translation to pos.
// FF3 0x0011dd20
// callcheck: GetMapSky, operator_new, MdlPk4GetPart, Pk4GetData, operator_delete
// (MapSkyGet and GetTim2, inline in FF3)

void MAP_SKY_DATA::Draw(sceVu0FVECTOR pos)
{
    GS_REG_SET    regs[5];
    sceVu0FMATRIX mat;
    int           no_frame;

    if (started == 0)
        return;

    regs[0].data = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
    regs[0].reg  = SCE_GS_ALPHA_1;
    regs[1].data = SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0);
    regs[1].reg  = SCE_GS_TEX1_1;
    regs[2].data = SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0);
    regs[2].reg  = SCE_GS_CLAMP_1;
    regs[3].data = SCE_GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 2);
    regs[3].reg  = SCE_GS_TEST_1;
    regs[4].data = SCE_GS_SET_ZBUF(0x118, 0xa, 0);
    regs[4].reg  = SCE_GS_ZBUF_1;
    SetGsRegsCached(regs, 5, 1);

    no_frame = 1;
    if (playing != 0)
        no_frame = MapSkyGet()->MovieSendFrame() == 0;
    if (no_frame)
        MakeTim2Direct(GetTim2(), 0, -1);

    // the unit matrix (rdata 0x3293c0) moved to pos + (0, 0, 0, 1) (0x3293f0)
    sceVu0UnitMatrix(mat);
    mat[3][0] = pos[0];
    mat[3][1] = pos[1];
    mat[3][2] = pos[2];
    mat[3][3] = pos[3] + 1.0f;
    mdl->SetWorldMatrix(mat);                       // FF3: through the vtable (+0x18)
    mdl->SetAlpha(0x80);
    mdl->DrawMdl();
}

// FF3 0x0011dea8
// callcheck: GetMapSky (MapSkyGet)

void MAP_SKY_DATA::Stop()
{
    if (started != 0)
        MapSkyGet()->MovieStop();
}

// FF3 0x0011def0
// callcheck: GetMapSky (MapSkyGet)

int MAP_SKY_DATA::IsNotStopping()
{
    if (started != 0)
        return MapSkyGet()->MovieIsNotStopping();
    return 1;
}

// FF3 0x0011df38
// callcheck: GetMapSky (MapSkyGet)

void MAP_SKY_DATA::Restart()
{
    if (started != 0)
        MapSkyGet()->MovieRestart();
}

// ──────────────────────────────────────────────────────────────────────
// The movie player
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0011df88

void MAP_SKY::Init()
{
    pkt_size = 0;
    play     = -1;
    file     = -1;
    tbp      = 0;
    width    = 0;
    height   = 0;
    loop     = 0;
    alpha_th = 0;
    load     = -1;
    started  = 0;
    hold     = 0;
}

// FF3 0x0011dfc0

void MAP_SKY::MovieSet(int file, int tbp, int width, int height, int loop, int alpha_th)
{
    this->file     = file;
    this->tbp      = tbp;
    this->width    = width;
    this->height   = height;
    this->loop     = loop;
    this->alpha_th = alpha_th;
    pkt_size = 0;
    load     = 0;
    play     = 0;
    started  = 0;
    hold     = 0;
}

// One step of the load: 0 lays out the decoder resources (and gives the
// scratchpad up to the decoder), 1 allocates the buffers (0 on a failed
// allocation, retried next call), 2 starts the stream, 3 waits for the
// loop reader's head. Returns 1 once playing has started (load 4) or when
// stopped. FF3 0x0011dff0

int MAP_SKY::MovieLoad()
{
    playPssRsrcs *rs;
    HEAP_WRK     *heap;
    int           mb;

    if (play == 3)
        return 1;

    rs = &rsrcs;
    switch (load)
    {
    case 0:
        gra3dUseScratchpad(0);
        rs->demuxBuffSize = 0x30000;
        load         = 1;
        rs->audioBuff     = nullptr;
        rs->audioBuffSize = 0;
        rs->iopBuff       = 0;
        rs->iopBuffSize   = 0;
        rs->zeroBuff      = nullptr;
        rs->iopZeroBuff   = 0;
        rs->mpegBuff      = nullptr;
        rs->rgb32         = nullptr;
        rs->path3tag      = nullptr;
        rs->mpegBuffSize  = (width * height * 9) / 2 + 0x2800;
        rs->read_buf      = nullptr;
        rs->demuxBuff     = nullptr;
        // fall through
    case 1:
        if (rs->mpegBuff == nullptr)
        {
            heap = GetSystemHeapWrkP();
            rs->mpegBuff = (u_char *)heapCtrlMalloc(heap, (width * height * 9) / 2 + 0x2800, 0, 0, 0);
            if (rs->mpegBuff == nullptr)
                return 0;
        }
        mb = (width / 16) * (height / 16);
        if (rs->rgb32 == nullptr)
        {
            rs->rgb32 = (sceIpuRGB32 *)heapCtrlMalloc(GetSystemHeapWrkP(), mb * sizeof(sceIpuRGB32), 0, 0, 0);
            if (rs->rgb32 == nullptr)
                return 0;
        }
        if (rs->demuxBuff == nullptr)
        {
            rs->demuxBuff = heapCtrlMalloc(GetSystemHeapWrkP(), 0x30000, 0, 0, 0);
            if (rs->demuxBuff == nullptr)
                return 0;
        }
        if (rs->path3tag == nullptr)
        {
            // 0x18 bytes of tags per macroblock plus 0x67, in 0x40 steps, x4
            rs->path3tag = (u_int *)heapCtrlMalloc(GetSystemHeapWrkP(), ((mb * 0x18 + 0x67) / 0x40) << 8, 0, 0, 0);
            if (rs->path3tag == nullptr)
                return 0;
        }
        if (rs->read_buf == nullptr)
        {
            rs->read_buf = (char *)heapCtrlMalloc(GetSystemHeapWrkP(), 0x4000, 0, 0, 0);
            if (rs->read_buf == nullptr)
                return 0;
        }
        load = 2;
        // fall through
    case 2:
        if (loop == 0)
        {
            while (MyStrStart(file, MAP_SKY_RING_BUF_NUM, MAP_SKY_ONE_BUF_SECTOR) == 0)
                ;
            playPssStart(MyStrRead, rs);
            if (alpha_th != 0)
                REG_IPU_CMD = MAP_SKY_IPU_SETTH_ON;
        }
        else
        {
            PssLoopSet(file, MAP_SKY_RING_BUF_NUM, MAP_SKY_ONE_BUF_SECTOR);
        }
        load = 3;
        // fall through
    case 3:
        if (loop != 0)
        {
            if (PssLoopStart() == 0)
                return 0;
            playPssStart(PssLoopRead, rs);
            if (alpha_th != 0)
                REG_IPU_CMD = MAP_SKY_IPU_SETTH_ON;
        }
        play = 1;
        load = 4;
        return 1;

    default:                                            // 4 running, -1 none
        return 1;
    }
}

// FF3 0x0011e338: the first frame is in once the load has run.

int MAP_SKY::MovieCheckStart()
{
    if (load != 4)
        return 0;
    if (playPssIsReady() == 0)
        return 0;
    started = 1;
    return 1;
}

// FF3 0x0011e388

int MAP_SKY::MovieIsReady()
{
    MovieLoad();
    return MovieCheckStart() != 0;
}

// Decode one frame into rsrcs.path3tag (its size to pkt_size for
// MapSkyMovieSendFrame). At the end of the stream: a requested stop
// finishes (play 3, returns 1); a looping movie switches readers and goes
// on; any other returns 1. 0 while decoding, skipped or held.
// FF3 0x0011e3b8

int MAP_SKY::MovieProc(int skip)
{
    playPssRsrcs *rs;
    int           size;

    if (started == 0)
        return 1;
    if (skip != 0)
        return 0;

    rs = &rsrcs;
    if (hold != 0)
        return 0;

    size = 0;
    if (playPssSetTexPacket(rs, tbp, &size, width, height) == 0)
    {
        if (size == 0)
            return 0;
        pkt_size = size;
    }
    else
    {
        if (play == 2)
        {
            play = 3;
            MovieEnd();
            return 1;
        }
        if (loop == 0)
            return 1;

        playPssEnd();
        PssLoopRewind();
        playPssStart(PssLoopRead, rs);
        playPssSetTexPacket(rs, tbp, &size, width, height);
    }
    return 0;
}

// Copy the frame's load chain into the packet buffer half being filled,
// when it fits. FF3 0x0011e4a0

int MAP_SKY::MovieSendFrame()
{
    PACKET_BUF_MGR *mgr;
    u_int           cur;

    if (pkt_size == 0 || play != 1)
        return 0;

    if (map_sky_pkt_mgr == nullptr)
        map_sky_pkt_mgr = GetPacketBufMgr();
    mgr = map_sky_pkt_mgr;

    cur = mgr->cur[(int)mgr->side];
    if (pkt_size > (int)(mgr->size - (cur - mgr->top[(int)mgr->side])))
        return 0;

    memcpy(ReiPan_GetHostPointer(cur), rsrcs.path3tag, pkt_size);
    mgr->cur[(int)mgr->side] = (cur + pkt_size) & 0x0fffffff;
    return 1;
}

// FF3 0x0011e570: from load state 2 on, the decoder and the stream are up.

void MAP_SKY::MovieEnd()
{
    started  = 0;
    hold     = 0;
    pkt_size = 0;
    if (load < 2)
        return;

    playPssEnd();
    REG_IPU_CMD = MAP_SKY_IPU_SETTH_OFF;
    if (loop != 0)
    {
        PssLoopEnd();
        return;
    }
    MyStrStop();
}

// FF3 0x0011e5e8: the buffers, and the scratchpad back to gra3d.

void MAP_SKY::MovieFreeBuf()
{
    playPssRsrcs *rs;

    if (load == -1)
        return;

    rs = &rsrcs;
    if (rs->read_buf != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), rs->read_buf, 0, 0, 0);
        rs->read_buf = nullptr;
    }
    if (rs->path3tag != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), rs->path3tag, 0, 0, 0);
        rs->path3tag = nullptr;
    }
    if (rs->demuxBuff != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), rs->demuxBuff, 0, 0, 0);
        rs->demuxBuff = nullptr;
    }
    if (rs->rgb32 != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), rs->rgb32, 0, 0, 0);
        rs->rgb32 = nullptr;
    }
    if (rs->mpegBuff != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), rs->mpegBuff, 0, 0, 0);
        rs->mpegBuff = nullptr;
    }
    gra3dUseScratchpad(1);
}

// FF3 0x0011e718

void MAP_SKY::Reset()
{
    MovieEnd();
    MovieFreeBuf();
    Init();
}

// Switch to another file without freeing the buffers: the next load
// steps restart at the stream. FF3 0x0011e748 (Ghidra had the FF2 match
// MapSpMoviRegistObj).

void MAP_SKY::MovieChange(int file)
{
    if (play != 1)
        return;

    MovieEnd();
    this->file = file;
    load = 2;
}

// FF3 0x0011e798

void MAP_SKY::MovieStop()
{
    if (play != 1)
        return;

    if (started != 0)
    {
        play = 2;
        playPssStop();
        return;
    }
    play = 3;
    MovieEnd();
}

// FF3 0x0011e7e8

int MAP_SKY::MovieIsNotStopping()
{
    return play != 2;
}

// FF3 0x0011e7f8

void MAP_SKY::MovieRestart()
{
    if (play == 3)
    {
        play = 0;
        load = 2;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The singleton
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002ac520: built on first use (MAP_SKY's constructor runs Init;
// the guard and the atexit of the empty destructor 0x11df80 come from the
// compiler).
// callcheck: MapSkyInit, atexit (MAP_SKY's constructor; the compiler's guard)

MAP_SKY *GetMapSky(void)
{
    static MAP_SKY map_sky;                             // bss 0x41e4c8

    return &map_sky;
}

// FF3 0x002afdf0: no constructor (main.c's GameSystemStart calls
// MapSkyDataInit); the guard and the atexit of the empty destructor
// 0x15b650 come from the compiler.
// callcheck: atexit (the compiler's)

MAP_SKY_DATA *GetMapSkyData(void)
{
    static MAP_SKY_DATA map_sky_data;                   // bss 0x41e568

    return &map_sky_data;
}

// ──────────────────────────────────────────────────────────────────────
// The dome's model classes (inline code FF3 emitted after the singleton
// getters, 0x2ac590..0x2ac870)
// ──────────────────────────────────────────────────────────────────────

// The destructors are compiler output, implicit here (each stores its
// class's vtable, runs the base destructor, the deleting one then
// operator_delete):
//   PssModel                FF3 0x002ac5c8, deleting FF3 0x002ac590 (GameModel's)
//   PssModelBuilder         FF3 0x002ac6d0, deleting FF3 0x002ac6f8 (NormalModelBuilder's)
//   DummyTextureModelObject FF3 0x002ac7a8, deleting FF3 0x002ac7b8 (only the vtable
//                           store: ModelObject's destructor is empty in FF3)

// The builder and the motion object: like GameModel::Build, but always a
// PssModelBuilder (no cloth). FF3 0x002ac5f0 (vtable +0x00).
// callcheck: NormalModelBuilderCtorBase (new PssModelBuilder), ModelDirectorConstruct, GetResult (builder vtable +0x14)

void PssModel::Build()
{
    void         **holder;
    ModelDirector *dir;

    // PC: FF3 allocates one 4-byte word for each of these (EE pointers)
    holder  = (void **)operator_new(sizeof(void *));
    *holder = pk4;
    builder = new (operator_new(sizeof(PssModelBuilder))) PssModelBuilder(holder);

    dir = new (operator_new(sizeof(ModelDirector))) ModelDirector(builder);
    dir->Construct();
    operator_delete(dir);
    operator_delete(holder);

    mot = builder->GetResult();
}

// Before the draw: the light set of the player's area (bank 0), no offset.
// FF3 0x002ac6a0 (vtable +0x20).

void PssModel::DrawMdlPre()
{
    GRA3DLIGHTDATA *light;

    light = MapDrawGetLightPtr(GetPlyrAreaNo(), 0);
    if (light == nullptr)               // PC: FF3 reads the null set (low EE memory)
        return;
    gra3dSetLightData(light, nullptr);
}

// The object: mpk / flgs / tpk of the PK4, no mono, no collision shapes.
// FF3 0x002ac740 (vtable +0x08).
// callcheck: ModelObjectCtorBase (DummyTextureModelObject's inline constructor)

void PssModelBuilder::BuildObject()
{
    obj = new (operator_new(sizeof(DummyTextureModelObject))) DummyTextureModelObject(mpk, flgs, tpk);
}

// Before the SGD draw: every texture of tpk but the first (the movie frame)
// into VRAM at its own TBP. FF3 0x002ac7f8 (vtable +0x00).

void DummyTextureModelObject::BeginDraw()
{
    int num;
    int i;

    num = (tpk != nullptr) ? ((PK4_HEADER *)tpk)->num : 0;
    for (i = 0; i < num; i++)
    {
        if (i != 0)
            Tim2PackSend(tpk, i, 0, -1, -1);
    }
}
