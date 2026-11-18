/* ==========================================================================
 *  ingame/tattoo_event.h
 *
 *  The tattoo grab event (FF3-only, no FF2 counterpart), FF3
 *  0x13a010..0x13b320, between camVci.c and candle_ctrl.c. A ghost's script
 *  (EJobE04) starts it: Rei is held, the camera eases in on her tattoo
 *  (joint 2), the picture flashes white into the dream look (the dream
 *  light, monochrome, screen effect 8, the map-sky effect, the YKN_18
 *  stream), a follow camera circles her while the ghost acts, and on
 *  EJobE05 the camera fades back out to the map camera. CameraMain runs the
 *  camera every frame (TattooEvCameraMain); while it is idle the map camera
 *  drives. The work is one bss block, 0x391000..0x391080.
 * ======================================================================== */
#ifndef _INGAME_TATTOO_EVENT_H
#define _INGAME_TATTOO_EVENT_H

struct ENE_WRK;                         // ingame/enemy/ene_main.h

void TattooEvInit(void);                // FF3 0x0013a010: EnemyMainInit clears the work
void TattooEvEneRelease(ENE_WRK *ene);  // FF3 0x0013a038: EneRelease; frees the sky effect when ene is the event's
void TattooEvStart(ENE_WRK *ene);       // FF3 0x0013a080: EJobE04; state 0 -> 2 (1 when Rei's viewfinder hold is dropped), keeps ene, preloads YKN_18_IREZUMI3A
void TattooEvResume(void);              // FF3 0x0013a158: EJobE05; state 6 -> 7 (the way back)
int  TattooEvCameraMain(int rect_flg);  // FF3 0x0013a178: CameraMain; 1 while idle (the map camera runs)
int  TattooEvIsLate(void);              // FF3 0x0013ac20: state > 5
int  TattooEvIsIdle(void);              // FF3 0x0013ac38: state == 0
void TattooEvEffectRelease(void);       // FF3 0x0013ac48: frees the sky effect (PlyrDeathCtrl step 10)
void TattooEvEffectStop(void);          // FF3 0x0013ac88: stops it (PlyrDeathCtrl step 9)
int  TattooEvEffectIsStopped(void);     // FF3 0x0013acb8: 1 when stopped or none (PlyrDeathCtrl step 10 waits)
int  TattooEvGetToggle(void);           // FF3 0x0013b270: flips 0 / 1 per event; EneStreamNearMain plays YKN_19_IREZUMI2B while 1
void TattooScrEffectSave(void);         // FF3 0x0013b280: EneEffInit keeps the screen effect number
void TattooScrEffectRestore(void);      // FF3 0x0013b2a8: EJobC10 puts it back
void TattooScrEffectAfter(void);        // FF3 0x0013b2c8: screen effect 9
void TattooScrEffectDream(void);        // FF3 0x0013b2e0: screen effect 8

#endif /* _INGAME_TATTOO_EVENT_H */
