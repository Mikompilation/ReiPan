/* ==========================================================================
 *  ingame/candle_ctrl.h
 *
 *  The HUD candle of the dream world (FF3-only; FF3 0x13b320..0x13dba8 plus
 *  the linkonce getters / destructors 0x2acfb8..0x2ad3e0). One object, RTTI
 *  CandleControl : IEventGraph2D (GetCandleControl, bss 0x41e540), holds the
 *  candle's power (0..100, burnt by CandleControl::Main, set by the event
 *  commands SET_CANDLE_POWER / SET_CANDLE_ENABLE, GO_DREAM_WORLD /
 *  GO_REAL_WORLD) and draws it from CANDLETEXTURE_PK4 (0x851): seven sprites
 *  per layer from member 'grade' (CandleControl::GetGrade: 0 full .. 3 low,
 *  4 out). A state object (RTTI CandleAnimeStatus and six subclasses, one
 *  function-local static each) animates grade changes: Normal (flicker,
 *  checks the grade), Change (cross-fade to the new grade), Fade (the flame
 *  dies), FadeWait (out: the world turns monochrome through phase 48),
 *  Lighting (relit) and DreamIn (GO_DREAM_WORLD). The flame's fire
 *  screen effect (SetEffects 2) follows the fire level.
 *  Earlier sessions called the class AlbumViewCtrl (a wrong FF2 match).
 * ======================================================================== */
#ifndef _INGAME_CANDLE_CTRL_H
#define _INGAME_CANDLE_CTRL_H

#include <stddef.h>                     // offsetof
#include "sdk/scetypes.h"

#define CANDLE_GRADE_OUT    4           // CandleControl::GetGrade: power <= 0

class CandleAnimeStatus;
class CandleControl;

// FF3 RTTI IEventGraph2D (vtable 0x341538): a 2D graphic loaded into a
// system-heap buffer and sent to VRAM on demand. EE size 0xc.
// layout: host -- the vptr (EE 4 bytes) and data (an EE pointer at +8) are
// 8 bytes here.
class IEventGraph2D
{
public:
    virtual ~IEventGraph2D();                   // EE vtable +0x00 0x002ad320, deleting +0x04 0x002ad348
    virtual int TexSend(int no);                // EE vtable +0x08 0x0013cae8 (IEventGraph2DTexSend)

    /* 0x04 */ int   loaded;                    // 1: data holds the whole file
    /* 0x08 */ void *data;                      // system-heap buffer, nullptr: none

protected:
    void DataFree();                            // FF3 0x0013c9e8
};

// FF3 RTTI CandleControl : IEventGraph2D (vtable 0x341520), EE size 0x24.
// Save block 18 (CandleControlSaveGet) is the 0x10 bytes enable..power.
// layout: host -- after IEventGraph2D's host fields; EE offsets in the comments.
class CandleControl : public IEventGraph2D
{
public:
    ~CandleControl() override;                  // EE vtable +0x00 0x002ad380, deleting +0x04 0x002ad3a8

    int DataLoad();                             // FF3 0x0013ca40: CANDLETEXTURE_PK4; 1 when loaded
    void Init();                                // FF3 0x0013cb48: grade 4, the rest 0
    void Term();                                // FF3 0x0013cb70: data freed, then Init
    void SetEnable(int enable);                 // FF3 0x0013cbb8: SET_CANDLE_ENABLE
    void SetLit(int lit);                       // FF3 0x0013ccb8: GO_REAL_WORLD: 0
    void Reset();                               // FF3 0x0013ccc0: GO_DREAM_WORLD: power 100, DreamIn
    void LoadRestore();                         // FF3 0x0013cd30: after a save is loaded
    int SetPower(float power);                  // FF3 0x0013cdd8: clamped to 0..100; 1 when clamped
    int AddPower(float d);                      // FF3 0x0013ce18
    int SubPower(float d);                      // FF3 0x0013ce38
    void Main();                                // FF3 0x0013ce58: IngameMain
    void UpdateGrade();                         // FF3 0x0013cf00
    int GetGrade();                             // FF3 0x0013cf28: 0..4 from power
    int IsOut();                                // FF3 0x0013cf90: active and power <= 0
    void SetState(CandleAnimeStatus *state);    // FF3 0x0013cfe8
    void Draw();                                // FF3 0x0013d068: IngameDrawSub

    /* 0x0c */ class CandleAnimeStatus *state;  // current animation state, nullptr before the first
    /* 0x10 */ int   enable;                    // SET_CANDLE_ENABLE
    /* 0x14 */ int   lit;                       // 1 in the dream world (GO_DREAM_WORLD), 0 in the real one
    /* 0x18 */ int   out;                       // 1: went out, the world is monochrome (phase 48)
    /* 0x1c */ float power;                     // 0..100
    /* 0x20 */ int   grade;                     // grade drawn on layer 0 (the old one while changing)

private:
    void FurnReset();                           // FF3 0x0013cb98
    int IsActive();                             // FF3 0x0013cfc8
};
static_assert(offsetof(IEventGraph2D, loaded) == sizeof(void *) + 0x0, "IEventGraph2D.loaded (EE 0x04)");
static_assert(offsetof(IEventGraph2D, data) == offsetof(IEventGraph2D, loaded) + sizeof(void *),
              "IEventGraph2D.data (EE 0x08)");
static_assert(offsetof(CandleControl, lit) == offsetof(CandleControl, enable) + 0x4, "CandleControl.lit (EE 0x14)");
static_assert(offsetof(CandleControl, out) == offsetof(CandleControl, enable) + 0x8, "CandleControl.out (EE 0x18)");
static_assert(offsetof(CandleControl, power) == offsetof(CandleControl, enable) + 0xc, "CandleControl.power (EE 0x1c)");
static_assert(offsetof(CandleControl, grade) == offsetof(CandleControl, enable) + 0x10, "CandleControl.grade (EE 0x20)");

CandleControl *GetCandleControl(void);                      // FF3 0x002ad278

int   CandleControlSaveGet(void **addr);                    // FF3 0x0013c938: {ctrl + 0x10, 0x10}

// Phase 48 (0x30): the switch to and from the monochrome world.
int   init_Story_CandleMono(void);                          // FF3 0x0013d5a8
int   one_Story_CandleMono(void);                           // FF3 0x0013d678
int   end_Story_CandleMono(void);                           // FF3 0x0013d700

#endif /* _INGAME_CANDLE_CTRL_H */
