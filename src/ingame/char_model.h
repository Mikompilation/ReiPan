/* ==========================================================================
 *  ingame/char_model.h
 *
 *  FF3's character model classes (C++, RTTI names; no FF2 counterpart):
 *  CharModel, a GameAnimeModel with a character number and a model type,
 *  the type's anime scripts, a watch control (the head turning to a target),
 *  the root-move data of its motions and a bounding box per character; and
 *  its five derivations -- CharPlayerModel (the player: flashlights, foot
 *  and arm IK, the face / hand shape motions), CharEnemyModel (a ghost:
 *  its bones' world matrices kept for its joints), CharNpcModel (a companion
 *  NPC), CharCatModel (the cats, two models each) -- and AnimeCodeInterpretorChar,
 *  the script handler whose two extra commands play the footstep and voice
 *  SEs. FF3 0x1441d8..0x1479b0, with the inline virtuals at 0x2ad470..0x2ad5b0.
 *
 *  The helper classes the models own (CharWatchControl, CharRmdData,
 *  CharPlayerShape and its statuses) are ingame/char_model_sub.c's
 *  (0x1479b0..): ingame/char_model_sub.h.
 * ======================================================================== */
#ifndef _INGAME_CHAR_MODEL_H
#define _INGAME_CHAR_MODEL_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FMATRIX, sceVu0FVECTOR
#include "graphics/game_anime_model.h"  // GameAnimeModel (and GameModel, GameMotionModel)
#include "graphics/graph3d/g3dLight.h"  // G3DLIGHT (CharPlayerModel)
#include "graphics/motion/motion.h"     // BMD_YAW_FUNC, MDL_MOT
#include "ingame/ingame_wrk.h"          // AnimeCodeInterpretorGame
#include "ingame/char_model_sub.h"     // CharWatchControl, CharRmdData, CharPlayerShape

struct ENE_WRK;                         // ingame/enemy/ene_main.h
struct PLYR_WRK;                        // ingame/plyr/player.h
struct PLYR_CAMERA_WRK;                 // ingame/photo/n_plyr_camera.h
struct CAT_WRK;                         // ingame/cat/cat_ctrl.h
struct GRA3DEMULATIONLIGHTDATACREATIONDATA; // graphics/graph3d/gra3dMisc.h

// A character's bounding box in its root joint's space (data 0x34fe20, one
// row per character; next >= 0: use that row instead -- every row without a
// box of its own falls back to 0; 0x34fdf0 is the one-row default for models
// without a root joint). A zero box means none (CharModel::HasBounds).
struct CHAR_BOUNDS                      /* 0x30 */
{
    /* 0x00 */ sceVu0FVECTOR min;
    /* 0x10 */ sceVu0FVECTOR max;
    /* 0x20 */ int           next;      // -1: this row
    /* 0x24 */ int           _24[3];    // 0
};
static_assert(sizeof(CHAR_BOUNDS) == 0x30, "CHAR_BOUNDS is 0x30 bytes");
static_assert(offsetof(CHAR_BOUNDS, next) == 0x20, "CHAR_BOUNDS.next");

// RTTI CharModel : GameAnimeModel, vtable 0x341710 (25 slots: GameAnimeModel's
// 17 with +0x00 Build, +0x08 / +0x0c the destructors, +0x14 IsDrawn, +0x34
// GetJointCoord, +0x3c CreateAnimeHandler and +0x40 StartAnime replaced, then
// the eight below). FF3 0x3c bytes; never made on its own -- each
// derivation's constructor runs it first.
// layout: host -- derived from GameAnimeModel; EE offsets in the comments.
class CharModel : public GameAnimeModel
{
public:
    CharModel();                        // FF3 0x00144ed0 (complete; base-object twin 0x00144f08)
    ~CharModel() override;              // FF3 0x00144fb0 (deleting 0x00145020; base 0x00144f40)

    void Build() override;              // FF3 0x001451a0: vtable +0x00
    int  IsDrawn() override;            // FF3 0x00145498: vtable +0x14, the box is in view
    int  GetJointCoord(int joint) override;     // FF3 0x00145528: vtable +0x34, joint -> coordinate, -1: none
    void CreateAnimeHandler() override; // FF3 0x00145550: vtable +0x3c, an AnimeCodeInterpretorChar
    // FF3 0x001450e0: vtable +0x40, the shape motions stopped, then GameAnimeModel's
    void StartAnime(int no, int start, int skip, int blend, float time, float speed) override;

    virtual void CreateWatchControl();  // FF3 0x00145220: vtable +0x44, a CharWatchControl at ctrl
    virtual void CreateShapes() {}      // FF3 0x002ad4d0: vtable +0x48, none
    virtual void CreateRmdData(void *data);     // FF3 0x00145280: vtable +0x4c, a CharRmdData at rmd
    virtual void StopShapes();          // FF3 0x00145170: vtable +0x50, every shape motion stopped
    // FF3 0x00145650: vtable +0x54, the box of b as min / max
    virtual void GetBounds(const CHAR_BOUNDS *b, float *min, float *max);
    // FF3 0x00145670: vtable +0x58, the box drawn with (CharPlayerModel widens it)
    virtual void GetBoundsDraw(const CHAR_BOUNDS *b, float *min, float *max);
    virtual int  CanRootMove() { return 1; }    // FF3 0x002ad4d8: vtable +0x5c
    virtual int  GetWalkStep() { return -1; }   // FF3 0x002ad4e0: vtable +0x60, the walk anime's step 0..3, -1: none

    int   Load(short chr, short type, short mdl, short mot, short shp);   // FF3 0x00145090: 1 when the requests were taken
    float GetRootMove(int axis, u_short attr);  // FF3 0x001452d0: the motion's root move on axis this step
    void  SetYawFunc(BMD_YAW_FUNC func, void *arg); // FF3 0x00145410: the bone motion's root-yaw callback
    float GetMotionYaw();               // FF3 0x00145458: the bone motion's root yaw to its end, 0 without one
    int   IsInView();                   // FF3 0x001454b8: 1 when the box is on screen (or there is none)
    int   CalcBBox(float (*bb)[4]);     // FF3 0x001456e0: the world box; 0: none
    int   CalcBBoxDefault(float (*bb)[4]);  // FF3 0x00145698: the default box at coordinate 0
    const CHAR_BOUNDS *GetBoundsDefault();  // FF3 0x001455b0
    const CHAR_BOUNDS *GetCharBounds(); // FF3 0x001455c8: this character's row
    int   HasBounds(const CHAR_BOUNDS *b);  // FF3 0x001455e0: 0 for a zero box
    // FF3 0x00145768: b through coordinate coord into the world box bb
    // (draw: the GetBoundsDraw box); 0 for a zero box.
    int   ApplyBounds(float (*bb)[4], const CHAR_BOUNDS *b, int coord, int draw);

    /* 0x30 */ short             chr;   // the character (the joint map and box row)
    /* 0x32 */ short             type;  // the model type (its anime scripts; 0x20 for the rest)
    /* 0x34 */ CharRmdData      *rmd;   // the root-move data (CreateRmdData), nullptr until built
    /* 0x38 */ CharWatchControl *ctrl;  // the watch control (CreateWatchControl), nullptr until built
};
static_assert(offsetof(CharModel, chr) == sizeof(GameAnimeModel), "CharModel.chr (EE +0x30)");
static_assert(offsetof(CharModel, type) == sizeof(GameAnimeModel) + 2, "CharModel.type (EE +0x32)");

// RTTI CharEnemyModel : CharModel, vtable 0x341690 (FF3 0x48 bytes,
// EneMdlCreate): a ghost. Each frame after the matrix it copies its bones'
// world matrices, which its joints are read from.
// layout: host -- derived from CharModel; EE offsets in the comments.
class CharEnemyModel : public CharModel
{
public:
    explicit CharEnemyModel(ENE_WRK *ene);  // FF3 0x00144948 (complete; base-object twin 0x00144900)
    ~CharEnemyModel() override;         // FF3 0x001449f0 (deleting 0x00144a50; base 0x00144990)

    void BuildPre() override;           // FF3 0x00144ab0: vtable +0x04, the enemy's effect slots let go of the model
    void SetWorldMatrix(sceVu0FMATRIX m) override;  // FF3 0x00144c40: vtable +0x18, then SaveBones
    void GetWorldMatrix(sceVu0FMATRIX out) override;    // FF3 0x00144ad0: vtable +0x1c, from the enemy's pos / rot
    void DrawMdlPre() override;         // FF3 0x00144dd8: vtable +0x20, the light set
    void GetCoordPos(float *out, int coord, float *in) override;   // FF3 0x00144d70: vtable +0x2c, through the saved matrix
    void GetCoordAxis(float *out, int coord, int axis) override;   // FF3 0x00144db0: vtable +0x30, a row of it

    void SaveBones();                   // FF3 0x00144c68: the bones' world matrices into bone (made the first time)

    /* 0x3c */ ENE_WRK       *ene;      // its enemy
    /* 0x40 */ sceVu0FMATRIX *bone;     // bone_num world matrices (system heap), nullptr until the first frame
    /* 0x44 */ int            bone_num; // the bone motion's bone count + 1
};
static_assert(offsetof(CharEnemyModel, ene) == sizeof(CharModel), "CharEnemyModel.ene (EE +0x3c)");

// RTTI CharNpcModel : CharModel, vtable 0x341780 (FF3 0x40 bytes,
// PlyrCameraMdlCreate, which inlines its constructor): a companion NPC.
// layout: host -- derived from CharModel; EE offsets in the comments.
class CharNpcModel : public CharModel
{
public:
    explicit CharNpcModel(PLYR_CAMERA_WRK *cam) : cam(cam) {}   // FF3: inlined in PlyrCameraMdlCreate 0x0021eeb8
    ~CharNpcModel() override {}         // FF3 0x002ad550 (deleting 0x002ad578)

    void GetWorldMatrix(sceVu0FMATRIX out) override;    // FF3 0x001458a0: vtable +0x1c, from the NPC's pos / rot
    void DrawMdlPre() override;         // FF3 0x001459b8: vtable +0x20, PlyrCameraSetLight
    void DrawSecond() override;         // FF3 0x00145a48: vtable +0x28, the shadow
    void CreateRmdData(void *data) override;    // FF3 0x00145840: vtable +0x4c, a CharPlayerRmdData
    void StopShapes() override {}       // FF3 0x002ad548: vtable +0x50, nothing

    /* 0x3c */ PLYR_CAMERA_WRK *cam;    // its NPC
};
static_assert(offsetof(CharNpcModel, cam) == sizeof(CharModel), "CharNpcModel.cam (EE +0x3c)");

// RTTI CharCatModel : CharModel, vtable 0x3415e0 (FF3 0x44 bytes,
// CatMdlCreate, which inlines its constructor): one of a cat's two
// models; part 1 hangs from part 0's root joint.
// layout: host -- derived from CharModel; EE offsets in the comments.
class CharCatModel : public CharModel
{
public:
    CharCatModel(CAT_WRK *cat, int part) : cat(cat), part(part) {}  // FF3: inlined in CatMdlCreate 0x00141c58
    ~CharCatModel() override {}         // FF3 0x002ad4e8 (deleting 0x002ad510)

    void GetWorldMatrix(sceVu0FMATRIX out) override;    // FF3 0x00144500: vtable +0x1c
    void DrawMdlPre() override;         // FF3 0x001446e8: vtable +0x20, the light set
    void CreateWatchControl() override; // FF3 0x001448a0: vtable +0x44, a CharCatWatchControl

    /* 0x3c */ CAT_WRK *cat;            // its cat
    /* 0x40 */ int      part;           // 0: the body, 1: the part on the body's root joint
};
static_assert(offsetof(CharCatModel, cat) == sizeof(CharModel), "CharCatModel.cat (EE +0x3c)");

// RTTI CharPlayerModel : CharModel, vtable 0x341800 (FF3 0x1c0 bytes,
// PlyrMdlCreate): the player. It carries the flashlights (FF2 kept them in
// plyr_wrk) that gra3dGenerateLightDataToChar folds into light sets, and
// the face / hand shape motions.
// layout: host -- derived from CharModel; EE offsets in the comments.
class CharPlayerModel : public CharModel
{
public:
    explicit CharPlayerModel(PLYR_WRK *plyr);   // FF3 0x00145c50 (complete; base-object twin 0x00145b00)
    ~CharPlayerModel() override;        // FF3 0x00145dd8 (deleting 0x00145e10; base 0x00145da0)

    void GetWorldMatrix(sceVu0FMATRIX out) override;    // FF3 0x00146390: vtable +0x1c, from the player's pos / rot
    void DrawMdlPre() override;         // FF3 0x00146038: vtable +0x20, the light set
    void DrawSecond() override;         // FF3 0x00147300: vtable +0x28, the shadow
    void CreateShapes() override;       // FF3 0x00145e50: vtable +0x48, the blink, face and hand shapes
    void CreateRmdData(void *data) override;    // FF3 0x00145fd8: vtable +0x4c, a CharPlayerRmdData
    void StopShapes() override;         // FF3 0x00147750: vtable +0x50, the costume's hidden meshes
    void GetBoundsDraw(const CHAR_BOUNDS *b, float *min, float *max) override;  // FF3 0x001464a8: vtable +0x58
    int  CanRootMove() override;        // FF3 0x00147938: vtable +0x5c, not during anime 0xad / 0xae
    int  GetWalkStep() override;        // FF3 0x00146640: vtable +0x60

    void ReleaseShapes();               // FF3 0x00145f60
    // FF3 0x001460b0: the room's light set for the player at pos, made as
    // data says (plus the projector spot, the lightning, the rim light).
    void SetLightData(float *pos, GRA3DEMULATIONLIGHTDATACREATIONDATA *data);
    GRA3DEMULATIONLIGHTDATACREATIONDATA *GetLightCreationData();    // FF3 0x00146258
    void FootIk();                      // FF3 0x001466e8: the feet on the floor (PlyrMdlMain)
    void ArmIk();                       // FF3 0x00146ac8: the lantern arm (PlyrMdlMain)
    int  IsLookAnime();                 // FF3 0x00146c90: anime 0xa1 / 0xa2
    void UpdateLight(int look);         // FF3 0x00146cb8: the flashlights for this frame
    void SetLight(int look);            // FF3 0x001472e8: UpdateLight (PlyrMdlMain)
    void CalcLightDir(float up, float dist, float pitch, float yaw, float *out);    // FF3 0x001473e8
    void CalcLightPos(float up, float dist, float pitch, float yaw, float *out);    // FF3 0x001474e8
    void SetIdleFrame();                // FF3 0x00147638: the camera idle's frame from the view pitch
    float GetIdleFrame();               // FF3 0x001476a0
    void UpdateShapes();                // FF3 0x00147868
    short GetShapeChr();                // FF3 0x001478c8: chr with the cat variants 3 / 4 as 0 / 1
    void SetDreamLight();               // FF3 0x001478e8: fl_on, no prelight, black fog
    void ClearDreamLight();             // FF3 0x00147918: fl_on off, the rooms prelit again

    /* 0x03c */ PLYR_WRK        *plyr;  // its player
    /* 0x040 */ CharPlayerShape *blink; // CharPlayerShapeMepati
    /* 0x044 */ CharPlayerShape *face;  // the default real / dream face
    /* 0x048 */ CharPlayerShape *hand;  // the light hand
    /* 0x04c */ int              _04c;  // not set
    // FF2 kept these three in plyr_wrk (fl, fl2, reflectionlight); FF3 in the
    // player model. gra3dGenerateLightDataToChar copies them into a light set
    // (Type INVALID_G3DLIGHTTYPE: off).
    /* 0x050 */ G3DLIGHT fl;            // the flashlight (a spot); also the light its shadow is cast with (MapDrawPlyrShadow)
    /* 0x0c0 */ G3DLIGHT fl2;           // the second flashlight (used when a point light)
    /* 0x130 */ G3DLIGHT reflectionlight;   // the self-reflection light
    /* 0x1a0 */ int      foot_on;       // FootIk has heights
    /* 0x1a4 */ int      foot_ik;       // 1 outside direction objects, 0 on them (PlyrMapDirAnimeCtrl): FootIk runs
    /* 0x1a8 */ float    foot_y[2];     // the eased floor heights under the left / right foot
    /* 0x1b0 */ float    light_scale;   // the map light's scale in the light set (0.55)
    /* 0x1b4 */ float    fl_scale;      // the flashlight's range scale (0.9)
    /* 0x1b8 */ int      fl_power;      // 0: the flashlight's intensity 1 (off), else the look's
    /* 0x1bc */ int      fl_on;         // the dream light (SetDreamLight): the flashlight is on regardless (MapLightIsPlayerLightOn)
};
static_assert(offsetof(CharPlayerModel, plyr) == sizeof(CharModel), "CharPlayerModel.plyr (EE +0x3c)");
static_assert(offsetof(CharPlayerModel, fl2) == offsetof(CharPlayerModel, fl) + 0x70, "CharPlayerModel.fl2 (EE +0xc0)");
static_assert(offsetof(CharPlayerModel, reflectionlight) == offsetof(CharPlayerModel, fl) + 0xe0, "CharPlayerModel.reflectionlight (EE +0x130)");
static_assert(offsetof(CharPlayerModel, foot_ik) == offsetof(CharPlayerModel, fl) + 0x154, "CharPlayerModel.foot_ik (EE +0x1a4)");
static_assert(offsetof(CharPlayerModel, fl_on) == offsetof(CharPlayerModel, fl) + 0x16c, "CharPlayerModel.fl_on (EE +0x1bc)");

// RTTI AnimeCodeInterpretorChar : AnimeCodeInterpretorGame, vtable 0x341570
// (FF3 0x10 bytes, CharModel::CreateAnimeHandler): command types 3 and 4
// go to the model -- type 3 plays a footstep (the floor's under joint 5 /
// 0x11) or a voice SE.
// layout: host -- derived from AnimeCodeInterpretorGame; EE offsets in the comments.
class AnimeCodeInterpretorChar : public AnimeCodeInterpretorGame
{
public:
    AnimeCodeInterpretorChar(MDL_MOT *mot, AnimeCodeManagerGame *scr, CharModel *owner)
        : AnimeCodeInterpretorGame(mot, scr), owner(owner) {}  // FF3: inlined in CharModel::CreateAnimeHandler
    ~AnimeCodeInterpretorChar() override {} // FF3 0x002ad470 (deleting 0x002ad480)
    void Command3(short arg) override;  // FF3 0x00144200: vtable +0x1c
    void Command4(short arg) override {}    // FF3 0x001443f0: vtable +0x20, nothing

    /* 0x0c */ CharModel *owner;
};
static_assert(offsetof(AnimeCodeInterpretorChar, owner) == sizeof(AnimeCodeInterpretorGame), "AnimeCodeInterpretorChar.owner (EE +0x0c)");

// data 0x2f5830: each character's coordinate per joint, -1: none
// (char_model_dat.inc; CharModel::GetJointCoord and SceneModel::GetJointCoord).
extern const short char_joint_tbl[47][29];

#endif /* _INGAME_CHAR_MODEL_H */
