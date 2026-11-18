/* ==========================================================================
 *  album/prg/album_load.h
 *
 *  FF3 0x125318..0x126738 (FF2 album_load.c): the album's memory-card load
 *  screen, run by phase 28. Pick a MEMORY CARD slot, the five card albums
 *  are listed, pick one, confirm, and its second file replaces the current
 *  album. FF2's machine with the same FF3 changes as album_save.c (the
 *  message id moved into the work, steps 4/5 read each card album's first
 *  file for the list, an error page hold at +0x18). FF3 has no drawing here:
 *  album/prg/album_load_scr.c shows the page from album_load_ctrl.
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_LOAD_H
#define _ALBUM_PRG_ALBUM_LOAD_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"

#define ALBUM_LOAD_ALBUM_MAX    5   // album directories on a card

// album_load_ctrl.step (AlbumLoadMain 0x1253b0)
enum ALBUM_LOAD_STEP
{
    ALBUM_LOAD_DISP_INIT = 0,       // AlbumLoadDispInit, AlbumMemInit
    ALBUM_LOAD_LOAD_WAIT = 1,       // AlbumMemMain
    ALBUM_LOAD_MC_EXE    = 2,       // slot select / the card machine
    ALBUM_LOAD_OUT       = 3,       // Main returns 1 once AlbumMemMain says so
};

// album_load_ctrl.mode
enum ALBUM_LOAD_MODE
{
    ALBUM_LOAD_MODE_SLOT_SEL = 0,
    ALBUM_LOAD_MODE_MC_LOAD  = 1,
};

// album_load_ctrl.mc_step (AlbumLoadMcLoad 0x1255e0; each Init falls into
// its Wait). FF2's steps with FF3's two album-file reads at 4/5.
enum ALBUM_LOAD_MC_STEP
{
    ALBUM_LOAD_MC_CARD_CHECK_INIT     = 0,
    ALBUM_LOAD_MC_CARD_CHECK_WAIT     = 1,
    ALBUM_LOAD_MC_GET_DIR_INFO_INIT   = 2,
    ALBUM_LOAD_MC_GET_DIR_INFO_WAIT   = 3,
    ALBUM_LOAD_MC_ALBUM_LOAD_INIT     = 4,      // FF3-only
    ALBUM_LOAD_MC_ALBUM_LOAD_WAIT     = 5,      // FF3-only
    ALBUM_LOAD_MC_LOAD_ALBUM_SEL_INIT = 6,
    ALBUM_LOAD_MC_LOAD_ALBUM_SEL_WAIT = 7,
    ALBUM_LOAD_MC_CHECK_AGAIN_INIT    = 8,
    ALBUM_LOAD_MC_CHECK_AGAIN_WAIT    = 9,
    ALBUM_LOAD_MC_LOAD_CONF_INIT      = 10,
    ALBUM_LOAD_MC_LOAD_CONF_WAIT      = 11,
    ALBUM_LOAD_MC_LOAD_INIT           = 12,
    ALBUM_LOAD_MC_LOAD_WAIT           = 13,
    ALBUM_LOAD_MC_ERROR_CONF_INIT     = 14,
    ALBUM_LOAD_MC_ERROR_CONF_WAIT     = 15,
    ALBUM_LOAD_MC_END_CONF            = 16,
};

// The work (bss 0x2f1570; AlbumLoadCtrlInit 0x125318 memsets 0x1c bytes,
// conf_csr starts at 1). The order of the bytes differs from album_save_ctrl. FF2's ALBUM_LOAD_CTRL with the message id of FF2's
// ALBUM_LOAD_DISP at +8 (AlbumLoadDispInit sets 0xc).
struct ALBUM_LOAD_CTRL              /* 0x1c */
{
    /* 0x00 */ s_char step;             // ALBUM_LOAD_STEP
    /* 0x01 */ s_char mc_step;          // ALBUM_LOAD_MC_STEP
    /* 0x02 */ s_char mode;             // ALBUM_LOAD_MODE
    /* 0x03 */ s_char dir_check_cnt;    // card album probed by steps 2..5
    /* 0x04 */ s_char slot_csr;         // MEMORY CARD port
    /* 0x05 */ s_char album_sel_csr;    // album, = dir_label - 1
    /* 0x06 */ s_char conf_csr;         // 0 yes, 1 no
    /* 0x07 */ s_char load_file_cnt;    // only cleared (FF2 counted loaded files)
    /* 0x08 */ int    msg_id;           // the page's message (bank 0x83)
    /* 0x0c */ s_char album_flg[ALBUM_LOAD_ALBUM_MAX];  // per card album: 1 valid, -1 broken, 0 none
    /* 0x11 */ u_char _11[3];
    /* 0x14 */ int    load_done;        // 1 after an album was loaded; phase 28 (one_Album_Load) reports it
    /* 0x18 */ int    err_wait;         // error page: frames before a missing card is reported (75 PAL / 90 NTSC)
};
static_assert(sizeof(ALBUM_LOAD_CTRL) == 0x1c, "ALBUM_LOAD_CTRL is 0x1c bytes");
static_assert(offsetof(ALBUM_LOAD_CTRL, conf_csr) == 0x06, "ALBUM_LOAD_CTRL.conf_csr");
static_assert(offsetof(ALBUM_LOAD_CTRL, msg_id) == 0x08, "ALBUM_LOAD_CTRL.msg_id");
static_assert(offsetof(ALBUM_LOAD_CTRL, album_flg) == 0x0c, "ALBUM_LOAD_CTRL.album_flg");
static_assert(offsetof(ALBUM_LOAD_CTRL, load_done) == 0x14, "ALBUM_LOAD_CTRL.load_done");
static_assert(offsetof(ALBUM_LOAD_CTRL, err_wait) == 0x18, "ALBUM_LOAD_CTRL.err_wait");
static_assert(offsetof(ALBUM_LOAD_CTRL, slot_csr) == 0x04, "ALBUM_LOAD_CTRL.slot_csr");
static_assert(offsetof(ALBUM_LOAD_CTRL, album_sel_csr) == 0x05, "ALBUM_LOAD_CTRL.album_sel_csr");

extern ALBUM_LOAD_CTRL album_load_ctrl;                 // bss 0x2f1570

// Phase 28's machine (album/prg/album_phase.c 0x128c78 runs it).
void AlbumLoadCtrlInit(void);       // FF3 0x00125318: reset, take the card (MemoryCardExeInit)
int  AlbumLoadMain(void);           // FF3 0x001253b0: one frame; 1 when the page is finished
void AlbumLoadEnd(void);            // FF3 0x00126710: give the card back (MemoryCardEnd)

#endif /* _ALBUM_PRG_ALBUM_LOAD_H */
