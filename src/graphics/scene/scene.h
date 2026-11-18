/* ==========================================================================
 *  graphics/scene/scene.h
 *
 *  Scenes (FF2 scene.c): the scene table (0x332be8), the scene slot
 *  SCENE_CTRL and its models (scene.c loads a scene into a slot, sets it up
 *  and draws it; scene_dat.c plays it), the scene file's header, the
 *  man-model queries of the scene effects and the light changes.
 * ======================================================================== */
#ifndef _GRAPHICS_SCENE_SCENE_H
#define _GRAPHICS_SCENE_SCENE_H

#include <stddef.h>                     // offsetof

#include "graphics/scene/fod.h"         // FOD_CTRL, FOD_CAMERA_DATA
#include "sdk/libvu0.h"                 // sceVu0FVECTOR, sceVu0FMATRIX
#include "sdk/scetypes.h"

class AcsModel;                         // ingame/plyr/acs_model.h
class SceneModel;                       // graphics/scene/IngameScene.h
struct BORROW_BUF;                      // graphics/movie/movie.h
struct SCENE_CTRL;

#define SCENE_NUM       0x44            // rows of the scene table
#define SCENE_CTRL_NUM  2               // scene slots (scene_ctrl, bss 0x3f7d70)
#define SCENE_MDL_NUM   8               // rows of each model group

// One scene (data 0x332be8, 0x44 rows). Each scene has three files from
// SCENE0010M_PSS (0x97b) on: scene_no * 3 + 0x97b is the movie / scene file.
// layout: host -- name is a pointer (EE row 0xc: value +0, name +4, movie +8).
struct SCENE_DAT
{
    int         value;                  // 0x0: 100 in every row: the movie's volume (SceneDatGetValue -> StartMovie)
    const char *name;                   // 0x4: four digits, "0010" .. "9902"
    int         movie;                  // 0x8: nonzero: a PSS movie (phase 67), else an engine scene (66)
};

// The groups of a scene's models (SCN_MDL.kind, SceneModel::Load's kind).
enum
{
    SCN_MDL_MAN  = 0,                   // characters (man_mdl)
    SCN_MDL_FURN = 1,                   // map furniture (furn_mdl)
    SCN_MDL_DOOR = 2,                   // map doors (door_mdl)
    SCN_MDL_ITEM = 3,                   // items (item_mdl)
};

// One model of a scene (0x90 bytes; FF2 SCN_ANM_MDL 0x4c, rebuilt). The
// scene header lists each group's model names (SceneMdlNameSet); a map
// object's number is the name's digits ("d012" -> door 12), which the scene
// hides on the map while its own copy plays (SceneMapObjShow).
// layout: host -- sc, mdl, efct, mot and shp are 4 bytes each on the EE;
// the offsets before sc are the ROM's (asserted).
struct SCN_MDL
{
    // Man models, each frame (SceneMdlUpdate): joint 0x1b's and joint 3's
    // positions and the first on screen (its z orders the man models' draw).
    /* 0x00 */ sceVu0FVECTOR head;
    /* 0x10 */ sceVu0FVECTOR pos;       // SceneManModelPosGet copies it; the haze anchor
    /* 0x20 */ sceVu0IVECTOR scr;       // sceVu0RotTransPers of head
    // Man models: the enemy aura the draw (SceneDrawManMdl) runs while
    // aura_type is nonzero -- EneAuraDraw(EneAuraDatGet(0, aura_type), alpha
    // aura_alpha + aura_amp * sin(aura_wave), aura_phase), aura_phase
    // stepping by aura_speed and aura_wave by aura_wave_speed (degrees, both
    // wrapped at 360). SceneManModelAuraSet writes them (scene effect record 29).
    /* 0x30 */ u_char     aura_type;
    /* 0x31 */ u_char     aura_alpha;
    /* 0x32 */ u_char     aura_amp;
    /* 0x33 */ u_char     _33;
    /* 0x34 */ float      aura_speed;
    /* 0x38 */ float      aura_phase;
    /* 0x3c */ float      aura_wave_speed;
    /* 0x40 */ float      aura_wave;
    /* 0x44 */ SCENE_CTRL *sc;          // the slot (SceneCtrlInit)
    /* 0x48 */ SceneModel *mdl;         // operator new(0x80); deleted through its vtable (SceneObjModelClear / SceneManModelClear)
    // Man models: the accessory models (FF2 efct_addr), drawn with the man
    // model and deleted through their vtables: [0] the lantern (hdr_flg[1],
    // joint 7; SceneEffectLenzFlareExec takes the lens point from it), [1]
    // hdr_flg[0]'s accessory (joint 0x13), [3] the character's own
    // (PlyrGetAcs, joint 2); [2] stays empty.
    /* 0x4c */ AcsModel  *efct[4];
    /* 0x5c */ u_char     _5c[4];
    /* 0x60 */ void      *mot;          // the motion data (member i + 1 of the group's motion PK4)
    /* 0x64 */ void      *shp;          // man models: the shape data (the shape PK4's member of the same name)
    /* 0x68 */ u_char     _68[4];
    /* 0x6c */ u_int      mdl_alpha;    // the draw's alpha (0x80 opaque); bit 31: set by a scene model fade (SceneEffectFadeModelCtrlExec)
    /* 0x70 */ short      obj_no;       // the number in name (StrGetNumber)
    /* 0x72 */ u_char     _72[2];
    /* 0x74 */ char       name[8];      // the model's name in the scene header ("d012", "f016", ...); FodSetMyLight's prefix
    /* 0x7c */ char       name_end;     // 0: ends name
    /* 0x7d */ u_char     kind;         // SCN_MDL_* (SceneCtrlInit)
    /* 0x7e */ u_char     disp;         // 1 (SceneCtrlInit); man models draw only while set
    /* 0x7f */ s_char     add;          // man models: SceneModel::Load's costume offset (the header, SceneMdlAddSet)
    /* 0x80 */ s_char     chr;          // man models: the character (PlyrGetMdlChr), < 0 none (no accessories)
    /* 0x81 */ u_char     _81[0xf];
};
static_assert(offsetof(SCN_MDL, pos) == 0x10, "SCN_MDL.pos (EE +0x10)");
static_assert(offsetof(SCN_MDL, scr) == 0x20, "SCN_MDL.scr (EE +0x20)");
static_assert(offsetof(SCN_MDL, aura_type) == 0x30, "SCN_MDL.aura_type (EE +0x30)");
static_assert(offsetof(SCN_MDL, aura_alpha) == 0x31, "SCN_MDL.aura_alpha (EE +0x31)");
static_assert(offsetof(SCN_MDL, aura_amp) == 0x32, "SCN_MDL.aura_amp (EE +0x32)");
static_assert(offsetof(SCN_MDL, aura_speed) == 0x34, "SCN_MDL.aura_speed (EE +0x34)");
static_assert(offsetof(SCN_MDL, aura_phase) == 0x38, "SCN_MDL.aura_phase (EE +0x38)");
static_assert(offsetof(SCN_MDL, aura_wave_speed) == 0x3c, "SCN_MDL.aura_wave_speed (EE +0x3c)");
static_assert(offsetof(SCN_MDL, aura_wave) == 0x40, "SCN_MDL.aura_wave (EE +0x40)");
static_assert(offsetof(SCN_MDL, mdl) == offsetof(SCN_MDL, sc) + sizeof(void *), "SCN_MDL.mdl (EE +0x48)");
static_assert(offsetof(SCN_MDL, efct) == offsetof(SCN_MDL, mdl) + sizeof(void *), "SCN_MDL.efct (EE +0x4c)");
static_assert(offsetof(SCN_MDL, mot) >= offsetof(SCN_MDL, efct) + 4 * sizeof(void *) + 4, "SCN_MDL.mot (EE +0x60; the host aligns it)");
static_assert(offsetof(SCN_MDL, shp) == offsetof(SCN_MDL, mot) + sizeof(void *), "SCN_MDL.shp (EE +0x64)");
static_assert(offsetof(SCN_MDL, mdl_alpha) == offsetof(SCN_MDL, shp) + sizeof(void *) + 4, "SCN_MDL.mdl_alpha (EE +0x6c)");
static_assert(offsetof(SCN_MDL, obj_no) == offsetof(SCN_MDL, mdl_alpha) + 4, "SCN_MDL.obj_no (EE +0x70)");
static_assert(offsetof(SCN_MDL, name) == offsetof(SCN_MDL, obj_no) + 4, "SCN_MDL.name (EE +0x74)");
static_assert(offsetof(SCN_MDL, name_end) == offsetof(SCN_MDL, obj_no) + 0xc, "SCN_MDL.name_end (EE +0x7c)");
static_assert(offsetof(SCN_MDL, kind) == offsetof(SCN_MDL, obj_no) + 0xd, "SCN_MDL.kind (EE +0x7d)");
static_assert(offsetof(SCN_MDL, disp) == offsetof(SCN_MDL, obj_no) + 0xe, "SCN_MDL.disp (EE +0x7e)");
static_assert(offsetof(SCN_MDL, add) == offsetof(SCN_MDL, obj_no) + 0xf, "SCN_MDL.add (EE +0x7f)");
static_assert(offsetof(SCN_MDL, chr) == offsetof(SCN_MDL, obj_no) + 0x10, "SCN_MDL.chr (EE +0x80)");
static_assert(offsetof(SCN_MDL, _81) + 0xf - offsetof(SCN_MDL, mdl_alpha) == 0x90 - 0x6c, "SCN_MDL (EE 0x90 bytes)");

// The scene file's header (member 0 of its PK4; SceneGetHeaderData). The
// model names follow it: man, door, furniture and item groups, 8 chars each,
// each group padded to 16 bytes (SceneMdlNameSet); then one 4-byte entry per
// man model whose first byte is its add (SceneMdlAddSet).
struct SCENE_HDR                        /* 0x20 + the names */
{
    /* 0x00 */ u_int  _00;
    /* 0x04 */ char   room[6];          // the room's name ("rs101"; SceneRoomNameToNo)
    /* 0x0a */ char   sub_room[6];      // the second room drawn with it
    /* 0x10 */ short  _10;              // FF2 mirror_flg, not read
    /* 0x12 */ short  man_mdl_num;      // each count is read as its low byte
    /* 0x14 */ short  door_num;
    /* 0x16 */ short  furn_num;
    /* 0x18 */ short  item_num;
    /* 0x1a */ u_char nuno_flg;         // SCENE_CTRL.nuno_flg
    /* 0x1b */ u_char _1b;
    /* 0x1c */ u_char flg[4];           // SCENE_CTRL.hdr_flg
};
static_assert(sizeof(SCENE_HDR) == 0x20, "SCENE_HDR (0x20 bytes)");
static_assert(offsetof(SCENE_HDR, room) == 0x04, "SCENE_HDR.room");
static_assert(offsetof(SCENE_HDR, sub_room) == 0x0a, "SCENE_HDR.sub_room");
static_assert(offsetof(SCENE_HDR, man_mdl_num) == 0x12, "SCENE_HDR.man_mdl_num");
static_assert(offsetof(SCENE_HDR, door_num) == 0x14, "SCENE_HDR.door_num");
static_assert(offsetof(SCENE_HDR, furn_num) == 0x16, "SCENE_HDR.furn_num");
static_assert(offsetof(SCENE_HDR, item_num) == 0x18, "SCENE_HDR.item_num");
static_assert(offsetof(SCENE_HDR, nuno_flg) == 0x1a, "SCENE_HDR.nuno_flg");
static_assert(offsetof(SCENE_HDR, flg) == 0x1c, "SCENE_HDR.flg");

// The scene file's members (FF2 SCENE_FILE; SCENE_CTRL +0x27e0,
// SceneDataAnalyze). FF3's scene file is a PK4: 0 the header, 1 the camera
// FOD, 2 the light FOD, 3..7 PK4s of the man motions, man shapes and the
// furniture / door / item motions, each with a name table as member 0 and
// model i's data as member i + 1. There is no effect FOD (eff_fod 0); the
// light revision and effect script are the scene's next two files.
// layout: host -- pointers are 4 bytes on the EE (EE offsets in the comments).
struct SCENE_FILE
{
    /* 0x00 */ void      *pk4;          // the scene file (SCENE_CTRL.scn_data_addr)
    /* 0x04 */ int        file_num;     // its member count
    /* 0x08 */ SCENE_HDR *hdr;
    /* 0x0c */ u_int     *cam_fod;
    /* 0x10 */ u_int     *lit_fod;
    /* 0x14 */ u_int     *eff_fod;      // 0 in FF3
    /* 0x18 */ void      *man_mot;
    /* 0x1c */ void      *man_shp;
    /* 0x20 */ void      *furn_mot;
    /* 0x24 */ void      *door_mot;
    /* 0x28 */ void      *item_mot;
};
static_assert(offsetof(SCENE_FILE, file_num) == sizeof(void *), "SCENE_FILE.file_num (EE +0x04)");
static_assert(offsetof(SCENE_FILE, hdr) == 2 * sizeof(void *), "SCENE_FILE.hdr (EE +0x08)");
static_assert(offsetof(SCENE_FILE, man_mot) == 6 * sizeof(void *), "SCENE_FILE.man_mot (EE +0x18)");
static_assert(sizeof(SCENE_FILE) == 11 * sizeof(void *), "SCENE_FILE (EE 0x2c bytes)");

// The scene's fog (FF2 SCENE_FOG; SceneInitEnvironment's defaults, the
// light revision file's copy, gra3dSetFog / SetFogColor in SceneDraw).
struct SCENE_FOG                        /* 0x20 */
{
    /* 0x00 */ float min;
    /* 0x04 */ float max;
    /* 0x08 */ float near_z;
    /* 0x0c */ float far_z;
    /* 0x10 */ int   r;
    /* 0x14 */ int   g;
    /* 0x18 */ int   b;
    /* 0x1c */ int   pad;
};
static_assert(sizeof(SCENE_FOG) == 0x20, "SCENE_FOG (0x20 bytes)");
static_assert(offsetof(SCENE_FOG, far_z) == 0x0c, "SCENE_FOG.far_z");
static_assert(offsetof(SCENE_FOG, r) == 0x10, "SCENE_FOG.r");
static_assert(offsetof(SCENE_FOG, b) == 0x18, "SCENE_FOG.b");

// One record of the light revision file (the scene's file + 1; FF2's .slt):
// a light's serial and G3DLIGHT. The file is { count, pad[3] }, count
// records, then the fog and FOD_AMBIENT_MAX ambients whose w carries "AMB".
struct SCENE_LIGHT_REV                  /* 0x90 */
{
    /* 0x00 */ FOD_LIT_SERIAL serial;
    /* 0x20 */ G3DLIGHT       light;
};
static_assert(sizeof(SCENE_LIGHT_REV) == 0x90, "SCENE_LIGHT_REV (0x90 bytes)");
static_assert(offsetof(SCENE_LIGHT_REV, light) == 0x20, "SCENE_LIGHT_REV.light");

// One scene slot (0x2830 bytes, two at bss 0x3f7d70; FF2 SCENE_CTRL 0x212c,
// one per buffer). scene.c loads a scene into a free slot (SceneCtrlLoadReq:
// the file into a BORROW_BUF, its light revision and effect script into
// system heap blocks; then SceneDataLoadMain's load_step 2..5 and
// SceneRoomLoadMain's room_step) and draws it (SceneDraw); scene_dat.c plays
// it (ScenePlayStart / Main / End) and releases it (SceneCtrlRelease). The
// four model groups, fod_ctrl, the fog and the camera data keep FF2's
// order; the tail is FF3's.
// layout: host -- the models and pointers are 4 bytes on the EE; EE offsets
// in the comments.
struct SCENE_CTRL
{
    /* 0x0000 */ SCN_MDL     man_mdl[SCENE_MDL_NUM];    // man_mdl_num used
    /* 0x0480 */ SCN_MDL     furn_mdl[SCENE_MDL_NUM];   // furn_num used: map furniture (MapObjSetHide type 1)
    /* 0x0900 */ SCN_MDL     item_mdl[SCENE_MDL_NUM];   // item_num used
    /* 0x0d80 */ SCN_MDL     door_mdl[SCENE_MDL_NUM];   // door_num used: map doors (MapObjSetHide type 0)
    /* 0x1200 */ FOD_CTRL    fod_ctrl;
    /* 0x2710 */ SCENE_FOG   fog;
    /* 0x2730 */ FOD_CAMERA_DATA CameraData;    // the scene camera (FodGetFirstCam, blended by SceneRenewEnvironment)
    /* 0x2760 */ sceVu0FMATRIX world;   // the models' world matrix: the room's origin (SceneDraw)
    /* 0x27a0 */ s_char      man_mdl_num;
    /* 0x27a1 */ s_char      door_num;
    /* 0x27a2 */ s_char      furn_num;
    /* 0x27a3 */ s_char      item_num;
    /* 0x27a4 */ BORROW_BUF *scn_buf;           // the scene file's buffer (SceneCtrlLoadReq), deleted by SceneCtrlRelease
    /* 0x27a8 */ u_char     *scn_data_addr;     // scn_buf->adrs
    /* 0x27ac */ u_int      *light_rev;         // the light revision file (system heap, SceneFileLoadReq), freed by SceneCtrlRelease
    /* 0x27b0 */ void       *eff_data;          // the effect script (system heap; SceneEffectMain), freed by SceneCtrlRelease
    /* 0x27b4 */ short       scene_no;          // the slot's scene (0 in a free slot, which load_step 0 marks)
    /* 0x27b6 */ short       room_no;           // SceneRoomNoGet
    /* 0x27b8 */ short       sub_room_no;       // SceneSubRoomNoGet
    /* 0x27ba */ u_char      _27ba;
    /* 0x27bb */ u_char      fod_flg;           // 1: ScenePlayMain steps fod_ctrl (FodNextFrame)
    /* 0x27bc */ u_char      count_flg;         // 1 (SceneCtrlInit); FF2's name, no reader seen
    /* 0x27bd */ u_char      stream_play;       // the preloaded stream was started
    /* 0x27be */ u_char      init_flg;          // 1: the slot may play (ScenePlayStart / ScenePlayMain)
    /* 0x27bf */ u_char      nuno_flg;          // SceneRoomLoadMain's MapLoadChangeArea side (the header)
    /* 0x27c0 */ short       room_bak;          // the room before the scene (SceneRoomLoadMain), back by SceneRoomRestore
    /* 0x27c2 */ u_char      _27c2[2];
    /* 0x27c4 */ float       fNearZBak;         // the camera's clip before the scene, back by ScenePlayEnd
    /* 0x27c8 */ float       fFarZBak;
    /* 0x27cc */ float       fFovBak;           // its field of view
    /* 0x27d0 */ int         MonotoneEnableBak; // monochrome before the scene
    /* 0x27d4 */ int         AreaSideBak;       // MapAreaSideGet before the scene
    // The header's flags: [0] the current player's man model gets accessory
    // 1, [1] accessory 0 (the lantern) and its lens flare.
    /* 0x27d8 */ u_char      hdr_flg[4];
    /* 0x27dc */ int         stream_id;         // the scene stream's StreamAuto id (SceneStreamLoadMain)
    /* 0x27e0 */ SCENE_FILE  file;
    /* 0x280c */ int         load_step;         // SceneDataLoadMain: 0 free, 2..4 loading, 5 loaded
    /* 0x2810 */ int         room_step;         // SceneRoomLoadMain: 0 / 6 done, 1..5 loading the rooms
    /* 0x2814 */ int         stream_step;       // SceneStreamLoadMain: 0 none, 1 request, 2 preloading, 3 ready
    /* 0x2818 */ u_char      _2818[4];
    /* 0x281c */ int         load_id[3];        // the LoadReq ids of the scene's files (SceneFileLoadReq)
    /* 0x2828 */ int         load_num;          // load_id's count
    /* 0x282c */ u_char      _282c[4];
};
static_assert(offsetof(SCENE_CTRL, furn_mdl) == SCENE_MDL_NUM * sizeof(SCN_MDL), "SCENE_CTRL.furn_mdl (EE +0x480)");
static_assert(offsetof(SCENE_CTRL, door_mdl) == 3 * SCENE_MDL_NUM * sizeof(SCN_MDL), "SCENE_CTRL.door_mdl (EE +0xd80)");
static_assert(offsetof(SCENE_CTRL, fod_ctrl) == 4 * SCENE_MDL_NUM * sizeof(SCN_MDL), "SCENE_CTRL.fod_ctrl (EE +0x1200)");
static_assert(offsetof(SCENE_CTRL, CameraData) == offsetof(SCENE_CTRL, fog) + 0x20, "SCENE_CTRL.CameraData (EE +0x2730)");
static_assert(offsetof(SCENE_CTRL, world) == offsetof(SCENE_CTRL, fog) + 0x50, "SCENE_CTRL.world (EE +0x2760)");
static_assert(offsetof(SCENE_CTRL, man_mdl_num) == offsetof(SCENE_CTRL, fog) + 0x90, "SCENE_CTRL.man_mdl_num (EE +0x27a0)");
static_assert(offsetof(SCENE_CTRL, item_num) == offsetof(SCENE_CTRL, man_mdl_num) + 3, "SCENE_CTRL.item_num (EE +0x27a3)");
static_assert(offsetof(SCENE_CTRL, sub_room_no) == offsetof(SCENE_CTRL, scene_no) + 4, "SCENE_CTRL.sub_room_no (EE +0x27b8)");
static_assert(offsetof(SCENE_CTRL, count_flg) == offsetof(SCENE_CTRL, scene_no) + 8, "SCENE_CTRL.count_flg (EE +0x27bc)");
static_assert(offsetof(SCENE_CTRL, init_flg) == offsetof(SCENE_CTRL, scene_no) + 0xa, "SCENE_CTRL.init_flg (EE +0x27be)");
static_assert(offsetof(SCENE_CTRL, room_bak) == offsetof(SCENE_CTRL, scene_no) + 0xc, "SCENE_CTRL.room_bak (EE +0x27c0)");
static_assert(offsetof(SCENE_CTRL, fFovBak) == offsetof(SCENE_CTRL, scene_no) + 0x18, "SCENE_CTRL.fFovBak (EE +0x27cc)");
static_assert(offsetof(SCENE_CTRL, AreaSideBak) == offsetof(SCENE_CTRL, scene_no) + 0x20, "SCENE_CTRL.AreaSideBak (EE +0x27d4)");
static_assert(offsetof(SCENE_CTRL, hdr_flg) == offsetof(SCENE_CTRL, scene_no) + 0x24, "SCENE_CTRL.hdr_flg (EE +0x27d8)");
static_assert(offsetof(SCENE_CTRL, stream_id) == offsetof(SCENE_CTRL, scene_no) + 0x28, "SCENE_CTRL.stream_id (EE +0x27dc)");
static_assert(offsetof(SCENE_CTRL, load_step) == offsetof(SCENE_CTRL, file) + sizeof(SCENE_FILE), "SCENE_CTRL.load_step (EE +0x280c)");
static_assert(offsetof(SCENE_CTRL, stream_step) == offsetof(SCENE_CTRL, load_step) + 8, "SCENE_CTRL.stream_step (EE +0x2814)");
static_assert(offsetof(SCENE_CTRL, load_id) == offsetof(SCENE_CTRL, load_step) + 0x10, "SCENE_CTRL.load_id (EE +0x281c)");
static_assert(offsetof(SCENE_CTRL, load_num) == offsetof(SCENE_CTRL, load_step) + 0x1c, "SCENE_CTRL.load_num (EE +0x2828)");
static_assert(offsetof(SCENE_CTRL, _282c) + 4 - offsetof(SCENE_CTRL, load_step) == 0x2830 - 0x280c, "SCENE_CTRL (EE 0x2830 bytes)");

extern SCENE_CTRL scene_ctrl[SCENE_CTRL_NUM];   // bss 0x3f7d70 (defined in scene_dat.c)

void SceneWrkClear(void);               // FF3 0x00253210: scene_ctrl cleared (IngameInit)
const char *SceneDatGetName(int scene_no);  // FF3 0x002530e8: "SCENE NO ERROR" for scene_no >= 0x44
int  SceneDatGetMovie(int scene_no);    // FF3 0x00253118: 0 for scene_no >= 0x44
int  GetFileNoFromSceneNo(int scene_no);    // FF3 0x00253148: scene_no * 3 + SCENE0010M_PSS (that for >= 0x44)
int  SceneDecisionMovie(int scene_no);      // FF3 0x00253188: SceneDatGetMovie
int  SceneEffectDataFileNoGet(int scene_no);    // FF3 0x00253168: GetFileNoFromSceneNo + 2, the effect script
int  SceneDatGetValue(int scene_no);        // FF3 0x002531b8: the row's value (0 for scene_no >= 0x44)
void SceneDatSetValue(int scene_no, int value); // FF3 0x002531e8: nothing for scene_no >= 0x44 (SceneVolDataApply)

// The slot's room / sub room; -1 for nullptr.
int  SceneRoomNoGet(SCENE_CTRL *sc);        // FF3 0x00252948
int  SceneSubRoomNoGet(SCENE_CTRL *sc);     // FF3 0x00252960
// One step of the slot's data load (load_step): 1 loaded, 0 busy, -1 for nullptr.
int  SceneDataLoadMain(SCENE_CTRL *sc);     // FF3 0x002533c0
// One step of the slot's room load (room_step): 1 done, 0 busy, 0xff for nullptr.
u_char SceneRoomLoadMain(SCENE_CTRL *sc);   // FF3 0x002534e0
void SceneObjModelClear(SCENE_CTRL *sc);    // FF3 0x00251628: the item, furniture and door models deleted
void SceneManModelClear(SCENE_CTRL *sc);    // FF3 0x00251220: the man models and their effect objects deleted
void SceneDoorModelDBFree(SCENE_CTRL *sc);  // FF3 0x00251768: empty in FF3
// The rooms, the models, the FOD camera and lights set up for play (the
// FOD, the first camera, the models, the prelight, the scene's rooms drawn,
// the map objects hidden, SceneEffectInit(1)).
void SceneInitAfterModelLoad(SCENE_CTRL *sc, int mim_init); // FF3 0x00251a30
void SceneDraw(SCENE_CTRL *sc, int subtitle);   // FF3 0x002520d8: one frame of the scene (subtitle: OPTION_WRK.subtitle)

// The playing slot's FOD set to frame (FodSetFrame); nothing when no scene
// plays. Unreferenced in FF3.
void SceneFodSetFrame(u_int frame);     // FF3 0x00252a50
FOD_LIGHT *SceneFodLightPtrGet(void);   // FF3 0x00252a80: the playing slot's FOD lights, nullptr; unreferenced

// The playing slot's man models (scene_dat.c's ScenePlayCtrlGet), found by
// obj_no mdl_id: the index-th such model, the first for index < 0
// (SceneManModelIdToSceneModelNo). Each does nothing when there is none (or
// no model built).
// The position plus dist units along the line towards the camera.
void SceneGetModelPDeformPos(float dist, float *pos, int mdl_id, int index);  // FF3 0x00252a98
void SceneManModelPosGet(float *pos, int mdl_id, int index);    // FF3 0x00252b88: SCN_MDL.pos
void SceneManModelTexFrameSet(int mdl_id, int index, u_int frame);  // FF3 0x00252c28: GameModelSetTexFrame, frame <= 0x80
// SCN_MDL's aura fields (type, alpha, amp, speed, aura_3c).
void SceneManModelAuraSet(float speed, float f3c, int mdl_id, int index, u_char type, u_char alpha, u_char amp);   // FF3 0x00252ce0
// A FOD light of the playing scene: the lights of type (2 spot, 3 point)
// and number light_no on the model mdl_type / mdl_id get colour, power and
// (spot) the cone in degrees.
void SceneChangeLightParameter(float r, float g, float b, float power, float cone,
                               int type, int light_no, int mdl_type, int mdl_id);  // FF3 0x00252e50
// The playing scene's hand spot light (cone in degrees).
void SceneChangeHandSpotLightParameter(float r, float g, float b, float power, float cone);   // FF3 0x00253028

// The slot holding scene_no (< SCENE_NUM), nullptr if none.
SCENE_CTRL *SceneCtrlFind(int scene_no);        // FF3 0x00253238
// The slot of scene_no if loaded, else a free slot with the scene file
// requested into a BorrowBuf (load_step 2); nullptr when none is free or the
// file is missing.
SCENE_CTRL *SceneCtrlLoadReq(int scene_no);     // FF3 0x00253288
// room_step 0 / 6 -> 1 (the rooms load), 1; else room_step 6, 0; -1 for
// nullptr or before the data are in (load_step <= 2). play (MovieReqIsPlay,
// from one_Story_Scene) is passed but not read.
int  SceneRoomLoadReq(SCENE_CTRL *sc, int play);    // FF3 0x00253490
// stream_step 0 -> 1 (SceneStreamLoadMain starts the stream), 1; -1 for
// nullptr or a stream already requested.
int  SceneStreamLoadReq(SCENE_CTRL *sc);        // FF3 0x002535f8

#endif /* _GRAPHICS_SCENE_SCENE_H */
