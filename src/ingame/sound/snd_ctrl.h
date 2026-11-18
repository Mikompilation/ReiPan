/* ==========================================================================
 *  ingame/sound/snd_ctrl.h
 *
 *  FF3's game-side sound API, FF3 0x257ae0..0x258a68. FF3-only: FF2 called
 *  its sound library directly. Two private tables sit over the library
 *  (FF2's system/eeiop snd.c / snd_bank.c / snd_util.c / snd_3d.c, plus an
 *  FF3 request layer for streamed BGM, all at 0x269fe0..):
 *
 *    - SE banks (28 entries): a bank is a pair of files, the BD body
 *      `file` and the HXD header `file + 1`, loaded once and reference
 *      counted by file number. File -1 means the default bank
 *      SE_BANK_DEFAULT (0x1a9b), loaded during the boot.
 *    - BGM slots (2): the stream handle and file number playing in each.
 *
 *  An SE id is (bank file << 16) | sample number. Volumes are 0..0x7f and
 *  become the library's 0..0x3fff through SndConvVolume.
 *
 *  The file sits where FF2's link order has ingame/plyr/sis_algo.c and
 *  sis_mdl.c; FF3 has no sister, and every FF2 name inherited here was a
 *  wrong match. FF3's real file name is unknown.
 * ======================================================================== */
#ifndef _INGAME_SOUND_SND_CTRL_H
#define _INGAME_SOUND_SND_CTRL_H

#include <stddef.h>                     // offsetof

#include "system/eeiop/snd_3d.h"     // SND_3D_SET, SND_3D_ENV, SndSetListener

#define SE_BANK_DEFAULT     0x1a9b      // file of the always-loaded SE bank (header 0x1a9c)

// SndSeBankLoad results.
#define SE_BANK_READY       0           // loaded and ready
#define SE_BANK_LOADING     -1          // requested, still loading
#define SE_BANK_ERROR       -2          // no such file, no free bank, or the request failed

#define SND_SE_PLAY_ERROR   -3          // SndSePlay: bank not ready or no voice

// System.
void SndSystemInit(void);               // FF3 0x00257ba0
void SndSeReset(void);                  // FF3 0x00257cd0: stop looping SE, default reverb
void SndSystemEnd(void);                // FF3 0x00257cf8
void SndIngameStopAll(int with_default);        // FF3 0x00257d20
void SndSetDefault(void);               // FF3 0x00257d50
int  SndGetFreeVoiceNum(void);          // FF3 0x00257d98
void SndSetVolume(int type, int vol);   // FF3 0x00257db0: group volume, 200 by default
void SndSetMonoMode(int mode);          // FF3 0x00257dc8: 1 mono, else stereo
int  SndConvVolume(int vol);            // FF3 0x00257df0: 0..0x7f -> 0..0x3fff

// SE banks.
void SndSeBankReleaseAll(int with_default);     // FF3 0x00257ae0
int  SndSeBankLoad(int file);           // FF3 0x00257e00: SE_BANK_*; -1 = SE_BANK_DEFAULT
int  SndSeBankIsLoaded(int file);       // FF3 0x00257f50: ready, or not in the table
int  SndSeBankIsEntry(int file);        // FF3 0x00257fc8
int  SndSeBankRelease(int file, int force);     // FF3 0x00258020: 1 when the bank went
int  SndSeBankReleaseAfterPlay(int play_id, int file, int force);   // FF3 0x002580d8

// SE voices (play ids from SndSePlay).
int  SndSePlay(unsigned int se, int vol, int pitch, int time, void *s3s, int loop);    // FF3 0x00258190
int  SndSeAutoPlay(unsigned int se, int vol, int pitch, int time, void *s3s, int loop);// FF3 0x002582a0
void SndSeFadeStop(int id, int time);   // FF3 0x00258340
void SndSeVolFade(int id, int vol, int time);   // FF3 0x00258358
void SndSeFadePitch(int id, int pitch, int time);       // FF3 0x00258398
void SndSeSetPosition(int id, float *pos);      // FF3 0x002583b0
int  SndSeIsPlaying(int id);            // FF3 0x002583c8
void SndAllBufStop(void);               // FF3 0x002583e0: every SE voice
int  SndSeIsLoop(unsigned int se);      // FF3 0x002583f8

// BGM (slot 0 or 1).
int  SndBgmPlay(int file, int vol, int pitch, int time, void *s3s, int flag, int slot);   // FF3 0x00258468
void SndBgmStop(int time, int slot);    // FF3 0x002585d8
void SndBgmStopAll(void);               // FF3 0x00258620
int  SndBgmPause(int slot);             // FF3 0x00258670
int  SndBgmRestart(int slot);           // FF3 0x00258698
int  SndBgmIsActive(int slot);          // FF3 0x002586c0
int  SndBgmIsAllIdle(void);             // FF3 0x002586e8
int  SndBgmPreload(int file, int vol, int pitch, int time, void *s3s, int flag, int slot);// FF3 0x00258700
int  SndBgmIsPreloaded(int slot);       // FF3 0x00258870
int  SndBgmStartPreloaded(int slot);    // FF3 0x00258898
void SndBgmVolFade(int vol, int time, int slot);        // FF3 0x002588c0
void SndBgmFadePitch(int pitch, int time, int slot);    // FF3 0x00258948
void SndBgmSetSpeed(float rate, int slot);      // FF3 0x002589c0
int  SndBgmIsFileSet(int file);         // FF3 0x00258a20

#endif /* _INGAME_SOUND_SND_CTRL_H */
