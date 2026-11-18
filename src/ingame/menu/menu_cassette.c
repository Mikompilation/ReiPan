/* ==========================================================================
 *  ingame/menu/menu_cassette.c
 *
 *  The cassette player (see menu_cassette.h). FF3 0x25a740..0x25bae0,
 *  FF3's own code. A BP_OBJ of CASSETTE_PHF with a cursor (BP_OBJ item
 *  0x41b), a scroll bar and a row hook on item 0x421 that prints the names
 *  of the held tapes; the LayoutAnim message window (type 5) shows the
 *  cursor tape's description, or asks before playing. The chosen tape plays
 *  as a kind-3 subtitle at a fixed point while the layout is out; the event
 *  camera looks at a fixed spot and the player stands at a fixed place
 *  meanwhile. FF3 0x0025bac0 is the GPhaseCtrl atexit thunk that ends the
 *  file (compiler-generated, ~GPHASE_CTRL in gphase.h).
 * ======================================================================== */
#include "ingame/menu/menu_cassette.h"

#include <new>                          // operator new / delete

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/graph2d/fade.h"      // FadeInReq, FadeOutReq, GetFadeState
#include "graphics/graph2d/g2d_draw.h"  // SQAR_DAT, DISP_SQAR, CopySqrDToSqr, DispSqrD
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, SetCurColorRGB, PrintMsg_ArrangeOneLine
#include "graphics/motion/anicode.h"    // SetDefaultGsDrawState
#include "ingame/ingame.h"              // IngameMain, IngameDrawSub
#include "ingame/ingame_wrk.h"          // ingame_wrk
#include "ingame/item/prg/item.h"       // ItemGetCnt, item_dat
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsgSetKeepOpen, Anim2dMsg*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/npc/npc_act.h"         // PlyrCameraEvCamSet, PlyrCameraEvCamEnd, PlyrCameraPlyrPlace
#include "ingame/plyr/player.h"         // PLYR_WRK
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "ingame/story_menu.h"          // LockTblInc, LockTblDec
#include "ingame/subtitle/subtitle.h"   // SubTitleReq3D, SubTitleStop, SubTitleIsEnd, SubTitleGetMsgId
#include "main/gphase.h"                // GetGPhaseCtrl, GPHASE_CTRL
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo
#include "system/b2d/BpObj.h"           // BP_OBJ
#include "system/eeiop/cd_file_dat.h"   // CASSETTE_PHF
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/pad/pad.h"             // pad_wrk
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, CheckPadRptPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define CASSETTE_ITEM_ROWS  0x421       // layout item whose hook prints the rows
#define CASSETTE_ITEM_CSR   0x41b       // the cursor item (also the window's alpha item)
#define CASSETTE_ROW_DY     0x23        // rows 35 px apart
#define CASSETTE_MSG_NAME   0x56        // message kind of the tape names (item_dat._0e)
#define CASSETTE_MSG_DESC   0x55        // message kind of the descriptions (item_dat._0e)
#define CASSETTE_MSG_MENU   0x6d        // (0x6d, 0) "play it?", (0x6d, 1) no tapes
#define CASSETTE_ANIM_WIN   5           // LayoutAnim type of the message window
#define CASSETTE_ANIM_FLG   4           // LAYOUT_ANIM flag the window keeps while open
#define CASSETTE_SUBTITLE   3           // subtitle kind of the tapes
#define CASSETTE_LABEL_IN   0x16        // capCtl labels: shown while the list is up
#define CASSETTE_LABEL_PLAY 0x11        // while a tape plays
#define CASSETTE_FADE_SPEED 0x1e
#define CASSETTE_BG_MAX     0x20        // background darkening alpha
#define CASSETTE_PLACE_ANIM 0xb0        // player anime while the menu is open
#define SE_CSR_MOVE         0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_PAGE             0x1a9b0005
#define SE_NONE             (-1)

// data 0x3630f0.
const CASSETTE_DAT cassette_tbl[CASSETTE_NUM] =
{
    { 0x35, -1, 1 },
    { 0x36, -1, 0 },
    { 0x37, -1, 2 },
    { 0x38, -1, 3 },
    { 0x39, -1, 4 },
    { 0x3a, -1, 5 },
    { 0x3b, -1, 6 },
};

// data 0x363148 / 0x363160.
static const BP_OBJ_SCROLL_DEF    cassette_scroll_def    = { 0x42c, 0x42f, 0x430, 0x431, 0x429 };
static const BP_OBJ_SCROLL_ARROWS cassette_scroll_arrows = { 0x428, 0x434, 0x42a, 0x435 };

// The player's place while the menu is open (xyz, facing in degrees; data
// 0x363190) and the event camera (fov sdata 0x37268c, position 0x3631a0,
// target 0x3631b0).
static const float cassette_plyr_place[4] = { 181.81f, -50.0f, 93.026f, 5.0f };
static float cassette_cam_pos[4] = { 200.43f, -63.06f, 91.57f, 1.0f };
static float cassette_cam_tgt[4] = { 111.88f, -73.66f, 62.94f, 1.0f };
static const float cassette_cam_fov = 0.76f;

// Where the tape's subtitle plays (sdata 0x372660).
static const float cassette_play_pos[4] = { 154.8f, -66.7f, 74.6f, 1.0f };

int cassette_heard[CASSETTE_NUM];                   // bss 0x3fd060
static sceVu0FVECTOR cassette_plyr_back;            // bss 0x3fd080: the player's place before

static int  *cassette_list;                         // sbss gp+0x4b0: rows of cassette_tbl held
static int   cassette_list_num;                     // sbss gp+0x4b4
static short cassette_csr;                          // sbss gp+0x4c0: MenuCsr id
static int   cassette_bg_alpha;                     // sbss gp+0x4c4
static short cassette_ret_phase;                    // sbss gp+0x4d0: phase to go back to
static u_char cassette_step;                        // sbss gp+0x4d2: one_Story_Cassette's step

static int      cassette_state  = CASSETTE_ST_OPEN; // sdata 0x37265c (CASSETTE_STATE)
static BP_OBJ  *cassette_obj    = nullptr;          // sdata 0x372670
static int      cassette_win    = -1;               // sdata 0x372674: LayoutAnim slot of the window
static int      cassette_phase  = 0;                // sdata 0x372688: running as phase 46 of its own

// ──────────────────────────────────────────────────────────────────────
// The owned-tape list
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025a740.
int *CassetteGetList(int *num)
{
    *num = cassette_list_num;
    return cassette_list;
}

// FF3 0x0025a758.
int CassetteListCreate(void)
{
    int num = CassetteTblGetNum();

    cassette_list     = (int *)heapCtrlMalloc(GetSystemHeapWrkP(), num * sizeof(int), 0, 0, 0);
    cassette_list_num = 0;
    for (int i = 0; i < num; i++)
    {
        u_short item = (u_short)CassetteTblGetItem(i);
        short   cnt  = 0;

        if (item < ITEM_NUM)
            cnt = *ItemGetCnt(item);
        if (cnt != 0)
            cassette_list[cassette_list_num++] = i;
    }
    if (cassette_list_num < 1)
    {
        heapCtrlFree(GetSystemHeapWrkP(), cassette_list, 0, 0, 0);
        cassette_list = nullptr;
    }
    return cassette_list_num;
}

// FF3 0x0025a850.
int CassetteGetNum(void)
{
    return cassette_list_num;
}

// FF3 0x0025a860.
void CassettePlay(short pos)
{
    float tmp[4] = { cassette_play_pos[0], cassette_play_pos[1], cassette_play_pos[2], cassette_play_pos[3] };

    SubTitleReq3D(CASSETTE_SUBTITLE, CassetteTblGetSubTitle(cassette_list[pos]), tmp);
}

// FF3 0x0025a8e0.
void CassetteSubTitleStop(void)
{
    SubTitleStop();
}

// FF3 0x0025a8f8.
int CassettePlayIsEnd(short pos)
{
    if (SubTitleIsEnd() != 0)
    {
        CassetteSetHeard(cassette_list[pos]);
        return 1;
    }

    int end_msg = CassetteTblGetEndMsg(cassette_list[pos]);

    CassetteTblGetSubTitle(cassette_list[pos]);     // read and unused in FF3
    if (end_msg != -1)
    {
        if (SubTitleGetMsgId() != end_msg)
            return 0;
        CassetteSetHeard(cassette_list[pos]);
    }
    return 0;
}

// FF3 0x0025a9a0.
void CassetteListRelease(void)
{
    heapCtrlFree(GetSystemHeapWrkP(), cassette_list, 0, 0, 0);
    cassette_list     = nullptr;
    cassette_list_num = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The menu (pad side)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025a9e0.
void CassetteMenuInit(short csr)
{
    int num;

    cassette_state = CASSETTE_ST_OPEN;
    num = CassetteListCreate();
    if (num > 0)
    {
        MenuCsrInit(csr, 1, (short)num, 0);
        MenuCsrSetViewY(csr, CASSETTE_VIEW_ROWS, 0);
        pad_wrk[0].stick_dpad = 1;
    }
}

// State 3: the cursor, L1 / R1 page past a full view, cancel closes, OK asks.
// State 5: OK on "yes" plays, cancel or "no" goes back. State 9: any key
// stops the tape; its end also comes back. 1 once the layout closed.
// FF3 0x0025aa48.
int CassetteMenuCtrl(short csr)
{
    if (cassette_state == CASSETTE_ST_LIST)
    {
        int close = 0;

        if (CassetteGetNum() < 1)
        {
            close = CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) != 0;
        }
        else
        {
            MenuMouseCsrObjId(csr, cassette_obj, CASSETTE_ITEM_CSR);    // PC
            MenuCsrUpdate(csr, 0);
            if (MenuCsrIsMovedSe(csr, SE_CSR_MOVE) == 0)
            {
                if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_PREV], SE_NONE, 0) != 0
                    && CassetteGetNum() >= CASSETTE_VIEW_ROWS + 1)
                {
                    MenuCsrPagePrevY(csr);
                    SndSePlay(SE_PAGE, 100, 0x1000, 0, nullptr, 0);
                }
                else if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_NEXT], SE_NONE, 0) != 0
                         && CassetteGetNum() >= CASSETTE_VIEW_ROWS + 1)
                {
                    MenuCsrPageNextY(csr);
                    SndSePlay(SE_PAGE, 100, 0x1000, 0, nullptr, 0);
                }
                else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) != 0)
                {
                    close = 1;
                }
                else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], SE_NONE, 0) != 0)
                {
                    cassette_state = CASSETTE_ST_ASK_ON;
                }
            }
        }
        if (close)
        {
            // The cancel press is turned into OK for the message window.
            pad_wrk[0].trg = (u_short)((pad_wrk[0].trg & ~pad_key[PAD_KEY_CANCEL]) | pad_key[PAD_KEY_OK]);
            LayoutAnimMain();
            cassette_state = CASSETTE_ST_CLOSE;
        }
    }

    if (cassette_state == CASSETTE_ST_ASK)
    {
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_NONE, 0) != 0)
        {
            cassette_state = CASSETTE_ST_ASK_OFF;
        }
        else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], SE_NONE, 0) != 0)
        {
            if (Anim2dMsgGetCsr() == 0)
            {
                CassettePlay(MenuCsrGetPos(csr));
                cassette_state = CASSETTE_ST_PLAY_OUT;
            }
            else
            {
                cassette_state = CASSETTE_ST_ASK_OFF;
            }
        }
    }

    if (cassette_state == CASSETTE_ST_PLAY)
    {
        ReiPan_MenuMouseConfirm();      // PC: a click stops the tape
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK] | pad_key[PAD_KEY_CANCEL], SE_NONE, 0) != 0)
        {
            CassetteSubTitleStop();
            cassette_state = CASSETTE_ST_PLAY_END;
        }
        if (CassettePlayIsEnd(MenuCsrGetPos(csr)) != 0)
            cassette_state = CASSETTE_ST_PLAY_END;
    }

    return cassette_state == CASSETTE_ST_END;
}

// FF3 0x0025acd0.
void CassetteMenuTerm(short csr)
{
    if (CassetteGetNum() > 0)
    {
        CassetteListRelease();
        MenuCsrRelease(csr);
        pad_wrk[0].stick_dpad = 0;
    }
}

// FF3 0x0025ad18.
void CassetteInit(void)
{
}

// FF3 0x0025ad20.
int CassetteTblGetNum(void)
{
    return CASSETTE_NUM;
}

// FF3 0x0025ad28.
int CassetteTblGetItem(int no)
{
    return cassette_tbl[no].item;
}

// FF3 0x0025ad48.
int CassetteTblGetEndMsg(int no)
{
    return cassette_tbl[no].end_msg;
}

// FF3 0x0025ad68.
int CassetteTblGetSubTitle(int no)
{
    return cassette_tbl[no].subtitle;
}

// FF3 0x0025ad88.
int CassetteGetHaveNum(void)
{
    int have = 0;

    for (int i = 0; i < CassetteTblGetNum(); i++)
    {
        u_short item = (u_short)CassetteTblGetItem(i);
        short   cnt  = 0;

        if (item < ITEM_NUM)
            cnt = *ItemGetCnt(item);
        if (cnt != 0)
            have++;
    }
    return have;
}

// ──────────────────────────────────────────────────────────────────────
// The layout (draw side)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025ae10.
void CassetteBgAlphaInit(void)
{
    cassette_bg_alpha = 0;
}

// FF3 0x0025ae18.
int CassetteBgFadeIn(void)
{
    if (cassette_bg_alpha + 4 < CASSETTE_BG_MAX)
    {
        cassette_bg_alpha += 4;
        return 0;
    }
    cassette_bg_alpha = CASSETTE_BG_MAX;
    return 1;
}

// FF3 0x0025ae48.
int CassetteBgFadeOut(void)
{
    if (cassette_bg_alpha - 4 > 0)
    {
        cassette_bg_alpha -= 4;
        return 0;
    }
    cassette_bg_alpha = 0;
    return 1;
}

// The game darkened behind the layout: a black screen-sized square.
// FF3 0x0025ae70.
void CassetteBgDraw(void)
{
    DISP_SQAR ds;
    SQAR_DAT  sq;

    sq.w     = 0x280;
    sq.h     = 0x1c0;
    sq.pri   = 0x40;
    sq.x     = 0;
    sq.y     = 0;
    sq.alpha = (u_char)cassette_bg_alpha;
    sq.r     = 0;
    sq.g     = 0;
    sq.b     = 0;
    CopySqrDToSqr(&ds, &sq);
    DispSqrD(&ds);
}

// Item hook of the rows item: the names of the tapes in the view, the
// cursor row brighter. Rows stop at the list's end counted from the top
// (FF3 compares top + row with the count).
// FF3 0x0025aed0.
static int CassetteDrawRows(const B2D_HOOK_ARGS *args)
{
    const int *xy    = (const int *)args->t1;
    int        alpha = (int)args->stack[2];
    int        num;
    int       *list  = CassetteGetList(&num);

    if (num > 0)
    {
        short top   = MenuCsrGetTopY(cassette_csr);
        short cur   = MenuCsrGetPosY(cassette_csr);
        int  *row   = &list[top];
        int   dy    = 0;

        for (int i = 0; i < CASSETTE_VIEW_ROWS; i++, dy += CASSETTE_ROW_DY)
        {
            if (top + i >= num)
                break;

            short   item = (short)CassetteTblGetItem(*row++);
            u_char *str  = GetMsgDataAddr(CASSETTE_MSG_NAME, item_dat[item]._0e);

            if (str != nullptr)
            {
                SetCurColorRGB(0x5a, 0x99, 0xa0);
                if (i == cur)
                    SetCurColorRGB(0xb7, 0xde, 0xe2);
                PrintMsg_ArrangeOneLine(str, xy[0], xy[1] + dy, 0x2d, alpha, 0xa0, 0, 0, 0);
            }
        }
        SetDefaultGsDrawState();
    }
    return 0;
}

// FF3 0x0025b030.
void CassetteSetRowHook(void)
{
    B2D_ITEM_HOOK hook = { CassetteDrawRows };

    B2dCtlSetItemHook(B2dCtlFindItem(BpCtlGetHandleByFileNo(CASSETTE_PHF), CASSETTE_ITEM_ROWS), &hook);
}

// FF3 0x0025b070.
void CassetteScrollBarDraw(void)
{
    int num;

    CassetteGetList(&num);
    if (num > 0)
    {
        BP_OBJ_SCROLL_POS pos;

        pos.first  = MenuCsrGetTopY(cassette_csr);
        pos.cursor = MenuCsrGetPosY(cassette_csr);
        pos.rows   = (short)num;
        pos.page   = CASSETTE_VIEW_ROWS;
        cassette_obj->DrawScrollBar(&pos, &cassette_scroll_arrows);
    }
}

// FF3 0x0025b0e8.
void CassetteScrollBarAdd(void)
{
    cassette_obj->AddScrollBar(&cassette_scroll_def);
    CassetteScrollBarDraw();
}

// FF3 0x0025b110.
void CassetteScrollBarDel(void)
{
    cassette_obj->DelScrollBar();
}

// FF3 0x0025b128.
void CassetteCsrUpdate(void)
{
    short row = MenuCsrGetPosY(cassette_csr);

    if (row < 0)
        row = 0;
    cassette_obj->SetCsrPos(CASSETTE_ITEM_CSR, 0, row);
}

// FF3 0x0025b160.
void CassetteCsrAdd(void)
{
    cassette_obj->AddCsr(CASSETTE_ITEM_CSR, 0, CASSETTE_ROW_DY, 0);
    CassetteCsrUpdate();
}

// FF3 0x0025b190.
void CassetteCsrFree(void)
{
    cassette_obj->FreeCsr();
}

// The window's line: the cursor tape's description, or "no tapes".
// FF3 0x0025b1a8.
void CassetteHelpSet(void)
{
    int  num;
    int *list = CassetteGetList(&num);
    int  msg0 = CASSETTE_MSG_MENU;
    int  msg1 = 1;

    if (num > 0)
    {
        int item = CassetteTblGetItem(list[MenuCsrGetPos(cassette_csr)]);

        msg0 = CASSETTE_MSG_DESC;
        msg1 = -1;
        if (item == -1)
            msg0 = -1;
        else
            msg1 = item_dat[(short)item]._0e;
    }
    Anim2dMsgSet(msg0, msg1);
}

// FF3 0x0025b238.
void CassetteHelpMode(int ask)
{
    if (ask != 0)
    {
        Anim2dMsgSetType(1);
        Anim2dMsgSet(CASSETTE_MSG_MENU, 0);
        return;
    }
    Anim2dMsgSetType(2);
    CassetteHelpSet();
}

// FF3 0x0025b280.
void CassetteMsgWinOpen(void)
{
    cassette_win = LayoutAnimStart(CASSETTE_ANIM_WIN, 0);
    LayoutMsgSetKeepOpen(1);
    LayoutAnimSetFlag(cassette_win, CASSETTE_ANIM_FLG);
    LayoutAnimSetAlpha(cassette_win, 0);
    CassetteHelpMode(0);
}

// FF3 0x0025b2c8.
int CassetteMsgWinIsReady(void)
{
    return LayoutAnimIsDataReady(cassette_win) != 0;
}

// FF3 0x0025b2f0.
void CassetteMsgWinSetAlphaItem(int handle, int id)
{
    LayoutAnimSetAlphaItemById(cassette_win, handle, id);
}

// FF3 0x0025b318.
void CassetteMsgWinClose(void)
{
    if (cassette_win != -1)
    {
        LayoutAnimClearFlag(cassette_win, CASSETTE_ANIM_FLG);
        LayoutMsgSetKeepOpen(0);
        LayoutAnimReset(cassette_win);
        cassette_win = -1;
    }
}

// FF3 0x0025b360.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void CassetteLayoutCreate(short csr)
{
    cassette_csr = csr;
    CassetteBgAlphaInit();
    cassette_obj = new BP_OBJ(CASSETTE_PHF);
}

// The draw side of the state machine, once per frame.
// FF3 0x0025b3a8.
// callcheck: GetCmnFadeLayer, CmnFadeLayerUpdate, CmnFadeLayerShowFadeIn, CmnFadeLayerSwitch, CmnFadeLayerHide (capCtl methods)
void CassetteLayoutMain(void)
{
    CassetteBgDraw();
    if ((u_int)(cassette_state - CASSETTE_ST_IN) < 7)
    {
        CassetteCsrUpdate();
        CassetteScrollBarDraw();
    }
    GetCmnFadeLayer()->Update();

    if (cassette_state == CASSETTE_ST_OPEN)
    {
        CassetteMsgWinOpen();
        cassette_state = CASSETTE_ST_LOAD;
    }
    if (cassette_state == CASSETTE_ST_LOAD)
    {
        int loaded = cassette_obj->Load();

        if (CassetteMsgWinIsReady() == 0)
            loaded = 0;
        if (CassetteBgFadeIn() != 0 && loaded != 0)
        {
            cassette_obj->In("in");
            CassetteSetRowHook();
            CassetteCsrAdd();
            CassetteScrollBarAdd();
            GetCmnFadeLayer()->base_alpha = 0x80;
            GetCmnFadeLayer()->ShowFadeIn(CASSETTE_LABEL_IN, CASSETTE_FADE_SPEED);
            CassetteMsgWinSetAlphaItem(cassette_obj->GetHandle(), CASSETTE_ITEM_CSR);
            CassetteHelpSet();
            cassette_state = CASSETTE_ST_IN;
        }
    }
    if (cassette_state == CASSETTE_ST_IN && cassette_obj->IsInEnd() != 0)
        cassette_state = CASSETTE_ST_LIST;
    if (cassette_state == CASSETTE_ST_LIST)
        CassetteHelpSet();
    if (cassette_state == CASSETTE_ST_ASK_ON)
    {
        CassetteHelpMode(1);
        cassette_state = CASSETTE_ST_ASK;
    }
    if (cassette_state == CASSETTE_ST_ASK_OFF)
    {
        CassetteHelpMode(0);
        cassette_state = CASSETTE_ST_LIST;
    }
    if (cassette_state == CASSETTE_ST_PLAY_OUT)
    {
        GetCmnFadeLayer()->Switch(CASSETTE_LABEL_PLAY, CASSETTE_FADE_SPEED);
        if (cassette_obj->Out("out") != 0)
        {
            CassetteHelpMode(0);
            cassette_state = CASSETTE_ST_PLAY;
        }
    }
    if (cassette_state == CASSETTE_ST_PLAY_END)
    {
        GetCmnFadeLayer()->Switch(CASSETTE_LABEL_IN, CASSETTE_FADE_SPEED);
        cassette_obj->In("in");
        CassetteHelpMode(0);
        cassette_state = CASSETTE_ST_IN;
    }
    if (cassette_state == CASSETTE_ST_CLOSE)
    {
        GetCmnFadeLayer()->Hide(CASSETTE_FADE_SPEED);
        if (cassette_obj->Out("out") != 0)
        {
            CassetteMsgWinClose();
            cassette_state = CASSETTE_ST_UNDARKEN;
        }
    }
    if (cassette_state == CASSETTE_ST_UNDARKEN && CassetteBgFadeOut() != 0)
        cassette_state = CASSETTE_ST_END;
}

// FF3 0x0025b658.
// callcheck: operator_delete, BpObjRelease, CmnFadeLayerTerm (delete, BP_OBJ::Release, capCtl::Term)
void CassetteLayoutRelease(void)
{
    CassetteCsrFree();
    CassetteScrollBarDel();
    GetCmnFadeLayer()->base_alpha = 0;
    GetCmnFadeLayer()->Term();
    if (cassette_obj != nullptr)
    {
        cassette_obj->Release();
        delete cassette_obj;
    }
    cassette_obj = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Heard flags
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0025b6b8.
void CassetteHeardClear(void)
{
    for (int i = 0; i < CASSETTE_NUM; i++)
        cassette_heard[i] = 0;
}

// FF3 0x0025b6e8.
void CassetteSetHeard(int no)
{
    cassette_heard[no] = 1;
}

// FF3 0x0025b720.
int CassetteGetState(int no)
{
    return cassette_heard[no];
}

// FF3 0x0025b738.
int CassetteHeardSaveGet(void **addr)
{
    *addr = cassette_heard;
    return sizeof(cassette_heard);
}
static_assert(sizeof(cassette_heard) == 0x1c, "save block 41 is 0x1c bytes");

// ──────────────────────────────────────────────────────────────────────
// Phase 46 Story_Cassette
// ──────────────────────────────────────────────────────────────────────

// No callers in the ROM.
// FF3 0x0025b750.
void CassetteMenuReq(void)
{
    cassette_phase = 1;
    GetGPhaseCtrl()->SetNext(CASSETTE_PHASE, 0);
}

// FF3 0x0025b790.
int init_Story_Cassette(void)
{
    LockTblInc(3);
    LockTblInc(0xf);
    LockTblInc(0x11);
    LockTblInc(0x12);
    ingame_wrk.plyr_wrk->_0e0 |= 4;
    if (cassette_phase != 0)
        cassette_ret_phase = GetGPhaseCtrl()->from;
    cassette_step = 0;
    return 1;
}

// Fade out, the event camera on and the player at the menu's place, fade
// in, the menu until it closes, fade out, camera and player back, fade in,
// and (as a phase of its own) back to the phase that asked for it.
// FF3 0x0025b810.
int one_Story_Cassette(void)
{
    if (cassette_phase != 0)
    {
        IngameMain(0);
        IngameDrawSub(0);
    }

    switch (cassette_step)
    {
    case 0:
        FadeOutReq(0, 0, 0, 0x10);
        cassette_step++;
        // fall through (jump table data 0x363170)
    case 1:
        if (GetFadeState() == 0)
            return 0;
        cassette_step++;
        // fall through
    case 2:
    {
        PLYR_WRK *plyr;

        FadeInReq(0, 0, 0, 0x10);
        PlyrCameraEvCamSet(cassette_cam_fov, 0.0f, cassette_cam_pos, cassette_cam_tgt);
        plyr = ingame_wrk.plyr_wrk;
        cassette_plyr_back[0] = plyr->pos[0];
        cassette_plyr_back[1] = plyr->pos[1];
        cassette_plyr_back[2] = plyr->pos[2];
        cassette_plyr_back[3] = plyr->pos[3];
        cassette_plyr_back[3] = (ingame_wrk.plyr_wrk->rot[1] * 180.0f) / 3.14159274f;
        PlyrCameraPlyrPlace(cassette_plyr_place, CASSETTE_PLACE_ANIM);
        cassette_step++;
    }
        // fall through
    case 3:
        if (GetFadeState() == 0)
            return 0;
        CassetteMenuInit(CASSETTE_CSR);
        CassetteLayoutCreate(CASSETTE_CSR);
        cassette_step++;
        // fall through
    case 4:
        if (CassetteMenuCtrl(CASSETTE_CSR) == 0)
        {
            CassetteLayoutMain();
            return 0;
        }
        cassette_step++;
        // fall through
    case 5:
        CassetteMenuTerm(CASSETTE_CSR);
        CassetteLayoutRelease();
        FadeOutReq(0, 0, 0, 0x10);
        cassette_step++;
        // fall through
    case 6:
        if (GetFadeState() == 0)
            return 0;
        FadeInReq(0, 0, 0, 0x10);
        PlyrCameraEvCamEnd();
        PlyrCameraPlyrPlace(cassette_plyr_back, 0);
        cassette_step++;
        // fall through
    case 7:
        if (GetFadeState() == 0)
            return 0;
        if (cassette_phase != 0)
            GetGPhaseCtrl()->SetNext(cassette_ret_phase, 0);
        cassette_step++;
        // fall through
    default:
        return 1;
    }
}

// FF3 0x0025ba68.
int end_Story_Cassette(void)
{
    LockTblDec(3);
    LockTblDec(0xf);
    LockTblDec(0x11);
    LockTblDec(0x12);
    ingame_wrk.plyr_wrk->_0e0 &= ~4;
    cassette_phase = 0;
    return 1;
}
