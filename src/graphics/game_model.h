/* ==========================================================================
 *  graphics/game_model.h
 *
 *  FF3's model object roots (C++, no FF2 counterpart; RTTI names):
 *  GameModel -- a model PK4 from the shared file cache, turned into a motion
 *  object by a model builder once loaded -- and GameMotionModel, which adds a
 *  motion file and a second file to it. GameAnimeModel (graphics/game_anime_model.h,
 *  0x1d36f0..0x1d3b40) and CharModel (ingame/char_model.c) derive from
 *  GameMotionModel; AcsModel, PssModel and the lantern model are plain
 *  GameModels. FF3 0x1d2a40..0x1d36f0, with GameModel's inline virtuals at
 *  0x2ac568..0x2ac590 / 0x2ac980.
 *
 *  A model: Load(file) asks the file cache for the PK4 (a full cache first
 *  runs the two free hooks, which release enemies); IsLoaded() answers once
 *  the file is in and builds the motion object (Build) the first time; per
 *  frame UpdateMatrix() hands the world matrix to it and DrawMdl() draws it.
 * ======================================================================== */
#ifndef _GRAPHICS_GAME_MODEL_H
#define _GRAPHICS_GAME_MODEL_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FMATRIX

class ModelManager;                          // graphics/mmanage.h
typedef ModelManager MDL_MOT;
class ModelBuilder;                     // graphics/model_builder.h
struct SGDCOORDINATE;                   // graphics/graph3d/sgd_types.h

// A free hook: run when the file cache is full, to make room (GameModel's
// constructor installs RandGhostReleaseAll / RandGhostCandleRelease).
typedef void (*GAME_MODEL_FREE_HOOK)(void);

// RTTI GameModel, vtable 0x341f80 (14 slots, in the order declared below).
// FF3 allocates it with operator new(0x1c) (the lanterns).
// layout: host -- the vptr and the pointers are 4 bytes on the EE; EE
// offsets in the comments.
class GameModel
{
public:
    GameModel();                        // FF3 0x001d2a80 (complete; base-object twin 0x001d2a40)

    virtual void Build();               // FF3 0x001d2bc8: vtable +0x00, the builder and the motion object
    virtual void BuildPre();            // FF3 0x002ac568: vtable +0x04, empty
    virtual ~GameModel();               // FF3 0x001d2b18: vtable +0x08 (deleting +0x0c 0x001d2b70; base 0x001d2ac0)
    virtual int  IsLoaded();            // FF3 0x001d2cf0: vtable +0x10, 1 once the file is in (builds then)
    virtual int  IsDrawn();             // FF3 0x002ac570: vtable +0x14, 1
    virtual void SetWorldMatrix(sceVu0FMATRIX m);   // FF3 0x001d3010: vtable +0x18
    virtual void GetWorldMatrix(sceVu0FMATRIX out); // FF3 0x001d2e60: vtable +0x1c, the unit matrix
    virtual void DrawMdlPre();          // FF3 0x002ac980: vtable +0x20, empty
    virtual void DrawFirst();           // FF3 0x002ac578: vtable +0x24, empty
    virtual void DrawSecond();          // FF3 0x002ac580: vtable +0x28, empty
    virtual void GetCoordPos(float *out, int coord, float *in);    // FF3 0x001d2e90: vtable +0x2c
    virtual void GetCoordAxis(float *out, int coord, int axis);    // FF3 0x001d2eb8: vtable +0x30
    virtual int  GetJointCoord(int joint);  // FF3 0x002ac588: vtable +0x34, 0 (CharModel: 0x145528)

    int    Load(short id);              // FF3 0x001d3040: 1 when the file cache took the request
    void   UpdateMatrix();              // FF3 0x001d2d50: GetWorldMatrix -> SetWorldMatrix
    void   DrawMdl();                   // FF3 0x001d2d98: the motion object's draw
    void   Draw();                      // FF3 0x001d2e08: DrawFirst, DrawSecond
    float  GetCoordRot(int coord, int axis);    // FF3 0x001d2ee0: -pi..pi
    SGDCOORDINATE *GetCoord(int no);    // FF3 0x001d3080: the coordinates, nullptr without a motion object
    void   SetAlpha(u_char alpha);      // FF3 0x001d30b0: every SGD's material alpha
    void   SetMeshAlpha(int no, u_char alpha);  // FF3 0x001d30e0: SGD no's material alpha
    void   SetTexFrame(u_char frame);   // FF3 0x001d3110: every texture's frame
    void   SetTexFrameOne(int no, u_char frame);    // FF3 0x001d3148: texture no's frame
    void   SetFreeHook(int no, GAME_MODEL_FREE_HOOK hook);  // FF3 0x001d3180
    void  *FileReq(short no);           // FF3 0x001d3190: file cache request, the hooks on failure
    void   FileRelease(short no);       // FF3 0x001d3238

    /* 0x04 */ MDL_MOT              *mot;       // the motion object (the builder's result); nullptr until built
    /* 0x08 */ ModelBuilder         *builder;   // made by Build, deleted by the destructor
    /* 0x0c */ GAME_MODEL_FREE_HOOK  hook[2];   // run in turn when the file cache is full
    /* 0x14 */ short                 id;        // CD_FILE_DAT of the model PK4, -1: none
    /* 0x18 */ void                 *pk4;       // its data in the file cache (mpk / tpk / ... members, MdlPk4GetPart)
};
static_assert(offsetof(GameModel, mot) == sizeof(void *), "GameModel.mot (EE +0x04)");
static_assert(offsetof(GameModel, builder) == 2 * sizeof(void *), "GameModel.builder (EE +0x08)");
static_assert(offsetof(GameModel, hook) == 3 * sizeof(void *), "GameModel.hook (EE +0x0c)");
static_assert(offsetof(GameModel, id) == 5 * sizeof(void *), "GameModel.id (EE +0x14)");
static_assert(offsetof(GameModel, pk4) == 6 * sizeof(void *), "GameModel.pk4 (EE +0x18)");
static_assert(sizeof(GameModel) == 7 * sizeof(void *), "GameModel (EE 0x1c bytes)");

// RTTI GameMotionModel : GameModel, vtable 0x341f40 (GameModel's slots with
// +0x00 Build, +0x08 / +0x0c the destructors and +0x10 IsLoaded replaced).
// FF3 allocates it with operator new(0x28) (0x1590c8 / 0x159f18).
// layout: host -- derived from GameModel; EE offsets in the comments.
class GameMotionModel : public GameModel
{
public:
    GameMotionModel();                  // FF3 0x001d32b0 (complete; base-object twin 0x001d3268)
    ~GameMotionModel() override;        // FF3 0x001d3340 (deleting 0x001d3388; base 0x001d32f8)

    void  Build() override;             // FF3 0x001d34b0: a MotionModelBuilder with the motion data
    int   IsLoaded() override;          // FF3 0x001d35b8: all three files in, then BuildPre + Build once

    int   Load(short mdl, short mot, short shp);    // FF3 0x001d33d8: 1 when all requests were taken
    short GetMotNo();                   // FF3 0x001d3670: the motion control's motion number
    float GetMotFrame();                // FF3 0x001d36b0: the motion control's frame

    /* 0x1c */ short mot_no;            // CD_FILE_DAT of the motion PK4, -1: none
    /* 0x1e */ short shp_no;            // CD_FILE_DAT of the second file, -1: none
    /* 0x20 */ void *mot_data;          // the motion PK4 (member 0 goes to the builder)
    /* 0x24 */ void *shp_data;          // the second file's data, to the builder as is
};
static_assert(offsetof(GameMotionModel, mot_no) == sizeof(GameModel), "GameMotionModel.mot_no (EE +0x1c)");
static_assert(offsetof(GameMotionModel, shp_no) == sizeof(GameModel) + 2, "GameMotionModel.shp_no (EE +0x1e)");
static_assert(offsetof(GameMotionModel, mot_data) == sizeof(GameModel) + sizeof(void *), "GameMotionModel.mot_data (EE +0x20)");
static_assert(offsetof(GameMotionModel, shp_data) == sizeof(GameModel) + 2 * sizeof(void *), "GameMotionModel.shp_data (EE +0x24)");

#endif /* _GRAPHICS_GAME_MODEL_H */
