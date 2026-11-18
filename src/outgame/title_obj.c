/* ==========================================================================
 *  outgame/title_obj.c
 *
 *  TITLE_OBJ (see title_obj.h): a PSS movie on the map sky's movie player,
 *  drawn as one sprite scaled to the 640x448 screen.
 *  FF3 0x2439d8..0x243e00, FF3's own code.
 * ======================================================================== */
#include "outgame/title_obj.h"

#include <libgraph.h>                   // SCE_GS_SET_*

#include "graphics/graph2d/g2d_draw.h"  // SPRT_DAT, DISP_SPRT, CopySprDToSpr, DispSprD
#include "ingame/map/MapSky.h"          // GetMapSky, MapSkyMovie*, MapSkyReset
#include "system/playpss/playpss.h"     // PLAY_PSS_MPEG_INFO, playPssGetMpegInfo

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The movie player's frame in GS memory: TBP 0x2bc0 (MapSkyMovieSet's
// argument), 320 wide, PSMCT32, 512x256, CLD 1. sdata 0x371e20.
static const u_long title_obj_tex0 = SCE_GS_SET_TEX0(0x2bc0, 5, SCE_GS_PSMCT32, 9, 8, 0, 0, 0, 0, 0, 0, 1);

static MAP_SKY *title_obj_sky;          // sdata 0x372900 (gp-0xbf0): GetMapSky, fetched once

static MAP_SKY *TitleObjGetSky(void)
{
    if (title_obj_sky == nullptr)
        title_obj_sky = GetMapSky();
    return title_obj_sky;
}

// ──────────────────────────────────────────────────────────────────────
// Construction
// ──────────────────────────────────────────────────────────────────────

// The movie file at alpha 0, normal blending, not paused; the player gets
// the file, frame buffer TBP 0x2bc0 and a 320x224 picture.
// FF3 0x00243a40 (complete; FF3 0x002439d8 TitleObjCtorBase is the
// identical base-object copy).
// callcheck: GetMapSky (TitleObjGetSky, the cached getter FF3 inlines)

TITLE_OBJ::TITLE_OBJ(int file)
{
    this->file = file;
    alpha_reg = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);   // 0x44
    alpha = 0;
    TitleObjGetSky()->MovieSet(this->file, 0x2bc0, 320, 224, 1, 0);
    pause = 0;
}

// FF3 0x00243b00 (complete; FF3 0x00243aa8 TitleObjDtorBase is the
// identical base-object copy).
// callcheck: GetMapSky (TitleObjGetSky, the cached getter FF3 inlines)

TITLE_OBJ::~TITLE_OBJ()
{
    pause = 0;
    TitleObjGetSky()->Reset();
    alpha = 0;
    file = -1;
    alpha_reg = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Play and draw
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00243b58.
// callcheck: GetMapSky (TitleObjGetSky, the cached getter FF3 inlines)

int TITLE_OBJ::CheckReady()
{
    if (file < 0)
        return 1;
    return TitleObjGetSky()->MovieIsReady();
}

// Step the movie; when its next frame went to the packet buffer, draw it
// over the whole screen (the picture's size from the stream, scaled to
// 640x448) at the object's alpha and blend. Returns 1 when there is no
// movie or it has ended, 0 while it plays.
// FF3 0x00243ba0.
// callcheck: GetMapSky (TitleObjGetSky, the cached getter FF3 inlines)

int TITLE_OBJ::Draw()
{
    if (file < 0)
        return 1;
    if (TitleObjGetSky()->MovieProc(pause) != 0)
        return 1;
    if (TitleObjGetSky()->MovieSendFrame() == 0)
        return 0;

    SPRT_DAT dat;
    dat.tex0  = 0;
    dat.u     = 0;
    dat.v     = 0;
    dat.w     = 640;
    dat.h     = 448;
    dat.x     = 0;
    dat.y     = 0;
    dat.pri   = 0xa0;
    dat.alpha = (u_char)alpha;
    dat.flip  = 0;
    dat.bln   = 1;

    DISP_SPRT ds;
    CopySprDToSpr(&ds, &dat);
    ds.tex1   = SCE_GS_SET_TEX1(1, 0, 1, 5, 0, 0, 0);           // 0x161
    ds.alphar = alpha_reg;
    ds.tex0   = title_obj_tex0;
    ds.test   = SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1);        // 0x30003

    PLAY_PSS_MPEG_INFO info;
    playPssGetMpegInfo(&info);
    ds.w   = info.width;
    ds.h   = info.height;
    ds.scw = 640.0f / (float)info.width;                        // sdata 0x371e28
    ds.sch = 448.0f / (float)info.height;                       // sdata 0x371e2c
    DispSprD(&ds);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Fades and pause
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00243d18.

int TITLE_OBJ::FadeIn(int frames)
{
    if (file < 0)
        return 1;

    int step = 0x80;
    if (frames > 0)
        step = 0x80 / frames;
    if (alpha + step < 0x80)
    {
        alpha += step;
        return 0;
    }
    alpha = 0x80;
    return 1;
}

// FF3 0x00243d70.

int TITLE_OBJ::FadeOut(int frames)
{
    if (file < 0)
        return 1;

    int step = 0x80;
    if (frames > 0)
        step = 0x80 / frames;
    if (alpha - step > 0)
    {
        alpha -= step;
        return 0;
    }
    alpha = 0;
    return 1;
}

// FF3 0x00243dc0.

void TITLE_OBJ::Pause()
{
    if (file >= 0)
        pause = 1;
}

// FF3 0x00243dd8.

int TITLE_OBJ::IsPaused()
{
    (void)this;
    return 1;
}

// FF3 0x00243de0.

void TITLE_OBJ::Resume()
{
    if (file >= 0)
        pause = 0;
}
