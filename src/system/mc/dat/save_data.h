/* ==========================================================================
 *  system/mc/dat/save_data.h
 *
 *  The save manifests (FF2 system/mc/dat/save_data.c): per card file, the
 *  list of callbacks that each name one block of game state {addr, size}.
 *  SetMemoryCardSaveDataInfo (mc_set_data.c) runs them; a file is its blocks
 *  back to back plus a 4-byte checksum.
 *
 *  FF3 has 60 blocks per save slot against FF2's 49, and its callbacks
 *  return the size (FF2's filled the whole MC_SAVE_DATA). Most were never
 *  split out by Ghidra (no direct callers) and were created 2026-10-14; the
 *  unnamed ones keep FUN_ names until their owner TU is reconstructed.
 *
 *  The callback prototypes below belong in their owners' headers: move each
 *  one there when that TU is reconstructed (the owner's file is the TU named
 *  in the group comment; stubgen put their stubs there). Each comment gives
 *  the EE block {address, size} the callback reports.
 * ======================================================================== */
#ifndef _SYSTEM_MC_DAT_SAVE_DATA_H
#define _SYSTEM_MC_DAT_SAVE_DATA_H

#include "album/prg/album_disp.h"               // AlbumIngameSaveGet, AlbumSaveGet*
#include "ingame/cat/cat_ctrl.h"                // CatGetSaveData
#include "ingame/enemy/enemy.h"                  // EneIdFlgSaveGet0..2
#include "ingame/photo/n_plyr_camera.h"          // PlyrCameraSaveGet
#include "system/mc/prg/mc_set_data.h"          // MC_SET_SAVE_FUNC

extern MC_SET_SAVE_FUNC save_system_data[2];    // sdata 0x3720b8: "Zero3System"
extern MC_SET_SAVE_FUNC save_play_data_head[2]; // sdata 0x3720c0: "BESLES-53825ZERO-BD"
extern MC_SET_SAVE_FUNC save_game_data[61];     // data 0x332ac0: "Zero3Play0".."Zero3Play4"
extern MC_SET_SAVE_FUNC save_album_info[2];     // sdata 0x3720c8: an album's "BESLES-53825ZERO-BAn"
extern MC_SET_SAVE_FUNC save_album_data[3];     // data 0x332bb8: an album's "Zero3Album<n-1>"

//   MemoWrkSaveGet 0x00203bd0 {0x32b560, 0x42}, MemoFlgSaveGet 0x00203be8 {0x371158, 0x8}: ingame/memo/memo.h

// ingame/mission/mission_list.c
//   MissionSaveGet 0x001ab3e0 {0x3e7600, 0x1c0}: ingame/mission/mission_list.h

// ingame/story_menu_photo.c
//   DocFlagSaveGet 0x001b0780 {0x325198, 0xa4}: ingame/story_menu_photo.h

// ingame/event/prg/ev_command.c (prototypes in ev_command.h)
//   EvSaveStreamSaveGet 0x001a33b8 {0x3e69c0, 0x60}, EvSaveScreenEffectSaveGet
//   0x0019fb18 {0x3730e8, 0x8}, EvSaveEffDitherSaveGet 0x0019fb28 {0x3730e0, 0x6}

// ingame/event/telephone.c (prototype in telephone.h)
//   TelGetSaveData 0x0025c1f0 {0x3fd0e8, 0xc}

// ingame/event/prg/ev_change.c (prototypes in ev_change.h)
//   EvDreamPlayerSaveGet 0x00199b08 {0x373030, 0x8}, EvWrkSaveGet 0x00199b18
//   {0x3e0098, 0x1be0}, EvChangeCtrlSaveGet 0x00199c80 {0x3e1c78, 0x168}

// ingame/event/prg/ev_macro.c

// ingame/event/prg/ev_timer.c
//   EvTimerCtrlSaveGet 0x0019ca60 {0x3e65e0, 0x1e4}: ingame/event/prg/ev_timer.h

// ingame/event/prg/ev_open.c (prototypes in ev_open.h)
//   EvCtrlCenterSaveGet 0x0019baf8 {0x3e4008, 0xe10}, EvPriCtrlSaveGet
//   0x0019bb10 {0x3e4e18, 0xe14}

// ingame/ingame_wrk.c (prototypes in ingame_wrk.h)
//   IngameWrkSaveGet 0x001cfd40 {0x327560, 0x4c}, TitleMenuItemOpenSaveGet
//   0x001cfeb0 {0x3eb368, 0x10}

// ingame/item/prg/item.c
int ItemSaveGet(void **addr);            // FF3 0x001ea8f0: {0x328fe0, 0x19e}

// ingame/map/MapHit.c
int MapLBuffSaveGet(void **addr);         // FF3 0x00119350: {0x37fd40, 0x2600}

// ingame/menu/menu_cassette.c
//   CassetteHeardSaveGet 0x0025b738 {0x3fd060, 0x1c}: ingame/menu/menu_cassette.h

// ingame/photo/photo_obj.c / photo_obj_dat.c
//   PhotoObjSlotSaveGet 0x00228ab8 {0x3f66e0, 0x140}, PhotoObjFlgSaveGet 0x0022a9d0 {0x3f7770, 0x18}

// ingame/menu/start_menu.c (prototypes in start_menu.h)
//   FinderStateSaveGet 0x001a5da0 {GetFinderStateWrk(), 0x10}, FinderSlotSaveGet
//   0x001a6288 {GetFinderSlotWrk(), 0x84}

// ingame/photo/n_plyr_camera.c (prototype in n_plyr_camera.h)
//   PlyrCameraSaveGet 0x0021e660 {0x3f62b0, 0x2b0}

// ingame/plyr/player.c
//   PlyrWrkSaveGet 0x002367f0 {0x32f4a0, 0x9f0}, PlyrEquipLvSaveGet 0x00237858
//   {0x373820, 0x7}: ingame/plyr/player.h

// ingame/puzzle/kaza/kaza_pzl.c
int MapLightRegSaveGet(void **addr);     // FF3 0x001f5548: {0x3f13b0, 0x300} (ingame/map/MapEff.c)
int MapSnapSaveGet(void **addr);         // FF3 0x001f6b70: {the room rows (sdata 0x370fb0), 0x800} (ingame/map/MapSnap.c)

// ingame/story_menu.c

#endif /* _SYSTEM_MC_DAT_SAVE_DATA_H */
