/* ==========================================================================
 *  save_load/prg/game_data_save.c
 *
 *  The memory-card save of phase 69 (FF3 0x1d5b40..0x1d7a58).
 *  ingame/story_save_mc.c calls McSaveGameInit, then McSaveGameMain every
 *  frame until it answers 1, then GameDataSaveEnd; the save screen
 *  (outgame/save_scr.c) draws mc_save_wrk meanwhile and opens its yes/no
 *  window with mc_save_wrk.msg.
 *
 *  The steps (MGS_STEP_*, game_data_save.h; each "init" step falls into
 *  its "wait" step in the same frame):
 *    0/1   look for the game directory on port 0 (none: 23, damaged: 19,
 *          unformatted: 29);
 *    2/3   load card file 1, the five play data heads;
 *    4/5   load each save's map snapshot and chapter title texture;
 *    6/7   the player picks a slot (a save or the first empty one) and
 *          confirms;
 *    8/9   the slot fades out on the screen;
 *    10/11 load card file 0, the system file, and merge it into the game's;
 *    12/13 write the system file, the game file (slot + 2) and the heads;
 *    14/15 an error message until OK / cancel (a new card restarts at 0);
 *    16/17 the written slot's textures again, then 18 "saved";
 *    19-22 remake a damaged directory, 23-28 make a new one and write the
 *          first save into slot 0, 29-34 format the card first.
 *  title_start_flg.save says whether a game was written.
 *
 *  Ported from FF2's game_data_save.c (MioPan) onto McLoadGame's shapes
 *  (game_data_load.c): FF3 dropped FF2's screen steps and save kinds and
 *  added the fade-out and the reload.
 * ======================================================================== */
#include "save_load/prg/game_data_save.h"

#include <string.h>

#include "ingame/event/prg/ev_main.h"           // EventEnd, EventAfterSave
#include "ingame/ingame_wrk.h"                  // title_start_flg, TitleMenuItemOpenBackup / Merge
#include "ingame/map/MapSnap.h"                 // MapSnapGetNo
#include "outgame/save_scr.h"                   // SaveScrMsgIsOpen, SaveScrIsSlotFadedOut
#include "system/eeiop/cd_file_dat.h"           // RS101_R01_TM2, CLEARDATA_TM2, SAVELOAD_CHAPTER*_TM2
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/eeiop/fileload.h"              // FileLoadReqEE, FileLoadIsEnd2
#include "system/mc/dat/save_data.h"
#include "system/mc/prg/mc.h"                   // MemoryCardExeInit, MemoryCardEnd
#include "system/mc/prg/mc_check.h"             // MemoryCardCheckInit / Main
#include "system/mc/prg/mc_check_broken.h"      // dir / file broken, new file
#include "system/mc/prg/mc_check_card.h"        // MemoryCardSetAccessPort, the card watch
#include "system/mc/prg/mc_check_empty.h"       // room for a directory
#include "system/mc/prg/mc_del_dir.h"           // MemoryCardDirDelInit / Main
#include "system/mc/prg/mc_format.h"            // MemoryCardFormatInit / Main
#include "system/mc/prg/mc_load.h"              // MemoryCardFileLoadInit / Main
#include "system/mc/prg/mc_make.h"              // MemoryCardNewMakeInit / Main
#include "system/mc/prg/mc_save.h"              // MemoryCardFileSaveInit / Main
#include "system/mc/prg/mc_set_data.h"          // paths, sizes, buffers, play data heads
#include "system/os/system.h"                   // GetPALMode
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe / RptPlaySe, pad_key

#include "common/reipan_stub.h"
#include "ingame/sound/snd_ctrl.h"      // PC: SndSePlay
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// The ROM's message ids (kind 0x83), by where they are set.
#define MGS_MSG_NONE        0x00
#define MGS_MSG_NO_CARD     0x01            // not a PS2 card / the card went away (-0x14, -1)
#define MGS_MSG_CARD_ERROR  0x02            // any other card error
#define MGS_MSG_FORMAT_ASK  0x03            // unformatted: format it? (yes / no)
#define MGS_MSG_FORMATTING  0x04
#define MGS_MSG_FORMAT_FAIL 0x05
#define MGS_MSG_SAVE_ERROR  0x07            // a file write or load failed
#define MGS_MSG_SAVED       0x08            // the game was saved
#define MGS_MSG_SELECT      0x0a            // pick a slot
#define MGS_MSG_OVERWRITE   0x0b            // overwrite this save? (yes / no)
#define MGS_MSG_UNFORMATTED 0x0d            // the card watch / system file: unformatted (-2)
#define MGS_MSG_MAKING      0x12            // the directory is being (re)made
#define MGS_MSG_MAKE_FAIL   0x13            // deleting / making the directory failed
#define MGS_MSG_SAVED_NEW   0x14            // the first save on a new directory
#define MGS_MSG_NEW_ASK     0x15            // no game directory: make one? (yes / no)
#define MGS_MSG_REMAKE_ASK  0x16            // damaged directory: make it again? (yes / no)
#define MGS_MSG_NO_SPACE    0x17            // no room for a new directory
#define MGS_MSG_BROKEN      0x19            // the head / system file is damaged
#define MGS_MSG_SAVING      0x1e
#define MGS_MSG_SAVE_ASK    0x1f            // save in this empty slot? (yes / no)
#define MGS_MSG_FORMATTED   0x39

#define MGS_FORMAT_END_TIME 0x3b            // "formatted" ends after 60 frames
#define MGS_MSG_WAIT_PAL    75              // error messages: 3 s before a card change shows
#define MGS_MSG_WAIT_NTSC   90

#define MGS_SE_CSR          0x1a9b0000      // SE_BANK_DEFAULT: cursor
#define MGS_SE_CANCEL       0x1a9b0001
#define MGS_SE_OK           0x1a9b0003

#define MGS_PAD_LEFT        0x8000
#define MGS_PAD_RIGHT       0x2000

#define MGS_FILE_HEAD       1               // card file labels (dir 0): the play data heads
#define MGS_FILE_SYSTEM     0               // the system file; a game is slot + 2
#define MGS_FILE_GAME       2
#define MGS_SAVE_FILES      3               // files one save writes

// ══════════════════════════════════════════════════════════════════════
// Data
// ══════════════════════════════════════════════════════════════════════

MC_SAVE_WRK mc_save_wrk;                        // data 0x328358 (read by save_scr.c)
static void *game_data_save_buff;               // sdata 0x36d5ec (gp-0x2a14): the card file buffer

// The files a save writes, in order: system, game, heads. Index 1 is set to
// the chosen slot before each write (data 0x3283d8, FF2: rodata).
static int save_file_label[MGS_SAVE_FILES] = {
    MGS_FILE_SYSTEM, MGS_FILE_GAME, MGS_FILE_HEAD,
};
// The first save on a new directory goes to slot 0 (data 0x3283e8).
static const int new_save_file_label[MGS_SAVE_FILES] = {
    MGS_FILE_SYSTEM, MGS_FILE_GAME, MGS_FILE_HEAD,
};

static void McSaveGameCtrlInit(void);
static void GameDataSaveMcMain(void);
static void GameDataSaveMcCheckInit(void);
static void GameDataSaveMcCheckWait(void);
static void GameDataSaveMcHeadLoadInit(void);
static void GameDataSaveMcHeadLoadWait(void);
static void GameDataSaveMcSnapFree(int slot);
static void GameDataSaveMcSnapLoadSlot(int slot);
static void GameDataSaveMcSnapLoadInit(void);
static void GameDataSaveMcSnapLoadWait(void);
static void GameDataSaveMcSaveFileSel(void);
static void GameDataSaveMcSaveFileSelPad(void);
static void GameDataSaveMcSaveConf(void);
static void GameDataSaveMcSaveConfPad(void);
static void GameDataSaveMcFadeInit(void);
static void GameDataSaveMcFadeWait(void);
static void GameDataSaveMcSystemLoadInit(void);
static void GameDataSaveMcSystemLoadWait(void);
static void GameDataSaveMcSaveInit(void);
static void GameDataSaveMcSaveWait(void);
static void GameDataSaveMcErrorConfInit(void);
static void GameDataSaveMcErrorConfWait(void);
static int  GameDataSaveMcErrorConfPad(void);
static void GameDataSaveMcReloadInit(void);
static void GameDataSaveMcReloadWait(void);
static void GameDataSaveMcEndConf(void);
static void GameDataSaveMcRemakeConfInit(void);
static void GameDataSaveMcRemakeConfWait(void);
static void GameDataSaveMcRemakeConfPad(void);
static void GameDataSaveMcRemakeDirDelInit(void);
static void GameDataSaveMcRemakeDirDelWait(void);
static void GameDataSaveMcNewMakeConfInit(void);
static void GameDataSaveMcNewMakeConfWait(void);
static void GameDataSaveMcNewMakeConfPad(void);
static void GameDataSaveMcNewMakeInit(void);
static void GameDataSaveMcNewMakeWait(void);
static void GameDataSaveMcNewMakeSaveInit(void);
static void GameDataSaveMcNewMakeSaveWait(void);
static void GameDataSaveMcFormatConfInit(void);
static void GameDataSaveMcFormatConfWait(void);
static void GameDataSaveMcFormatConfPad(void);
static void GameDataSaveMcFormatInit(void);
static void GameDataSaveMcFormatWait(void);
static void GameDataSaveMcFormatEndInit(void);
static void GameDataSaveMcFormatEndWait(void);
static void GameDataSaveMcEveryFrameCheck(void);

// ══════════════════════════════════════════════════════════════════════
// Phase 69 entry points
// ══════════════════════════════════════════════════════════════════════

// Takes the card for port 0 and forgets every slot's textures (the
// pointers are only cleared). FF2 GameDataSaveInit's place.
// FF3 0x001d5b40.
void McSaveGameInit(void)
{
    McSaveGameCtrlInit();
    MemoryCardExeInit();
    MemoryCardSetAccessPort(0);
    game_data_save_buff = nullptr;
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        mc_save_wrk.tex[i] = nullptr;
        mc_save_wrk.snap_file[i] = -1;
        mc_save_wrk.chap_tex[i] = nullptr;
        mc_save_wrk.chap_file[i] = -1;
    }
}

// FF3 0x001d5bc8; FF2 GameDataSaveCtrlInit's place.
static void McSaveGameCtrlInit(void)
{
    memset(&mc_save_wrk, 0, 0x1c);                 // msg .. _019 (EE offsets up to tex)
    mc_save_wrk.msg = MGS_MSG_NONE;
    mc_save_wrk.yesno = 1;
    mc_save_wrk.mode = 0;
    mc_save_wrk.step = MGS_STEP_CHECK_INIT;
    mc_save_wrk.slot = 0;
    mc_save_wrk.save_cnt = 0;
    mc_save_wrk.fmt_end_timer = 0;
    mc_save_wrk.msg_timer = 0;
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
        mc_save_wrk.slot_flg[i] = 0;
}

// Two frames of start-up (the first clears title_start_flg.save), then the
// save steps until mode 3: 1.
// FF3 0x001d5c50.
int McSaveGameMain(void)
{
    switch (mc_save_wrk.mode)
    {
    case 0:
        mc_save_wrk.mode = 1;
        title_start_flg.save = 0;
        break;
    case 1:
        mc_save_wrk.mode = MC_SAVE_MODE_SLOTS;
        break;
    case MC_SAVE_MODE_SLOTS:
        GameDataSaveMcMain();
        return 0;
    case MC_SAVE_MODE_END:
        return 1;
    }
    return 0;
}

// FF3 0x001d79e0.
void GameDataSaveEnd(void)
{
    LiberateDataMemoryArea(&game_data_save_buff);
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        LiberateDataMemoryArea(&mc_save_wrk.tex[i]);
        LiberateDataMemoryArea(&mc_save_wrk.chap_tex[i]);
    }
    MemoryCardEnd();
}

// ══════════════════════════════════════════════════════════════════════
// The steps
// ══════════════════════════════════════════════════════════════════════

// FF3 0x001d5cd0: the init steps call their wait step right after (a jal
// then a tail j in the same case).
static void GameDataSaveMcMain(void)
{
    switch (mc_save_wrk.step)
    {
    case MGS_STEP_CHECK_INIT:
        GameDataSaveMcCheckInit();
        // fall through
    case MGS_STEP_CHECK_WAIT:
        GameDataSaveMcCheckWait();
        return;
    case MGS_STEP_HEAD_LOAD_INIT:
        GameDataSaveMcHeadLoadInit();
        // fall through
    case MGS_STEP_HEAD_LOAD_WAIT:
        GameDataSaveMcHeadLoadWait();
        return;
    case MGS_STEP_SNAP_LOAD_INIT:
        GameDataSaveMcSnapLoadInit();
        // fall through
    case MGS_STEP_SNAP_LOAD_WAIT:
        GameDataSaveMcSnapLoadWait();
        return;
    case MGS_STEP_SELECT:
        GameDataSaveMcSaveFileSel();
        return;
    case MGS_STEP_ASK:
        GameDataSaveMcSaveConf();
        return;
    case MGS_STEP_FADE_INIT:
        GameDataSaveMcFadeInit();
        // fall through
    case MGS_STEP_FADE_WAIT:
        GameDataSaveMcFadeWait();
        return;
    case MGS_STEP_SYS_LOAD_INIT:
        GameDataSaveMcSystemLoadInit();
        // fall through
    case MGS_STEP_SYS_LOAD_WAIT:
        GameDataSaveMcSystemLoadWait();
        return;
    case MGS_STEP_SAVE_INIT:
        GameDataSaveMcSaveInit();
        // fall through
    case MGS_STEP_SAVE_WAIT:
        GameDataSaveMcSaveWait();
        return;
    case MGS_STEP_ERROR_INIT:
        GameDataSaveMcErrorConfInit();
        // fall through
    case MGS_STEP_ERROR_WAIT:
        GameDataSaveMcErrorConfWait();
        return;
    case MGS_STEP_RELOAD_INIT:
        GameDataSaveMcReloadInit();
        // fall through
    case MGS_STEP_RELOAD_WAIT:
        GameDataSaveMcReloadWait();
        return;
    case MGS_STEP_END_CONF:
        GameDataSaveMcEndConf();
        return;
    case MGS_STEP_REMAKE_CONF_INIT:
        GameDataSaveMcRemakeConfInit();
        // fall through
    case MGS_STEP_REMAKE_CONF_WAIT:
        GameDataSaveMcRemakeConfWait();
        return;
    case MGS_STEP_DIR_DEL_INIT:
        GameDataSaveMcRemakeDirDelInit();
        // fall through
    case MGS_STEP_DIR_DEL_WAIT:
        GameDataSaveMcRemakeDirDelWait();
        return;
    case MGS_STEP_NEW_CONF_INIT:
        GameDataSaveMcNewMakeConfInit();
        // fall through
    case MGS_STEP_NEW_CONF_WAIT:
        GameDataSaveMcNewMakeConfWait();
        return;
    case MGS_STEP_NEW_MAKE_INIT:
        GameDataSaveMcNewMakeInit();
        // fall through
    case MGS_STEP_NEW_MAKE_WAIT:
        GameDataSaveMcNewMakeWait();
        return;
    case MGS_STEP_NEW_SAVE_INIT:
        GameDataSaveMcNewMakeSaveInit();
        // fall through
    case MGS_STEP_NEW_SAVE_WAIT:
        GameDataSaveMcNewMakeSaveWait();
        return;
    case MGS_STEP_FORMAT_CONF_INIT:
        GameDataSaveMcFormatConfInit();
        // fall through
    case MGS_STEP_FORMAT_CONF_WAIT:
        GameDataSaveMcFormatConfWait();
        return;
    case MGS_STEP_FORMAT_INIT:
        GameDataSaveMcFormatInit();
        // fall through
    case MGS_STEP_FORMAT_WAIT:
        GameDataSaveMcFormatWait();
        return;
    case MGS_STEP_FORMAT_END_INIT:
        GameDataSaveMcFormatEndInit();
        // fall through
    case MGS_STEP_FORMAT_END_WAIT:
        GameDataSaveMcFormatEndWait();
        return;
    }
}

// Lists the game directory ("<dir>/*") on port 0.
// FF3 0x001d5e78.
static void GameDataSaveMcCheckInit(void)
{
    char path_name[64];

    memset(path_name, 0, 55);
    MemoryCardMakeSearchDirPath(path_name, 0);
    MemoryCardCheckInit(0, 0, path_name);
    mc_save_wrk.step = MGS_STEP_CHECK_WAIT;
    mc_save_wrk.save_cnt = 0;
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
        mc_save_wrk.slot_flg[i] = 0;
}

// A whole directory goes on to the head file. A damaged one (or one that
// is there but unreadable) is offered a remake when the card has room, no
// directory a new one, an unformatted card a format; no room is an error.
// FF3 0x001d5ef0.
static void GameDataSaveMcCheckWait(void)
{
    int res = MemoryCardCheckMain();

    if (res == 1)
    {
        if (MemoryCardCheckDirBroken(0) != 0)
        {
            mc_save_wrk.msg = MGS_MSG_NONE;
            mc_save_wrk.step = MGS_STEP_HEAD_LOAD_INIT;
            return;
        }
        if (MemoryCardCheckEmptyBroken(0) != 0)
        {
            mc_save_wrk.msg = MGS_MSG_REMAKE_ASK;
            mc_save_wrk.step = MGS_STEP_REMAKE_CONF_INIT;
            mc_save_wrk.yesno = 1;
            return;
        }
    }
    else
    {
        if (res >= 0)
            return;
        switch (res)
        {
        case -0x14:                         // not a PS2 card
            mc_save_wrk.msg = MGS_MSG_NO_CARD;
            mc_save_wrk.step = MGS_STEP_ERROR_INIT;
            return;
        case -6:                            // the directory is not empty but unreadable
            if (MemoryCardCheckEmptyBroken(0) != 0)
            {
                mc_save_wrk.msg = MGS_MSG_REMAKE_ASK;
                mc_save_wrk.step = MGS_STEP_REMAKE_CONF_INIT;
                mc_save_wrk.yesno = 1;
                return;
            }
            break;
        case -4:                            // no game directory
            if (MemoryCardCheckEmpty(0) != 0)
            {
                mc_save_wrk.msg = MGS_MSG_NEW_ASK;
                mc_save_wrk.step = MGS_STEP_NEW_CONF_INIT;
                return;
            }
            break;
        case -2:                            // unformatted
            mc_save_wrk.msg = MGS_MSG_FORMAT_ASK;
            mc_save_wrk.step = MGS_STEP_FORMAT_CONF_INIT;
            mc_save_wrk.yesno = 1;
            return;
        case -1:                            // the card was swapped: look again
            mc_save_wrk.msg = MGS_MSG_NONE;
            mc_save_wrk.step = MGS_STEP_CHECK_INIT;
            return;
        default:                            // -0x13 .. -7, -5, -3 and the rest
            mc_save_wrk.msg = MGS_MSG_CARD_ERROR;
            mc_save_wrk.step = MGS_STEP_ERROR_INIT;
            return;
        }
    }
    mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    mc_save_wrk.msg = MGS_MSG_NO_SPACE;
}

// Reads card file 1 (the play data heads).
// FF3 0x001d6058.
static void GameDataSaveMcHeadLoadInit(void)
{
    char path_name[64];
    int size;

    memset(path_name, 0, 55);
    LiberateDataMemoryArea(&game_data_save_buff);
    MemoryCardSetFilePath(path_name, 0, MGS_FILE_HEAD);
    size = GetMemoryCardDataSize(0, MGS_FILE_HEAD);
    game_data_save_buff = GetDataMemoryArea(size);
    MemoryCardFileLoadInit(0, 0, path_name, game_data_save_buff, size);
    mc_save_wrk.msg = MGS_MSG_NONE;
    mc_save_wrk.step = MGS_STEP_HEAD_LOAD_WAIT;
}

// A whole head file is developed; the slots holding a save get flag 1 (a
// slot marked -1 keeps it), and with at least one save the textures load,
// else the slot screen opens empty. A damaged head file (or a short read)
// is offered a remake like a damaged directory. The buffer is freed once
// the load ends, either way.
// FF3 0x001d60e8.
static void GameDataSaveMcHeadLoadWait(void)
{
    int res = MemoryCardFileLoadMain();
    int broken = 0;

    if (res == 1)
    {
        int size = GetMemoryCardDataSize(0, MGS_FILE_HEAD);

        if (MemoryCardCheckFileBroken(game_data_save_buff, size) == 0)
        {
            broken = 1;
        }
        else if (MemoryCardCheckNewFileLoad(game_data_save_buff, size) == 0)
        {
            DevelopMemoryCardLoadData(game_data_save_buff, 0, MGS_FILE_HEAD);
            for (int i = 0; i < MC_SAVE_SLOTS; i++)
            {
                if (GetMemoryCardPlayDataFlg(i) == 1)
                {
                    if (mc_save_wrk.slot_flg[i] != -1)
                        mc_save_wrk.slot_flg[i] = 1;
                }
                else
                {
                    mc_save_wrk.slot_flg[i] = 0;
                }
            }
            mc_save_wrk.msg = MGS_MSG_SELECT;
            mc_save_wrk.step = MGS_STEP_SELECT;
            for (int i = 0; i < MC_SAVE_SLOTS; i++)
            {
                if (GetMemoryCardPlayDataFlg(i) == 1)
                {
                    mc_save_wrk.msg = MGS_MSG_NONE;
                    mc_save_wrk.step = MGS_STEP_SNAP_LOAD_INIT;
                    break;
                }
            }
        }
        else                                // a fresh head file: nothing saved yet
        {
            mc_save_wrk.msg = MGS_MSG_SELECT;
            mc_save_wrk.step = MGS_STEP_SELECT;
        }
    }
    else if (res < 0)
    {
        switch (res)
        {
        case -3:                            // short read
            broken = 1;
            break;
        case -2:                            // unformatted
            mc_save_wrk.msg = MGS_MSG_FORMAT_ASK;
            mc_save_wrk.step = MGS_STEP_FORMAT_CONF_INIT;
            mc_save_wrk.yesno = 1;
            break;
        default:                            // -10 .. -4 and the rest
            mc_save_wrk.msg = MGS_MSG_CARD_ERROR;
            mc_save_wrk.step = MGS_STEP_ERROR_INIT;
            break;
        }
    }
    if (broken)
    {
        if (MemoryCardCheckEmptyBroken(0) != 0)
        {
            mc_save_wrk.msg = MGS_MSG_REMAKE_ASK;
            mc_save_wrk.step = MGS_STEP_REMAKE_CONF_INIT;
            mc_save_wrk.yesno = 1;
        }
        else
        {
            mc_save_wrk.msg = MGS_MSG_NO_SPACE;
            mc_save_wrk.step = MGS_STEP_ERROR_INIT;
        }
    }
    if (res != 0)
        LiberateDataMemoryArea(&game_data_save_buff);
}

// Drops one slot's textures (the slot that is about to be written).
// FF3 0x001d6300.
static void GameDataSaveMcSnapFree(int slot)
{
    LiberateDataMemoryArea(&mc_save_wrk.tex[slot]);
    mc_save_wrk.tex[slot] = nullptr;
    LiberateDataMemoryArea(&mc_save_wrk.chap_tex[slot]);
    mc_save_wrk.chap_tex[slot] = nullptr;
}

// One saved slot's two textures from the CD: the map snapshot of the place
// it was saved in (finder-mode saves: the clear-data picture) and the
// chapter title (finder-mode saves: chapter 15's). A place without a
// snapshot stops after freeing the buffers. McLoadGameMcSnapLoadInit's loop
// body, except the snapshot loads at priority 5.
// FF3 0x001d6358.
static void GameDataSaveMcSnapLoadSlot(int slot)
{
    int file;
    u_int size;

    LiberateDataMemoryArea(&mc_save_wrk.tex[slot]);
    LiberateDataMemoryArea(&mc_save_wrk.chap_tex[slot]);

    if (GetMemoryCardPlayDataFinderFlg(slot) == 0)
    {
        int no = MapSnapGetNo(GetMemoryCardPlayDataArea(slot),
                              GetMemoryCardPlayDataChapter(slot));
        if (no < 0)
            return;
        file = no + RS101_R01_TM2;
    }
    else
    {
        file = CLEARDATA_TM2;
    }
    mc_save_wrk.snap_file[slot] = file;
    size = GetFileSize(mc_save_wrk.snap_file[slot]);
    if (size != (u_int)-1 && GetFileSize(mc_save_wrk.snap_file[slot]) != 0)
    {
        void *addr = GetDataMemoryArea((int)GetFileSize(mc_save_wrk.snap_file[slot]));
        mc_save_wrk.tex[slot] = addr;
        FileLoadReqEE(mc_save_wrk.snap_file[slot], addr, 5, nullptr, nullptr);
    }

    if (GetMemoryCardPlayDataFinderFlg(slot) == 0)
    {
        int chapter = GetMemoryCardPlayDataB02(slot);

        file = SAVELOAD_CHAPTER01_TM2;
        if (chapter > 0)
            file = chapter + TM2SAVELOAD_LABEL_START;
        mc_save_wrk.chap_file[slot] = file;
    }
    else
    {
        mc_save_wrk.chap_file[slot] = SAVELOAD_CHAPTER15_TM2;
    }
    size = GetFileSize(mc_save_wrk.chap_file[slot]);
    if (size != (u_int)-1 && GetFileSize(mc_save_wrk.chap_file[slot]) != 0)
    {
        void *addr = GetDataMemoryArea((int)GetFileSize(mc_save_wrk.chap_file[slot]));
        mc_save_wrk.chap_tex[slot] = addr;
        FileLoadReqEE(mc_save_wrk.chap_file[slot], addr, 3, nullptr, nullptr);
    }
}

// Every saved slot's textures; empty slots drop theirs. Then the card
// watch is armed.
// FF3 0x001d6530.
static void GameDataSaveMcSnapLoadInit(void)
{
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        if (GetMemoryCardPlayDataFlg(i) == 1)
        {
            GameDataSaveMcSnapLoadSlot(i);
        }
        else
        {
            LiberateDataMemoryArea(&mc_save_wrk.tex[i]);
            mc_save_wrk.snap_file[i] = -1;
            LiberateDataMemoryArea(&mc_save_wrk.chap_tex[i]);
            mc_save_wrk.chap_file[i] = -1;
        }
    }
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.msg = MGS_MSG_NONE;
    mc_save_wrk.step = MGS_STEP_SNAP_LOAD_WAIT;
}

// Once both textures of every saved slot are in, the slot screen opens
// with the cursor on the first save.
// FF3 0x001d6620.
static void GameDataSaveMcSnapLoadWait(void)
{
    int first = 0;

    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        if (GetMemoryCardPlayDataFlg(i) != 1)
            continue;
        if (FileLoadIsEnd2(mc_save_wrk.snap_file[i], mc_save_wrk.tex[i]) == 0
            || FileLoadIsEnd2(mc_save_wrk.chap_file[i], mc_save_wrk.chap_tex[i]) == 0)
        {
            mc_save_wrk.msg = MGS_MSG_NONE;
            mc_save_wrk.step = MGS_STEP_SNAP_LOAD_WAIT;
            break;
        }
        mc_save_wrk.msg = MGS_MSG_SELECT;
        mc_save_wrk.step = MGS_STEP_SELECT;
        if (!first)
        {
            mc_save_wrk.slot = (s_char)i;
            first = 1;
        }
    }
    GameDataSaveMcEveryFrameCheck();
}

// FF3 0x001d6740.
static void GameDataSaveMcSaveFileSel(void)
{
    GameDataSaveMcSaveFileSelPad();
    GameDataSaveMcEveryFrameCheck();
}

// The cursor runs over the saves plus one empty slot (at most five),
// wrapping; OK asks to overwrite a save or to fill the empty slot, cancel
// leaves without saving.
// FF3 0x001d6760.
static void GameDataSaveMcSaveFileSelPad(void)
{
    int num;

    if (SaveScrMsgIsOpen() == 0)
        return;
    num = 0;
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        if (GetMemoryCardPlayDataFlg(i) == 1)
            num++;
    }
    num++;
    if (num > MC_SAVE_SLOTS)
        num = MC_SAVE_SLOTS;
    mc_save_wrk.slot_num = (s_char)num;

    // PC: the mouse.
    {
        ReiPan_MenuBox box;

        ReiPan_MenuMouseBegin(REIPAN_MM_WHEEL_HORIZONTAL, nullptr);
        if (SaveScrMouseBox(&box.x, &box.y, &box.w, &box.h))
        {
            const int to = MenuMouseStepList(mc_save_wrk.slot, &box, SaveScrMouseStep(), 0.0f, num, nullptr);

            if (to != mc_save_wrk.slot)
            {
                mc_save_wrk.slot = (s_char)to;
                SndSePlay(MGS_SE_CSR, 100, 0x1000, 0, nullptr, 0);
                return;
            }
        }
    }

    if (CheckPadRptPlaySe(MGS_PAD_LEFT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.slot = (s_char)((mc_save_wrk.slot + num - 1) % num);
    }
    else if (CheckPadRptPlaySe(MGS_PAD_RIGHT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.slot = (s_char)((mc_save_wrk.slot + 1) % num);
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) != 0)
    {
        mc_save_wrk.yesno = 1;
        mc_save_wrk.msg = MGS_MSG_OVERWRITE;
        if (GetMemoryCardPlayDataFlg(mc_save_wrk.slot) == 0)
            mc_save_wrk.msg = MGS_MSG_SAVE_ASK;
        mc_save_wrk.step = MGS_STEP_ASK;
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0)
    {
        mc_save_wrk.mode = MC_SAVE_MODE_END;
    }
}

// FF3 0x001d68a8.
static void GameDataSaveMcSaveConf(void)
{
    GameDataSaveMcSaveConfPad();
    GameDataSaveMcEveryFrameCheck();
}

// Left / right toggle yes / no; OK on yes saves (the slot fades out
// first), OK on no and cancel go back to the slots.
// FF3 0x001d68c8.
static void GameDataSaveMcSaveConfPad(void)
{
    if (SaveScrMsgIsOpen() == 0)
        return;
    if (CheckPadRptPlaySe(MGS_PAD_LEFT, MGS_SE_CSR, 0) != 0
        || CheckPadRptPlaySe(MGS_PAD_RIGHT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.yesno ^= 1;
        return;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) != 0)
    {
        if (mc_save_wrk.yesno == 0)
        {
            mc_save_wrk.save_cnt = 0;
            mc_save_wrk.msg = MGS_MSG_SAVING;
            mc_save_wrk.step = MGS_STEP_FADE_INIT;
        }
        else
        {
            mc_save_wrk.msg = MGS_MSG_SELECT;
            mc_save_wrk.step = MGS_STEP_SELECT;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0)
    {
        mc_save_wrk.msg = MGS_MSG_SELECT;
        mc_save_wrk.step = MGS_STEP_SELECT;
    }
}

// FF3 0x001d69c0.
static void GameDataSaveMcFadeInit(void)
{
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.step = MGS_STEP_FADE_WAIT;
}

// The save screen fades the chosen slot out (SaveScrFadeUpdate); then its
// textures go and the write starts.
// FF3 0x001d69f0.
static void GameDataSaveMcFadeWait(void)
{
    if (SaveScrIsSlotFadedOut() != 0)
    {
        GameDataSaveMcSnapFree(mc_save_wrk.slot);
        mc_save_wrk.step = MGS_STEP_SYS_LOAD_INIT;
    }
    GameDataSaveMcEveryFrameCheck();
}

// Reads card file 0 (the system file).
// FF3 0x001d6a30.
static void GameDataSaveMcSystemLoadInit(void)
{
    char path_name[64];
    int size;

    memset(path_name, 0, 55);
    LiberateDataMemoryArea(&game_data_save_buff);
    MemoryCardSetFilePath(path_name, 0, MGS_FILE_SYSTEM);
    size = GetMemoryCardDataSize(0, MGS_FILE_SYSTEM);
    game_data_save_buff = GetDataMemoryArea(size);
    MemoryCardFileLoadInit(0, 0, path_name, game_data_save_buff, size);
    mc_save_wrk.msg = MGS_MSG_SAVING;
    mc_save_wrk.step = MGS_STEP_SYS_LOAD_WAIT;
}

// The card's system file (unless fresh) is developed between
// TitleMenuItemOpenBackup and TitleMenuItemOpenMerge, so what the card
// had opened is kept along with the game's; then the writes start.
// FF3 0x001d6ac8.
static void GameDataSaveMcSystemLoadWait(void)
{
    int res = MemoryCardFileLoadMain();

    if (res == 1)
    {
        int size = GetMemoryCardDataSize(0, MGS_FILE_SYSTEM);

        if (MemoryCardCheckFileBroken(game_data_save_buff, size) == 0)
        {
            mc_save_wrk.msg = MGS_MSG_BROKEN;
            mc_save_wrk.step = MGS_STEP_ERROR_INIT;
        }
        else
        {
            if (MemoryCardCheckNewFileLoad(game_data_save_buff, size) == 0)
            {
                TitleMenuItemOpenBackup();
                DevelopMemoryCardLoadData(game_data_save_buff, 0, MGS_FILE_SYSTEM);
                TitleMenuItemOpenMerge();
            }
            mc_save_wrk.msg = MGS_MSG_SAVING;
            mc_save_wrk.step = MGS_STEP_SAVE_INIT;
            mc_save_wrk.save_cnt = 0;
        }
    }
    else if (res < 0)
    {
        switch (res)
        {
        case -3:                            // short read
            mc_save_wrk.msg = MGS_MSG_BROKEN;
            break;
        case -2:
            mc_save_wrk.msg = MGS_MSG_UNFORMATTED;
            break;
        default:                            // -10 .. -4 and the rest
            mc_save_wrk.msg = MGS_MSG_SAVE_ERROR;
            break;
        }
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&game_data_save_buff);
}

// Writes the next of the three files (system, the slot's game file, the
// heads with this slot's head filled in). The events' screen effects are
// put into their save form around the copy into the buffer.
// FF3 0x001d6c08.
static void GameDataSaveMcSaveInit(void)
{
    char path_name[64];
    int label;
    int size;

    memset(path_name, 0, 55);
    save_file_label[1] = mc_save_wrk.slot + MGS_FILE_GAME;
    LiberateDataMemoryArea(&game_data_save_buff);
    SetMemoryCardPlayDataHead(mc_save_wrk.slot);
    label = save_file_label[mc_save_wrk.save_cnt];
    MemoryCardSetFilePath(path_name, 0, label);
    size = GetMemoryCardDataSize(0, label);
    game_data_save_buff = GetDataMemoryArea(size);
    EventEnd();
    SetMemoryCardSaveDataToBuff(game_data_save_buff, 0, label);
    EventAfterSave();
    MemoryCardFileSaveInit(0, 0, path_name, game_data_save_buff, size);
    mc_save_wrk.msg = MGS_MSG_SAVING;
    mc_save_wrk.save_cnt++;
    mc_save_wrk.step = MGS_STEP_SAVE_WAIT;
}

// After the third file "saved" (the slot's textures load again first); a
// failed write marks the slot -1 and clears its head.
// FF3 0x001d6d08.
static void GameDataSaveMcSaveWait(void)
{
    int res = MemoryCardFileSaveMain();

    if (res == 1)
    {
        if (mc_save_wrk.save_cnt < MGS_SAVE_FILES)
        {
            mc_save_wrk.msg = MGS_MSG_SAVING;
            mc_save_wrk.step = MGS_STEP_SAVE_INIT;
        }
        else
        {
            mc_save_wrk.msg = MGS_MSG_SAVED;
            mc_save_wrk.step = MGS_STEP_RELOAD_INIT;
            mc_save_wrk.slot_flg[mc_save_wrk.slot] = 1;
        }
    }
    else if (res < 0)
    {
        mc_save_wrk.slot_flg[mc_save_wrk.slot] = -1;
        MemoryCardPlayDataHeadClear(mc_save_wrk.slot);
        mc_save_wrk.msg = MGS_MSG_SAVE_ERROR;
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&game_data_save_buff);
}

// Forgets the slots' file numbers (the slot screen hides them), arms the
// card watch and, unless the message is about the card itself (0, 1),
// keeps a missing card from replacing it for 3 s.
// FF3 0x001d6de0.
static void GameDataSaveMcErrorConfInit(void)
{
    for (int i = 0; i < MC_SAVE_SLOTS; i++)
    {
        mc_save_wrk.chap_file[i] = -1;
        mc_save_wrk.snap_file[i] = -1;
    }
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.step = MGS_STEP_ERROR_WAIT;
    mc_save_wrk.save_cnt = 0;
    mc_save_wrk.slot = 0;
    if (mc_save_wrk.msg < 2)
    {
        mc_save_wrk.msg_timer = 0;
        return;
    }
    mc_save_wrk.msg_timer = GetPALMode() != 0 ? MGS_MSG_WAIT_PAL : MGS_MSG_WAIT_NTSC;
}

// OK / cancel end it; a new card (-1) or an unformatted one (-2) starts
// over; a missing card (-0x14) replaces the message once the timer ran
// out. The watch is re-armed after each answer.
// FF3 0x001d6e88.
static void GameDataSaveMcErrorConfWait(void)
{
    int res;

    if (GameDataSaveMcErrorConfPad() != 0)
        return;
    if (mc_save_wrk.msg_timer > 0)
        mc_save_wrk.msg_timer--;
    res = MemoryCardCheckEveryFrameMain();
    if (res == -1 || res == -2)
    {
        mc_save_wrk.msg = MGS_MSG_NONE;
        mc_save_wrk.step = MGS_STEP_CHECK_INIT;
        return;
    }
    if (res != 1)
    {
        if (res == -0x14)
        {
            if (mc_save_wrk.msg_timer != 0)
                return;
            mc_save_wrk.msg = MGS_MSG_NO_CARD;
        }
        else if (res >= 0)
        {
            return;
        }
    }
    MemoryCardCheckEveryFrameInit(0, 0);
}

// 1 (and mode 3) when OK or cancel closes the message.
// FF3 0x001d6f48.
static int GameDataSaveMcErrorConfPad(void)
{
    if (SaveScrMsgIsOpen() == 0)
        return 0;
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) == 0
        && CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) == 0)
        return 0;
    mc_save_wrk.mode = MC_SAVE_MODE_END;
    return 1;
}

// The written slot's textures again, so the screen shows the new save.
// FF3 0x001d6fd0.
static void GameDataSaveMcReloadInit(void)
{
    GameDataSaveMcSnapLoadSlot(mc_save_wrk.slot);
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.step = MGS_STEP_RELOAD_WAIT;
}

// FF3 0x001d7010.
static void GameDataSaveMcReloadWait(void)
{
    int slot = mc_save_wrk.slot;

    if (FileLoadIsEnd2(mc_save_wrk.snap_file[slot], mc_save_wrk.tex[slot]) != 0
        && FileLoadIsEnd2(mc_save_wrk.chap_file[slot], mc_save_wrk.chap_tex[slot]) != 0)
        mc_save_wrk.step = MGS_STEP_END_CONF;
    GameDataSaveMcEveryFrameCheck();
}

// "saved" until OK / cancel; then the game goes on (title_start_flg.save 1).
// FF3 0x001d70a0.
static void GameDataSaveMcEndConf(void)
{
    int end;

    if (SaveScrMsgIsOpen() == 0)
        return;
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    end = 1;
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) == 0)
        end = CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0;
    if (end)
    {
        title_start_flg.save = 1;
        mc_save_wrk.mode = MC_SAVE_MODE_END;
    }
}

// FF3 0x001d7120.
static void GameDataSaveMcRemakeConfInit(void)
{
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.msg = MGS_MSG_REMAKE_ASK;
    mc_save_wrk.step = MGS_STEP_REMAKE_CONF_WAIT;
    mc_save_wrk.yesno = 1;
}

// FF3 0x001d7160.
static void GameDataSaveMcRemakeConfWait(void)
{
    GameDataSaveMcRemakeConfPad();
    GameDataSaveMcEveryFrameCheck();
}

// Yes deletes the damaged directory; no and cancel end without saving.
// FF3 0x001d7180.
static void GameDataSaveMcRemakeConfPad(void)
{
    if (SaveScrMsgIsOpen() == 0)
        return;
    if (CheckPadRptPlaySe(MGS_PAD_LEFT, MGS_SE_CSR, 0) != 0
        || CheckPadRptPlaySe(MGS_PAD_RIGHT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.yesno ^= 1;
        return;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) != 0)
    {
        if (mc_save_wrk.yesno == 0)
        {
            mc_save_wrk.msg = MGS_MSG_MAKING;
            mc_save_wrk.step = MGS_STEP_DIR_DEL_INIT;
        }
        else
        {
            mc_save_wrk.mode = MC_SAVE_MODE_END;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0)
    {
        mc_save_wrk.mode = MC_SAVE_MODE_END;
    }
}

// FF3 0x001d7260.
static void GameDataSaveMcRemakeDirDelInit(void)
{
    MemoryCardDirDelInit(0, 0, 0);
    mc_save_wrk.msg = MGS_MSG_MAKING;
    mc_save_wrk.step = MGS_STEP_DIR_DEL_WAIT;
}

// Deleted: the directory is made anew.
// FF3 0x001d72a0.
static void GameDataSaveMcRemakeDirDelWait(void)
{
    int res = MemoryCardDirDelMain();

    if (res == 1)
    {
        mc_save_wrk.msg = MGS_MSG_MAKING;
        mc_save_wrk.step = MGS_STEP_NEW_MAKE_INIT;
    }
    else if (res < 0)
    {
        mc_save_wrk.msg = MGS_MSG_MAKE_FAIL;
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
}

// FF3 0x001d7300.
static void GameDataSaveMcNewMakeConfInit(void)
{
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.msg = MGS_MSG_NEW_ASK;
    mc_save_wrk.step = MGS_STEP_NEW_CONF_WAIT;
    mc_save_wrk.yesno = 1;
}

// FF3 0x001d7340.
static void GameDataSaveMcNewMakeConfWait(void)
{
    GameDataSaveMcNewMakeConfPad();
    GameDataSaveMcEveryFrameCheck();
}

// Yes makes the directory; no and cancel end without saving.
// FF3 0x001d7360.
static void GameDataSaveMcNewMakeConfPad(void)
{
    if (SaveScrMsgIsOpen() == 0)
        return;
    if (CheckPadRptPlaySe(MGS_PAD_LEFT, MGS_SE_CSR, 0) != 0
        || CheckPadRptPlaySe(MGS_PAD_RIGHT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.yesno ^= 1;
        return;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) != 0)
    {
        if (mc_save_wrk.yesno == 0)
        {
            mc_save_wrk.msg = MGS_MSG_MAKING;
            mc_save_wrk.step = MGS_STEP_NEW_MAKE_INIT;
        }
        else
        {
            mc_save_wrk.mode = MC_SAVE_MODE_END;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0)
    {
        mc_save_wrk.mode = MC_SAVE_MODE_END;
    }
}

// Makes the game directory (icon and files) from a buffer of its size.
// FF3 0x001d7440.
static void GameDataSaveMcNewMakeInit(void)
{
    int size = GetMemoryCardDirSize(0);

    LiberateDataMemoryArea(&game_data_save_buff);
    game_data_save_buff = GetDataMemoryArea(size);
    MemoryCardNewMakeInit(0, 0, 0, game_data_save_buff, size);
    mc_save_wrk.step = MGS_STEP_NEW_MAKE_WAIT;
}

// FF3 0x001d74a0.
static void GameDataSaveMcNewMakeWait(void)
{
    int res = MemoryCardNewMakeMain();

    if (res == 1)
    {
        MemoryCardPlayDataHeadInit();
        mc_save_wrk.msg = MGS_MSG_SAVING;
        mc_save_wrk.step = MGS_STEP_NEW_SAVE_INIT;
        mc_save_wrk.save_cnt = 0;
    }
    else if (res < 0)
    {
        mc_save_wrk.msg = MGS_MSG_MAKE_FAIL;
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&game_data_save_buff);
}

// GameDataSaveMcSaveInit for slot 0 of a new directory.
// FF3 0x001d7528.
static void GameDataSaveMcNewMakeSaveInit(void)
{
    char path_name[64];
    int label;
    int size;

    memset(path_name, 0, 55);
    SetMemoryCardPlayDataHead(0);
    LiberateDataMemoryArea(&game_data_save_buff);
    label = new_save_file_label[mc_save_wrk.save_cnt];
    MemoryCardSetFilePath(path_name, 0, label);
    size = GetMemoryCardDataSize(0, label);
    game_data_save_buff = GetDataMemoryArea(size);
    EventEnd();
    SetMemoryCardSaveDataToBuff(game_data_save_buff, 0, label);
    EventAfterSave();
    MemoryCardFileSaveInit(0, 0, path_name, game_data_save_buff, size);
    mc_save_wrk.step = MGS_STEP_NEW_SAVE_WAIT;
    mc_save_wrk.save_cnt++;
}

// FF3 0x001d7610.
static void GameDataSaveMcNewMakeSaveWait(void)
{
    int res = MemoryCardFileSaveMain();

    if (res == 1)
    {
        if (mc_save_wrk.save_cnt < MGS_SAVE_FILES)
        {
            mc_save_wrk.msg = MGS_MSG_SAVING;
            mc_save_wrk.step = MGS_STEP_NEW_SAVE_INIT;
        }
        else
        {
            mc_save_wrk.msg = MGS_MSG_SAVED_NEW;
            mc_save_wrk.step = MGS_STEP_RELOAD_INIT;
            mc_save_wrk.slot_flg[0] = 1;
        }
    }
    else if (res < 0)
    {
        mc_save_wrk.slot_flg[0] = -1;
        MemoryCardPlayDataHeadClear(0);
        mc_save_wrk.msg = MGS_MSG_SAVE_ERROR;
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&game_data_save_buff);
}

// FF3 0x001d76d8.
static void GameDataSaveMcFormatConfInit(void)
{
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_save_wrk.msg = MGS_MSG_FORMAT_ASK;
    mc_save_wrk.step = MGS_STEP_FORMAT_CONF_WAIT;
    mc_save_wrk.yesno = 1;
}

// The card watch as in GameDataSaveMcEveryFrameCheck, but an unformatted
// card (-2) is what this question is about and changes nothing.
// FF3 0x001d7718.
static void GameDataSaveMcFormatConfWait(void)
{
    int res;

    GameDataSaveMcFormatConfPad();
    res = MemoryCardCheckEveryFrameMain();
    if (res >= 0)
        return;
    switch (res)
    {
    case -0x14:
    case -1:
        mc_save_wrk.msg = MGS_MSG_NO_CARD;
        break;
    case -2:
        return;
    default:
        mc_save_wrk.msg = MGS_MSG_CARD_ERROR;
        break;
    }
    mc_save_wrk.step = MGS_STEP_ERROR_INIT;
}

// Yes formats; no and cancel end without saving.
// FF3 0x001d7790.
static void GameDataSaveMcFormatConfPad(void)
{
    if (SaveScrMsgIsOpen() == 0)
        return;
    if (CheckPadRptPlaySe(MGS_PAD_LEFT, MGS_SE_CSR, 0) != 0
        || CheckPadRptPlaySe(MGS_PAD_RIGHT, MGS_SE_CSR, 0) != 0)
    {
        mc_save_wrk.yesno ^= 1;
        return;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MGS_SE_OK, 0) != 0)
    {
        if (mc_save_wrk.yesno == 0)
        {
            mc_save_wrk.msg = MGS_MSG_FORMATTING;
            mc_save_wrk.step = MGS_STEP_FORMAT_INIT;
        }
        else
        {
            mc_save_wrk.mode = MC_SAVE_MODE_END;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MGS_SE_CANCEL, 0) != 0)
    {
        mc_save_wrk.mode = MC_SAVE_MODE_END;
    }
}

// FF3 0x001d7870.
static void GameDataSaveMcFormatInit(void)
{
    MemoryCardFormatInit(0, 0);
    mc_save_wrk.msg = MGS_MSG_FORMATTING;
    mc_save_wrk.step = MGS_STEP_FORMAT_WAIT;
}

// FF3 0x001d78a8.
static void GameDataSaveMcFormatWait(void)
{
    int res = MemoryCardFormatMain();

    if (res == 1)
    {
        mc_save_wrk.msg = MGS_MSG_FORMATTED;
        mc_save_wrk.step = MGS_STEP_FORMAT_END_INIT;
    }
    else if (res < 0)
    {
        mc_save_wrk.msg = MGS_MSG_FORMAT_FAIL;
        mc_save_wrk.step = MGS_STEP_ERROR_INIT;
    }
}

// FF3 0x001d7908.
static void GameDataSaveMcFormatEndInit(void)
{
    mc_save_wrk.msg = MGS_MSG_FORMATTED;
    mc_save_wrk.fmt_end_timer = 0;
    mc_save_wrk.step = MGS_STEP_FORMAT_END_WAIT;
}

// "formatted" for 60 frames, then the directory is made.
// FF3 0x001d7928.
static void GameDataSaveMcFormatEndWait(void)
{
    mc_save_wrk.fmt_end_timer++;
    if (mc_save_wrk.fmt_end_timer > MGS_FORMAT_END_TIME)
    {
        mc_save_wrk.msg = MGS_MSG_MAKING;
        mc_save_wrk.step = MGS_STEP_NEW_MAKE_INIT;
    }
}

// The card is polled while the screen waits for the textures or the
// player: losing it shows a message.
// FF3 0x001d7960.
static void GameDataSaveMcEveryFrameCheck(void)
{
    int res = MemoryCardCheckEveryFrameMain();

    if (res >= 0)
        return;
    switch (res)
    {
    case -0x14:
    case -1:
        mc_save_wrk.msg = MGS_MSG_NO_CARD;
        break;
    case -2:
        mc_save_wrk.msg = MGS_MSG_UNFORMATTED;
        break;
    default:
        mc_save_wrk.msg = MGS_MSG_CARD_ERROR;
        break;
    }
    mc_save_wrk.step = MGS_STEP_ERROR_INIT;
}
