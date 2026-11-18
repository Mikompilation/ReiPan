#include <cstdio>
#include <cstring>
#include "system/mc/prg/mc_check_dir.h"
#include "reipan/save/reipan_save_codec.h"
#include "reipan/reipan_memory.h"
#include "reipan/io/reipan_log.h"
#include "ingame/event/prg/ev_main.h"
#include "system/eeiop/cddat.h"
#include "system/eeiop/cd_file_dat.h"
static int reipan_slot_disk_size;
/* ==========================================================================
 *  save_load/prg/game_data_load.c
 *
 *  The memory-card load of phase 68 Story_LoadGame_Mc (FF3-only, FF3
 *  0x1d4678..0x1d5b40). ingame/story_load_mc.c calls McLoadGameInit, then
 *  McLoadGameMain every frame until it answers 1, then McLoadGameEnd; the load
 *  screen (outgame/save_load_scr.c) draws mc_load_wrk meanwhile and opens its
 *  yes/no window with mc_load_wrk.msg.
 *
 *  The steps (MLG_STEP_*, one per frame; each "init" step falls into its
 *  "wait" step in the same frame):
 *    0/1   look for the game directory on port 0;
 *    2/3   load card file 1, the five play data heads (which slots hold a
 *          save, their chapter, place, time ..);
 *    4/5   load each save's map snapshot and chapter title texture for the
 *          slot screen;
 *    6/7   the player picks a save and confirms;
 *    8/9   load card file 0, the system file, then 10/11 the game file
 *          (slot + 2) and develop both into the game;
 *    12/13 an error message until OK / cancel (a new card restarts at 0);
 *    14    "loaded", until OK / cancel or 150 frames.
 *  OK / cancel in 6, 13 and 14 end it (mode 3); title_start_flg.load says
 *  whether a game was loaded.
 *
 *  Written from the FF3 code: FF2 had no such file (its game_data_save.c,
 *  whose step functions these mirror one for one, only saved).
 * ======================================================================== */
#include "save_load/prg/game_data_load.h"

#include <string.h>

#include "ingame/event/prg/ev_main.h"           // EventAfterLoad
#include "ingame/ingame_wrk.h"                  // title_start_flg
#include "ingame/map/MapSnap.h"                 // MapSnapGetNo
#include "ingame/sound/snd_ctrl.h"              // SndSePlay
#include "outgame/save_load_scr.h"              // SaveLoadScrMsgIsOpen
#include "system/eeiop/cd_file_dat.h"           // RS101_R01_TM2, CLEARDATA_TM2, SAVELOAD_CHAPTER*_TM2
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/eeiop/fileload.h"              // FileLoadReqEE, FileLoadIsEnd2
#include "system/mc/prg/mc.h"                   // MemoryCardExeInit, MemoryCardEnd
#include "system/mc/prg/mc_check.h"             // MemoryCardCheckInit / Main
#include "system/mc/prg/mc_check_broken.h"      // dir / file broken, new file
#include "system/mc/prg/mc_check_card.h"        // MemoryCardSetAccessPort, the card watch
#include "system/mc/prg/mc_load.h"              // MemoryCardFileLoadInit / Main
#include "system/mc/prg/mc_set_data.h"          // paths, sizes, buffers, play data heads
#include "system/os/system.h"                   // GetPALMode
#include "system/pad/pad_ctrl.h"                // CheckPadTrgPlaySe / RptPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// The ROM's message ids (kind 0x83), by where they are set.
#define MLG_MSG_NONE        0x00
#define MLG_MSG_NO_CARD     0x01            // not a PS2 card / the card went away (-0x14, -1)
#define MLG_MSG_CARD_ERROR  0x02            // any other card error
#define MLG_MSG_ASK         0x0e            // load this file? (yes / no)
#define MLG_MSG_LOAD_ERROR  0x0f            // a file load failed
#define MLG_MSG_LOADED      0x10            // the game was loaded
#define MLG_MSG_NO_DATA     0x18            // unformatted / no game directory (-2, -4)
#define MLG_MSG_DIR_BROKEN  0x19            // the game directory / head file is damaged
#define MLG_MSG_LOADING     0x1d            // the system file loads
#define MLG_MSG_LOADING2    0x20            // the game file loads
#define MLG_MSG_NO_SAVE     0x21            // no save in the head file (or a fresh one)
#define MLG_MSG_SELECT      0x22            // pick a file
#define MLG_MSG_SYS_BROKEN  0x33            // the system file is damaged
#define MLG_MSG_GAME_BROKEN 0x34            // the game file is damaged

#define MLG_END_TIME        0x95            // "loaded" ends after 150 frames
#define MLG_MSG_WAIT_PAL    75              // error messages: 3 s before a card change shows
#define MLG_MSG_WAIT_NTSC   90

#define MLG_SE_CSR          0x1a9b0000      // SE_BANK_DEFAULT: cursor
#define MLG_SE_CANCEL       0x1a9b0001
#define MLG_SE_BUZZER       0x1a9b0002      // OK on an empty slot
#define MLG_SE_OK           0x1a9b0003

#define MLG_PAD_LEFT        0x8000
#define MLG_PAD_RIGHT       0x2000

#define MLG_FILE_HEAD       1               // card file labels (dir 0): the play data heads
#define MLG_FILE_SYSTEM     0               // the system file; a game is slot + 2

// ══════════════════════════════════════════════════════════════════════
// Data
// ══════════════════════════════════════════════════════════════════════

MC_LOAD_WRK mc_load_wrk;                        // data 0x3282e0 (read by save_load_scr.c)
static void *mc_load_data_buff;                 // sdata 0x36d5e8 (gp-0x2a18): the card file being read

static void McLoadGameCtrlInit(void);
static void McLoadGameMcMain(void);
static void McLoadGameMcCheckInit(void);
static void McLoadGameMcCheckWait(void);
static void McLoadGameMcHeadLoadInit(void);
static void McLoadGameMcHeadLoadWait(void);
static void McLoadGameMcSnapLoadInit(void);
static void McLoadGameMcSnapLoadWait(void);
static void McLoadGameMcFileSel(void);
static void McLoadGameMcFileSelPad(void);
static void McLoadGameMcConf(void);
static void McLoadGameMcConfPad(void);
static void McLoadGameMcSystemLoadInit(void);
static void McLoadGameMcSystemLoadWait(void);
static void McLoadGameMcLoadInit(void);
static void McLoadGameMcLoadWait(void);
static void McLoadGameMcErrorConfInit(void);
static void McLoadGameMcErrorConfWait(void);
static int  McLoadGameMcErrorConfPad(void);
static void McLoadGameMcEndConf(void);
static void McLoadGameMcCardWatch(void);

static void McLoadGameSePlay(int se)
{
    SndSePlay((u_int)se, 100, 0x1000, 0, nullptr, 0);
}

// ══════════════════════════════════════════════════════════════════════
// Phase 68 entry points
// ══════════════════════════════════════════════════════════════════════

// Takes the card for port 0 and forgets every slot's textures (the
// pointers are only cleared).
// FF3 0x001d4678.
void McLoadGameInit(void)
{
    McLoadGameCtrlInit();
    MemoryCardExeInit();
    MemoryCardSetAccessPort(0);
    mc_load_data_buff = nullptr;
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        mc_load_wrk.tex[i] = nullptr;
        mc_load_wrk.snap_file[i] = -1;
        mc_load_wrk.chap_tex[i] = nullptr;
        mc_load_wrk.chap_file[i] = -1;
    }
}

// FF3 0x001d4700.
static void McLoadGameCtrlInit(void)
{
    memset(&mc_load_wrk, 0, offsetof(MC_LOAD_WRK, tex));
    mc_load_wrk.msg = MLG_MSG_NONE;
    mc_load_wrk.yesno = 1;
    mc_load_wrk.mode = 0;
    mc_load_wrk.step = MLG_STEP_CHECK_INIT;
    mc_load_wrk.slot = 0;
    mc_load_wrk.end_timer = 0;
    mc_load_wrk.msg_timer = 0;
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        mc_load_wrk.slot_flg[i] = 0;
}

// Two frames of start-up (the first clears title_start_flg.load), then the
// steps until mode 3: 1.
// FF3 0x001d4788.
int McLoadGameMain(void)
{
    switch (mc_load_wrk.mode)
    {
    case 0:
        mc_load_wrk.mode = 1;
        title_start_flg.load = 0;
        break;
    case 1:
        mc_load_wrk.mode = MC_LOAD_MODE_SLOTS;
        break;
    case MC_LOAD_MODE_SLOTS:
        McLoadGameMcMain();
        return 0;
    case MC_LOAD_MODE_END:
        return 1;
    }
    return 0;
}

// FF3 0x001d5ac0.
void McLoadGameEnd(void)
{
    LiberateDataMemoryArea(&mc_load_data_buff);
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        LiberateDataMemoryArea(&mc_load_wrk.tex[i]);
        LiberateDataMemoryArea(&mc_load_wrk.chap_tex[i]);
    }
    MemoryCardEnd();
}

// ══════════════════════════════════════════════════════════════════════
// The steps
// ══════════════════════════════════════════════════════════════════════

// FF3 0x001d4808 (jump table 0x359f50): the init steps call their wait step
// right after (a jal then a tail j in the same case).
static void McLoadGameMcMain(void)
{
    switch (mc_load_wrk.step)
    {
    case MLG_STEP_CHECK_INIT:
        McLoadGameMcCheckInit();
        // fall through
    case MLG_STEP_CHECK_WAIT:
        McLoadGameMcCheckWait();
        return;
    case MLG_STEP_HEAD_LOAD_INIT:
        McLoadGameMcHeadLoadInit();
        // fall through
    case MLG_STEP_HEAD_LOAD_WAIT:
        McLoadGameMcHeadLoadWait();
        return;
    case MLG_STEP_SNAP_LOAD_INIT:
        McLoadGameMcSnapLoadInit();
        // fall through
    case MLG_STEP_SNAP_LOAD_WAIT:
        McLoadGameMcSnapLoadWait();
        return;
    case MLG_STEP_SELECT:
        McLoadGameMcFileSel();
        return;
    case MLG_STEP_ASK:
        McLoadGameMcConf();
        return;
    case MLG_STEP_SYS_LOAD_INIT:
        McLoadGameMcSystemLoadInit();
        // fall through
    case MLG_STEP_SYS_LOAD_WAIT:
        McLoadGameMcSystemLoadWait();
        return;
    case MLG_STEP_GAME_LOAD_INIT:
        McLoadGameMcLoadInit();
        // fall through
    case MLG_STEP_GAME_LOAD_WAIT:
        McLoadGameMcLoadWait();
        return;
    case MLG_STEP_ERROR_INIT:
        McLoadGameMcErrorConfInit();
        // fall through
    case MLG_STEP_ERROR_WAIT:
        McLoadGameMcErrorConfWait();
        return;
    case MLG_STEP_END_CONF:
        McLoadGameMcEndConf();
        return;
    }
}

// Lists the game directory ("<dir>/*") on port 0.
// FF3 0x001d48e8.
static void McLoadGameMcCheckInit(void)
{
    char path_name[64];

    memset(path_name, 0, 55);
    MemoryCardMakeSearchDirPath(path_name, 0);
    MemoryCardCheckInit(0, 0, path_name);
    mc_load_wrk.step = MLG_STEP_CHECK_WAIT;
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        mc_load_wrk.slot_flg[i] = 0;
}

// A whole directory goes on to the head file; anything else is a message.
// FF3 0x001d4960.
static void McLoadGameMcCheckWait(void)
{
    int res = MemoryCardCheckMain();

    if (res == 1)
    {
        if (MemoryCardCheckDirBroken(0) != 0)
        {
            mc_load_wrk.msg = MLG_MSG_NONE;
            mc_load_wrk.step = MLG_STEP_HEAD_LOAD_INIT;
            return;
        }
        mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
    }
    else
    {
        if (res >= 0)
            return;
        switch (res)
        {
        case -0x14:                         // not a PS2 card
            mc_load_wrk.msg = MLG_MSG_NO_CARD;
            break;
        case -6:                            // the directory is not empty but unreadable
            mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
            break;
        case -4:                            // no game directory
        case -2:                            // unformatted
            mc_load_wrk.msg = MLG_MSG_NO_DATA;
            break;
        case -1:                            // the card was swapped: look again
            mc_load_wrk.msg = MLG_MSG_NONE;
            mc_load_wrk.step = MLG_STEP_CHECK_INIT;
            return;
        default:
            mc_load_wrk.msg = MLG_MSG_CARD_ERROR;
            break;
        }
    }
    mc_load_wrk.step = MLG_STEP_ERROR_INIT;
}

// Reads card file 1 (the play data heads).
// FF3 0x001d4a30.
static void McLoadGameMcHeadLoadInit(void)
{
    char path_name[64];
    int size;

    memset(path_name, 0, 55);
    LiberateDataMemoryArea(&mc_load_data_buff);
    MemoryCardSetFilePath(path_name, 0, MLG_FILE_HEAD);
    size = GetMemoryCardDataSize(0, MLG_FILE_HEAD);
    mc_load_data_buff = GetDataMemoryArea(size);
    MemoryCardFileLoadInit(0, 0, path_name, mc_load_data_buff, size);
    mc_load_wrk.msg = MLG_MSG_NONE;
    mc_load_wrk.step = MLG_STEP_HEAD_LOAD_WAIT;
}

// A whole head file is developed (GetMemoryCardPlayData* answer from it);
// the slots holding a save get flag 1 (a slot marked -1 keeps it), and with
// at least one save the textures load. The buffer is freed once the load
// ends, either way.
// FF3 0x001d4ac0.
static void McLoadGameMcHeadLoadWait(void)
{
    int res = MemoryCardFileLoadMain();

    if (res == 1)
    {
        int size = GetMemoryCardDataSize(0, MLG_FILE_HEAD);

        if (MemoryCardCheckFileBroken(mc_load_data_buff, size) == 0)
        {
            mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
        }
        else if (MemoryCardCheckNewFileLoad(mc_load_data_buff, size) != 0)
        {
            mc_load_wrk.msg = MLG_MSG_NO_SAVE;
        }
        else
        {
            DevelopMemoryCardLoadData(mc_load_data_buff, 0, MLG_FILE_HEAD);
            for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
            {
                if (GetMemoryCardPlayDataFlg(i) == 1)
                {
                    if (mc_load_wrk.slot_flg[i] != -1)
                        mc_load_wrk.slot_flg[i] = 1;
                }
                else
                {
                    mc_load_wrk.slot_flg[i] = 0;
                }
            }
            mc_load_wrk.msg = MLG_MSG_NO_SAVE;
            mc_load_wrk.step = MLG_STEP_ERROR_INIT;
            for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
            {
                if (GetMemoryCardPlayDataFlg(i) == 1)
                {
                    mc_load_wrk.msg = MLG_MSG_NONE;
                    mc_load_wrk.step = MLG_STEP_SNAP_LOAD_INIT;
                }
            }
            LiberateDataMemoryArea(&mc_load_data_buff);
            return;
        }
        mc_load_wrk.step = MLG_STEP_ERROR_INIT;
    }
    else if (res < 0)
    {
        switch (res)
        {
        case -3:                            // short read
            mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
            break;
        case -2:
            mc_load_wrk.msg = MLG_MSG_NO_DATA;
            break;
        default:                            // -10 .. -4 and the rest
            mc_load_wrk.msg = MLG_MSG_LOAD_ERROR;
            break;
        }
        mc_load_wrk.step = MLG_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&mc_load_data_buff);
}

// Per saved slot two textures from the CD: the map snapshot of the place it
// was saved in (finder-mode saves: the clear-data picture) and the chapter
// title (finder-mode saves: chapter 15's). A place without a snapshot skips
// the slot after freeing its buffers (its file numbers stay). Empty slots
// drop theirs. Then the card watch is armed.
// FF3 0x001d4cb0.
static void McLoadGameMcSnapLoadInit(void)
{
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        if (GetMemoryCardPlayDataFlg(i) == 1)
        {
            int file;
            u_int size;

            LiberateDataMemoryArea(&mc_load_wrk.tex[i]);
            LiberateDataMemoryArea(&mc_load_wrk.chap_tex[i]);

            if (GetMemoryCardPlayDataFinderFlg(i) == 0)
            {
                int no = MapSnapGetNo(GetMemoryCardPlayDataArea(i),
                                      GetMemoryCardPlayDataChapter(i));
                if (no < 0)
                    continue;
                file = no + RS101_R01_TM2;
            }
            else
            {
                file = CLEARDATA_TM2;
            }
            mc_load_wrk.snap_file[i] = file;
            size = GetFileSize(mc_load_wrk.snap_file[i]);
            if (size != (u_int)-1 && GetFileSize(mc_load_wrk.snap_file[i]) != 0)
            {
                void *addr = GetDataMemoryArea((int)GetFileSize(mc_load_wrk.snap_file[i]));
                mc_load_wrk.tex[i] = addr;
                FileLoadReqEE(mc_load_wrk.snap_file[i], addr, 3, nullptr, nullptr);
            }

            if (GetMemoryCardPlayDataFinderFlg(i) == 0)
            {
                int chapter = GetMemoryCardPlayDataB02(i);

                file = chapter + TM2SAVELOAD_LABEL_START;
                if (chapter < 1)
                    file = SAVELOAD_CHAPTER01_TM2;
                mc_load_wrk.chap_file[i] = file;
            }
            else
            {
                mc_load_wrk.chap_file[i] = SAVELOAD_CHAPTER15_TM2;
            }
            size = GetFileSize(mc_load_wrk.chap_file[i]);
            if (size != (u_int)-1 && GetFileSize(mc_load_wrk.chap_file[i]) != 0)
            {
                void *addr = GetDataMemoryArea((int)GetFileSize(mc_load_wrk.chap_file[i]));
                mc_load_wrk.chap_tex[i] = addr;
                FileLoadReqEE(mc_load_wrk.chap_file[i], addr, 3, nullptr, nullptr);
            }
        }
        else
        {
            LiberateDataMemoryArea(&mc_load_wrk.tex[i]);
            mc_load_wrk.snap_file[i] = -1;
            LiberateDataMemoryArea(&mc_load_wrk.chap_tex[i]);
            mc_load_wrk.chap_file[i] = -1;
        }
    }
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_load_wrk.msg = MLG_MSG_NONE;
    mc_load_wrk.step = MLG_STEP_SNAP_LOAD_WAIT;
}

// Once both textures of every saved slot are in, the slot screen opens
// with the cursor on the first save.
// FF3 0x001d4f28.
static void McLoadGameMcSnapLoadWait(void)
{
    int first = 0;

    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        if (GetMemoryCardPlayDataFlg(i) != 1)
            continue;
        if (FileLoadIsEnd2(mc_load_wrk.snap_file[i], mc_load_wrk.tex[i]) == 0
            || FileLoadIsEnd2(mc_load_wrk.chap_file[i], mc_load_wrk.chap_tex[i]) == 0)
        {
            mc_load_wrk.msg = MLG_MSG_NONE;
            mc_load_wrk.step = MLG_STEP_SNAP_LOAD_WAIT;
            break;
        }
        mc_load_wrk.msg = MLG_MSG_SELECT;
        mc_load_wrk.step = MLG_STEP_SELECT;
        if (!first)
        {
            mc_load_wrk.slot = (s_char)i;
            first = 1;
        }
    }
    McLoadGameMcCardWatch();
}

// FF3 0x001d5048.
static void McLoadGameMcFileSel(void)
{
    McLoadGameMcFileSelPad();
    McLoadGameMcCardWatch();
}

// Left / right move the cursor to the previous / next slot holding a save
// (wrapping; the cursor SE only when it moved), OK on a save asks, OK on
// anything else buzzes, cancel leaves without loading.
// FF3 0x001d5068.
// callcheck: SndSePlay (McLoadGameSePlay)
// PC: the mouse: the slots that hold a save.
static int McLoadGameMouseCan(int slot)
{
    return GetMemoryCardPlayDataFlg(slot) == 1;
}

static void McLoadGameMcFileSelPad(void)
{
    int old;

    if (SaveLoadScrMsgIsOpen() == 0)
        return;
    old = mc_load_wrk.slot;
    {
        ReiPan_MenuBox box;

        ReiPan_MenuMouseBegin(REIPAN_MM_WHEEL_HORIZONTAL, nullptr);
        if (SaveLoadScrMouseBox(&box.x, &box.y, &box.w, &box.h))
        {
            mc_load_wrk.slot = (s_char)MenuMouseStepList(old, &box, SaveLoadScrMouseStep(), 0.0f,
                                                         SAVE_LOAD_SLOTS, McLoadGameMouseCan);
            if (old != mc_load_wrk.slot)
            {
                McLoadGameSePlay(MLG_SE_CSR);
                return;
            }
        }
    }
    if (CheckPadRptPlaySe(MLG_PAD_LEFT, -1, 0) != 0)
    {
        for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        {
            mc_load_wrk.slot = (s_char)((mc_load_wrk.slot + 4) % SAVE_LOAD_SLOTS);
            if (GetMemoryCardPlayDataFlg(mc_load_wrk.slot) == 1)
                break;
        }
    }
    else if (CheckPadRptPlaySe(MLG_PAD_RIGHT, -1, 0) != 0)
    {
        for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        {
            mc_load_wrk.slot = (s_char)((mc_load_wrk.slot + 1) % SAVE_LOAD_SLOTS);
            if (GetMemoryCardPlayDataFlg(mc_load_wrk.slot) == 1)
                break;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0) != 0)
    {
        if (mc_load_wrk.slot_flg[mc_load_wrk.slot] == 1)
        {
            McLoadGameSePlay(MLG_SE_OK);
            mc_load_wrk.msg = MLG_MSG_ASK;
            mc_load_wrk.step = MLG_STEP_ASK;
            mc_load_wrk.yesno = 1;
            return;
        }
        McLoadGameSePlay(MLG_SE_BUZZER);
        return;
    }
    else
    {
        if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MLG_SE_CANCEL, 0) != 0)
            mc_load_wrk.mode = MC_LOAD_MODE_END;
        return;
    }
    if (old != mc_load_wrk.slot)
        McLoadGameSePlay(MLG_SE_CSR);
}

// FF3 0x001d5258.
static void McLoadGameMcConf(void)
{
    McLoadGameMcConfPad();
    McLoadGameMcCardWatch();
}

// Left / right toggle yes / no; OK on yes loads, OK on no and cancel go
// back to the slots.
// FF3 0x001d5278.
static void McLoadGameMcConfPad(void)
{
    if (SaveLoadScrMsgIsOpen() == 0)
        return;
    if (CheckPadRptPlaySe(MLG_PAD_LEFT, MLG_SE_CSR, 0) != 0
        || CheckPadRptPlaySe(MLG_PAD_RIGHT, MLG_SE_CSR, 0) != 0)
    {
        mc_load_wrk.yesno ^= 1;
        return;
    }
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MLG_SE_OK, 0) != 0)
    {
        if (mc_load_wrk.yesno == 0)
        {
            mc_load_wrk.msg = MLG_MSG_LOADING;
            mc_load_wrk.step = MLG_STEP_SYS_LOAD_INIT;
        }
        else
        {
            mc_load_wrk.msg = MLG_MSG_SELECT;
            mc_load_wrk.step = MLG_STEP_SELECT;
        }
    }
    else if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MLG_SE_CANCEL, 0) != 0)
    {
        mc_load_wrk.msg = MLG_MSG_SELECT;
        mc_load_wrk.step = MLG_STEP_SELECT;
    }
}

// Reads card file 0 (the system file).
// FF3 0x001d5368.
static void McLoadGameMcSystemLoadInit(void)
{
    char path_name[64];
    int size;

    memset(path_name, 0, 55);
    LiberateDataMemoryArea(&mc_load_data_buff);
    MemoryCardSetFilePath(path_name, 0, MLG_FILE_SYSTEM);
    size = GetMemoryCardDataSize(0, MLG_FILE_SYSTEM);
    mc_load_data_buff = GetDataMemoryArea(size);
    MemoryCardFileLoadInit(0, 0, path_name, mc_load_data_buff, size);
    mc_load_wrk.msg = MLG_MSG_LOADING;
    mc_load_wrk.step = MLG_STEP_SYS_LOAD_WAIT;
}

// FF3 0x001d5400.
static void McLoadGameMcSystemLoadWait(void)
{
    int res = MemoryCardFileLoadMain();

    if (res == 1)
    {
        int size = GetMemoryCardDataSize(0, MLG_FILE_SYSTEM);

        if (MemoryCardCheckFileBroken(mc_load_data_buff, size) == 0)
        {
            mc_load_wrk.step = MLG_STEP_ERROR_INIT;
            mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
        }
        else if (MemoryCardCheckNewFileLoad(mc_load_data_buff, size) == 0)
        {
            DevelopMemoryCardLoadData(mc_load_data_buff, 0, MLG_FILE_SYSTEM);
            mc_load_wrk.step = MLG_STEP_GAME_LOAD_INIT;
            mc_load_wrk.msg = MLG_MSG_LOADING2;
        }
        else
        {
            mc_load_wrk.step = MLG_STEP_ERROR_INIT;
            mc_load_wrk.msg = MLG_MSG_SYS_BROKEN;
        }
    }
    else if (res < 0)
    {
        switch (res)
        {
        case -3:                            // short read
            mc_load_wrk.msg = MLG_MSG_DIR_BROKEN;
            break;
        case -2:
            mc_load_wrk.msg = MLG_MSG_NO_DATA;
            break;
        default:                            // -10 .. -4 and the rest
            mc_load_wrk.msg = MLG_MSG_LOAD_ERROR;
            break;
        }
        mc_load_wrk.step = MLG_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&mc_load_data_buff);
}

// Reads the chosen game file (card file slot + 2).
// FF3 0x001d5538.
static void McLoadGameMcLoadInit(void)
{
    char path_name[64];
    int file = mc_load_wrk.slot + 2;
    int size;

    memset(path_name, 0, 55);
    LiberateDataMemoryArea(&mc_load_data_buff);
    MemoryCardSetFilePath(path_name, 0, file);
    size = GetMemoryCardDataSize(0, file);
    char entry[16];std::snprintf(entry, sizeof(entry), "Zero3Play%d", mc_load_wrk.slot);
    reipan_slot_disk_size = GetMemoryCardCheckDirEntrySize(entry);
    if (reipan_slot_disk_size != size && reipan_slot_disk_size != (int)ReiPanSave::Ps2Size)
    {
        mc_load_wrk.step = MLG_STEP_ERROR_INIT;
        mc_load_wrk.msg = MLG_MSG_GAME_BROKEN;
        return;
    }
    mc_load_data_buff = GetDataMemoryArea(size);
    MemoryCardFileLoadInit(0, 0, path_name, mc_load_data_buff, reipan_slot_disk_size);
    mc_load_wrk.msg = MLG_MSG_LOADING2;
    mc_load_wrk.step = MLG_STEP_GAME_LOAD_WAIT;
}

// A whole game file is developed into the game, the events are told, and
// "loaded" shows.
// FF3 0x001d55e0.
static void McLoadGameMcLoadWait(void)
{
    int file = mc_load_wrk.slot + 2;
    int res = MemoryCardFileLoadMain();

    if (res == 1)
    {
        int size = GetMemoryCardDataSize(0, file);

        if (reipan_slot_disk_size == (int)ReiPanSave::Ps2Size)
        {
            auto *data = (uint8_t *)mc_load_data_buff;
            ReiPanSave::Bytes disk(data, data + reipan_slot_disk_size), native;
            std::string error;
            if (!ReiPanSave::ToNative(disk, native, error) ||
                !ReiPanSave::PrepareLoaded(native,
                    (const uint8_t *)ReiPan_GetHostPointer(EVENT_DATA_ADDR),
                    GetFileSize(EventGetMode() == 2 ? EVENT_DB_OBJ : EVENT_OBJ), error))
            {
                ReiPan_LogPrintf("PS2 save import rejected: %s\n", error.c_str());
                mc_load_wrk.step = MLG_STEP_ERROR_INIT;
                mc_load_wrk.msg = MLG_MSG_GAME_BROKEN;
                return;
            }
            std::memcpy(data, native.data(), native.size());
        }

        if (MemoryCardCheckFileBroken(mc_load_data_buff, size) == 0
            || MemoryCardCheckNewFileLoad(mc_load_data_buff, size) != 0)
        {
            mc_load_wrk.step = MLG_STEP_ERROR_INIT;
            mc_load_wrk.msg = MLG_MSG_GAME_BROKEN;
        }
        else
        {
            DevelopMemoryCardLoadData(mc_load_data_buff, 0, file);
            EventAfterLoad();
            mc_load_wrk.end_timer = 0;
            mc_load_wrk.msg = MLG_MSG_LOADED;
            mc_load_wrk.step = MLG_STEP_END_CONF;
        }
    }
    else if (res < 0)
    {
        switch (res)
        {
        case -3:                            // short read
            mc_load_wrk.msg = MLG_MSG_GAME_BROKEN;
            break;
        case -2:
            mc_load_wrk.msg = MLG_MSG_NO_DATA;
            break;
        default:
            mc_load_wrk.msg = MLG_MSG_LOAD_ERROR;
            break;
        }
        mc_load_wrk.step = MLG_STEP_ERROR_INIT;
    }
    if (res != 0)
        LiberateDataMemoryArea(&mc_load_data_buff);
}

// Forgets the slots' file numbers (the slot screen hides them), arms the card
// watch and, unless the message is about the card itself (0, 1, 0x18),
// keeps a missing card from replacing it for 3 s.
// FF3 0x001d5730.
static void McLoadGameMcErrorConfInit(void)
{
    for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
    {
        mc_load_wrk.chap_file[i] = -1;
        mc_load_wrk.snap_file[i] = -1;
    }
    mc_load_wrk.slot = 0;
    MemoryCardCheckEveryFrameInit(0, 0);
    mc_load_wrk.step = MLG_STEP_ERROR_WAIT;
    if ((mc_load_wrk.msg >= 0 && mc_load_wrk.msg < 2) || mc_load_wrk.msg == MLG_MSG_NO_DATA)
    {
        mc_load_wrk.msg_timer = 0;
        return;
    }
    mc_load_wrk.msg_timer = GetPALMode() != 0 ? MLG_MSG_WAIT_PAL : MLG_MSG_WAIT_NTSC;
}

// OK / cancel end it; a newly inserted card (-1) starts over; a missing card
// (-0x14) replaces the message once the timer ran out; -2 shows the
// no-data message. The watch is re-armed after each answer.
// FF3 0x001d57f8.
static void McLoadGameMcErrorConfWait(void)
{
    int res;

    if (McLoadGameMcErrorConfPad() != 0)
        return;
    if (mc_load_wrk.msg_timer > 0)
        mc_load_wrk.msg_timer--;
    res = MemoryCardCheckEveryFrameMain();
    if (res == -1)
    {
        for (int i = 0; i < SAVE_LOAD_SLOTS; i++)
        {
            mc_load_wrk.chap_file[i] = -1;
            mc_load_wrk.snap_file[i] = -1;
        }
        mc_load_wrk.msg = MLG_MSG_NONE;
        mc_load_wrk.step = MLG_STEP_CHECK_INIT;
        return;
    }
    if (res != 1)
    {
        if (res == -2)
        {
            mc_load_wrk.msg = MLG_MSG_NO_DATA;
        }
        else if (res == -0x14)
        {
            if (mc_load_wrk.msg_timer != 0)
                return;
            mc_load_wrk.msg = MLG_MSG_NO_CARD;
        }
        else if (res >= 0)
        {
            return;
        }
    }
    MemoryCardCheckEveryFrameInit(0, 0);
}

// 1 (and mode 3) when OK or cancel closes the message.
// FF3 0x001d5908.
static int McLoadGameMcErrorConfPad(void)
{
    if (SaveLoadScrMsgIsOpen() == 0)
        return 0;
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MLG_SE_OK, 0) == 0
        && CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MLG_SE_CANCEL, 0) == 0)
        return 0;
    mc_load_wrk.mode = MC_LOAD_MODE_END;
    return 1;
}

// "loaded" until OK / cancel or 150 frames; then the game starts
// (title_start_flg.load 1).
// FF3 0x001d5990.
static void McLoadGameMcEndConf(void)
{
    int end;

    if (SaveLoadScrMsgIsOpen() == 0)
        return;
    ReiPan_MenuMouseConfirm();          // PC: a click acknowledges
    end = 1;
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], MLG_SE_OK, 0) == 0)
        end = CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_CANCEL], MLG_SE_CANCEL, 0) != 0;
    mc_load_wrk.end_timer++;
    if (mc_load_wrk.end_timer > MLG_END_TIME)
        end = 1;
    if (end)
    {
        mc_load_wrk.mode = MC_LOAD_MODE_END;
        title_start_flg.load = 1;
    }
}

// The card is polled while the screen waits for the textures or the player:
// losing it shows a message.
// FF3 0x001d5a40.
static void McLoadGameMcCardWatch(void)
{
    int res = MemoryCardCheckEveryFrameMain();

    if (res >= 0)
        return;
    switch (res)
    {
    case -0x14:
    case -1:
        mc_load_wrk.msg = MLG_MSG_NO_CARD;
        break;
    case -2:
        mc_load_wrk.msg = MLG_MSG_NO_DATA;
        break;
    default:
        mc_load_wrk.msg = MLG_MSG_CARD_ERROR;
        break;
    }
    mc_load_wrk.step = MLG_STEP_ERROR_INIT;
}
