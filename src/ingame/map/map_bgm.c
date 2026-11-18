/* ==========================================================================
 *  ingame/map/map_bgm.c
 *
 *  Per-room background music (see map_bgm.h). FF3 0x1fa768..0x1fada8,
 *  ported from MioPan's FF2 map_bgm.c and checked against the FF3 code.
 *
 *  As in FF2, map_bgmSave is the per-save block and map_bgmMain() polls
 *  the stream the player's place wants. FF3 differs in what it polls:
 *  map_bgmGetStrFile() reads the room row of map_bgm_tbl (the column
 *  map_bgmSave.list) directly -- the work copy aMapWrkBGMTbl is seeded,
 *  edited by EvMapStreamChange and saved, but nothing in FF3 reads it back
 *  -- sets the row's reverb depth on a room change, and overrides it with
 *  fixed streams: the HUD candle, chapter 0 (snow) and the other side's
 *  places.
 * ======================================================================== */
#include "ingame/map/map_bgm.h"

#include "ingame/candle_ctrl.h"                  // GetCandleControl, CandleControl
#include "ingame/ingame_wrk.h"                  // ingame_wrk, title_start_flg
#include "ingame/plyr/player.h"                 // PLYR_WRK.floor
#include "ingame/story_door.h"          // MapAreaNoGet / SideGet
#include "ingame/map/MapSnap.h"                 // MapSnapGetWrk (the current room row)
#include "system/eeiop/cd_file_dat.h"           // the stream file numbers
#include "system/eeiop/snd.h"                   // SndSetEffect
#include "system/eeiop/stream_auto.h"           // StreamAuto*

// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────

// One row per room / sub-room, five stream columns (only column 0 is used
// on the disc), the reverb depth and 3. Data 0x32a6d8, 125 rows.
static const MAP_BGM_DAT map_bgm_tbl[MAP_BGM_TBL_NUM] =                /* data 32a6d8 */
{
    {   1, 1, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  24, 3 },
    {   2, 1, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  40, 3 },
    {   2, 2, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  32, 3 },
    {   2, 3, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  36, 3 },
    {   3, 1, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  54, 3 },
    {   3, 2, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  48, 3 },
    {   3, 3, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  32, 3 },
    {   3, 4, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  40, 3 },
    {   4, 1, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  24, 3 },
    {   4, 2, { KAN_26_MUON2_STR_STR, -1, -1, -1, -1 },  16, 3 },
    {   5, 1, { KAN_25_MUON1_STR_STR, -1, -1, -1, -1 },  20, 3 },
    {   6, 1, { KAN_00_AME_STR_STR, -1, -1, -1, -1 },  24, 3 },
    {   6, 2, { KAN_SE_14_KAZE2_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 101, 1, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  60, 3 },
    { 101, 2, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 102, 1, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 102, 2, { KAN_36_M_DOMAROUKA_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 103, 1, { KAN_42_KEI3_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 103, 2, { KAN_42_KEI3_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 103, 3, { KAN_42_KEI3_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 103, 4, { KAN_42_KEI3_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 104, 1, { KAN_02_IRORI_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 104, 2, { KAN_02_IRORI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 104, 3, { KAN_02_IRORI_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 104, 4, { KAN_18_KEI2_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 105, 1, { KAN_02_IRORI_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 105, 2, { KAN_10_OSHIRASAMA_STR_STR, -1, -1, -1, -1 },  10, 3 },
    { 105, 3, { KAN_02_IRORI_STR_STR, -1, -1, -1, -1 },  60, 3 },
    { 105, 4, { KAN_SE_14_KAZE2_STR_STR, -1, -1, -1, -1 },  20, 3 },
    { 106, 1, { KAN_03_NAKANIWA1_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 106, 2, { KAN_03_NAKANIWA1_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 106, 3, { KAN_03_NAKANIWA1_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 201, 1, { KAN_04_GENKAN2_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 201, 2, { KAN_04_GENKAN2_STR_STR, -1, -1, -1, -1 },  56, 3 },
    { 201, 3, { KAN_28_H_NAWA_STR_STR, -1, -1, -1, -1 },  36, 3 },
    { 202, 1, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 202, 2, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  24, 3 },
    { 202, 3, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 202, 4, { KAN_17_KEI1_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 202, 5, { KAN_17_KEI1_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 202, 6, { KAN_17_KEI1_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 203, 1, { KAN_05_NAKANIWA2_STR_STR, -1, -1, -1, -1 },  24, 3 },
    { 203, 2, { KAN_05_NAKANIWA2_STR_STR, -1, -1, -1, -1 },  30, 3 },
    { 203, 3, { KAN_05_NAKANIWA2_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 204, 1, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 204, 2, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 204, 3, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 204, 4, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 205, 1, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 205, 2, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 205, 3, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 205, 4, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  72, 3 },
    { 205, 5, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  56, 3 },
    { 205, 6, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 206, 1, { KAN_10_OSHIRASAMA_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 207, 1, { KAN_10_OSHIRASAMA_STR_STR, -1, -1, -1, -1 },  24, 3 },
    { 208, 1, { KAN_10_OSHIRASAMA_STR_STR, -1, -1, -1, -1 },  24, 3 },
    { 209, 1, { KAN_08_TOUSYU_STR_STR, -1, -1, -1, -1 },  56, 3 },
    { 209, 2, { KAN_08_TOUSYU_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 209, 3, { KAN_08_TOUSYU_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 209, 4, { KAN_25_MUON1_STR_STR, -1, -1, -1, -1 },  26, 3 },
    { 210, 1, { KAN_38_MIKOHEYA_STR_STR, -1, -1, -1, -1 },  76, 3 },
    { 210, 2, { KAN_38_MIKOHEYA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 211, 1, { KAN_19_NANIKAIRU_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 211, 2, { KAN_08_TOUSYU_STR_STR, -1, -1, -1, -1 },  56, 3 },
    { 211, 3, { KAN_25_MUON1_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 212, 1, { KAN_19_NANIKAIRU_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 213, 1, { KAN_28_H_NAWA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 301, 1, { KAN_34_M_DOZOU_STR_STR, -1, -1, -1, -1 },  54, 3 },
    { 302, 1, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 303, 1, { KAN_36_M_DOMAROUKA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 303, 2, { KAN_36_M_DOMAROUKA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 303, 3, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 303, 4, { KAN_01_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 304, 1, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 304, 2, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 304, 3, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 305, 1, { KAN_35_M_HIROMA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 305, 2, { KAN_25_MUON1_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 306, 1, { KAN_34_M_DOZOU_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 306, 2, { KAN_34_M_DOZOU_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 306, 3, { KAN_34_M_DOZOU_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 306, 4, { KAN_37_M_FUTAGO_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 307, 1, { KAN_13_YUKASHITA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 307, 2, { KAN_13_YUKASHITA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 307, 3, { KAN_13_YUKASHITA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 307, 4, { KAN_38_MIKOHEYA_STR_STR, -1, -1, -1, -1 },  87, 3 },
    { 307, 5, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 308, 1, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 308, 2, { KAN_11_SHIKIRI_STR_STR, -1, -1, -1, -1 },  24, 3 },
    { 401, 1, { KAN_19_NANIKAIRU_STR_STR, -1, -1, -1, -1 },  28, 3 },
    { 401, 2, { KAN_19_NANIKAIRU_STR_STR, -1, -1, -1, -1 },  28, 3 },
    { 401, 3, { KAN_07_KINPAKU_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 401, 4, { KAN_09_DAIKUROUKA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 401, 5, { KAN_09_DAIKUROUKA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 401, 6, { KAN_19_NANIKAIRU_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 402, 1, { KAN_10_OSHIRASAMA_STR_STR, -1, -1, -1, -1 },  16, 3 },
    { 403, 1, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  76, 3 },
    { 403, 2, { KAN_07_KINPAKU_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 403, 3, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  76, 3 },
    { 403, 4, { KAN_09_DAIKUROUKA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 403, 5, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  76, 3 },
    { 404, 1, { KAN_29_H_HIROMA_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 405, 1, { KAN_28_H_NAWA_STR_STR, -1, -1, -1, -1 },  54, 3 },
    { 406, 1, { KAN_27_H_GENKAN_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 407, 1, { KAN_30_H_IKESU_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 408, 1, { KAN_31_H_ZASHIKIROU_STR_STR, -1, -1, -1, -1 },  40, 3 },
    { 409, 1, { KAN_32_H_NAWADONO_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 410, 1, { KAN_39_MISOGI_STR_STR, -1, -1, -1, -1 },  87, 3 },
    { 501, 1, { KAN_12_OONAKANIWA_STR_STR, -1, -1, -1, -1 },  95, 3 },
    { 502, 1, { KAN_12_OONAKANIWA_STR_STR, -1, -1, -1, -1 },  48, 3 },
    { 502, 2, { KAN_12_OONAKANIWA_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 502, 3, { KAN_12_OONAKANIWA_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 503, 1, { KAN_39_MISOGI_STR_STR, -1, -1, -1, -1 },  80, 3 },
    { 503, 2, { KAN_39_MISOGI_STR_STR, -1, -1, -1, -1 },  72, 3 },
    { 503, 3, { KAN_26_MUON2_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 503, 4, { KAN_39_MISOGI_STR_STR, -1, -1, -1, -1 },  32, 3 },
    { 504, 1, { KAN_14_MIZUROUKA_STR_STR, -1, -1, -1, -1 },  95, 3 },
    { 505, 1, { KAN_15_RASEN_STR_STR, -1, -1, -1, -1 }, 111, 3 },
    { 505, 2, { KAN_15_RASEN_STR_STR, -1, -1, -1, -1 }, 111, 3 },
    { 506, 1, { KAN_14_MIZUROUKA_STR_STR, -1, -1, -1, -1 },  95, 3 },
    { 507, 1, { KAN_16_MIZUHIROMA_STR_STR, -1, -1, -1, -1 }, 127, 3 },
    { 508, 1, { KAN_33_M_KUSABI_STR_STR, -1, -1, -1, -1 },  64, 3 },
    { 509, 1, { KAN_21_YUKI_STR_STR, -1, -1, -1, -1 },  95, 3 },
    { 509, 2, { KAN_21_YUKI_STR_STR, -1, -1, -1, -1 },  95, 3 },
};

static MAP_BGM_SAVE map_bgmSave;                                        /* bss 3f3038 */

// FF3 addresses these from one base (0x3f3238), so they may be one struct.
static int   bgm_act_flg;                                               /* bss 3f3238 */
static int   bgm_play_id;                                               /* bss 3f323c */
static int   now_str_file;                                              /* bss 3f3240 */
static int   bgm_now_sector;                                            /* bss 3f3244 */
static short bgm_rev_room;                                              /* bss 3f3248: the row whose reverb is set */
static short bgm_rev_sub;                                               /* bss 3f324a */

static CandleControl *candle_ctrl;                                      /* sbss gp-0xb90: GetCandleControl's, fetched once */

// ──────────────────────────────────────────────────────────────────────
// The save block: {&map_bgmSave, its size}.
// FF3 0x001fa768.

int map_bgmSetSave(void **addr)
{
    *addr = &map_bgmSave;
    return sizeof(map_bgmSave);
}

// ──────────────────────────────────────────────────────────────────────
// SET_BGM_LIST: the column of map_bgm_tbl played from now on.
// FF3 0x001fa780.

void map_bgmSetList(int list)
{
    map_bgmSave.list = list;
}

// ──────────────────────────────────────────────────────────────────────
// The row of (room, sub); row 0 when none matches.
// FF3 0x001fa790.

static const MAP_BGM_DAT *map_bgmGetTbl(short room, short sub)
{
    const MAP_BGM_DAT *dat = map_bgm_tbl;

    while (dat->room != room || dat->sub != sub)
    {
        dat++;
        if (dat >= map_bgm_tbl + MAP_BGM_TBL_NUM)
        {
            return map_bgm_tbl;
        }
    }
    return dat;
}

// ──────────────────────────────────────────────────────────────────────
// Fresh game: active, nothing playing, column 0, the work table from the
// disc table's column 0.
// FF3 0x001fa7e8.

void map_bgmInit(void)
{
    bgm_act_flg             = 1;
    bgm_now_sector          = 0;
    bgm_rev_sub             = -1;
    now_str_file            = -1;
    bgm_play_id             = -1;
    bgm_rev_room            = -1;
    map_bgmSave.disable_cnt = 0;
    map_bgmSave.list        = 0;

    for (int i = 0; i < MAP_BGM_TBL_NUM; i++)
    {
        map_bgmSave.aMapWrkBGMTbl[i] = map_bgm_tbl[i].str_file[0];
    }
}

// ──────────────────────────────────────────────────────────────────────
// EvMapStreamChange: row no's work entry. Below the first stream -1 (no
// music), past the last stream file the disc table's entry again.
// FF3 0x001fa858.

void map_bgmChangeTbl(int no, int str_file)
{
    if (str_file < KAN_00_AME_STR_STR)
    {
        map_bgmSave.aMapWrkBGMTbl[no] = -1;
    }
    else if (str_file < BGM_END_DMY)
    {
        map_bgmSave.aMapWrkBGMTbl[no] = str_file;
    }
    else
    {
        map_bgmSave.aMapWrkBGMTbl[no] = map_bgm_tbl[no].str_file[map_bgmSave.list];
    }
}

// ──────────────────────────────────────────────────────────────────────
// Starts stream str_file (its header is the next file) from
// bgm_now_sector, fading in over time.
// FF3 0x001fa8d8.

static int map_bgmPlayIn(int str_file, int time)
{
    return StreamAutoPlayNonReset(str_file, str_file + 1, 0x10, 0, 1, 0x3200, time, 0x14,
                                  (SND_3D_SET *)0, bgm_now_sector);
}

// ──────────────────────────────────────────────────────────────────────
// The stream the player's place wants now, -1 for none. On a new room row
// the row's reverb depth goes to core 0 first. The other side (world 1)
// plays fixed streams by area; elsewhere the HUD candle and chapter 0 win
// over the room row. A missing row fades the music out.
// FF3 0x001fa920.

static int map_bgmGetStrFile(void)
{
    MAP_SNAP_WRK *cur      = MapSnapGetWrk();
    int           str_file = -1;

    if (title_start_flg.mode == 2)
    {
        return -1;
    }

    if (cur->room != nullptr)
    {
        short              room = cur->room->room;
        s_char             sub  = cur->room->sub;
        const MAP_BGM_DAT *dat  = map_bgmGetTbl(room, sub);

        str_file = dat->str_file[map_bgmSave.list];
        if (bgm_rev_room != room || bgm_rev_sub != sub)
        {
            bgm_rev_sub  = sub;
            bgm_rev_room = room;
            SndSetEffect(0, (dat->rev_depth * 0x7fff) / 0x7f, 3);
        }
    }

    if (ingame_wrk.b03 == 1 && MapAreaSideGet() == 1)
    {
        short floor = ingame_wrk.plyr_wrk->floor;
        int   area  = MapAreaNoGet();

        if (area == 0x33)
        {
            return KAN_25_MUON1_STR_STR;
        }
        if (area == 0x32 && floor == 0xf)
        {
            return KAN_26_MUON2_STR_STR;
        }
        if (area == 0x34 && floor == 0xf)
        {
            return KAN_SE_14_KAZE2_STR_STR;
        }
        return (u_int)(ingame_wrk.b01 - 6) < 2 ? KAN_07_KINPAKU_STR_STR : KAN_06_YORU_STR_STR;
    }

    if (candle_ctrl == nullptr)
    {
        candle_ctrl = GetCandleControl();
    }
    if (candle_ctrl->enable != 0 && candle_ctrl->lit != 0 && candle_ctrl->out != 0)
    {
        return KAN_20_ROUSOKU_STR_STR;
    }
    if (ingame_wrk.b01 == 0)
    {
        return KAN_21_YUKI_STR_STR;
    }
    if (cur->room == nullptr)
    {
        return -1;
    }
    if (str_file >= 0)
    {
        return str_file;
    }
    map_bgmFadeOut(10, 0);
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Switches to the wanted stream when it changed: the old one fades out
// over 60, the new one starts from sector 0 over 20. 0 when the right one
// already plays, 1 otherwise (also when nothing is wanted).
// FF3 0x001fab18.

static int map_bgmPlaySub(void)
{
    int str_file = map_bgmGetStrFile();

    if (str_file < 0)
    {
        return 1;
    }
    if (now_str_file == str_file)
    {
        return 0;
    }

    StreamAutoFadeOut(bgm_play_id, 0x3c);
    bgm_now_sector = 0;
    now_str_file   = str_file;
    bgm_play_id    = map_bgmPlayIn(str_file, 0x14);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001fab88.

void map_bgmMain(void)
{
    if (bgm_act_flg != 0 && map_bgmSave.disable_cnt == 0)
    {
        map_bgmPlaySub();
    }
}

// ──────────────────────────────────────────────────────────────────────
// vol 0 stops the stream, keeping its sector for map_bgmFadeIn; anything
// else ducks it to vol. Either way map_bgmMain stays quiet until a
// matching map_bgmFadeIn.
// FF3 0x001fabc0.

void map_bgmFadeOut(int time, int vol)
{
    if (bgm_act_flg == 0)
    {
        return;
    }

    map_bgmSave.disable_cnt++;

    if (vol != 0)
    {
        StreamAutoFade(bgm_play_id, vol, time);
        return;
    }
    if (bgm_play_id != -1)
    {
        bgm_now_sector = StreamAutoGetNowSector(bgm_play_id);
        StreamAutoFadeOut(bgm_play_id, time);
        bgm_play_id = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Shuts the room music down until the next map_bgmInit.
// FF3 0x001fac68.

void map_bgmRelease(int time)
{
    StreamAutoFadeOut(bgm_play_id, time);
    bgm_act_flg = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Pays back one map_bgmFadeOut. At zero: a ducked stream comes back to
// full volume; a stopped one is switched (the player may have moved) or
// restarted from its saved sector.
// FF3 0x001faca0.

void map_bgmFadeIn(int time)
{
    if (bgm_act_flg == 0)
    {
        return;
    }

    if (map_bgmSave.disable_cnt >= 1)
    {
        map_bgmSave.disable_cnt--;
        if (map_bgmSave.disable_cnt >= 1)
        {
            return;
        }
    }

    if (bgm_play_id != -1)
    {
        StreamAutoFade(bgm_play_id, 0x3200, time);
        return;
    }
    if (map_bgmPlaySub() == 0)
    {
        bgm_play_id = map_bgmPlayIn(now_str_file, time);
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 while the room stream plays (or is queued).
// FF3 0x001fad40.

int map_bgmIsPlaying(void)
{
    if (bgm_act_flg != 0 && bgm_play_id != -1)
    {
        return StreamAutoIsPlaying(bgm_play_id);
    }
    return 0;
}

// FF3 0x001fad88 is a compiler atexit thunk (CandleControl's destructor
// 0x2ad380 on GetCandleControl's function-local static 0x41e540; every file
// calling that inline accessor gets one); the host runs the static's
// destructor itself.
