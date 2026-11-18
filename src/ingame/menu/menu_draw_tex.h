/* ==========================================================================
 *  ingame/menu/menu_draw_tex.h
 *
 *  Menu pictures (FF3-only, 0x209610..0x20b148, after the B2d message hooks
 *  and before the cursor objects of ingame/menu/menu_csr_obj.c; the link
 *  slot FF2's menu_map.c had). Three pieces:
 *
 *  - Menu_DrawTex (RTTI, vtable 0x341fe0): one picture file. Load mallocs
 *    the file's size from the system heap and LoadReq's it; Draw sends the
 *    TIM2 and draws it as one sprite. Menu_DrawTexMulti (vtable 0x341fc0)
 *    takes its buffer from the shared file cache at 0x32d700 instead
 *    (file_cache.h), so pictures several menus show are loaded once.
 *  - MENU_PIC: a picture view with two Menu_DrawTex slots that cross-fades
 *    from one picture to the next (album cover icons, message pictures, the
 *    item-get window, CTutorial). Callers set `pic` to the file wanted and
 *    call Step every frame, Draw + DrawNext to draw.
 *  - The same two-slot cross-fade for message texts (MENU_MSG_OBJ, no
 *    callers in the retail game) and for the title menu's item pictures
 *    (TITLE_MENU_OBJ, outgame/title_menu.c).
 *
 *  The fade: cnt runs from len down to -1; GetFade is fade_max at rest and
 *  lerps fade_min..fade_max with cnt / len while fading. slot[cur] is the
 *  outgoing one (alpha GetFade), slot[cur ^ 1] the incoming one (alpha
 *  fade_max - GetFade); at the end cur flips and the old slot is freed.
 * ======================================================================== */
#ifndef _INGAME_MENU_MENU_DRAW_TEX_H
#define _INGAME_MENU_MENU_DRAW_TEX_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"

// Menu_DrawTex::file values besides file numbers.
#define MENU_DRAW_TEX_NONE      (-1)    // nothing set
#define MENU_DRAW_TEX_BLANK     (-2)    // set, ready, draws nothing

// The fixed VRAM place of the menu pictures (TBP0 / CBP, in 64-word blocks).
#define MENU_DRAW_TEX_TBP       0x2cc4
#define MENU_DRAW_TEX_CBP       0x2bc0

// GS register values the pictures use.
#define MENU_DRAW_TEX_ALPHA_BLEND   0x44        // (Cs - Cd) * As + Cd
#define MENU_DRAW_TEX_TEST_ALWAYS   0x30003     // alpha test always, z always
#define MENU_DRAW_TEX_TEST_AREF     0x3000d     // alpha test GREATER, ref << 4, z always

// One picture file (EE 0x38 bytes; RTTI Menu_DrawTex, vtable 0x341fe0).
// layout: host -- the vptr and buf are 4 bytes on the EE, 8 here; the fields
// from `file` on keep their EE distances (asserted below).
class Menu_DrawTex
{
public:
    Menu_DrawTex();                         // FF3 0x00209670 MenuDrawTexCtor (base copy 0x00209610)
    virtual ~Menu_DrawTex();                // EE vtable +0x00 0x002096f8, deleting +0x04 0x00209720
    virtual void Load(int file_no);         // EE vtable +0x08: MenuDrawTexLoad 0x00209770
    virtual void Release();                 // EE vtable +0x0c: MenuDrawTexRelease 0x00209820
    virtual int  IsReady();                 // EE vtable +0x10: MenuDrawTexIsReady 0x002098a0

    void Clear();                           // FF3 0x00209758: no buffer, no load, no file
    int IsSet();                            // FF3 0x002098f0: file != -1
    int IsFile(int file_no);                // FF3 0x00209900
    void SetVram(void *tim2);               // FF3 0x00209910 (no callers)
    void FixTex0(void *tim2);               // FF3 0x00209960
    void Draw(int x, int y, int z, int pri, u_char r, u_char g, u_char b, u_char alpha, int rot);  // FF3 0x00209a10

    /* 0x04 */ void  *buf;                  // the file's data (TIM2), 0: none
    /* 0x08 */ int    file;                 // file number, MENU_DRAW_TEX_NONE / _BLANK
    /* 0x0c */ int    load_id;              // LoadReq id, -1: none
    /* 0x10 */ int    u;                    // texel rectangle drawn (SPRT_DAT u/v/w/h)
    /* 0x14 */ int    v;
    /* 0x18 */ int    w;
    /* 0x1c */ int    h;
    /* 0x20 */ u_long alphar;               // GS ALPHA
    /* 0x28 */ u_long test;                 // GS TEST
    /* 0x30 */ int    direct;               // 1: placed at MENU_DRAW_TEX_TBP / _CBP (Menu_DrawTex::FixTex0)
};
static_assert(offsetof(Menu_DrawTex, u)       - offsetof(Menu_DrawTex, file) == 0x10 - 0x08, "Menu_DrawTex.u");
static_assert(offsetof(Menu_DrawTex, load_id) - offsetof(Menu_DrawTex, file) == 0x0c - 0x08, "Menu_DrawTex.load_id");
static_assert(offsetof(Menu_DrawTex, h)       - offsetof(Menu_DrawTex, file) == 0x1c - 0x08, "Menu_DrawTex.h");
static_assert(offsetof(Menu_DrawTex, alphar)  - offsetof(Menu_DrawTex, file) == 0x20 - 0x08, "Menu_DrawTex.alphar");
static_assert(offsetof(Menu_DrawTex, test)    - offsetof(Menu_DrawTex, file) == 0x28 - 0x08, "Menu_DrawTex.test");
static_assert(offsetof(Menu_DrawTex, direct)  - offsetof(Menu_DrawTex, file) == 0x30 - 0x08, "Menu_DrawTex.direct");

// A picture from the shared load table (RTTI Menu_DrawTexMulti : Menu_DrawTex,
// vtable 0x341fc0; EE 0x38 bytes, no fields of its own).
// layout: host -- as Menu_DrawTex.
class Menu_DrawTexMulti : public Menu_DrawTex
{
public:
    Menu_DrawTexMulti();                    // FF3 0x00209be0 MenuDrawTexMultiCtor (base copy 0x00209bb0)
    ~Menu_DrawTexMulti() override;          // EE vtable +0x00 0x00209c48, deleting +0x04 0x00209c80
    void Load(int file_no) override;        // EE vtable +0x08: MenuDrawTexMultiLoad 0x00209cc0
    void Release() override;                // EE vtable +0x0c: MenuDrawTexMultiRelease 0x00209d00
    int  IsReady() override;                // EE vtable +0x10: MenuDrawTexMultiIsReady 0x00209d40
};
static_assert(sizeof(Menu_DrawTexMulti) == sizeof(Menu_DrawTex), "Menu_DrawTexMulti adds no fields");

// The picture cross-fader (EE 0x60 bytes, operator new(0x60) + the
// constructor; no vtable, but C++: C1/C2 constructor copies and a destructor).
// layout: host -- the two slot pointers come first; the fields from `cur` on
// keep their EE distances (asserted below).
struct MENU_PIC
{
    MENU_PIC(int multi);                // FF3 0x00209eb8 (MenuPicCtor; base-object copy 0x00209d88 MenuPicCtorBase): two slots, Menu_DrawTexMulti when multi
    ~MENU_PIC();                        // FF3 0x0020a040 (MenuPicDtor): deletes the two slots
    void DrawSlot(int slot, u_char alpha, u_long alphar, u_long test);  // FF3 0x0020a098 (MenuPicDrawSlot)
    int  GetFade();                     // FF3 0x0020a288 (MenuPicGetFade)
    int  IsNextReady();                 // FF3 0x0020a318 (MenuPicIsNextReady)
    void Draw();                        // FF3 0x0020a350 (MenuPicDraw): load[cur]
    void DrawNext();                    // FF3 0x0020a3f8 (MenuPicDrawNext): load[cur ^ 1]
    int  CutFade();                     // FF3 0x0020a4a8 (MenuPicCutFade)
    int  Step();                        // FF3 0x0020a4e8 (MenuPicStep): per frame; 1 while fading

    /* 0x00 */ Menu_DrawTex *load[2];   // load[cur] shown (fading out), load[cur ^ 1] coming in
    /* 0x08 */ s_char  cur;
    /* 0x09 */ u_char  fade_min;        // 0
    /* 0x0a */ u_char  fade_max;        // 0x80
    /* 0x0c */ float   scale;           // alpha factor (the caller's alpha / 128)
    /* 0x10 */ u_long  alphar;          // GS ALPHA, MENU_DRAW_TEX_ALPHA_BLEND
    /* 0x18 */ int     fix;             // 1: ALPHA (Cs - Cd) * FIX + Cd, FIX = the alpha
    /* 0x1c */ int     mode;            // 0 cross-fade, 1 dissolve (alpha test)
    /* 0x20 */ int     x;
    /* 0x24 */ int     y;
    /* 0x28 */ int     z;
    /* 0x2c */ int     pri;
    /* 0x30 */ u_char  rgb[3];          // 0x80, 0x80, 0x80
    /* 0x34 */ int     u;               // texel rectangle, copied into the slot when drawn
    /* 0x38 */ int     v;
    /* 0x3c */ int     w;
    /* 0x40 */ int     h;
    /* 0x44 */ int     rot;             // != 0: rotation about (x, y)
    /* 0x48 */ int     pic;             // file wanted next; -1 none pending, -2 blank
    /* 0x4c */ int     fade;            // fade counter, len .. 0; -1: idle
    /* 0x50 */ int     fade_len;        // 8
    /* 0x54 */ int     enable;          // Step starts and runs fades
    /* 0x58 */ int     release_wait;    // frames until the old slot is freed (CutFade)
    /* 0x5c */ int     _05c;
};
static_assert(offsetof(MENU_PIC, load) == 0, "MENU_PIC.load");
static_assert(offsetof(MENU_PIC, fade_max) - offsetof(MENU_PIC, cur) == 0x0a - 0x08, "MENU_PIC.fade_max");
static_assert(offsetof(MENU_PIC, scale)    - offsetof(MENU_PIC, cur) == 0x0c - 0x08, "MENU_PIC.scale");
static_assert(offsetof(MENU_PIC, alphar)   - offsetof(MENU_PIC, cur) == 0x10 - 0x08, "MENU_PIC.alphar");
static_assert(offsetof(MENU_PIC, fix)      - offsetof(MENU_PIC, cur) == 0x18 - 0x08, "MENU_PIC.fix");
static_assert(offsetof(MENU_PIC, mode)     - offsetof(MENU_PIC, cur) == 0x1c - 0x08, "MENU_PIC.mode");
static_assert(offsetof(MENU_PIC, x)        - offsetof(MENU_PIC, cur) == 0x20 - 0x08, "MENU_PIC.x");
static_assert(offsetof(MENU_PIC, pri)      - offsetof(MENU_PIC, cur) == 0x2c - 0x08, "MENU_PIC.pri");
static_assert(offsetof(MENU_PIC, rgb)      - offsetof(MENU_PIC, cur) == 0x30 - 0x08, "MENU_PIC.rgb");
static_assert(offsetof(MENU_PIC, u)        - offsetof(MENU_PIC, cur) == 0x34 - 0x08, "MENU_PIC.u");
static_assert(offsetof(MENU_PIC, h)        - offsetof(MENU_PIC, cur) == 0x40 - 0x08, "MENU_PIC.h");
static_assert(offsetof(MENU_PIC, rot)      - offsetof(MENU_PIC, cur) == 0x44 - 0x08, "MENU_PIC.rot");
static_assert(offsetof(MENU_PIC, pic)      - offsetof(MENU_PIC, cur) == 0x48 - 0x08, "MENU_PIC.pic");
static_assert(offsetof(MENU_PIC, fade)     - offsetof(MENU_PIC, cur) == 0x4c - 0x08, "MENU_PIC.fade");
static_assert(offsetof(MENU_PIC, enable)   - offsetof(MENU_PIC, cur) == 0x54 - 0x08, "MENU_PIC.enable");
static_assert(offsetof(MENU_PIC, release_wait) - offsetof(MENU_PIC, cur) == 0x58 - 0x08, "MENU_PIC.release_wait");
static_assert(offsetof(MENU_PIC, _05c)     - offsetof(MENU_PIC, cur) == 0x5c - 0x08, "MENU_PIC is 0x60 bytes on the EE");

// One message of a MENU_MSG_OBJ (0xc bytes); -1 fields: empty.
struct MENU_MSG_SLOT
{
    /* 0x00 */ int type;                // message type (MsgPrintPageZ)
    /* 0x04 */ int id;
    /* 0x08 */ int page;                // MsgSetPage
};
static_assert(sizeof(MENU_MSG_SLOT) == 0xc, "MENU_MSG_SLOT is 0xc bytes");

// The message-text cross-fader (no callers in the retail game; size past
// 0x54 unknown). C++: C1/C2 constructor and destructor copies.
struct MENU_MSG_OBJ                     /* >= 0x54 */
{
    MENU_MSG_OBJ();                     // FF3 0x0020a818 (MenuMsgObjCtor; base-object copy 0x0020a770 MenuMsgObjCtorBase)
    ~MENU_MSG_OBJ();                    // FF3 0x0020a910 (MenuMsgObjDtor; base-object copy 0x0020a8c0 MenuMsgObjDtorBase)
    void ClearSlot(int slot);                                   // FF3 0x0020a960 (MenuMsgObjClearSlot)
    void SetSlot(int slot, int type, int id, int page);         // FF3 0x0020a988 (MenuMsgObjSetSlot)
    int  IsSlot(int slot, int type, int id, int page);          // FF3 0x0020a9b0 (MenuMsgObjIsSlot)
    void DrawSlot(int slot, int alpha);                         // FF3 0x0020a9f8 (MenuMsgObjDrawSlot)
    int  GetFade();                                             // FF3 0x0020aaf0 (MenuMsgObjGetFade)
    int  CutFade();                                             // FF3 0x0020abc0 (MenuMsgObjCutFade)
    void SetMsg(int type, int id, int page);                    // FF3 0x0020ac08 (MenuMsgObjSetMsg)
    int  Step();                                                // FF3 0x0020ace8 (MenuMsgObjStep)

    /* 0x00 */ MENU_MSG_SLOT slot[2];
    /* 0x18 */ s_char  cur;
    /* 0x19 */ u_char  fade_min;        // 0
    /* 0x1a */ u_char  fade_max;        // 0x80
    /* 0x1c */ float   scale;
    /* 0x20 */ int     mode;            // 2: the fade runs there and back
    /* 0x24 */ int     _024[3];
    /* 0x30 */ int     x;
    /* 0x34 */ int     y;
    /* 0x38 */ int     _038;
    /* 0x3c */ int     pri;
    /* 0x40 */ u_char  rgb[3];
    /* 0x43 */ s_char  x_wide;
    /* 0x44 */ s_char  y_wide;
    /* 0x48 */ int     fade;            // -1: idle
    /* 0x4c */ int     fade_len;        // 8
    /* 0x50 */ int     enable;          // Step runs
};
static_assert(sizeof(MENU_MSG_OBJ) == 0x54, "MENU_MSG_OBJ: 0x54 bytes as far as known");
static_assert(offsetof(MENU_MSG_OBJ, cur) == 0x18, "MENU_MSG_OBJ.cur");
static_assert(offsetof(MENU_MSG_OBJ, scale) == 0x1c, "MENU_MSG_OBJ.scale");
static_assert(offsetof(MENU_MSG_OBJ, mode) == 0x20, "MENU_MSG_OBJ.mode");
static_assert(offsetof(MENU_MSG_OBJ, x) == 0x30, "MENU_MSG_OBJ.x");
static_assert(offsetof(MENU_MSG_OBJ, pri) == 0x3c, "MENU_MSG_OBJ.pri");
static_assert(offsetof(MENU_MSG_OBJ, rgb) == 0x40, "MENU_MSG_OBJ.rgb");
static_assert(offsetof(MENU_MSG_OBJ, x_wide) == 0x43, "MENU_MSG_OBJ.x_wide");
static_assert(offsetof(MENU_MSG_OBJ, y_wide) == 0x44, "MENU_MSG_OBJ.y_wide");
static_assert(offsetof(MENU_MSG_OBJ, fade) == 0x48, "MENU_MSG_OBJ.fade");
static_assert(offsetof(MENU_MSG_OBJ, enable) == 0x50, "MENU_MSG_OBJ.enable");

// The title menu's item cross-fader (0x1c bytes; operator new + ctor
// 0x20adb8 by init_Title_Mode): two slots holding a menu item each, slot[cur]
// shown at GetFade, slot[cur ^ 1] at fade_max - GetFade while a change fades.
// C++: C1/C2 constructor and destructor copies.
struct TITLE_MENU_OBJ                   /* 0x1c */
{
    TITLE_MENU_OBJ();                   // FF3 0x0020adb8 (TitleMenuObjCtor; base-object copy 0x0020ad38 TitleMenuObjCtorBase): slots empty, fade 0..0x80, len 8
    ~TITLE_MENU_OBJ();                  // FF3 0x0020ae88 (TitleMenuObjDtor; base-object copy 0x0020ae38 TitleMenuObjDtorBase): slots empty
    void ClearSlot(int slot);           // FF3 0x0020aed8 (TitleMenuObjClearSlot)
    void SetSlot(int slot, int item);   // FF3 0x0020aef8 (TitleMenuObjSetSlot)
    int  IsSlot(int slot, int item);    // FF3 0x0020af10 (TitleMenuObjIsSlot)
    int  GetFade();                     // FF3 0x0020af30 (TitleMenuObjGetFade): fade_max idle, else the lerp
    void Reset();                       // FF3 0x0020afc0 (TitleMenuObjReset): cur 0, idle, slots empty
    int  CutFade();                     // FF3 0x0020b000 (TitleMenuObjCutFade)
    void SetItem(int item);             // FF3 0x0020b048 (TitleMenuObjSetItem): start fading to item
    int  Step();                        // FF3 0x0020b0f8 (TitleMenuObjStep): per frame; 1 while fading

    /* 0x00 */ int    slot[2];          // item number, -1: empty
    /* 0x08 */ s_char cur;
    /* 0x09 */ u_char fade_min;         // 0
    /* 0x0a */ u_char fade_max;         // 0x80
    /* 0x0c */ float  scale;            // alpha factor (the hook's alpha / 128)
    /* 0x10 */ int    cnt;              // fade counter, counts down; -1: idle
    /* 0x14 */ int    len;              // fade length (8)
    /* 0x18 */ int    enable;           // Step runs
};
static_assert(sizeof(TITLE_MENU_OBJ) == 0x1c, "TITLE_MENU_OBJ is 0x1c bytes");
static_assert(offsetof(TITLE_MENU_OBJ, cur) == 0x08, "TITLE_MENU_OBJ.cur");
static_assert(offsetof(TITLE_MENU_OBJ, fade_max) == 0x0a, "TITLE_MENU_OBJ.fade_max");
static_assert(offsetof(TITLE_MENU_OBJ, scale) == 0x0c, "TITLE_MENU_OBJ.scale");
static_assert(offsetof(TITLE_MENU_OBJ, cnt) == 0x10, "TITLE_MENU_OBJ.cnt");
static_assert(offsetof(TITLE_MENU_OBJ, len) == 0x14, "TITLE_MENU_OBJ.len");
static_assert(offsetof(TITLE_MENU_OBJ, enable) == 0x18, "TITLE_MENU_OBJ.enable");

#endif /* _INGAME_MENU_MENU_DRAW_TEX_H */
