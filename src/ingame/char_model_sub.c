// ingame/char_model_sub.c -- the helpers of the character model classes
// (FF3 0x1479b0..0x149c98 and their inline virtuals 0x2ad5b0..0x2adbf0;
// FF3-only C++, no FF2 counterpart). See char_model_sub.h.

#include <math.h>                       // fabsf
#include <stdlib.h>                     // rand
#include <string.h>                     // memset

#include "common/reipan_stub.h"
#include "common/pk4.h"                 // Pk4GetData
#include "common/vu0math.h"             // NormalizeAngle, Vu0Acosf
#include "graphics/gs_reg.h"            // SetDefaultGsRegs
#include "graphics/graph3d/gra3dSGD.h"  // _SetPREVIOUSTRI2PRIM
#include "graphics/graph3d/gra3dSGDData.h"  // sgdCalcCoordinateMatrix, sgdClearCoordCalcFlg*
#include "graphics/graph3d/sgd_types.h" // SGDCOORDINATE
#include "graphics/mmanage.h"           // ModelManager (MDL_MOT)
#include "ingame/cat/cat_ctrl.h"        // CAT_WRK, CatDraw
#include "ingame/char_model.h"          // CharModel, CharPlayerModel
#include "ingame/char_model_sub.h"
#include "ingame/enemy/ene_main.h"      // ENE_WRK, EnemyDrawOne
#include "ingame/enemy/fly_ctrl.h"      // FLY_MDL_WRK, FlyDraw
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/map/MapPut.h"
#include "ingame/npc/npc_act.h"         // PlyrCameraIsSpeaking
#include "ingame/photo/n_plyr_camera.h" // PLYR_CAMERA_WRK, PlyrCameraDraw
#include "ingame/plyr/player.h"         // PLYR_WRK, PlyrGetCurWrk
#include "ingame/plyr/plyr_sub.h"       // PlyrMdlDraw

// ──────────────────────────────────────────────────────────────────────
// The player's shape motions.
// ──────────────────────────────────────────────────────────────────────

// The model's shape motion per state kind (CHAR_SHAPE_*), by
// CharPlayerModel::GetShapeChr (0 / 1 / 2); -1: none. FF3 data 0x3507f0.
static const short char_shape_no[13][3] =
{
    {  1,  1,  0 },                     // CHAR_SHAPE_BLINK
    { 24,  0, -1 },                     // CHAR_SHAPE_FACE_REAL
    {  7,  6, -1 },                     // CHAR_SHAPE_FACE_DREAM
    {  3,  2,  4 },                     // CHAR_SHAPE_FACE_DAMAGE
    {  4,  3,  5 },                     // CHAR_SHAPE_FACE_ANIME4
    { 25, 25, 26 },                     // CHAR_SHAPE_FACE_ANIME37
    {  6, -1, -1 },                     // CHAR_SHAPE_FACE_SPEAK
    { -1, -1, -1 },                     // CHAR_SHAPE_HAND_DEFAULT
    {  8,  7,  8 },                     // CHAR_SHAPE_HAND_LIGHT
    { 22, 21, 22 },                     // CHAR_SHAPE_HAND_DAMAGE (and no + 1)
    { -1, -1, -1 },                     // CHAR_SHAPE_HAND_LADDER
    { -1, -1, -1 },                     // CHAR_SHAPE_HAND_DOOR
    { 21, 20, 21 },                     // CHAR_SHAPE_HAND_CAMERA
};

// The shape motions play at speed 1 from frame 0 (FF3 reads 1.0 from its own
// rdata word at each call site, 0x36d9a0..0x36d9c8). A state leaving for a
// default one plays its motion backwards (stop_end 1, reverse 1) to undo it.
#define CHAR_SHAPE_SPEED 1.0f

// No state yet (kind 0xe).
// FF3 0x001479d8 (complete; base-object twin 0x001479b0)

CharPlayerShape::CharPlayerShape(CharPlayerModel *mdl)
{
    this->mdl = mdl;
    cnt       = 0;
    status    = nullptr;
    kind      = CHAR_SHAPE_NONE;
}

// FF3 0x00147a00

short CharPlayerShape::GetShapeNo(int kind)
{
    return char_shape_no[kind][mdl->GetShapeChr()];
}

// The state picks the next one.
// FF3 0x00147a48

void CharPlayerShape::Update()
{
    if (status != nullptr)
        status->Update(this);
}

// To state st: the old one's Leave (told the new one), then st's Enter
// (told the old one). Nothing when kind is already the current one.
// FF3 0x00147a78

void CharPlayerShape::SetStatus(CharPlayerShapeStatus *st, int kind)
{
    CharPlayerShapeStatus *old;

    if (this->kind == kind)
        return;

    old = status;
    if (old != nullptr)
        old->Leave(this, st);           // FF3: vtable +0x04
    this->kind = kind;
    status     = st;
    st->Enter(this, old);               // FF3: vtable +0x00
}

// The blink: after 90 frames, a 1 % chance a frame to play the blink shape
// (not while the player is in a damage mode), and the count starts again.
// FF3 0x00147b00

void CharPlayerShapeMepati::Update()
{
    short no;

    cnt++;
    if (cnt < 0)
        cnt = 0;
    else if (cnt > 90)
        cnt = 90;
    if (cnt < 90)
        return;

    if (rand() % 100 + 1 >= 2)
        return;
    if ((u_int)(ingame_wrk.plyr_wrk->mode - 1) < 4)     // PLYR_MODE_DMG1..4
        return;

    no = GetShapeNo(CHAR_SHAPE_BLINK);
    if (no != -1)
        mdl->mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);  // FF3: vtable +0x10
    cnt = 0;
}

// FF3 0x00147bf0

void CharPlayerShapeStatus::Update(CharPlayerShape *shape)
{
    short chr   = shape->mdl->GetShapeChr();
    short anime = shape->mdl->GetAnimeNo();
    int   kind  = GetNextKind(chr, anime);              // FF3: vtable +0x08

    shape->SetStatus(GetStatus(kind), kind);            // FF3: vtable +0x0c
}

// The face: anime 4, the damage modes, anime 0x25, an NPC speaking to
// character 0, else the default face of the world the game is in.
// FF3 0x00147c78

int CharPlayerShapeStatusFace::GetNextKind(short chr, short anime)
{
    if (anime == 4)
        return CHAR_SHAPE_FACE_ANIME4;
    if ((u_int)(ingame_wrk.plyr_wrk->mode - 1) < 4)     // PLYR_MODE_DMG1..4
        return CHAR_SHAPE_FACE_DAMAGE;
    if (anime == 0x25)
        return CHAR_SHAPE_FACE_ANIME37;
    if (chr == 0 && PlyrCameraIsSpeaking((u_char)-1) != 0)
        return CHAR_SHAPE_FACE_SPEAK;
    return (s_char)ingame_wrk.b03 == 1 ? CHAR_SHAPE_FACE_REAL : CHAR_SHAPE_FACE_DREAM;
}

// FF3 keeps each state's Instance in an sbss pointer of its own (gp-0xb68
// .. gp-0xb3c), filled on first use; the singletons are the same objects.
// FF3 0x00147d00
// callcheck: CharPlayerShapeDefaultDreamFaceInstance (CharPlayerShapeDefaultDreamFace::Instance)

CharPlayerShapeStatus *CharPlayerShapeStatusFace::GetStatus(int kind)
{
    if (kind == CHAR_SHAPE_FACE_REAL)
        return CharPlayerShapeDefaultRealFace::Instance();
    if (kind == CHAR_SHAPE_FACE_DREAM)
        return CharPlayerShapeDefaultDreamFace::Instance();
    return CharPlayerShapeSpecialFace::Instance();
}

// FF3 0x00147d78

void CharPlayerShapeDefaultRealFace::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    short no = shape->GetShapeNo(shape->kind);

    if (no != -1)
        shape->mdl->mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// FF3 0x00147de8

void CharPlayerShapeDefaultRealFace::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    short no = shape->GetShapeNo(shape->kind);

    if (no != -1)
        shape->mdl->mot->StopShape(no);                 // FF3: vtable +0x2c
}

// FF3 0x00147e38

void CharPlayerShapeDefaultDreamFace::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    short no = shape->GetShapeNo(shape->kind);

    if (no != -1)
        shape->mdl->mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// FF3 0x00147ea8

void CharPlayerShapeDefaultDreamFace::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    short no = shape->GetShapeNo(shape->kind);

    if (no != -1)
        shape->mdl->mot->StopShape(no);
}

// FF3 0x00147ef8

void CharPlayerShapeSpecialFace::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(shape->kind);

    if (no != -1)
        mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// Back to a default face: played backwards, except the damage face, which
// stops; to anything else it stops.
// FF3 0x00147f68
// callcheck: CharPlayerShapeDefaultDreamFaceInstance (CharPlayerShapeDefaultDreamFace::Instance)

void CharPlayerShapeSpecialFace::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(shape->kind);

    if (no == -1)
        return;
    if ((next == CharPlayerShapeDefaultRealFace::Instance()
         || next == CharPlayerShapeDefaultDreamFace::Instance())
        && shape->kind != CHAR_SHAPE_FACE_DAMAGE)
    {
        mot->PlayShape(no, 0, 1, 1, 0.0f, CHAR_SHAPE_SPEED);
        return;
    }
    mot->StopShape(no);
}

// The hand: the damage modes, the ladder, the door, the viewfinder, the
// flashlight in hand, else the default hand. chr and anime are not used.
// FF3 0x00148060

int CharPlayerShapeStatusHand::GetNextKind(short chr, short anime)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;

    if ((u_int)(plyr->mode - 1) < 4)                    // PLYR_MODE_DMG1..4
        return CHAR_SHAPE_HAND_DAMAGE;
    if (plyr->mode == PLYR_MODE_5)
        return CHAR_SHAPE_HAND_LADDER;
    if (plyr->flags & 0x8)
        return CHAR_SHAPE_HAND_DOOR;
    if (plyr->flags & (PLYR_FLG_400000 | PLYR_FLG_800000))
        return CHAR_SHAPE_HAND_CAMERA;
    return (plyr->flags & 0x8000) ? CHAR_SHAPE_HAND_LIGHT : CHAR_SHAPE_HAND_DEFAULT;
}

// FF3 0x001480c8 (a jump table at rdata 0x3507d8 for kinds 7..12)
// callcheck: CharPlayerShapeLightHandInstance (CharPlayerShapeLightHand::Instance)

CharPlayerShapeStatus *CharPlayerShapeStatusHand::GetStatus(int kind)
{
    switch (kind)
    {
    case CHAR_SHAPE_HAND_LIGHT:  return CharPlayerShapeLightHand::Instance();
    case CHAR_SHAPE_HAND_DAMAGE: return CharPlayerShapeDamageHand::Instance();
    case CHAR_SHAPE_HAND_LADDER: return CharPlayerShapeLadderHand::Instance();
    case CHAR_SHAPE_HAND_DOOR:   return CharPlayerShapeDoorHand::Instance();
    case CHAR_SHAPE_HAND_CAMERA: return CharPlayerShapeCameraHand::Instance();
    default:                     return CharPlayerShapeDefaultHand::Instance();
    }
}

// FF3 0x001481a8

void CharPlayerShapeLightHand::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_LIGHT);

    if (no != -1)
        mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// To the door or the camera hand: played backwards; else it stops.
// FF3 0x00148218

void CharPlayerShapeLightHand::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_LIGHT);

    if (no == -1)
        return;
    if (next == CharPlayerShapeDoorHand::Instance() || next == CharPlayerShapeCameraHand::Instance())
        mot->PlayShape(no, 0, 1, 1, 0.0f, CHAR_SHAPE_SPEED);
    else
        mot->StopShape(no);
}

// Two shape motions, no and no + 1.
// FF3 0x001482f0

void CharPlayerShapeDamageHand::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_DAMAGE);

    if (no == -1)
        return;
    mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
    mot->PlayShape(no + 1, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// To the default or the light hand: both played backwards; else both stop.
// FF3 0x001483a8
// callcheck: CharPlayerShapeLightHandInstance (CharPlayerShapeLightHand::Instance)

void CharPlayerShapeDamageHand::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_DAMAGE);

    if (no == -1)
        return;
    if (next == CharPlayerShapeDefaultHand::Instance() || next == CharPlayerShapeLightHand::Instance())
    {
        mot->PlayShape(no, 0, 1, 1, 0.0f, CHAR_SHAPE_SPEED);
        mot->PlayShape(no + 1, 0, 1, 1, 0.0f, CHAR_SHAPE_SPEED);
        return;
    }
    mot->StopShape(no);
    mot->StopShape(no + 1);
}

// FF3 0x001484d8

void CharPlayerShapeCameraHand::Enter(CharPlayerShape *shape, CharPlayerShapeStatus *prev)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_CAMERA);

    if (no != -1)
        mot->PlayShape(no, 0, 0, 0, 0.0f, CHAR_SHAPE_SPEED);
}

// Always played backwards.
// FF3 0x00148548

void CharPlayerShapeCameraHand::Leave(CharPlayerShape *shape, CharPlayerShapeStatus *next)
{
    MDL_MOT *mot = shape->mdl->mot;
    short    no  = shape->GetShapeNo(CHAR_SHAPE_HAND_CAMERA);

    if (no != -1)
        mot->PlayShape(no, 0, 1, 1, 0.0f, CHAR_SHAPE_SPEED);
}

// The state singletons: function-local statics (FF3 guards each with a
// byte and registers its destructor with atexit: 0x1485b8..0x148678, the
// destructors set the base vtable back).
// callcheck: atexit (the host C++ runtime destroys the statics; each below)

// FF3 0x002ad5b0
CharPlayerShapeDefaultRealFace *CharPlayerShapeDefaultRealFace::Instance()
{
    static CharPlayerShapeDefaultRealFace s;
    return &s;
}

// FF3 0x002ad5f8
// callcheck: atexit
CharPlayerShapeDefaultDreamFace *CharPlayerShapeDefaultDreamFace::Instance()
{
    static CharPlayerShapeDefaultDreamFace s;
    return &s;
}

// FF3 0x002ad640
// callcheck: atexit
CharPlayerShapeLightHand *CharPlayerShapeLightHand::Instance()
{
    static CharPlayerShapeLightHand s;
    return &s;
}

// FF3 0x002ad700
// callcheck: atexit
CharPlayerShapeSpecialFace *CharPlayerShapeSpecialFace::Instance()
{
    static CharPlayerShapeSpecialFace s;
    return &s;
}

// FF3 0x002ad748
// callcheck: atexit
CharPlayerShapeDefaultHand *CharPlayerShapeDefaultHand::Instance()
{
    static CharPlayerShapeDefaultHand s;
    return &s;
}

// FF3 0x002ad790
// callcheck: atexit
CharPlayerShapeDamageHand *CharPlayerShapeDamageHand::Instance()
{
    static CharPlayerShapeDamageHand s;
    return &s;
}

// FF3 0x002ad7d8
// callcheck: atexit
CharPlayerShapeLadderHand *CharPlayerShapeLadderHand::Instance()
{
    static CharPlayerShapeLadderHand s;
    return &s;
}

// FF3 0x002ad820
// callcheck: atexit
CharPlayerShapeDoorHand *CharPlayerShapeDoorHand::Instance()
{
    static CharPlayerShapeDoorHand s;
    return &s;
}

// FF3 0x002ad868
// callcheck: atexit
CharPlayerShapeCameraHand *CharPlayerShapeCameraHand::Instance()
{
    static CharPlayerShapeCameraHand s;
    return &s;
}

// ──────────────────────────────────────────────────────────────────────
// Root moves.
// ──────────────────────────────────────────────────────────────────────

// The root-move file, the blend move cleared (memset 0xc).
// FF3 0x001486c8 (complete; base-object twin 0x00148690)

CharRmdData::CharRmdData(void *data)
{
    this->data = data;
    memset(blend_move, 0, sizeof(blend_move));
}

// FF3 0x00148700

float CharRmdData::CalcMove(const CHAR_RMD_KEY *key, int num, int axis, float frame)
{
    if (num <= 0)
        return 0.0f;
    if ((float)num <= frame)
        frame = (float)(num - 1);
    return key[(int)frame].move[axis];
}

// The move on axis during a blend: the frame's move times the step.
// FF3 0x00148748

float CharRmdData::GetBlendMove(int axis, float step)
{
    return blend_move[axis] * step;
}

// Motion no's move on axis at frame (a fixed one, or the motion's key),
// kept for GetBlendMove, times step.
// FF3 0x00148760

float CharRmdData::GetMove(short no, int axis, float frame, float step)
{
    const CHAR_RMD_MOT *mot;
    float               move;

    move = GetFixedMove(no, axis);                      // FF3: vtable +0x04
    if (move == CHAR_RMD_NONE)
    {
        mot  = (const CHAR_RMD_MOT *)Pk4GetData(data, no);
        move = CalcMove((const CHAR_RMD_KEY *)(mot + 1), mot->num, axis, frame);   // FF3: vtable +0x00
    }
    blend_move[axis] = move;
    return move * step;
}

// The turn motions: motion numbers no..no + 3 of a row move y / z by its
// values (x keeps the keys). FF3 data 0x350840, 8 rows; a negative no
// would end the table.
// FF3 0x00148820

// layout: host -- the ROM rows (short no, pad, float y, float z) as a C table.
struct CHAR_RMD_FIXED
{
    short no;
    float move[2];                      // y, z
};
static const CHAR_RMD_FIXED char_rmd_fixed[8] =
{
    { 56, {  0.2191489f, 0.2191489f } },
    { 62, { -0.256f,     0.256f     } },
    { 68, {  0.2191489f, 0.2191489f } },
    { 74, { -0.256f,     0.256f     } },
    { 80, {  0.4291666f, 0.4291666f } },
    { 86, { -0.4681818f, 0.4681818f } },
    { 92, {  0.4291666f, 0.4291666f } },
    { 98, { -0.4681818f, 0.4681818f } },
};

float CharPlayerRmdData::GetFixedMove(short no, int axis)
{
    int i;

    if (axis == 0)
        return CHAR_RMD_NONE;
    for (i = 0; i < 8 && char_rmd_fixed[i].no >= 0; i++)
    {
        if (no >= char_rmd_fixed[i].no && no <= char_rmd_fixed[i].no + 3)
            return axis == 1 ? char_rmd_fixed[i].move[0] : char_rmd_fixed[i].move[1];
    }
    return CHAR_RMD_NONE;
}

// ──────────────────────────────────────────────────────────────────────
// The character draw table.
// ──────────────────────────────────────────────────────────────────────

static CHAR_DRAW_ENT char_draw_tbl[CHAR_DRAW_MAX];  // bss 0x391650
static MAP_PUT      *char_draw_plyr_put;            // sdata gp-0x5b14

// FF3 0x001488a0

MAP_PUT *CharDrawGetPlayerPut(void)
{
    return char_draw_plyr_put;
}

// A free entry, taken (id = its buffer), nullptr when all 32 are in use.
// FF3 0x001488a8

static CHAR_DRAW_ENT *CharDrawNew(void)
{
    int i;

    for (i = 0; i < CHAR_DRAW_MAX; i++)
    {
        CHAR_DRAW_ENT *ent = &char_draw_tbl[i];

        if (ent->id < 1)
        {
            ent->put   = nullptr;
            ent->id    = CHAR_DRAW_BUFF_TOP + i;
            ent->owner = nullptr;
            return ent;
        }
    }
    return nullptr;
}

// FF3 0x001488f0

CHAR_DRAW_ENT *CharDrawFind(void *owner)
{
    int i;

    for (i = 0; i < CHAR_DRAW_MAX; i++)
    {
        if (char_draw_tbl[i].id >= 1 && char_draw_tbl[i].owner == owner)
            return &char_draw_tbl[i];
    }
    return nullptr;
}

// FF3 0x00148930

void CharDrawDelete(void *owner)
{
    CHAR_DRAW_ENT *ent = CharDrawFind(owner);
    MAP_PUT       *put;

    if (ent == nullptr)
        return;
    MpaPutDeleteOneObj(ent->put);
    put        = ent->put;
    ent->id    = -1;
    ent->put   = nullptr;
    if (char_draw_plyr_put == put)
        char_draw_plyr_put = nullptr;
}

// The callbacks: the entry is the put's work, its owner the character.

// FF3 0x001489b0 (hidden in 0x148930's body in Ghidra until 2026-10-16)

static void CharDrawEnemyFunc(MAP_PUT *put, u_int flags)
{
    if (flags & 0x2)
        return;
    EnemyDrawOne((ENE_WRK *)((CHAR_DRAW_ENT *)MapPutGetWork(put))->owner, flags);
    SetDefaultGsRegs();
    _SetPREVIOUSTRI2PRIM(nullptr);
}

// FF3 0x00148a08

static void CharDrawFlyFunc(MAP_PUT *put, u_int flags)
{
    if (flags & 0x2)
        return;
    FlyDraw((FLY_MDL_WRK *)((CHAR_DRAW_ENT *)MapPutGetWork(put))->owner);
    SetDefaultGsRegs();
    _SetPREVIOUSTRI2PRIM(nullptr);
}

// Only the current player's entry draws.
// FF3 0x00148b80

static void CharDrawPlayerFunc(MAP_PUT *put, u_int flags)
{
    PLYR_WRK *plyr;

    if (flags & 0x1)
        return;
    plyr = (PLYR_WRK *)((CHAR_DRAW_ENT *)MapPutGetWork(put))->owner;
    if (plyr != PlyrGetCurWrk())
        return;
    PlyrMdlDraw(plyr, flags);
    SetDefaultGsRegs();
    _SetPREVIOUSTRI2PRIM(nullptr);
}

// FF3 0x00148ca8

static void CharDrawPlyrCameraFunc(MAP_PUT *put, u_int flags)
{
    if (flags & 0x800)
        return;
    PlyrCameraDraw((PLYR_CAMERA_WRK *)((CHAR_DRAW_ENT *)MapPutGetWork(put))->owner);
    SetDefaultGsRegs();
    _SetPREVIOUSTRI2PRIM(nullptr);
}

// FF3 0x00148d90

static void CharDrawCatFunc(MAP_PUT *put, u_int flags)
{
    if (flags & 0x1000)
        return;
    CatDraw((CAT_WRK *)((CHAR_DRAW_ENT *)MapPutGetWork(put))->owner);
    SetDefaultGsRegs();
    _SetPREVIOUSTRI2PRIM(nullptr);
}

// An entry for owner (nullptr when it has one): a callback put on mdl's
// coordinate 0 (its local-world matrix), with pos (MapPutSetFuncObj) for
// the enemies and the flies.
static CHAR_DRAW_ENT *CharDrawSet(void *owner, int kind, MAP_PUT_DRAW_FUNC func, float *pos, GameModel *mdl)
{
    CHAR_DRAW_ENT *ent;

    if (CharDrawFind(owner) != nullptr)
        return nullptr;
    ent = CharDrawNew();
    if (ent == nullptr)                 // PC: FF3 writes through the null entry when all 32 are taken
        return nullptr;
    if (pos != nullptr)
        ent->put = MapPutSetFuncObj(ent->id, func, pos, 0);
    else
        ent->put = MapPutSetFunc(ent->id, func, 0);
    MapPutSetMatrixPtr(ent->put, mdl->GetCoord(0)->matLocalWorld[0]);
    MapPutSetWork(ent->put, ent);
    ent->owner = owner;
    ent->kind  = kind;
    return ent;
}

// FF3 0x00148a60
// callcheck: CharDrawFind, CharDrawNew, GameModelGetCoord, MapPutSetFunc, MapPutSetFuncObj, MapPutSetMatrixPtr, MapPutSetWork (in CharDrawSet, the five setters' shared body)

int CharDrawSetEnemy(ENE_WRK *ene)
{
    CharDrawSet(ene, CHAR_DRAW_ENEMY, CharDrawEnemyFunc, ene->pos, ene->mdl);
    return 0;
}

// FF3 0x00148af0
// callcheck: CharDrawFind, CharDrawNew, GameModelGetCoord, MapPutSetFunc, MapPutSetFuncObj, MapPutSetMatrixPtr, MapPutSetWork (in CharDrawSet, the five setters' shared body)

int CharDrawSetFly(FLY_MDL_WRK *fly)
{
    CharDrawSet(fly, CHAR_DRAW_FLY, CharDrawFlyFunc, fly->npos, fly->mdl);
    return 0;
}

// The player's put is also kept for CharDrawGetPlayerPut.
// FF3 0x00148c00
// callcheck: CharDrawFind, CharDrawNew, GameModelGetCoord, MapPutSetFunc, MapPutSetFuncObj, MapPutSetMatrixPtr, MapPutSetWork (in CharDrawSet, the five setters' shared body)

int CharDrawSetPlayer(PLYR_WRK *plyr)
{
    CHAR_DRAW_ENT *ent;

    if (plyr == nullptr)
        plyr = PlyrGetCurWrk();
    ent = CharDrawSet(plyr, CHAR_DRAW_PLAYER, CharDrawPlayerFunc, nullptr, plyr->mdl);
    if (ent != nullptr)
        char_draw_plyr_put = ent->put;
    return 0;
}

// FF3 0x00148d00
// callcheck: CharDrawFind, CharDrawNew, GameModelGetCoord, MapPutSetFunc, MapPutSetFuncObj, MapPutSetMatrixPtr, MapPutSetWork (in CharDrawSet, the five setters' shared body)

int CharDrawSetPlyrCamera(PLYR_CAMERA_WRK *cam)
{
    CharDrawSet(cam, CHAR_DRAW_PLYR_CAMERA, CharDrawPlyrCameraFunc, nullptr, cam->mdl);
    return 0;
}

// FF3 0x00148de8
// callcheck: CharDrawFind, CharDrawNew, GameModelGetCoord, MapPutSetFunc, MapPutSetFuncObj, MapPutSetMatrixPtr, MapPutSetWork (in CharDrawSet, the five setters' shared body)

int CharDrawSetCat(CAT_WRK *cat)
{
    CharDrawSet(cat, CHAR_DRAW_CAT, CharDrawCatFunc, nullptr, cat->mdl);
    return 0;
}

// FF3 0x00148e78 (memset 0x200)

void CharDrawInit(void)
{
    memset(char_draw_tbl, 0, sizeof(char_draw_tbl));
    char_draw_plyr_put = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The watch control.
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00148eb0 (prm is not read)

int CharWatchControl::IsInRange(CHAR_WATCH_PRM *prm, float rot)
{
    if (off == 1)
        return 1;
    if (mdl->GetJointCoord(0x1b) < 0)                   // FF3: vtable +0x34
        return 0;
    return fabsf(rot) < 1.2566371f;                     // 0.4 pi (rdata 0x36d9e0)
}

// The target's angles seen from coord. ang[1]: the turn between the
// coordinate's axis row (0: x, 1: y) and the target, both flattened onto the
// xz plane, signed by the y of their cross product. ang[2]: the target's
// elevation (signed by its height; 0 when the cosine is over 1).
// FF3 0x001498e8 (this is not read; VU0 inline asm)

void CharWatchControl::CalcAngle(sceVu0FVECTOR ang, SGDCOORDINATE *coord, CHAR_WATCH_PRM *prm, int axis)
{
    sceVu0FVECTOR d;
    sceVu0FVECTOR a;
    sceVu0FVECTOR b;
    sceVu0FVECTOR c;
    float cs;

    sceVu0SubVector(d, prm->pos, coord->matLocalWorld[3]);
    sceVu0CopyVector(a, coord->matLocalWorld[axis]);
    sceVu0CopyVector(b, d);
    a[1] = 0.0f;
    b[1] = 0.0f;
    sceVu0Normalize(a, a);              // PC: a zero vector stays zero, as the VU's rsqrt gives
    sceVu0Normalize(b, b);
    ang[1] = Vu0Acosf(sceVu0InnerProduct(a, b));
    sceVu0OuterProduct(c, b, a);
    if (c[1] < 0.0f)
        ang[1] = -ang[1];

    a[0] = d[0];  a[1] = d[1];  a[2] = d[2];
    b[0] = d[0];  b[1] = 0.0f;  b[2] = d[2];
    sceVu0Normalize(a, a);
    sceVu0Normalize(b, b);
    cs = sceVu0InnerProduct(a, b);
    if (cs > 1.0f)                      // rdata 0x36d9e8
    {
        ang[2] = 0.0f;
        return;
    }
    ang[2] = Vu0Acosf(cs);
    if (d[1] < 0.0f)
        ang[2] = -ang[2];
}

// The joint's world rotation W (its matrix under parent_lw, translation
// dropped): matCoord becomes Rz(pitch) W Ry(yaw) W^T matCoord, in the
// SDK's sceVu0MulMatrix order.
// FF3 0x001495a8 (this is not read; VU0 inline asm, the transpose by pextlw / pcpyld)

void CharWatchControl::TurnJoint(SGDCOORDINATE *coord, sceVu0FMATRIX parent_lw, float yaw, float pitch)
{
    sceVu0FMATRIX unit;
    sceVu0FMATRIX lw;
    sceVu0FMATRIX inv;
    sceVu0FMATRIX rot;
    sceVu0FMATRIX m;

    sceVu0UnitMatrix(unit);             // data 0x3293c0
    sceVu0MulMatrix(lw, parent_lw, coord->matCoord);
    lw[3][0] = lw[3][1] = lw[3][2] = 0.0f;  // the zero vector at data 0x329530
    sceVu0TransposeMatrix(inv, lw);
    sceVu0RotMatrixY(rot, unit, NormalizeAngle(yaw));
    sceVu0MulMatrix(m, inv, rot);
    sceVu0MulMatrix(m, m, lw);
    sceVu0RotMatrixZ(rot, unit, NormalizeAngle(pitch));
    sceVu0MulMatrix(m, m, rot);
    sceVu0MulMatrix(coord->matCoord, coord->matCoord, m);
}

// The body (joint 1, about its x), then the head: joint 2's angles, half of
// the eased turn into joint 0xf and half into joint 2, then the eyes (joints
// 4 and 0x10, x axis rows): when their turns disagree in sign both take the
// mean. Each angle closes 1/prm of its gap per frame; while off the targets
// stay zero, so the joints ease back. A missing joint ends the chain there.
// FF3 0x00148f30

void CharWatchControl::Update(CHAR_WATCH_PRM *prm)
{
    int            on = off != 1;
    SGDCOORDINATE *coord = mdl->GetCoord(0);
    int            body = mdl->GetJointCoord(1);       // FF3: vtable +0x34
    int            head = mdl->GetJointCoord(0xf);
    int            neck = mdl->GetJointCoord(2);
    int            eye_no[2];
    float          lim[4];
    sceVu0FVECTOR  ang;
    sceVu0FVECTOR  eye[2];
    sceVu0FMATRIX  unit;
    sceVu0FMATRIX  rot;
    SGDCOORDINATE *c;
    int            i;

    eye_no[0] = mdl->GetJointCoord(4);
    eye_no[1] = mdl->GetJointCoord(0x10);
    if (body < 0)
        return;
    sceVu0UnitMatrix(unit);             // data 0x3293c0

    // the body
    ang[0] = ang[1] = ang[2] = ang[3] = 0.0f;   // the zero vector at data 0x329530
    if (on)
    {
        CalcAngle(ang, &coord[body], prm, 1);
        ang[1] = CharClampf(ang[1], -prm->prm[10], prm->prm[10]);  // FF3: inlined here
    }
    body_yaw += (ang[1] - body_yaw) / prm->prm[2];
    sceVu0RotMatrixX(rot, unit, NormalizeAngle(body_yaw));
    c = &coord[body];
    sceVu0MulMatrix(c->matCoord, c->matCoord, rot);
    if (neck < 0 || head < 0)
    {
        sgdClearCoordCalcFlgParents(c);
        sgdCalcCoordinateMatrix(c);
        return;
    }

    // the head
    c = &coord[neck];
    sgdClearCoordCalcFlgParents(c);
    sgdCalcCoordinateMatrix(c);
    ang[0] = ang[1] = ang[2] = ang[3] = 0.0f;
    if (on)
    {
        CalcAngle(ang, c, prm, 1);
        ang[1] = CharClampf(ang[1], -prm->prm[7], prm->prm[7]);
        ang[2] = CharClampf(ang[2], -prm->prm[8], prm->prm[9]);
    }
    head_yaw += (-ang[1] - head_yaw) / prm->prm[1];
    head_pitch += (ang[2] - head_pitch) / prm->prm[1];
    TurnJoint(&coord[head], SgdPtr<SGDCOORDINATE>(coord[head].pParent)->matLocalWorld,
              head_yaw * 0.5f, head_pitch * 0.5f);              // rdata 0x36d9e4
    TurnJoint(c, SgdPtr<SGDCOORDINATE>(c->pParent)->matLocalWorld,
              head_yaw * 0.5f, head_pitch * 0.5f);
    if (eye_no[0] < 0 || eye_no[1] < 0)
    {
        sgdClearCoordCalcFlgParents(c);
        sgdCalcCoordinateMatrix(c);
        return;
    }

    // the eyes; one eye's turn limits mirror the other's
    sgdClearCoordCalcFlgParents(&coord[eye_no[0]]);
    sgdCalcCoordinateMatrix(&coord[eye_no[0]]);
    sgdClearCoordCalcFlgCoord(&coord[eye_no[1]]);
    sgdCalcCoordinateMatrix(&coord[eye_no[1]]);
    lim[0] = prm->prm[4];               // eye 0: -prm[3]..prm[4]
    lim[1] = -prm->prm[3];
    lim[2] = prm->prm[3];               // eye 1: -prm[4]..prm[3]
    lim[3] = -prm->prm[4];
    for (i = 0; i < 2; i++)
    {
        eye[i][0] = eye[i][1] = eye[i][2] = eye[i][3] = 0.0f;
        if (on)
        {
            CalcAngle(eye[i], &coord[eye_no[i]], prm, 0);
            eye[i][1] = CharClampf(eye[i][1], lim[i * 2 + 1], lim[i * 2]);
            eye[i][2] = CharClampf(eye[i][2], -prm->prm[6], prm->prm[5]);
        }
    }
    if (eye[0][1] * eye[1][1] < 0.0f)
        eye[0][1] = eye[1][1] = (eye[0][1] + eye[1][1]) * 0.5f;  // rdata 0x36d9e4
    for (i = 0; i < 2; i++)
    {
        c = &coord[eye_no[i]];
        eye_yaw[i] += (eye[i][1] - eye_yaw[i]) / prm->prm[0];
        sceVu0RotMatrixY(rot, unit, NormalizeAngle(eye_yaw[i]));
        sceVu0MulMatrix(c->matCoord, c->matCoord, rot);
        eye_pitch[i] += (eye[i][2] - eye_pitch[i]) / prm->prm[0];
        sceVu0RotMatrixZ(rot, unit, NormalizeAngle(-eye_pitch[i]));
        sceVu0MulMatrix(c->matCoord, c->matCoord, rot);
    }
    sgdClearCoordCalcFlgCoord(&coord[eye_no[0]]);
    sgdClearCoordCalcFlgCoord(&coord[eye_no[1]]);
    sgdCalcCoordinateMatrix(&coord[eye_no[0]]);
    sgdCalcCoordinateMatrix(&coord[eye_no[1]]);
}

// A cat turns its head (joint 2) by the whole eased angle and recalculates
// its eyes (joints 4 / 0x10) without turning them. Its pitch limits read
// prm[9] / prm[8] the other way round from CharWatchControl::Update.
// FF3 0x00149ad0 (no joint check: FF3 indexes joint 2 as returned)

void CharCatWatchControl::Update(CHAR_WATCH_PRM *prm)
{
    int            on = off != 1;
    SGDCOORDINATE *coord = mdl->GetCoord(0);
    int            head = mdl->GetJointCoord(2);       // FF3: vtable +0x34
    int            eye0 = mdl->GetJointCoord(4);
    int            eye1 = mdl->GetJointCoord(0x10);
    sceVu0FVECTOR  ang;
    SGDCOORDINATE *c;

    ang[0] = ang[1] = ang[2] = ang[3] = 0.0f;   // the zero vector at data 0x329530
    c = &coord[head];
    sgdClearCoordCalcFlgParents(c);
    sgdCalcCoordinateMatrix(c);
    if (on)
    {
        CalcAngle(ang, c, prm, 0);
        ang[1] = CharClampf(ang[1], -prm->prm[7], prm->prm[7]);
        ang[2] = CharClampf(ang[2], -prm->prm[9], prm->prm[8]);
    }
    head_yaw += (-ang[1] - head_yaw) / prm->prm[1];
    head_pitch += (ang[2] - head_pitch) / prm->prm[1];
    TurnJoint(c, SgdPtr<SGDCOORDINATE>(c->pParent)->matLocalWorld, head_yaw, head_pitch);
    sgdClearCoordCalcFlgParents(&coord[eye0]);
    sgdCalcCoordinateMatrix(&coord[eye0]);
    sgdClearCoordCalcFlgCoord(&coord[eye1]);
    sgdCalcCoordinateMatrix(&coord[eye1]);
}

// ──────────────────────────────────────────────────────────────────────
// Inline helpers (linkonce, 0x2ad5b0..0x2adbf0).
// ──────────────────────────────────────────────────────────────────────

// FF3 0x002ad688

float CharClampf(float v, float lo, float hi)
{
    if (!(lo <= v))
        return lo;
    if (hi < v)
        return hi;
    return v;
}

// ──────────────────────────────────────────────────────────────────────
// Compiler-generated code the host C++ makes itself (listed for the ledger).
// ──────────────────────────────────────────────────────────────────────

// The atexit destructors of the state singletons (the function-local statics
// of the Instance functions; each jumps to the class's complete destructor):
//   FF3 0x001485b8 CharPlayerShapeDefaultRealFace   FF3 0x001485d0 CharPlayerShapeDefaultDreamFace
//   FF3 0x001485e8 CharPlayerShapeSpecialFace       FF3 0x00148600 CharPlayerShapeDefaultHand
//   FF3 0x00148618 CharPlayerShapeLightHand         FF3 0x00148630 CharPlayerShapeDamageHand
//   FF3 0x00148648 CharPlayerShapeLadderHand        FF3 0x00148660 CharPlayerShapeDoorHand
//   FF3 0x00148678 CharPlayerShapeCameraHand
// The base-object constructor twins: FF3 0x001479b0 (CharPlayerShape),
// FF3 0x00148690 (CharRmdData).
// The deleting destructors (the header's inline virtual destructors):
//   FF3 0x002ad6e0 CharPlayerShapeStatus            FF3 0x002ad8c0 CharPlayerShape
//   FF3 0x002ad8f0 CharPlayerShapeMepati            FF3 0x002ad920 CharPlayerShapeStatusFace
//   FF3 0x002ad950 CharPlayerShapeDefaultRealFace   FF3 0x002ad980 CharPlayerShapeDefaultDreamFace
//   FF3 0x002ad9b0 CharPlayerShapeSpecialFace       FF3 0x002ad9e0 CharPlayerShapeStatusHand
//   FF3 0x002ada10 CharPlayerShapeDefaultHand       FF3 0x002ada40 CharPlayerShapeLightHand
//   FF3 0x002ada70 CharPlayerShapeDamageHand        FF3 0x002adaa0 CharPlayerShapeLadderHand
//   FF3 0x002adad0 CharPlayerShapeDoorHand          FF3 0x002adb00 CharPlayerShapeCameraHand
//   FF3 0x002adb40 CharRmdData                      FF3 0x002adb70 CharPlayerRmdData
//   FF3 0x002adba0 CharCatWatchControl              FF3 0x002adbd0 CharWatchControl
// The complete destructors of the states (the atexit thunks' targets):
//   FF3 0x002ad940 CharPlayerShapeDefaultRealFace   FF3 0x002ad970 CharPlayerShapeDefaultDreamFace
//   FF3 0x002ad9a0 CharPlayerShapeSpecialFace       FF3 0x002ada00 CharPlayerShapeDefaultHand
//   FF3 0x002ada30 CharPlayerShapeLightHand         FF3 0x002ada60 CharPlayerShapeDamageHand
//   FF3 0x002ada90 CharPlayerShapeLadderHand        FF3 0x002adac0 CharPlayerShapeDoorHand
//   FF3 0x002adaf0 CharPlayerShapeCameraHand
