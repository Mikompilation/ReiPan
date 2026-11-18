/* ==========================================================================
 *  common/cmn_caption.c
 *
 *  Button-guide captions from CAPTION_PK4 (see cmn_caption.h). FF3-only,
 *  FF3 0x13e6a8..0x13efa0. No retail caller.
 * ======================================================================== */
#include "common/cmn_caption.h"

#include <stddef.h>                     // offsetof

#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, DISP_SPRT, CopySprDToSpr, DispSprD
#include "graphics/graph2d/tim2.h"      // PK4SendVram
#include "system/eeiop/cd_file_dat.h"   // CAPTION_PK4
#include "system/eeiop/cddat.h"         // GetFileSize
#include "system/os/eecdvd.h"           // LoadReq
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP

// One picture of a caption line: a caption_spr index at a screen position.
struct CAPTION_ITEM                 /* 8 */
{
    int   spr;
    short x;
    short y;
};
static_assert(sizeof(CAPTION_ITEM) == 8, "CAPTION_ITEM is 8 bytes on the PS2");
static_assert(offsetof(CAPTION_ITEM, x) == 4, "CAPTION_ITEM.x");

// A caption line: its pictures.
// layout: host -- item is an EE pointer (4 bytes) there; host-only rodata.
struct CAPTION_LINE
{
    int                 num;
    const CAPTION_ITEM *item;
};

// The pictures of both pages (tex0 0x...abc0: page 0, the pad icons;
// 0x...2bd0: page 1, the words). FF3 builds this table at start-up:
// FF3 0x0013e890 (__static_initialization_and_destruction_0) stores every
// field, FF3 0x0013ef80 (_GLOBAL__I_, the .ctors entry at 0x2e911c) calls
// it; a static initializer does the same here. bss 0x3910c8.
static SPRT_DAT caption_spr[CAPTION_SPR_NUM] =
{
    { 0x20058a059d40abc0ull,  63,   1,  24, 24, 0, 0, 0, 0x80, 0, 1 },   //  0 cross
    { 0x20058a059d40abc0ull,  89,   1,  24, 24, 0, 0, 0, 0x80, 0, 1 },   //  1 triangle
    { 0x20058a059d40abc0ull,  63,  27,  24, 24, 0, 0, 0, 0x80, 0, 1 },   //  2 circle
    { 0x20058a059d40abc0ull,  89,  27,  24, 24, 0, 0, 0, 0x80, 0, 1 },   //  3 square
    { 0x20058a059d40abc0ull,   1,   1,  60, 24, 0, 0, 0, 0x80, 0, 1 },   //  4 L1 R1
    { 0x20058a059d40abc0ull,   1,  27,  60, 24, 0, 0, 0, 0x80, 0, 1 },   //  5 L2 R2
    { 0x20058a059d40abc0ull, 115,   1,  12, 24, 0, 0, 0, 0x80, 0, 1 },   //  6 left/right dots
    { 0x20058a059d40abc0ull, 115,  27,  12, 24, 0, 0, 0, 0x80, 0, 1 },   //  7 up/down dots
    { 0x20058a059d40abc0ull,   1,  52,  53, 11, 0, 0, 0, 0x80, 0, 1 },   //  8 SELECT
    { 0x20058a059d40abc0ull,  56,  52,  53, 11, 0, 0, 0, 0x80, 0, 1 },   //  9 SELECT (second)
    { 0x20058a059d40abc0ull,   1,   1,  31, 24, 0, 0, 0, 0x80, 0, 1 },   // 10 L1
    { 0x20058a2621412bd0ull,  59,  53,  69, 24, 0, 0, 0, 0x80, 0, 1 },   // 11 "Confirm"
    { 0x20058a2621412bd0ull,  59, 131,  60, 24, 0, 0, 0, 0x80, 0, 1 },   // 12 "Return"
    { 0x20058a2621412bd0ull, 146,   1,  85, 24, 0, 0, 0, 0x80, 0, 1 },   // 13 "Next Page"
    { 0x20058a2621412bd0ull,  59, 105,  61, 24, 0, 0, 0, 0x80, 0, 1 },   // 14 "Change"
    { 0x20058a2621412bd0ull,  59, 157,  60, 24, 0, 0, 0, 0x80, 0, 1 },   // 15 "Resize"
    { 0x20058a2621412bd0ull, 122, 105, 119, 24, 0, 0, 0, 0x80, 0, 1 },   // 16 "Change Floors"
    { 0x20058a2621412bd0ull,  59,  79,  69, 24, 0, 0, 0, 0x80, 0, 1 },   // 17 "Protect"
    { 0x20058a2621412bd0ull,   1, 157,  56, 24, 0, 0, 0, 0x80, 0, 1 },   // 18 "Delete"
    { 0x20058a2621412bd0ull,   1, 183,  43, 24, 0, 0, 0, 0x80, 0, 1 },   // 19 "Menu"
    { 0x20058a2621412bd0ull,   1, 105,  42, 24, 0, 0, 0, 0x80, 0, 1 },   // 20 "Next"
    { 0x20058a2621412bd0ull,  59,   1,  85, 24, 0, 0, 0, 0x80, 0, 1 },   // 21 "Move Page"
    { 0x20058a2621412bd0ull,  59, 183,  64, 24, 0, 0, 0, 0x80, 0, 1 },   // 22 "Replay"
    { 0x20058a2621412bd0ull,  59,  27,  82, 24, 0, 0, 0, 0x80, 0, 1 },   // 23 "Notebook"
    { 0x20058a2621412bd0ull,   1,   1,  43, 24, 0, 0, 0, 0x80, 0, 1 },   // 24 "Copy"
    { 0x20058a2621412bd0ull, 130,  79, 123, 24, 0, 0, 0, 0x80, 0, 1 },   // 25 "Change Finder"
    { 0x20058a2621412bd0ull,   1,  53,  42, 24, 0, 0, 0, 0x80, 0, 1 },   // 26 "Stop"
    { 0x20058a2621412bd0ull,   1, 131,  43, 24, 0, 0, 0, 0x80, 0, 1 },   // 27 "Peek"
    { 0x20058a2621412bd0ull, 143,  27, 103, 24, 0, 0, 0, 0x80, 0, 1 },   // 28 "Change Mode"
    { 0x20058a2621412bd0ull,   1,  27,  51, 24, 0, 0, 0, 0x80, 0, 1 },   // 29 "Crawl"
    { 0x20058a2621412bd0ull,   1,  79,  43, 24, 0, 0, 0, 0x80, 0, 1 },   // 30 "Push"
    { 0x20058a2621412bd0ull, 130,  53, 123, 24, 0, 0, 0, 0x80, 0, 1 },   // 31 "Change Screen"
    { 0x20058a2621412bd0ull,   1, 209,  31, 24, 0, 0, 0, 0x80, 0, 1 },   // 32 "R1" badge
};

static const CAPTION_ITEM caption_line00[5] =   // rdata 0x34f3e0
{
    {  0, 446,  21 },
    {  1, 537,  21 },
    { 11, 468,  21 },
    { 12, 559,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line01[11] =   // rdata 0x34f408
{
    {  0, 160,  21 },
    {  1, 537,  21 },
    {  4, 251,  21 },
    {  5, 418,  21 },
    {  6, 394,  21 },
    {  7, 406,  21 },
    { 11, 182,  21 },
    { 12, 559,  21 },
    { 13, 309,  21 },
    { 14, 476,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line02[4] =   // rdata 0x34f460
{
    {  0, 513,  21 },
    {  1, 537,  21 },
    { 12, 559,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line03[7] =   // rdata 0x34f480
{
    {  0, 278,  21 },
    {  1, 537,  21 },
    {  4, 360,  21 },
    { 12, 559,  21 },
    { 15, 300,  21 },
    { 16, 418,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line04[9] =   // rdata 0x34f4b8
{
    {  0, 303,  21 },
    {  1, 537,  21 },
    {  2, 394,  21 },
    {  3, 472,  21 },
    { 12, 559,  21 },
    { 17, 325,  21 },
    { 18, 416,  21 },
    { 19, 494,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line05[10] =   // rdata 0x34f500
{
    {  0, 247,  21 },
    {  1, 537,  21 },
    {  4, 418,  21 },
    {  6, 311,  21 },
    {  7, 323,  21 },
    { 12, 559,  21 },
    { 14, 476,  21 },
    { 20, 269,  21 },
    { 21, 333,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line06[9] =   // rdata 0x34f550
{
    {  0, 303,  21 },
    {  1, 537,  21 },
    {  5, 418,  21 },
    {  6, 394,  21 },
    {  7, 406,  21 },
    { 11, 325,  21 },
    { 12, 559,  21 },
    { 14, 476,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line07[12] =   // rdata 0x34f598
{
    {  0, 247,  21 },
    {  1, 537,  21 },
    {  2, 374,  70 },
    {  4, 418,  21 },
    {  6, 311,  21 },
    {  7, 323,  21 },
    { 12, 559,  21 },
    { 14, 476,  21 },
    { 20, 269,  21 },
    { 21, 333,  21 },
    { 31, 398,  70 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line08[7] =   // rdata 0x34f5f8
{
    {  1, 537,  21 },
    {  4, 418,  21 },
    {  6, 394,  21 },
    {  7, 406,  21 },
    { 12, 559,  21 },
    { 14, 476,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line09[7] =   // rdata 0x34f630
{
    {  0, 303,  21 },
    {  1, 537,  21 },
    {  4, 394,  21 },
    { 11, 325,  21 },
    { 12, 559,  21 },
    { 13, 452,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line10[3] =   // rdata 0x34f668
{
    {  0, 555,  21 },
    { 20, 577,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line11[7] =   // rdata 0x34f680
{
    {  0, 308,  21 },
    {  1, 537,  21 },
    {  4, 394,  21 },
    { 12, 559,  21 },
    { 13, 452,  21 },
    { 22, 330,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line12[7] =   // rdata 0x34f6b8
{
    {  0, 335,  21 },
    {  1, 537,  21 },
    { 10, 426,  21 },
    { 11, 357,  21 },
    { 12, 559,  21 },
    { 23, 455,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line13[9] =   // rdata 0x34f6f0
{
    {  0, 329,  21 },
    {  1, 537,  21 },
    {  2, 394,  21 },
    {  3, 472,  21 },
    { 12, 559,  21 },
    { 18, 416,  21 },
    { 19, 494,  21 },
    { 24, 351,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line14[8] =   // rdata 0x34f738
{
    {  0, 366,  21 },
    {  1, 537,  21 },
    {  6, 430,  21 },
    {  7, 442,  21 },
    { 12, 559,  21 },
    { 20, 388,  21 },
    { 21, 452,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line15[7] =   // rdata 0x34f778
{
    {  0, 393,  21 },
    {  1, 484,  21 },
    {  8, 508,  22 },
    {  9, 508,  33 },
    { 11, 415,  21 },
    { 12, 559,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line16[5] =   // rdata 0x34f7b0
{
    {  0, 537,  21 },
    {  1, 392,  21 },
    { 12, 559,  21 },
    { 25, 414,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line17[4] =   // rdata 0x34f7d8
{
    {  0, 531,  21 },
    {  1, 555,  21 },
    { 26, 577,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line18[3] =   // rdata 0x34f7f8
{
    {  1, 537,  21 },
    { 12, 559,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line19[11] =   // rdata 0x34f810
{
    {  0, 238,  21 },
    {  1, 537,  21 },
    {  3, 329,  21 },
    {  5, 418,  21 },
    {  6, 394,  21 },
    {  7, 406,  21 },
    { 11, 260,  21 },
    { 12, 559,  21 },
    { 14, 476,  21 },
    { 19, 351,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line20[3] =   // rdata 0x34f868
{
    {  0, 554,  21 },
    { 27, 576,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line21[7] =   // rdata 0x34f880
{
    {  0, 321,  21 },
    {  1, 537,  21 },
    {  3, 412,  21 },
    { 11, 343,  21 },
    { 12, 559,  21 },
    { 28, 434,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line22[5] =   // rdata 0x34f8b8
{
    {  0, 450,  21 },
    {  1, 537,  21 },
    { 12, 559,  21 },
    { 22, 472,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line23[3] =   // rdata 0x34f8e0
{
    {  0, 554,  21 },
    { 30, 575,  21 },
    { 32, 179, 455 },
};
static const CAPTION_ITEM caption_line24[4] =   // rdata 0x34f8f8
{
    {  0, 537,  21 },
    { 12, 559,  21 },
    { 25, 414,  21 },
    { 32, 383,  21 },
};

static const CAPTION_LINE caption_line_tbl[CAPTION_LINE_NUM] =    // rdata 0x34f318
{
    {  5, caption_line00 },
    { 11, caption_line01 },
    {  4, caption_line02 },
    {  7, caption_line03 },
    {  9, caption_line04 },
    { 10, caption_line05 },
    {  9, caption_line06 },
    { 12, caption_line07 },
    {  7, caption_line08 },
    {  7, caption_line09 },
    {  3, caption_line10 },
    {  7, caption_line11 },
    {  7, caption_line12 },
    {  9, caption_line13 },
    {  8, caption_line14 },
    {  7, caption_line15 },
    {  5, caption_line16 },
    {  4, caption_line17 },
    {  3, caption_line18 },
    { 11, caption_line19 },
    {  3, caption_line20 },
    {  7, caption_line21 },
    {  5, caption_line22 },
    {  3, caption_line23 },
    {  4, caption_line24 },
};

// The loaded pack (heap memory, sdata 0x36bec8; nullptr: not loaded).
static void *caption_pk4;

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e6a8.

void CmnCaptionDisp(int no, u_char alpha)
{
    DISP_SPRT ds;

    if (no == -1)
        return;
    const CAPTION_LINE *line = &caption_line_tbl[no];   // FF3 also tests this address against 0

    CmnCaptionSendVram();
    for (int i = 0; i < line->num; i++)
    {
        const CAPTION_ITEM *item = &line->item[i];
        CopySprDToSpr(&ds, &caption_spr[item->spr]);
        ds.x = (float)item->x;
        ds.y = (float)item->y;
        ds.alpha = alpha;
        ds.pri = 0xa0;
        ds.z = 0xfff5f;
        DispSprD(&ds);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e7a8.

void CmnCaptionLoadReq(void)
{
    caption_pk4 = heapCtrlMalloc(GetSystemHeapWrkP(), GetFileSize(CAPTION_PK4), 0, 0, 0);
    if (caption_pk4 != nullptr)
        LoadReq(CAPTION_PK4, caption_pk4);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e810.

void CmnCaptionSendVram(void)
{
    if (caption_pk4 != nullptr)
        PK4SendVram(caption_pk4, 0, -1, -1);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0013e848.

void CmnCaptionRelease(void)
{
    if (caption_pk4 != nullptr)
    {
        heapCtrlFree(GetSystemHeapWrkP(), caption_pk4, 0, 0, 0);
        caption_pk4 = nullptr;
    }
}
