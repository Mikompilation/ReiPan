/* ==========================================================================
 *  ingame/cat/cat_ctrl.c
 *
 *  FF3-only: the cat (FF3 0x13efa0..0x1441d8). No FF2 counterpart; written
 *  from FF3 alone. See cat_ctrl.h for the act hierarchy. One cat (the loops
 *  stop at CAT_NUM); its models are two CharCatModel (char_model.c) and an
 *  optional AcsModel.
 *
 *  The 21 functions reached only through the act tables (the empty ones,
 *  0x141240..0x141260, 0x1432a0, 0x143328, 0x1435d0, 0x1437d0, the pet
 *  steps 0x143898.., 0x143de0, 0x144030, 0x1440d8, 0x144158, 0x142a08,
 *  0x142f98) had been swallowed by their neighbours' Ghidra bodies and were
 *  created 2026-10-19.
 * ======================================================================== */

#include <math.h>                       // fabs, fabsf, sqrtf
#include <string.h>                     // memset
#include <new>                          // placement new

#include "ingame/cat/cat_ctrl.h"
#include "common/cpp_heap.h"            // operator_new, operator_delete
#include "common/vu0math.h"             // RotLimitChk, RotVectorZXY, GetTrgtRot, NormalizeAngle, GetRandValIByF
#include "graphics/effect/effect.h"     // SetEffects
#include "graphics/graph3d/gra3d.h"     // gra3dcamSet*, gra3dApplyCamera
#include "graphics/mmanage.h"           // ModelManager (PlayShape, IsShapeEnd)
#include "ingame/camera/event_camera.h" // EventCamera*
#include "ingame/char_model.h"          // CharCatModel
#include "ingame/char_model_sub.h"      // CharWatchControl, CHAR_WATCH_PRM, CharDrawSetCat, CharDrawDelete
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapCtrl.h"         // MapAreaHitLineCheck
#include "ingame/map/map_rectangle.h"   // MrecIsInEvent
#include "ingame/npc/npc_act.h"         // PlyrCameraAreaIsDrawn, PlyrCameraSetRandLabel, PlyrCameraEvCam*, EvNowSynchroMode, PlyrCameraIsTalking, PlyrCameraPlyrPlace
#include "ingame/photo/finder_photo.h"  // GetPhotoModeWrk, GetPhotoModeWrk1e8, GetAlbumSaveDisp
#include "ingame/photo/fndr_effect.h"   // PHOTO_WRK_1E8
#include "ingame/photo/n_plyr_camera.h" // PlyrCameraIsFixed, GetPlyrCameraWrk
#include "ingame/plyr/acs_model.h"      // AcsModel
#include "ingame/plyr/player.h"         // PLYR_WRK, GetPlyrAreaNo, PlyrLineHitCheck, SetPlyrAnime, CheckPlyrAnimeEnd
#include "ingame/plyr/plyr_gauge.h"     // PlyrGaugeTimeStep
#include "ingame/plyr/plyr_sub.h"       // AllPlyrIsReady, PlyrGetJointPos, PlyrGetJointRot, PlyrFinderPointMove
#include "ingame/plyr/unit_ctl.h"       // GetStickRaw, UnitGetTrgtRot
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "ingame/story_menu.h"          // IsLockTblOn, IsLockTblCnt, LockTblInc, LockTblDec
#include "main/gphase.h"                // GetGPhaseCtrl
#include "system/eeiop/cd_file_dat.h"   // CH015_*, CH019_*
#include "system/eeiop/snd_3d.h"        // SND_3D_SET
#include "system/os/system.h"           // GetPALMode
#include "system/pad/pad_ctrl.h"        // CheckPadNowPlaySe, CheckPadTrgPlaySe, pad_key

// ──────────────────────────────────────────────────────────────────────
// Tables (rodata 0x34f920..0x34fdd0, sdata 0x36bed0..0x36c058)
// ──────────────────────────────────────────────────────────────────────

#define CAT_ACT_SIZE    0x34            // the act state CatActReset clears (CAT_WRK.no .. _32)

// A position label (rodata 0x34f920, CAT_LABEL_NUM rows of 0x30).
struct CAT_LABEL_DAT
{
    /* 0x00 */ u_short weight[2];       // the free act's pick weight (CatLabelSelect), [EvNowSynchroMode()]
    /* 0x04 */ int     area;            // its area
    /* 0x08 */ u_char  _08[8];
    /* 0x10 */ float   pos[3];
    /* 0x1c */ float   rot;             // facing, degrees
    /* 0x20 */ s_char  motset[2];       // motion sets (CatGetMotSet) by idle kind
    /* 0x22 */ s_char  move;            // move kind (0 stay, 1 loop)
    /* 0x23 */ s_char  sub;             // sub act (0 event spot, 1 door look), -1: none
    /* 0x24 */ s_char  spot;            // the sub act's event spot / door
    /* 0x25 */ u_char  _25[0xb];
};
static_assert(sizeof(CAT_LABEL_DAT) == 0x30, "CAT_LABEL_DAT size");
static_assert(offsetof(CAT_LABEL_DAT, pos) == 0x10, "CAT_LABEL_DAT.pos");
static_assert(offsetof(CAT_LABEL_DAT, motset) == 0x20, "CAT_LABEL_DAT.motset");
static_assert(offsetof(CAT_LABEL_DAT, spot) == 0x24, "CAT_LABEL_DAT.spot");

static const CAT_LABEL_DAT cat_label_tbl[CAT_LABEL_NUM] =    // rodata 0x34f920
{
    { { 35, 25 }, 0x30, {}, { 158.6f,  -7.9f, 244.0f  },  103.0f, { 0,  3 }, 0, -1, -1, {} },
    { { 15,  0 }, 0x30, {}, { 187.5f,   0.0f, 218.1f  },   49.1f, { 1, -1 }, 1,  0,  0, {} },
    { { 15,  0 }, 0x32, {}, {  89.6f, -58.4f, 111.47f },    0.0f, { 0,  3 }, 0, -1, -1, {} },
    { { 10, 50 }, 0x31, {}, {  94.4f, -7.95f,  92.75f }, -137.0f, { 0,  3 }, 0, -1, -1, {} },
    { { 15,  0 }, 0x30, {}, { 121.3f, -50.0f, 178.4f  },  180.0f, { 2, -1 }, 1,  1,  0, {} },
    { { 10, 25 }, 0x31, {}, { 200.2f, -15.1f, 139.9f  },  360.0f, { 3, -1 }, 1, -1, -1, {} },
};

// A label change when the player leaves area 0x30 (rodata 0x34fa40).
struct CAT_LABEL_MOVE
{
    u_char weight;
    u_char _1;
    s_char from;
    s_char to;
};
static_assert(sizeof(CAT_LABEL_MOVE) == 4, "CAT_LABEL_MOVE size");

static const CAT_LABEL_MOVE cat_label_move[6] =     // rodata 0x34fa40
{
    { 15, 0, 0, 1 }, { 15, 0, 0, 4 }, { 15, 0, 1, 0 }, { 15, 0, 1, 4 }, { 15, 0, 4, 0 }, { 15, 0, 4, 1 },
};

// A motion set: start once, then loop for wait[rand 3] seconds, then end
// (rodata 0x34fa58, 6 bytes each).
struct CAT_MOT_SET
{
    s_char start;
    s_char loop;
    s_char end;
    s_char wait[3];                     // seconds
};
static_assert(sizeof(CAT_MOT_SET) == 6, "CAT_MOT_SET size");

static const CAT_MOT_SET cat_mot_set[4] =   // rodata 0x34fa58
{
    { -1,  0, -1, { 0, 1,  3 } },
    { -1, 20, -1, { 0, 1,  3 } },
    { 21, -1, -1, { 0, 1,  3 } },
    { 13, 14, -1, { 1, 5, 10 } },
};

// An idle motion: mot, then loop min..max times, then end (8 bytes).
struct CAT_IDLE_MOT
{
    s_char  mot;
    s_char  loop;
    s_char  end;
    s_char  min;
    s_char  max;
    u_char  _5;
    u_short weight;                     // the pick weight
};
static_assert(sizeof(CAT_IDLE_MOT) == 8, "CAT_IDLE_MOT size");

static const CAT_IDLE_MOT cat_idle_mot0[4] =    // rodata 0x34fa70: idle kind 0
{
    { 1, -1, -1, 1, 3, 0, 25 }, { 2, 3, 4, 1, 3, 0, 25 }, { 5, -1, -1, 1, 3, 0, 25 }, { 6, -1, -1, 1, 3, 0, 25 },
};
static const CAT_IDLE_MOT cat_idle_mot1[2] =    // rodata 0x34fa90: idle kind 1
{
    { 16, 17, 18, 1, 3, 0, 20 }, { 19, -1, -1, 1, 3, 0, 80 },
};
static const CAT_IDLE_MOT cat_react_mot0[2] =   // rodata 0x34faa0: the pet reaction, idle kind 0
{
    { 7, 8, 9, 1, 3, 0, 50 }, { 10, 11, 12, 1, 3, 0, 50 },
};

static const CAT_IDLE_MOT *const cat_idle_mot_tbl[2] = { cat_idle_mot0, cat_idle_mot1 };     // sdata 0x36bf30
// sdata 0x36bf38; kind 1 points at the door table (0x34fab0) with no rows
static const CAT_IDLE_MOT *const cat_react_mot_tbl[2] = { cat_react_mot0, nullptr };
static const s_char cat_idle_mot_num[2]  = { 4, 2 };   // sdata 0x36bf70
static const s_char cat_react_mot_num[2] = { 2, 0 };   // sdata 0x36bf78

// The cat's door (rodata 0x34fab0, 0x2c): the point it watches, the camera
// the door uses when it follows, the label behind the door.
struct CAT_DOOR_DAT
{
    /* 0x00 */ int    door;             // the door's number (DOOR_DAT dbuff)
    /* 0x04 */ float  pos[3];
    /* 0x10 */ float  cam_pos[3];
    /* 0x1c */ float  cam_tgt[3];
    /* 0x28 */ s_char label;
    /* 0x29 */ u_char _29[3];
};
static_assert(sizeof(CAT_DOOR_DAT) == 0x2c, "CAT_DOOR_DAT size");

static const CAT_DOOR_DAT cat_door_tbl[1] =     // rodata 0x34fab0
{
    { 0x12e, { 119.0f, -68.0f, 163.0f }, { 145.33f, -84.47f, 168.94f }, { 97.98f, -48.53f, 149.83f }, 2, {} },
};

// The pet act's event camera by label (rodata 0x34fae0, 0x40).
struct CAT_VIEW_CAM
{
    /* 0x00 */ float  plyr[4];          // the player's place (PlyrCameraPlyrPlace: pos, facing)
    /* 0x10 */ float  pos[3];
    /* 0x1c */ float  tgt[3];
    /* 0x28 */ float  fov;
    /* 0x2c */ float  roll;
    /* 0x30 */ s_char label;
    /* 0x31 */ u_char _31[0xf];
};
static_assert(sizeof(CAT_VIEW_CAM) == 0x40, "CAT_VIEW_CAM size");
static_assert(offsetof(CAT_VIEW_CAM, label) == 0x30, "CAT_VIEW_CAM.label");

static const CAT_VIEW_CAM cat_view_cam[3] =     // rodata 0x34fae0
{
    { { 178.1f,   0.0f, 242.6f, 270.0f }, { 180.0f,  -22.0f, 273.0f  }, { 155.0f,  -6.0f, 209.0f }, 0.77f, 0.0f, 0, {} },
    { {  88.1f, -49.0f, 131.0f, 174.0f }, {  61.0f,  -78.0f, 135.5f  }, { 268.0f,  34.0f,   1.4f }, 0.86f, 0.0f, 2, {} },
    { {  83.3f,   0.0f,  77.5f,  36.0f }, {  65.28f, -29.0f,  85.52f }, { 104.0f,  -7.0f,  88.0f }, 0.74f, 0.0f, 3, {} },
};

// Per motion: the SE it starts and the two shape motions' intervals in
// seconds (rodata 0x34fba0, 0xc each).
struct CAT_MOT_SE
{
    int se;                             // -1: none
    int shape_wait[2];
};
static_assert(sizeof(CAT_MOT_SE) == 0xc, "CAT_MOT_SE size");

static const CAT_MOT_SE cat_mot_se[29] =    // rodata 0x34fba0
{
    { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 },
    { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 },
    { 0x1bf70001, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 },
    { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 1, 1 },
    { -1, 10, 10 }, { 0x1bf70001, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 },
    { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 }, { -1, 10, 10 },
};

// The fixed act's positions (rodata 0x34fd00, 0x30).
struct CAT_FIX_POS
{
    /* 0x00 */ int    area;
    /* 0x04 */ u_char _04[0xc];
    /* 0x10 */ float  pos[3];
    /* 0x1c */ float  rot;              // degrees
    /* 0x20 */ s_char mot;              // -1: none
    /* 0x21 */ u_char _21[0xf];
};
static_assert(sizeof(CAT_FIX_POS) == 0x30, "CAT_FIX_POS size");

static const CAT_FIX_POS cat_fix_pos[2] =   // rodata 0x34fd00
{
    { 0x30, {}, { 158.6f,  -7.9f, 244.0f  }, 103.0f, 14, {} },
    { 0x32, {}, {  89.6f, -58.4f, 111.47f },   0.0f, 14, {} },
};

// The event spot (sdata 0x36bf80): inside rectangle label, play mot, then go
// to next_label.
struct CAT_EV_SPOT
{
    int    label;                       // MrecIsInEvent label
    s_char mot;
    s_char next_label;
    u_char _6[2];
};
static_assert(sizeof(CAT_EV_SPOT) == 8, "CAT_EV_SPOT size");

static const CAT_EV_SPOT cat_ev_spot[1] = { { 0x42c, 0x16, 0, {} } };  // sdata 0x36bf80

// Accessories (rodata 0x34fda0 through sdata 0x36bfb0): A000_PK4 + file on
// the body's joint.
struct CAT_ACS_DAT
{
    short file;
    short _2;
    int   joint;
};
static_assert(sizeof(CAT_ACS_DAT) == 8, "CAT_ACS_DAT size");

static const CAT_ACS_DAT cat_acs_dat0[2] = { { 21, 0, 2 }, { 22, 0, 15 } };   // rodata 0x34fda0
static const CAT_ACS_DAT *const cat_acs_tbl[CAT_NUM] = { cat_acs_dat0 };       // sdata 0x36bfb0

// The second model's motion variants (sdata 0x36bf88 / 0x36bf90; counts 0x36bf98):
// row[0] is the body's motion, the part plays one of row[0..2].
static const s_char cat_mot_var0[4] = { 0x00, 0x1d, 0x1e, 0 };
static const s_char cat_mot_var1[4] = { 0x13, 0x1f, 0x20, 0 };
static const s_char *const cat_mot_var[2] = { cat_mot_var0, cat_mot_var1 };     // sdata 0x36bf40
static const u_char cat_mot_var_num[2] = { 3, 3 };

// Model files (sdata 0x36bfd0 / 0x36bfe0). Per cat {chr, type, motion PK4}
// at cat * 3 (part 1 at + 4); per (cat + type) {model, shape} at * 2 (part
// 1 at + 4). Only type 0 has files.
static const short cat_mdl_chr[8]  = { 0xc, 5, CH015_DEFAULT_PK4, 0, 0xf, 7, CH019_DEFAULT_PK4, 0 };
static const short cat_mdl_file[8] = { CH015_PK4, CH015_SHP, 0, 0, CH019_PK4, -1, 0, 0 };

// Idle kind weights (sdata 0x36c030, copied to the stack).
static const u_short cat_idle_kind_weight[2] = { 80, 20 };

// The door look's player-camera event camera (rodata 0x34fdb0 / 0x34fdc0).
static float cat_evcam_pos[4] = { 147.9f, -84.86f, 225.69f, 1.0f };
static float cat_evcam_tgt[4] = { 120.99f, -56.53f, 137.07f, 1.0f };

static const sceVu0FVECTOR cat_origin = { 0.0f, 0.0f, 0.0f, 1.0f };     // rdata 0x329500
static sceVu0FMATRIX cat_unit = {                                       // data 0x3293c0
    { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f },
};

// ──────────────────────────────────────────────────────────────────────
// Work (bss 0x3914f0..0x391610, sbss gp-0x710..)
// ──────────────────────────────────────────────────────────────────────

static sceVu0FVECTOR cat_view_eye;      // bss 0x3914f0: the cat view's eye
static sceVu0FVECTOR cat_view_rot;      // bss 0x391500: its base rotation (eye -> the cat's head)
static sceVu0FVECTOR cat_view_up;       // bss 0x391510: written, read nowhere in this file
static float cat_view_pitch;            // bss 0x391520: the stick's added pitch
static float cat_view_yaw;              // bss 0x391524: the stick's added yaw
static sceVu0FVECTOR cat_se_pos;        // bss 0x391530: the SE's source (the head)
static SND_3D_SET cat_se_3d;            // bss 0x391540: pos only
static CAT_WRK cat_wrk[CAT_NUM];        // bss 0x391550

static int    cat_area_from = 0;        // sbss gp-0x710: the area the player left by a door
static int    cat_area_to = 0;          // sbss gp-0x70c: the area it entered, -1: none
static s_char cat_idle_step;            // sbss gp-0x704
static s_char cat_idle_sel;             // sbss gp-0x703
static s_char cat_idle_loop;            // sbss gp-0x702
static s_char cat_react_step;           // sbss gp-0x701
static s_char cat_react_sel;            // sbss gp-0x700
static s_char cat_react_loop;           // sbss gp-0x6ff
static s_char cat_motset_step;          // sbss gp-0x6fe: 0 start, 1 loop, 2 end
static int    cat_motset_wait;          // sbss gp-0x6fc: frames
static int    cat_shape_wait[2];        // sbss gp-0x6e8
static int    cat_shape_play[2];        // sbss gp-0x6e0

// ──────────────────────────────────────────────────────────────────────
// Act tables (the functions are defined below)
// ──────────────────────────────────────────────────────────────────────

typedef void (*CAT_ACT_INIT)(CAT_WRK *cat);
typedef int  (*CAT_ACT_FUNC)(CAT_WRK *cat);

static void CatActFixInit(CAT_WRK *cat);
static int  CatActFixCheck(CAT_WRK *cat);
static int  CatActFixMain(CAT_WRK *cat);
static void CatActFreeInit(CAT_WRK *cat);
static int  CatActFreeCheck(CAT_WRK *cat);
static int  CatActFreeMain(CAT_WRK *cat);
static void CatStayInit(CAT_WRK *cat);
static int  CatStayCheck(CAT_WRK *cat);
static int  CatStayMain(CAT_WRK *cat);
static void CatLoopInit(CAT_WRK *cat);
static int  CatLoopCheck(CAT_WRK *cat);
static int  CatLoopMain(CAT_WRK *cat);
static void CatEvSpotInit(CAT_WRK *cat);
static int  CatEvSpotCheck(CAT_WRK *cat);
static int  CatEvSpotMain(CAT_WRK *cat);
static void CatDoorLookInit(CAT_WRK *cat);
static int  CatDoorLookCheck(CAT_WRK *cat);
static int  CatDoorLookMain(CAT_WRK *cat);
static void CatIdleInit(CAT_WRK *cat);
static int  CatIdleCheck(CAT_WRK *cat);
static int  CatIdleMainLoop(CAT_WRK *cat);
static void CatPetInit(CAT_WRK *cat);
static int  CatPetCheck(CAT_WRK *cat);
static int  CatPetMain(CAT_WRK *cat);
static int  CatPetStep0(CAT_WRK *cat);
static int  CatPetStep1(CAT_WRK *cat);
static int  CatPetStep2(CAT_WRK *cat);
static int  CatPetStep3(CAT_WRK *cat);
static int  CatPetStep4(CAT_WRK *cat);
static int  CatPetStep5(CAT_WRK *cat);
static int  CatPetStep6(CAT_WRK *cat);
static int  CatPetStep7(CAT_WRK *cat);

// top acts: 0 fixed, 1 free (sdata 0x36bed0 / 0x36bed8 / 0x36bee0)
static const CAT_ACT_INIT cat_act_init[2]  = { CatActFixInit, CatActFreeInit };
static const CAT_ACT_FUNC cat_act_check[2] = { CatActFixCheck, CatActFreeCheck };
static const CAT_ACT_FUNC cat_act_main[2]  = { CatActFixMain, CatActFreeMain };

// the free act's move kinds: 0 stay, 1 loop (sdata 0x36c000 / 0x36c008 / 0x36c010)
static const CAT_ACT_INIT cat_move_init[2]  = { CatStayInit, CatLoopInit };
static const CAT_ACT_FUNC cat_move_check[2] = { CatStayCheck, CatLoopCheck };
static const CAT_ACT_FUNC cat_move_main[2]  = { CatStayMain, CatLoopMain };

// the free act's sub acts: 0 event spot, 1 door look (sdata 0x36c018 / 0x36c020 / 0x36c028)
static const CAT_ACT_INIT cat_sub_init[2]  = { CatEvSpotInit, CatDoorLookInit };
static const CAT_ACT_FUNC cat_sub_check[2] = { CatEvSpotCheck, CatDoorLookCheck };
static const CAT_ACT_FUNC cat_sub_main[2]  = { CatEvSpotMain, CatDoorLookMain };

// stay: 0 idle, 1 pet (sdata 0x36c040 / 0x36c048 / 0x36c050; CatStayCheck /
// CatStayMain call the entries by fixed index)
static const CAT_ACT_INIT cat_stay_init[2]  = { CatIdleInit, CatPetInit };
static const CAT_ACT_FUNC cat_stay_check[2] = { CatIdleCheck, CatPetCheck };
static const CAT_ACT_FUNC cat_stay_main[2]  = { CatIdleMainLoop, CatPetMain };

// the pet act's steps (data 0x2f30a0)
static const CAT_ACT_FUNC cat_pet_step[8] =
{
    CatPetStep0, CatPetStep1, CatPetStep2, CatPetStep3, CatPetStep4, CatPetStep5, CatPetStep6, CatPetStep7,
};

static int  CatGetJointPos(s_char cat, int joint, float *out, float *rot);
static int  CatGetHeadPos(s_char cat, float *out, float *rot);
static int  CatIsActive(CAT_WRK *cat);
static void CatMdlMotionStart(s_char cat, s_char mot, s_char start);
static void CatMdl2MotionStart(s_char cat, s_char mot, s_char start);
static CharCatModel *CatGetMdl(s_char cat);

// ──────────────────────────────────────────────────────────────────────
// The act state back to its start: every top act's init runs.
// FF3 0x0013efa0

static void CatActReset(CAT_WRK *cat, s_char no)
{
    s_char i;

    memset(cat, 0, CAT_ACT_SIZE);
    cat->no  = no;
    cat->act = -1;
    for (i = 0; i < 2; i++)
        cat_act_init[i](cat);
}

// One frame of the acts: with none running, the first whose check fires
// starts; the running one's main returning non-zero ends it.
// FF3 0x0013f028

static int CatActMain(CAT_WRK *cat)
{
    s_char act = cat->act;
    s_char i;

    if (act == -1)
    {
        for (i = 0; i < 2; i++)
        {
            if (cat_act_check[i](cat) != 0)
            {
                cat->act = i;
                break;
            }
        }
        act = cat->act;
    }
    if (act != -1 && cat_act_main[act](cat) != 0)
        cat->act = -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The cat view (the pet act sets PLYR_WRK _0e0 bit 1; the viewfinder then
// looks from the player's eye toward the cat's head).
// FF3 0x0013f0e8

void CatViewInit(void)
{
    PLYR_WRK     *p = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR head;
    float         w = 1.0f;

    cat_view_pitch = 0.0f;
    cat_view_yaw   = 0.0f;
    cat_view_eye[0] = 0.0f;
    cat_view_eye[1] = p->crawl_h;
    cat_view_eye[2] = 0.0f;
    cat_view_eye[3] = w;
    RotVectorZXY(cat_view_eye, cat_view_eye, p->rot);
    cat_view_eye[0] += p->pos[0];
    cat_view_eye[1] += p->pos[1];
    cat_view_eye[2] += p->pos[2];
    cat_view_eye[3] = w;
    CatGetHeadPos(0, head, nullptr);
    GetTrgtRot(cat_view_eye, head, cat_view_rot, 3);
    cat_view_up[0] = 0.0f;
    cat_view_up[1] = w;
    cat_view_up[2] = 0.0f;
    cat_view_up[3] = w;
    RotVectorZXY(cat_view_up, cat_view_up, cat_view_rot);
}

// The right stick (GetStickRaw 5) turns the cat view: x past 50 from the
// centre turns the yaw (faster with PAD_KEY_05 held), y the pitch (scaled by
// the gauge's time step); the finder point follows the raw stick.
// callcheck: litodp, dpcmp (the EE's soft-double helpers of fabs((double)sx) >= 50.0)
// FF3 0x0013f1c8

void CatViewCtrl(void)
{
    u_char x, y;
    int    sx, sy;
    float  d, f, speed;

    if (IsLockTblOn(3) != 0)
        return;

    d = 0.0f;
    GetStickRaw(5, &x, &y);
    sx = (s_char)(x - 0x80);
    sy = (s_char)(y - 0x80);
    if (fabs((double)sx) >= 50.0)
    {
        f = (sx < 1) ? (float)sx + 50.0f : (float)sx - 50.0f;
        f = f / 78.0f;
        speed = (CheckPadNowPlaySe((u_short)pad_key[PAD_KEY_05], -1, 0) != 0) ? 0.0698131695f : 0.0383972414f;
        d = speed * f;
    }
    cat_view_yaw += d;
    if (cat_view_yaw > 0.69813168f)
        cat_view_yaw = 0.69813168f;
    if (cat_view_yaw < -0.69813168f)
        cat_view_yaw = -0.69813168f;

    d = 0.0f;
    if (fabs((double)sy) >= 50.0)
    {
        f = (sy < 1) ? (float)sy + 50.0f : (float)sy - 50.0f;
        f = f / 78.0f;
        speed = (CheckPadNowPlaySe((u_short)pad_key[PAD_KEY_05], -1, 0) != 0) ? 0.0558505356f : 0.0314159244f;
        d = speed * f * PlyrGaugeTimeStep(GetAlbumSaveDisp());
    }
    cat_view_pitch -= d;
    if (cat_view_pitch > 0.523598731f)
        cat_view_pitch = 0.523598731f;
    if (cat_view_pitch < -0.523598731f)
        cat_view_pitch = -0.523598731f;
    PlyrFinderPointMove((float)sx, (float)sy);
}

// FF3 0x0013f468

void CatViewCameraSet(void)
{
    sceVu0FVECTOR rot;
    sceVu0FVECTOR tgt;
    float         fov;

    rot[0] = cat_view_pitch + cat_view_rot[0];
    rot[1] = cat_view_yaw + cat_view_rot[1];
    rot[2] = cat_view_rot[2];
    rot[3] = cat_view_rot[3];
    RotLimitChk(&rot[1]);
    tgt[0] = 0.0f;
    tgt[1] = 0.0f;
    tgt[2] = 50.0f;
    tgt[3] = 1.0f;
    RotVectorZXY(tgt, tgt, rot);
    tgt[0] += cat_view_eye[0];
    tgt[1] += cat_view_eye[1];
    tgt[2] += cat_view_eye[2];
    tgt[3] += cat_view_eye[3];
    gra3dcamSetPosition(cat_view_eye);
    gra3dcamSetTarget(tgt, 1);
    fov = GetPhotoModeWrk()->GetZoom() * 0.835f;
    gra3dcamSetFov(fov * (1.85f - GetPhotoModeWrk1e8()->GetDispRate()));
    gra3dApplyCamera(nullptr, 1, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The areas the player crossed by the last door (from: the door opened,
// to: the door closed).
// FF3 0x0013f590

static void CatAreaMoveClear(void)
{
    cat_area_to   = -1;
    cat_area_from = -1;
}

// FF3 0x0013f5a8
static void CatAreaMoveSet(int to, int area)
{
    if (to == 0)
    {
        cat_area_from = area;
        cat_area_to   = -1;
        return;
    }
    cat_area_to = area;
}

// FF3 0x0013f5c8
static int CatAreaMoveIsChanged(void)
{
    if (cat_area_to == -1 || cat_area_to == cat_area_from)
        return 0;
    return 1;
}

// 1 (once; the record is cleared) when the player went from area from to an
// area other than not_to.
// FF3 0x0013f5f0

static int CatAreaMoveCheck(int from, int not_to)
{
    if (CatAreaMoveIsChanged() == 0 || cat_area_from != from || cat_area_to == not_to)
        return 0;
    CatAreaMoveClear();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Bit 1: pos is within dist of the cat's head and within ang of the head's
// facing (yaw); bit 2 (with rot, pos's facing): the head is within ang of
// pos's facing. 0 without a model.
// callcheck: fptodp, dptofp (the EE's soft-double helpers of fabs((double)d))
// FF3 0x0013f658

static int CatSightCheck(float dist, float ang, s_char cat, float *pos, float *rot)
{
    sceVu0FVECTOR head;
    float         head_rot[4];
    float         r[4];
    float         d[2];
    float         dx, dy, dz;
    int           ret = 0;

    if (CatGetHeadPos(cat, head, head_rot) == 0)
        return 0;

    dx = head[0] - pos[0];
    dy = head[1] - pos[1];
    dz = head[2] - pos[2];
    // FF3: vadday.x / vmaddz.x with vf1 -- taken as x2 + y2 + z2
    if (sqrtf(dx * dx + dy * dy + dz * dz) <= dist)
    {
        GetTrgtRot(head, pos, r, 3);
        d[0] = r[0] - head_rot[0];
        d[1] = r[1] - head_rot[1];
        RotLimitChk(&d[0]);
        RotLimitChk(&d[1]);
        d[0] = (float)fabs((double)d[0]);
        d[1] = (float)fabs((double)d[1]);
        ret = d[1] <= ang;
        if (rot != nullptr)
        {
            GetTrgtRot(pos, head, r, 3);
            d[0] = r[0] - rot[0];
            d[1] = r[1] - rot[1];
            RotLimitChk(&d[0]);
            RotLimitChk(&d[1]);
            d[0] = (float)fabs((double)d[0]);
            if ((float)fabs((double)d[1]) <= ang)
                ret |= 2;
        }
    }
    return ret;
}

// CatSightCheck against the player's head (joint 2), when the cat is active
// and nothing stands between them.
// FF3 0x0013f830

static int CatPlyrSightCheck(float dist, float ang, s_char cat)
{
    sceVu0FVECTOR ppos;
    float         prot[4];
    sceVu0FVECTOR head;

    if (AllPlyrIsReady() == 0)
        return 0;
    PlyrGetJointPos(2, ppos, nullptr);
    prot[0] = PlyrGetJointRot(2, 0);
    prot[1] = PlyrGetJointRot(2, 1);
    prot[2] = 0.0f;
    prot[3] = 0.0f;
    if (CatGetHeadPos(cat, head, nullptr) == 0)
        return 0;
    if (CatIsActive(CatGetWrk(cat)) == 0)
        return 0;
    if (PlyrLineHitCheck(head) != 0)
        return 0;
    if (MapAreaHitLineCheck(head, ppos, GetPlyrAreaNo()) != 0)
        return 0;
    return CatSightCheck(dist, ang, cat, ppos, prot);
}

// The flat (x, z) distance of the cat's and the player's joint 0x1b against
// dist: mode 0 at least, 1 equal, 2 at most.
// FF3 0x0013f950

static int CatPlyrDistCheck(float dist, s_char cat, s_char mode)
{
    sceVu0FVECTOR c, p;
    float         d;

    if (AllPlyrIsReady() == 0)
        return 0;
    CatGetJointPos(cat, 0x1b, c, nullptr);
    PlyrGetJointPos(0x1b, p, nullptr);
    d = sqrtf((c[0] - p[0]) * (c[0] - p[0]) + (c[2] - p[2]) * (c[2] - p[2]));
    if (mode == 1)
        return dist == d;
    if (mode == 0)
        return dist <= d;
    if (mode == 2)
        return d <= dist;
    return 0;
}

// The player's new position against the cat's body (joint 0x1b, flat
// distance r + 3.5) when both are in area and within 35 in height: a move
// that started outside goes back to pos_old; one already inside is pushed
// out to the radius, away from the cat.
// FF3 0x0013fa38

void CatPlyrHitCheck(float r, int area, float *pos, float *pos_old)
{
    CAT_WRK      *cat;
    sceVu0FVECTOR j, ang, v;
    sceVu0FMATRIX m;
    float         rot[4];
    float         d_new, d_old;
    s_char        i;

    r += 3.5f;
    for (i = 0; i < CAT_NUM; i++)
    {
        cat = CatGetWrk(i);
        if (cat->loaded == 0 || cat->active == 0 || cat->visible == 0 || cat->area != area)
            continue;
        if (fabsf(pos[1] - cat->pos[1]) > 35.0f)
            continue;

        CatGetJointPos(i, 0x1b, j, nullptr);
        d_new = sqrtf((j[0] - pos[0]) * (j[0] - pos[0]) + (j[2] - pos[2]) * (j[2] - pos[2]));
        d_old = sqrtf((j[0] - pos_old[0]) * (j[0] - pos_old[0]) + (j[2] - pos_old[2]) * (j[2] - pos_old[2]));
        if (d_new >= r)
            continue;
        if (d_old < r)
        {
            j[1] = pos_old[1];
            GetTrgtRot(j, pos_old, rot, 2);
            ang[0] = NormalizeAngle(rot[0]);
            ang[1] = NormalizeAngle(rot[1]);
            ang[2] = NormalizeAngle(rot[2]);
            ang[3] = 0.0f;              // PC: not set in FF3 (stack), not read
            sceVu0RotMatrix(m, cat_unit, ang);
            v[0] = 0.0f;
            v[1] = 0.0f;
            v[2] = r;
            v[3] = 1.0f;
            sceVu0ApplyMatrix(v, m, v);
            pos[0] = j[0] + v[0];
            pos[1] = j[1] + v[1];
            pos[2] = j[2] + v[2];
            pos[3] = j[3] + v[3];
        }
        else
        {
            pos[0] = pos_old[0];
            pos[1] = pos_old[1];
            pos[2] = pos_old[2];
            pos[3] = pos_old[3];
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Table getters (the row count through num).
// FF3 0x0013fc68

static const CAT_LABEL_DAT *CatGetLabelTbl(s_char *num)
{
    if (num != nullptr)
        *num = CAT_LABEL_NUM;
    return cat_label_tbl;
}

// FF3 0x0013fc80
static const CAT_LABEL_MOVE *CatGetLabelMoveTbl(s_char *num)
{
    if (num != nullptr)
        *num = 6;
    return cat_label_move;
}

// FF3 0x0013fc98
static const CAT_VIEW_CAM *CatGetViewCamTbl(s_char *num)
{
    if (num != nullptr)
        *num = 3;
    return cat_view_cam;
}

// FF3 0x0013fcb0
static const CAT_EV_SPOT *CatGetEvSpotTbl(s_char *num)
{
    if (num != nullptr)
        *num = 1;
    return cat_ev_spot;
}

// FF3 0x0013fcc8
static const CAT_DOOR_DAT *CatGetDoorTbl(s_char *num)
{
    if (num != nullptr)
        *num = 1;
    return cat_door_tbl;
}

// FF3 0x0013fce0
static const CAT_FIX_POS *CatGetFixPosTbl(s_char *num)
{
    if (num != nullptr)
        *num = 2;
    return cat_fix_pos;
}

// FF3 0x0013fcf8
static const CAT_MOT_SET *CatGetMotSet(s_char no)
{
    return &cat_mot_set[no];
}

// An idle motion of kind: *sel when >= 0, else a weighted random one (into
// *sel). nullptr when the kind has none.
// FF3 0x0013fd20

static const CAT_IDLE_MOT *CatGetIdleMot(s_char kind, s_char *sel)
{
    const CAT_IDLE_MOT *tbl;
    s_char              num = cat_idle_mot_num[kind];
    s_char              i, s;
    short               sum, r;

    if (num <= 0)
        return nullptr;
    tbl = cat_idle_mot_tbl[kind];
    s   = *sel;
    if (s < 0)
    {
        sum = 0;
        for (i = 0; i < num; i++)
            sum = (short)(sum + tbl[i].weight);
        r   = (short)GetRandValIByF(sum);
        sum = 0;
        for (i = 0; i < num; i++)
        {
            sum = (short)(sum + tbl[i].weight);
            if (r < sum)
                break;
        }
        if (i < num)
            *sel = i;
        s = *sel;
    }
    return &cat_idle_mot_tbl[kind][s];
}

// The same over the pet reactions.
// FF3 0x0013fe50

static const CAT_IDLE_MOT *CatGetReactMot(s_char kind, s_char *sel)
{
    const CAT_IDLE_MOT *tbl;
    s_char              num = cat_react_mot_num[kind];
    s_char              i, s;
    short               sum, r;

    if (num <= 0)
        return nullptr;
    tbl = cat_react_mot_tbl[kind];
    s   = *sel;
    if (s < 0)
    {
        sum = 0;
        for (i = 0; i < num; i++)
            sum = (short)(sum + tbl[i].weight);
        r   = (short)GetRandValIByF(sum);
        sum = 0;
        for (i = 0; i < num; i++)
        {
            sum = (short)(sum + tbl[i].weight);
            if (r < sum)
                break;
        }
        if (i < num)
            *sel = i;
        s = *sel;
    }
    return &cat_react_mot_tbl[kind][s];
}

// FF3 0x0013ff80
static const CAT_MOT_SE *CatGetMotSe(s_char mot)
{
    if (mot < 0)
        return nullptr;
    return &cat_mot_se[mot];
}

// The part's motion for the body's mot: a random variant for the two
// motions that have them.
// FF3 0x0013ffb0

static s_char CatMotVariant(s_char mot)
{
    const s_char *v;
    u_char        i;

    for (i = 0; i < 2; i++)
    {
        v = cat_mot_var[i];
        if (mot == v[0])
            return v[GetRandValIByF(cat_mot_var_num[i])];
    }
    return mot;
}

// A random free label (no NPC holds it) by the weights of mode; -1: none.
// FF3 0x00140028

static s_char CatLabelSelect(s_char no, int mode)
{
    CAT_WRK             *cat = CatGetWrk(no);
    const CAT_LABEL_DAT *tbl;
    s_char               num, i;
    short                sum, r;

    tbl = CatGetLabelTbl(&num);
    sum = 0;
    for (i = 0; i < num; i++)
    {
        if (cat->lock[i] == 0)
            sum = (short)(sum + tbl[i].weight[mode]);
    }
    r   = (short)GetRandValIByF(sum);
    sum = 0;
    for (i = 0; i < num; i++)
    {
        if (cat->lock[i] == 0)
        {
            sum = (short)(sum + tbl[i].weight[mode]);
            if (r < sum)
                return i;
        }
    }
    return -1;
}

// FF3 0x00140140
static s_char CatLabelGetMove(s_char label)
{
    return CatGetLabelTbl(nullptr)[label].move;
}

// FF3 0x00140180
static s_char CatLabelGetSub(s_char label)
{
    return CatGetLabelTbl(nullptr)[label].sub;
}

// FF3 0x001401c0
static s_char CatLabelGetSpot(s_char label)
{
    return CatGetLabelTbl(nullptr)[label].spot;
}

// FF3 0x00140200
static int CatLabelGetArea(s_char label)
{
    return CatGetLabelTbl(nullptr)[label].area;
}

// A weighted random move from label to a free label; label when none.
// FF3 0x00140240

static s_char CatLabelMoveSelect(s_char no, s_char label)
{
    CAT_WRK              *cat = CatGetWrk(no);
    const CAT_LABEL_MOVE *tbl;
    s_char                num, i;
    u_char                sum, r;

    tbl = CatGetLabelMoveTbl(&num);
    sum = 0;
    for (i = 0; i < num; i++)
    {
        if (cat->lock[tbl[i].to] == 0 && tbl[i].from == label)
            sum = (u_char)(sum + tbl[i].weight);
    }
    if (sum != 0)
    {
        r   = (u_char)GetRandValIByF(sum);
        sum = 0;
        for (i = 0; i < num; i++)
        {
            if (cat->lock[tbl[i].to] == 0 && tbl[i].from == label)
            {
                sum = (u_char)(sum + tbl[i].weight);
                if (r < sum)
                    return tbl[i].to;
            }
        }
    }
    return label;
}

// NPC_SET_POS_LABEL: the cat to label's place and facing, its look off.
// FF3 0x00140378

void CatSetPosLabel(s_char cat, s_char label)
{
    const CAT_LABEL_DAT *e = &CatGetLabelTbl(nullptr)[label];
    sceVu0FVECTOR        pos;
    float                rot[4];

    pos[0] = e->pos[0];
    pos[1] = e->pos[1];
    pos[2] = e->pos[2];
    pos[3] = 1.0f;
    rot[0] = (e->rot * 3.14159274f) / 180.0f;
    RotLimitChk(rot);
    CatSetPos(cat, e->area, pos, rot[0]);
    CatSetEyeOn(cat, 0);
    CatSetEyePos(cat, nullptr);
    ingame_wrk.plyr_wrk->_0e0 &= ~2;
}

// NPC_MOTION_CALL: both models' animes (the part's may be a variant) and
// the motion's SE from the head.
// FF3 0x00140470

void CatMotionCall(s_char cat, s_char mot, s_char start)
{
    const CAT_MOT_SE *se;
    void             *s3s;

    CatMdlMotionStart(cat, mot, start);
    CatMdl2MotionStart(cat, CatMotVariant(mot), start);
    if (CatIsActive(CatGetWrk(cat)) == 0)
        return;
    se = CatGetMotSe(mot);
    if (se == nullptr || se->se == -1)
        return;
    if (CatGetHeadPos(cat, cat_se_pos, nullptr) != 0)
    {
        cat_se_3d.pos = cat_se_pos;
        s3s = &cat_se_3d;
    }
    else
    {
        s3s = nullptr;
    }
    SndSePlay((u_int)se->se, 100, 0x1000, 0, s3s, 0);
}

// Label's motion set number idx (idx < 0: random among the row's six bytes
// from motset -- FF3 draws 0..5 there; every caller passes 0 or 1).
// FF3 0x00140580

static s_char CatLabelGetMotSet(s_char label, s_char idx)
{
    s_char        num;
    const s_char *b = CatGetLabelTbl(&num)[label].motset;

    if (idx < 0)
        idx = (s_char)GetRandValIByF(num);
    return b[idx];
}

// The number of idle motions before kind 1 comes back: 4..10 for kind 1, 0
// for kind 0.
// FF3 0x001405f0

static s_char CatIdleCountGet(s_char kind)
{
    if (kind == 0)
        return 0;
    return (s_char)(GetRandValIByF(7) + 4);
}

// The pet act's event camera: a random row of label, the player put at its
// place.
// FF3 0x00140630

static void CatViewCamSet(s_char label)
{
    const CAT_VIEW_CAM *tbl, *e;
    s_char              num, n, i;
    s_char              list[3];
    sceVu0FVECTOR       pos, tgt;

    tbl = CatGetViewCamTbl(&num);
    n   = 0;
    for (i = 0; i < num; i++)
    {
        if (tbl[i].label == label)
            list[n++] = i;
    }
    e = &tbl[list[(s_char)GetRandValIByF(n)]];
    PlyrCameraPlyrPlace(e->plyr, -1);
    EventCameraReq();
    EventCameraSetWorldFlg(1);
    pos[0] = e->pos[0];
    pos[1] = e->pos[1];
    pos[2] = e->pos[2];
    pos[3] = 1.0f;
    tgt[0] = e->tgt[0];
    tgt[1] = e->tgt[1];
    tgt[2] = e->tgt[2];
    tgt[3] = 1.0f;
    EventCameraSetPosition(pos);
    EventCameraSetTarget(tgt);
    EventCameraSetFov(e->fov);
    EventCameraSetRoll(e->roll);
}

// FF3 0x001407a8
static void CatViewCamEnd(void)
{
    EventCameraCut();
    EventCameraSetWorldFlg(0);
}

// The player stands in the event spot's rectangle.
// FF3 0x001407c8

static int CatEvSpotIsIn(s_char no)
{
    PLYR_WRK *p = ingame_wrk.plyr_wrk;

    return MrecIsInEvent(p->pos, CatGetEvSpotTbl(nullptr)[no].label, p->floor) != 0;
}

// FF3 0x00140818
static s_char CatEvSpotGetMot(s_char no)
{
    return CatGetEvSpotTbl(nullptr)[no].mot;
}

// FF3 0x00140850
static s_char CatEvSpotGetLabel(s_char no)
{
    return CatGetEvSpotTbl(nullptr)[no].next_label;
}

// FF3 0x00140888
static void CatDoorGetPos(s_char no, float *out)
{
    const CAT_DOOR_DAT *e = &CatGetDoorTbl(nullptr)[no];

    out[0] = e->pos[0];
    out[1] = e->pos[1];
    out[2] = e->pos[2];
    out[3] = 1.0f;
}

// The follow camera of the cat's door no when it is door.
// FF3 0x001408f8

static int CatDoorGetCamera(s_char no, short door, float *pos, float *tgt)
{
    const CAT_DOOR_DAT *e = &CatGetDoorTbl(nullptr)[no];

    if (e->door == door)
    {
        pos[0] = e->cam_pos[0];
        pos[1] = e->cam_pos[1];
        pos[3] = 1.0f;
        pos[2] = e->cam_pos[2];
        tgt[0] = e->cam_tgt[0];
        tgt[1] = e->cam_tgt[1];
        tgt[3] = 1.0f;
        tgt[2] = e->cam_tgt[2];
    }
    return e->door == door;
}

// FF3 0x001409b8
static s_char CatDoorGetLabel(s_char no)
{
    return CatGetDoorTbl(nullptr)[no].label;
}

// The fixed act: the cat to fixed position no, its motion started.
// FF3 0x001409f8

static void CatPutFixPos(s_char cat, s_char no)
{
    const CAT_FIX_POS *e = &CatGetFixPosTbl(nullptr)[no];
    sceVu0FVECTOR      pos;
    float              rot[4];

    pos[0] = e->pos[0];
    pos[1] = e->pos[1];
    pos[2] = e->pos[2];
    pos[3] = 1.0f;
    rot[0] = (e->rot * 3.14159274f) / 180.0f;
    RotLimitChk(rot);
    CatSetPos(cat, e->area, pos, rot[0]);
    CatSetEyeOn(cat, 0);
    CatSetEyePos(cat, nullptr);
    ingame_wrk.plyr_wrk->_0e0 &= ~2;
    if (e->mot != -1)
        CatMotionCall(cat, e->mot, 0);
}

// ──────────────────────────────────────────────────────────────────────
// An idle motion of kind (CatGetIdleMot): mot, loop min..max times, end.
// FF3 0x00140b10

static void CatIdleMotStart(s_char cat, s_char kind)
{
    cat_idle_step = 0;
    cat_idle_sel  = -1;
    CatMotionCall(cat, CatGetIdleMot(kind, &cat_idle_sel)->mot, 5);
}

// 1 once the idle motion is over.
// FF3 0x00140b60

static int CatIdleMotMain(s_char cat, s_char kind)
{
    const CAT_IDLE_MOT *m = CatGetIdleMot(kind, &cat_idle_sel);

    if (cat_idle_step == 1)
        goto loop;
    if (cat_idle_step == 2)
        goto end;
    if (cat_idle_step != 0)
        return 1;

    if (CatMotionIsEnd(cat) == 0)
        return 0;
    if (m->loop < 0)
        return 1;
    cat_idle_loop = (s_char)(m->min + GetRandValIByF(m->max - m->min + 1));
    if (cat_idle_loop <= 0)
    {
        if (m->end < 0)
            return 1;
        CatMotionCall(cat, m->end, 5);
        cat_idle_step = 2;
        return 0;
    }
    CatMotionCall(cat, m->loop, 5);
    cat_idle_step++;
  loop:
    if (CatMotionIsEnd(cat) == 0)
        return 0;
    cat_idle_loop--;
    if (cat_idle_loop > 0)
    {
        CatMotionCall(cat, m->loop, 5);
        return 0;
    }
    if (m->end < 0)
        return 1;
    CatMotionCall(cat, m->end, 5);
    cat_idle_step++;
  end:
    if (CatMotionIsEnd(cat) == 0)
        return 0;
    cat_idle_step++;
    return 1;
}

// The pet reaction of kind (CatGetReactMot); 0 when the kind has none.
// FF3 0x00140ce0

static int CatReactMotStart(s_char cat, s_char kind)
{
    const CAT_IDLE_MOT *m;

    cat_react_step = 0;
    cat_react_sel  = -1;
    m = CatGetReactMot(kind, &cat_react_sel);
    if (m != nullptr)
        CatMotionCall(cat, m->mot, 10);
    return m != nullptr;
}

// 1 once the reaction is over (or there is none).
// FF3 0x00140d48

static int CatReactMotMain(s_char cat, s_char kind)
{
    const CAT_IDLE_MOT *m = CatGetReactMot(kind, &cat_react_sel);

    if (m == nullptr)
        return 1;
    if (cat_react_step == 1)
        goto loop;
    if (cat_react_step == 2)
        goto end;
    if (cat_react_step != 0)
        return 1;

    if (CatMotionIsEnd(cat) == 0)
        return 0;
    if (m->loop < 0)
        return 1;
    cat_react_loop = (s_char)(m->min + GetRandValIByF(m->max - m->min + 1));
    if (cat_react_loop <= 0)
    {
        if (m->end < 0)
            return 1;
        CatMotionCall(cat, m->end, 5);
        cat_react_step = 2;
        return 0;
    }
    CatMotionCall(cat, m->loop, 5);
    cat_react_step++;
  loop:
    if (CatMotionIsEnd(cat) == 0)
        return 0;
    cat_react_loop--;
    if (cat_react_loop > 0)
    {
        CatMotionCall(cat, m->loop, 5);
        return 0;
    }
    if (m->end < 0)
        return 1;
    CatMotionCall(cat, m->end, 5);
    cat_react_step++;
  end:
    if (CatMotionIsEnd(cat) == 0)
        return 0;
    cat_react_step++;
    return 1;
}

// Label's motion set idx from phase (0 start, 1 loop, 2 end; a missing
// motion skips to the next phase); the loop runs wait[rand 3] seconds.
// FF3 0x00140ed0

static void CatMotSetStart(s_char cat, s_char label, s_char idx, s_char start, s_char phase)
{
    const CAT_MOT_SET *ms;
    s_char             set, mot;

    set = CatLabelGetMotSet(label, idx);
    if (set < 0)
        return;
    ms = CatGetMotSet(set);
    if (ms == nullptr)
        return;

    cat_motset_wait = 0;
    cat_motset_step = phase;
    if (phase == 0)
    {
        mot = ms->start;
        if (mot >= 0)
            goto play;
        cat_motset_step = 1;
    }
    else if (phase != 1)
    {
        if (phase != 2)
        {
            cat_motset_wait = 0;
            return;
        }
        goto end;
    }
    if (ms->loop >= 0)
    {
        cat_motset_wait = ms->wait[GetRandValIByF(3)] * (GetPALMode() == 0 ? 30 : 25);
        mot = ms->loop;
        goto play;
    }
    cat_motset_step++;
  end:
    mot = ms->end;
    if (mot < 0)
    {
        cat_motset_wait = 0;
        cat_motset_step++;
        return;
    }
  play:
    CatMotionCall(cat, mot, start);
}

// One frame of the motion set: -1 no set, 0 running, 1 looping (waiting),
// 2 looping with the wait over, 3 ended and restarted.
// FF3 0x00141020

static int CatMotSetMain(s_char cat, s_char label, s_char idx)
{
    const CAT_MOT_SET *ms;
    s_char             set;

    set = CatLabelGetMotSet(label, idx);
    if (set < 0)
        return -1;
    ms = CatGetMotSet(set);
    if (ms == nullptr)
        return -1;

    if (cat_motset_step == 1)
        goto loop;
    if (cat_motset_step == 2)
        goto end;
    if (cat_motset_step != 0)
        goto out;

    if (CatMotionIsEnd(cat) == 0)
        goto out;
    cat_motset_step++;
    if (ms->loop < 0)
        goto out;
    cat_motset_wait = ms->wait[GetRandValIByF(3)] * (GetPALMode() == 0 ? 30 : 25);
    CatMotionCall(cat, ms->loop, 5);
  loop:
    if (cat_motset_wait > 0)
    {
        cat_motset_wait--;
        return 1;
    }
    if (ms->end < 0)
    {
        cat_motset_wait = ms->wait[GetRandValIByF(3)] * (GetPALMode() == 0 ? 30 : 25);
        goto out;
    }
    CatMotionCall(cat, ms->end, 5);
    cat_motset_step++;
  end:
    if (CatMotionIsEnd(cat) != 0)
    {
        CatMotionCall(cat, ms->start, 5);
        cat_motset_step = 0;
        return 3;
    }
  out:
    if (cat_motset_wait < 1 && cat_motset_step == 1)
        return 2;
    return 0;
}

// Motion mot's shape motion i interval in frames, 0: none.
// FF3 0x001411d0

static int CatMotGetShapeWait(s_char mot, s_char i)
{
    const CAT_MOT_SE *se = CatGetMotSe(mot);

    if (se == nullptr)
        return 0;
    return se->shape_wait[i] * (GetPALMode() == 0 ? 30 : 25);
}

// ──────────────────────────────────────────────────────────────────────
// The fixed act: an NPC_ACT row (CatSetFixPos) puts the cat at one of two
// fixed positions until the next reset. The per-position hooks are empty.
// FF3 0x00141240 / 0x00141248 / 0x00141250 / 0x00141258

static void CatFixPos0Init(CAT_WRK *cat) {}
static void CatFixPos0Main(CAT_WRK *cat) {}     // FF3 0x00141248
static void CatFixPos1Init(CAT_WRK *cat) {}     // FF3 0x00141250
static void CatFixPos1Main(CAT_WRK *cat) {}     // FF3 0x00141258

// FF3 0x00141260
static void CatActFixInit(CAT_WRK *cat) {}

// FF3 0x00141268
static int CatActFixCheck(CAT_WRK *cat)
{
    // sdata 0x36bfa0, copied to the stack
    static const CAT_ACT_INIT init[2] = { CatFixPos0Init, CatFixPos1Init };
    CAT_WRK *w   = CatGetWrk(cat->no);
    s_char   pos = w->fix_pos;

    if (pos >= 0)
    {
        CatPutFixPos(cat->no, pos);
        init[w->fix_pos](cat);
    }
    return pos >= 0;
}

// FF3 0x001412e8
static int CatActFixMain(CAT_WRK *cat)
{
    // sdata 0x36bfa8, copied to the stack
    static const CAT_ACT_INIT main_fn[2] = { CatFixPos0Main, CatFixPos1Main };
    CAT_WRK *w = CatGetWrk(cat->no);

    if (w->fix_pos >= 0)
        main_fn[w->fix_pos](cat);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The accessory.
// FF3 0x00141350

static const CAT_ACS_DAT *CatGetAcsDat(s_char cat, s_char no)
{
    return &cat_acs_tbl[cat][no];
}

// callcheck: AcsModelCtor (the new-expression)
// FF3 0x00141388
static void CatAcsCreate(s_char cat)
{
    CAT_WRK           *w = CatGetWrk(cat);
    const CAT_ACS_DAT *d;
    AcsModel          *acs;

    if ((s_char)w->acs_no <= 0)
        return;
    d   = CatGetAcsDat(cat, (s_char)(w->acs_no - 1));
    acs = new (operator_new(sizeof(AcsModel))) AcsModel;
    w->acs = acs;
    if (acs->Load(d->file) == 0)
    {
        if (w->acs != nullptr)
        {
            w->acs->~AcsModel();        // FF3: delete through vtable +0x0c
            operator_delete(w->acs);
        }
        w->acs = nullptr;
    }
}

// FF3 0x00141440
static void CatAcsDelete(CAT_WRK *cat)
{
    if (cat->acs != nullptr)
    {
        cat->acs->~AcsModel();          // FF3: delete through vtable +0x0c
        operator_delete(cat->acs);
        cat->acs = nullptr;
    }
}

// FF3 0x00141480
static int CatAcsIsLoaded(CAT_WRK *cat)
{
    if (cat->acs != nullptr && cat->acs->IsLoaded() == 0)
        return 0;
    return 1;
}

// The accessory on its joint of the body.
// FF3 0x001414c0

static void CatAcsSetCoord(s_char cat)
{
    CAT_WRK           *w = CatGetWrk(cat);
    CharCatModel      *m = w->mdl;
    const CAT_ACS_DAT *d;
    int                c;

    if (w->acs == nullptr)
        return;
    d = CatGetAcsDat(cat, (s_char)(w->acs_no - 1));
    c = m->GetJointCoord(d->joint);
    w->acs->SetCoord(&m->GetCoord(0)[c]);
}

// FF3 0x00141570
static void CatAcsDraw(CAT_WRK *cat)
{
    if (cat->acs != nullptr)
    {
        cat->acs->UpdateMatrix();
        cat->acs->DrawMdl();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001415b0

CAT_WRK *CatGetWrk(s_char cat)
{
    return &cat_wrk[cat];
}

// The model pointers forgotten (a load restores the work with stale ones).
// FF3 0x001415d8

static void CatMdlClear(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    w->mdl     = nullptr;
    w->mdl2    = nullptr;
    w->loaded  = 0;
    w->visible = 0;
    w->acs     = nullptr;
}

// The place, look, locks and acts back to their start (models kept).
// FF3 0x00141610

static void CatReset(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);
    s_char   i;

    CatActReset(w, cat);
    w->pos[0]    = 0.0f;
    w->pos[1]    = 0.0f;
    w->fix_pos   = -1;
    w->pos[3]    = 1.0f;
    w->pos[2]    = 0.0f;
    w->rot[0]    = 0.0f;
    w->rot[1]    = 0.0f;
    w->rot[2]    = 0.0f;
    w->rot[3]    = 0.0f;
    w->area      = -1;
    w->mot       = -1;
    w->mot2      = -1;
    w->active    = 0;
    w->act_reset = 0;
    w->run_act   = -1;
    w->run_label = -1;
    for (i = 0; i < CAT_LABEL_NUM; i++)
        w->lock[i] = 0;
    w->eye_on     = 0;
    w->eye_pos_on = 0;
    w->eye_pos[0] = 0.0f;
    w->eye_pos[1] = 0.0f;
    w->eye_pos[2] = 0.0f;
    w->eye_pos[3] = 0.0f;
}

// FF3 0x001416e8
static void CatInit(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    w->acs_no   = 0;
    w->type     = 0;
    w->type_old = 0;
    CatMdlClear(cat);
    CatReset(cat);
}

// The body / the part once loaded, else nullptr.
// FF3 0x00141730

static CharCatModel *CatGetMdl(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    return (w->loaded != 0) ? w->mdl : nullptr;
}

// FF3 0x00141768
static CharCatModel *CatGetMdl2(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    return (w->loaded != 0) ? w->mdl2 : nullptr;
}

// Save block 55: the works.
// PC: the block is saved in the host layout (the work holds the model
// pointers, which CatCtrlLoadRestore clears after a load); FF3's is 0xc0
// bytes (open-items "Save blocks vs. host layouts").
// FF3 0x001417a0

int CatGetSaveData(void **addr)
{
    *addr = cat_wrk;
    return (int)sizeof(cat_wrk);
}

// ──────────────────────────────────────────────────────────────────────
// The body's two shape motions (2 and 1, sdata 0x36bfb8): each plays again
// after its interval for the body's anime (CatMotGetShapeWait).
// FF3 0x001417b8

static void CatShapeReset(void)
{
    s_char i;

    for (i = 0; i < 2; i++)
    {
        cat_shape_play[i] = 0;
        cat_shape_wait[i] = 0;
    }
}

// FF3 0x001417f8
static void CatShapeMain(CAT_WRK *cat)
{
    const int     shape[2] = { 2, 1 };  // sdata 0x36bfb8, copied to the stack
    ModelManager *mot = cat->mdl->mot;
    int           wait;
    s_char        i;

    for (i = 0; i < 2; i++)
    {
        if (cat_shape_play[i] == 0)
        {
            if (cat_shape_wait[i] != 0)
            {
                cat_shape_wait[i]--;
                continue;
            }
            wait = CatMotGetShapeWait(cat->mot, i);
            if (wait != 0)
            {
                cat_shape_wait[i] = wait;
                mot->PlayShape(shape[i], 0, 0, 0, 0.0f, 1.0f);  // FF3: vtable +0x10
                cat_shape_play[i] = 1;
            }
        }
        if (cat_shape_play[i] != 0 && mot->IsShapeEnd((short)shape[i]) != 0)    // FF3: vtable +0x3c
            cat_shape_play[i] = 0;
    }
}

// The body's watch: toward target, or the player's head; off (easing back)
// while the target is out of its range.
// FF3 0x00141930

static void CatWatchMain(CAT_WRK *cat, float *target)
{
    CharWatchControl *ctrl = cat->mdl->ctrl;
    CHAR_WATCH_PRM    prm =             // rodata 0x34fd60, copied to the stack
    {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 5.0f, 15.0f, 50.0f, 0.34906584f, 0.448798954f, 0.184799567f, 0.120830484f,
          1.04719758f, 0.314159274f, 0.628318548f, 0.34906584f, 0.0f },
    };
    sceVu0FVECTOR v;

    if (ctrl == nullptr)
        return;

    if (target == nullptr)
    {
        if (AllPlyrIsReady() != 0)
        {
            PlyrGetJointPos(2, v, nullptr);
            prm.pos[0] = v[0];
            prm.pos[1] = v[1];
            prm.pos[2] = v[2];
            prm.pos[3] = v[3];
        }
    }
    else
    {
        prm.pos[0] = target[0];
        prm.pos[1] = target[1];
        prm.pos[2] = target[2];
        prm.pos[3] = target[3];
    }
    UnitGetTrgtRot(cat->pos, prm.pos, v, 2);
    v[1] -= cat->rot[1];
    RotLimitChk(&v[1]);
    if (ctrl->IsInRange(&prm, v[1]) == 0)
    {
        ctrl->off = 1;
        ctrl->Update(&prm);             // FF3: through the control's vtable +0x08
        ctrl->off = 0;
        return;
    }
    ctrl->Update(&prm);                 // FF3: through the control's vtable +0x08
}

// FF3 0x00141a50
static void CatVisibleCheck(CAT_WRK *cat)
{
    cat->visible = 1;
    if (PlyrCameraAreaIsDrawn(cat->area) == 0)
        cat->visible = 0;
}

// One frame of every cat: once both models (and the accessory) are in, the
// animes start and the draw entry is made; a loaded cat updates its
// matrices, watch, shape motions and (unless an event holds it) its acts.
// FF3 0x00141a88

static void CatMain(void)
{
    CAT_WRK *w;
    s_char   i;

    for (i = 0; i < CAT_NUM; i++)
    {
        w = CatGetWrk(i);
        if (w->loaded == 0)
        {
            if (w->mdl == nullptr || w->mdl->IsLoaded() == 0     // FF3: vtable +0x10
                || w->mdl2 == nullptr || w->mdl2->IsLoaded() == 0)
                continue;
            if (CatAcsIsLoaded(w) != 0)
            {
                w->loaded = 1;
                if (w->mot != -1)
                    w->mdl->StartAnime(w->mot, 0, 0, 1, 0.0f, 1.0f);   // FF3: vtable +0x40
                if (w->mot2 != -1)
                    w->mdl2->StartAnime(w->mot2, 0, 0, 1, 0.0f, 1.0f);
                CatAcsSetCoord(i);
                w->mdl2->ctrl->off = 1;
                CatSetEyeOn(i, w->eye_on);
                CatShapeReset();
                CharDrawSetCat(w);
            }
            if (w->loaded == 0)
                continue;
        }
        w->mdl->UpdateMatrix();
        w->mdl2->UpdateMatrix();
        CatWatchMain(w, (w->eye_pos_on != 0) ? w->eye_pos : nullptr);
        CatShapeMain(w);
        if (w->run_act != 0)
        {
            CatActMain(w);
            if (w->act_reset != 0)
            {
                w->act_reset = 0;
                CatActReset(w, i);
            }
        }
        CatVisibleCheck(w);
    }
}

// Both models of type (-1: the current) loading; act_reset restarts the
// acts. 0 when the cat is inactive, already has models, or a load could
// not start (both are deleted then).
// callcheck: operator_new, CharModelCtor (the CharCatModel new-expressions;
// FF3 inlines the constructor)
// FF3 0x00141c58

int CatMdlCreate(s_char cat, s_char type, int act_reset)
{
    CAT_WRK      *w = CatGetWrk(cat);
    CharCatModel *m;
    int           ok0, ok1;
    int           c3, ct2;

    if (w->active == 0 || w->mdl != nullptr || w->mdl2 != nullptr)
        return 0;
    if (type < 0)
        type = w->type;

    c3  = cat * 3;
    ct2 = (cat + type) * 2;
    m = new (operator_new(sizeof(CharCatModel))) CharCatModel(w, 0);
    w->mdl = m;
    ok0 = m->Load(cat_mdl_chr[c3], cat_mdl_chr[c3 + 1], cat_mdl_file[ct2], cat_mdl_chr[c3 + 2], cat_mdl_file[ct2 + 1]);
    m = new (operator_new(sizeof(CharCatModel))) CharCatModel(w, 1);
    w->mdl2 = m;
    ok1 = m->Load(cat_mdl_chr[c3 + 4], cat_mdl_chr[c3 + 5], cat_mdl_file[ct2 + 4], cat_mdl_chr[c3 + 6], cat_mdl_file[ct2 + 5]);

    if (ok0 != 0 && ok1 != 0)
    {
        if (w->type != type)
        {
            w->type_old = w->type;
            w->type     = type;
        }
        CatAcsCreate(cat);
        ingame_wrk.plyr_wrk->_0e0 &= ~2;
        if (act_reset != 0)
            CatActReset(w, cat);
        return 1;
    }

    if (w->mdl != nullptr)
    {
        w->mdl->~CharCatModel();        // FF3: delete through vtable +0x0c
        operator_delete(w->mdl);
    }
    w->mdl = nullptr;
    if (w->mdl2 != nullptr)
    {
        w->mdl2->~CharCatModel();
        operator_delete(w->mdl2);
    }
    w->mdl2 = nullptr;
    return 0;
}

// The models, draw entry and accessory deleted.
// FF3 0x00141e70

int CatMdlDelete(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    if (w->mdl == nullptr || w->mdl2 == nullptr)
        return 0;
    CharDrawDelete(w);
    CatAcsDelete(w);
    ingame_wrk.plyr_wrk->_0e0 &= ~2;
    if (w->mdl != nullptr)
    {
        w->mdl->~CharCatModel();        // FF3: delete through vtable +0x0c
        operator_delete(w->mdl);
    }
    w->mdl = nullptr;
    if (w->mdl2 != nullptr)
    {
        w->mdl2->~CharCatModel();
        operator_delete(w->mdl2);
    }
    w->loaded = 0;
    w->mdl2   = nullptr;
    return 1;
}

// NPC_MODEL_CHANGE: the models of type (-1: the previous type) unless it is
// already the current.
// FF3 0x00141f28

int CatMdlChange(s_char cat, s_char type, int act_reset)
{
    CAT_WRK *w   = CatGetWrk(cat);
    int      ret = 0;

    if (type < 0)
        type = w->type_old;
    if (w->mdl != nullptr)
    {
        if (type == -1 || w->type == type)
            goto out;
        CatMdlDelete(cat);
    }
    ret = CatMdlCreate(cat, type, 0);
  out:
    if (act_reset != 0)
        CatActReset(w, cat);
    return ret;
}

// ──────────────────────────────────────────────────────────────────────
// ingame.c's entry points (Ghidra had them as CmnLayer*, a guess).
// FF3 0x00141ff0

void CatCtrlInit(void)
{
    CatInit(0);
}

// FF3 0x00142008
void CatCtrlLoadRestore(void)
{
    CatMdlClear(0);
}

// FF3 0x00142020
void CatCtrlReset(void)
{
    CatReset(0);
}

// FF3 0x00142038
void CatCtrlEnd(void)
{
    CatMdlDelete(0);
    CatCtrlInit();
}

// FF3 0x00142058
void CatCtrlMain(void)
{
    CatMain();
}

// Drawn and moving: loaded, active (NPC_ACTIVE_FLG) and its area drawn,
// unless lock 0x19 is on.
// FF3 0x00142070

static int CatIsActive(CAT_WRK *cat)
{
    if (IsLockTblOn(0x19) != 0 || cat == nullptr)
        return 0;
    return cat->active != 0 && cat->loaded != 0 && cat->visible != 0;
}

// FF3 0x001420d0
void CatDraw(CAT_WRK *cat)
{
    if (CatIsActive(cat) == 0)
        return;
    cat->mdl->DrawMdl();
    cat->mdl2->DrawMdl();
    CatAcsDraw(cat);
}

// FF3 0x00142120
int CatIsLoaded(s_char cat)
{
    return CatGetWrk(cat)->loaded;
}

// FF3 0x00142148
int CatGetActive(s_char cat)
{
    return CatGetWrk(cat)->active;
}

// FF3 0x00142170
void CatSetActive(s_char cat, int on)
{
    CatGetWrk(cat)->active = on;
}

// NPC_FIX_EVENT: hold != 0 hands the cat to the event (its acts stop, its
// look off; NPC 0 gets a new label unless fixed itself). hold 0 lets it go:
// back to label (-1: the one saved, -1 again: the acts restart from a
// reset).
// FF3 0x001421a0

void CatFixEvent(s_char cat, int hold, s_char label)
{
    CAT_WRK *w = CatGetWrk(cat);

    if (hold != 0)
    {
        w->run_label = label;
        w->run_act   = 0;
        CatActReset(w, cat);
        CatSetEyeOn(cat, 0);
        CatSetEyePos(cat, nullptr);
        if (PlyrCameraIsFixed(0) == 0)
            GetPlyrCameraWrk(0)->act.label = PlyrCameraSetRandLabel(0, -1, 0);
        return;
    }

    w->act_reset = 0;
    w->run_act   = 1;
    if (label == -1)
    {
        label = w->run_label;
        if (label == -1)
        {
            w->run_act   = -1;
            w->act_reset = 1;
            goto out;
        }
    }
    w->label = label;
    CatSetPosLabel(cat, label);
  out:
    w->run_label = -1;
    CatSetEyeOn(cat, 0);
    CatSetEyePos(cat, nullptr);
}

// FF3 0x001422e0
int CatIsHeld(s_char cat)
{
    s_char i;

    if (cat < 0)
    {
        for (i = 0; i < CAT_NUM; i++)
        {
            if (CatGetWrk(i)->run_act == 0)
                return 1;
        }
        return 0;
    }
    return CatGetWrk(cat)->run_act == 0;
}

// FF3 0x00142358
void CatSetFixPos(s_char cat, s_char no)
{
    CAT_WRK *w = CatGetWrk(cat);

    w->fix_pos = no;
    CatActReset(w, cat);
}

// FF3 0x001423a0
void CatSetPos(s_char cat, int area, float *pos, float rot)
{
    CAT_WRK *w = CatGetWrk(cat);

    w->area   = area;
    w->pos[0] = pos[0];
    w->pos[1] = pos[1];
    w->pos[2] = pos[2];
    w->pos[3] = pos[3];
    w->rot[1] = rot;
}

// FF3 0x001423f8
static void CatMdlMotionStart(s_char cat, s_char mot, s_char start)
{
    CharCatModel *m;

    CatGetWrk(cat)->mot = mot;
    m = CatGetMdl(cat);
    if (m != nullptr)
        m->StartAnime(mot, start, 0, 1, 0.0f, 1.0f);    // FF3: vtable +0x40
}

// FF3 0x00142488
static void CatMdl2MotionStart(s_char cat, s_char mot, s_char start)
{
    CharCatModel *m;

    CatGetWrk(cat)->mot2 = mot;
    m = CatGetMdl2(cat);
    if (m != nullptr)
        m->StartAnime(mot, start, 0, 1, 0.0f, 1.0f);    // FF3: vtable +0x40
}

// The body's anime is over; motions 0, 14 and 20 loop and always count as
// over (as does no model).
// FF3 0x00142518

int CatMotionIsEnd(s_char cat)
{
    CharCatModel *m = CatGetMdl(cat);
    s_char        mot;

    if (m != nullptr)
    {
        mot = CatGetWrk(cat)->mot;
        if (mot != 0 && mot != 14 && mot != 20)
            return m->IsAnimeEnd();
    }
    return 1;
}

// FF3 0x00142598
void CatSetEyeOn(s_char cat, int on)
{
    CharCatModel *m = CatGetMdl(cat);

    if (m == nullptr)
        return;
    m->ctrl->off = (on == 0) ? 1 : 0;
    CatGetWrk(cat)->eye_on = on;
}

// FF3 0x00142600
void CatSetEyePos(s_char cat, float *pos)
{
    CAT_WRK *w = CatGetWrk(cat);

    if (pos == nullptr)
    {
        w->eye_pos_on = 0;
        w->eye_pos[0] = 0.0f;
        w->eye_pos[1] = 0.0f;
        w->eye_pos[2] = 0.0f;
        w->eye_pos[3] = 0.0f;
        return;
    }
    w->eye_pos_on = 1;
    w->eye_pos[0] = pos[0];
    w->eye_pos[1] = pos[1];
    w->eye_pos[2] = pos[2];
    w->eye_pos[3] = pos[3];
}

// FF3 0x00142690
void CatSetMdlType(s_char cat, s_char type)
{
    CatGetWrk(cat)->type = type;
}

// FF3 0x001426c8
void CatSetAcsNum(s_char cat, u_char no)
{
    CatGetWrk(cat)->acs_no = no;
}

// FF3 0x00142700
s_char CatGetMdlType(s_char cat)
{
    return CatGetWrk(cat)->type;
}

// FF3 0x00142728
u_char CatGetAcsNum(s_char cat)
{
    return CatGetWrk(cat)->acs_no;
}

// The body's joint position (and its coordinate's x / y rotation); 0
// without a model.
// FF3 0x00142750

static int CatGetJointPos(s_char cat, int joint, float *out, float *rot)
{
    CharCatModel *m = CatGetMdl(cat);
    int           c;

    if (m == nullptr)
        return 0;
    c = m->GetJointCoord(joint);                            // FF3: vtable +0x34
    m->GetCoordPos(out, c, const_cast<float *>(cat_origin));    // FF3: vtable +0x2c
    if (rot != nullptr)
    {
        rot[0] = m->GetCoordRot(c, 0);
        rot[1] = m->GetCoordRot(c, 1);
        rot[3] = 0.0f;
        rot[2] = 0.0f;
    }
    return 1;
}

// FF3 0x00142820
static int CatGetHeadPos(s_char cat, float *out, float *rot)
{
    return CatGetJointPos(cat, 2, out, rot);
}

// FF3 0x00142848
void CatSetLabelLock(s_char cat, s_char label, int on)
{
    CatGetWrk(cat)->lock[label] = on;
}

// FF3 0x00142890
s_char CatGetLabel(s_char cat)
{
    return CatGetWrk(cat)->label;
}

// FF3 0x001428b8
void CatActRestart(s_char cat)
{
    CAT_WRK *w = CatGetWrk(cat);

    if (w->active != 0 && w->run_act != 0)
        CatActReset(w, cat);
}

// ──────────────────────────────────────────────────────────────────────
// Sub act 0, the event spot: while the player stands in the spot's
// rectangle, one try in five (then 300 s; else 20 s) plays the spot's
// motion and moves the cat to the spot's label.
// FF3 0x00142910

static void CatEvSpotInit(CAT_WRK *cat)
{
    cat->spot       = CatLabelGetSpot(cat->label);
    cat->spot_ready = 1;
    cat->spot_step  = 0;
    cat->spot_wait  = 0;
}

// FF3 0x00142950
static int CatEvSpotCheck(CAT_WRK *cat)
{
    if (cat->spot_ready == 0)
    {
        if (cat->spot_wait > 0)
        {
            cat->spot_wait--;
            return 0;
        }
        cat->spot_ready = 1;
    }
    if (CatEvSpotIsIn(cat->spot) == 0)
        return 0;
    if (GetRandValIByF(100) < 20)
    {
        cat->spot_ready = 0;
        cat->spot_wait  = (GetPALMode() == 0) ? 9000 : 7500;
        return 1;
    }
    cat->spot_ready = 0;
    cat->spot_wait  = (GetPALMode() == 0) ? 600 : 500;
    return 0;
}

// FF3 0x00142a08
static int CatEvSpotMain(CAT_WRK *cat)
{
    s_char label;

    if (cat->spot_step == 0)
    {
        CatMotionCall(cat->no, CatEvSpotGetMot(cat->spot), 5);
        cat->spot_step = 1;
    }
    else if (cat->spot_step != 1)
    {
        return 0;
    }
    if (CatMotionIsEnd(cat->no) == 0)
        return 0;
    label = CatEvSpotGetLabel(cat->spot);
    if (label == -1)
        return 0;
    CatSetPosLabel(cat->no, label);
    CatMotSetStart(cat->no, label, 0, 0, 0);
    cat->next_label = label;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sub act 1, the door look: the cat watches its door, or the player when
// near; now and then it replays its motion set. The player-camera event
// camera covers rectangle 0xbc1. When the player opens the door
// (CatDoorFollowStart) it follows through.
// FF3 0x00142ac0

static void CatLookWaitSet(CAT_WRK *cat)
{
    int n = (GetPALMode() == 0) ? 120 : 100;

    cat->look_wait = (short)(GetRandValIByF(n) + n);
}

// FF3 0x00142b18
static int CatLookWaitStep(CAT_WRK *cat)
{
    if (cat->look_wait > 0)
    {
        cat->look_wait--;
        return 0;
    }
    return 1;
}

// FF3 0x00142b48
static void CatLookWait2Set(CAT_WRK *cat)
{
    cat->look_wait2 = (GetPALMode() == 0) ? 240 : 200;
}

// Every 8 s a coin toss.
// FF3 0x00142b80

static int CatLookWait2Step(CAT_WRK *cat)
{
    if (cat->look_wait2 < 1)
    {
        cat->look_wait2 = (GetPALMode() == 0) ? 240 : 200;
        return GetRandValIByF(100) > 49;
    }
    cat->look_wait2--;
    return 0;
}

// FF3 0x00142be8
static void CatLookDoor(CAT_WRK *cat)
{
    sceVu0FVECTOR v;

    CatLookWaitSet(cat);
    CatLookWait2Set(cat);
    CatDoorGetPos(cat->door, v);
    CatSetEyePos(cat->no, v);
    CatSetEyeOn(cat->no, 1);
    cat->eye = 1;
}

// The eye target by the player's distance (40): near, the player (the
// motion may play once look_wait ran out); far, the door (when the door is
// in sight and the coin toss wins); 1 when the motion should play.
// FF3 0x00142c40

static int CatLookCheck(CAT_WRK *cat)
{
    float         dist = 40.0f;
    sceVu0FVECTOR v;
    int           ret = 0;
    int           waited, toss;

    waited = CatLookWaitStep(cat);
    toss   = CatLookWait2Step(cat);
    if (CatPlyrDistCheck(dist, cat->no, 2) != 0)
    {
        if (cat->eye != 2)
        {
            cat->eye = 2;
            CatSetEyePos(cat->no, nullptr);
        }
        if (cat->eye == 2 && waited != 0)
            ret = 1;
    }
    else if (CatPlyrDistCheck(dist, cat->no, 0) != 0)
    {
        CatDoorGetPos(cat->door, v);
        if (cat->eye != 1)
        {
            cat->eye = 1;
            CatSetEyePos(cat->no, v);
        }
        if (cat->eye == 1 && (CatSightCheck(dist, 0.17453292f, cat->no, v, nullptr) & 1) != 0 && toss != 0)
            ret = 1;
    }
    else
    {
        if ((CatPlyrSightCheck(dist, 0.785398066f, cat->no) & 1) == 0)
        {
            if (cat->eye == 0)
                goto out;
            cat->eye = 0;
        }
        else
        {
            if (cat->eye == 2)
                goto out;
            cat->eye = 2;
        }
        CatSetEyePos(cat->no, nullptr);
    }
  out:
    if (ret != 0)
        CatLookWaitSet(cat);
    return ret;
}

// FF3 0x00142e10
static int CatLookMot(CAT_WRK *cat)
{
    if (CatMotionIsEnd(cat->no) == 0)
        return 0;
    CatMotSetStart(cat->no, cat->label, 0, 5, 0);
    return 1;
}

// FF3 0x00142e60
static void CatLookStart(CAT_WRK *cat)
{
    CatLookDoor(cat);
    cat->look_step = 0;
}

// FF3 0x00142e88
static void CatLookMain(CAT_WRK *cat)
{
    s_char s = cat->look_step;

    if (s == 0)
    {
        if (CatLookCheck(cat) != 0)
        {
            s = 1;
            cat->look_step = 1;
        }
        else
        {
            s = cat->look_step;
        }
    }
    if (s == 1 && CatLookMot(cat) != 0)
        cat->look_step = 0;
}

// FF3 0x00142f00
static void CatEvCamClear(CAT_WRK *cat)
{
    cat->evcam = 0;
}

// FF3 0x00142f08
static void CatEvCamCheck(CAT_WRK *cat)
{
    PLYR_WRK *p = ingame_wrk.plyr_wrk;

    if (MrecIsInEvent(p->pos, 0xbc1, p->floor) == 0)
    {
        if (cat->evcam != 0)
        {
            PlyrCameraEvCamEnd();
            cat->evcam = 0;
        }
    }
    else if (cat->evcam == 0)
    {
        PlyrCameraEvCamSet(0.83f, 0.0f, cat_evcam_pos, cat_evcam_tgt);
        cat->evcam = 1;
    }
}

// FF3 0x00142f98
static void CatDoorLookInit(CAT_WRK *cat) {}

// FF3 0x00142fa0
static int CatDoorLookCheck(CAT_WRK *cat)
{
    cat->door      = CatLabelGetSpot(cat->label);
    cat->door_stat = -1;
    CatLookStart(cat);
    CatEvCamClear(cat);
    return 1;
}

// The look until the player opens the door; then the go motion (0x18) and
// through to the door's label.
// FF3 0x00142fe8

static int CatDoorLookMain(CAT_WRK *cat)
{
    s_char label;

    switch (cat->door_stat)
    {
    case -1:
        CatLookMain(cat);
        CatEvCamCheck(cat);
        return 0;
    case 1:
        if (cat->door_wait > 0)
        {
            cat->door_wait--;
            return 0;
        }
        CatMotionCall(cat->no, 0x18, 0);
        cat->door_stat = 2;
        break;
    case 2:
        break;
    default:
        return 0;
    }
    if (CatMotionIsEnd(cat->no) == 0)
        return 0;
    LockTblDec(3);
    label = CatDoorGetLabel(cat->door);
    if (label == -1)
        return 0;
    CatSetPosLabel(cat->no, label);
    CatMotSetStart(cat->no, cat->label, 0, 0, 0);
    cat->next_label = label;
    return 1;
}

// init_Story_Door_Open: the door's from area; 1 (with the cat's camera)
// when cat 0 watches this door from label 4 and goes through with the
// player (motion 0x17, lock 3 until it is through).
// FF3 0x001430f0

int CatDoorFollowStart(short door, float *pos, float *tgt)
{
    CAT_WRK *w;

    CatAreaMoveSet(0, GetPlyrAreaNo());
    w = CatGetWrk(0);
    if (w->label != 4 || w->run_act == 0 || w->visible == 0 || w->loaded == 0 || w->active == 0)
    {
        w->door_stat = -1;
        return 0;
    }
    if (w->door_stat != -1)
        return 0;
    if (CatDoorGetCamera(w->door, door, pos, tgt) == 0)
        return 0;
    CatSetEyePos(w->no, nullptr);
    CatSetEyeOn(w->no, 0);
    LockTblInc(3);
    CatMotionCall(w->no, 0x17, 0);
    w->door_stat = 0;
    return 1;
}

// FF3 0x001431f8
void CatDoorFollowGo(void)
{
    CAT_WRK *w = CatGetWrk(0);

    if (w->door_stat == 0)
    {
        w->door_wait = 0;
        w->door_stat = 1;
    }
}

// FF3 0x00143228
void CatDoorFollowEnd(void)
{
    CatAreaMoveSet(1, GetPlyrAreaNo());
}

// ──────────────────────────────────────────────────────────────────────
// The free act: the cat at a label, running the label's move kind and sub
// act; it moves to another label when the player left area 0x30 by a door
// to anywhere but 0x32 (outside the dream side).
// FF3 0x00143250

static int CatAreaMoveIsReq(CAT_WRK *cat)
{
    if (CatLabelGetArea(cat->label) != 0x30)
        return 0;
    if (EvNowSynchroMode() != 0)
        return 0;
    return CatAreaMoveCheck(0x30, 0x32) != 0;
}

// FF3 0x001432a0
static void CatActFreeInit(CAT_WRK *cat)
{
    cat->step       = 0;
    cat->next_label = -1;
}

// A sub act's label, else a random free one (put there).
// FF3 0x001432b0

static int CatActFreeCheck(CAT_WRK *cat)
{
    s_char label;

    if (cat->next_label == -1)
    {
        label = CatLabelSelect(cat->no, EvNowSynchroMode());
        cat->label = label;
        CatSetPosLabel(cat->no, label);
        cat->step = 0;
    }
    else
    {
        cat->label      = cat->next_label;
        cat->next_label = -1;
        cat->step       = 0;
    }
    return 1;
}

// FF3 0x00143328
static int CatActFreeMain(CAT_WRK *cat)
{
    s_char s = cat->step;
    s_char k, label;

    if (s == 0)
    {
        cat->move = CatLabelGetMove(cat->label);
        cat->sub  = CatLabelGetSub(cat->label);
        cat_move_init[cat->move](cat);
        if (cat->sub != -1)
            cat_sub_init[cat->sub](cat);
        cat->move_stat = 0;
        cat->step      = 1;
        s              = 1;
        cat->sub_stat  = 0;
    }
    if (s != 1)
        return 0;

    if (cat->move_stat == 0)
    {
        if (cat_move_check[cat->move](cat) != 0)
        {
            cat->move_stat = 1;
            if (cat_move_main[cat->move](cat) != 0)
                cat->move_stat = 0;
        }
    }
    else if (cat->move_stat == 1)
    {
        if (cat_move_main[cat->move](cat) != 0)
            cat->move_stat = 0;
    }

    k = cat->sub;
    if (k != -1)
    {
        if (cat->sub_stat == 0)
        {
            if (cat_sub_check[k](cat) == 0)
                goto area;
            k = cat->sub;
            cat->sub_stat = 1;
        }
        else if (cat->sub_stat != 1)
        {
            goto area;
        }
        if (cat_sub_main[k](cat) != 0)
        {
            cat->sub_stat = 0;
            return 1;
        }
    }
  area:
    if (CatAreaMoveIsReq(cat) != 0)
    {
        label = CatLabelMoveSelect(cat->no, cat->label);
        if (cat->label != label)
        {
            cat->label = label;
            CatSetPosLabel(cat->no, label);
            cat->step = 0;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Stay (move kind 0): the label's motion sets by idle kind (0: 80, 1: 20),
// idle motions between, and the pet act.
// FF3 0x00143518

static s_char CatIdleKindSelect(void)
{
    u_short w[2] = { cat_idle_kind_weight[0], cat_idle_kind_weight[1] };
    short   sum = 0, r;
    s_char  i;

    for (i = 0; i < 2; i++)
        sum = (short)(sum + w[i]);
    r   = (short)GetRandValIByF(sum);
    sum = 0;
    for (i = 0; i < 2; i++)
    {
        sum = (short)(sum + w[i]);
        if (r < sum)
            return i;
    }
    return -1;
}

// FF3 0x001435d0
static void CatIdleInit(CAT_WRK *cat) {}

// FF3 0x001435d8
static int CatIdleCheck(CAT_WRK *cat)
{
    int on = CatIsActive(CatGetWrk(cat->no));

    if (on != 0)
    {
        cat->idle     = CatIdleKindSelect();
        cat->idle_cnt = CatIdleCountGet(cat->idle);
        CatMotSetStart(cat->no, cat->label, cat->idle, 0, 0);
        cat->idle_step = 0;
    }
    return on != 0;
}

// One frame of idling: the motion set; at its end a new kind may be drawn;
// with cont, an idle motion follows each loop wait and kind 1 returns after
// idle_cnt of them. Without cont, 1 once the set reached its loop.
// FF3 0x00143650

static int CatIdleMain(CAT_WRK *cat, int cont)
{
    s_char k;
    int    r;

    switch (cat->idle_step)
    {
    case 0:
        r = CatMotSetMain(cat->no, cat->label, cat->idle);
        if (r == 0)
            return 0;
        if (r == 3)
        {
            k = CatIdleKindSelect();
            if (cat->idle != k)
            {
                cat->idle     = k;
                cat->idle_cnt = CatIdleCountGet(k);
                CatMotSetStart(cat->no, cat->label, cat->idle, 5, 0);
            }
            return 0;
        }
        if (cont == 0)
            return 1;
        if (r == 1)
            return 0;
        cat->idle_step = 1;
        // fall through
    case 1:
        CatIdleMotStart(cat->no, cat->idle);
        cat->idle_step = 2;
        // fall through
    case 2:
        if (CatIdleMotMain(cat->no, cat->idle) == 0)
            return 0;
        cat->idle_step = 3;
        // fall through
    case 3:
        break;
    default:
        return 0;
    }

    if (cont == 0)
        return 1;
    k = cat->idle_cnt;
    if (k > 0)
    {
        k--;
        cat->idle_cnt = k;
    }
    if (k < 1 && cat->idle != 1)
    {
        cat->idle = 1;
        CatMotSetStart(cat->no, cat->label, 1, 5, 0);
    }
    cat->idle_step = 0;
    return 0;
}

// FF3 0x001437d0
static int CatIdleMainLoop(CAT_WRK *cat)
{
    return CatIdleMain(cat, 1);
}

// The cat's reaction while stroked (kind 1 waits for its set to loop
// first): 0 running, 1 over (the set resumes at its loop), 2 none for the
// kind.
// FF3 0x001437e8

static int CatPetReact(CAT_WRK *cat)
{
    s_char k;
    int    r;

    if (cat->react == 0)
    {
        k = cat->idle;
        if (k == 1)
        {
            if (CatIdleMain(cat, 0) == 0)
                return 0;
            k = cat->idle;
        }
        if (CatReactMotStart(cat->no, k) == 0)
            return 2;
        cat->react = 1;
    }
    r = CatReactMotMain(cat->no, cat->idle);
    if (r != 0)
    {
        cat->react = 0;
        CatMotSetStart(cat->no, cat->label, cat->idle, 5, 1);
    }
    return r != 0;
}

// The pet act's steps (player animes 0xa7..0xac; 0x40 = cross, 0x20 =
// circle in the pad's trigger mask). Each returns 1 to go on to pet_next.
// 0: the camera, kneel (0xa8).
// FF3 0x00143898

static int CatPetStep0(CAT_WRK *cat)
{
    CatIdleMain(cat, 0);
    CatViewCamSet(cat->label);
    SetEffects(6, 1, 0x10);
    SetPlyrAnime(0xa8, 0, 0xff, 0xff, 1, 0.0f);
    cat->pet_next = 1;
    return 1;
}

// 1: kneeling down; cross again stands up at once.
// FF3 0x00143900

static int CatPetStep1(CAT_WRK *cat)
{
    CatIdleMain(cat, 0);
    if (CheckPadTrgPlaySe(0x40, -1, 0) != 0)
        cat->pet_again = 1;
    if (CheckPlyrAnimeEnd(0xa8) == 0)
        return 0;
    if (cat->pet_again == 0)
    {
        LockTblDec(3);
        LockTblDec(0xc);
        SetPlyrAnime(0xa7, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 2;
    }
    else
    {
        cat->pet_again = 0;
        SetPlyrAnime(0xa9, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 6;
    }
    return 1;
}

// 2: crouched; cross stands up, circle strokes.
// FF3 0x001439c8

static int CatPetStep2(CAT_WRK *cat)
{
    CatIdleMain(cat, 0);
    if (CheckPadTrgPlaySe(0x40, -1, 0) != 0)
    {
        LockTblInc(3);
        LockTblInc(0xc);
        SetPlyrAnime(0xa9, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 6;
        return 1;
    }
    if (CheckPadTrgPlaySe(0x20, -1, 0) == 0)
        return 0;
    SetPlyrAnime(0xab, 0xff, 0xff, 0xff, 1, 0.0f);
    cat->pet_next = 3;
    return 1;
}

// 3: reaching out (0xab).
// FF3 0x00143a88

static int CatPetStep3(CAT_WRK *cat)
{
    CatIdleMain(cat, 0);
    if (CheckPadTrgPlaySe(0x40, -1, 0) != 0)
        cat->pet_again = 1;
    if (CheckPlyrAnimeEnd(0xab) == 0)
        return 0;
    if (cat->pet_again == 0)
    {
        SetPlyrAnime(0xaa, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->react    = 0;
        cat->pet_next = 4;
    }
    else
    {
        cat->pet_again = 0;
        SetPlyrAnime(0xac, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 5;
    }
    return 1;
}

// 4: stroking (0xaa) while the cat reacts; circle (or its end) withdraws.
// FF3 0x00143b48

static int CatPetStep4(CAT_WRK *cat)
{
    int r = CatPetReact(cat);

    if (r != 1)
    {
        if (r != 2)
            return 0;
        if (CheckPadTrgPlaySe(0x20, -1, 0) == 0)
            return 0;
    }
    SetPlyrAnime(0xac, 0xff, 0xff, 0xff, 1, 0.0f);
    cat->pet_next = 5;
    return 1;
}

// 5: withdrawing the hand (0xac).
// FF3 0x00143be0

static int CatPetStep5(CAT_WRK *cat)
{
    if (CheckPadTrgPlaySe(0x40, -1, 0) != 0)
        cat->pet_again = 1;
    if (CheckPlyrAnimeEnd(0xac) == 0)
        return 0;
    if (cat->pet_again == 0)
    {
        SetPlyrAnime(0xa7, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 2;
    }
    else
    {
        cat->pet_again = 0;
        LockTblInc(3);
        LockTblInc(0xc);
        SetPlyrAnime(0xa9, 0xff, 0xff, 0xff, 1, 0.0f);
        cat->pet_next = 6;
    }
    return 1;
}

// 6: standing up (0xa9).
// FF3 0x00143ca0

static int CatPetStep6(CAT_WRK *cat)
{
    if (CheckPlyrAnimeEnd(0xa9) == 0)
        return 0;
    SetPlyrAnime(0, 0xff, 0xff, 0xff, 1, 0.0f);
    cat->pet_next = 7;
    return 1;
}

// 7: the camera back; the act ends.
// FF3 0x00143d00

static int CatPetStep7(CAT_WRK *cat)
{
    LockTblDec(3);
    LockTblDec(0xc);
    ingame_wrk.plyr_wrk->_0e0 &= ~2;
    CatViewCamEnd();
    SetEffects(6, 1, 0x10);
    cat->pet_next = -1;
    return 1;
}

// No NPC talks, and NPC 0 is not fixed in area 0x32 with the cat.
// FF3 0x00143d68

static int CatPetCanStart(void)
{
    if (PlyrCameraIsTalking(0xff) != 0)
        return 0;
    if (PlyrCameraIsFixed(0) != 0 && GetPlyrCameraWrk(0)->area == 0x32 && CatGetWrk(0)->area == 0x32)
        return 0;
    return 1;
}

// FF3 0x00143de0
static void CatPetInit(CAT_WRK *cat) {}

// The pet act starts on OK in Story_Normal (phase 32) with the player free,
// within 30 of the cat and facing it (45 degrees).
// FF3 0x00143de8

static int CatPetCheck(CAT_WRK *cat)
{
    PLYR_WRK *p = ingame_wrk.plyr_wrk;

    if ((p->flags & 0x1c00000) != 0 || (p->flags2 & 0x3f800) != 0)
        return 0;
    if (IsLockTblCnt(3) != 0)
        return 0;
    if (GetGPhaseCtrl()->leaf->id != 0x20)
        return 0;
    if (CatPlyrDistCheck(30.0f, cat->no, 2) == 0)
        return 0;
    if ((CatPlyrSightCheck(30.0f, 0.785398066f, cat->no) & 2) == 0)
        return 0;
    if (CatPetCanStart() == 0)
        return 0;
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], -1, 0) == 0)
        return 0;
    cat->pet_hold  = 0;
    cat->pet_next  = -1;
    cat->pet_step  = 0;
    cat->pet_again = 0;
    LockTblInc(3);
    LockTblInc(0xc);
    p->_0e0 |= 2;
    return 1;
}

// One pet step; while the player is busy (flags 0x1c00000) it falls back
// to step 2 (after the cat's reaction). 1 once over.
// FF3 0x00143f40

static int CatPetMain(CAT_WRK *cat)
{
    s_char s = cat->pet_step;

    if (s < 0)
        return 1;
    if (s > 0)
    {
        if ((ingame_wrk.plyr_wrk->flags & 0x1c00000) != 0)
        {
            if (s == 4)
            {
                if (CatPetReact(cat) == 0)
                {
                    cat->pet_hold = 1;
                    return 0;
                }
                cat->pet_hold = 0;
            }
            cat->pet_step = 2;
            return 0;
        }
        if (cat->pet_hold != 0)
        {
            if (CatPetReact(cat) == 0)
                return 0;
            cat->pet_hold = 0;
            cat->pet_step = 2;
            s = 2;
        }
    }
    if (cat_pet_step[s](cat) != 0)
        cat->pet_step = cat->pet_next;
    return 0;
}

// FF3 0x00144030
static void CatStayInit(CAT_WRK *cat)
{
    s_char i;

    for (i = 0; i < 2; i++)
        cat_stay_init[i](cat);
    cat->stay_stat = -1;
}

// FF3 0x00144098
static int CatStayCheck(CAT_WRK *cat)
{
    int r = cat_stay_check[0](cat);

    if (r != 0)
        cat->stay_stat = 0;
    return r != 0;
}

// FF3 0x001440d8
static int CatStayMain(CAT_WRK *cat)
{
    if (cat->stay_stat < 0)
        return 0;
    if (cat->stay_stat == 0)
    {
        cat_stay_main[0](cat);
        if (cat_stay_check[1](cat) != 0)
            cat->stay_stat = 1;
    }
    else if (cat_stay_main[1](cat) != 0)
    {
        cat->stay_stat = 0;
        cat->idle_step = 0;
    }
    return 0;
}

// Loop (move kind 1): the label's motion set 0 for ever.
// FF3 0x00144158

static void CatLoopInit(CAT_WRK *cat) {}

// FF3 0x00144160
static int CatLoopCheck(CAT_WRK *cat)
{
    CatMotSetStart(cat->no, cat->label, 0, 0, 0);
    return 1;
}

// FF3 0x00144190
static int CatLoopMain(CAT_WRK *cat)
{
    CatMotSetMain(cat->no, cat->label, 0);
    return 0;
}

// FF3 0x001441b8 (FUN_001441b8) is compiler output: the atexit thunk of the
// GPhaseCtrl singleton CatPetCheck's inline getter instantiates,
// GPhaseCtrlTerm(0x41e468); implicit for the C++ static.
