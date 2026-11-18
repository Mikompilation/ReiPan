#ifndef REIPAN_UI_TITLE_H
#define REIPAN_UI_TITLE_H

namespace ReiPanUiTitle
{
/* The Settings menu's row that opens the title screen window.  Call from
 * inside an open menu. */
void MenuItem();

/* The title screen window: the backdrop, the scenes and their rooms, the two
 * camera points and the path between them, and the music.  It edits
 * reipan_config.title and tells outgame/title_room.c when a change needs
 * acting on; the overlay's settle-and-save writes it to reipan.ini.  Call
 * once a frame from Draw(), on the game thread. */
void Draw();
} // namespace ReiPanUiTitle

#endif
