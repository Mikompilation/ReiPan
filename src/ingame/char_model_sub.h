/* ==========================================================================
 *  ingame/char_model_sub.h
 *
 *  The helpers of FF3's character model classes (C++, RTTI names; no FF2
 *  counterpart; FF3 0x1479b0..0x149c98, right after char_model.c, with
 *  their inline virtuals at 0x2ad5b0..0x2adbf0):
 *  - CharPlayerShape and its states: the player model's face / hand shape
 *    motions and the blink (CharPlayerShapeMepati);
 *  - CharRmdData / CharPlayerRmdData: the root-move data of a model's motions;
 *  - the character draw table (bss 0x391650): the player, the enemies, the
 *    fly ghosts, the companion NPC and the cats put into the map's draw list
 *    (MapPut) as callbacks, so they are sorted and drawn with the room;
 *  - CharWatchControl: the head turning to a target.
 *  The file name is descriptive (the slot was guessed as FF2 alg_manage.c,
 *  whose two functions sit in the cine film menu after this run).
 * ======================================================================== */
#ifndef _INGAME_CHAR_MODEL_SUB_H
#define _INGAME_CHAR_MODEL_SUB_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

class CharModel;                        // ingame/char_model.h
class CharPlayerModel;                  // ingame/char_model.h
struct MAP_PUT;                         // ingame/map/MapPut.h

// The CharWatchControl's parameters (CharWatchControl::Update takes them each
// frame): the point the model looks at, then per joint group the divisor of
// the frame's step (a turn closes 1/n of its gap per frame; 10, 70, 600 for
// the default) and the angle limits in radians. Enemies: the ROM rows in
// enemy_mot_dat.inc (EneMdlWatchUpdate); [0..2] of prm become 10000 while
// paralysis 0 runs and 1 while it is invisible.
//   prm[0]  the eyes' divisor          prm[1]  the head's divisor
//   prm[2]  the body's divisor         prm[3] / prm[4]  the eyes' turn limits
//   prm[5] / prm[6]  the eyes' up / down limits      prm[7]  the head's turn limit
//   prm[8] / prm[9]  the head's down / up limits (the cat's: up / down)
//   prm[10] the body's turn limit      prm[11] not read here
struct CHAR_WATCH_PRM
{
    /* 0x00 */ sceVu0FVECTOR pos;
    /* 0x10 */ float         prm[12];
};
static_assert(sizeof(CHAR_WATCH_PRM) == 0x40, "CHAR_WATCH_PRM size");
static_assert(offsetof(CHAR_WATCH_PRM, prm) == 0x10, "CHAR_WATCH_PRM.prm");

struct SGDCOORDINATE;                   // graphics/graph3d/sgd_types.h

// RTTI CharWatchControl, vtable 0x341b48 (3 slots: +0x00 / +0x04 the
// destructors 0x2adbc0 / 0x2adbd0, +0x08 Update 0x148f30). FF3 0x28 bytes,
// made by CharModel::CreateWatchControl (the constructor is inlined there)
// at the model's +0x38. CHAR_MDL_CTRL was its stand-in name. It turns the
// body (joint 1), the head (joints 0xf and 2, half the turn each) and the
// eyes (joints 4 and 0x10) toward CHAR_WATCH_PRM.pos, easing each angle.
// layout: host -- the vptr and mdl are 4 bytes on the EE; EE offsets in the comments.
class CharWatchControl
{
public:
    explicit CharWatchControl(CharModel *mdl) : mdl(mdl) {}    // FF3: inlined in CharModel::CreateWatchControl 0x00145220
    virtual ~CharWatchControl() {}      // FF3 0x002adbc0 (deleting 0x002adbd0)

    // FF3 0x00148f30: vtable +0x08, the frame's look: each joint group's
    // target angles (zero while off, so the joints ease back), clamped to
    // prm's limits, eased into the state below and rotated into the joints'
    // matrices; the coordinates are recalculated.
    virtual void Update(CHAR_WATCH_PRM *prm);

    // FF3 0x00148eb0: 0 when rot (the target's angle off the model's
    // facing) is 0.4 pi or more, or the model has no joint 0x1b (the head):
    // the caller then feeds the watch with the control off. 1 while off.
    int IsInRange(CHAR_WATCH_PRM *prm, float rot);

    // FF3 0x001498e8: the target's angles from coord (ang[1] the turn off
    // its axis row, ang[2] the elevation; ang[0] / ang[3] kept).
    void CalcAngle(sceVu0FVECTOR ang, SGDCOORDINATE *coord, CHAR_WATCH_PRM *prm, int axis);
    // FF3 0x001495a8: turns coord's matrix by yaw (about y) and pitch
    // (about z) in its world frame (parent_lw: its parent's local-world).
    void TurnJoint(SGDCOORDINATE *coord, sceVu0FMATRIX parent_lw, float yaw, float pitch);

    /* 0x04 */ CharModel *mdl;          // its model
    /* 0x08 */ int        off = 0;      // 1: off (EneMdlCtrlSet mode 0, PlyrLookMain), 0: on
    /* 0x0c */ float      body_yaw = 0.0f;          // joint 1's eased turn
    /* 0x10 */ float      head_yaw = 0.0f;          // joints 0xf / 2 (the cat: 2), eased
    /* 0x14 */ float      head_pitch = 0.0f;
    /* 0x18 */ float      eye_yaw[2] = {};          // joints 4 / 0x10, eased
    /* 0x20 */ float      eye_pitch[2] = {};
};
static_assert(offsetof(CharWatchControl, mdl) == sizeof(void *), "CharWatchControl.mdl (EE +0x04)");
typedef CharWatchControl CHAR_MDL_CTRL;

// RTTI CharCatWatchControl : CharWatchControl, vtable 0x341b30 (+0x08: the
// watch of a cat, 0x149ad0). Made by CharCatModel::CreateWatchControl 0x001448a0.
// layout: host -- CharWatchControl with nothing added.
class CharCatWatchControl : public CharWatchControl
{
public:
    explicit CharCatWatchControl(CharModel *mdl) : CharWatchControl(mdl) {}  // FF3: inlined in 0x001448a0
    ~CharCatWatchControl() override {}  // FF3 0x002adb90 (deleting 0x002adba0)

    // FF3 0x00149ad0: vtable +0x08, the head (joint 2) only, its full turn.
    void Update(CHAR_WATCH_PRM *prm) override;
};

// FF3 0x002ad688: v limited to lo..hi (linkonce; savepoint.c calls it too).
float CharClampf(float v, float lo, float hi);

// ──────────────────────────────────────────────────────────────────────
// Root moves.
// ──────────────────────────────────────────────────────────────────────

// "No fixed move" (CharRmdData::GetFixedMove): GetMove reads the motion's
// keys instead. FF3 keeps 1.0 in three rdata words (0x36d9d0 / d4 / d8).
#define CHAR_RMD_NONE 1.0f

// A motion's root-move member of the root-move PK4 (Pk4GetData(data, motion
// number)): a key per frame after a 0x10 header (file data, EE layout).
struct CHAR_RMD_KEY                     /* 0x10 */
{
    /* 0x00 */ float _00;
    /* 0x04 */ float move[3];           // x, y, z: the root's move this frame
};
struct CHAR_RMD_MOT                     /* 0x10 + num keys */
{
    /* 0x00 */ int _00;
    /* 0x04 */ int num;                 // number of keys (frames)
    /* 0x08 */ int _08[2];
    // CHAR_RMD_KEY key[num] follows
};
static_assert(sizeof(CHAR_RMD_KEY) == 0x10, "CHAR_RMD_KEY size");
static_assert(sizeof(CHAR_RMD_MOT) == 0x10, "CHAR_RMD_MOT size (the keys start at +0x10)");

// RTTI CharRmdData, vtable 0x341b08 (+0x00 CalcMove 0x148700, +0x04
// GetFixedMove 0x2adb20, +0x08 / +0x0c the destructors 0x2adb30 / 0x2adb40):
// the root-move data of a model's motions (the motion PK4's member 1),
// which CharModel::GetRootMove reads. FF3 0x14 bytes, CharModel::CreateRmdData.
// layout: host -- the vptr and data are 4 bytes on the EE; EE offsets in the comments.
class CharRmdData
{
public:
    explicit CharRmdData(void *data);   // FF3 0x001486c8 (complete; base-object twin 0x00148690)

    // FF3 0x00148700: vtable +0x00, key (int)frame's move on axis; frames
    // past the end read the last key, 0 without keys.
    virtual float CalcMove(const CHAR_RMD_KEY *key, int num, int axis, float frame);
    // FF3 0x002adb20: vtable +0x04, CHAR_RMD_NONE (every move from the keys).
    virtual float GetFixedMove(short no, int axis) { return CHAR_RMD_NONE; }
    virtual ~CharRmdData() {}           // FF3 0x002adb30 (deleting 0x002adb40)

    float GetBlendMove(int axis, float step);   // FF3 0x00148748: blend_move[axis] * step
    float GetMove(short no, int axis, float frame, float step);    // FF3 0x00148760: motion no's move at frame

    /* 0x04 */ void *data;              // the root-move file
    /* 0x08 */ float blend_move[3];     // the last move GetMove read (a frame's), GetBlendMove's
};
static_assert(offsetof(CharRmdData, data) == sizeof(void *), "CharRmdData.data (EE +0x04)");

// RTTI CharPlayerRmdData : CharRmdData, vtable 0x341af0 (+0x04
// GetFixedMove 0x148820; the destructors 0x2adb60 / 0x2adb70). The player
// and the NPCs use it (CharPlayerModel / CharNpcModel::CreateRmdData inline
// the constructor).
// layout: host -- CharRmdData with nothing added.
class CharPlayerRmdData : public CharRmdData
{
public:
    explicit CharPlayerRmdData(void *data) : CharRmdData(data) {}  // FF3: inlined in 0x00145fd8 / 0x00145840
    ~CharPlayerRmdData() override {}    // FF3 0x002adb60 (deleting 0x002adb70)

    // FF3 0x00148820: the turn motions (four from each of the eight rows of
    // data 0x350840) move a fixed amount on y / z; CHAR_RMD_NONE otherwise.
    float GetFixedMove(short no, int axis) override;
};

// ──────────────────────────────────────────────────────────────────────
// The player's shape motions.
// ──────────────────────────────────────────────────────────────────────

class CharPlayerShape;

// The shape states' kinds (CharPlayerShape.kind), also the row of the shape
// number table (data 0x3507f0, 3 per row: by CharPlayerModel::GetShapeChr).
enum
{
    CHAR_SHAPE_BLINK        = 0,        // CharPlayerShapeMepati's row
    CHAR_SHAPE_FACE_REAL    = 1,        // the default face in the real world (ingame_wrk.b03 1)
    CHAR_SHAPE_FACE_DREAM   = 2,        // the default face otherwise
    CHAR_SHAPE_FACE_DAMAGE  = 3,        // player modes 1..4 (damage)
    CHAR_SHAPE_FACE_ANIME4  = 4,        // while anime 4 runs
    CHAR_SHAPE_FACE_ANIME37 = 5,        // while anime 0x25 runs
    CHAR_SHAPE_FACE_SPEAK   = 6,        // character 0 while an NPC speaks (PlyrCameraIsSpeaking)
    CHAR_SHAPE_HAND_DEFAULT = 7,
    CHAR_SHAPE_HAND_LIGHT   = 8,        // flags 0x8000 (the flashlight in hand)
    CHAR_SHAPE_HAND_DAMAGE  = 9,        // player modes 1..4; two shape motions (no, no + 1)
    CHAR_SHAPE_HAND_LADDER  = 10,       // player mode 5
    CHAR_SHAPE_HAND_DOOR    = 11,       // flags 0x8
    CHAR_SHAPE_HAND_CAMERA  = 12,       // flags 0xc00000 (going into / in the viewfinder)
    CHAR_SHAPE_NONE         = 0xe,      // after construction; the base state's kind
};

// RTTI CharPlayerShapeStatus, vtable 0x341880 (6 slots, in the order declared
// below): a state of a player shape (face, hand). The states are stateless
// singletons (function-local statics in their Instance functions; FF3
// registers their destructors with atexit, 0x1485b8..0x148678).
// layout: host -- a vptr only (4 bytes on the EE); so are the derived states below.
class CharPlayerShapeStatus
{
public:
    virtual void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) {}  // FF3 0x002ad6b0: vtable +0x00, nothing
    virtual void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) {}  // FF3 0x002ad6b8: vtable +0x04, nothing
    virtual int  GetNextKind(short chr, short anime) { return CHAR_SHAPE_NONE; }   // FF3 0x002ad6c0: vtable +0x08
    virtual CharPlayerShapeStatus *GetStatus(int kind) { return nullptr; }      // FF3 0x002ad6c8: vtable +0x0c
    virtual ~CharPlayerShapeStatus() {} // FF3 0x002ad6d0 (deleting 0x002ad6e0)

    // FF3 0x00147bf0: the next kind for the model's character and anime,
    // and that kind's state for shape (CharPlayerShape::SetStatus).
    void Update(CharPlayerShape *shape);
};

// RTTI CharPlayerShapeStatusFace : CharPlayerShapeStatus, vtable 0x3419e0.
// layout: host -- a vptr only.
class CharPlayerShapeStatusFace : public CharPlayerShapeStatus
{
public:
    int  GetNextKind(short chr, short anime) override;          // FF3 0x00147c78: vtable +0x08
    CharPlayerShapeStatus *GetStatus(int kind) override;        // FF3 0x00147d00: vtable +0x0c
    ~CharPlayerShapeStatusFace() override {}    // FF3 0x002ad910 (deleting 0x002ad920)
};

// RTTI CharPlayerShapeStatusHand : CharPlayerShapeStatus, vtable 0x341960.
// layout: host -- a vptr only.
class CharPlayerShapeStatusHand : public CharPlayerShapeStatus
{
public:
    int  GetNextKind(short chr, short anime) override;          // FF3 0x00148060: vtable +0x08
    CharPlayerShapeStatus *GetStatus(int kind) override;        // FF3 0x001480c8: vtable +0x0c
    ~CharPlayerShapeStatusHand() override {}    // FF3 0x002ad9d0 (deleting 0x002ad9e0)
};

// The face states (singletons; vtables 0x3419c0 / 0x3419a0 / 0x341980).
// layout: host -- a vptr only (all the states below).
class CharPlayerShapeDefaultRealFace : public CharPlayerShapeStatusFace
{
public:
    static CharPlayerShapeDefaultRealFace *Instance();      // FF3 0x002ad5b0 (gp+0x718)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x00147d78
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x00147de8
    ~CharPlayerShapeDefaultRealFace() override {}           // FF3 0x002ad940 (deleting 0x002ad950)
};
// layout: host -- a vptr only.
class CharPlayerShapeDefaultDreamFace : public CharPlayerShapeStatusFace
{
public:
    static CharPlayerShapeDefaultDreamFace *Instance();     // FF3 0x002ad5f8 (gp+0x730)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x00147e38
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x00147ea8
    ~CharPlayerShapeDefaultDreamFace() override {}          // FF3 0x002ad970 (deleting 0x002ad980)
};
// layout: host -- a vptr only.
class CharPlayerShapeSpecialFace : public CharPlayerShapeStatusFace
{
public:
    static CharPlayerShapeSpecialFace *Instance();          // FF3 0x002ad700 (gp+0x778)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x00147ef8
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x00147f68
    ~CharPlayerShapeSpecialFace() override {}               // FF3 0x002ad9a0 (deleting 0x002ad9b0)
};

// The hand states (singletons; vtables 0x341940 / 0x341920 / 0x341900 /
// 0x3418e0 / 0x3418c0 / 0x3418a0). Default, ladder and door keep the base
// Enter / Leave (nothing: their table rows hold no shape).
// layout: host -- a vptr only.
class CharPlayerShapeDefaultHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeDefaultHand *Instance();          // FF3 0x002ad748 (gp+0x76c)
    ~CharPlayerShapeDefaultHand() override {}               // FF3 0x002ada00 (deleting 0x002ada10)
};
// layout: host -- a vptr only.
class CharPlayerShapeLightHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeLightHand *Instance();            // FF3 0x002ad640 (gp+0x71c)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x001481a8
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x00148218
    ~CharPlayerShapeLightHand() override {}                 // FF3 0x002ada30 (deleting 0x002ada40)
};
// layout: host -- a vptr only.
class CharPlayerShapeDamageHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeDamageHand *Instance();           // FF3 0x002ad790 (gp+0x770)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x001482f0
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x001483a8
    ~CharPlayerShapeDamageHand() override {}                // FF3 0x002ada60 (deleting 0x002ada70)
};
// layout: host -- a vptr only.
class CharPlayerShapeLadderHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeLadderHand *Instance();           // FF3 0x002ad7d8 (gp+0x774)
    ~CharPlayerShapeLadderHand() override {}                // FF3 0x002ada90 (deleting 0x002adaa0)
};
// layout: host -- a vptr only.
class CharPlayerShapeDoorHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeDoorHand *Instance();             // FF3 0x002ad820 (gp+0x77c)
    ~CharPlayerShapeDoorHand() override {}                  // FF3 0x002adac0 (deleting 0x002adad0)
};
// layout: host -- a vptr only.
class CharPlayerShapeCameraHand : public CharPlayerShapeStatusHand
{
public:
    static CharPlayerShapeCameraHand *Instance();           // FF3 0x002ad868 (gp+0x768)
    void Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev) override;   // FF3 0x001484d8
    void Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next) override;   // FF3 0x00148548
    ~CharPlayerShapeCameraHand() override {}                // FF3 0x002adaf0 (deleting 0x002adb00)
};

// RTTI CharPlayerShape, vtable 0x341a18 (+0x00 / +0x04 the destructors
// 0x2ad8b0 / 0x2ad8c0, +0x08 Update 0x147a48): one of the player model's
// shape-motion channels, driven by a state. FF3 0x14 bytes.
// layout: host -- the vptr and the pointers are 4 bytes on the EE; EE offsets in the comments.
class CharPlayerShape
{
public:
    explicit CharPlayerShape(CharPlayerModel *mdl); // FF3 0x001479d8 (complete; base-object twin 0x001479b0)
    virtual ~CharPlayerShape() {}       // FF3 0x002ad8b0 (deleting 0x002ad8c0)
    virtual void Update();              // FF3 0x00147a48: vtable +0x08, the state's Update

    // FF3 0x00147a00: the model's shape motion for kind (table data
    // 0x3507f0 by the shape character), -1: none.
    short GetShapeNo(int kind);

    // FF3 0x00147a78: to state st (kind its number); the old state's Leave,
    // then the Enter of st. Nothing when kind is the current one.
    void SetStatus(CharPlayerShapeStatus *st, int kind);

    /* 0x04 */ CharPlayerModel       *mdl;
    /* 0x08 */ CharPlayerShapeStatus *status;   // nullptr: none yet
    /* 0x0c */ int                    kind;     // CHAR_SHAPE_*; CHAR_SHAPE_NONE after construction
    /* 0x10 */ short                  cnt;      // CharPlayerShapeMepati: frames since the last blink (0..90)
};
static_assert(offsetof(CharPlayerShape, mdl) == sizeof(void *), "CharPlayerShape.mdl (EE +0x04)");

// RTTI CharPlayerShapeMepati : CharPlayerShape, vtable 0x341a00 (the blink;
// destructors 0x2ad8e0 / 0x2ad8f0). CharPlayerModel::CreateShapes inlines
// the constructor.
// layout: host -- CharPlayerShape with nothing added.
class CharPlayerShapeMepati : public CharPlayerShape
{
public:
    explicit CharPlayerShapeMepati(CharPlayerModel *mdl) : CharPlayerShape(mdl) {}  // FF3: inlined in 0x00145e50
    ~CharPlayerShapeMepati() override {}
    void Update() override;             // FF3 0x00147b00: vtable +0x08
};

// ──────────────────────────────────────────────────────────────────────
// The character draw table: 32 entries (bss 0x391650) keyed by their owner.
// Each holds a MapPut callback put (buffer 2000 + its index) on the owner
// model's coordinate 0, so MapPutDraw sorts and draws the character with the
// room; the callback skips the passes whose flag bit says so.
// ──────────────────────────────────────────────────────────────────────

struct PLYR_WRK;                        // ingame/plyr/player.h
struct ENE_WRK;                         // ingame/enemy/ene_main.h
struct PLYR_CAMERA_WRK;                 // ingame/photo/n_plyr_camera.h
struct CAT_WRK;                         // ingame/cat/cat_ctrl.h
struct FLY_MDL_WRK;                     // ingame/enemy/fly_ctrl.h

enum
{
    CHAR_DRAW_PLAYER      = 0,          // PLYR_WRK, PlyrMdlDraw; skipped by pass bit 0x1
    CHAR_DRAW_ENEMY       = 1,          // ENE_WRK, EnemyDrawOne; bit 0x2
    CHAR_DRAW_FLY         = 2,          // FLY_MDL_WRK, FlyDraw; bit 0x2
    CHAR_DRAW_PLYR_CAMERA = 3,          // PLYR_CAMERA_WRK (the companion NPC), PlyrCameraDraw; bit 0x800
    CHAR_DRAW_CAT         = 4,          // CAT_WRK, CatDraw; bit 0x1000
};

#define CHAR_DRAW_MAX      32
#define CHAR_DRAW_BUFF_TOP 2000         // MapPut buffer of entry 0

// One entry (0x10 on the EE).
// layout: host -- put and owner are EE pointers (4 bytes); EE offsets in the comments.
struct CHAR_DRAW_ENT
{
    /* 0x00 */ int      id;             // CHAR_DRAW_BUFF_TOP + index while used; 0 / -1 free
    /* 0x04 */ int      kind;           // CHAR_DRAW_*
    /* 0x08 */ MAP_PUT *put;            // its callback put
    /* 0x0c */ void    *owner;          // the work it draws
};
static_assert(offsetof(CHAR_DRAW_ENT, put) == 0x08, "CHAR_DRAW_ENT.put");

void     CharDrawInit(void);            // FF3 0x00148e78 (was AlgManageWrkInit): IngameInit, IngameNewGameReset
MAP_PUT *CharDrawGetPlayerPut(void);    // FF3 0x001488a0: the player's put (sdata gp-0x5b14), nullptr: none
CHAR_DRAW_ENT *CharDrawFind(void *owner);   // FF3 0x001488f0: owner's entry, nullptr: none
void     CharDrawDelete(void *owner);   // FF3 0x00148930: owner's put deleted, its entry freed
int      CharDrawSetEnemy(ENE_WRK *ene);    // FF3 0x00148a60: enemyIsReadyData; 0
int      CharDrawSetFly(FLY_MDL_WRK *fly);  // FF3 0x00148af0: the fly model start 0x1bf480; 0
int      CharDrawSetPlayer(PLYR_WRK *plyr); // FF3 0x00148c00: PlyrMdlIsReady (nullptr: the current player); 0
int      CharDrawSetPlyrCamera(PLYR_CAMERA_WRK *cam);   // FF3 0x00148d00: PlyrCameraUpdate once ready; 0
int      CharDrawSetCat(CAT_WRK *cat);  // FF3 0x00148de8: the cat model start 0x141a88; 0

#endif /* _INGAME_CHAR_MODEL_SUB_H */
