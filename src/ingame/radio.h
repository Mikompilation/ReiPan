/* ==========================================================================
 *  ingame/radio.h
 *
 *  The radios (FF3-only, FF3 0x24cf18..0x24da20; no FF2 code). The event
 *  script switches radios on and off (RADIO_TURN_ON / RADIO_TURN_OFF, ids
 *  0..18 of the table at data 0x362060); up to six play at once. A playing
 *  radio loops its broadcast SE and the static-noise SE 0x1b750000 at a
 *  fixed point of one world (ingame_wrk.b03), louder the nearer the player
 *  hears it from and the more they face it; the loudest one shows its
 *  caption at the bottom of the screen (MovieTitleDispMain). A radio of the
 *  other world, or every radio under lock 0x1a (SET_RADIO_LOCK_FLG), is
 *  suspended: its voices stop and its banks are released. The ids of the
 *  playing radios are saved (save_game_data entry 59).
 *
 *  Earlier sessions took the module for savepoint_main.c (FF2 low matches:
 *  SavePointMainInit / SavePointMain); FF3's save points live elsewhere.
 * ======================================================================== */
#ifndef _INGAME_RADIO_H
#define _INGAME_RADIO_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"
#include "sdk/libvu0.h"                 // sceVu0FVECTOR

#define RADIO_DAT_NUM   19              // rows of radio_dat (RADIO_TURN_ON takes no < 19)
#define RADIO_SLOT_NUM  6               // radios on at once

// One radio of the table (data 0x362060, 0x20 bytes; no pointers).
struct RADIO_DAT
{
    /* 0x00 */ short         no;        // its own row number
    /* 0x02 */ short         caption;   // MovieTitleDispMain's message (bank 0x7c), -1: none
    /* 0x04 */ int           se;        // broadcast SE (bank << 16 | no), -1: none
    /* 0x08 */ int           world;     // ingame_wrk.b03 it plays in
    /* 0x0c */ float         range;     // silent from this distance on
    /* 0x10 */ sceVu0FVECTOR pos;
};
static_assert(sizeof(RADIO_DAT) == 0x20, "RADIO_DAT is 0x20 bytes");
static_assert(offsetof(RADIO_DAT, world) == 0x08, "RADIO_DAT.world");
static_assert(offsetof(RADIO_DAT, pos) == 0x10, "RADIO_DAT.pos");

// A playing radio (operator new(0x20); no vtable, no pointers).
class Radio
{
public:
    explicit Radio(short no);                       // FF3 0x0024cf60 (base-object copy 0x0024cf18)
    ~Radio();                                       // FF3 0x0024cfc0

    void  Suspend();                                // FF3 0x0024d0a0
    void  Resume();                                 // FF3 0x0024d0d0
    float CalcDistRate(const float *listen);        // FF3 0x0024d0f8
    float CalcFaceRate(const float *listen, const float *rot);  // FF3 0x0024d1a8
    void  Play(int loudest);                        // FF3 0x0024d298
    void  DrawCaption();                            // FF3 0x0024d408

    /* 0x00 */ short no;                // row of radio_dat
    /* 0x04 */ int   voice;             // the broadcast's SndSePlay id, -1: none
    /* 0x08 */ int   noise;             // the static's SndSePlay id, -1: none
    /* 0x0c */ float dist_rate;         // 1 at the radio .. 0 at its range
    /* 0x10 */ float face_rate;         // 0.5 facing away .. 1 facing it
    /* 0x14 */ float vol;               // dist_rate * face_rate
    /* 0x18 */ int   asleep;            // 1: suspended, banks released
    /* 0x1c */ int   loudest;           // 1: shows its caption

private:
    void BankLoad();                                // FF3 0x0024cfd8
    void BankRelease();                             // FF3 0x0024d018
};
static_assert(sizeof(Radio) == 0x20, "Radio is 0x20 bytes on the EE");
static_assert(offsetof(Radio, voice) == 0x04, "Radio.voice");
static_assert(offsetof(Radio, dist_rate) == 0x0c, "Radio.dist_rate");
static_assert(offsetof(Radio, vol) == 0x14, "Radio.vol");
static_assert(offsetof(Radio, asleep) == 0x18, "Radio.asleep");
static_assert(offsetof(Radio, loudest) == 0x1c, "Radio.loudest");

void RadioInit(void);                   // FF3 0x0024d4c8: IngameInit
void RadioEnd(void);                    // FF3 0x0024d518: IngameEnd
void RadioMain(void);                   // FF3 0x0024d590: IngameMain
void RadioDraw(void);                   // FF3 0x0024d718: IngameDraw
int  RadioTurnOn(short no);             // FF3 0x0024d7f0: RADIO_TURN_ON; 1 when created
void RadioTurnOff(short no);            // FF3 0x0024d890: RADIO_TURN_OFF; no < 0: all
void RadioSaveStore(void);              // FF3 0x0024d928: IngameSaveInit
void RadioLoadRestore(void);            // FF3 0x0024d978: IngameLoadRestore
int  RadioSaveGet(void **addr);         // FF3 0x0024d9d0: {0x3f7a68, 0xc}

#endif /* _INGAME_RADIO_H */
