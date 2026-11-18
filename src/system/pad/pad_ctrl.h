/* ==========================================================================
 *  system/pad/pad_ctrl.h
 *
 *  The game-side pad layer (FF3-only, FF3 0x2254b8..0x225a58, after
 *  outgame/option.c): init and per-frame update over the driver
 *  (system/pad/pad.h), button checks that play a sound effect, getters of
 *  counters / pressure / sticks, per-pad lock flags that blank all of them,
 *  and the button configuration: 26 action masks copied every frame from one
 *  of three layouts chosen in the options.
 * ======================================================================== */
#ifndef _SYSTEM_PAD_PAD_CTRL_H
#define _SYSTEM_PAD_PAD_CTRL_H

#include "sdk/scetypes.h"

// Button masks per game action (data 0x32dd08; zero in the ROM, PadKeyUpdate
// copies them each frame from pad_key_cfg[OptionWrk.now.pad_cfg]). IngameMain,
// phase 12 and the menus test them with CheckPadTrgPlaySe.
enum PAD_KEY
{
    PAD_KEY_OK     = 0,                     // 0x32dd08: confirm (phase 12, SE 0x1a9b0003)
    PAD_KEY_CANCEL = 1,                     // 0x32dd0c: cancel (phase 12, SE 0x1a9b0001)
    PAD_KEY_02     = 2,                     // 0x32dd10: held, the crawl's faster move (PlyrCrawlMove, anime 0x15)
    PAD_KEY_FINDER = 3,                     // 0x32dd14: into the viewfinder (PlyrFinderStartCheck)
    PAD_KEY_FINDER_OFF = 4,                 // 0x32dd18: out of it (PlyrFinderEndCheck)
    PAD_KEY_05     = 5,                     // 0x32dd1c: held, the crawl's faster turn (PlyrCrawlMove)
    PAD_KEY_06     = 6,                     // 0x32dd20: } presses that escape a ghost's hold (PlyrHoldEscapeCheck); the shutter
    PAD_KEY_07     = 7,                     // 0x32dd24: }   (PlyrFinderShutterCheck; player 1 charges while held)
    PAD_KEY_MENU   = 9,                     // 0x32dd2c: story menu (phase 0x25)
    PAD_KEY_10     = 10,                    // 0x32dd30: pressed with cancel, the map menu stays open (StoryMapStepMain)
    PAD_KEY_MAP    = 11,                    // 0x32dd34: map (phase 0x3a), with PAD_KEY_MAP2
    PAD_KEY_MAP2   = 12,                    // 0x32dd38
    PAD_KEY_MEMO   = 13,                    // 0x32dd3c: the memo menu (MemoNoticeMenuCheck)
    PAD_KEY_TAB_PREV = 14,                  // 0x32dd40: repeat (with d-pad left), previous tab (the document menu)
    PAD_KEY_TAB_NEXT = 15,                  // 0x32dd44: repeat (with d-pad right), next tab
    PAD_KEY_PAGE_PREV = 16,                 // 0x32dd48: repeat, previous page (the bonus shop's list)
    PAD_KEY_PAGE_NEXT = 17,                 // 0x32dd4c: repeat, next page
    PAD_KEY_18     = 18,                    // 0x32dd50: the flash (PlyrFlashInput); also shows the HUD (PhotoModeHudFadeMain)
    PAD_KEY_LENS_SHOT  = 19,                // 0x32dd54: } a shot with the selected lens (PlyrFinderLensShotCheck)
    PAD_KEY_LENS_SHOT2 = 20,                // 0x32dd58: }
    PAD_KEY_LENS_NEXT = 21,                 // 0x32dd5c: next lens equip slot (LensSelectCtrl)
    PAD_KEY_LENS_PREV = 22,                 // 0x32dd60: previous lens equip slot (LensSelectCtrl)
    PAD_KEY_ZOOM_OUT = 23,                  // 0x32dd64: held, zoom out (PhotoModeZoomCtrl)
    PAD_KEY_ZOOM_IN  = 24,                  // 0x32dd68: held, zoom in (PhotoModeZoomCtrl)
    PAD_KEY_PAUSE  = 25,                    // 0x32dd6c: pause (phase 0x2c)
    PAD_KEY_NUM    = 26,
};
#define PAD_KEY_CFG_NUM 3                   // button layouts (option setting)

extern u_int pad_key[PAD_KEY_NUM];                          // data 0x32dd08
extern u_int pad_key_cfg[PAD_KEY_CFG_NUM][PAD_KEY_NUM];     // data 0x32dd70

void PadInitAll(void);
void PadFrameUpdate(void);              // per frame, from pre_super

// Button checks on pad 'pad'; 1 when a bit of 'mask' is set and the pad is
// not locked, then plays se unless it is -1.
int CheckPadTrgPlaySe(u_short mask, int se, int pad);   // PAD_WRK.trg
int CheckPadRptPlaySe(u_short mask, int se, int pad);   // PAD_WRK.rpt
int CheckPadNowPlaySe(u_short mask, int se, int pad);   // PAD_WRK.held

// Getters; while pad 0 is locked they return the rest value (0, or 0x7f for
// stick bytes). For the stick getters 'axis' picks the stick (0 left, 1
// right) and 'stick' != 0 the previous frame's value (PAD_WRK an_* [2][2]).
u_short PadGetCount(int button, int pad);
u_char  PadGetPressure(int button, int pad);
u_char  PadGetAnalogRaw(int axis, int pad);
u_short PadGetStickWord(int axis, int stick, int pad);
float   PadGetStickFloat(int axis, int stick, int pad);
u_char  PadGetStickByte(int axis, int stick, int pad);

// Locks: a locked pad reads as idle (pad < 0: both).
int  IsPadLocked(int pad);
void PadLockSet(int pad);
void PadLockReset(int pad);

void  PadKeyUpdate(void);                       // pad_key = pad_key_cfg[option setting]
u_int PadKeyGetConfig(int cfg, int key);        // pad_key_cfg[cfg][key]

#endif /* _SYSTEM_PAD_PAD_CTRL_H */
