# ReiPan UI

ReiPan's Dear ImGui C++ integration and application UI code belongs in this
directory. The Dear ImGui core and SDL3/SDL_GPU backends are provided by the
`imgui::imgui` CMake target.

The renderer owns the bootstrap lifecycle. Add application panels to
`ReiPanUi::Draw()`; the initial panel mirrors the renderer statistics sampled for
the SDL window title.

## RmlUi

The player-facing menus are RmlUi documents (`resources/rml/*.rml` and
`.rcss`), separate from the ImGui developer overlay, brought over from MioPan
the way MioPan has them. `reipan_rmlui.cpp` is the backend -- SDL_GPU rendering
through `rmlui.vert`/`rmlui.frag`, input (keys, mouse, and the pad as keys),
and the one `Rml::Context`. Each screen is a file of its own that loads its
document and handles its events:

- `reipan_rml_pause.cpp` -- the in-game pause screen: Continue, Options,
  Quit, in place of the layout menu `ingame/pause/prg/pause.c` starts.
  Options opens the options screen over it.
- `reipan_rml_options.cpp` -- the options screen: the game's own settings
  (`OPTION_DAT`) and the port's (`reipan.ini`) in sections, in place of
  phase 7's screens, from the pause and from the title menu. From the pause,
  `pause.c` runs the backdrop the game's option screens have there under it:
  the menu background with its snow and fog.
- `reipan_rml_gamefiles.cpp` -- the screen for a boot that finds no game: the
  player chooses a disc image or IMG_BD.BIN, the choice is checked and saved
  in `[paths] data_folder`, and only then does the game start. It runs before
  anything of the game, so it stands in for nothing and is silent, the overlay
  stays away, and its art owes nothing to the disc.
- `reipan_rml_common.cpp` -- what the screens share: the game's menu sounds,
  button prompts, the yes / no window.

A screen stands in for one the game still runs: each frame the phase asks for
the screen and acts on the player's answer with its own code. With RmlUi
unavailable -- or turned off with `REIPAN_RMLUI=0`, to set a screen beside the
game's own -- the game's own screens run as they always did. While a modal
document is up the game reads no input (`ReiPan_InputSetMenuSuppressed`), and
the press that closes it never reaches the game. Pictures made at run time
reach RmlUi as `gen:` images (`ReiPanRml::SetGeneratedImage`).

The game's own screens, the ones RmlUi does not stand in for, take the mouse
and Enter / Backspace through `reipan/io/reipan_menu_mouse.h` and
`ingame/menu/menu_mouse.h`: clicks and the wheel become pad presses, so each
menu runs the code it runs for the pad. Where a target is comes from what
`B2dCtlDraw` drew last frame, not from coordinates kept per screen. Hint rows
and Yes / No are handled once for every screen (`MenuMousePoll`), a cursor being
updated or a pad check for OK / cancel outside free play claims the mouse, so
right click is back on every menu, and screens add one call for their rows.
`build.py --mouse` scripts it and `mmcheck.py` outlines the targets on a shot.

The look they share is `resources/rml/theme.rcss`, Fatal Frame III's own,
taken from its menu art (`tools/reverse/tm2png.py --file PAUSE_PHF` and the
MENUBG / MENUTOP / OPTION layouts): the pause window's jade hemp-leaf band,
steel sides and silver bars, slate-blue nights, the cream cursor sheen of its
menus and the teal glow of its pause. Its images are drawn by
`tools/rml_art.py`. RmlUi, FreeType and the Crimson Text font come from
`cmake/rmlui.cmake`, and `resources/rml` is deployed next to the executable on
every build.
