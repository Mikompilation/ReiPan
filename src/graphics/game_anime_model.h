/* ==========================================================================
 *  graphics/game_anime_model.h
 *
 *  GameAnimeModel (FF3's own C++ class, RTTI name; no FF2 counterpart): a
 *  GameMotionModel that plays anime scripts -- an AnimeCodeManagerGame (the
 *  script player) and an AnimeCodeInterpretorGame (its handler, which starts
 *  the motions on the model's motion object), both made when the model is
 *  built. Each frame SetWorldMatrix advances the script clock and steps the
 *  script before handing the matrix on. CharModel (ingame/char_model.c) and
 *  its player / enemy / NPC models derive from it. FF3 0x1d36f0..0x1d3b40.
 * ======================================================================== */
#ifndef _GRAPHICS_GAME_ANIME_MODEL_H
#define _GRAPHICS_GAME_ANIME_MODEL_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "graphics/game_model.h"        // GameMotionModel

class AnimeCodeManagerGame;             // ingame/ingame_wrk.h
class AnimeCodeInterpretorGame;         // ingame/ingame_wrk.h

// RTTI GameAnimeModel : GameMotionModel, vtable 0x341ef0 (17 slots:
// GameModel's 14 with +0x00 Build, +0x08 / +0x0c the destructors and +0x18
// SetWorldMatrix replaced, then the three below). FF3 0x30 bytes; only
// CharModel's constructors build one (as their base).
// layout: host -- derived from GameMotionModel; EE offsets in the comments.
class GameAnimeModel : public GameMotionModel
{
public:
    GameAnimeModel();                   // FF3 0x001d3728 (complete; base-object twin 0x001d36f0)
    ~GameAnimeModel() override;         // FF3 0x001d37d0 (deleting 0x001d3840; base 0x001d3760)

    void  Build() override;             // FF3 0x001d3a48: GameMotionModel's, then the script player and its handler
    void  SetWorldMatrix(sceVu0FMATRIX m) override; // FF3 0x001d3958: the script's frame, then GameModel's

    virtual void CreateAnimeManager();  // FF3 0x001d3a90: vtable +0x38, scr
    virtual void CreateAnimeHandler();  // FF3 0x001d3ad0: vtable +0x3c, hdl (after scr)
    // FF3 0x001d38b0: vtable +0x40, script no from frame `start` (skip steps
    // run at once; blend: the first frame blends in), clock time, speed
    // (CharModel overrides it: 0x1450e0)
    virtual void StartAnime(int no, int start, int skip, int blend, float time, float speed);

    int   GetAnimeLength(int no);       // FF3 0x001d38e0: script no's length in frames, 0 without a player
    float GetAnimeTime();               // FF3 0x001d3910: the script clock, 0 without a player
    int   IsAnimeEnd();                 // FF3 0x001d3928: non-zero once the script has run out (and before any start)
    short GetAnimeNo();                 // FF3 0x001d3940: the script started last (-1: none), 0 without a player
    void  SetAnimeSpeed(float speed);   // FF3 0x001d39f0: the motion object's and the script clock's speed
    int   GetAnimeFlag(int n);          // FF3 0x001d3b20: the script's command flag n (one step long), 0 without a player

    /* 0x28 */ AnimeCodeInterpretorGame *hdl;   // the script's handler (CreateAnimeHandler); nullptr until built
    /* 0x2c */ AnimeCodeManagerGame     *scr;   // the script player (CreateAnimeManager); nullptr until built
};
static_assert(offsetof(GameAnimeModel, hdl) == sizeof(GameMotionModel), "GameAnimeModel.hdl (EE +0x28)");
static_assert(offsetof(GameAnimeModel, scr) == sizeof(GameMotionModel) + sizeof(void *), "GameAnimeModel.scr (EE +0x2c)");
static_assert(sizeof(GameAnimeModel) == sizeof(GameMotionModel) + 2 * sizeof(void *), "GameAnimeModel (EE 0x30 bytes)");

#endif /* _GRAPHICS_GAME_ANIME_MODEL_H */
