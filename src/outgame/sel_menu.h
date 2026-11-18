/* ==========================================================================
 *  outgame/sel_menu.h
 *
 *  The select menu of the game clear menu (phase 43 Story_ClearMenu,
 *  ingame/clear/ClearMenu.c): the CLEARMENU_PHF layout (+ language) on
 *  MenuCsr 0x2b, one row per entry of a list its caller sets, each naming
 *  the result it answers; a yes/no window shows the row's help line and,
 *  on request, the question. FF3 0x1ed658..0x1edd90, FF3's own code (file
 *  name invented); the twin of ingame/gameover_menu.c.
 * ======================================================================== */
#ifndef _OUTGAME_SEL_MENU_H
#define _OUTGAME_SEL_MENU_H

#include <stddef.h>                     // offsetof

// One row of the list (0x14 bytes, the ROM layout); a row with id -1 ends it.
struct SEL_MENU_ROW                     /* 0x14 */
{
    /* 0x00 */ int id;                  // B2D item of the row (its name's 5th character is the row, '1'..)
    /* 0x04 */ int kind;                // message kind of the row text and its help line
    /* 0x08 */ int msg;                 // the row text (SelMenuDrawRow)
    /* 0x0c */ int help;                // the help line (YesNoSetMsg)
    /* 0x10 */ int result;              // SelMenuGetResult once chosen
};
static_assert(sizeof(SEL_MENU_ROW) == 0x14, "SEL_MENU_ROW is 0x14 bytes");
static_assert(offsetof(SEL_MENU_ROW, help) == 0x0c, "SEL_MENU_ROW.help");
static_assert(offsetof(SEL_MENU_ROW, result) == 0x10, "SEL_MENU_ROW.result");

void SelMenuSetList(const SEL_MENU_ROW *list);  // FF3 0x001ed658
void SelMenuInit(void);                 // FF3 0x001ed7f8: result -1, the layout, MenuCsrInit(0x2b, ..)
int  SelMenuLoad(void);                 // FF3 0x001ed998: 1 when loaded; plays "in"
int  SelMenuIsInEnd(void);              // FF3 0x001edae0: the in animation is over
int  SelMenuSelect(void);               // FF3 0x001edb08: 1 when a row is chosen (cancel: the last row)
int  SelMenuConfirm(void);              // FF3 0x001edc18: the yes/no question: 1 yes, -1 no, 0 waiting
int  SelMenuGetResult(void);            // FF3 0x001edcd8: the chosen row's result, -1 none
int  SelMenuIsOut(void);                // FF3 0x001edce0: 1 when "out" is done
void SelMenuRelease(void);              // FF3 0x001edd08

#endif /* _OUTGAME_SEL_MENU_H */
