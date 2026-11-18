/* ==========================================================================
 *  ingame/map/MapSky.h
 *
 *  FF3's map sky (0x11d890..0x11e820, the dome's model classes inline at
 *  0x2ac590..0x2ac870; FF2's MapSky.c is a different, pak-based sky). Two
 *  objects:
 *  - MAP_SKY (GetMapSky, 0x41e4c8): a PSS movie player that decodes into a
 *    texture at a TBP. The title (outgame/title_obj.c) plays its backdrop
 *    movie on it (320x224 at TBP 0x2bc0, looping), the sky data below its
 *    sky movies (160x128 on the dome model's texture).
 *  - MAP_SKY_DATA (GetMapSkyData, 0x41e568): the sky dome, a PssModel
 *    (DOME_MDL_PK4) whose texture shows the nine SCENE91xxM_PSS movies in a
 *    shuffled order (graphics/effect/effect_mapsky.c shows it in game).
 * ======================================================================== */
#ifndef _INGAME_MAP_MAPSKY_H
#define _INGAME_MAP_MAPSKY_H

#include <stddef.h>                     // offsetof

#include "ingame/ingame.h"              // GameModel
#include "graphics/model_builder.h"     // NormalModelBuilder
#include "graphics/model_object.h"      // TextureModelObject
#include "sdk/libvu0.h"                 // sceVu0FVECTOR
#include "system/playpss/playpss.h"     // playPssRsrcs

// The movie player. load: 0 set, 1 allocating, 2 starting the stream,
// 3 waiting for the loop reader, 4 running; -1 none (Init).
// play: 0 set, 1 playing, 2 stopping, 3 stopped; -1 none.
// layout: host -- rsrcs holds pointers (4 bytes on the EE); EE offsets in
// the comments. FF3 0x64 bytes.
// C++ in FF3: the static's constructor runs Init (GetMapSky's first use),
// and FF3 registers its empty destructor 0x11df80 with atexit.
struct MAP_SKY
{
    MAP_SKY() { Init(); }

    void Init();                                    // FF3 0x0011df88 (MapSkyInit): file -1, states -1
    void MovieSet(int file, int tbp, int width, int height, int loop, int alpha_th);
                                                    // FF3 0x0011dfc0 (MapSkyMovieSet): parameters, states 0
    int  MovieLoad();                               // FF3 0x0011dff0 (MapSkyMovieLoad): steps the load; 1 when it has started
    int  MovieCheckStart();                         // FF3 0x0011e338 (MapSkyMovieCheckStart)
    int  MovieIsReady();                            // FF3 0x0011e388 (MapSkyMovieIsReady): steps the load; 1 when the PSS plays
    int  MovieProc(int skip);                       // FF3 0x0011e3b8 (MapSkyMovieProc): decodes a frame; 1 at the end (or not started)
    int  MovieSendFrame();                          // FF3 0x0011e4a0 (MapSkyMovieSendFrame): the frame's chain into the packet buffer; 1 when queued
    void MovieEnd();                                // FF3 0x0011e570 (MapSkyMovieEnd): stops decoding and the stream
    void MovieFreeBuf();                            // FF3 0x0011e5e8 (MapSkyMovieFreeBuf)
    void Reset();                                   // FF3 0x0011e718 (MapSkyReset): end, free, Init
    void MovieChange(int file);                     // FF3 0x0011e748 (MapSkyMovieChange): next file while playing
    void MovieStop();                               // FF3 0x0011e798 (MapSkyMovieStop): 1 -> 2 (playPssStop), or -> 3 when not started
    int  MovieIsNotStopping();                      // FF3 0x0011e7e8 (MapSkyMovieIsNotStopping): play != 2
    void MovieRestart();                            // FF3 0x0011e7f8 (MapSkyMovieRestart): 3 -> 0, load 2

    /* 0x00 */ int    file;             // the PSS (CD_FILE_DAT), -1 none
    /* 0x04 */ int    tbp;              // texture base the frames load to
    /* 0x08 */ int    width;            // picture size
    /* 0x0c */ int    height;
    /* 0x10 */ int    loop;             // read through the PSS loop reader (pss_loop.c), else MyStr*
    /* 0x14 */ int    alpha_th;         // IPU SETTH th0 = 10 while it plays
    /* 0x18 */ char   load;             // load state (above)
    /* 0x19 */ char   play;             // play state (above)
    /* 0x1c */ int    started;          // playPssIsReady seen
    /* 0x20 */ int    hold;             // non-zero: MovieProc does not decode (only cleared here)
    /* 0x24 */ int    pkt_size;         // bytes of the last frame's load chain (rsrcs.path3tag)
    /* 0x28 */ playPssRsrcs rsrcs;
};
static_assert(offsetof(MAP_SKY, load) == 0x18, "MAP_SKY.load");
static_assert(offsetof(MAP_SKY, play) == 0x19, "MAP_SKY.play");
static_assert(offsetof(MAP_SKY, pkt_size) == 0x24, "MAP_SKY.pkt_size");
static_assert(offsetof(MAP_SKY, rsrcs) == 0x28, "MAP_SKY.rsrcs");

// The sky dome model (RTTI PssModel : GameModel, 0x1c bytes, vtable
// 0x3411c8; its methods are inline code at 0x2ac590..0x2ac6d0). No fields
// of its own; its destructors are GameModel's (implicit here; FF3 0x2ac5c8,
// deleting 0x2ac590).
// layout: host -- derived from GameModel; FF3 0x1c bytes.
class PssModel : public GameModel
{
public:
    void Build() override;              // FF3 0x002ac5f0: vtable +0x00 (a PssModelBuilder)
    void DrawMdlPre() override;         // FF3 0x002ac6a0: vtable +0x20, the player's area light
};

// The dome's model object (RTTI DummyTextureModelObject : TextureModelObject,
// vtable 0x341248; FF3 operator new(0x18), no fields of its own): a texture
// object that never sends texture 0 -- that one is the movie frame, which
// MAP_SKY_DATA::Draw puts there itself (MakeTim2Direct of the first picture).
// Its constructor is inline (PssModelBuilder::BuildObject, mono always
// null); the other slots are ModelObject's (EndDraw 0x2ac7d8, DrawSgd,
// SetTexFrame / SetTexFrameOne / GetTexFrame 0x2ac7e0..0x2ac7f0), the
// destructors 0x2ac7a8 / deleting 0x2ac7b8 implicit here.
// layout: host (as TextureModelObject); FF3 0x18 bytes.
class DummyTextureModelObject : public TextureModelObject
{
public:
    DummyTextureModelObject(void *mpk, void *flgs, void *tpk)
        : TextureModelObject(mpk, flgs, tpk, nullptr) {}

    void BeginDraw() override;          // FF3 0x002ac7f8: vtable +0x00, textures 1.. of tpk
};
static_assert(sizeof(DummyTextureModelObject) == sizeof(TextureModelObject), "DummyTextureModelObject adds no fields");

// The dome's builder (RTTI PssModelBuilder : NormalModelBuilder, vtable
// 0x341218; FF3 operator new(0x28), no fields of its own): a normal builder
// whose object is a DummyTextureModelObject and which makes no collision
// shapes. BuildTexture is ModelBuilder's (0x2ac730), BuildMotion and
// GetResult NormalModelBuilder's. Constructor inline (PssModel::Build);
// destructors 0x2ac6d0 / deleting 0x2ac6f8 implicit here.
// layout: host (as NormalModelBuilder); FF3 0x28 bytes.
class PssModelBuilder : public NormalModelBuilder
{
public:
    explicit PssModelBuilder(void *const *pk4) : NormalModelBuilder(pk4) {}

    void BuildObject() override;        // FF3 0x002ac740: vtable +0x08
};
static_assert(sizeof(PssModelBuilder) == sizeof(NormalModelBuilder), "PssModelBuilder adds no fields");

// C++ in FF3: no constructor (GameSystemStart calls Init), but an empty
// destructor 0x15b650 that FF3 registers with atexit.
// layout: host -- pointers (4 bytes on the EE); EE offsets in the comments.
struct MAP_SKY_DATA
{
    void  Init();                       // FF3 0x0011d890 (MapSkyDataInit)
    void  Create();                     // FF3 0x0011d8a0 (MapSkyDataCreate): new dome model, loading
    void  Release();                    // FF3 0x0011d900 (MapSkyDataRelease)
    void  Shuffle();                    // FF3 0x0011d978 (MapSkyDataShuffle)
    int   NextFile();                   // FF3 0x0011da98 (MapSkyDataNextFile)
    void  FreeList();                   // FF3 0x0011dae8 (MapSkyDataFreeList)
    int   IsReady();                    // FF3 0x0011db30 (MapSkyDataIsReady): 1 once the model is in (first movie set)
    int   Proc();                       // FF3 0x0011dc38 (MapSkyDataProc): per frame; 1 while not started
    void  Draw(sceVu0FVECTOR pos);      // FF3 0x0011dd20 (MapSkyDataDraw)
    void  Stop();                       // FF3 0x0011dea8 (MapSkyDataStop)
    int   IsNotStopping();              // FF3 0x0011def0 (MapSkyDataIsNotStopping)
    void  Restart();                    // FF3 0x0011df38 (MapSkyDataRestart)
    void *GetTim2();                    // the dome's first texture (inline in FF3)

    /* 0x00 */ int      *list;          // the shuffled movie ids, -1 terminated (system heap, 0x28)
    /* 0x04 */ int      *next;          // the next id in it
    /* 0x08 */ int       _08;
    /* 0x0c */ PssModel *mdl;           // the dome (Create)
    /* 0x10 */ int       started;       // the model is in and the first movie set
    /* 0x14 */ int       playing;       // the current movie is ready
};
static_assert(offsetof(MAP_SKY_DATA, next) == sizeof(int *), "MAP_SKY_DATA.next (EE +0x04)");
static_assert(offsetof(MAP_SKY_DATA, playing) == offsetof(MAP_SKY_DATA, started) + 4,
              "MAP_SKY_DATA.playing (EE +0x14)");

// FF3 0x002ac520 / 0x002afdf0: the singletons' getters (emitted with the
// statics after the program; written in MapSky.c with their classes).
MAP_SKY      *GetMapSky(void);
MAP_SKY_DATA *GetMapSkyData(void);

#endif /* _INGAME_MAP_MAPSKY_H */
