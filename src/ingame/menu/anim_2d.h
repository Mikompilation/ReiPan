/* ==========================================================================
 *  ingame/menu/anim_2d.h
 *
 *  Layout animations and the layout message window (FF2 anim_2d.c's place in
 *  the link order; the code is FF3's). FF3 0x150b40..0x153460: the in-game
 *  HUD's item hooks (0x150b40..0x151680), the HUD fire wobble (bss
 *  0x391988), the message picture class CTutorial (0x151870..0x151d80), the
 *  LayoutMsg window (0x151d80..0x1528b0, work at bss 0x3919b0) and the
 *  LayoutAnim slots (0x1528b0..0x153460, 8 slots at bss 0x3919e8).
 *
 *  A layout animation plays one Bp layout file through states
 *  1 (loading) -> 2 (open, "in" label played) -> 3 (closing, waits for the Bp
 *  tracks) -> 4 (closed, one frame) -> 0 (free); the per-type callbacks of
 *  the type table (LAYOUT_ANIM_TBL) decide when to close.
 * ======================================================================== */
#ifndef _INGAME_MENU_ANIM_2D_H
#define _INGAME_MENU_ANIM_2D_H

#include <stddef.h>                     // offsetof

#include "sdk/scetypes.h"
#include "system/b2d/B2dCtl.h"          // B2D_ITEM

// LAYOUT_ANIM.flags (LayoutAnimSetFlag / LayoutAnimClearFlag).
#define LAYOUT_ANIM_HOLD        0x1     // state 2 does not ask onUpdate (stays open)
#define LAYOUT_ANIM_WAIT        0x2     // state 1 does not open even when loaded
#define LAYOUT_ANIM_LABEL_LAYOUT 0x4    // open with label "layout" instead of "in"
#define LAYOUT_ANIM_LABEL_NEW   0x8     // open with label "NewLabel_0" (wins over 4)

// One layout animation slot; 8 of them at bss 0x3919e8 (LayoutAnimGetSlot).
// EE 0x28 bytes. layout: host -- item (+0x1c) is a pointer; the fields
// before it keep their EE offsets.
struct LAYOUT_ANIM
{
    /* 0x00 */ int       no;            // own slot number
    /* 0x04 */ int       type;          // LayoutAnimStart's type: index of layout_anim_tbl
    /* 0x08 */ int       arg;           // LayoutAnimStart's arg, given to the Bp entry when it opens
    /* 0x0c */ int       state_old;     // state at the start of this frame's LayoutAnimMain
    /* 0x10 */ int       state;         // 0 free, 1 loading, 2 open, 3 closing, 4 closed
    /* 0x14 */ int       handle;        // Bp layout handle, -1: none
    /* 0x18 */ u_int     flags;         // LAYOUT_ANIM_*
    /* 0x1c */ B2D_ITEM *item;          // alpha source (part 10), nullptr: use alpha
    /* 0x20 */ int       alpha;         // 0x80 after start
    /* 0x24 */ int       alpha_scale;   // 0x80 = 1.0
};
static_assert(offsetof(LAYOUT_ANIM, no) == 0x00, "LAYOUT_ANIM.no");
static_assert(offsetof(LAYOUT_ANIM, type) == 0x04, "LAYOUT_ANIM.type");
static_assert(offsetof(LAYOUT_ANIM, arg) == 0x08, "LAYOUT_ANIM.arg");
static_assert(offsetof(LAYOUT_ANIM, state_old) == 0x0c, "LAYOUT_ANIM.state_old");
static_assert(offsetof(LAYOUT_ANIM, state) == 0x10, "LAYOUT_ANIM.state");
static_assert(offsetof(LAYOUT_ANIM, handle) == 0x14, "LAYOUT_ANIM.handle");
static_assert(offsetof(LAYOUT_ANIM, flags) == 0x18, "LAYOUT_ANIM.flags");

// One item record of a layout type's item list (data 0x351428, 0x351440):
// LayoutAnimSetItemHooks installs the item draw hook on each id, and the
// hook prints message (msg0, msg1) in the item; the list ends with an id < 0.
struct LAYOUT_ANIM_ITEM                 /* 0x0c */
{
    /* 0x00 */ int id;                  // B2d item id
    /* 0x04 */ int msg0;
    /* 0x08 */ int msg1;
};
static_assert(sizeof(LAYOUT_ANIM_ITEM) == 0x0c, "LAYOUT_ANIM_ITEM is 0x0c bytes");
static_assert(offsetof(LAYOUT_ANIM_ITEM, msg0) == 0x04, "LAYOUT_ANIM_ITEM.msg0");
static_assert(offsetof(LAYOUT_ANIM_ITEM, msg1) == 0x08, "LAYOUT_ANIM_ITEM.msg1");

// Per-type callback: takes the slot's Bp handle.
typedef int (*LAYOUT_ANIM_FUNC)(int handle);

// One entry of the layout type table (13 x 0x1c at data 0x3512b8). EE 0x1c
// bytes. layout: host -- callbacks and the item list are pointers.
struct LAYOUT_ANIM_TBL
{
    /* 0x00 */ int                     type;
    /* 0x04 */ int                     file;        // Bp file; 0x12dd and 0x12e3 + GetLanguage()
    /* 0x08 */ LAYOUT_ANIM_FUNC        on_load_req; // after the load request, may be nullptr
    /* 0x0c */ LAYOUT_ANIM_FUNC        on_open;     // state 1 -> 2
    /* 0x10 */ LAYOUT_ANIM_FUNC        on_update;   // state 2, != 0: start closing
    /* 0x14 */ LAYOUT_ANIM_FUNC        on_close;    // state 3 once played out; != 0: release the slot
    /* 0x18 */ const LAYOUT_ANIM_ITEM *items;       // items that get the draw hook, may be nullptr
};
static_assert(offsetof(LAYOUT_ANIM_TBL, file) == 0x04, "LAYOUT_ANIM_TBL.file");

// LAYOUT_MSG.result: what LayoutMsgInput read this frame.
#define LAYOUT_MSG_NONE     0
#define LAYOUT_MSG_CANCEL   1           // pad_key[PAD_KEY_CANCEL], when cancel is allowed
#define LAYOUT_MSG_OK       2           // pad_key[PAD_KEY_OK]

// LAYOUT_MSG.flags (LayoutMsgSetFlags; every start sets 1).
#define LAYOUT_MSG_NO_CANCEL_SE 0x1     // cancel plays no SE 0x1a9b0001
#define LAYOUT_MSG_NO_OK_SE     0x2     // confirm/close plays no SE 0x1a9b0003
#define LAYOUT_MSG_SILENT_CSR   0x4     // cursor moves play no SE 0x1a9b0000

// The layout message window work (bss 0x3919b0, 0x38 bytes; the step routine
// pointer sits last, so the host layout matches the EE one).
typedef int (*LAYOUT_MSG_STEP)(int handle);
struct LAYOUT_MSG
{
    /* 0x00 */ int             msg0;    // -1: none
    /* 0x04 */ int             arg;     // LayoutMsgSetArg; cancel allowed: 1 always, 2 never, 0 unless csr is 2
    /* 0x08 */ int             msg1;    // -1: none
    /* 0x0c */ int             result;  // LAYOUT_MSG_*, stored while keep_open; cleared by LayoutMsgSet/SetMode
    /* 0x10 */ int             csr;     // choice cursor 0/1; 2 without choice; 3 after cancel
    /* 0x14 */ int             cursor_part[2][2];   // B2dSprGetPartData parts 1, 2 of the two cursor items
    /* 0x24 */ int             keep_open;   // != 0: record the result, the owner closes the window
    /* 0x28 */ int             choice;  // LayoutMsgSetMode: 1 with a choice
    /* 0x2c */ int             flags;   // LAYOUT_MSG_NO_*/SILENT_*
    /* 0x30 */ LAYOUT_MSG_STEP step;    // per-frame routine LayoutMsgSet picks (single page / pages)
};
static_assert(offsetof(LAYOUT_MSG, msg1) == 0x08, "LAYOUT_MSG.msg1");
static_assert(offsetof(LAYOUT_MSG, result) == 0x0c, "LAYOUT_MSG.result");
static_assert(offsetof(LAYOUT_MSG, csr) == 0x10, "LAYOUT_MSG.csr");
static_assert(offsetof(LAYOUT_MSG, cursor_part) == 0x14, "LAYOUT_MSG.cursor_part");
static_assert(offsetof(LAYOUT_MSG, keep_open) == 0x24, "LAYOUT_MSG.keep_open");
static_assert(offsetof(LAYOUT_MSG, choice) == 0x28, "LAYOUT_MSG.choice");
static_assert(offsetof(LAYOUT_MSG, flags) == 0x2c, "LAYOUT_MSG.flags");
static_assert(offsetof(LAYOUT_MSG, step) == 0x30, "LAYOUT_MSG.step");

// ---- the in-game HUD ----

// The HUD layout's work (0x28 bytes; the constructor fills it and loads the
// file GetFileNo picks, UpdateItems sets up its items, Animate runs it every
// frame). C++ in FF3: its users do operator new(0x28) + 0x1503c8 and
// 0x150540 + operator delete. Only the story menus open one (the camera
// menu, the item menu, the menu top): the HP / film / candle panel drawn
// beside the menu page.
struct HUD_LAYOUT
{
    HUD_LAYOUT(int arg);                // FF3 0x001503c8 (HudLayoutInit; base-object copy FF3 0x001502b0)
    ~HUD_LAYOUT();                      // FF3 0x00150540 (HudLayoutRelease; base-object copy FF3 0x001504e0)
    int IsReady() const;                // FF3 0x001505a0 (HudLayoutIsReady): BpCtlIsDataReady of its file
    void Close();                       // FF3 0x001505c0 (HudLayoutClose): LOADING -> DONE, else plays label 1 ("out_next") once "in" ended, label 3 ("cancel") while it plays
    void Main();                        // FF3 0x00150620 (HudLayoutMain): per frame
    void Start();                       // FF3 0x001506d8 (HudLayoutStart): UpdateItems, once the data is ready
    void Animate();                     // FF3 0x001506f0 (HudLayoutAnimate): position, gauges, candle fire, BpCtlMain
    void Idle();                        // FF3 0x00150aa8 (HudLayoutIdle): Animate, after the label played
    int Play(int label);                // FF3 0x00150ac0 (HudLayoutPlay): label hud_label[arg][label]; 1 if played
    int GetFileNo() const;              // FF3 0x00150b40 (HudGetFileNo)
    void UpdateItems();                 // FF3 0x00150c38 (HudUpdateItems): from Start

    /* 0x00 */ int    state;            // HUD_LAYOUT_*
    /* 0x04 */ int    file;             // Bp file, GetFileNo
    /* 0x08 */ int    a1;               // B2dCtlSetPos's x (the menus set 0)
    /* 0x0c */ int    a2;               // B2dCtlSetPos's y (the menus set 0x16)
    /* 0x10 */ int    arg;              // label row (hud_label): 0 the plain HUD ("in"), 1 the menus ("item_camera_in"); 0 also allows the lens slots
    /* 0x14 */ s_char chr;              // PlyrGetCurNo: 0 Rei, 1 Miku, 2 Kei (face item order)
    /* 0x18 */ int    flash;            // PlyrFlashAble: != 0 shows NUM_flash
    /* 0x1c */ int    kaku;             // PlyrCamAble: != 0 shows R HP_kaku (Miku's slow gauge)
    /* 0x20 */ int    no_camera;        // 1: no camera items (b03 world 1 or the finder not ready)
    /* 0x24 */ int    lens;             // lens slots: 0 none, 1 one, 2 three
};
static_assert(sizeof(HUD_LAYOUT) == 0x28, "HUD_LAYOUT is 0x28 bytes");
static_assert(offsetof(HUD_LAYOUT, file) == 0x04, "HUD_LAYOUT.file");
static_assert(offsetof(HUD_LAYOUT, chr) == 0x14, "HUD_LAYOUT.chr");
static_assert(offsetof(HUD_LAYOUT, flash) == 0x18, "HUD_LAYOUT.flash");
static_assert(offsetof(HUD_LAYOUT, kaku) == 0x1c, "HUD_LAYOUT.kaku");
static_assert(offsetof(HUD_LAYOUT, no_camera) == 0x20, "HUD_LAYOUT.no_camera");
static_assert(offsetof(HUD_LAYOUT, lens) == 0x24, "HUD_LAYOUT.lens");

// The fire items' wobble (bss 0x391988, 0x28 bytes; 0x1502a0 returns it):
// HUD_LAYOUT::Animate moves cur from `from` to `to` over 7 frames, then picks a new
// random `to`. cur[0] scales the fire item, cur[1] its other axis, cur[2]
// its alpha (HudDrawFire).
struct HUD_FIRE
{
    HUD_FIRE();                         // FF3 0x00151680 (Anim2dStaticInit)

    /* 0x00 */ float cur[3];
    /* 0x0c */ float time;
    /* 0x10 */ float from[3];
    /* 0x1c */ float to[3];
};
static_assert(sizeof(HUD_FIRE) == 0x28, "HUD_FIRE is 0x28 bytes");
static_assert(offsetof(HUD_FIRE, time) == 0x0c, "HUD_FIRE.time");
static_assert(offsetof(HUD_FIRE, from) == 0x10, "HUD_FIRE.from");
static_assert(offsetof(HUD_FIRE, to) == 0x1c, "HUD_FIRE.to");

extern HUD_FIRE hud_fire;               // bss 0x391988
HUD_FIRE *FUN_001502a0(void);           // &hud_fire


// HUD_LAYOUT.state (HUD_LAYOUT::Main).
#define HUD_LAYOUT_LOADING  0               // waiting for the data
#define HUD_LAYOUT_RUN      1               // items set up; animated until the playing label ends
#define HUD_LAYOUT_DONE     2               // the label played (or closed before loading); still animated


// ---- message pictures ----

// Picture key table (data 0x351190): key -> pictures base .. base + count - 1,
// one set of 0xb per language.
struct MSG_PIC_TBL
{
    /* 0x00 */ int key;                 // -1 ends the table
    /* 0x04 */ int base;
    /* 0x08 */ int count;
};
static_assert(sizeof(MSG_PIC_TBL) == 0x0c, "MSG_PIC_TBL is 0x0c bytes");
static_assert(offsetof(MSG_PIC_TBL, base) == 0x04, "MSG_PIC_TBL.base");
static_assert(offsetof(MSG_PIC_TBL, count) == 0x08, "MSG_PIC_TBL.count");

struct MENU_PIC;

// FF3 RTTI name CTutorial (the help viewer's picture; functions still MsgPic*).
// The picture shown beside a message, one per page (vtable 0x341c20, 0x14
// bytes on the EE; 0x14e0f8 news one). Its key comes from the message
// (0x2adbf0), its page from MsgGetPage.
// layout: host -- the vptr is 4 bytes on the EE, 8 here.
class CTutorial
{
public:
    CTutorial();                          // FF3 0x001518d0 (base 0x00151870)
    virtual ~CTutorial();                 // FF3 0x00151980 (base 0x00151930, deleting 0x001519d0)

    int  IsActive(void) const;
    void SetPage(int page);
    void Set(int key);
    void Reset(void);
    void Open(void);
    void Update(int key);
    void Draw(short x, short y, float f);
    void End(void);

    int           active;
    int           key;                  // -1: none
    int           page;                 // -1: none
    MENU_PIC *pic;
};
static_assert(sizeof(CTutorial) >= 0x14, "CTutorial keeps its EE fields");

int MsgPicHasKey(int key);              // the key has pictures
int MsgPicGetPicNo(int key, int page);

// ---- layout animations ----
void         Anim2dInit(void);
void         LayoutAnimMain(void);                  // once per frame from the menus/phases
int          LayoutAnimStart(int type, int arg);    // slot 0..7, -1 when none free
void         LayoutAnimReset(int slot);             // slot < 0: all
void         LayoutAnimResetOne(int slot);
void         LayoutAnimResetAll(void);              // LayoutAnimReset(-1)
LAYOUT_ANIM *LayoutAnimGetSlot(int slot);           // nullptr unless 0 <= slot < 8
int          LayoutAnimIsActive(const LAYOUT_ANIM *anim);
LAYOUT_ANIM *LayoutAnimFindType(int type);
int          LayoutAnimGetState(int slot);
int          LayoutAnimIsStateChanged(int slot);
int          LayoutAnimIsAnyActive(void);
int          LayoutAnimIsAnyVisible(void);
int          LayoutAnimIsDataReady(int slot);
void         LayoutAnimSetFlag(int slot, u_int bits);
void         LayoutAnimClearFlag(int slot, u_int bits);
void         LayoutAnimSetAlpha(int slot, int alpha);
int          LayoutAnimGetAlpha(int slot);
void         LayoutAnimSetAlphaScale(int slot, int scale);
void         LayoutAnimSetAlphaItem(int slot, B2D_ITEM *item);
void         LayoutAnimSetAlphaItemById(int slot, int handle, int id);
void         LayoutAnimSetAlphaItemByName(int slot, int handle, const char *name);
const LAYOUT_ANIM_TBL  *LayoutAnimGetTypeTbl(int type);
const LAYOUT_ANIM_TBL  *LayoutAnimGetTypeTblByHandle(int handle);
const LAYOUT_ANIM_ITEM *LayoutAnimFindItemAttr(int handle, int id);
void         LayoutAnimSetItemHooks(int handle, const LAYOUT_ANIM_TBL *tbl);
void         LayoutAnimClear(int slot);
void         LayoutAnimClearAll(void);
int          LayoutAnimIsPlayEnd(int handle);

// ---- the layout message window (layout types 3, 5, 9, 12) ----
void LayoutMsgSet(int msg0, int msg1);
void LayoutMsgSetMode(int mode, int csr);           // 1: with choice (csr 0/1), 2: plain
void LayoutMsgSetType(int mode);                    // LayoutMsgSetMode(mode, 1)
void LayoutMsgSetArg(int arg);
int  LayoutMsgGetCsr(void);
int  LayoutMsgGetResult(void);                      // LAYOUT_MSG_* while keep_open
void LayoutMsgSetKeepOpen(int keep);
void LayoutMsgSetFlags(int flags);
int  LayoutMsgGetPageBreakNum(void);
int  LayoutMsgInput(int handle);                    // LAYOUT_MSG_* this frame
void LayoutMsgSetCursorItems(int handle, int id0, int id1);
int  LayoutMsgOnLoadReq(int handle);
int  LayoutMsgOnUpdate(int handle);
int  LayoutMsgOnClose(int handle);
int  LayoutMsgOnUpdateB(int handle);                // types 5/9: same as LayoutMsgOnUpdate
int  LayoutMsgOnCloseB(int handle);                 // types 5/9: same as LayoutMsgOnClose

// Exported wrappers other modules call (FF3 0x152790..0x152808).
int  Anim2dMsgGetCsr(void);
void Anim2dMsgSet(int msg0, int msg1);
int  Anim2dMsgGetPageBreakNum(void);
void Anim2dMsgSetMode(int mode, int csr);
void Anim2dMsgSetType(int mode);

// The per-language texture pack (file 0xac9 + language at EE 0x12f7100).
void LangTexLoadReq(void);                          // from one_Boot_Load
void LangTexSend(int no);                           // TIM2 'no' of the pack to the GS

int  NullFuncRet0(void);                            // STL allocator helpers that do nothing
void NullFunc(void);

// Message window on_open callbacks; in Ghidra they sit inside other
// functions (0x1526d8 in LayoutMsgSetCursorItems, 0x152820 in
// LayoutMsgOnUpdateB).
int FUN_001526d8(int handle);           // types 3/12 on_open
int FUN_00152820(int handle);           // types 5/9 on_open

#endif /* _INGAME_MENU_ANIM_2D_H */
