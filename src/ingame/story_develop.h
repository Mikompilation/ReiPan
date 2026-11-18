/* ==========================================================================
 *  ingame/story_develop.h
 *
 *  Developing films (FF3-only; GENZOU_PHF, genzou = developing):
 *
 *  - the develop table: 20 films Rei can shoot (SHOOT_DEVELOP_FILM, photo
 *    step 14) and later develop, one 4-byte row each at data 0x325260
 *    (save block 4, DevelopFilmSaveGet), with their ROM data develop_dat
 *    (rodata 0x359308: pictures, the window message, the document the film
 *    turns into and the memo pages it reveals);
 *  - phase 38 Story_Develop (DEVELOP_REQUEST): the films waiting to be
 *    developed as a scrolling film strip, the current one's picture large;
 *    OK asks "develop this film?" and yes opens
 *  - phase 65 Story_DevelopFilm (a child of 38): the developed picture
 *    dissolves in over the film's picture (150 frames, SE 0x1b790001), then
 *    back in 38 the film's message, its memo rewards and the next film.
 *
 *  FF3 0x1b3078..0x1b57d8 + 0x2b91a8..0x2b94b0 (DevelopListFill and
 *  DevelopLayoutStep, out of line after story_menu_photo.c's DocListFill).
 *  Named after the phases (story_door.c, story_main.c).
 * ======================================================================== */
#ifndef _INGAME_STORY_DEVELOP_H
#define _INGAME_STORY_DEVELOP_H

#include "sdk/scetypes.h"

#include <stddef.h>                     // offsetof

#define DEVELOP_FILM_NUM    20

// The rows' states.
enum DEVELOP_STATE
{
    DEVELOP_STATE_NONE  = 0,            // not shot
    DEVELOP_STATE_SHOT  = 1,            // shot, not handed in yet (GET_DEVELOP_FILM -> 2)
    DEVELOP_STATE_READY = 2,            // waiting to be developed (REMAIN_UNDEVELOP_FILMS)
    DEVELOP_STATE_DONE  = 3,            // developed (GET_DEVELOP_STATUS_)
};

// One film (data 0x325260, 4 bytes each; save block 4).
struct DEVELOP_FILM
{
    /* 0x0 */ s_char state;             // DEVELOP_STATE_*
    /* 0x1 */ s_char area;              // the area it was shot in (GetPlyrAreaNo)
    /* 0x2 */ short  arg;               // the shooter's argument (SHOOT_DEVELOP_FILM / ingame_wrk._28)
};
static_assert(sizeof(DEVELOP_FILM) == 4, "DEVELOP_FILM is 4 bytes");
static_assert(offsetof(DEVELOP_FILM, area) == 1, "DEVELOP_FILM.area");
static_assert(offsetof(DEVELOP_FILM, arg) == 2, "DEVELOP_FILM.arg");

extern DEVELOP_FILM develop_film[DEVELOP_FILM_NUM];     // data 0x325260

// A memo page a developed film reveals (MemoSetPage).
struct DEVELOP_MEMO
{
    /* 0x0 */ short  memo;              // < 0x21; -1 none
    /* 0x2 */ s_char page;
};
static_assert(sizeof(DEVELOP_MEMO) == 4, "DEVELOP_MEMO is 4 bytes");
static_assert(offsetof(DEVELOP_MEMO, page) == 2, "DEVELOP_MEMO.page");

// A film's ROM data (rodata 0x359308, 0x24 bytes each).
struct DEVELOP_DAT
{
    /* 0x00 */ short        no;
    /* 0x02 */ short        menu_pic;   // MENU_PIC files: + 0 the film, + 1 developed, + 2 / + 3 their strip thumbnails
    /* 0x04 */ short        msg;        // message type 2: msg the film, msg + 1 after developing
    /* 0x06 */ short        _06;
    /* 0x08 */ int          soul;       // ghost list entry photographed when developed (< 0xd9; -1 in every row)
    /* 0x0c */ u_short      doc[2];     // the film's document before / after developing (< 0xa4)
    /* 0x10 */ DEVELOP_MEMO memo[3];
    /* 0x1c */ int          _1c;        // 0 in every row, no reader
    /* 0x20 */ int          new_game;   // forgotten on a new game (1 in every row)
};
static_assert(sizeof(DEVELOP_DAT) == 0x24, "DEVELOP_DAT is 0x24 bytes");
static_assert(offsetof(DEVELOP_DAT, menu_pic) == 0x02, "DEVELOP_DAT.menu_pic");
static_assert(offsetof(DEVELOP_DAT, msg) == 0x04, "DEVELOP_DAT.msg");
static_assert(offsetof(DEVELOP_DAT, soul) == 0x08, "DEVELOP_DAT.soul");
static_assert(offsetof(DEVELOP_DAT, doc) == 0x0c, "DEVELOP_DAT.doc");
static_assert(offsetof(DEVELOP_DAT, memo) == 0x10, "DEVELOP_DAT.memo");
static_assert(offsetof(DEVELOP_DAT, _1c) == 0x1c, "DEVELOP_DAT._1c");
static_assert(offsetof(DEVELOP_DAT, new_game) == 0x20, "DEVELOP_DAT.new_game");

extern const DEVELOP_DAT develop_dat[DEVELOP_FILM_NUM];  // rodata 0x359308

void DevelopFilmInit(void);             // FF3 0x001b3078: IngameInit
void DevelopFilmNewGame(void);          // FF3 0x001b30c8: IngameNewGameReset
void DevelopFilmEnd(void);              // FF3 0x001b3118: IngameEnd (nothing)
int  DevelopFilmShoot(short film, int area, char arg);  // FF3 0x001b3130: SHOOT_DEVELOP_FILM; 1 when the row was free
int  DevelopFilmGetAll(void);           // FF3 0x001b31a8: GET_DEVELOP_FILM, shot -> ready; how many
int  DevelopFilmDevelop(short film);    // FF3 0x001b31f0: ready -> developed; 1 when it was ready
int  DevelopFilmIsRemain(void);         // FF3 0x001b32c0: a film is ready (REMAIN_UNDEVELOP_FILMS)
void DevelopFilmItemUpdate(void);       // FF3 0x001b3308: item 0x10 = a film is ready
int  DevelopFilmIsAreaOver(short film); // FF3 0x001b3340: no caller
void DevelopReq(void);                  // FF3 0x001b3378: DEVELOP_REQUEST, phase 38
int  DevelopFilmSaveGet(void **addr);   // FF3 0x001b3398: {0x325260, 0x50} (save block 4)

// The films ready to be developed (8 bytes on the EE; the menu new's one).
// layout: host -- list is a pointer (EE 4 bytes); num follows it.
struct DEVELOP_LIST
{
    DEVELOP_LIST();                     // FF3 0x001b33e8 (DevelopListCtor; base 0x001b33c8)
    ~DEVELOP_LIST();                    // FF3 0x001b3420 (DevelopListDtor)

    void  Build();                      // FF3 0x001b3438
    void  Free();                       // FF3 0x001b34a8
    int   HasRemain();                  // FF3 0x001b34f8: a listed film is still ready
    short Fill(int store);              // FF3 0x002b91a8: the count; with store into list

    /* 0x0 */ short *list;              // film rows (system heap)
    /* 0x4 */ short  num;
};
static_assert(offsetof(DEVELOP_LIST, num) == sizeof(short *), "DEVELOP_LIST.num follows list");

// Phase 38 Story_Develop
int  init_Story_Develop(void);          // FF3 0x001b3580
int  one_Story_Develop(void);           // FF3 0x001b3860
int  end_Story_Develop(void);           // FF3 0x001b3748
void pre_Story_Develop(void);           // FF3 0x001b3850
void after_Story_Develop(void);         // FF3 0x001b3858

// Phase 65 Story_DevelopFilm
int  init_Story_DevelopFilm(void);      // FF3 0x001b5220
int  one_Story_DevelopFilm(void);       // FF3 0x001b55c8
int  end_Story_DevelopFilm(void);       // FF3 0x001b5538

#endif /* _INGAME_STORY_DEVELOP_H */
