/* ==========================================================================
 *  outgame/title_room.h
 *
 *  PORT ADDITION -- the title screen's room backdrop (after MioPan's
 *  outgame/title_room.h, which does the same for FF2).
 *
 *  The original title plays a misty movie (OUTGAME_BG_PSS) behind a picture
 *  of Rei (TITLEBG_PHF) and the logo and menus of TITLE_PHF over both.  This
 *  puts one of the game's own rooms there instead, loaded, lit, fogged and
 *  animated by the code the game plays it with, while the camera travels
 *  between two points.  The movie and the picture go; the logo, PRESS START,
 *  the menus and every screen behind them are the ROM's own.  title_menu.c is
 *  still the whole title, and this file only sits around it.
 *
 *  It is wired in five places:
 *    - gphase.c's tables name init / pre / end_TitleRoom_Mode in Title_Mode's
 *      (5) slots; each runs the ROM's own callback (title_menu.c) and adds
 *      the room and the music around it.
 *    - pre_Title_Mode() calls TitleRoomDrawBackdrop() between its black
 *      rectangle and the layouts, so the room is under the logo.
 *    - TitleObjDrawCallback() asks TitleRoomHidesTitleArt() before drawing
 *      the movie and Rei's picture.
 *    - one_Title_Top() asks TitleRoomHoldsAttract() before counting towards
 *      the attract movie.
 *    - init_Title_Mode() preloads TitleRoomTitleBgm() where it named
 *      BGM_00_TITLE.
 *  With [title] backdrop = classic and music = 0 in reipan.ini none of them
 *  does anything, and the title is exactly the original.  The music choice
 *  applies to either backdrop.
 *
 *  Everything it reads is reipan_config.title; the overlay's Settings > Title
 *  screen window edits that live and calls in here to act on a change.  Both
 *  run on the game thread, so neither needs a lock.
 * ======================================================================== */

#ifndef _OUTGAME_TITLE_ROOM_H
#define _OUTGAME_TITLE_ROOM_H

/* --------------------------------------------------------------------------
 *  Title_Mode's callbacks as gphase.c's tables name them.  Each runs the
 *  ROM's own (init_Title_Mode / pre_Title_Mode / end_Title_Mode).
 * ------------------------------------------------------------------------ */
int  init_TitleRoom_Mode(void);
void pre_TitleRoom_Mode(void);
int  end_TitleRoom_Mode(void);

/* --------------------------------------------------------------------------
 *  For title_menu.c.
 *
 *  DrawBackdrop   pre_Title_Mode(), after its black rectangle: the room and
 *                 the veil over it, when there is one to draw.
 *  HidesTitleArt  non-zero while the room is the backdrop (loading or shown):
 *                 the movie and the picture of Rei are not drawn, so the room
 *                 is the picture and the logo is all that sits on it.
 *  HoldsAttract   non-zero while the room is up and the attract movie is
 *                 off: Title_Top does not count its idle frames.
 * ------------------------------------------------------------------------ */
void TitleRoomDrawBackdrop(void);
int  TitleRoomHidesTitleArt(void);
int  TitleRoomHoldsAttract(void);

/* init_Title_Mode()'s stream: BGM_00_TITLE, or the track chosen for the title
 * (reipan_config.title.music).  The ROM's own preload then plays it looped
 * in BGM slot 0, so a chosen track starts the way the theme does, survives
 * the option and video screens as the theme does, and is never swapped in
 * over a theme that has only just started. */
int  TitleRoomTitleBgm(void);

/* --------------------------------------------------------------------------
 *  For the overlay.
 * ------------------------------------------------------------------------ */
typedef enum
{
    TITLE_ROOM_OFF,             /* classic backdrop, or not on the title       */
    TITLE_ROOM_LOADING,         /* streaming in                                */
    TITLE_ROOM_SHOWN,           /* resident and drawn                          */
    TITLE_ROOM_RELEASED,        /* on a screen that needs the room memory      */
    TITLE_ROOM_MISSING,         /* that room is not on this disc               */
} TITLE_ROOM_STATE;

TITLE_ROOM_STATE TitleRoomGetState(void);

/* The rooms, by FF3's area number (1..58; area n is files ROS00_PK4 + n * 7
 * ..+6).  Code is the model file's own name ("RS101"), place the part of the
 * game it belongs to.  A room with night lighting (the house) can be shown on
 * either side. */
int         TitleRoomCount(void);           /* areas 0 .. count - 1 */
const char *TitleRoomCode(int area);
const char *TitleRoomPlace(int area);
int         TitleRoomIsOnDisc(int area);
int         TitleRoomHasNight(int area);

/* Drop what is resident and load the current scene's room again; also how a
 * backdrop change takes effect.  With camera_set at 0 the room is framed by
 * itself once it is in. */
void TitleRoomReload(void);

/* The scenes.  The title picks one at random among the enabled each time it
 * comes up, and moves on as reipan_config.title.change says -- fading to black
 * and up again on another, never the same one twice running while there is a
 * choice.  Indices are into reipan_config.title.scenes[].
 *
 *   Current  the one on screen (or loading).
 *   Show     put that one up now.
 *   Hold     while non-zero the title stays on its scene -- the overlay holds
 *            it for as long as the window is open, so a scene being edited
 *            does not fade away under the editor. */
int  TitleRoomCurrentScene(void);
void TitleRoomShowScene(int scene);
void TitleRoomHoldScene(int hold);

/* Re-frame a scene's room by itself: now, if it is the one up, else the next
 * time it loads.  A room is framed from its first shot of its own (below),
 * else from the floor the player can walk. */
void TitleRoomFrameAutomatically(int scene);

/* The room's own shots: the fixed and four-corner cameras its map camera cuts
 * between, as the designers placed them.  Count and Current describe the room
 * on screen (0 / -1 while none is); Use sets a scene's points from one --
 * its eye and field of view, panning a few degrees either side of its aim --
 * when that scene is the one shown.  Returns 1 when it did. */
int  TitleRoomShotCount(void);
int  TitleRoomShotCurrent(void);
int  TitleRoomUseShot(int scene, int index);

/* Copy the live camera into a scene's point 0 (A) or 1 (B).  In a game, the
 * area Rei is in becomes that scene's room as well -- and if that differs
 * from the one the other point was taken in, both points take this view.
 * Returns 1 when a room change came with it. */
int  TitleRoomCaptureView(int scene, int point);

/* The current scene's camera path.  Progress is 0..1 over one whole cycle
 * (out and back for ping-pong). */
float TitleRoomGetProgress(void);
void  TitleRoomSetProgress(float progress);
int   TitleRoomGetPaused(void);
void  TitleRoomSetPaused(int paused);
/* -1 runs the path; 0 or 1 holds the camera on that point. */
int   TitleRoomGetPreview(void);
void  TitleRoomSetPreview(int point);
/* Steer the camera by hand with the pad's sticks (the overlay lists the
 * controls).  The path stops while it is on, and the title menus do not hear
 * the pad. */
int   TitleRoomGetFly(void);
void  TitleRoomSetFly(int fly);

/* The tracks the overlay offers.  Entry 0 is "the game's own title music";
 * the rest are stream files, looped. */
int         TitleRoomMusicCount(void);
int         TitleRoomMusicFile(int index);
const char *TitleRoomMusicName(int index);
const char *TitleRoomMusicNote(int index);
int         TitleRoomMusicIndex(int file);
int         TitleRoomMusicIsOnDisc(int index);
/* Apply reipan_config.title.music / music_volume now rather than on the next
 * screen change. */
void        TitleRoomMusicChanged(void);

#endif /* _OUTGAME_TITLE_ROOM_H */
