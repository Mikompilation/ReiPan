/* ==========================================================================
 *  graphics/scene/IngameScene.h
 *
 *  In-game scenes, FF3 0x256da8..0x257ae0 (FF3 file in FF2 scn_test.o's link
 *  slot after scene_effect.c; named after FF2's IngameScene.c, which holds
 *  the same Story_Scene phases). Nothing of FF2's scene-test debug menu is
 *  left. Contents:
 *  - SceneModel, the model class of a scene's actors and objects (RTTI,
 *    vtable 0x3423e8): a GameMotionModel whose motion / shape data come with
 *    the scene file and whose world matrix the scene sets;
 *  - phases 66 Story_Scene (find or load the scene's slot, its data and
 *    rooms) and 89 Story_Scene_Main (stream, play, back to the map);
 *  - the scene man-model, room-name and scene-stream tables;
 *  - the sleep flags (bss 0x3fcf10, save block 26): (kind, id) pairs the
 *    events set (SET_SLEEP_FLG_) and clear (also DevelopFilmDevelop, kind 2);
 *    while one is set the world side 0 stays (ScnTestIngameMain,
 *    SleepFlgSideWake).
 * ======================================================================== */
#ifndef _GRAPHICS_SCENE_INGAMESCENE_H
#define _GRAPHICS_SCENE_INGAMESCENE_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FMATRIX
#include "graphics/game_model.h"        // GameMotionModel

// RTTI SceneModel : GameMotionModel, vtable 0x3423e8 (GameModel's 14 slots;
// +0x00 Build, +0x08 / +0x0c the destructors, +0x10 IsLoaded, +0x1c
// GetWorldMatrix and +0x34 GetJointCoord replaced). scene.c allocates it with
// operator new(0x80) for every model of a scene and Loads it with the
// SCN_MDL's number, group and data.
// layout: host -- derived from GameMotionModel; EE offsets in the comments.
class SceneModel : public GameMotionModel
{
public:
    SceneModel();                       // FF3 0x00256e60 (complete; base-object twin 0x00256e30)
    ~SceneModel() override;             // FF3 0x00256eb8 (deleting 0x00256ee0; base 0x00256e90)

    void Build() override;              // FF3 0x00257040: vtable +0x00, GameMotionModel's
    int  IsLoaded() override;           // FF3 0x00257028: vtable +0x10, GameModel's (the motion data come with the scene)
    void GetWorldMatrix(sceVu0FMATRIX out) override;    // FF3 0x00257188: vtable +0x1c, the matrix SetMatrix stored
    int  GetJointCoord(int joint) override;     // FF3 0x002571e8: vtable +0x34, char_joint_tbl of the man model's character

    // The model file of no in group kind (0 man, 1 furniture, 2 door, 3
    // item) requested, with the motion / shape data the scene holds.
    void  Load(short no, short kind, void *mot, void *shp, short add);  // FF3 0x00256f18
    // The model file of no in group kind: a man model in the player's
    // costume plus add, F000 / D000 / O000_PK4 + no; -1 for no < 0 or
    // another kind.
    short GetFileNo(short no, short kind, short add);   // FF3 0x00256f70
    void  PlayMotion(float frame, float speed);         // FF3 0x00257058: bone motion 0
    void  PlayShapeAll(float frame, float speed);       // FF3 0x00257090: every shape motion
    void  ResetCloth();                 // FF3 0x00257130: the cloth back to rest, the unit matrix
    void  SetMatrix(sceVu0FMATRIX m);   // FF3 0x002571b0: the world matrix (GetWorldMatrix)
    void  SetFrame(float frame);        // FF3 0x00257238: every motion's frame

    /* 0x30 */ sceVu0FMATRIX matrix;    // the world matrix
    /* 0x70 */ short         no;        // the model number in its group (Load)
    /* 0x72 */ u_char        _72[0xe];
};
static_assert(offsetof(SceneModel, matrix) % 16 == 0, "SceneModel.matrix is a quadword (EE +0x30)");
static_assert(offsetof(SceneModel, matrix) == ((sizeof(GameMotionModel) + 15) & ~(size_t)15),
              "SceneModel.matrix (EE +0x30, GameMotionModel 0x28 rounded to a quadword)");
static_assert(offsetof(SceneModel, no) == offsetof(SceneModel, matrix) + 0x40, "SceneModel.no (EE +0x70)");
static_assert(offsetof(SceneModel, _72) + 0xe - offsetof(SceneModel, matrix) == 0x80 - 0x30, "SceneModel (EE 0x80 bytes)");

// The model files of a scene's groups (SceneModel::GetFileNo).
short SceneManMdlFileNoGet(short no);   // FF3 0x00256da8: the man model's CH file (the table), -1: none
short SceneFurnMdlFileNoGet(short no);  // FF3 0x00256dd0: F000_PK4 + no
short SceneDoorMdlFileNoGet(short no);  // FF3 0x00256df0: D000_PK4 + no
short SceneItemMdlFileNoGet(short no);  // FF3 0x00256e10: O000_PK4 + no

// Phase 66 Story_Scene: the scene MovieReqPlay asked for (phase 39 sends
// engine scenes here) found or loaded into a slot, its data and rooms
// loaded, then phase 89 to play it (MovieReqIsPlay) or back.
int  init_Story_Scene(void);            // FF3 0x002574c8
int  one_Story_Scene(void);             // FF3 0x00257548
int  end_Story_Scene(void);             // FF3 0x00257540
void pre_Story_Scene(void);             // FF3 0x00257630
void after_Story_Scene(void);           // FF3 0x00257638

// Phase 89 Story_Scene_Main: the scene's stream, ScenePlayStart, play to the
// end, then the map back and MovieReqGetRetPhase.
int  init_Story_Scene_Main(void);       // FF3 0x00257268
int  one_Story_Scene_Main(void);        // FF3 0x002572e8
int  end_Story_Scene_Main(void);        // FF3 0x002572c0

// The scene man-model table (rodata 0x362b48): man model no -> character
// (char_joint_tbl row) / CH model file, -1: none.
int  SceneManMdlTblChrGet(int no);      // FF3 0x00257640
int  SceneManMdlTblFileGet(int no);     // FF3 0x00257688
// The room whose name starts name's ("rs101" 1 .. "rhp00" 59, rodata
// 0x362dc8), -1 for none or nullptr.
int  SceneRoomNameToNo(const char *name);   // FF3 0x002576d0
// The SCENE<no>_STR stream of a scene (table rodata 0x362fa8: scenes 2, 3,
// 6, 8, 9, 10, 12, 15..18, 33, 35 -> 0xa4f..0xa67), -1 when it has none;
// its header file follows it.
int  SceneStrFileGet(int scene_no);     // FF3 0x002577c0

void ScnTestIngameInit(void);           // FF3 0x00257810: the sleep flags cleared (IngameNewGame, IngameInit)
void ScnTestIngameEnd(void);            // FF3 0x00257828: the same (IngameEnd)
// Each frame (IngameMain): in world 1 (ingame_wrk.b03) on side 0 with no
// sleep flag set, the side change to 1 (NightChangeReq(1, 1)); its result.
int  ScnTestIngameMain(void);           // FF3 0x00257840

#define SLEEP_FLG_NUM   16              // (kind, id) pairs at bss 0x3fcf10

// GO_REAL_WORLD: on side 0 with no sleep flag set, MapAreaSideSet(1) and 1;
// else 0.
int  SleepFlgSideWake(void);            // FF3 0x002578c8
void SleepFlgClearAll(void);            // FF3 0x00257938
void SleepFlgSet(short kind, short id); // FF3 0x002579b8: SET_SLEEP_FLG_ 1; nothing if already set or full
int  SleepFlgClear(short kind, short id);   // FF3 0x00257a58: SET_SLEEP_FLG_ 0; its index, -1: was not set
int  SleepFlgSaveGet(void **addr);      // FF3 0x00257ac8: {sleep_flg (0x3fcf10), 0x40}, save block 26

#endif /* _GRAPHICS_SCENE_INGAMESCENE_H */
