/* ==========================================================================
 *  system/b2d/B2dCtl.h
 *
 *  FF3's B2d layout core (file string "B2dCtl.c", 0x100b80..0x104320):
 *  45 entries of items, each item 18 animated parts driven by key lists
 *  from a per-entry key pool; B2dCtlDraw steps the keys and draws every
 *  item through the draw hooks BpCtl installs.
 * ======================================================================== */
#ifndef _SYSTEM_B2D_B2DCTL_H
#define _SYSTEM_B2D_B2DCTL_H

#include <stddef.h>                     // offsetof
#include <stdint.h>                     // intptr_t

#include "sdk/scetypes.h"

// An item's draw hook (item +0xc). FF3 calls it with eight register words
// (a0..t3) and 8-byte stack slots, and the words mean different things per
// hook. Hooks also read the item and handle being drawn (B2dGetDrawItem,
// BpGetDrawHandle). Callers pass the hook in a struct whose first word is
// the function. The core's own calls, per hook:
//   sprite (kind 0): a0 part 0 (texture), a1 u, a2 v, a3 w, t0 h, t1 x, t2 y,
//     t3 sx, stack sy, alpha, rgb, flags
//   hook 4 (kind 0 tiled): as sprite with t1/t2 the item x/y, then stack
//     repeat x, repeat y, flags
//   rect (kind 1): a0 rgb bytes (&part 1 value), a1 w, a2 h, a3 x, t0 y,
//     t1 sx, t2 sy, t3 alpha, stack flags
//   quad (rotated / tiled-rotated sprite, item hook2): a0 texture,
//     a1 xy[8], a2 uv[8], a3 alpha, t0 rgb, t1 flags
//   flat quad (rotated rect, item hook2): a0 rgb bytes, a1 xy[8], a2 alpha,
//     a3 flags
//   hook 5 (kind 2): a0 item name, a1 w, a2 h, a3 repeat x, t0 repeat y,
//     t1 xy[8], t2 rotation, t3 rgb, stack sx, sy, alpha, flags
//   hook 6 (kind 3): a0 item name, a1 xy[8], a2 rotation, a3 rgb, t0 sx,
//     t1 sy, t2 alpha
//   texture set: a0 handle, a1 texture (returns the texture now set);
//   texture load (B2dFile command 0): a0 handle, a1 argument 1, a2 file name;
//   error print: a0 code, a1 file, a2 line, a3
// PC: the words travel in a struct, wide enough for the pointer ones.
// layout: host -- register words widened to intptr_t; no EE image.
struct B2D_HOOK_ARGS
{
    intptr_t a0, a1, a2, a3, t0, t1, t2, t3;
    intptr_t stack[6];
};
static_assert(offsetof(B2D_HOOK_ARGS, t1) == 5 * sizeof(intptr_t), "B2D_HOOK_ARGS.t1");
static_assert(offsetof(B2D_HOOK_ARGS, stack) == 8 * sizeof(intptr_t), "B2D_HOOK_ARGS.stack");
typedef int (*B2D_ITEM_HOOK_FUNC)(const B2D_HOOK_ARGS *args);

// A rectangle (B2dItemGetRect, the hit tests).
struct B2D_RECT                         /* 0x10 */
{
    int x, y, w, h;
};
static_assert(sizeof(B2D_RECT) == 0x10, "B2D_RECT is 0x10 bytes");

// B2D_KEY.flags.
#define B2D_KEY_EASE_MASK   0x0000f     // 1 linear, 2 hold, 3 sine in, 4 sine out (B2dPartStep)
#define B2D_KEY_BLEND_MASK  0x000f0     // on part 10's first key: draw blend flags (BpGetBlendAlpha)
#define B2D_KEY_LOOP_END    0x00100     // last key; next links back to the loop start
#define B2D_KEY_LOOP_START  0x00200
#define B2D_KEY_NO_START    0x00800     // B2dPartAddKey: a first key does not set the value
#define B2D_KEY_FLIP_MASK   0x0f000     // on part 6's first key: 0x1000 / 0x2000 mirror u / v
#define B2D_KEY_TILE_MASK   0xf0000     // on part 12's first key: mirror odd columns / rows (B2dTileFlipFlags)

// One animation key of a part (0x10 bytes on the EE, from the entry's key
// pool). The part moves from `start` to `value` over `frames` frames.
// layout: host -- next is a pointer.
struct B2D_KEY
{
    /* 0x00 */ int      value;          // float bits on type-1 parts, RGBA bytes on type 3
    /* 0x04 */ u_int    flags;          // B2D_KEY_*
    /* 0x08 */ int      frames;         // -1: free pool slot (B2dKeyFree)
    /* 0x0c */ B2D_KEY *next;
};
static_assert(offsetof(B2D_KEY, flags) == 0x04, "B2D_KEY.flags");
static_assert(offsetof(B2D_KEY, frames) == 0x08, "B2D_KEY.frames");

// The 18 parts of an item. Kind 1 (rect) items keep their colour in part 1.
enum B2D_PART_NO
{
    B2D_PART_TEX     = 0,               // texture (BpTex's texture-set hook)
    B2D_PART_U       = 1,
    B2D_PART_V       = 2,
    B2D_PART_W       = 3,
    B2D_PART_H       = 4,
    B2D_PART_PARENT  = 5,               // parent item id, -1 none
    B2D_PART_X       = 6,
    B2D_PART_Y       = 7,
    B2D_PART_SX      = 8,               // 0x400 = 1.0
    B2D_PART_SY      = 9,
    B2D_PART_ALPHA   = 10,              // 0x80 = 1.0; 0 is not drawn
    B2D_PART_TEXT    = 11,              // text id (the layout text hooks)
    B2D_PART_REPX    = 12,              // tile count (sign ignored)
    B2D_PART_REPY    = 13,
    B2D_PART_ANCHORX = 14,              // pivot as a fraction of w (0x400 = 1.0)
    B2D_PART_ANCHORY = 15,
    B2D_PART_ROT     = 16,              // rotation (0x1059d8)
    B2D_PART_RGB     = 17,              // packed RGBA, 0x80 = 1.0 per byte (type 3)
};

// One animated value of an item (0x18 bytes on the EE, 18 per item from item
// +0x18). B2dSprGetPartData returns `value`. layout: host -- `keys` is a
// pointer; the words before it keep their EE offsets.
struct B2D_PART
{
    /* 0x00 */ int      start;          // the value the current key started from
    /* 0x04 */ int      value;
    /* 0x08 */ u_short  type;           // 0 int, 1 float, 3 RGBA bytes (B2dPartStep)
    /* 0x0a */ u_short  flags;          // bit 0: frozen, skipped by B2dItemStep / B2dCtlRewind (B2dSprFreeze*)
    /* 0x0c */ int      frames;         // frames played of the current key
    /* 0x10 */ B2D_KEY *keys;
    /* 0x14 */ B2D_KEY *cur;            // key being played; nullptr when the part stopped
};
static_assert(offsetof(B2D_PART, value) == 0x04, "B2D_PART.value");
static_assert(offsetof(B2D_PART, type) == 0x08, "B2D_PART.type");
static_assert(offsetof(B2D_PART, flags) == 0x0a, "B2D_PART.flags");
static_assert(offsetof(B2D_PART, frames) == 0x0c, "B2D_PART.frames");
static_assert(offsetof(B2D_PART, keys) == 0x10, "B2D_PART.keys");

#define B2D_ITEM_PARTS  18

// One item of a loaded layout (0x1c8 bytes on the EE = 0x18 + 18 parts;
// B2dCtlFindItem walks them by id at +4, B2dCtlFindItemByName by name; the
// handle's list runs through +0x14). layout: host -- name, hooks and next
// are pointers; the parts keep EE offsets only in the comments.
struct B2D_ITEM
{
    /* 0x00 */ u_short            no;       // its slot in the entry's item array (B2dCtlFindOrAllocItem)
    /* 0x02 */ u_short            kind;     // low nibble: draw kind 0..3 (B2dCtlDraw); bit 4 hidden
    /* 0x04 */ int                id;       // -1: free slot
    /* 0x08 */ const char        *name;     // the item's label (GetLabelSuffixType reads its suffix)
    /* 0x0c */ B2D_ITEM_HOOK_FUNC hook;     // B2dCtlSetItemHook; replaces the kind's draw hook
    /* 0x10 */ B2D_ITEM_HOOK_FUNC hook2;    // B2dCtlSetItemHook2; replaces the (flat) quad hook of rotated draws
    /* 0x14 */ B2D_ITEM          *next;
    /* 0x18 */ B2D_PART           part[B2D_ITEM_PARTS];
};
static_assert(offsetof(B2D_ITEM, kind) == 0x02, "B2D_ITEM.kind");
static_assert(offsetof(B2D_ITEM, id) == 0x04, "B2D_ITEM.id");

// One loaded layout of the B2d core (0x34 bytes on the EE, 45 at 0x373f40;
// the handle is the index, the same as the Bp entry index). B2dCtlSetup
// binds it to the layout work, B2dCtlReset empties it.
// layout: host -- data, items, keys and list are pointers; the comments keep
// the EE offsets.
struct B2D_ENTRY
{
    /* 0x00 */ int       id;            // the handle; -1 when unused
    /* 0x04 */ void     *data;          // the layout work (B2dCtlSetup's work)
    /* 0x08 */ B2D_ITEM *items;         // nitem items from the work
    /* 0x0c */ B2D_KEY  *keys;          // nkey pool keys after the items
    /* 0x10 */ int       nitem;
    /* 0x14 */ int       nkey;
    /* 0x18 */ int       key_next;      // B2dKeyAlloc's search start
    /* 0x1c */ int       x;             // B2dCtlSetPos (reset 0)
    /* 0x20 */ int       y;
    /* 0x24 */ int       alpha;         // B2dCtlSetAlpha (reset 0x80)
    /* 0x28 */ int       sx;            // B2dCtlSetScale (reset 0x400 = 1.0)
    /* 0x2c */ int       sy;
    /* 0x30 */ B2D_ITEM *list;          // head of the item list (B2dCtlGetItemList)
};
static_assert(offsetof(B2D_ENTRY, id) == 0x00, "B2D_ENTRY.id");

#define B2D_ENTRY_MAX   45

extern B2D_ENTRY b2d_entry[B2D_ENTRY_MAX];  // 0x373f40

// The core's draw hooks (sdata gp-0x7fc8..-0x7fb0), set by BpCtlSetHooks
// (and FUN_0010e538, FUN_001f87b0 for sprite and quad).
B2D_ITEM_HOOK_FUNC B2dGetSpriteFunc(void);
B2D_ITEM_HOOK_FUNC B2dGetRectFunc(void);
B2D_ITEM_HOOK_FUNC B2dGetQuadFunc(void);
B2D_ITEM_HOOK_FUNC B2dGetQuadFlatFunc(void);
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc4(void);
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc5(void);
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc6(void);
void B2dSetSpriteFunc(B2D_ITEM_HOOK_FUNC func);
void B2dSetRectFunc(B2D_ITEM_HOOK_FUNC func);
void B2dSetQuadFunc(B2D_ITEM_HOOK_FUNC func);
void B2dSetQuadFlatFunc(B2D_ITEM_HOOK_FUNC func);
void B2dSetDrawFunc4(B2D_ITEM_HOOK_FUNC func);
void B2dSetDrawFunc5(B2D_ITEM_HOOK_FUNC func);
void B2dSetDrawFunc6(B2D_ITEM_HOOK_FUNC func);
void B2dSetTexSetFunc(B2D_ITEM_HOOK_FUNC func);         // called when part 0 changes between sprites
void B2dResetCurTex(void);                              // forget the texture set last
void B2dSetErrPrintFunc(B2D_ITEM_HOOK_FUNC func);
void B2dErrPrint(int code, const char *file, int line, int a3);

// The item being drawn (sdata gp-0x7fd0) and the play mode (gp-0x7fd4:
// 1 restart, 2 seek on the first frames only, 4 play), both read by the
// draw loop (B2dCtlDraw).
extern B2D_ITEM *b2d_draw_item;
extern int       b2d_play_mode;

B2D_ITEM *B2dGetDrawItem(void);
void      B2dSetPlayModeStep(void);                     // mode = (mode & 0xf0) + 4

// ---- entries ----
int        B2dCtlIsEntryUsed(int handle);
B2D_ENTRY *B2dCtlGetEntry(int handle);                  // nullptr (and an error print) when unused / out of range
void       B2dCtlSetPos(int handle, int x, int y);
void       B2dCtlGetPos(int handle, int *x, int *y);    // null outputs skipped
void       B2dCtlSetScale(int handle, int sx, int sy);
void       B2dCtlGetScale(int handle, int *sx, int *sy);
int        B2dCtlGetAlpha(int handle);                  // -1 for a bad handle
void       B2dCtlSetAlpha(int handle, int alpha);
int        B2dCtlSetup(int handle, void *work, int nitem, int nkey);   // items, then keys, in `work` (B2dFileGetWorkSize's sizes)
int        B2dCtlReset(int handle);                     // frees keys and items, resets the transform
int        B2dCtlInit(void);                            // clears the entries (B2dSystemInit)
void       B2dCtlGetPool(int handle, B2D_ITEM **items, int *nitem, B2D_KEY **keys, int *nkey);
void       B2dCtlSetPool(int handle, B2D_ITEM *items, int nitem, B2D_KEY *keys, int nkey);

// ---- items ----
B2D_ITEM *B2dCtlGetItemList(int handle);                // nullptr for a bad handle
void      B2dCtlSetItemList(int handle, B2D_ITEM *list);
B2D_ITEM *B2dCtlGetItems(int handle);
int       B2dCtlGetItemNum(int handle);
int       B2dCtlCountItemList(int handle);              // -1 for a bad handle
int       B2dCtlCountItemGroups(int handle);            // runs of items sharing a parent
int       B2dCtlGetItemIndex(int handle, const B2D_ITEM *item);    // -1 when not listed
B2D_ITEM *B2dCtlGetItemByIndex(int handle, int n);
B2D_ITEM *B2dCtlFindItem(int handle, int id);                   // nullptr (and an error print) when missing
B2D_ITEM *B2dCtlFindItemByName(int handle, const char *name);
B2D_ITEM *B2dCtlFindOrAllocItem(B2D_ENTRY *entry, int id, int *found);
B2D_ITEM *B2dCtlNewItem(B2D_ENTRY *entry, int id, int *found, const char *name);
int       B2dCtlAppendItem(B2D_ENTRY *entry, B2D_ITEM *item);
int       B2dCtlRemoveItem(B2D_ENTRY *entry, B2D_ITEM *item);   // 0x102860 (no Ghidra function)
int       B2dCtlDeleteItem(int handle, int id);         // 0x1029f0 (no Ghidra function)
// Item creators: a fresh (or reused) item `id` of one kind, scale 1.0, no
// parent, anchors 0, appended to the list when new; its slot number, or -1.
int       B2dCtlAddSpriteItem(int handle, int id, int tex, int u, int v, int w, int h, int text, const char *name);
int       B2dCtlAddRectItem(int handle, int id, int rgba, int w, int h, int text);   // part 1 holds the colour
int       B2dCtlAddHookItem(int handle, int id, int w, int h, int text, const char *name);
int       B2dCtlAddGroupItem(int handle, int id, const char *name);  // its own parent id; children name it

// layout: host -- one function pointer (EE: the first word of a stack struct).
struct B2D_ITEM_HOOK
{
    B2D_ITEM_HOOK_FUNC func;
};
static_assert(offsetof(B2D_ITEM_HOOK, func) == 0, "B2D_ITEM_HOOK.func");

void B2dCtlSetItemHook(B2D_ITEM *item, const B2D_ITEM_HOOK *hook);  // hook nullptr: none
void B2dCtlSetItemHook2(B2D_ITEM *item, const B2D_ITEM_HOOK *hook); // item->hook2, same rules

// ---- animation ----
void     B2dCtlRewind(int handle);                      // every part back to its first key
int      B2dCtlIsPlaying(int handle);                   // 1 while any part has a current key
int      B2dPartStep(B2D_PART *part);
int      B2dItemStep(B2D_ITEM *item, int frame);        // parts stepped
int      B2dCtlDraw(int handle, int frame);             // steps + draws; parts still playing, -1 bad handle, -2 no sprite hook
void     B2dPartInit(B2D_PART *part);
void     B2dPartReset(B2D_PART *part);
B2D_KEY *B2dKeyAlloc(int handle);
void     B2dKeyFree(B2D_KEY *key);
B2D_KEY *B2dKeyNew(int handle, u_int flags, const int *value, int frames);
int      B2dPartAddKey(int handle, B2D_ITEM *item, u_int part, u_int flags, const int *value, int frames);
B2D_KEY *B2dPartRemoveKey(B2D_PART *part, B2D_KEY *key);
int      B2dPartFreeKeys(B2D_ITEM *item, u_int part);
int      B2dItemClearPartKeys(B2D_ITEM *item, int part);
int      B2dCtlClearPartKeys(int handle, int id, int part);
void     B2dCtlClearItemKeys(int handle, int id);
int      B2dPartKeepFirstKey(int handle, B2D_ITEM *item, u_int part);
int      B2dCtlKeepFirstKeys(int handle);
// The by-id wrappers and key lookups below have no Ghidra functions yet.
int      B2dCtlFreePartKeys(int handle, int id, u_int part);           // 0x1024d8
int      B2dCtlAddPartKey(int handle, int id, u_int part, u_int flags, const int *value, int frames);  // 0x102700
int      B2dPartGetKeyIndex(B2D_ITEM *item, int part, const B2D_KEY *key);   // 0x102798; list length when absent
B2D_KEY *B2dPartGetKeyByIndex(B2D_ITEM *item, int part, int n);        // 0x1027d0
int      B2dCtlKeepFirstKey(int handle, int id, u_int part);           // 0x102fc0

// ---- geometry ----
int   B2dItemGetRect(B2D_RECT *out, const B2D_ITEM *item, const B2D_ITEM *parent);
void  B2dItemGetQuad(int *xy, const B2D_ITEM *item, const B2D_ITEM *parent);   // xy[8]
u_int B2dRgbaMul(u_int a, u_int b);                     // per byte a * b / 128, clamped

#endif /* _SYSTEM_B2D_B2DCTL_H */
