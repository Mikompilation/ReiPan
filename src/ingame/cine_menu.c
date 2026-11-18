/* ==========================================================================
 *  ingame/cine_menu.c
 *
 *  The cine film menu (see cine_menu.h). FF3 0x149c98..0x14ac58, FF3's own
 *  code, built like the cassette menu (menu_cassette.c, FF3 0x25a740..):
 *  a BP_OBJ of EISYASITU_PHF with a cursor (item 0x41b), a scroll bar and a
 *  row hook on item 0x421 that prints the names of the held films; the
 *  LayoutAnim message window (type 5) shows the cursor film's description,
 *  or asks before playing. "Yes" requests the film on the projector of the
 *  player's area and the menu closes; cine_film.c plays it. The fade layer
 *  shows label 0xb while the list is up (the cassette menu's 0x16 / 0x11
 *  switching has no counterpart here). FF3 0x0014ac38 is the GPhaseCtrl
 *  atexit thunk that ends the file (compiler output: GPhaseCtrlTerm on the
 *  singleton this file's inline GetGPhaseCtrl instantiates; ~GPHASE_CTRL in
 *  gphase.h). FF2's matcher filed two of these functions as alg_manage.c's
 *  alg_manageIsReadyAlg / ClearAlg (lookalikes, not the same code).
 * ======================================================================== */
#include "ingame/cine_menu.h"

#include <new>                          // operator new / delete

#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, capCtl
#include "common/heapctrl.h"            // heapCtrlMalloc, heapCtrlFree
#include "graphics/graph2d/g2d_draw.h"  // SQAR_DAT, DISP_SQAR, CopySqrDToSqr, DispSqrD
#include "graphics/graph2d/message.h"   // GetMsgDataAddr, SetCurColorRGB, PrintMsg_ArrangeOneLine
#include "graphics/motion/anicode.h"    // SetDefaultGsDrawState
#include "ingame/cine_film.h"           // CineFilmPlayReq, CineFilmSelect
#include "ingame/ingame.h"              // IngameMain, IngameDrawSub
#include "ingame/item/prg/item.h"       // ItemGetCnt, item_dat, ITEM_NUM
#include "ingame/menu/anim_2d.h"        // LayoutAnim*, LayoutMsgSetKeepOpen, Anim2dMsg*
#include "ingame/menu/menu_csr.h"       // MenuCsr*
#include "ingame/plyr/player.h"         // GetPlyrAreaNo
#include "ingame/sound/snd_ctrl.h"      // SndSePlay
#include "ingame/story_menu.h"          // LockTblInc, LockTblDec
#include "main/gphase.h"                // GetGPhaseCtrl, GPHASE_CTRL
#include "system/b2d/B2dCtl.h"          // B2dCtlFindItem, B2dCtlSetItemHook
#include "system/b2d/BpCtl.h"           // BpCtlGetHandleByFileNo
#include "system/b2d/BpObj.h"           // BP_OBJ
#include "system/eeiop/cd_file_dat.h"   // EISYASITU_PHF, SCENE9xxxM_PSS
#include "system/os/sys_heap.h"         // GetSystemHeapWrkP
#include "system/pad/pad.h"             // pad_wrk
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, CheckPadRptPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define CINE_ITEM_ROWS      0x421       // layout item whose hook prints the rows
#define CINE_ITEM_CSR       0x41b       // the cursor item (also the window's and fade layer's alpha item)
#define CINE_ROW_DY         0x23        // rows 35 px apart
#define CINE_MSG_NAME       0x56        // message kind of the film names (item_dat._0e)
#define CINE_MSG_DESC       0x55        // message kind of the descriptions (item_dat._0e)
#define CINE_MSG_MENU       0x5d        // (0x5d, 0) "play it?", (0x5d, 1) no films
#define CINE_ANIM_WIN       5           // LayoutAnim type of the message window
#define CINE_ANIM_FLG       4           // LAYOUT_ANIM flag the window keeps while open
#define CINE_LABEL_IN       0xb         // capCtl label shown while the list is up
#define CINE_BG_MAX         0x20        // background darkening alpha
#define CINE_INGAME_SKIP  9           // IngameMain / skip flags while the menu is open
#define SE_CSR_MOVE         0x1a9b0000
#define SE_CANCEL           0x1a9b0001
#define SE_DECIDE           0x1a9b0003
#define SE_PAGE             0x1a9b0005
#define SE_NONE             (-1)

// data 0x3508c8. Row 7 is film 6's second cut (no item of its own:
// CineFilmSelect swaps 6 for 7 once 6 was played through); rows 9 and 10
// are films the event script puts in a projector itself.
const CINE_FILM_DAT cine_film_tbl[CINE_FILM_NUM] =
{
    { 0x3c, SCENE9105M_PSS, { -1,  -1 } },
    { 0x3d, SCENE9100M_PSS, { 10, 100 } },
    { 0x3e, SCENE9101M_PSS, { -1,  -1 } },
    { 0x3f, SCENE9102M_PSS, { -1,  -1 } },
    { 0x40, SCENE9103M_PSS, { -1,  -1 } },
    { 0x41, SCENE9104M_PSS, { -1,  -1 } },
    { 0x42, SCENE9106M_PSS, { -1,  -1 } },
    { -1,   SCENE9107M_PSS, { -1,  -1 } },
    { 0x44, SCENE9108M_PSS, { -1,  -1 } },
    { -1,   SCENE9500M_PSS, { -1,  -1 } },
    { -1,   SCENE9600M_PSS, { -1,  -1 } },
};

// data 0x3508a0 / 0x3508b8 (the cassette menu's values).
static const BP_OBJ_SCROLL_DEF    cine_scroll_def    = { 0x42c, 0x42f, 0x430, 0x431, 0x429 };
static const BP_OBJ_SCROLL_ARROWS cine_scroll_arrows = { 0x428, 0x434, 0x42a, 0x435 };

static short  cine_csr;                         // sbss gp-0x680: MenuCsr id
static int    cine_bg_alpha;                    // sbss gp-0x67c
static int   *cine_list;                        // sbss gp-0x670: rows of cine_film_tbl held
static int    cine_list_num;                    // sbss gp-0x66c
static short  cine_ret_phase;                   // sbss gp-0x664: phase to go back to

static int     cine_menu_state = CINE_MENU_ST_OPEN; // sdata 0x36d9ec (CINE_MENU_STATE)
static BP_OBJ *cine_obj        = nullptr;           // sdata 0x36d9f0
static int     cine_win        = -1;                // sdata 0x36d9f4: LayoutAnim slot of the window
static int     cine_menu_open  = 0;                 // sdata 0x36da08: CineMenuGetState

// ──────────────────────────────────────────────────────────────────────
// The menu (pad side)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00149c98.
void CineMenuInit(short csr)
{
    int num;

    cine_menu_state = CINE_MENU_ST_OPEN;
    num = CineFilmListCreate();
    if (num > 0)
    {
        MenuCsrInit(csr, 1, (short)num, 0);
        MenuCsrSetViewY(csr, CINE_MENU_VIEW_ROWS, 0);
        pad_wrk[0].stick_dpad = 1;
    }
}

// State 3: the cursor, L1 / R1 page past a full view, cancel closes, OK asks.
// State 5: OK on "yes" puts the film in the player's area's projector and
// closes; cancel or "no" goes back. 1 once the layout closed.
// FF3 0x00149d00.
int CineMenuCtrl(short csr)
{
    if (cine_menu_state == CINE_MENU_ST_LIST)
    {
        int close = 0;

        if (CineFilmGetNum() < 1)
        {
            close = CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) != 0;
        }
        else
        {
            MenuMouseCsrObjId(csr, cine_obj, CINE_ITEM_CSR);    // PC
            MenuCsrUpdate(csr, 0);
            if (MenuCsrIsMovedSe(csr, SE_CSR_MOVE) == 0)
            {
                if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_PREV], SE_NONE, 0) != 0
                    && CineFilmGetNum() >= CINE_MENU_VIEW_ROWS + 1)
                {
                    MenuCsrPagePrevY(csr);
                    SndSePlay(SE_PAGE, 100, 0x1000, 0, nullptr, 0);
                }
                else if (CheckPadRptPlaySe(pad_key[PAD_KEY_PAGE_NEXT], SE_NONE, 0) != 0
                         && CineFilmGetNum() >= CINE_MENU_VIEW_ROWS + 1)
                {
                    MenuCsrPageNextY(csr);
                    SndSePlay(SE_PAGE, 100, 0x1000, 0, nullptr, 0);
                }
                else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) != 0)
                {
                    close = 1;
                }
                else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], SE_DECIDE, 0) != 0)
                {
                    cine_menu_state = CINE_MENU_ST_ASK_ON;
                }
            }
        }
        if (close)
        {
            // The cancel press is turned into OK for the message window.
            pad_wrk[0].trg = (u_short)((pad_wrk[0].trg & ~pad_key[PAD_KEY_CANCEL]) | pad_key[PAD_KEY_OK]);
            LayoutAnimMain();
            cine_menu_state = CINE_MENU_ST_CLOSE;
        }
    }

    if (cine_menu_state == CINE_MENU_ST_ASK)
    {
        if (CheckPadTrgPlaySe(pad_key[PAD_KEY_CANCEL], SE_CANCEL, 0) != 0)
        {
            cine_menu_state = CINE_MENU_ST_ASK_OFF;
        }
        else if (CheckPadTrgPlaySe(pad_key[PAD_KEY_OK], SE_NONE, 0) != 0)
        {
            if (Anim2dMsgGetCsr() == 0)
            {
                int film = CineFilmSelect(CineFilmGetListNo(MenuCsrGetPos(csr)));

                CineFilmPlayReq(GetPlyrAreaNo(), CineFilmTblGetFile(film));
                cine_menu_state = CINE_MENU_ST_PLAY_OUT;
            }
            else
            {
                cine_menu_state = CINE_MENU_ST_ASK_OFF;
            }
        }
    }

    return cine_menu_state == CINE_MENU_ST_END;
}

// FF3 0x00149f58.
void CineMenuTerm(short csr)
{
    if (CineFilmGetNum() > 0)
    {
        CineFilmListRelease();
        MenuCsrRelease(csr);
        pad_wrk[0].stick_dpad = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The layout (draw side)
// ──────────────────────────────────────────────────────────────────────

// FF3 0x00149fa0.
void CineBgAlphaInit(void)
{
    cine_bg_alpha = 0;
}

// FF3 0x00149fa8.
int CineBgFadeIn(void)
{
    if (cine_bg_alpha + 4 < CINE_BG_MAX)
    {
        cine_bg_alpha += 4;
        return 0;
    }
    cine_bg_alpha = CINE_BG_MAX;
    return 1;
}

// FF3 0x00149fd8.
int CineBgFadeOut(void)
{
    if (cine_bg_alpha - 4 > 0)
    {
        cine_bg_alpha -= 4;
        return 0;
    }
    cine_bg_alpha = 0;
    return 1;
}

// The game darkened behind the layout: a black screen-sized square.
// FF3 0x0014a000.
void CineBgDraw(void)
{
    DISP_SQAR ds;
    SQAR_DAT  sq;

    sq.w     = 0x280;
    sq.h     = 0x1c0;
    sq.pri   = 0x40;
    sq.x     = 0;
    sq.y     = 0;
    sq.alpha = (u_char)cine_bg_alpha;
    sq.r     = 0;
    sq.g     = 0;
    sq.b     = 0;
    CopySqrDToSqr(&ds, &sq);
    DispSqrD(&ds);
}

// Item hook of the rows item: the names of the films in the view, the
// cursor row brighter. Rows stop at the list's end counted from the top.
// FF3 0x0014a060.
static int CineMenuDrawRows(const B2D_HOOK_ARGS *args)
{
    const int *xy    = (const int *)args->t1;
    int        alpha = (int)args->stack[2];
    int        num   = CineFilmGetNum();

    if (num > 0)
    {
        short top = MenuCsrGetTopY(cine_csr);
        short cur = MenuCsrGetPosY(cine_csr);
        int   dy  = 0;

        for (int i = 0; i < CINE_MENU_VIEW_ROWS; i++, dy += CINE_ROW_DY)
        {
            if (top + i >= num)
                break;

            short   item = (short)CineFilmTblGetItem(CineFilmGetListNo(top + i));
            u_char *str  = GetMsgDataAddr(CINE_MSG_NAME, item_dat[item]._0e);

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

// FF3 0x0014a1b8.
void CineMenuSetRowHook(void)
{
    B2D_ITEM_HOOK hook = { CineMenuDrawRows };

    B2dCtlSetItemHook(B2dCtlFindItem(BpCtlGetHandleByFileNo(EISYASITU_PHF), CINE_ITEM_ROWS), &hook);
}

// FF3 0x0014a1f8.
void CineScrollBarDraw(void)
{
    int num = CineFilmGetNum();

    if (num > 0)
    {
        BP_OBJ_SCROLL_POS pos;

        pos.first  = MenuCsrGetTopY(cine_csr);
        pos.cursor = MenuCsrGetPosY(cine_csr);
        pos.rows   = (short)num;
        pos.page   = CINE_MENU_VIEW_ROWS;
        cine_obj->DrawScrollBar(&pos, &cine_scroll_arrows);
    }
}

// FF3 0x0014a268.
void CineScrollBarAdd(void)
{
    cine_obj->AddScrollBar(&cine_scroll_def);
    CineScrollBarDraw();
}

// FF3 0x0014a290.
void CineScrollBarDel(void)
{
    cine_obj->DelScrollBar();
}

// FF3 0x0014a2a8.
void CineCsrUpdate(void)
{
    short row = MenuCsrGetPosY(cine_csr);

    if (row < 0)
        row = 0;
    cine_obj->SetCsrPos(CINE_ITEM_CSR, 0, row);
}

// FF3 0x0014a2e0.
void CineCsrAdd(void)
{
    cine_obj->AddCsr(CINE_ITEM_CSR, 0, CINE_ROW_DY, 0);
    CineCsrUpdate();
}

// FF3 0x0014a310.
void CineCsrFree(void)
{
    cine_obj->FreeCsr();
}

// The window's line: the cursor film's description, or "no films".
// FF3 0x0014a328.
void CineHelpSet(void)
{
    int msg0 = CINE_MSG_MENU;
    int msg1 = 1;

    if (CineFilmGetNum() > 0)
    {
        int item = CineFilmTblGetItem(CineFilmGetListNo(MenuCsrGetPos(cine_csr)));

        msg0 = CINE_MSG_DESC;
        msg1 = -1;
        if (item == -1)
            msg0 = -1;
        else
            msg1 = item_dat[(short)item]._0e;
    }
    Anim2dMsgSet(msg0, msg1);
}

// FF3 0x0014a3a0.
void CineHelpMode(int ask)
{
    if (ask != 0)
    {
        Anim2dMsgSetType(1);
        Anim2dMsgSet(CINE_MSG_MENU, 0);
        return;
    }
    Anim2dMsgSetType(2);
    CineHelpSet();
}

// FF3 0x0014a3e8.
void CineMsgWinOpen(void)
{
    cine_win = LayoutAnimStart(CINE_ANIM_WIN, 0);
    LayoutMsgSetKeepOpen(1);
    LayoutAnimSetFlag(cine_win, CINE_ANIM_FLG);
    LayoutAnimSetAlpha(cine_win, 0);
    CineHelpMode(0);
}

// FF3 0x0014a430.
int CineMsgWinIsReady(void)
{
    return LayoutAnimIsDataReady(cine_win) != 0;
}

// FF3 0x0014a458.
void CineMsgWinSetAlphaItem(int handle, int id)
{
    LayoutAnimSetAlphaItemById(cine_win, handle, id);
}

// FF3 0x0014a480.
void CineMsgWinClose(void)
{
    if (cine_win != -1)
    {
        LayoutAnimClearFlag(cine_win, CINE_ANIM_FLG);
        LayoutMsgSetKeepOpen(0);
        LayoutAnimReset(cine_win);
        cine_win = -1;
    }
}

// FF3 0x0014a4c8.
// callcheck: operator_new, BpObjCtor (new BP_OBJ)
void CineLayoutCreate(short csr)
{
    cine_csr = csr;
    CineBgAlphaInit();
    cine_obj = new BP_OBJ(EISYASITU_PHF);
}

// The draw side of the state machine, once per frame.
// FF3 0x0014a510.
// callcheck: GetCmnFadeLayer, CmnFadeLayerUpdate, CmnFadeLayerShow, CmnFadeLayerSetItem (capCtl methods)
void CineLayoutMain(void)
{
    CineBgDraw();
    if ((u_int)(cine_menu_state - CINE_MENU_ST_IN) < 7)
    {
        CineCsrUpdate();
        CineScrollBarDraw();
    }
    GetCmnFadeLayer()->Update();

    if (cine_menu_state == CINE_MENU_ST_OPEN)
    {
        CineMsgWinOpen();
        cine_menu_state = CINE_MENU_ST_LOAD;
    }
    if (cine_menu_state == CINE_MENU_ST_LOAD)
    {
        int loaded = cine_obj->Load();

        if (CineMsgWinIsReady() == 0)
            loaded = 0;
        if (CineBgFadeIn() != 0 && loaded != 0)
        {
            cine_obj->In("in");
            CineMenuSetRowHook();
            CineCsrAdd();
            CineScrollBarAdd();
            GetCmnFadeLayer()->Show(CINE_LABEL_IN);
            GetCmnFadeLayer()->SetItem(cine_obj->GetHandle(), CINE_ITEM_CSR);
            CineMsgWinSetAlphaItem(cine_obj->GetHandle(), CINE_ITEM_CSR);
            cine_menu_state = CINE_MENU_ST_IN;
        }
    }
    if (cine_menu_state == CINE_MENU_ST_IN && cine_obj->IsInEnd() != 0)
        cine_menu_state = CINE_MENU_ST_LIST;
    if (cine_menu_state == CINE_MENU_ST_LIST)
        CineHelpSet();
    if (cine_menu_state == CINE_MENU_ST_ASK_ON)
    {
        CineHelpMode(1);
        cine_menu_state = CINE_MENU_ST_ASK;
    }
    if (cine_menu_state == CINE_MENU_ST_ASK_OFF)
    {
        CineHelpMode(0);
        cine_menu_state = CINE_MENU_ST_LIST;
    }
    if (cine_menu_state == CINE_MENU_ST_PLAY_OUT && cine_obj->Out("out") != 0)
    {
        CineMsgWinClose();
        cine_menu_state = CINE_MENU_ST_UNDARKEN;
    }
    if (cine_menu_state == CINE_MENU_ST_CLOSE && cine_obj->Out("out") != 0)
    {
        CineMsgWinClose();
        cine_menu_state = CINE_MENU_ST_UNDARKEN;
    }
    if (cine_menu_state == CINE_MENU_ST_UNDARKEN && CineBgFadeOut() != 0)
        cine_menu_state = CINE_MENU_ST_END;
}

// FF3 0x0014a758.
// callcheck: operator_delete, BpObjRelease, CmnFadeLayerSetItemHandle, CmnFadeLayerTerm (delete, BP_OBJ::Release, capCtl methods)
void CineLayoutRelease(void)
{
    CineCsrFree();
    CineScrollBarDel();
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    if (cine_obj != nullptr)
    {
        cine_obj->Release();
        delete cine_obj;
    }
    cine_obj = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The film table and the held-film list
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0014a7c8.
int CineFilmTblGetNum(void)
{
    return CINE_FILM_NUM;
}

// FF3 0x0014a7d0.
int CineFilmTblGetItem(int no)
{
    if (no < 0)
        return -1;
    return cine_film_tbl[no].item;
}

// FF3 0x0014a7f8.
int CineFilmTblGetFile(int no)
{
    if (no < 0)
        return -1;
    return cine_film_tbl[no].file;
}

// FF3 0x0014a820.
int *CineFilmTblGetPhotoWin(int no)
{
    if (no < 0)
        return nullptr;
    // cine_film.c only reads the pair; the ROM hands out a plain pointer.
    return const_cast<int *>(cine_film_tbl[no].photo);
}

// FF3 0x0014a840.
int CineFilmTblFindFile(int file)
{
    if (file >= 0)
    {
        int num = CineFilmTblGetNum();

        for (int i = 0; i < num; i++)
        {
            if (cine_film_tbl[i].file == file)
                return i;
        }
    }
    return -1;
}

// FF3 0x0014a8a8.
int CineFilmListCreate(void)
{
    int num = CineFilmTblGetNum();

    cine_list     = (int *)heapCtrlMalloc(GetSystemHeapWrkP(), num * sizeof(int), 0, 0, 0);
    cine_list_num = 0;
    for (int i = 0; i < num; i++)
    {
        int item = CineFilmTblGetItem(i);

        if (item < 0)
            continue;

        short cnt = 0;

        if ((u_short)item < ITEM_NUM)
            cnt = *ItemGetCnt((u_short)item);
        if (cnt != 0)
            cine_list[cine_list_num++] = i;
    }
    if (cine_list_num < 1)
    {
        heapCtrlFree(GetSystemHeapWrkP(), cine_list, 0, 0, 0);
        cine_list = nullptr;
    }
    return cine_list_num;
}

// FF3 0x0014a9a0.
int CineFilmGetNum(void)
{
    return cine_list_num;
}

// FF3 0x0014a9b0.
int CineFilmGetListNo(int pos)
{
    return cine_list[pos];
}

// FF3 0x0014a9c8.
void CineFilmListRelease(void)
{
    heapCtrlFree(GetSystemHeapWrkP(), cine_list, 0, 0, 0);
    cine_list     = nullptr;
    cine_list_num = 0;
}

// Rows without an item read -1, which fails the (u_short) range check.
// FF3 0x0014aa08.
int CineFilmGetHaveNum(void)
{
    int have = 0;

    for (int i = 0; i < CineFilmTblGetNum(); i++)
    {
        u_short item = (u_short)CineFilmTblGetItem(i);
        short   cnt  = 0;

        if (item < ITEM_NUM)
            cnt = *ItemGetCnt(item);
        if (cnt != 0)
            have++;
    }
    return have;
}

// ──────────────────────────────────────────────────────────────────────
// Phase 47 Story_CineMenu
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0014aa90.
void CineMenuStateClear(void)
{
    cine_menu_open = 0;
}

// Also drops a pending press of the finder key this frame.
// FF3 0x0014aa98.
void CineMenuReq(void)
{
    GetGPhaseCtrl()->SetNext(CINE_MENU_PHASE, 0);
    if ((pad_wrk[0].trg & pad_key[PAD_KEY_FINDER]) != 0)
        pad_wrk[0].trg &= (u_short)~pad_key[PAD_KEY_FINDER];
}

// FF3 0x0014ab08.
int CineMenuGetState(void)
{
    return cine_menu_open;
}

// FF3 0x0014ab10.
int init_Story_CineMenu(void)
{
    LockTblInc(3);
    LockTblInc(9);
    LockTblInc(0x17);
    cine_ret_phase = GetGPhaseCtrl()->from;
    CineMenuInit(CINE_MENU_CSR);
    CineLayoutCreate(CINE_MENU_CSR);
    cine_menu_open = 1;
    return 1;
}

// The game runs on under the menu (IngameMain / IngameDrawSub with skip
// flags 9); once it closed, back to the phase that asked for it.
// FF3 0x0014ab78.
int one_Story_CineMenu(void)
{
    IngameMain(CINE_INGAME_SKIP);
    IngameDrawSub(0);
    if (CineMenuCtrl(CINE_MENU_CSR) == 0)
    {
        CineLayoutMain();
        return 0;
    }
    GetGPhaseCtrl()->SetNext(cine_ret_phase, 0);
    return 1;
}

// FF3 0x0014abf0.
int end_Story_CineMenu(void)
{
    CineMenuTerm(CINE_MENU_CSR);
    CineLayoutRelease();
    cine_menu_open = 0;
    LockTblDec(3);
    LockTblDec(9);
    LockTblDec(0x17);
    return 1;
}
