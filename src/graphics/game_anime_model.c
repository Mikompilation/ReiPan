/* ==========================================================================
 *  graphics/game_anime_model.c
 *
 *  GameAnimeModel (FF3's own C++ class, RTTI name; no FF2 counterpart): the
 *  GameMotionModel that plays anime scripts through an AnimeCodeManagerGame
 *  and its AnimeCodeInterpretorGame (ingame/ingame_wrk.c), and the thin
 *  accessors the player, enemies and NPCs call on it. FF3 0x1d36f0..0x1d3b40.
 *  Written from the FF3 decompilation.
 *
 *  FF3 compiles each constructor and destructor twice (complete and
 *  base-object, same body) plus a deleting destructor; one C++ definition
 *  stands for each set.
 * ======================================================================== */
#include "graphics/game_anime_model.h"

#include <new>                          // placement new on FF3's C++ heap (operator_new)

#include "album/prg/scr_play.h"         // AnimeCodeManager::GetLength
#include "common/cpp_heap.h"            // operator_new / operator_delete
#include "graphics/motion/motion.h"     // MDL_MOT, MOT_CTRL
#include "ingame/ingame_wrk.h"          // AnimeCodeManagerGame, AnimeCodeInterpretorGame

// ──────────────────────────────────────────────────────────────────────
// No script player or handler yet (Build makes them).
// FF3 0x001d3728 (complete object) and FF3 0x001d36f0 (the base-object twin
// CharModel's constructors call).
// callcheck: GameMotionModelCtorBase (the base class constructor)

GameAnimeModel::GameAnimeModel()
{
    hdl = nullptr;
    scr = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the script player, then its handler, then GameMotionModel's part.
// FF3 0x001d37d0 (vtable +0x08), FF3 0x001d3760 (base object), FF3 0x001d3840
// (deleting: then operator_delete).
// callcheck: GameMotionModelDtorBase (the base class destructor)

GameAnimeModel::~GameAnimeModel()
{
    if (scr != nullptr)
    {
        // FF3: its vtable +4, the deleting destructor
        scr->~AnimeCodeManagerGame();
        operator_delete(scr);
    }
    if (hdl != nullptr)
    {
        // FF3: its vtable +4, the deleting destructor
        hdl->~AnimeCodeInterpretorGame();
        operator_delete(hdl);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Starts script no on the player (AnimeCodeManagerGame::Start's arguments
// as they come); nothing without a player.
// FF3 0x001d38b0 (vtable +0x40).
// callcheck: MotScrStart (AnimeCodeManagerGame::Start)

void GameAnimeModel::StartAnime(int no, int start, int skip, int blend, float time, float speed)
{
    if (scr != nullptr)
        scr->Start(no, start, skip, blend, time, speed);
}

// ──────────────────────────────────────────────────────────────────────
// Script no's length in frames, 0 without a player.
// FF3 0x001d38e0
// callcheck: ScrPlayGetLength (AnimeCodeManager::GetLength)

int GameAnimeModel::GetAnimeLength(int no)
{
    if (scr != nullptr)
        return scr->GetLength(no);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The script clock, 0 without a player.
// FF3 0x001d3910

float GameAnimeModel::GetAnimeTime()
{
    float time = 0.0f;

    if (scr != nullptr)
        time = scr->time;
    return time;
}

// ──────────────────────────────────────────────────────────────────────
// The script cursor's end flag (1 from construction until a start), 0
// without a player.
// FF3 0x001d3928

int GameAnimeModel::IsAnimeEnd()
{
    int end = 0;

    if (scr != nullptr)
        end = scr->cur.end;
    return end;
}

// ──────────────────────────────────────────────────────────────────────
// The script started last (FF3 reads the halfword), 0 without a player.
// FF3 0x001d3940

short GameAnimeModel::GetAnimeNo()
{
    short no = 0;

    if (scr != nullptr)
        no = (short)scr->scr_no;
    return no;
}

// ──────────────────────────────────────────────────────────────────────
// The frame's script work, then the matrix to the motion object: the clock
// advances unless the motion control is blending, and the script steps
// (starting the motions its frames name). Nothing at all without a motion
// object or a player.
// FF3 0x001d3958 (vtable +0x18).
// callcheck: GetBoneMotion (mot vtable +0x40); MotScrAdvance (AnimeCodeManagerGame::Advance); Step (scr vtable +0x08); GameModelSetWorldMatrix (GameModel::SetWorldMatrix)

void GameAnimeModel::SetWorldMatrix(sceVu0FMATRIX m)
{
    if (mot == nullptr || scr == nullptr)
        return;

    if (mot->GetBoneMotion()->blend == 0)
        scr->Advance();
    scr->Step();

    GameModel::SetWorldMatrix(m);
}

// ──────────────────────────────────────────────────────────────────────
// The motion object's speed (its vtable +0x24) and the script clock's.
// FF3 0x001d39f0
// callcheck: SetSpeed (mot vtable +0x24)

void GameAnimeModel::SetAnimeSpeed(float speed)
{
    if (mot != nullptr)
        mot->SetSpeed(speed);
    if (scr != nullptr)
        scr->speed = speed;
}

// ──────────────────────────────────────────────────────────────────────
// GameMotionModel's build, then the script player and its handler (the
// handler needs both the motion object and the player).
// FF3 0x001d3a48 (vtable +0x00; CharModel's Build 0x1451a0 calls it).
// callcheck: GameMotionModelBuild (GameMotionModel::Build)

void GameAnimeModel::Build()
{
    GameMotionModel::Build();
    CreateAnimeManager();               // FF3: vtable +0x38
    CreateAnimeHandler();               // FF3: vtable +0x3c
}

// ──────────────────────────────────────────────────────────────────────
// A new script player (FF3 0x58 bytes on its heap).
// FF3 0x001d3a90 (vtable +0x38).
// callcheck: MotScrCtor (a new-expression)

void GameAnimeModel::CreateAnimeManager()
{
    scr = new (operator_new(sizeof(AnimeCodeManagerGame))) AnimeCodeManagerGame();
}

// ──────────────────────────────────────────────────────────────────────
// A new handler over the motion object and the player, which also becomes
// the player's handler (FF3 0xc bytes on its heap).
// FF3 0x001d3ad0 (vtable +0x3c).
// callcheck: MotScrHdlCtor (a new-expression)

void GameAnimeModel::CreateAnimeHandler()
{
    hdl = new (operator_new(sizeof(AnimeCodeInterpretorGame))) AnimeCodeInterpretorGame(mot, scr);
    scr->hdl = hdl;
}

// ──────────────────────────────────────────────────────────────────────
// The script's command flag n (set by type-5 commands, cleared each step),
// 0 without a player.
// FF3 0x001d3b20

int GameAnimeModel::GetAnimeFlag(int n)
{
    if (scr != nullptr)
        return scr->flag[n];
    return 0;
}
