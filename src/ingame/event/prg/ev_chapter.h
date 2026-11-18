/* ==========================================================================
 *  ingame/event/prg/ev_chapter.h
 *
 *  FF3 0x19a0a0..0x19abe8, between ev_change.c and ev_exe.c. FF3's event
 *  files are linked in alphabetical order, so this one's name sorts between
 *  those two; the real name is unknown and the file is named after its
 *  largest part, the chapter title. It holds:
 *
 *    - GetObjectPos (FF2 had it in common/zero2_util.c);
 *    - the chapter title (CHAPTER_DISP_START): a TIM2 per chapter and
 *      language (CHAPTER_TITLE_00_TM2 + chapter + language * 0x10) loaded to
 *      the system heap, faded in over the lower half of the screen, held and
 *      faded out; IngameDrawSub steps it every frame;
 *    - the event ghost tables: for each of the 60 areas the ghosts (enemy
 *      type, id) events registered (GHOST_REGIST / GHOST_PRELOAD_REGIST /
 *      GHOST_DELETE), and the set of the area the player is in, which
 *      EvGhostAreaChange loads and releases on a room change.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_CHAPTER_H
#define _INGAME_EVENT_PRG_EV_CHAPTER_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

// GetObjectPos's obj_type (the event scripts' object kinds).
#define EV_OBJ_TYPE_PLAYER      6       // the player's position
#define EV_OBJ_TYPE_FURN        8       // a placed map object "f%03d"
#define EV_OBJ_TYPE_DOOR        9       // a placed map object "d%03d"

// The object's position into pos (zeroed first): ghost kinds 0..2 from their
// enemy work, 6 the player, 8 / 9 a placed map object of the player's area.
// 1 when found. FF3 0x0019a0a0.
int  GetObjectPos(float *pos, u_char obj_type, int obj_id);

// ── the chapter title ─────────────────────────────────────────────────────

void EvChapterDispRelease(void);        // FF3 0x0019a590: frees the picture, clears the work (EventRelease)
void EvChapterDispReq(u_char chapter);  // FF3 0x0019a5e0: CHAPTER_DISP_START (nothing while one shows)
u_char EvChapterDispIsActive(void);     // FF3 0x0019a638: 1 from the request until the fade-out ends
void EvChapterDispInit(void);           // FF3 0x0019a648: EventInit
void EvChapterDispMain(void);           // FF3 0x0019a670: IngameDrawSub, every frame
void EvChapterDispNop(void);            // FF3 0x0019a698: empty, no callers

// ── the event ghost tables ────────────────────────────────────────────────

// One registered ghost; id -1: a free row (rows are kept packed).
struct EV_GHOST_TBL                     /* 0xc */
{
    /* 0x0 */ int type;                 // enemy type (EneLoadReq's)
    /* 0x4 */ int id;                   // enemy id; -1 free
    /* 0x8 */ int preload;              // GHOST_PRELOAD_REGIST: 1 (only bit 0 is read)
};
static_assert(sizeof(EV_GHOST_TBL) == 0xc, "EV_GHOST_TBL is 0xc bytes");
static_assert(offsetof(EV_GHOST_TBL, id) == 0x4, "EV_GHOST_TBL.id");
static_assert(offsetof(EV_GHOST_TBL, preload) == 0x8, "EV_GHOST_TBL.preload");

#define EV_GHOST_TBL_MAX    7           // rows per area (0x54 bytes)
#define EV_GHOST_AREA_NUM   60          // areas (bss 0x3e1de0..0x3e3190, save block 6)

int  EvGhostTblSaveGet(void **addr);    // FF3 0x0019a6a0: {the area tables, 0x13b0} (save block 6)
void EvGhostTblInit(void);              // FF3 0x0019a6b8: every row free (EventMacroInit)
void EvGhostCtrlReleaseAll(void);       // FF3 0x0019a738: the current set released (GO_REAL_WORLD)
void EvGhostTblRegist(int area, int type, int id, int preload);  // FF3 0x0019a7f0: into the first free row
void EvGhostTblDelete(int area, int type, int id);  // FF3 0x0019a880: later rows move up
void EvGhostAreaChange(int area);       // FF3 0x0019a930: the area's table becomes the current set
int  EvGhostPreloadIsEnd(void);         // FF3 0x0019ab00: 0 while a preload ghost of the set still loads
int  EvGhostTblExist(int area, int type, int id);   // FF3 0x0019ab98: 1 when the area's table holds it (EneRelease)

#endif /* _INGAME_EVENT_PRG_EV_CHAPTER_H */
