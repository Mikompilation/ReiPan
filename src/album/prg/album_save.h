/* ==========================================================================
 *  album/prg/album_save.h
 *
 *  FF3 0x126f68..0x128a58 (FF2 album_save.c): the album's memory-card save
 *  screen, run by phase 27 (0x128a68 / 0x128ab8, a child of phase 8). Pick a
 *  MEMORY CARD slot, the screen lists the five albums on the card, pick one,
 *  confirm, and the current album is written there (two files). FF2's machine
 *  with two FF3 additions: steps 4/5 load each card album's first file for
 *  the list (AlbumSaveMcAlbumLoad*), and an error page hold (+0x1c). FF3 has
 *  no drawing here: album/prg/album_save_scr.c shows the page from
 *  album_save_ctrl (the B2d layout code after plyr_gauge.c, 0x12a068..,
 *  "hensyu_albmslct", reads it too).
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_SAVE_H
#define _ALBUM_PRG_ALBUM_SAVE_H

#include <stddef.h>                 // offsetof
#include "sdk/scetypes.h"

#define ALBUM_SAVE_ALBUM_MAX    5   // album directories on a card (dir_label 1..5)

// album_save_ctrl.step
enum ALBUM_SAVE_STEP
{
    ALBUM_SAVE_DISP_INIT = 0,       // claim the staging buffer
    ALBUM_SAVE_LOAD_WAIT = 1,       // wait for it
    ALBUM_SAVE_MC_EXE    = 2,       // slot select / the card machine
    ALBUM_SAVE_OUT       = 3,       // buffer given back; Main returns 1 once AlbumMemMain says so
};

// album_save_ctrl.mode
enum ALBUM_SAVE_MODE
{
    ALBUM_SAVE_MODE_SLOT_SEL = 0,
    ALBUM_SAVE_MODE_MC_SAVE  = 1,
};

// album_save_ctrl.mc_step (jump table rodata 0x34e018; each Init falls into its Wait).
enum ALBUM_SAVE_MC_STEP
{
    ALBUM_SAVE_MC_CARD_CHECK_INIT     = 0,
    ALBUM_SAVE_MC_CARD_CHECK_WAIT     = 1,
    ALBUM_SAVE_MC_GET_DIR_INFO_INIT   = 2,
    ALBUM_SAVE_MC_GET_DIR_INFO_WAIT   = 3,
    ALBUM_SAVE_MC_ALBUM_LOAD_INIT     = 4,      // FF3-only
    ALBUM_SAVE_MC_ALBUM_LOAD_WAIT     = 5,      // FF3-only
    ALBUM_SAVE_MC_SAVE_ALBUM_SEL_INIT = 6,
    ALBUM_SAVE_MC_SAVE_ALBUM_SEL_WAIT = 7,
    ALBUM_SAVE_MC_CHECK_AGAIN_INIT    = 8,
    ALBUM_SAVE_MC_CHECK_AGAIN_WAIT    = 9,
    ALBUM_SAVE_MC_SAVE_CONF_INIT      = 10,
    ALBUM_SAVE_MC_SAVE_CONF_WAIT      = 11,
    ALBUM_SAVE_MC_SAVE_INIT           = 12,
    ALBUM_SAVE_MC_SAVE_WAIT           = 13,
    ALBUM_SAVE_MC_ERROR_CONF_INIT     = 14,
    ALBUM_SAVE_MC_ERROR_CONF_WAIT     = 15,
    ALBUM_SAVE_MC_END_CONF            = 16,
    ALBUM_SAVE_MC_REMAKE_CONF_INIT    = 17,
    ALBUM_SAVE_MC_REMAKE_CONF_WAIT    = 18,
    ALBUM_SAVE_MC_REMAKE_DIR_DEL_INIT = 19,
    ALBUM_SAVE_MC_REMAKE_DIR_DEL_WAIT = 20,
    ALBUM_SAVE_MC_NEW_MAKE_CONF_INIT  = 21,
    ALBUM_SAVE_MC_NEW_MAKE_CONF_WAIT  = 22,
    ALBUM_SAVE_MC_NEW_MAKE_INIT       = 23,
    ALBUM_SAVE_MC_NEW_MAKE_WAIT       = 24,
    ALBUM_SAVE_MC_FORMAT_CONF_INIT    = 25,
    ALBUM_SAVE_MC_FORMAT_CONF_WAIT    = 26,
    ALBUM_SAVE_MC_FORMAT_INIT         = 27,
    ALBUM_SAVE_MC_FORMAT_WAIT         = 28,
    ALBUM_SAVE_MC_FORMAT_END_INIT     = 29,
    ALBUM_SAVE_MC_FORMAT_END_WAIT     = 30,
};

// The work (bss 0x2f1590; AlbumSaveCtrlInit memsets 0x20 bytes). FF2 kept the
// message id in a separate ALBUM_SAVE_DISP; FF3 has it at +0xc. The display
// code at 0x12f470 / 0x12f9c8 reads step, mc_step, mode and dir_check_cnt.
struct ALBUM_SAVE_CTRL              /* 0x20 */
{
    /* 0x00 */ s_char step;             // ALBUM_SAVE_STEP
    /* 0x01 */ s_char mc_step;          // ALBUM_SAVE_MC_STEP
    /* 0x02 */ s_char mode;             // ALBUM_SAVE_MODE
    /* 0x03 */ s_char slot_csr;         // MEMORY CARD port
    /* 0x04 */ s_char album_sel_csr;    // album, = dir_label - 1
    /* 0x05 */ s_char conf_csr;         // 0 yes, 1 no
    /* 0x06 */ s_char dir_check_cnt;    // album probed by steps 2..5 (left at 5 once the list is read)
    /* 0x07 */ s_char save_file_cnt;    // files written so far (two per album)
    /* 0x08 */ s_char format_end_cnt;   // frames "format successful" has been held
    /* 0x09 */ u_char _09[3];
    /* 0x0c */ int    msg_id;           // the page's message (ALBUM_SAVE_MSG_*)
    /* 0x10 */ s_char album_flg[ALBUM_SAVE_ALBUM_MAX];  // per card album: 1 valid, -1 broken, 0 none
    /* 0x15 */ u_char _15[3];
    /* 0x18 */ int    save_done;        // 1 after a save went through
    /* 0x1c */ int    err_wait;         // error page: frames before a missing card is reported (75 PAL / 90 NTSC)
};
static_assert(sizeof(ALBUM_SAVE_CTRL) == 0x20, "ALBUM_SAVE_CTRL is 0x20 bytes");
static_assert(offsetof(ALBUM_SAVE_CTRL, msg_id) == 0x0c, "ALBUM_SAVE_CTRL.msg_id");
static_assert(offsetof(ALBUM_SAVE_CTRL, album_flg) == 0x10, "ALBUM_SAVE_CTRL.album_flg");
static_assert(offsetof(ALBUM_SAVE_CTRL, save_done) == 0x18, "ALBUM_SAVE_CTRL.save_done");
static_assert(offsetof(ALBUM_SAVE_CTRL, err_wait) == 0x1c, "ALBUM_SAVE_CTRL.err_wait");

extern ALBUM_SAVE_CTRL album_save_ctrl;                 // bss 0x2f1590

void AlbumSaveCtrlInit(void);       // FF3 0x00126f68: reset, take the card (MemoryCardExeInit)
int  AlbumSaveMain(void);           // FF3 0x00127008: one frame; 1 when the page is finished
void AlbumSaveEnd(void);            // FF3 0x00128a30: give the card back

#endif /* _ALBUM_PRG_ALBUM_SAVE_H */
