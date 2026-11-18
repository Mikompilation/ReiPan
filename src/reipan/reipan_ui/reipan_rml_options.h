#ifndef REIPAN_RML_OPTIONS_H
#define REIPAN_RML_OPTIONS_H

/*
 * The options screen, in RmlUi: the game's own Options and the port's
 * settings on one page, in place of phase 7's screens (outgame/option*.c).
 *
 * Sections run across the top -- Game, Display, Graphics, Audio and Controls
 * -- and each is a column of rows: a choice (left / right steps it), a slider
 * (left / right moves it, the mouse can drag it) or an action.  Up / down move
 * the cursor, L1 / R1 (Q / E, Page Up / Down, Tab) change section, Triangle,
 * Circle or Escape backs out.  The mouse does all of it too: hover moves the
 * cursor, the arrows and the slider take clicks, the sections and the prompts
 * are buttons.
 *
 * Everything applies the moment it changes.  The port's settings save
 * themselves to reipan.ini as they do from the overlay.  The game's own are
 * written to the option work's live settings (GetOptionWrk()->now), which is
 * what the game reads and what a game save stores (save block 17), exactly
 * as an OK on the game's own option screens leaves them; the sound settings
 * reach the sound layer at once, as phase 7's after step pushes them.
 *
 * The opener drives it with the C entry points below: Open() it, then poll
 * IsOpen() once a frame; the screen closes itself when the player backs out.
 */

/* Where the screen was opened from. */
#define REIPAN_RML_OPTIONS_FROM_TITLE 0
#define REIPAN_RML_OPTIONS_FROM_PAUSE 1

#ifdef __cplusplus
extern "C" {
#endif

/* 1 with the screen up; 0 when RmlUi is unavailable and the caller should
 * fall back to the game's own option phase. */
int  ReiPan_RmlOptionsOpen(int from);
void ReiPan_RmlOptionsClose(void);
int  ReiPan_RmlOptionsIsOpen(void);

#ifdef __cplusplus
}

namespace Rml
{
class Context;
}

namespace ReiPanRmlOptions
{
/* The backend's hooks: load the document, release it, and once per frame
 * before RmlUi updates, refresh what the rows show. */
bool Init(Rml::Context *context);
void Shutdown();
void Frame();
}
#endif

#endif
