/* ==========================================================================
 *  graphics/scene/scene_dat.h
 *
 *  Scene play control (FF3; FF2 scene_dat.o's link slot, which held only
 *  tables): one engine scene played from a loaded SCENE_CTRL slot
 *  (ScenePlayStart / Main / IsEnd / End for one_Story_Scene_Main), the slot
 *  release, the per-scene volumes of scene.vol and FF2 scene_dat.c's PAL
 *  cut-timing lists. FF3 0x253620..0x254018 + 0x2be808 / 0x2bef98.
 * ======================================================================== */
#ifndef _GRAPHICS_SCENE_SCENE_DAT_H
#define _GRAPHICS_SCENE_SCENE_DAT_H

#include <stddef.h>                     // offsetof

#include "graphics/scene/scene.h"       // SCENE_CTRL, SCN_MDL
#include "sdk/scetypes.h"

// scene.vol (SCENE_VOL 0xa48): "VOL\0", the row count, then one row per
// scene in scene_dat's order (all 68 in the EU file). Its values replace
// scene_dat's (SceneVolDataApply), the volume StartMovie / the scene stream
// play at.
struct SCENE_VOL_HDR
{
    /* 0x0 */ char magic[4];            // "VOL"
    /* 0x4 */ int  num;                 // rows that follow
};
static_assert(sizeof(SCENE_VOL_HDR) == 0x8, "SCENE_VOL_HDR (0x8 bytes)");

struct SCENE_VOL_ROW
{
    /* 0x0 */ char  name[4];            // the scene's four digits (SceneDatGetName)
    /* 0x4 */ short _4;                 // 0 in every row
    /* 0x6 */ short vol;                // 55..100
};
static_assert(sizeof(SCENE_VOL_ROW) == 0x8, "SCENE_VOL_ROW (0x8 bytes)");
static_assert(offsetof(SCENE_VOL_ROW, vol) == 0x6, "SCENE_VOL_ROW.vol");

// One row of the PAL cut-timing table (rodata 0x362810): a scene and its
// -1-terminated list of the frames its camera cuts on; a row with a null
// list ends the table.
// layout: host -- list is 4 bytes on the EE (row 0x8).
struct SCENE_CUT_TIMING
{
    /* 0x0 */ int        scene_no;
    /* 0x4 */ const int *list;
};
static_assert(offsetof(SCENE_CUT_TIMING, list) == sizeof(void *), "SCENE_CUT_TIMING.list (EE +0x4)");

void SceneVolDataLoadReq(void);         // FF3 0x00253d88: scene.vol into a system heap block (one_Boot_Load)
void SceneVolDataApply(void);           // FF3 0x00253e18: its volumes into scene_dat
void SceneVolDataFree(void);            // FF3 0x00253ed8
void SceneWrkRelease(void);             // FF3 0x00253c38: both slots released, scene_ctrl cleared (IngameEnd)

// One step of the slot's scene stream (stream_step): 1 ready or none, 0
// preloading, -1 no stream file (then 1 on the next call) or nullptr.
int  SceneStreamLoadMain(SCENE_CTRL *sc);   // FF3 0x00253620
// Starts scene_no from its loaded slot once data, rooms and stream are
// ready; 1 when it started.
int  ScenePlayStart(int scene_no);      // FF3 0x00253728
// One frame of the playing scene; skip: START / OK may end it. 1 while a
// scene plays.
int  ScenePlayMain(int subtitle, int skip); // FF3 0x00253888
int  ScenePlayIsEnd(void);              // FF3 0x002539e8: 1 when no scene plays or it is over
void ScenePlayEnd(void);                // FF3 0x00253a08: back to the game (camera, light, rooms), the slot released
void SceneCtrlRelease(SCENE_CTRL *sc);  // FF3 0x00253b18: models, buffers and stream let go, the slot cleared
void SceneCtrlReleaseAll(void);         // FF3 0x00253c00
SCENE_CTRL *ScenePlayCtrlGet(void);     // FF3 0x00253d78: the playing slot, nullptr
// num names of 8 chars from data into rows (their object numbers too);
// returns data past them, aligned to 16.
u_char *SceneMdlNameSet(SCN_MDL *row, int num, u_char *data);  // FF3 0x00253f20
const int *SceneCutTimingGet(int scene_no); // FF3 0x00253fc0: scene_no's cut list, nullptr
// The map doors and furniture the scene replaces shown (show 1) or hidden,
// the scene's door / furniture / item models opaque or invisible.
void SceneMapObjShow(SCENE_CTRL *sc, int show); // FF3 0x002be808 (out of line)
int  StrGetNumber(const char *str, int len);    // FF3 0x002bef98: the first digits in str[0..len) (at most 10)

#endif /* _GRAPHICS_SCENE_SCENE_DAT_H */
