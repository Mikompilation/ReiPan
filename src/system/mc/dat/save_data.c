/* ==========================================================================
 *  system/mc/dat/save_data.c
 *
 *  The save manifests (FF2 system/mc/dat/save_data.c; data only). Each list
 *  ends with a null entry; the comment after an entry is its index and the
 *  block size it reports. Prototypes and block addresses: save_data.h.
 * ======================================================================== */
#include "system/mc/dat/save_data.h"

#include "ingame/candle_ctrl.h"                 // CandleControlSaveGet
#include "ingame/cine_film.h"                   // CineSaveGetProjector, CineSaveGetFlag
#include "ingame/enemy/random_ghost.h"          // RandGhostSaveGet
#include "ingame/event/ev_queue.h"              // EvQueueSaveGet
#include "ingame/event/telephone.h"             // TelGetSaveData
#include "ingame/event/prg/ev_change.h"         // EvWrkSaveGet, EvChangeCtrlSaveGet, EvDreamPlayerSaveGet
#include "ingame/event/prg/ev_chapter.h"        // EvGhostTblSaveGet
#include "ingame/event/prg/ev_command.h"        // EvSave*SaveGet
#include "ingame/sleep_movie.h"                 // FinderBankSaveGet
#include "ingame/event/prg/ev_exe.h"            // EvExeCtrlSaveGet
#include "ingame/event/prg/ev_open.h"           // EvCtrlCenterSaveGet, EvPriCtrlSaveGet
#include "ingame/event/prg/ev_se.h"             // ev_seSetSave
#include "ingame/event/prg/ev_timer.h"          // EvTimerCtrlSaveGet
#include "ingame/map/MarkEvent.h"               // MarkEventFlagSaveGet
#include "ingame/map/DoorStat.h"                // DoorStatSaveGet
#include "ingame/map/FurnSave.h"                // FurnSaveSaveGet
#include "ingame/map/map_bgm.h"                 // map_bgmSetSave
#include "ingame/ingame_wrk.h"                    // IngameWrkSaveGet, TitleMenuItemOpenSaveGet
#include "ingame/item/prg/soul_list.h"          // SoulListSaveGet, SoulListBonusSaveGet
#include "ingame/memo/memo.h"                   // MemoWrkSaveGet, MemoFlgSaveGet
#include "ingame/menu/menu_cassette.h"          // CassetteHeardSaveGet
#include "ingame/menu/start_menu.h"             // FinderStateSaveGet / FinderSlotSaveGet
#include "ingame/mission/mission_list.h"        // MissionSaveGet
#include "ingame/photo/finder_photo.h"          // PhotoModeSaveGet*
#include "ingame/photo/photo_obj.h"             // PhotoObjSlotSaveGet
#include "ingame/photo/photo_obj_dat.h"         // PhotoObjFlgSaveGet
#include "ingame/plyr/player.h"                 // PlyrEquipLvSaveGet, PlyrWrkSaveGet
#include "ingame/story_door.h"                 // MapAreaGetSaveData
#include "ingame/story_develop.h"              // DevelopFilmSaveGet
#include "ingame/story_night.h"                // NightStartPosGetSaveData
#include "ingame/story_menu_photo.h"           // DocFlagSaveGet
#include "ingame/puzzle/puzzle.h"              // PuzzleGetSaveWrk
#include "ingame/radio.h"                       // RadioSaveGet
#include "outgame/option.h"                     // OptionWrkSaveGet
#include "graphics/effect/effect_data.h"       // ScreenEffectParamSaveGet
#include "graphics/scene/IngameScene.h"        // SleepFlgSaveGet

MC_SET_SAVE_FUNC save_system_data[2] =     /* sdata 3720b8 */
{
    TitleMenuItemOpenSaveGet,       //  0  0x10
    (MC_SET_SAVE_FUNC)0
};

MC_SET_SAVE_FUNC save_play_data_head[2] =  /* sdata 3720c0 */
{
    SetSave_PlayDataHead,           //  0  0xb4
    (MC_SET_SAVE_FUNC)0
};

MC_SET_SAVE_FUNC save_game_data[61] =      /* data 332ac0 */
{
    ItemSaveGet,                    //  0  0x19e
    MemoWrkSaveGet,                 //  1  0x42
    MemoFlgSaveGet,                 //  2  0x8
    DocFlagSaveGet,                   //  3  0xa4
    DevelopFilmSaveGet,             //  4  0x50
    EvChangeCtrlSaveGet,                   //  5  0x168
    EvGhostTblSaveGet,              //  6  0x13b0
    EvExeCtrlSaveGet,               //  7  0xe10 (host: the table's size)
    EvCtrlCenterSaveGet,            //  8  0xe10
    ev_seSetSave,                   //  9  0x960
    EvTimerCtrlSaveGet,             // 10  0x1e4
    EvWrkSaveGet,                   // 11  0x1be0
    EvSaveStreamSaveGet,            // 12  0x60
    map_bgmSetSave,                 // 13  0x1fc
    EvSaveScreenEffectSaveGet,      // 14  0x8
    EvSaveEffDitherSaveGet,         // 15  0x6
    EvDreamPlayerSaveGet,                   // 16  0x8
    OptionWrkSaveGet,               // 17  0x14
    CandleControlSaveGet,           // 18  0x10
    FinderStateSaveGet,             // 19  0x10
    FinderSlotSaveGet,              // 20  0x84
    SoulListSaveGet,                // 21  0x1b2
    SoulListBonusSaveGet,           // 22  0x10
    FinderBankSaveGet,              // 23  0x10
    PuzzleGetSaveWrk,               // 24  0x2c (the puzzle clear flags)
    MissionSaveGet,                 // 25  0x1c0
    SleepFlgSaveGet,                // 26  0x40
    IngameWrkSaveGet,               // 27  0x4c (host: the struct's size)
    PlyrWrkSaveGet,                 // 28  0x9f0 (host: the table's size)
    PhotoObjFlgSaveGet,             // 29  0x18
    PhotoObjSlotSaveGet,            // 30  0x140 (host: the table's size)
    EneIdFlgSaveGet0,                   // 31  0x30c
    EneIdFlgSaveGet1,                   // 32  0xe0
    EneIdFlgSaveGet2,                   // 33  0x1ce
    MapAreaGetSaveData,             // 34  0x8
    PhotoModeSaveGetLens,           // 35  0x70
    PhotoModeSaveGetMode,           // 36  0x1
    PhotoModeSaveGetFilm,           // 37  0x1
    PhotoModeSaveGetAlbumSaveDisp,  // 38  0x8
    CineSaveGetProjector,           // 39  0xc
    CineSaveGetFlag,                // 40  0xb
    CassetteHeardSaveGet,           // 41  0x1c
    TelGetSaveData,                 // 42  0xc
    RandGhostSaveGet,               // 43  0x1e0
    NightStartPosGetSaveData,       // 44  0x14
    AlbumIngameSaveGet,             // 45  0x8
    ScreenEffectParamSaveGet,       // 46  0xc
    MapAreaDataGetSaveData,         // 47  0xa00
    FurnSaveSaveGet,                // 48  0x3000
    DoorStatSaveGet,                // 49  0x404
    EvPriCtrlSaveGet,               // 50  0xe14
    MapLBuffSaveGet,                // 51  0x2600
    MapLightRegSaveGet,             // 52  0x300
    MapSnapSaveGet,                 // 53  0x800
    PlyrCameraSaveGet,              // 54  0x2b0
    CatGetSaveData,                   // 55  0xc0
    PlyrEquipLvSaveGet,             // 56  0x7
    EvQueueSaveGet,                 // 57  0xc8
    MarkEventFlagSaveGet,             // 58  0x10
    RadioSaveGet,                   // 59  0xc
    (MC_SET_SAVE_FUNC)0
};

MC_SET_SAVE_FUNC save_album_info[2] =      /* sdata 3720c8 */
{
    AlbumSaveGetInfo,               //  0  0x10
    (MC_SET_SAVE_FUNC)0
};

MC_SET_SAVE_FUNC save_album_data[3] =      /* data 332bb8 */
{
    AlbumSaveGetData,               //  0  0x20c
    AlbumSaveGetPhoto,              //  1  0xe8000
    (MC_SET_SAVE_FUNC)0
};
