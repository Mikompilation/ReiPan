/* ==========================================================================
 *  ingame/event/telephone.c
 *
 *  The telephone (see telephone.h). FF3-only, FF3 0x25bae0..0x25c300.
 *
 *  Frames per second for the event script's seconds are 25 (PAL) / 30
 *  (GetPALMode), as everywhere in FF3's event code.
 * ======================================================================== */
#include "ingame/event/telephone.h"

#include <new>                                  // placement new on FF3's C++ heap (operator_new)

#include "common/cpp_heap.h"                    // operator_new, operator_delete
#include "ingame/event/tel_talk.h"              // TelTalkScript
#include "ingame/plyr/player.h"                 // GetPlyrAreaNo
#include "ingame/sound/snd_ctrl.h"              // SndSePlay, SndSeFadeStop, SndSeVolFade, SndSeBankIsLoaded
#include "system/eeiop/cd_file_dat.h"           // SYS_REAL_*_BD_BD, TEL_KONSEN_1_STR_STR
#include "system/os/system.h"                   // GetPALMode

#define TEL_SE_RING_NEAR    0x1b7d0001          // SYS_REAL_1_BD sample 1: the ring heard in the phone's room
#define TEL_SE_RING_FAR     0x1b7d0000          // SYS_REAL_1_BD sample 0: the ring heard from further away
#define TEL_SE_TALK_END     0x1bf70004          // SYS_REAL_3_BD sample 4: the end of a call
#define TEL_SE_PITCH        0x1000
#define TEL_RING_STOP_TIME  10                  // frames of the ring's fade-out
#define TEL_RING_VOL_TIME   20                  // frames of a level change's fade
#define TEL_TALK_VOL        0x3200              // the call's voice stream volume
#define TEL_TALK_TIME       5                   // its fade-in / the map BGM's duck time
#define TEL_TALK_SUB_MODE   2                   // TelTalkScript::Work's arguments from TelTalkMain
#define TEL_TALK_WIN_MODE   0x80

static TEL_RING_SE_WRK tel_se;                  // bss 0x3fd090
static TEL_RING_WRK    tel_ring;                // bss 0x3fd0d0
static TEL_SAVE        tel_save;                // bss 0x3fd0e8
static u_char          tel_status;              // sdata 0x3739c8 (gp+0x4d8), TEL_ST_*
static TelTalkScript  *tel_talk;                // sdata 0x3739d0 (gp+0x4e0), null: no call

// The phone's position (sdata 0x372698).
static const sceVu0FVECTOR tel_phone_pos = { 84.3f, -17.1f, 230.8f, 1.0f };

// Ring volumes per level (data 0x3631f0): { near SE, far SE }.
static const int tel_ring_vol[4][2] =
{
    {   0,   0 },                               // 0: not heard
    {   0,  50 },                               // 1: far, quiet
    {   0, 100 },                               // 2: far
    { 100,   0 },                               // 3: near
};

// The two calls (data 0x363220 / 0x363250). The {-1, -1} after the end
// record is in the ROM; Work stops at TEL_TALK_CMD_END.
static const TEL_TALK_CMD tel_talk_script0[] =
{
    { TEL_TALK_CMD_WIN,  0 }, { TEL_TALK_CMD_SUB, 1 }, { TEL_TALK_CMD_WIN, 2 },
    { TEL_TALK_CMD_WAIT, 30 }, { TEL_TALK_CMD_END, -1 }, { -1, -1 },
};
static const TEL_TALK_CMD tel_talk_script1[] =
{
    { TEL_TALK_CMD_WIN,  0 }, { TEL_TALK_CMD_WIN, 1 }, { TEL_TALK_CMD_WIN, 2 },
    { TEL_TALK_CMD_WIN,  3 }, { TEL_TALK_CMD_WAIT, 30 }, { TEL_TALK_CMD_END, -1 }, { -1, -1 },
};

// sdata 0x3726a8.
static const TEL_TALK_CMD *const tel_talk_script[] = { tel_talk_script0, tel_talk_script1 };

// TEL_TALK_START numbers (data 0x363210): both play TEL_KONSEN_1_STR.
static const TEL_TALK_TBL tel_talk_tbl[] =
{
    { TEL_KONSEN_1_STR_STR, 0 },
    { TEL_KONSEN_1_STR_STR, 1 },
};

static int  TelRingGetAreaLevel(int area);
static void TelRingInit(void);
static void TelRingBankReq(void);
static int  TelRingBankIsReady(void);
static void TelRingBankEnd(void);
static void TelRingSePlay(void);
static void TelRingSeStop(void);
static void TelRingSeVolUpdate(void);
static void TelSetStatus(u_char status);
static void TelRingStart(int sec);
static int  TelRingMain(void);
static void TelRingReq(int ring, int pause);
static int  TelRingCtrl(int ring);

// How loud the ring is in an area: 3 the phone's own areas, 1 / 2 the
// areas around them, 0 everywhere else (and -1).
// FF3 0x0025bae0
static int TelRingGetAreaLevel(int area)
{
    switch (area)
    {
    case 0x2f:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x38:
    case 0x39:
    case 0x3a:
        return 1;
    case 0x30:
    case 0x36:
        return 3;
    case 0x31:
    case 0x37:
        return 2;
    default:
        return 0;
    }
}

// TEL_VOL_OFF (-1: level 0) / TEL_VOL_ON (the player's area).
// FF3 0x0025bb28
void TelRingSetArea(int area)
{
    tel_se.level = TelRingGetAreaLevel(area);
}

// FF3 0x0025bb50
void TelRingRaiseArea(int area)
{
    int level;

    if (tel_se.level != -1)
    {
        level = TelRingGetAreaLevel(area);
        if (tel_se.level < level)
            tel_se.level = level;
    }
}

// FF3 0x0025bba0
void TelRingLowerArea(int area)
{
    int level;

    if (tel_se.level != -1)
    {
        level = TelRingGetAreaLevel(area);
        if (level < tel_se.level)
            tel_se.level = level;
    }
}

// FF3 0x0025bbf0
static void TelRingInit(void)
{
    tel_se.level     = -1;
    tel_se.s3s.pos   = tel_se.pos;
    tel_se.bank      = TEL_BANK_NONE;
    tel_se.se_far    = -1;
    tel_se.se_near   = -1;
    tel_se.level_set = -1;
    tel_se.pos[0] = tel_phone_pos[0];
    tel_se.pos[1] = tel_phone_pos[1];
    tel_se.pos[2] = tel_phone_pos[2];
    tel_se.pos[3] = tel_phone_pos[3];
}

// FF3 0x0025bc60
static void TelRingBankReq(void)
{
    if (tel_se.bank == TEL_BANK_NONE)
        tel_se.bank = TEL_BANK_WAIT;
}

// The banks are loaded by the room / event code (SndSeBankLoad); this only
// waits for them.
// FF3 0x0025bc80
static int TelRingBankIsReady(void)
{
    if (tel_se.bank == TEL_BANK_OK)
        return 1;
    if (tel_se.bank != TEL_BANK_WAIT)
        return 0;

    if (SndSeBankIsLoaded(SYS_REAL_1_BD_BD) == 0 || SndSeBankIsLoaded(SYS_REAL_2_BD_BD) == 0
        || SndSeBankIsLoaded(SYS_REAL_3_BD_BD) == 0)
        return 0;

    tel_se.bank = TEL_BANK_OK;
    return 1;
}

// FF3 0x0025bd08
static void TelRingBankEnd(void)
{
    if (tel_se.bank != TEL_BANK_NONE)
        tel_se.bank = TEL_BANK_NONE;
}

// FF3 0x0025bd28
static void TelRingSePlay(void)
{
    tel_se.se_near = SndSePlay(TEL_SE_RING_NEAR, tel_ring_vol[tel_se.level][0], TEL_SE_PITCH, 0, &tel_se.s3s, 0);
    tel_se.se_far  = SndSePlay(TEL_SE_RING_FAR, tel_ring_vol[tel_se.level][1], TEL_SE_PITCH, 0, &tel_se.s3s, 0);
    tel_se.level_set = tel_se.level;
}

// FF3 0x0025bdc0
static void TelRingSeStop(void)
{
    SndSeFadeStop(tel_se.se_near, TEL_RING_STOP_TIME);
    SndSeFadeStop(tel_se.se_far, TEL_RING_STOP_TIME);
}

// FF3 0x0025bdf8
static void TelRingSeVolUpdate(void)
{
    if (tel_se.level_set != tel_se.level)
    {
        SndSeVolFade(tel_se.se_near, tel_ring_vol[tel_se.level][0], TEL_RING_VOL_TIME);
        SndSeVolFade(tel_se.se_far, tel_ring_vol[tel_se.level][1], TEL_RING_VOL_TIME);
        tel_se.level_set = tel_se.level;
    }
}

// The call's last record (TelTalkScript, talk command 3).
// FF3 0x0025be78
void TelTalkEndSePlay(void)
{
    SndSePlay(TEL_SE_TALK_END, 100, TEL_SE_PITCH, 0, &tel_se.s3s, 0);
}

// FF3 0x0025beb0
static void TelSetStatus(u_char status)
{
    tel_status = status;
}

// TEL_STATUS.
// FF3 0x0025bec0
int TelStatusCheck(u_char mask)
{
    return (tel_status & mask) != 0;
}

// One ring of sec seconds; 0 rings until TEL_STOP (the count then only
// toggles between 1 and 0, so the SEs start once).
// FF3 0x0025bed8
static void TelRingStart(int sec)
{
    tel_ring.len = sec * (GetPALMode() != 0 ? 25 : 30);
    tel_ring.cnt = sec * (GetPALMode() != 0 ? 25 : 30);
    if (sec == 0)
    {
        tel_ring.cnt = 2;
        tel_ring.len = 2;
    }
    tel_ring.endless = sec == 0;
    TelSetStatus(TEL_ST_RING);
}

// 1 once the ring is over (the frame after its SEs were stopped).
// FF3 0x0025bf60
static int TelRingMain(void)
{
    if (tel_ring.len == 0)
        return 1;

    if (tel_ring.cnt == 0)
    {
        if (tel_ring.endless == 0)
        {
            TelRingSeStop();
            tel_ring.len = 0;
            return 0;
        }
        tel_ring.cnt = 1;
    }
    else
    {
        if (tel_ring.cnt == tel_ring.len)
        {
            if (TelRingBankIsReady() == 0)
                return 0;
            TelRingSePlay();
        }
        tel_ring.cnt--;
    }

    TelRingSeVolUpdate();
    return 0;
}

// FF3 0x0025bff8
static void TelRingReq(int ring, int pause)
{
    tel_ring.pause     = pause;
    tel_ring.pause_cnt = 0;
    TelRingSetArea(GetPlyrAreaNo());
    TelRingStart(ring);
}

// Ring, pause, ring again; 1 when a ring is over and there is no pause.
// FF3 0x0025c038
static int TelRingCtrl(int ring)
{
    if (tel_ring.pause_cnt != 0)
    {
        if (--tel_ring.pause_cnt == 0)
            TelRingStart(ring);
        return 0;
    }

    if (TelRingMain() == 0)
        return 0;
    if (tel_ring.pause == 0)
        return 1;

    tel_ring.pause_cnt = tel_ring.pause * (GetPALMode() != 0 ? 25 : 30);
    TelSetStatus(TEL_ST_PAUSE);
    return 0;
}

// IngameInit (and TelStop).
// FF3 0x0025c0d0
void TelInit(void)
{
    tel_save.ring   = 0;
    tel_save.active = 0;
    tel_save.pause  = 0;
    TelRingInit();
    TelSetStatus(TEL_ST_IDLE);
}

// After a load: tel_save came from the save data.
// FF3 0x0025c100
void TelLoadRestore(void)
{
    if (tel_save.active != 0)
        TelStart(tel_save.ring, tel_save.pause);
}

// TEL_START ring pause.
// FF3 0x0025c138
void TelStart(int ring, int pause)
{
    tel_save.active = 1;
    tel_save.ring   = ring;
    tel_save.pause  = pause;
    TelRingBankReq();
    TelRingReq(tel_save.ring, tel_save.pause);
}

// IngameMain.
// FF3 0x0025c180
int TelMain(void)
{
    if (tel_save.active == 0)
        return 0;
    if (TelRingCtrl(tel_save.ring) == 0)
        return 0;

    TelStop();
    return 1;
}

// TEL_STOP.
// FF3 0x0025c1c8
void TelStop(void)
{
    TelRingSeStop();
    TelRingBankEnd();
    TelInit();
}

// Save data row 42.
// FF3 0x0025c1f0
int TelGetSaveData(void **addr)
{
    *addr = &tel_save;
    return sizeof(TEL_SAVE);
}

// TEL_TALK_STOP / IngameInit. FF3 drops the pointer without deleting the
// object (a running call's voice keeps playing); kept.
// FF3 0x0025c208
void TelTalkClear(void)
{
    tel_talk = NULL;
}

// TEL_TALK_START no. A second start while a call runs stops that call
// (the object stays until TelTalkMain sees it end).
// FF3 0x0025c210
void TelTalkStart(int no)
{
    if (tel_talk != NULL)
    {
        tel_talk->Stop();
        return;
    }

    // FF3: operator new(0x40), then TelTalkScriptInit -- the constructor here.
    tel_talk = new (operator_new(sizeof(TelTalkScript))) TelTalkScript;
    tel_talk->Start(tel_talk_script[tel_talk_tbl[no].script], tel_talk_tbl[no].file, TEL_TALK_VOL, TEL_TALK_TIME,
                    NULL);
}

// IngameMain.
// FF3 0x0025c2a8
int TelTalkMain(void)
{
    if (tel_talk == NULL)
        return 1;
    if (tel_talk->Work(TEL_TALK_SUB_MODE, TEL_TALK_WIN_MODE) == 0)
        return 0;

    operator_delete(tel_talk);                  // trivial destructor: FF3 calls operator delete only
    tel_talk = NULL;
    return 1;
}

// TEL_TALK_END.
// FF3 0x0025c2f0
int TelTalkIsEnd(void)
{
    return tel_talk == NULL;
}
