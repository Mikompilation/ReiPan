#ifndef REIPAN_RML_PAUSE_H
#define REIPAN_RML_PAUSE_H

/*
 * The pause screen, in RmlUi: Continue, Options, Quit.
 *
 * Stands in for layout animation 4, the menu ingame/pause/prg/pause.c starts
 * over the captured game screen.  The game still owns the phase: it draws the
 * paused frame, asks this screen what the player chose, and acts on it exactly
 * as on the layout menu's choice (PauseMenuGetSel).  Options is the RmlUi
 * options screen (reipan_rml_options.h), opened over this one without
 * answering; only when that cannot open is it answered with
 * REIPAN_RML_PAUSE_OPTIONS, the game's own option phase, which comes back
 * here with the cursor on Options.
 */

/* What the player chose -- the same codes as PauseMenuGetSel(). */
#define REIPAN_RML_PAUSE_NONE     (-1)
#define REIPAN_RML_PAUSE_CONTINUE 0
#define REIPAN_RML_PAUSE_OPTIONS  1
#define REIPAN_RML_PAUSE_QUIT     2

#ifdef __cplusplus
extern "C" {
#endif

/* 1 with the screen up, its cursor on `cursor` (one of the codes above); 0
 * when RmlUi is unavailable and the game's own menu should run instead. */
int  ReiPan_RmlPauseOpen(int cursor);
void ReiPan_RmlPauseClose(void);
int  ReiPan_RmlPauseIsOpen(void);
int  ReiPan_RmlPauseConsumeResult(void);

#ifdef __cplusplus
}

namespace Rml
{
class Context;
}

namespace ReiPanRmlPause
{
bool Init(Rml::Context *context);
void Shutdown();
void Frame();
}
#endif

#endif
