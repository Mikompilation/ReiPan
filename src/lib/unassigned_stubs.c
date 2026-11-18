// lib/unassigned_stubs.c -- stubs and data of FF3 code whose file is not settled yet.
// Move each piece to its real file when that TU is defined (tus.py define).
#include <stddef.h>                 // offsetof
#include <string.h>                 // memcpy
#include "ingame/ingame_wrk.h"         // PLAY_TIME
#include "ingame/map/MapSky.h"
#include "outgame/option.h"
#include "album/prg/album_view.h"
#include "ingame/event/prg/ev_command.h"
#include "ingame/camera/finder_camera.h"
#include "ingame/plyr/plyr_shot.h"      // SUBJECT_WRK (AddPhotoData)
#include "sdk/libcdvd.h"                 // sceCdCLOCK (AddPhotoData)
#include "ingame/camera/map_camera.h"    // APPROACH_CAMERA (the damage camera shots)
#include "graphics/graph2d/message.h"
#include "ingame/sound/snd_ctrl.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM
#include "system/b2d/BpCtl.h"           // B2D_RECT
#include "graphics/graph2d/g2d_draw.h"
#include "graphics/motion/anicode.h"   // MSG_PIC_KEY_DAT
#include "system/eeiop/snd.h"           // VOLSET
#include "system/eeiop/snd_stream.h"
#include "graphics/graph3d/g3dCore.h"
#include "ingame/photo/finder_photo.h"     // FILM_WRK, LENS_WRK, HUD_HP_WRK
#include "ingame/map/MapDoor.h"
#include "ingame/map/RegDat.h"           // MB_OUT_SECTION (RegSecList*)
#include "ingame/plyr/acs_model.h"
#include "ingame/photo/photo.h"
#include "ingame/plyr/plyr_sub.h"      // LADDER_DAT, PLYR_MDL_FX
#include "ingame/ingame.h"              // GameModel, CharPlayerModel
#include "graphics/game_model.h"
#include "graphics/graph3d/sgd_types.h" // SGDCOORDINATE
#include "ingame/map/MapObjWrk.h"       // MAP_OBJ_MDL
#include "ingame/plyr/plyr_mdl.h"       // PLYR_LOOK_PRM / PLYR_LOOK_WRK
#include "ingame/map/hit_check_base.h"
#include "ingame/map/MapPut.h"
#include "ingame/map/LightCtl.h"
#include "system/b2d/B2dBase.h"
#include "ingame/menu/menuCamBasic.h"
#include "ingame/menu/menuCamStat.h"
#include "ingame/menu/anim_2d.h"         // HUD_LAYOUT
#include "ingame/ingame_effect.h"
#include "graphics/effect/effect.h"     // EFFECT_CONT
#include "graphics/effect/effect_obj.h"
#include "ingame/map/map_rectangle.h"
#include "ingame/event/prg/ev_macro.h"
#include "ingame/event/prg/ev_open.h"
#include "graphics/motion/motion.h"
#include "graphics/graph3d/g3dLightEx.h"
#include "ingame/map/foot_se.h"
#include "graphics/effect/effect_oth.h"
#include "ingame/enemy/fly_ctrl.h"

#include "system/mc/dat/save_data.h"
#include "ingame/subtitle/subtitle.h"
#include "system/playpss/playpss.h"
#include "system/playpss/ldimage.h"
#include "ingame/memo/memo.h"

#include "common/reipan_stub.h"
#include "common/heapctrl.h"
#include "system/eeiop/cd_file_dat.h"   // VCI_CAMERA_DATA_PK4
#include "system/os/eecdvd.h"           // LoadReqGetAddr, IsLoadEnd
#include "system/mc/prg/mc.h"

#include "system/pad/pad.h"

struct MAP_CAM_DATA;                // ingame/camera/map_camera.h
class GameModel;                    // graphics/game_model.h
struct GRA3DCAMERA;                 // graphics/graph3d/gra3d.h
struct HUD_LAYOUT;                  // ingame/menu/anim_2d.h

// The language pack pictures (data 0x351470; FUN_00180360, EffKind1EDrawParts).
SPRT_DAT lang_effdat[36] = {
    { 0x20059405dd40abc0ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 0
    { 0x20059425dd40abc0ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 1
    { 0x20059445dd40abe0ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 2
    { 0x20059465dd40abe0ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 3
    { 0x2005948621412c00ULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 4
    { 0x200594a621412c00ULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 5
    { 0x200594c59d40ac80ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 6
    { 0x200594e59d40ac80ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 7
    { 0x200595059d40ac90ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 8
    { 0x200595259d40ac90ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 9
    { 0x2005a545e1312caaULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 10
    { 0x2005a5c5e1312caaULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 11
    { 0x2005db85dd40ad32ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 12
    { 0x2005dba5dd40ad52ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 13
    { 0x2005dbc5dd40ad72ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 14
    { 0x2005dbe5dd40ad72ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 15
    { 0x2005dc05dd40ad92ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 16
    { 0x2005dc25dd40ad92ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 17
    { 0x2005dc45e1412db2ULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 18
    { 0x2005dc65e1412df2ULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 19
    { 0x2005dc85dd40ae32ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 20
    { 0x2005dca59940ae52ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 21
    { 0x2005dcc59940ae5aULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 22
    { 0x2005dce59d40ae62ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 23
    { 0x2005dd059d40ae72ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 24
    { 0x2005dd259940ae82ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 25
    { 0x2005dd455540ae8aULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 26
    { 0x2005dd65e1412e8cULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 27
    { 0x2005dd859d40aeccULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 28
    { 0x20067fa5dd30aeedULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 29
    { 0x2006802621312f2dULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 30
    { 0x200680a62532302dULL, 0, 0, 512, 256, 0, 0, 0, 128, 0, 0 },  // 31
    { 0x200681262131322dULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 32
    { 0x200681a5dd30b32dULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 33
    { 0x200682259930b36dULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 34
    { 0x200682a5e131337dULL, 0, 0, 256, 128, 0, 0, 0, 128, 0, 0 },  // 35
};

// The effect textures in colour / monochrome pairs (data 0x351b00..0x352680;
// FF2 g2d_main.c effdat[98]). Values from the ELF; owner not settled.
SPRT_DAT effdat[92] = {
    { 0x2007c6255540bd40ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 0
    { 0x2007c6455540bd40ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 1
    { 0x2007c6655540bd42ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 2
    { 0x2007c6855540bd42ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 3
    { 0x2007c6a59940bd44ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 4
    { 0x2007c6c59940bd44ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 5
    { 0x2007c6e59940bd4cULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 6
    { 0x2007c7059940bd4cULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 7
    { 0x2007c7259940bd54ULL, 0, 0, 62, 62, 0, 0, 0, 128, 0, 0 },  // 8
    { 0x2007c7459940bd54ULL, 0, 0, 62, 62, 0, 0, 0, 128, 0, 0 },  // 9
    { 0x2007c7655540bd5cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 10
    { 0x2007c7855540bd5cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 11
    { 0x2007c7a51140bd5eULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 12
    { 0x2007c7c51140bd5eULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 13
    { 0x2007c7e55540bd5fULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 14
    { 0x2007c8055540bd5fULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 15
    { 0x2007c8255540bd61ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 16
    { 0x2007c8455540bd61ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 17
    { 0x2007c8659940bd63ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 18
    { 0x2007c8859940bd63ULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 19
    { 0x2007c8a51140bd6bULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 20
    { 0x2007c8c51140bd6bULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 21
    { 0x2007c8e5dd40bd6cULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 22
    { 0x2007c905dd40bd6cULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 23
    { 0x2007c9255540bd8cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 24
    { 0x2007c9455540bd8cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 25
    { 0x2007c9655540bd8eULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 26
    { 0x2007c9855540bd8eULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 27
    { 0x2007c9a55540bd90ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 28
    { 0x2007c9c55540bd90ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 29
    { 0x2007c9e55540bd92ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 30
    { 0x2007ca055540bd92ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 31
    { 0x2007ca255540bd94ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 32
    { 0x2007ca455540bd94ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 33
    { 0x2007ca655540bd96ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 34
    { 0x2007ca855540bd96ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 35
    { 0x2007caa55540bd98ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 36
    { 0x2007cac55540bd98ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 37
    { 0x2007cae55540bd9aULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 38
    { 0x2007cb055540bd9aULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 39
    { 0x2007cb255540bd9cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 40
    { 0x2007cb455540bd9cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 41
    { 0x2007cb655540bd9eULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 42
    { 0x2007cb855540bd9eULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 43
    { 0x2007cba55540bda0ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 44
    { 0x2007cbc55540bda0ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 45
    { 0x2007cbe55540bda2ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 46
    { 0x2007cc055540bda2ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 47
    { 0x2007cc255540bda4ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 48
    { 0x2007cc455540bda4ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 49
    { 0x2007cc655540bda6ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 50
    { 0x2007cc855540bda6ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 51
    { 0x2007cca55540bda8ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 52
    { 0x2007ccc55540bda8ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 53
    { 0x2007cce55540bdaaULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 54
    { 0x2007cd055540bdaaULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 55
    { 0x2007cd255540bdacULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 56
    { 0x2007cd455540bdacULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 57
    { 0x2007cd651140bdaeULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 58
    { 0x2007cd851140bdaeULL, 0, 0, 16, 16, 0, 0, 0, 128, 0, 0 },  // 59
    { 0x2007cda55540bdafULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 60
    { 0x2007cdc55540bdafULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 61
    { 0x2007cde621413db1ULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 62
    { 0x2007ce0621413db1ULL, 0, 0, 256, 256, 0, 0, 0, 128, 0, 0 },  // 63
    { 0x2007e8859d30be71ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 64
    { 0x2007e9059d30be71ULL, 0, 0, 128, 64, 0, 0, 0, 128, 0, 0 },  // 65
    { 0x2007e9859130be91ULL, 0, 0, 16, 48, 0, 0, 0, 128, 0, 0 },  // 66
    { 0x2007ea059130be91ULL, 0, 0, 16, 48, 0, 0, 0, 128, 0, 0 },  // 67
    { 0x2007ea859930be9cULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 68
    { 0x2007eb059930be9cULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 69
    { 0x2007eb859930beacULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 70
    { 0x2007ec059930beacULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 71
    { 0x2007ec859930bebcULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 72
    { 0x2007ed059930bebcULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 73
    { 0x2007ed859930beccULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 74
    { 0x2007ee059930beccULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 75
    { 0x2007ee859930bedcULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 76
    { 0x2007ef059930bedcULL, 0, 0, 64, 64, 0, 0, 0, 128, 0, 0 },  // 77
    { 0x2007ef855530beecULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 78
    { 0x2007f0055530beecULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 79
    { 0x2007f0855530bef0ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 80
    { 0x2007f1055530bef0ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 81
    { 0x2007f185dd30bef4ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 82
    { 0x2007f205dd30bef4ULL, 0, 0, 128, 128, 0, 0, 0, 128, 0, 0 },  // 83
    { 0x2007f2855530bf34ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 84
    { 0x2007f3055530bf34ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 85
    { 0x2007f3855530bf38ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 86
    { 0x2007f4055530bf38ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 87
    { 0x2007f4855530bf3cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 88
    { 0x2007f5055530bf3cULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 89
    { 0x2007f5855530bf40ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 90
    { 0x2007f6055530bf40ULL, 0, 0, 32, 32, 0, 0, 0, 128, 0, 0 },  // 91
};

// ---- stubs (tools/reverse/stubgen.py) ----















































