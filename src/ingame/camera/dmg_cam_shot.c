/* ==========================================================================
 *  ingame/camera/dmg_cam_shot.c
 *
 *  The damage camera shots, FF3 0x1374d8..0x137cb8 (FF3-only, no FF2
 *  counterpart; see dmg_cam_shot.h). PlyrApproachCameraCtrl (map_camera.c)
 *  calls DmgCamShotInit when a hit or hold starts and DmgCamShotMain every
 *  frame of APPROACH_CAMERA_FLOW_SHOT.
 * ======================================================================== */

#include "ingame/camera/dmg_cam_shot.h"

#include "common/vu0math.h"             // RotLimitChk
#include "graphics/graph3d/gra3d.h"     // gra3dcamGetPosition
#include "ingame/camera/map_camera.h"   // APPROACH_CAMERA
#include "ingame/enemy/ene_main.h"      // ENE_WRK, EneGetJointPos, ENE_STATE_ACT
#include "ingame/ingame_wrk.h"          // ingame_wrk.plyr_wrk
#include "ingame/photo/finder_photo.h"  // GetAlbumSaveDisp
#include "ingame/plyr/player.h"         // PLYR_WRK
#include "ingame/plyr/plyr_gauge.h"     // PlyrGaugeTimeStep
#include "ingame/plyr/plyr_mdl.h"       // PlyrAreaLineHitCheck
#include "ingame/plyr/plyr_sub.h"       // PlyrGetJointPos
#include "ingame/plyr/unit_ctl.h"       // GetMidPos, UnitGetTrgtRot
#include "reipan/os/reipan_rand.h"      // ReiPan_Rand, REIPAN_RAND_MAXF
#include "sdk/libvu0.h"

// The number of table rows (DmgCamShotInit scans 0x57) and of shots one
// kind may gather (the work below holds 16; no kind has more than 9).
#define DMG_CAM_SHOT_DAT_NUM 87
#define DMG_CAM_SHOT_MAX     16

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// The shot table (data 0x2f15d0), by kind; values exactly as in the ROM.
// { kind, pos_follow, pos_base, pos, tgt_follow, tgt_base, tgt }
static DMG_CAM_SHOT_DAT dmg_cam_shot_dat[DMG_CAM_SHOT_DAT_NUM] =
{
    {  0, 0, 0, {  -18.33f, -21.119999f,  -20.31f,     1.0f }, 0, 0, { -10.849999f,  -22.15f,  -13.19f,     1.0f } },   //  0
    {  0, 0, 0, {   -7.62f,  -34.32f,  -13.36f,     1.0f }, 0, 0, {    -1.8f,  -29.65f, -6.1499996f,     1.0f } },   //  1
    {  0, 0, 0, {    9.79f,  -32.75f, -16.189999f,     1.0f }, 0, 0, { 5.5499997f,   -28.9f,   -7.54f,     1.0f } },   //  2
    {  0, 0, 0, {  -14.29f, -21.519999f,   -1.64f,     1.0f }, 0, 0, { -4.4199996f,  -24.33f, -0.14999999f,     1.0f } },   //  3
    {  0, 0, 0, {   18.57f, -22.609999f,    8.37f,     1.0f }, 0, 0, {    9.08f, -23.189999f, 4.2299995f,     1.0f } },   //  4
    {  0, 0, 0, {   -3.29f, -32.629997f,   18.07f,     1.0f }, 0, 0, {   -1.05f,  -28.58f,    8.79f,     1.0f } },   //  5
    {  0, 0, 0, {    16.3f, -31.609999f,   -6.56f,     1.0f }, 0, 0, {    7.74f,  -28.15f, -1.8299999f,     1.0f } },   //  6
    {  1, 0, 0, {   -1.52f, -41.239998f, 2.9899998f,     1.0f }, 0, 0, { 7.5499997f, -4.8599997f,   10.57f,     1.0f } },   //  7
    {  1, 0, 0, {   23.91f, -35.519997f, -20.089998f,     1.0f }, 0, 0, { 4.7799997f,  -13.86f,    4.95f,     1.0f } },   //  8
    {  1, 0, 0, {   45.85f, -16.609999f, -2.4099998f,     1.0f }, 0, 0, { 8.959999f, -15.2699995f, 7.5499997f,     1.0f } },   //  9
    {  1, 0, 0, { 20.919998f, -8.089999f,   38.98f,     1.0f }, 0, 0, {   -2.03f, -15.059999f,    9.19f,     1.0f } },   // 10
    {  1, 0, 0, { -19.529999f,  -27.33f,    36.8f,     1.0f }, 0, 0, {     2.0f,   -13.0f, 8.629999f,     1.0f } },   // 11
    {  1, 0, 0, {   12.78f,   -4.27f,   27.58f,     1.0f }, 0, 0, { -5.8199997f,  -20.72f, -1.4899999f,     1.0f } },   // 12
    {  1, 0, 0, { 19.839998f,   -6.04f,   -8.91f,     1.0f }, 0, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 13
    {  1, 1, 1, {     0.0f,     0.0f,     3.0f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 14
    {  2, 1, 0, { 6.3199997f, -28.039999f, 31.689999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 15
    {  2, 1, 0, {  -16.97f, -31.419998f, 20.689999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 16
    {  2, 1, 0, {    7.56f, -25.869999f, 22.179998f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 17
    {  2, 1, 0, { 5.7599998f,  -43.96f, 22.179998f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 18
    {  2, 0, 0, { 21.099998f, -29.119999f,  -16.48f,     1.0f }, 0, 0, { 5.6099997f, -30.259998f,   -1.06f,     1.0f } },   // 19
    {  2, 0, 0, {  -25.98f,  -39.73f,    18.9f,     1.0f }, 0, 0, { -7.6699996f,  -32.42f,    9.41f,     1.0f } },   // 20
    {  2, 1, 3, {     0.0f,     0.0f,     3.0f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 21
    {  2, 1, 1, {     0.0f,     0.0f,     4.0f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 22
    {  3, 0, 0, {  -10.44f,  -28.66f,  -13.79f,     1.0f }, 0, 0, { -4.5499997f,  -27.96f,   -2.58f,     1.0f } },   // 23
    {  3, 0, 0, { 11.809999f,  -27.82f,  -17.74f,     1.0f }, 0, 0, { 6.0299997f,  -26.57f, -6.5299997f,     1.0f } },   // 24
    {  3, 0, 0, {   10.12f, -40.289997f,   -8.61f,     1.0f }, 0, 0, { 3.9699998f,  -32.03f, -1.2199999f,     1.0f } },   // 25
    {  3, 0, 0, {    8.65f, -37.519997f,   11.99f,     1.0f }, 0, 0, { 1.3399999f,  -30.13f, 4.7299995f,     1.0f } },   // 26
    {  3, 0, 0, {   -7.66f, -31.769999f,   12.82f,     1.0f }, 0, 0, {   -2.61f,  -27.82f,    1.88f,     1.0f } },   // 27
    {  3, 0, 0, {   -8.48f,  -28.83f,  -10.32f,     1.0f }, 0, 0, {    -2.1f,  -29.24f,    0.63f,     1.0f } },   // 28
    {  3, 0, 0, { 10.419999f,   -30.9f, -7.2599998f,     1.0f }, 0, 0, { 1.1999999f, -29.099998f,    1.25f,     1.0f } },   // 29
    {  4, 0, 0, { 20.189999f,  -15.24f, 18.949999f,     1.0f }, 0, 0, { -2.2099998f,  -21.56f,    5.97f,     1.0f } },   // 30
    {  4, 0, 0, {  -14.73f, -21.439999f,   21.25f,     1.0f }, 0, 0, {     5.7f, -22.019999f, 4.1499996f,     1.0f } },   // 31
    {  4, 0, 0, {   -20.9f, -29.539999f,   -6.75f,     1.0f }, 0, 0, { 0.90999997f, -23.199999f, 7.1899996f,     1.0f } },   // 32
    {  4, 0, 0, {   31.05f,  -16.38f, 2.4799998f,     1.0f }, 1, 5, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 33
    {  4, 0, 0, { 21.679998f,  -34.16f, 3.9499998f,     1.0f }, 0, 0, { -1.9699999f,  -22.06f,    5.99f,     1.0f } },   // 34
    {  4, 0, 0, { 16.789999f,  -29.46f, 38.539997f,     1.0f }, 0, 0, { 5.9199996f, -22.779999f, 15.139999f,     1.0f } },   // 35
    {  4, 0, 0, { 15.669999f, -27.269999f,  -11.32f,     1.0f }, 0, 0, {   -1.25f, -22.869999f,    8.78f,     1.0f } },   // 36
    {  4, 1, 3, {     0.0f,     0.0f,     3.0f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 37
    {  4, 1, 1, {     0.0f,     0.0f,     3.0f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 38
    {  7, 0, 0, {  -19.22f,  -23.63f, -12.599999f,     1.0f }, 0, 0, { 11.809999f, -25.929998f, 18.429998f,     1.0f } },   // 39
    {  7, 0, 0, { -5.4399996f, -28.029999f,  -19.91f,     1.0f }, 0, 0, {    4.04f,  -24.58f, 22.849998f,     1.0f } },   // 40
    {  7, 0, 0, { 11.629999f,  -29.14f, -10.719999f,     1.0f }, 0, 0, { -15.089999f, -27.609999f, 24.109999f,     1.0f } },   // 41
    {  7, 0, 0, { 23.789999f,  -32.07f,   -7.08f,     1.0f }, 0, 0, {   -13.0f,  -26.71f, 16.339998f,     1.0f } },   // 42
    {  7, 0, 0, {    8.73f, -32.859997f, 29.669998f,     1.0f }, 0, 0, { -14.349999f,  -23.72f,   -6.58f,     1.0f } },   // 43
    {  7, 0, 0, {  -18.71f,   -41.3f,   24.07f,     1.0f }, 0, 0, {    6.47f, -18.009998f, -3.3899999f,     1.0f } },   // 44
    {  9, 0, 0, {    8.94f,  -30.64f, 31.769999f,     1.0f }, 0, 0, {    1.25f, -25.949999f,    7.16f,     1.0f } },   // 45
    {  9, 0, 0, { 19.939999f,  -32.21f,  -10.08f,     1.0f }, 0, 0, { -0.79999995f, -25.279999f, 4.3599997f,     1.0f } },   // 46
    {  9, 0, 0, { -12.629999f,  -18.96f, -0.78999996f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 47
    {  9, 0, 0, {  -13.04f, -32.449997f, 19.019999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 48
    {  9, 0, 0, {  -16.07f, -9.679999f,   14.48f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 49
    {  9, 0, 0, {  -15.79f,  -38.82f, 29.039999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 50
    {  9, 0, 0, {    11.7f,   -19.9f,   23.39f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 51
    { 10, 0, 0, {   19.82f, -28.369999f, 28.429998f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 52
    { 10, 0, 0, {  -24.96f,   -42.1f, 13.589999f,     1.0f }, 0, 0, {    -7.2f, -24.439999f, 5.2599998f,     1.0f } },   // 53
    { 10, 0, 0, {   -8.99f, -32.339996f,   -7.75f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 54
    { 10, 0, 0, { 17.119999f,  -24.48f, -1.8599999f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 55
    { 11, 0, 0, {  -24.49f,  -36.42f, -31.599998f,     1.0f }, 0, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 56
    { 11, 0, 0, {   32.28f, -28.109999f,  -29.89f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 57
    { 11, 0, 0, {   30.25f,  -29.15f, 37.699997f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 58
    { 11, 0, 0, {  -40.64f, -17.529999f, 18.439999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 59
    { 11, 0, 0, {   15.37f,  -57.37f,    12.4f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 60
    { 11, 0, 0, {  -12.24f, -32.539997f, -23.279999f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 61
    {  6, 0, 0, {  -18.88f, -24.099998f,   -11.7f,     1.0f }, 1, 1, {     0.0f,     2.0f,     5.0f,     1.0f } },   // 62
    {  6, 0, 0, { 18.259998f, -31.089998f,  -11.78f,     1.0f }, 1, 3, {     0.0f,     0.0f,    -5.0f,     1.0f } },   // 63
    {  6, 0, 0, {    9.23f, -42.379997f,  -18.96f,     1.0f }, 1, 1, {     0.0f,     0.0f,     5.0f,     1.0f } },   // 64
    {  6, 0, 0, {   27.99f, -29.849998f, -8.879999f,     1.0f }, 1, 5, {     0.0f,     3.0f,     0.0f,     1.0f } },   // 65
    {  6, 0, 0, {    22.3f, -33.429996f, 30.439999f,     1.0f }, 1, 1, {     0.0f,     0.0f,     5.0f,     1.0f } },   // 66
    {  6, 0, 0, {  -17.71f,  -20.64f,   38.91f,     1.0f }, 0, 0, {    4.04f,  -24.71f,   -0.59f,     1.0f } },   // 67
    {  6, 0, 0, {  -29.81f,  -32.25f, 15.669999f,     1.0f }, 1, 4, {     0.0f, -24.509998f,     0.0f,     1.0f } },   // 68
    {  5, 0, 0, {   15.45f, -32.379997f, 18.869999f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 69
    {  5, 0, 0, { -12.389999f,  -23.15f,   20.49f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 70
    {  5, 0, 0, {   13.79f,  -38.96f,    5.18f,     1.0f }, 0, 0, {    3.59f,  -28.65f,     1.8f,     1.0f } },   // 71
    {  5, 0, 0, { -11.179999f, -34.219997f, 7.6099997f,     1.0f }, 1, 5, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 72
    {  5, 0, 0, {  -11.44f,  -35.94f,   11.73f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 73
    {  8, 0, 0, { -9.099999f,   -36.3f, 21.929998f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 74
    {  8, 0, 0, { 8.639999f, -36.839996f, 11.5199995f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 75
    {  8, 0, 0, { -34.839996f, -36.089996f,  -42.98f,     1.0f }, 0, 0, { -24.779999f,  -34.92f, -32.059998f,     1.0f } },   // 76
    {  8, 0, 0, { 14.629999f, -23.839998f,   16.71f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 77
    {  8, 0, 0, { 19.599998f, -14.339999f, 38.769997f,     1.0f }, 1, 1, {     0.0f,     5.0f,     0.0f,     1.0f } },   // 78
    {  8, 0, 0, {  -17.38f,  -36.07f,   48.76f,     1.0f }, 0, 0, {  -13.69f, -32.339996f,   34.82f,     1.0f } },   // 79
    { 14, 0, 0, {   19.81f, -26.539999f, -23.089998f,     1.0f }, 1, 3, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 80
    { 14, 0, 0, {  -22.73f, -40.719997f, -20.669998f,     1.0f }, 1, 1, {     0.0f,     0.0f,     0.0f,     1.0f } },   // 81
    { 14, 0, 0, {  -51.92f, -26.929998f, -10.959999f,     1.0f }, 0, 0, {   65.99f,  -18.97f,   26.21f,     1.0f } },   // 82
    { 14, 0, 0, {   48.87f,  -19.25f,  -11.98f,     1.0f }, 0, 0, { -70.759995f,  -22.07f,   26.06f,     1.0f } },   // 83
    { 14, 0, 0, {   31.15f,  -13.61f, -15.099999f,     1.0f }, 0, 0, {  -70.31f,  -43.16f,   49.48f,     1.0f } },   // 84
    { 14, 0, 0, { 9.469999f,  -50.66f, -26.869999f,     1.0f }, 0, 0, {  -27.39f,   10.32f,    75.0f,     1.0f } },   // 85
    { 13, 1, 0, {   15.28f,  -23.14f,   24.99f,     1.0f }, 1, 0, { -305.05f,  -46.44f, -337.68997f,     1.0f } },   // 86
};

static DMG_CAM_SHOT dmg_cam_shot[DMG_CAM_SHOT_MAX];     /* bss 0x390cd0 */
static u_char dmg_cam_order[DMG_CAM_SHOT_MAX];          /* bss 0x390fd0: the shuffled play order */
static u_char dmg_cam_shot_num;                         /* sbss gp-0x760 */
static float  dmg_cam_time;                             /* sbss gp-0x75c: frames left of the current shot */
static u_char dmg_cam_cur;                              /* sbss gp-0x758: its place in dmg_cam_order */

// rand() / RAND_MAX as the ROM writes it (2147483520.0f, rdata 0x36bb60).
// callcheck: rand (PC: ReiPan_Rand, the ROM's 31-bit range)
static inline float DmgCamRand01(void)
{
    return (float)ReiPan_Rand() / REIPAN_RAND_MAXF;
}

static void DmgCamShotSetPos(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app);
static void DmgCamShotSetTarget(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app);
static void DmgCamShotUpdate(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app);
static int  DmgCamShotNext(APPROACH_CAMERA *app);

// ──────────────────────────────────────────────────────────────────────
// Gathers the shots of kind (the attacker's PLYR_WRK._147), shuffles their
// order (ten passes of random swaps) and starts the first visible one; with
// none the approach camera keeps its own eye and target.
// FF3 0x001374d8.
// callcheck: rand (DmgCamRand01)

void DmgCamShotInit(s_char kind, APPROACH_CAMERA *app)
{
    DMG_CAM_SHOT *shot;
    int i, j, pass;
    u_char tmp;

    dmg_cam_shot_num = 0;
    for (i = 0; i < DMG_CAM_SHOT_DAT_NUM; i++)
    {
        if (dmg_cam_shot_dat[i].kind == kind)
        {
            dmg_cam_shot[dmg_cam_shot_num].dat = &dmg_cam_shot_dat[i];
            dmg_cam_shot_num++;
        }
    }

    for (i = 0; i < dmg_cam_shot_num; i++)
    {
        dmg_cam_order[i] = (u_char)i;
    }

    // (int)(n * rand / RAND_MAX) reaches n when rand() is within 64 of
    // RAND_MAX; the ROM swaps with the byte after the gathered ones then.
    for (pass = 0; pass < 10; pass++)
    {
        for (i = 0; i < dmg_cam_shot_num; i++)
        {
            j = (int)((float)dmg_cam_shot_num * DmgCamRand01() + 0.0f);
            tmp              = dmg_cam_order[i];
            dmg_cam_order[i] = dmg_cam_order[j];
            dmg_cam_order[j] = tmp;
        }
    }

    dmg_cam_cur  = 0;
    dmg_cam_time = DmgCamRand01() * 60.0f + 90.0f;          /* rdata 0x36bb64, 0x36bb68 */

    if (dmg_cam_shot_num == 0 || DmgCamShotNext(app) == 0)
    {
        sceVu0CopyVector(app->PosInitVector, app->npos);
        sceVu0CopyVector(app->NeckInitVector, app->inpos);
    }
    else
    {
        shot = &dmg_cam_shot[dmg_cam_order[dmg_cam_cur]];
        sceVu0CopyVector(app->PosInitVector, shot->pos);
        sceVu0CopyVector(app->NeckInitVector, shot->tgt);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A shot's anchor: its point into pos, its facing (radians, y) into *rot.
// FF3 0x00137718.

static void DmgCamShotGetBase(u_char base, ENE_WRK *ene, float *pos, float *rot)
{
    PLYR_WRK *plyr = ingame_wrk.plyr_wrk;
    sceVu0FVECTOR a, b;
    sceVu0FVECTOR trot;

    switch (base)
    {
    case DMG_CAM_BASE_PLYR:
        sceVu0CopyVector(pos, plyr->pos);
        *rot = plyr->rot[1];
        break;

    case DMG_CAM_BASE_PLYR_JOINT:
        PlyrGetJointPos(2, pos, nullptr);
        *rot = plyr->rot[1];
        break;

    case DMG_CAM_BASE_ENE:
        sceVu0CopyVector(pos, ene->pos);
        *rot = ene->rot[1];
        break;

    case DMG_CAM_BASE_ENE_JOINT:
        EneGetJointPos(ene, 2, pos, nullptr);
        *rot = ene->rot[1];
        break;

    case DMG_CAM_BASE_MID:
        sceVu0CopyVector(a, plyr->pos);
        sceVu0CopyVector(b, ene->pos);
        GetMidPos(pos, a, b);
        UnitGetTrgtRot(a, pos, trot, 2);
        *rot = trot[1];
        break;

    case DMG_CAM_BASE_MID_JOINT:
        PlyrGetJointPos(2, a, nullptr);
        EneGetJointPos(ene, 2, b, nullptr);
        GetMidPos(pos, a, b);
        UnitGetTrgtRot(a, pos, trot, 2);
        *rot = trot[1];
        break;

    default:
        sceVu0CopyVector(pos, gra3dcamGetPosition());
        *rot = plyr->rot[1];
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The shot's eye: the row's eye offset turned by the anchor's facing and
// moved to the anchor.
// FF3 0x001378a0.

static void DmgCamShotSetPos(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app)
{
    sceVu0FVECTOR base;
    sceVu0FMATRIX m;
    float rot = 0.0f;

    DmgCamShotGetBase(shot->dat->pos_base, app->pEneWrk, base, &rot);
    RotLimitChk(&rot);
    sceVu0UnitMatrix(m);
    sceVu0RotMatrixY(m, m, rot);
    sceVu0TransMatrix(m, m, base);
    sceVu0ApplyMatrix(shot->pos, m, shot->dat->pos);
}

// ──────────────────────────────────────────────────────────────────────
// The shot's target, likewise from the row's target offset and base.
// FF3 0x00137930.

static void DmgCamShotSetTarget(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app)
{
    sceVu0FVECTOR base;
    sceVu0FMATRIX m;
    float rot = 0.0f;

    DmgCamShotGetBase(shot->dat->tgt_base, app->pEneWrk, base, &rot);
    RotLimitChk(&rot);
    sceVu0UnitMatrix(m);
    sceVu0RotMatrixY(m, m, rot);
    sceVu0TransMatrix(m, m, base);
    sceVu0ApplyMatrix(shot->tgt, m, shot->dat->tgt);
}

// FF3 0x001379c0.

static void DmgCamShotUpdate(DMG_CAM_SHOT *shot, APPROACH_CAMERA *app)
{
    DmgCamShotSetPos(shot, app);
    DmgCamShotSetTarget(shot, app);
}

// ──────────────────────────────────────────────────────────────────────
// Starts a new 90..150 frame timer and looks for the next shot in the
// order (the current one last) whose eye sees both its target and the
// player's listen point; a different one goes to PosInit / NeckInit and
// becomes current. 1 when a visible shot was found (the current one
// counts).
// FF3 0x001379f8.
// callcheck: rand (DmgCamRand01)

static int DmgCamShotNext(APPROACH_CAMERA *app)
{
    DMG_CAM_SHOT *shot;
    int i     = 1;
    int found = 0;
    int idx;

    dmg_cam_time = DmgCamRand01() * 60.0f + 90.0f;          /* rdata 0x36bb70, 0x36bb74 */

    if (dmg_cam_shot_num == 0)
    {
        return 0;
    }

    do
    {
        idx  = (dmg_cam_cur + i) % dmg_cam_shot_num;
        shot = &dmg_cam_shot[dmg_cam_order[idx]];
        DmgCamShotUpdate(shot, app);
        if (PlyrAreaLineHitCheck(shot->pos, shot->tgt) == 0
            && PlyrAreaLineHitCheck(shot->pos, ingame_wrk.plyr_wrk->listen_pos) == 0)
        {
            if (dmg_cam_cur != idx)
            {
                sceVu0CopyVector(app->PosInitVector, shot->pos);
                sceVu0CopyVector(app->NeckInitVector, shot->tgt);
                dmg_cam_cur = (u_char)idx;
            }
            found = 1;
        }
        i++;
        if (dmg_cam_shot_num < i)
        {
            return found;
        }
    } while (!found);

    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the shots: counts the shot's time down by the player's
// timer step (kind 13 holds it), cuts to the next one when it runs out,
// and moves the eye / target of the shots that follow their anchor.
// 0 without shots or once the attacker no longer acts.
// FF3 0x00137ba0 (was AlbumViewCsrDisp, a link-order guess).

int DmgCamShotMain(APPROACH_CAMERA *app)
{
    DMG_CAM_SHOT *shot;
    float step;

    step = PlyrGaugeTimeStep(GetAlbumSaveDisp());

    // PC: without an attacker the ROM reads ENE_WRK.state at EE address
    // 0x4cc; treat it as not acting.
    if (dmg_cam_shot_num == 0 || app->pEneWrk == nullptr || app->pEneWrk->state != ENE_STATE_ACT)
    {
        return 0;
    }

    if (dmg_cam_time - step <= 0.0f)
    {
        return DmgCamShotNext(app);
    }

    if (ingame_wrk.plyr_wrk->_147 != 13)
    {
        dmg_cam_time -= step;
    }

    shot = &dmg_cam_shot[dmg_cam_order[dmg_cam_cur]];
    DmgCamShotUpdate(shot, app);
    if (shot->dat->pos_follow == 1)
    {
        sceVu0CopyVector(app->PosInitVector, shot->pos);
    }
    if (shot->dat->tgt_follow == 1)
    {
        sceVu0CopyVector(app->NeckInitVector, shot->tgt);
    }
    return 1;
}
