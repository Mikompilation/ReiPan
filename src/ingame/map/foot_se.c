// ingame/map/foot_se.c -- footstep sound banks.
// FF3 0x001c7cb8..0x001c83d0 (+ foot_seGetWrkNo4FileNo 0x002ba500 in the
// linkonce run); FF2 foot_se.c (MioPan port for the slot code).
//
// FF3 against FF2: the ELF's per-room surface table and foot_se_label_tbl are
// gone. A room's footstep sounds are the se_ numbers in its rcb
// (RcbGetSeNoList), sound n is the file pair FOOT_00_0_DOMA_BD_BD + 2n
// (body) / + 2n + 1 (HXD header), and FootSeInit records which of the 51
// pairs are on the disc. foot_seSetRoom merges the lists of both room
// buffers; foot_sePlay takes the file number (FootSePlayFloor looks it up
// from the floor under the foot), and the draws from rand() changed slightly.

#include "ingame/map/foot_se.h"

#include <string.h>                             // memset

#include "ingame/ingame_effect.h"               // IgEffectFootStepReq
#include "ingame/ingame_wrk.h"                  // ingame_wrk.plyr_wrk
#include "ingame/map/MapLoad.h"                 // MapLoadGetFreeMemAddr, MapLoadGetRoomData, MapLoadGetBuffID
#include "ingame/map/MapRcb.h"                  // RcbGetSeNoList
#include "ingame/map/map_rectangle.h"           // MrecSetSEInfo, MrecGetSeNo
#include "ingame/plyr/player.h"                 // PLYR_WRK.floor
#include "ingame/story_door.h"                  // MapDoorModeIsMove, MapDoorAreaAtPos, MapDoorRoomMhGet
#include "reipan/os/reipan_rand.h"              // ReiPan_Rand (the ROM's rand)
#include "system/eeiop/cd_file_dat.h"           // FOOT_00_0_DOMA_BD_BD
#include "system/eeiop/cddat.h"                 // GetFileSize
#include "system/eeiop/snd_3d.h"                // SND_3D_SET
#include "system/eeiop/snd_bank.h"              // SndBank*

// Resident bank slots (FF2 WRK_MAX).
#define WRK_MAX             12

// Footstep sound numbers 0..50: FOOT_00 .. FOOT_50 on the disc.
#define FOOT_SE_NUM         51

// The rcb's "no footstep" sound number.
#define FOOT_SE_NONE        99

// The room member that holds the rcb (MapLoadGetRoomData).
#define FOOT_SE_RCB_MEMBER  3

// The footstep that also raises a water ripple (IgEffectFootStepReq).
static_assert(FOOT_00_0_DOMA_BD_BD + 2 * 44 == FOOT_44_0_SUIMEN_BD_BD, "FOOT file pairs");

// One resident bank each; -1: free (bss 0x3eb2e0).
static int foot_se_manage[WRK_MAX];

// Bit n: FOOT_nn's body file has a size on this disc (sbss gp-0x248 /
// gp-0x244; zeroed by FootSe_static_init 0x1c8388, filled by FootSeInit).
static u_int foot_se_exist[2];

// ──────────────────────────────────────────────────────────────────────
// Sound number -> body file number; -2 for the "none" number, -1 for one
// out of range or not on the disc. Inlined at both callers (FF2's
// foot_seGetFileNo4Label).

static int foot_seGetFileNo4SeNo(int no)
{
    if (no == FOOT_SE_NONE)
    {
        return -2;
    }

    if ((u_int)no >= FOOT_SE_NUM || (foot_se_exist[no >> 5] & (1u << (no & 0x1f))) == 0)
    {
        return -1;
    }

    return FOOT_00_0_DOMA_BD_BD + no * 2;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7cb8. Appends file_no to list unless it is already there; the
// new count.

static int foot_seAddFileNo(int file_no, int *list, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (list[i] == file_no)
        {
            return count;
        }
    }

    list[count] = file_no;
    return count + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7d00. Merges the footstep files of room buffer room (0 / 1) into
// list (count entries so far); the new count. Nothing bounds list: the rcb's
// list of at most 8 sounds per room keeps two rooms under WRK_MAX.

static int foot_seGetSeStat(int room, int *list, int count)
{
    int se_no[WRK_MAX];

    MAP_ROOM_DAT *dat = MapLoadGetFreeMemAddr(room);
    if (dat == nullptr)
    {
        return count;
    }

    void *rcb = MapLoadGetRoomData(dat, FOOT_SE_RCB_MEMBER);
    if (rcb == nullptr)
    {
        return count;
    }

    int num = RcbGetSeNoList(rcb, se_no);
    for (int i = 0; i < num; i++)
    {
        int file_no = foot_seGetFileNo4SeNo(se_no[i]);
        if (file_no >= 0)
        {
            count = foot_seAddFileNo(file_no, list, count);
        }
    }

    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7e08. Called by MapLoadCallFurn once a room is in: both buffers'
// footstep files stay loaded, so the room behind a door keeps its steps.

void foot_seSetRoom(void)
{
    int list[WRK_MAX];

    int count = foot_seGetSeStat(0, list, 0);
    count = foot_seGetSeStat(1, list, count);
    foot_seSetNewFiles(list, count);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7e48. IngameInit / IngameNewGameReset.

void FootSeInit(void)
{
    for (int i = 0; i < WRK_MAX; i++)
    {
        foot_se_manage[i] = -1;
    }

    foot_se_exist[0] = 0;
    foot_se_exist[1] = 0;

    for (int no = 0; no < FOOT_SE_NUM; no++)
    {
        int file_no = FOOT_00_0_DOMA_BD_BD + no * 2;

        // Labels have no size ((u_int)-1); a 0 size is an empty slot.
        if (GetFileSize(file_no) != (u_int)-1 && GetFileSize(file_no) != 0)
        {
            foot_se_exist[no >> 5] |= 1u << (no & 0x1f);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7f18. Frees one slot.

static void foot_seReleaseWrk(int *bank_id)
{
    if (*bank_id >= 0)
    {
        SndBankRelease(*bank_id);
        *bank_id = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7f58. EvGoRealWorld, GameSystemEnd, IngameEnd, IngameNewGameReset.

void foot_seRelease(void)
{
    for (int i = 0; i < WRK_MAX; i++)
    {
        foot_seReleaseWrk(&foot_se_manage[i]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x002ba500 (out of line in the linkonce run). The slot whose bank
// holds file_no, -1.

static int foot_seGetWrkNo4FileNo(int file_no)
{
    for (int i = 0; i < WRK_MAX; i++)
    {
        int no;

        if (foot_se_manage[i] >= 0 &&
            SndBankGetFileNo(foot_se_manage[i], &no) == SND_BANK_OK &&
            file_no == no)
        {
            return i;
        }
    }

    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c7fa0. Re-cuts the slots to hold exactly file_array: marks what
// is loaded, releases what nothing wants, then claims a slot per missing
// file. Stops when the sound system has no free bank left.

void foot_seSetNewFiles(const int *file_array, int file_num)
{
    int need_flg[WRK_MAX];
    int exist_flg[WRK_MAX];

    memset(need_flg, 0, sizeof(need_flg));
    memset(exist_flg, 0, sizeof(exist_flg));

    for (int i = 0; i < file_num; i++)
    {
        int no = foot_seGetWrkNo4FileNo(file_array[i]);
        if (no != -1)
        {
            need_flg[no] = 1;
            exist_flg[i] = 1;
        }
    }

    for (int i = 0; i < WRK_MAX; i++)
    {
        if (need_flg[i] == 0)
        {
            foot_seReleaseWrk(&foot_se_manage[i]);
        }
    }

    for (int i = 0; i < file_num; i++)
    {
        if (file_array[i] < 0 || exist_flg[i] != 0)
        {
            continue;
        }

        if (SndBankGetFreeBankNum() == 0)
        {
            return;
        }

        int *slot = nullptr;
        for (int j = 0; j < WRK_MAX; j++)
        {
            if (foot_se_manage[j] == -1)
            {
                slot = &foot_se_manage[j];
                break;
            }
        }

        // The ROM stores through the null slot when all twelve are taken;
        // the rcb lists (8 per room) never get there. Skipped here.
        int bank_id = SndBankNew(file_array[i], file_array[i] + 1, -1);
        if (slot != nullptr)
        {
            *slot = bank_id;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c8120. One footstep of a loaded file at pos. Volume is spread
// over 0.86..1.15 of nominal; pitch, one time in three (bits 7.. of the
// second draw), over 0.86..1.10.
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)

void foot_sePlay(int file_no, float *pos, int vol, int pitch)
{
    int bank_no = -1;

    int no = foot_seGetWrkNo4FileNo(file_no);
    if (no != -1)
    {
        bank_no = foot_se_manage[no];
    }

    if (bank_no < 0)
    {
        return;
    }

    int rnd_vol = ((115 - ReiPan_Rand() % 30) * vol) / 100;

    int r = ReiPan_Rand();
    if ((r >> 7) % 3 == 0)
    {
        pitch = ((110 - r % 25) * pitch) / 100;
    }

    SND_3D_SET set;
    memset(&set, 0, sizeof(set));
    set.pos = pos;

    SndBankPlay(bank_no, 0, 1, 0, rnd_vol, pitch, 0, &set);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c8260. A fixed footstep file (the animation code's ladder step,
// FOOT_02_0_HASHIGO_BD_BD).

void FootSePlayId(int se, float *pos, int vol, int pitch)
{
    foot_sePlay(se, pos, vol, pitch);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001c8278. The footstep of the floor under pos: the footstep
// rectangle of map hit mh_no (while a door moves, the mh of the room on the
// side of the door that holds pos), plus its effect.

void FootSePlayFloor(float *pos, int mh_no, int vol, int pitch)
{
    if (MapDoorModeIsMove())
    {
        int area = MapDoorAreaAtPos(ingame_wrk.plyr_wrk->floor, pos);
        if (area >= 0)
        {
            mh_no = MapDoorRoomMhGet(MapLoadGetBuffID(area));
        }
    }

    MrecSetSEInfo(pos, mh_no);

    int file_no = foot_seGetFileNo4SeNo(MrecGetSeNo());
    if (file_no < 0)
    {
        return;
    }

    IgEffectFootStepReq(pos, file_no);
    foot_sePlay(file_no, pos, vol, pitch);
}
