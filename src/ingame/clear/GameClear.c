// ingame/clear/GameClear.c -- CGameClear, the game-clear sequence (phase 42):
// ending movie, results page, reward lists. FF3 0x0010bba0..0x0010ce28 plus
// the template table finders 0x002abe98 / 0x002abef0; FF3-only (open-items.md
// "0x10bbb0..0x116fa0"). Its first functions had wrong FF2 FurnCtl* names.
#include "ingame/clear/GameClear.h"

#include <new>                          // operator new / delete
#include <string.h>                     // strcmp

#include "album/prg/album.h"            // AlbumSetIngame
#include "common/cmn_util.h"            // STEP_SEQ, ClearReward*, ClearRankGet, NumGetDigits, PrintNumber*
#include "common/cmn_fade_layer.h"      // GetCmnFadeLayer, CmnFadeLayer*
#include "graphics/graph2d/message.h"   // MsgPrintPage, GetMsgDataAddr, MsgGetLineWidth, PrintMsg_ArrangeOneLine, PrintNumber_N
#include "graphics/motion/anicode.h"    // B2D_HOOK_REC, B2dSetHookList, LabelAlignGetX, SetCurColorPacked, SetDefaultGsDrawState
#include "graphics/movie/movie.h"       // EndMovie, MovieDispMain
#include "ingame/ingame.h"              // GPhaseCtrlRequest
#include "ingame/ingame_wrk.h"          // ingame_wrk, PLAY_TIME, IngameWrkGetDifficulty, IngameWrkGetBestScore
#include "ingame/item/prg/soul_list.h"  // SoulListGetHaveNum, SoulListGetNum
#include "ingame/map/map_bgm.h"         // map_bgmFadeOut
#include "ingame/menu/start_menu.h"     // FINDER_STATE_WRK, GetFinderStateWrk
#include "ingame/movie_room_menu/prg/movie_projecter.h"    // MovieReqPlay, MovieReqSetFilmMode, MovieReqSetSkipMask
#include "ingame/sound/snd_ctrl.h"      // SndBgmStopAll, SndAllBufStop
#include "main/gphase.h"                // GetGPhaseCtrl, GPHASE_CTRL::SetNext, GetGPhaseTree
#include "main/gphase_tree.h"           // GPHASE_TREE::Reset
#include "outgame/outgame_bg.h"         // OutgameBg*
#include "outgame/save_info_disp.h"     // SaveInfoPrintPlayTime
#include "system/b2d/B2dBase.h"         // CB2dBase, B2dBase*
#include "system/b2d/B2dCtl.h"          // B2D_ITEM, B2D_HOOK_ARGS, B2dGetDrawItem
#include "system/b2d/BpCtl.h"           // B2dDrawSpriteDirect
#include "system/eeiop/cd_file_dat.h"   // GAMECLEAR_PHF, OUTGAME_BG_PSS, BGM_04_CLEAR_STR_*
#include "system/eeiop/stream_auto.h"   // StreamAutoSetExclusiveMode, StreamAutoPlay
#include "system/os/system.h"           // GetSysWrk, SYS_WRK::GetLanguage
#include "system/pad/pad_ctrl.h"        // CheckPadTrgPlaySe, pad_key
#include "ingame/menu/menu_mouse.h"    // PC: the mouse in menus

// ──────────────────────────────────────────────────────────────────────
// Callees in runs whose file is not settled yet (unassigned@ in tus.json).
// Move each block to its real header when that run is defined.
// ──────────────────────────────────────────────────────────────────────


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

#define GAME_CLEAR_PHASE            0x2a        // phase 42, this sequence
#define GAME_CLEAR_PHASE_NEXT       0x2b        // after a story clear (difficulties 0..3)
#define GAME_CLEAR_PHASE_MISSION    0x0b        // after a mission clear / failure
#define GAME_CLEAR_PHASE_MOVIE      0x43        // linked while the ending movie plays

#define GAME_CLEAR_MOVIE            0x15        // the ending movies (MovieReqPlay)
#define GAME_CLEAR_MOVIE_ENDING2    0x16

#define GAME_CLEAR_SE_OK            0x1a9b0003
#define GAME_CLEAR_MSG_TYPE         0x5b        // the clear screen's messages
#define GAME_CLEAR_COL              0x2d        // message colour label
#define GAME_CLEAR_ITEM_FADE        0x40d       // the fade layer's item
#define GAME_CLEAR_ROW_DY           0x1b        // reward rows, px
#define GAME_CLEAR_ROW_FRAMES       0x1e        // frames between reward rows
#define GAME_CLEAR_TEXT_DY          0x10        // text below its item's y

#define GAME_CLEAR_HIST_NUM         0x100

// The y of a reward list's item, one entry per drawn frame (bss 0x377ff0 and
// 0x3781f4): later rows follow it with a delay, so the list unrolls.
struct GAME_CLEAR_HIST                  /* 0x204 */
{
    /* 0x000 */ int   num;              // entries, stops growing at 0xff
    /* 0x004 */ short y[GAME_CLEAR_HIST_NUM];
};
static_assert(sizeof(GAME_CLEAR_HIST) == 0x204, "GAME_CLEAR_HIST is 0x204 bytes");

static PLAY_TIME       game_clear_time;     // bss 0x377fe0: the play time at the clear
static GAME_CLEAR_HIST game_clear_hist[2];  // bss 0x377ff0: per reward list

static u_int game_clear_kept_finder[2];     // sdata gp-0x9a0 / gp-0x99c
static int   game_clear_mode;               // sdata gp-0x7ec4 (GameClearSetMode)
static int   game_clear_ending;             // sdata gp-0x7ec8: 1 the other ending (movie 0x16)
static int   game_clear_coll_num;           // sdata gp-0x7ec0 (GameClearCountCollection)
static int   game_clear_coll_max;           // sdata gp-0x7ebc
static int   game_clear_movie_req;          // sdata gp-0x7eb8: set with the movie phase, ends CGameClear::Run

// A {key, value} table ended by value -1 (FF3 template code 0x002abe98).
struct GAME_CLEAR_ID_TBL                /* 0x8 */
{
    /* 0x00 */ int id;
    /* 0x04 */ int value;
};
static_assert(sizeof(GAME_CLEAR_ID_TBL) == 0x8, "GAME_CLEAR_ID_TBL is 8 bytes");

// A {name, value} table ended by value -1 (FF3 template code 0x002abef0).
// layout: host -- name is a pointer.
struct GAME_CLEAR_NAME_TBL
{
    const char *name;
    int         value;
};

// Item -> message of the results page labels (data 0x34c338); 0x413 has
// no label.
static const GAME_CLEAR_ID_TBL game_clear_label_tbl[] =
{
    { 0x40f, 0 }, { 0x410, 1 }, { 0x411, 2 }, { 0x412, 3 }, { 0x413, 4 }, { -1, -1 },
};

// Reward -> message (data 0x34c368).
static const GAME_CLEAR_ID_TBL game_clear_reward_tbl[] =
{
    { 0, 5 }, { 1, 6 }, { 2, 7 }, { 3, 8 }, { 4, 9 }, { 5, 11 }, { 9, 10 }, { 6, 12 }, { 7, 13 }, { 8, 14 },
    { -1, -1 },
};

// Sprite item -> difficulty (data 0x34c2d8) and -> rank (data 0x34c300).
static const GAME_CLEAR_NAME_TBL game_clear_difficulty_tbl[] =
{
    { "R Easy", 0 }, { "R Normal", 1 }, { "R Hard", 2 }, { "R Nightmare", 3 }, { nullptr, -1 },
};
static const GAME_CLEAR_NAME_TBL game_clear_rank_tbl[] =
{
    { "R S", 0 }, { "R A", 1 }, { "R B", 2 }, { "R C", 3 }, { "R D", 4 }, { "R E", 5 }, { nullptr, -1 },
};

static const char game_clear_label_in[]     = "gameclear_in";  // data 0x34c258
static const char game_clear_label_omake1[] = "omake1_in";     // data 0x34c0b8
static const char game_clear_label_omake2[] = "omake2_in";     // data 0x34c0a8
static const char game_clear_label_out[]    = "out";           // data 0x36b640

static int GameClearStepStart(STEP_SEQ *seq);
static int GameClearStepMovie(STEP_SEQ *seq);
static int GameClearStepBgLoad(STEP_SEQ *seq);
static int GameClearStepBgReady(STEP_SEQ *seq);
static int GameClearStepLoad(STEP_SEQ *seq);
static int GameClearStepIn(STEP_SEQ *seq);
static int GameClearStepResult(STEP_SEQ *seq);
static int GameClearStepRewards(STEP_SEQ *seq);
static int GameClearStepRewards2(STEP_SEQ *seq);
static int GameClearStepOut(STEP_SEQ *seq);
static int GameClearStepBgEnd(STEP_SEQ *seq);
static int GameClearStepEnd(STEP_SEQ *seq);

// ──────────────────────────────────────────────────────────────────────
// The table finders. FF3 0x002abe98 / 0x002abef0.

static const GAME_CLEAR_ID_TBL *GameClearTblFindId(const GAME_CLEAR_ID_TBL *tbl, const int *id)
{
    for (int i = 0; tbl[i].value != -1; i++)
    {
        if (tbl[i].id == *id)
            return &tbl[i];
    }
    return nullptr;
}

static const GAME_CLEAR_NAME_TBL *GameClearTblFindName(const GAME_CLEAR_NAME_TBL *tbl, const char *const *name)
{
    for (int i = 0; tbl[i].value != -1; i++)
    {
        if (strcmp(tbl[i].name, *name) == 0)
            return &tbl[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The owner's layout. FF3 0x0010bba0.

static CB2dBase *GameClearGetLayout(STEP_SEQ *seq)
{
    return ((CGameClear *)seq->owner)->layout;
}

// ──────────────────────────────────────────────────────────────────────
// A clear screen message. FF3 0x0010bbb0 (was FurnCtlCheckKakuType).

static void GameClearPrintMsg(int msg, int x, int y, int alpha)
{
    MsgPrintPage(GAME_CLEAR_MSG_TYPE, msg, x, y, GAME_CLEAR_COL, alpha, 0, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// out_lo below lo, out_hi above hi, else v mapped linearly.
// FF3 0x0010bbf8 (was FurnCtlClearAll).

static int GameClearLinearMap(int lo, int hi, int v, int out_lo, int out_hi)
{
    if (v < lo)
        return out_lo;
    if (v > hi)
        return out_hi;
    return (int)(((float)(v - lo) / (float)(hi - lo)) * (float)(out_hi - out_lo) + (float)out_lo);
}

// ──────────────────────────────────────────────────────────────────────
// The open rewards lo..hi, centred at x. y joins the list's history; row r
// (of the open ones) appears GAME_CLEAR_ROW_FRAMES * r frames after the
// list started, at the y the item had then, r rows down, fading out over
// y 280..300. FF3 0x0010bc60.

static void GameClearDrawRewardList(int lo, int hi, int x, int y, int alpha, u_int side)
{
    GAME_CLEAR_HIST *hist = &game_clear_hist[side & 1];
    int              num  = hist->num;
    int              wait = 0;
    int              ofs  = 0;

    hist->y[num] = (short)y;
    if (num < 0xff)
        hist->num = num + 1;

    for (int i = lo; i <= hi; i++)
    {
        if (ClearRewardIsOpen(i) == 0)
            continue;

        num = hist->num;
        int back    = num - wait;
        int visible = wait < num;

        wait += GAME_CLEAR_ROW_FRAMES;
        if (visible)
        {
            // FF3 carries y and alpha over from row to row: a row below the
            // fade band dims the rows after it too.
            if (back < num)
                y = hist->y[back];
            y += ofs;
            alpha = alpha * (0xff - GameClearLinearMap(0x118, 300, y, 0, 0xff)) / 0xff;
            MsgPrintPage(GAME_CLEAR_MSG_TYPE, GameClearTblFindId(game_clear_reward_tbl, &i)->value, x, y,
                         GAME_CLEAR_COL, alpha, 0, 0, 0, 2);
        }
        y   += GAME_CLEAR_ROW_DY;
        ofs += GAME_CLEAR_ROW_DY;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The item hooks (list 0x34c0e8). Text hooks: t1 the item's rect, t3 its
// colour, stack[2] its alpha (B2dCtl.h).

// Item 0x3f5: the first list with an open reward, history 0. FF3 0x0010be00.
static int GameClearHookRewardList(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;
    int        lo = 0;
    int        hi = 5;

    SetCurColorPacked((u_int)args->t3);
    if (ClearRewardAny(0, 5) == 0)
    {
        lo = 6;
        hi = 9;
    }
    GameClearDrawRewardList(lo, hi, LabelAlignGetX(2, xy), xy[1], (int)args->stack[2], 0);
    SetDefaultGsDrawState();
    return 0;
}

// Item 0x3f6: rewards 6..9, history 1. FF3 0x0010be90.
static int GameClearHookRewardList2(const B2D_HOOK_ARGS *args)
{
    const int *xy = (const int *)args->t1;

    SetCurColorPacked((u_int)args->t3);
    GameClearDrawRewardList(6, 9, LabelAlignGetX(2, xy), xy[1], (int)args->stack[2], 1);
    SetDefaultGsDrawState();
    return 0;
}

// Items 0x40f..0x418: the labels, or the values. a1 is the digit step of
// the play time and best score. FF3 0x0010bef0.
static int GameClearHookStat(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM   *item  = B2dGetDrawItem();
    const int  *xy    = (const int *)args->t1;
    int         step  = (int)args->a1;
    int         alpha = (int)args->stack[2];
    int         x     = xy[0];
    int         y     = xy[1] + GAME_CLEAR_TEXT_DY;
    const GAME_CLEAR_ID_TBL *label;

    SetCurColorPacked((u_int)args->t3);
    label = GameClearTblFindId(game_clear_label_tbl, &item->id);
    if (label != nullptr)
    {
        if (item->id != 0x413)
            GameClearPrintMsg(label->value, x, y, alpha);
    }
    else
    {
        switch (item->id)
        {
        case 0x414:                     // the dream world count (ingame_wrk.b04, 0xff after init)
        {
            int     cnt = (s_char)ingame_wrk.b04 + 1;
            u_char *str = GetMsgDataAddr(0x69, 9);
            int     w   = MsgGetLineWidth(str, nullptr);

            PrintMsg_ArrangeOneLine(str, x, y, GAME_CLEAR_COL, alpha, 0, 0, 0, 0);
            PrintNumber_N(cnt, NumGetDigits(cnt, 1), x + w + 4, y, GAME_CLEAR_COL, (u_char)alpha, 0, 1, 0);
            break;
        }
        case 0x415:
            SaveInfoPrintPlayTime(&game_clear_time, x, y, step, alpha, 0);
            break;
        case 0x416:
        {
            u_int score = IngameWrkGetBestScore();

            PrintNumberOrZero((int)score, x, y, (u_char)alpha);
            PrintMsg_ArrangeOneLine(GetMsgDataAddr(GAME_CLEAR_MSG_TYPE, 0xf), x + step * NumGetDigits((int)score, 1),
                                    y, GAME_CLEAR_COL, alpha, 0, 0, 0, 0);
            break;
        }
        case 0x417:
            PrintNumberPair(game_clear_coll_num, game_clear_coll_max, x, y, alpha, 1);
            break;
        }
    }
    SetDefaultGsDrawState();
    return 0;
}

// Sprites "R Easy" .. "R Nightmare": the played difficulty only.
// FF3 0x0010c130 (created this session; was inside 0x0010bef0).
static int GameClearHookDifficulty(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM *item = B2dGetDrawItem();

    if (GameClearTblFindName(game_clear_difficulty_tbl, &item->name)->value == IngameWrkGetDifficulty(1))
        B2dDrawSpriteDirect(args);
    return 0;
}

// Sprites "R S" .. "R E": the rank of the play time only. FF3 0x0010c220.
static int GameClearHookRank(const B2D_HOOK_ARGS *args)
{
    B2D_ITEM *item = B2dGetDrawItem();

    if (GameClearTblFindName(game_clear_rank_tbl, &item->name)->value == ClearRankGet(&game_clear_time))
        B2dDrawSpriteDirect(args);
    return 0;
}

// Data 0x34c0e8.
static const B2D_HOOK_REC game_clear_hooks[] =
{
    { GameClearHookRewardList,  0, 0x3f5, nullptr },
    { GameClearHookRewardList2, 0, 0x3f6, nullptr },
    { GameClearHookStat,        0, 0x40f, nullptr },
    { GameClearHookStat,        0, 0x410, nullptr },
    { GameClearHookStat,        0, 0x411, nullptr },
    { GameClearHookStat,        0, 0x412, nullptr },
    { GameClearHookStat,        0, 0x413, nullptr },
    { GameClearHookStat,        0, 0x414, nullptr },
    { GameClearHookStat,        0, 0x415, nullptr },
    { GameClearHookStat,        0, 0x416, nullptr },
    { GameClearHookStat,        0, 0x417, nullptr },
    { GameClearHookStat,        0, 0x418, nullptr },
    { GameClearHookDifficulty,  0, 0, "R Easy" },
    { GameClearHookDifficulty,  0, 0, "R Normal" },
    { GameClearHookDifficulty,  0, 0, "R Hard" },
    { GameClearHookDifficulty,  0, 0, "R Nightmare" },
    { GameClearHookRank,        0, 0, "R S" },
    { GameClearHookRank,        0, 0, "R A" },
    { GameClearHookRank,        0, 0, "R B" },
    { GameClearHookRank,        0, 0, "R C" },
    { GameClearHookRank,        0, 0, "R D" },
    { GameClearHookRank,        0, 0, "R E" },
    { nullptr,                 -1, -1, nullptr },
};

// ──────────────────────────────────────────────────────────────────────
// The steps (data 0x34c2a0, in this order). Each returns STEP_* (LightCtl.h).

// Sound off. Missions return to phase 0xb; the story plays its ending
// movie under phase 0x43. FF3 0x0010c928.
static int GameClearStepStart(STEP_SEQ *seq)
{
    (void)seq;
    SndBgmStopAll();
    SndAllBufStop();
    if ((u_int)(GameClearGetMode() - GAME_CLEAR_MODE_MISSION_CLEAR) < 2)
    {
        GetGPhaseCtrl()->SetNext(GAME_CLEAR_PHASE_MISSION, 0);
        AlbumSetIngame(1, 0);
        return STEP_END;
    }

    MovieReqPlay(game_clear_ending == 0 ? GAME_CLEAR_MOVIE : GAME_CLEAR_MOVIE_ENDING2, 1);
    MovieReqSetFilmMode(0);
    MovieReqSetSkipMask(0x800);
    GetGPhaseCtrl()->SetNext(GAME_CLEAR_PHASE_MOVIE, 1);
    game_clear_movie_req = 1;
    return STEP_NEXT;
}

// The movie to its end; then the outgame backdrop movie and the clear BGM.
// FF3 0x0010c880.
static int GameClearStepMovie(STEP_SEQ *seq)
{
    (void)seq;
    if (MovieDispMain(1, -1) == 0)
        return STEP_CONT;

    EndMovie();
    OutgameBgStart(0, OUTGAME_BG_PSS, -1);
    map_bgmFadeOut(30, 0);
    GetGPhaseTree()->Reset();
    StreamAutoSetExclusiveMode(1, 30);
    StreamAutoPlay(BGM_04_CLEAR_STR_STR, BGM_04_CLEAR_STR_HXD, 0xb, 0, 1, 0x3200, 0, nullptr);
    return STEP_NEXT;
}

// FF3 0x0010c840.
static int GameClearStepBgLoad(STEP_SEQ *seq)
{
    (void)seq;
    if (OutgameBgIsLoaded() == 0)
        return STEP_CONT;
    return OutgameBgLoadWait() != 0 ? STEP_NEXT : STEP_CONT;
}

// FF3 0x0010c818.
static int GameClearStepBgReady(STEP_SEQ *seq)
{
    (void)seq;
    OutgameBgDraw();
    return OutgameBgIsReady() != 0;
}

// FF3 0x0010c7b8.
static int GameClearStepLoad(STEP_SEQ *seq)
{
    OutgameBgDraw();
    GameClearGetLayout(seq)->Load(GetSysWrk()->GetLanguage() + GAMECLEAR_PHF);
    return STEP_NEXT;
}

// Layout loaded: the hooks, "gameclear_in", the fade layer on item 0x40d.
// FF3 0x0010c6e0 (created this session; was inside 0x0010c4b0).
static int GameClearStepIn(STEP_SEQ *seq)
{
    CB2dBase *layout = GameClearGetLayout(seq);

    OutgameBgDraw();
    if (layout->IsReady() == 0)
        return STEP_CONT;

    B2dSetHookList(layout->handle, game_clear_hooks);
    layout->Play(game_clear_label_in);
    GetCmnFadeLayer()->SetItemHandle(layout->FindItem(GAME_CLEAR_ITEM_FADE));
    GetCmnFadeLayer()->Show(10);
    return STEP_NEXT;
}

// The results page: OK goes on to the rewards ("omake1_in"), or out when
// none is open. FF3 0x0010c5c8 (created this session; was inside 0x0010c4b0).
static int GameClearStepResult(STEP_SEQ *seq)
{
    CB2dBase *layout = GameClearGetLayout(seq);
    u_int     rewards;

    OutgameBgDraw();
    if (layout->IsPlayEnd() == 0)
        return STEP_CONT;
    ReiPan_MenuMouseConfirm();          // PC: a click goes on
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], GAME_CLEAR_SE_OK, 0) == 0)
        return STEP_CONT;

    rewards = ClearRewardAny(0, 5) | (ClearRewardAny(6, 9) << 1);
    ((CGameClear *)seq->owner)->rewards = rewards;
    if (rewards == 0)
    {
        seq->Jump(GameClearStepOut);
        layout->PlayEx(game_clear_label_out);
        return STEP_NEXT;
    }
    if ((int)rewards >= 0 && (int)rewards < 4)
    {
        game_clear_hist[0].num = 0;
        game_clear_hist[1].num = 0;
        seq->Jump(GameClearStepRewards);
        layout->Play(game_clear_label_omake1);
    }
    return STEP_NEXT;
}

// The first reward list, once all its rows are out: OK shows the second
// ("omake2_in") when both have rewards, else plays out.
// FF3 0x0010c4b0.
static int GameClearStepRewards(STEP_SEQ *seq)
{
    CB2dBase *layout = GameClearGetLayout(seq);
    u_int     rewards;
    int       lo = 0;
    int       hi = 5;

    OutgameBgDraw();
    if (layout->IsPlayEnd() == 0)
        return STEP_CONT;

    rewards = ((CGameClear *)seq->owner)->rewards;
    if ((rewards & GAME_CLEAR_REWARD_LIST1) == 0)
    {
        lo = 6;
        hi = 9;
    }
    if (game_clear_hist[0].num < ClearRewardCount(lo, hi) * GAME_CLEAR_ROW_FRAMES + GAME_CLEAR_ROW_FRAMES)
        return STEP_CONT;
    ReiPan_MenuMouseConfirm();          // PC: a click goes on
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], GAME_CLEAR_SE_OK, 0) == 0)
        return STEP_CONT;

    if ((rewards & GAME_CLEAR_REWARD_LIST1) != 0 && (rewards & GAME_CLEAR_REWARD_LIST2) != 0)
    {
        layout->Play(game_clear_label_omake2);
        return STEP_NEXT;
    }
    seq->Jump(GameClearStepOut);
    layout->PlayEx(game_clear_label_out);
    return STEP_NEXT;
}

// The second reward list, once all its rows are out: OK plays out.
// FF3 0x0010c400.
static int GameClearStepRewards2(STEP_SEQ *seq)
{
    CB2dBase *layout = GameClearGetLayout(seq);

    OutgameBgDraw();
    if (layout->IsPlayEnd() == 0)
        return STEP_CONT;
    if (game_clear_hist[1].num < ClearRewardCount(6, 9) * GAME_CLEAR_ROW_FRAMES + GAME_CLEAR_ROW_FRAMES)
        return STEP_CONT;
    ReiPan_MenuMouseConfirm();          // PC: a click goes on
    if (CheckPadTrgPlaySe((u_short)pad_key[PAD_KEY_OK], GAME_CLEAR_SE_OK, 0) == 0)
        return STEP_CONT;

    layout->PlayEx(game_clear_label_out);
    return STEP_NEXT;
}

// "out" played: the backdrop goes. FF3 0x0010c3b0.
static int GameClearStepOut(STEP_SEQ *seq)
{
    OutgameBgDraw();
    if (GameClearGetLayout(seq)->IsPlayEnd() == 0)
        return STEP_CONT;
    OutgameBgStop();
    return STEP_NEXT;
}

// FF3 0x0010c370.
static int GameClearStepBgEnd(STEP_SEQ *seq)
{
    int stopped;

    (void)seq;
    OutgameBgDraw();
    stopped = OutgameBgIsStopped();
    if (stopped != 0)
        OutgameBgRelease();
    return stopped != 0;
}

// A story clear goes on to phase 0x2b. FF3 0x0010c318.
static int GameClearStepEnd(STEP_SEQ *seq)
{
    int mode = GameClearGetMode();

    (void)seq;
    if (mode >= 0 && mode < 4)
        GetGPhaseCtrl()->SetNext(GAME_CLEAR_PHASE_NEXT, 0);
    return STEP_END;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010cae8 (GameClearCtor; FF3 0x0010c9f8 GameClearCtorBase is its
// twin, copying the same step list from 0x34c268).
// callcheck: operator_new, B2dBaseCtor, StepSeqInit (new CB2dBase, new STEP_SEQ)

CGameClear::CGameClear()
{
    static const STEP_FUNC steps[] =
    {
        GameClearStepStart, GameClearStepMovie, GameClearStepBgLoad, GameClearStepBgReady,
        GameClearStepLoad, GameClearStepIn, GameClearStepResult, GameClearStepRewards,
        GameClearStepRewards2, GameClearStepOut, GameClearStepBgEnd, GameClearStepEnd,
        nullptr,
    };

    layout = new CB2dBase;
    seq = new STEP_SEQ;
    seq->Add(steps);
    seq->owner = this;
    // PC: FF3 leaves rewards to GameClearStepResult (operator new memory).
    rewards = 0;
    game_clear_time = ingame_wrk.play_time;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x0010cc28 (GameClearDtor; base 0x0010cbd8, deleting 0x0010cc78).
// callcheck: operator_delete (delete seq)

CGameClear::~CGameClear()
{
    if (layout != nullptr)
        delete layout;                  // FF3: virtual deleting dtor (vtable +0x04)
    delete seq;
}

// ──────────────────────────────────────────────────────────────────────
// One frame. Finished, or the movie phase requested: the fade layer goes.
// FF3 0x0010ccd0.

int CGameClear::Run()
{
    if (seq->Run() == 0 && game_clear_movie_req == 0)
        return 0;

    game_clear_movie_req = 0;
    GetCmnFadeLayer()->SetItemHandle(nullptr);
    GetCmnFadeLayer()->Term();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// GAME_CLEAR_: before it sets them. FF3 0x0010cd28.

void GameClearKeepFinderFlags(void)
{
    FINDER_STATE_WRK *wrk = GetFinderStateWrk();

    game_clear_kept_finder[0] = (u_int)wrk->_04;
    game_clear_kept_finder[1] = (u_int)wrk->_08;
}

// FF3 0x0010cd68.
int GameClearGetKeptFinderFlag(u_int i)
{
    return (int)game_clear_kept_finder[i & 1];
}

// FF3 0x0010cd80 (was FurnCtlFindInit).
void GameClearSetMode(int mode, int ending)
{
    game_clear_mode   = mode;
    game_clear_ending = ending;
}

// FF3 0x0010cd90.
int GameClearGetMode(void)
{
    return game_clear_mode;
}

// FF3 0x0010cd98 (was FurnCtlDeleteDrawFlgAll).
void GameClearRequest(void)
{
    GPhaseCtrlRequest(GAME_CLEAR_PHASE, 0);
    game_clear_movie_req = 0;
}

// FF3 0x0010cdc0 (was FurnCtlGetSize).
void GameClearCountCollection(void)
{
    game_clear_coll_num = SoulListGetHaveNum();
    game_clear_coll_max = SoulListGetNum();
}

// FF3 0x0010cdf0 / 0x0010ce10 are the atexit thunks of the GPhase singletons
// this file's inline getters instantiate (GPhaseCtrlTerm(0x41e468), the tree
// at gp+0x6a0); the host's singletons destroy themselves (main/gphase.h).
