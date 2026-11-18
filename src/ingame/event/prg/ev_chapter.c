/* ==========================================================================
 *  ingame/event/prg/ev_chapter.c
 *
 *  FF3 0x19a0a0..0x19abe8: GetObjectPos, the chapter title and the event
 *  ghost tables (see ev_chapter.h for why the file has this name).
 * ======================================================================== */
#include "ingame/event/prg/ev_chapter.h"

#include <stdio.h>                              // sprintf
#include <string.h>                             // memset

#include "common/heapctrl.h"                    // heapCtrlMalloc, heapCtrlFree
#include "common/vu0math.h"                     // Vu0Sinf
#include "graphics/graph2d/g2d_draw.h"          // SPRT_DAT, DISP_SPRT, CopySprDToSpr, DispSprD, effdat, Start/EndDmaDirectTrans
#include "graphics/graph2d/tim2.h"              // Tim2GetFirstPicture, MakeTim2Direct, MakeClutDirect
#include "graphics/gs_reg.h"                    // GS_REG_SET, SetGsRegsCached
#include "ingame/enemy/ene_main.h"              // EneGetPos, SearchEneWrkNo, EneGetState
#include "ingame/enemy/enemy.h"                 // EneLoadReq, EneReleaseReq
#include "ingame/ingame_wrk.h"                  // ingame_wrk
#include "ingame/map/MapObjWrk.h"               // MapObjPutCheck
#include "ingame/plyr/player.h"                 // PLYR_WRK, GetPlyrAreaNo
#include "sdk/libgraph.h"                       // SCE_GIF_SET_TAG, SCE_GS_*
#include "system/eeiop/cd_file_dat.h"           // CHAPTER_TITLE_00_TM2
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/eeiop/fileload.h"              // FileLoadReqEE, FileLoadIsEnd2
#include "system/os/sys_heap.h"                 // GetSystemHeapWrkP
#include "system/os/system.h"                   // SYS_WRK, GetSysWrk

// ──────────────────────────────────────────────────────────────────────
// The object's position into pos: ghost kinds 0..2 from their enemy work,
// the player, or a placed map object ("f%03d" furniture / "d%03d" door) of
// the player's area. 0 for the other kinds (FF2 also had the sister, 7, and
// the ghost kind 5, and printed an error banner for the rest).
// FF3 0x0019a0a0; FF2 GetObjectPos (common/zero2_util.c).

int GetObjectPos(float *pos, u_char obj_type, int obj_id)
{
    char name[128];

    pos[0] = pos[1] = pos[2] = pos[3] = 0.0f;   // FF3: vsub of the vector with itself

    switch (obj_type) {
    case 0:
    case 1:
    case 2:
        return EneGetPos(pos, obj_type, (short)obj_id) != 0;

    case EV_OBJ_TYPE_PLAYER:
        pos[0] = ingame_wrk.plyr_wrk->pos[0];
        pos[1] = ingame_wrk.plyr_wrk->pos[1];
        pos[2] = ingame_wrk.plyr_wrk->pos[2];
        pos[3] = ingame_wrk.plyr_wrk->pos[3];
        return 1;

    case EV_OBJ_TYPE_FURN:
    case EV_OBJ_TYPE_DOOR:
        sprintf(name, (obj_type == EV_OBJ_TYPE_FURN) ? "f%03d" : "d%03d", obj_id);
        return MapObjPutCheck(pos, GetPlyrAreaNo(), name) >= 0;

    default:
        return 0;
    }
}

// ══════════════════════════════════════════════════════════════════════
// The chapter title
// ══════════════════════════════════════════════════════════════════════

// The title's work (sdata gp-0x4a0, 8 bytes; cleared whole by Init and
// Release).
struct EV_CHAPTER_DISP                  /* 0x8 */
{
    /* 0x0 */ u_char step;              // EV_CHAPTER_STEP_*
    /* 0x1 */ u_char chapter;           // the file's number (CHAPTER_TITLE_00_TM2 + chapter)
    /* 0x2 */ u_char active;            // 1 from the request until the release
    /* 0x3 */ u_char fade;              // EV_CHAPTER_FADE_*
    /* 0x4 */ int    cnt;               // frames in the fade step
};
static_assert(sizeof(EV_CHAPTER_DISP) == 0x8, "EV_CHAPTER_DISP is 8 bytes");
static_assert(offsetof(EV_CHAPTER_DISP, active) == 0x2, "EV_CHAPTER_DISP.active");
static_assert(offsetof(EV_CHAPTER_DISP, cnt) == 0x4, "EV_CHAPTER_DISP.cnt");

enum
{
    EV_CHAPTER_STEP_LOAD = 0,           // start the picture's load
    EV_CHAPTER_STEP_WAIT = 1,           // wait for it
    EV_CHAPTER_STEP_DRAW = 2,           // fade in, hold, fade out
    EV_CHAPTER_STEP_END  = 3            // release
};

enum
{
    EV_CHAPTER_FADE_START = 0,
    EV_CHAPTER_FADE_IN    = 1,          // 10 frames
    EV_CHAPTER_FADE_HOLD  = 2,          // 90 frames
    EV_CHAPTER_FADE_OUT   = 3           // 30 frames
};

static EV_CHAPTER_DISP ev_chapter_disp; // sdata gp-0x4a0
static void *ev_chapter_tm2;            // sdata gp-0x498: the picture (system heap)

// FF3's shared inline cache of GetSysWrk (sdata gp-0xc60).
static SYS_WRK *ev_chapter_sys_wrk;

static int EvChapterDispFile(void)
{
    if (ev_chapter_sys_wrk == nullptr)
        ev_chapter_sys_wrk = GetSysWrk();
    return ev_chapter_disp.chapter + ev_chapter_sys_wrk->GetLanguage() * 0x10 + CHAPTER_TITLE_00_TM2;
}

// The halves of the title picture (data 0x355570): the chapter line, and the
// title line under it to the right.
static const SPRT_DAT chapter_title_spr[2] = {
    { 0x2005980621312bc0, 1, 0x01, 0xfe, 0x99, 0x00, 0x127, 0, 0x80, 0, 0 },
    { 0x2005980621312bc0, 1, 0x9c, 0xfe, 0x47, 0xfe, 0x179, 0, 0x80, 0, 0 },
};

#define CHAPTER_TITLE_PIC_TEX0  0x200598826932abc0  // rdata 0x370130: the picture header's GsTex0
#define CHAPTER_TITLE_SPR_TEX0  0x200598866932abc0  // rdata 0x370138: the sprites' TEX0
#define CHAPTER_TITLE_TBP       0x2bc0
#define CHAPTER_TITLE_CBP       0x2cc4

// The shade's GS state (data 0x3555d0): ALPHA_1 (Cs - Cd) * As + Cd, TEX1_1
// linear, CLAMP_1 repeat, TEST_1 alpha test off / z test always, ZBUF_1 no
// z writes.
static const GS_REG_SET chapter_shade_regs[5] = {
    { 0x44,        SCE_GS_ALPHA_1 },
    { 0x161,       SCE_GS_TEX1_1 },
    { 0,           SCE_GS_CLAMP_1 },
    { 0x30003,     SCE_GS_TEST_1 },
    { 0x10a000118, SCE_GS_ZBUF_1 },
};

// The shade's screen corners (data 0x3555b0): the lower half of the screen.
static const int chapter_shade_xy[4][2] = {
    { 0, 0xe0 }, { 0x280, 0xe0 }, { 0, 0x1c0 }, { 0x280, 0x1c0 },
};

// A shade over the lower half of the screen: the lower half of the effect
// texture effdat[68] stretched over it, clear at the top edge and alpha at
// the bottom (a 4-vertex strip, gouraud, textured, blended).
// FF3 0x002b8d00 (an out-of-line copy among the inline bodies at the end of
// the text; only the chapter title calls it).

static void EvChapterDispDrawShade(u_char alpha)
{
    const SPRT_DAT *tex = &effdat[68];
    int uv[4][2];
    u_long *p;
    u_int *q;
    int i;

    uv[0][0] = 0;       uv[0][1] = tex->h >> 1;
    uv[1][0] = tex->w;  uv[1][1] = tex->h >> 1;
    uv[2][0] = 0;       uv[2][1] = tex->h;
    uv[3][0] = tex->w;  uv[3][1] = tex->h;

    SetGsRegsCached(chapter_shade_regs, 5, 1);

    p = (u_long *)StartDmaDirectTrans();
    p[0] = SCE_GIF_SET_TAG(2, 1, 0, 0, SCE_GIF_PACKED, 1);     // 0x1000000000008002
    p[1] = SCE_GIF_PACKED_AD;
    p[2] = 0;
    p[3] = SCE_GS_TEXFLUSH;
    p[4] = tex->tex0;
    p[5] = SCE_GS_TEX0_1;
    p[6] = 0x30ae400000008004ULL;       // 4 loops, PRIM 0x15c (strip, gouraud, textured, blended, UV), 3 regs
    p[7] = 0x431;                       // RGBAQ, UV, XYZF2

    q = (u_int *)(p + 8);
    for (i = 0; i < 4; i++) {
        q[0]  = 0x80;
        q[1]  = 0x80;
        q[2]  = 0x80;
        q[3]  = (i / 2 != 0) ? alpha : 0;
        q[4]  = (u_int)uv[i][0] << 4;
        q[5]  = (u_int)uv[i][1] << 4;
        q[6]  = 0;
        q[7]  = 0;
        q[8]  = (u_int)(chapter_shade_xy[i][0] + 0x6c0) << 4;
        q[9]  = (u_int)(chapter_shade_xy[i][1] + 0x720) << 4;
        q[10] = 0;
        q[11] = (i < 2) ? 0x8000 : 0;   // ADC: the first two only prime the strip
        q += 12;
    }
    EndDmaDirectTrans((qword *)q);
}

// ──────────────────────────────────────────────────────────────────────
// Steps the title: load, wait, draw, release. FF3 0x0019a188.

static int EvChapterDispLoadReq(void);
static int EvChapterDispIsLoadEnd(void);
static void EvChapterDispDraw(void);

static void EvChapterDispStep(void)
{
    switch (ev_chapter_disp.step) {
    case EV_CHAPTER_STEP_LOAD:
        if (EvChapterDispLoadReq() != 0)
            ev_chapter_disp.step = EV_CHAPTER_STEP_WAIT;
        break;
    case EV_CHAPTER_STEP_WAIT:
        if (EvChapterDispIsLoadEnd() != 0)
            ev_chapter_disp.step = EV_CHAPTER_STEP_DRAW;
        break;
    case EV_CHAPTER_STEP_DRAW:
        EvChapterDispDraw();
        break;
    case EV_CHAPTER_STEP_END:
        EvChapterDispRelease();
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The picture of the chapter in the game's language into a system-heap
// block. A chapter without a file (00 and 15) ends the title at once: 0.
// FF3 0x0019a228 (callcheck: GetSysWrk, GetLanguage (through EvChapterDispFile)).

static int EvChapterDispLoadReq(void)
{
    int file = EvChapterDispFile();

    if (GetFileSize(file) == (u_int)-1 || GetFileSize(file) == 0) {
        EvChapterDispRelease();
        return 0;
    }

    ev_chapter_tm2 = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(file), 0, 0, 0);
    FileLoadReqEE(file, ev_chapter_tm2, 4, nullptr, nullptr);
    return 1;
}

// FF3 0x0019a300 (callcheck: GetSysWrk, GetLanguage (through EvChapterDispFile)).
static int EvChapterDispIsLoadEnd(void)
{
    return FileLoadIsEnd2(EvChapterDispFile(), ev_chapter_tm2) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// The picture and its clut to VRAM (every frame it shows). FF3 0x0019a358.

static void EvChapterDispSendTex(void)
{
    Tim2GetFirstPicture(ev_chapter_tm2)->GsTex0 = CHAPTER_TITLE_PIC_TEX0;
    MakeTim2Direct(ev_chapter_tm2, 0, CHAPTER_TITLE_TBP);
    MakeClutDirect(ev_chapter_tm2, 0, CHAPTER_TITLE_CBP);
}

// ──────────────────────────────────────────────────────────────────────
// Fades the title in (a quarter sine over 10 frames), holds it 90 frames
// and fades it out (30 frames), then asks for the release. Draws the shade
// and the two halves of the picture with the fade's alpha.
// FF3 0x0019a3a0.

static void EvChapterDispDraw(void)
{
    DISP_SPRT ds;
    u_int alpha = 0x80;
    int i;

    switch (ev_chapter_disp.fade) {
    case EV_CHAPTER_FADE_START:
        ev_chapter_disp.fade = EV_CHAPTER_FADE_IN;
        ev_chapter_disp.cnt = 0;
        // fall through
    case EV_CHAPTER_FADE_IN:
        alpha = (int)(Vu0Sinf(((float)ev_chapter_disp.cnt * 1.5707964f) / 10.0f) * 128.0f) & 0xff;
        if (++ev_chapter_disp.cnt >= 10) {
            ev_chapter_disp.cnt = 0;
            ev_chapter_disp.fade = EV_CHAPTER_FADE_HOLD;
        }
        break;
    case EV_CHAPTER_FADE_HOLD:
        if (++ev_chapter_disp.cnt >= 90) {
            ev_chapter_disp.cnt = 30;
            ev_chapter_disp.fade = EV_CHAPTER_FADE_OUT;
        }
        break;
    case EV_CHAPTER_FADE_OUT:
        alpha = (int)(Vu0Sinf(((float)ev_chapter_disp.cnt * 1.5707964f) / 30.0f) * 128.0f) & 0xff;
        if (--ev_chapter_disp.cnt <= 0)
            ev_chapter_disp.step = EV_CHAPTER_STEP_END;
        break;
    }

    if (alpha == 0)
        return;

    EvChapterDispDrawShade((u_char)alpha);
    EvChapterDispSendTex();
    for (i = 0; i < 2; i++) {
        CopySprDToSpr(&ds, &chapter_title_spr[i]);
        ds.tex0 = CHAPTER_TITLE_SPR_TEX0;
        ds.alpha = (u_char)alpha;
        DispSprD(&ds);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019a590.

void EvChapterDispRelease(void)
{
    if (ev_chapter_tm2 != nullptr) {
        heapCtrlFree(GetSystemHeapWrkP(), ev_chapter_tm2, 0, 0, 0);
        ev_chapter_tm2 = nullptr;
    }
    memset(&ev_chapter_disp, 0, sizeof(ev_chapter_disp));
}

// ──────────────────────────────────────────────────────────────────────
// CHAPTER_DISP_START's request (EvChapterDispStart); ignored while a title
// shows. FF3 0x0019a5e0.

void EvChapterDispReq(u_char chapter)
{
    if (EvChapterDispIsActive() != 0)
        return;

    ev_chapter_disp.step = EV_CHAPTER_STEP_LOAD;
    ev_chapter_disp.fade = EV_CHAPTER_FADE_START;
    ev_chapter_disp.active = 1;
    ev_chapter_disp.cnt = 0;
    ev_chapter_disp.chapter = chapter;
}

// FF3 0x0019a638.
u_char EvChapterDispIsActive(void)
{
    return ev_chapter_disp.active;
}

// FF3 0x0019a648.
void EvChapterDispInit(void)
{
    memset(&ev_chapter_disp, 0, sizeof(ev_chapter_disp));
    ev_chapter_tm2 = nullptr;
}

// FF3 0x0019a670.
void EvChapterDispMain(void)
{
    if (ev_chapter_disp.active != 0)
        EvChapterDispStep();
}

// FF3 0x0019a698: an empty function nothing calls.
void EvChapterDispNop(void)
{
}

// ══════════════════════════════════════════════════════════════════════
// The event ghost tables
// ══════════════════════════════════════════════════════════════════════

static EV_GHOST_TBL ev_ghost_tbl[EV_GHOST_AREA_NUM][EV_GHOST_TBL_MAX];  // bss 0x3e1de0
static EV_GHOST_TBL ev_ghost_ctrl[EV_GHOST_TBL_MAX];   // bss 0x3e3190: the current area's set
static_assert(sizeof(ev_ghost_tbl) == 0x13b0, "the ghost tables are save block 6 (0x13b0 bytes)");

// FF3 0x0019a6a0.
int EvGhostTblSaveGet(void **addr)
{
    *addr = ev_ghost_tbl;
    return sizeof(ev_ghost_tbl);
}

// FF3 0x0019a6b8.
void EvGhostTblInit(void)
{
    int area;
    int i;

    for (area = 0; area < EV_GHOST_AREA_NUM; area++) {
        for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
            ev_ghost_tbl[area][i].id = -1;
        }
    }
    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        ev_ghost_ctrl[i].id = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Releases every ghost of the current set and frees its rows (GO_REAL_WORLD).
// FF3 0x0019a738.

void EvGhostCtrlReleaseAll(void)
{
    int i;

    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        if (ev_ghost_ctrl[i].id == -1)
            return;
        EneReleaseReq((u_char)ev_ghost_ctrl[i].type, (short)ev_ghost_ctrl[i].id);
        ev_ghost_ctrl[i].id = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0019a7a0.

static int EvGhostTblIsRegist(int area, int type, int id)
{
    EV_GHOST_TBL *row = ev_ghost_tbl[area];
    int i;

    for (i = 0; i < EV_GHOST_TBL_MAX; i++, row++) {
        if (row->id == id && row->type == type)
            return 1;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// GHOST_REGIST / GHOST_PRELOAD_REGIST: the ghost into the area's first free
// row (nothing when the table is full). FF3 asks EvGhostTblIsRegist first and
// ignores the answer, so a ghost can be in a table twice.
// FF3 0x0019a7f0.

void EvGhostTblRegist(int area, int type, int id, int preload)
{
    EV_GHOST_TBL *row;
    int i;

    EvGhostTblIsRegist(area, type, id);

    row = ev_ghost_tbl[area];
    for (i = 0; i < EV_GHOST_TBL_MAX; i++, row++) {
        if (row->id == -1) {
            row->preload = preload;
            row->type = type;
            row->id = id;
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// GHOST_DELETE: the ghost's row out of the area's table, the rows after it
// moved up one. FF3 0x0019a880.

void EvGhostTblDelete(int area, int type, int id)
{
    EV_GHOST_TBL *tbl = ev_ghost_tbl[area];
    int i;

    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        if (tbl[i].id == id && tbl[i].type == type)
            break;
    }
    if (i == EV_GHOST_TBL_MAX)
        return;

    for (; i < EV_GHOST_TBL_MAX - 1; i++) {
        tbl[i] = tbl[i + 1];
    }
    tbl[i].id = -1;
}

// ──────────────────────────────────────────────────────────────────────
// The player entered area: its table becomes the current set. Ghosts of the
// old set that the new one does not hold are released; ghosts of the new
// set the old one did not hold are loaded, unless one is already in an
// enemy work. MapLoadSetNowRoom, MapDoorRoomChange.
// FF3 0x0019a930.

void EvGhostAreaChange(int area)
{
    EV_GHOST_TBL old_ctrl[EV_GHOST_TBL_MAX];
    int keep[EV_GHOST_TBL_MAX];
    int i;
    int j;

    memset(keep, 0, sizeof(keep));
    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        old_ctrl[i] = ev_ghost_ctrl[i];
    }
    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        ev_ghost_ctrl[i] = ev_ghost_tbl[area][i];
    }

    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        if (old_ctrl[i].id == -1)
            break;
        for (j = 0; j < EV_GHOST_TBL_MAX; j++) {
            if (ev_ghost_ctrl[j].id == old_ctrl[i].id && ev_ghost_ctrl[j].type == old_ctrl[i].type) {
                keep[j] = 1;
                break;
            }
        }
        if (j == EV_GHOST_TBL_MAX)
            EneReleaseReq((u_char)old_ctrl[i].type, (short)old_ctrl[i].id);
    }

    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        if (ev_ghost_ctrl[i].id == -1)
            return;
        if (keep[i] != 1
            && SearchEneWrkNo((u_char)ev_ghost_ctrl[i].type, (short)ev_ghost_ctrl[i].id) < 0) {
            EneLoadReq((u_char)ev_ghost_ctrl[i].type, (short)ev_ghost_ctrl[i].id);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// 0 while a preload ghost of the current set is still loading (EneGetState
// 1). The scan stops with 1 at the first free row or the first row that is
// not a preload. MapAreaChange and MapDoorModeWait wait on it.
// FF3 0x0019ab00 (FF2 ev_macro.c's EventEnd_StreamRelease held this place).

int EvGhostPreloadIsEnd(void)
{
    int i;

    for (i = 0; i < EV_GHOST_TBL_MAX; i++) {
        if (ev_ghost_ctrl[i].id == -1)
            return 1;
        if ((ev_ghost_ctrl[i].preload & 1) == 0)    // FF3 tests bit 0 of the low byte
            return 1;
        if (EneGetState((u_char)ev_ghost_ctrl[i].type, (short)ev_ghost_ctrl[i].id) == 1)
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when the area's table holds the ghost: EneRelease keeps it.
// FF3 0x0019ab98.

int EvGhostTblExist(int area, int type, int id)
{
    EV_GHOST_TBL *row = ev_ghost_tbl[area];
    int i;

    for (i = 0; i < EV_GHOST_TBL_MAX; i++, row++) {
        if (row->type == type && row->id == id)
            return 1;
    }
    return 0;
}
