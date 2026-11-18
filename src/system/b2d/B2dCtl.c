// system/b2d/B2dCtl.c -- FF3's B2d layout core (error file string "B2dCtl.c",
// 0x34bd60; FF3 0x100b80..0x104320). No FF2 counterpart: written from the
// FF3 decompilation and, for the draws (whose hook arguments run onto the
// stack), the disassembly.

#include <stdio.h>                      // printf
#include <stdlib.h>                     // abs
#include <string.h>

#include "common/reipan_stub.h"
#include "system/b2d/B2dCtl.h"
#include "system/b2d/B2dSpr.h"          // B2dSin, B2dRotatePoints
#include "reipan/io/reipan_menu_mouse.h" // PC: ReiPan_MenuMouseNoteItem

B2D_ENTRY b2d_entry[B2D_ENTRY_MAX];     // 0x373f40

static int b2d_cur_tex;                 // sdata gp-0x7fd8: texture set last (-1 none)
int        b2d_play_mode;               // sdata gp-0x7fd4
B2D_ITEM  *b2d_draw_item;               // sdata gp-0x7fd0
static u_int b2d_play_frame;            // sdata gp-0x7fcc: frames since the mode-1 restart

static B2D_ITEM_HOOK_FUNC b2d_sprite_func;      // gp-0x7fc8
static B2D_ITEM_HOOK_FUNC b2d_rect_func;        // gp-0x7fc4
static B2D_ITEM_HOOK_FUNC b2d_quad_func;        // gp-0x7fc0
static B2D_ITEM_HOOK_FUNC b2d_quad_flat_func;   // gp-0x7fbc
static B2D_ITEM_HOOK_FUNC b2d_draw_func4;       // gp-0x7fb8 (no setter caller found)
static B2D_ITEM_HOOK_FUNC b2d_draw_func5;       // gp-0x7fb4 (BpCtl: no-op)
static B2D_ITEM_HOOK_FUNC b2d_draw_func6;       // gp-0x7fb0 (BpCtl: no-op)
static B2D_ITEM_HOOK_FUNC b2d_err_print_func;   // sdata 0x36b54c
static B2D_ITEM_HOOK_FUNC b2d_tex_set_func;     // sdata gp-0x9e8 (0x372b08)

// The part types the items start with (0x2e9208): {part, type}, ended by a
// part <= 0. Only the colour is not a plain integer.
static const int b2d_part_type_tbl[][2] =
{
    { B2D_PART_RGB, 3 },
    { -1, -1 },
};

// FF3's inlined fixed-point steps: a * b / 0x400 rounded (the values are
// 0x400 = 1.0), truncated, and the alpha product a * b / 0x80.
static inline int B2dScale(int a, int b)
{
    return (a * b + 0x200) / 0x400;
}

static inline int B2dScaleTrunc(int a, int b)
{
    return a * b / 0x400;
}

static inline int B2dAlphaMul(int a, int b)
{
    return a * b / 0x80;
}

// The draw flags a part's first key carries.
// PC: FF3 reads keys->flags without a check (a low-memory read when a part
// has no keys); no keys reads as no flags here.
static inline u_int B2dPartKeyFlags(const B2D_PART *part, u_int mask)
{
    return (part->keys != nullptr) ? (part->keys->flags & mask) : 0;
}

static inline u_int B2dItemDrawFlags(const B2D_PART *parts)
{
    return B2dPartKeyFlags(&parts[B2D_PART_X], B2D_KEY_FLIP_MASK)
         | B2dPartKeyFlags(&parts[B2D_PART_REPX], B2D_KEY_TILE_MASK)
         | B2dPartKeyFlags(&parts[B2D_PART_ALPHA], B2D_KEY_BLEND_MASK);
}

static int B2dDrawSpriteTiles(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha);
static int B2dDrawSpriteRot(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha);
static int B2dDrawRectRot(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha);

// ──────────────────────────────────────────────────────────────────────
// The draw-hook accessors: seven set/get pairs of two instructions each.
// Ghidra has no functions at 0x100b98, 0x100bc0, 0x100bc8, 0x100bd8,
// 0x100be8 (it folds them into the preceding function); decoded from the
// bytes.

// FF3 0x00100b80.
void B2dSetSpriteFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_sprite_func = func;
}

// FF3 0x00100b88.
B2D_ITEM_HOOK_FUNC B2dGetSpriteFunc(void)
{
    return b2d_sprite_func;
}

// FF3 0x00100b90.
void B2dSetRectFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_rect_func = func;
}

// FF3 0x00100b98 (no Ghidra function).
B2D_ITEM_HOOK_FUNC B2dGetRectFunc(void)
{
    return b2d_rect_func;
}

// FF3 0x00100ba0.
void B2dSetQuadFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_quad_func = func;
}

// FF3 0x00100ba8.
B2D_ITEM_HOOK_FUNC B2dGetQuadFunc(void)
{
    return b2d_quad_func;
}

// FF3 0x00100bb0.
void B2dSetQuadFlatFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_quad_flat_func = func;
}

// FF3 0x00100bb8.
B2D_ITEM_HOOK_FUNC B2dGetQuadFlatFunc(void)
{
    return b2d_quad_flat_func;
}

// FF3 0x00100bc0 (no Ghidra function).
void B2dSetDrawFunc4(B2D_ITEM_HOOK_FUNC func)
{
    b2d_draw_func4 = func;
}

// FF3 0x00100bc8 (no Ghidra function).
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc4(void)
{
    return b2d_draw_func4;
}

// FF3 0x00100bd0.
void B2dSetDrawFunc5(B2D_ITEM_HOOK_FUNC func)
{
    b2d_draw_func5 = func;
}

// FF3 0x00100bd8 (no Ghidra function).
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc5(void)
{
    return b2d_draw_func5;
}

// FF3 0x00100be0.
void B2dSetDrawFunc6(B2D_ITEM_HOOK_FUNC func)
{
    b2d_draw_func6 = func;
}

// FF3 0x00100be8 (no Ghidra function).
B2D_ITEM_HOOK_FUNC B2dGetDrawFunc6(void)
{
    return b2d_draw_func6;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100bf0. The handle compares unsigned (negative = out of range).

int B2dCtlIsEntryUsed(int handle)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
        return 0;
    return b2d_entry[handle].id != -1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100c28.

B2D_ENTRY *B2dCtlGetEntry(int handle)
{
    int line = 0x97;

    if ((u_int)handle < B2D_ENTRY_MAX)
    {
        if (b2d_entry[handle].id != -1)
            return &b2d_entry[handle];
        line = 0x9c;
    }
    B2dErrPrint(handle, "B2dCtl.c", line, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100c88.

void B2dCtlSetItemHook(B2D_ITEM *item, const B2D_ITEM_HOOK *hook)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, "B2dCtl.c", 0xb8, 0);
        return;
    }
    item->hook = (hook != nullptr) ? hook->func : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100cd8.

void B2dCtlSetItemHook2(B2D_ITEM *item, const B2D_ITEM_HOOK *hook)
{
    if (item == nullptr)
    {
        B2dErrPrint(-1, "B2dCtl.c", 0xc1, 0);
        return;
    }
    item->hook2 = (hook != nullptr) ? hook->func : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100d28.

B2D_ITEM *B2dGetDrawItem(void)
{
    return b2d_draw_item;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00100d48.

void B2dSetPlayModeStep(void)
{
    b2d_play_mode = (b2d_play_mode & 0xf0) + 4;
}

// ──────────────────────────────────────────────────────────────────────
// The entry's transform: position, scale (0x400 = 1.0), alpha.
// FF3 0x00100d78.

void B2dCtlSetPos(int handle, int x, int y)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry != nullptr)
    {
        entry->y = y;
        entry->x = x;
    }
}

// FF3 0x00100db8.
void B2dCtlGetPos(int handle, int *x, int *y)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return;
    if (x != nullptr)
        *x = entry->x;
    if (y != nullptr)
        *y = entry->y;
}

// FF3 0x00100e10.
void B2dCtlSetScale(int handle, int sx, int sy)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry != nullptr)
    {
        entry->sy = sy;
        entry->sx = sx;
    }
}

// FF3 0x00100e50.
void B2dCtlGetScale(int handle, int *sx, int *sy)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return;
    if (sx != nullptr)
        *sx = entry->sx;
    if (sy != nullptr)
        *sy = entry->sy;
}

// FF3 0x00100ea8.
int B2dCtlGetAlpha(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;
    return entry->alpha;
}

// FF3 0x00100ed8.
void B2dCtlSetAlpha(int handle, int alpha)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry != nullptr)
        entry->alpha = alpha;
}

// ──────────────────────────────────────────────────────────────────────
// The first item of the handle's list labelled `name`. FF3 0x00100f08.

B2D_ITEM *B2dCtlFindItemByName(int handle, const char *name)
{
    B2D_ITEM *item = B2dCtlGetItemList(handle);

    if (name == nullptr)
    {
        B2dErrPrint(-2, "B2dCtl.c", 0x155, 0);
        return nullptr;
    }
    for (; item != nullptr; item = item->next)
    {
        if (item->name != nullptr && strcmp(item->name, name) == 0)
            return item;
    }
    B2dErrPrint(-1, "B2dCtl.c", 0x162, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The item `id` of the handle's item array. FF3 0x00100f98.
// callcheck: B2dCtlFindItem (Ghidra lists a reference to itself, the loop
// branch; the body calls only B2dCtlGetEntry and B2dErrPrint)

B2D_ITEM *B2dCtlFindItem(int handle, int id)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return nullptr;
    for (int i = 0; i < entry->nitem; i++)
    {
        if (entry->items[i].id == id)
            return &entry->items[i];
    }
    B2dErrPrint(id, "B2dCtl.c", 0x178, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The entry's item list and item array. FF3 0x00101090.

B2D_ITEM *B2dCtlGetItemList(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    return (entry != nullptr) ? entry->list : nullptr;
}

// FF3 0x001010c0.
void B2dCtlSetItemList(int handle, B2D_ITEM *list)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry != nullptr)
        entry->list = list;
}

// FF3 0x001010f0.
B2D_ITEM *B2dCtlGetItems(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    return (entry != nullptr) ? entry->items : nullptr;
}

// FF3 0x00101120.
int B2dCtlGetItemNum(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    return (entry != nullptr) ? entry->nitem : 0;
}

// ──────────────────────────────────────────────────────────────────────
// The length of the item list. FF3 0x00101150 (reads entry->list before
// the null check; reordered here).

int B2dCtlCountItemList(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);
    int        n     = 0;

    if (entry == nullptr)
        return -1;
    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
        n++;
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Counts the runs of listed items: a new run starts at a root item (no
// parent) or where the parent id changes. FF3 0x001011a8.

int B2dCtlCountItemGroups(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);
    int        prev  = -1;
    int        n     = 0;

    if (entry == nullptr)
        return -1;
    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        int parent = item->part[B2D_PART_PARENT].value;

        if (parent == -1 || prev != parent)
            n++;
        prev = parent;
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The position of `item` in the list, -1 when absent. FF3 0x00101218.
// PC: FF3 reads entry->list without checking the entry.

int B2dCtlGetItemIndex(int handle, const B2D_ITEM *item)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);
    int        i     = 0;

    if (entry == nullptr)
        return -1;
    for (B2D_ITEM *it = entry->list; it != item; it = it->next)
    {
        if (it == nullptr)
            return -1;
        i++;
    }
    return i;
}

// ──────────────────────────────────────────────────────────────────────
// The n-th listed item; nullptr for n < 0 or past the end. FF3 0x00101260.
// PC: FF3 reads entry->list without checking the entry.

B2D_ITEM *B2dCtlGetItemByIndex(int handle, int n)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr || n < 0)
        return nullptr;

    B2D_ITEM *item = entry->list;

    for (int i = 0; i < n; i++)
    {
        if (item == nullptr)
            return nullptr;
        item = item->next;
    }
    return item;
}

// ──────────────────────────────────────────────────────────────────────
// Every animated part back to its first key: the value jumps to the first
// key's and the next step moves on to the second key. Does nothing while
// no sprite hook is installed (B2dSystemInit not run). FF3 0x001012b8.

void B2dCtlRewind(int handle)
{
    if (b2d_sprite_func == nullptr)
        return;

    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return;
    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        for (int i = 0; i < B2D_ITEM_PARTS; i++)
        {
            B2D_PART *part = &item->part[i];
            B2D_KEY  *key  = part->keys;

            if (key != nullptr && !(part->flags & 1))
            {
                part->value  = key->value;
                part->cur    = key;
                part->start  = key->value;
                part->frames = key->frames;
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 while any part of any listed item still plays a key. FF3 0x00101348.

int B2dCtlIsPlaying(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return 0;
    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        for (int i = 0; i < B2D_ITEM_PARTS; i++)
        {
            if (item->part[i].cur != nullptr)
                return 1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00101408.

void B2dSetTexSetFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_tex_set_func = func;
}

// ──────────────────────────────────────────────────────────────────────
// Forgets the texture set last, so the next sprite sets its own. Called
// after anything that changes the texture behind the core's back.
// FF3 0x00101410.

void B2dResetCurTex(void)
{
    b2d_cur_tex = -1;
}

// ──────────────────────────────────────────────────────────────────────
// A type-3 part's step: each RGBA byte from start towards the key's by
// num / den. FF3 0x00101420 (Ghidra's FF2 match "CBuffReset" was wrong).
// PC: den is 0 for ease-0 keys, where FF3 divides by zero (break 7); the
// value is left alone here.

static int B2dPartLerpRgba(B2D_PART *part, int num, int den)
{
    const u_char *start = (const u_char *)&part->start;
    u_char       *value = (u_char *)&part->value;
    const u_char *key   = (const u_char *)&part->cur->value;

    if (den == 0)
        return 0;
    for (int i = 0; i < 4; i++)
        value[i] = (u_char)(start[i] + (u_char)((int)((u_int)key[i] - (u_int)start[i]) * num / den));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// One frame of a part along its current key. When the key's frames are
// played the value lands on the key's and the next key starts; otherwise
// the key's ease gives num / den: 1 linear, 2 hold, 3 / 4 a quarter sine
// in / out (0x1000 = 1.0). FF3 0x00101470.

int B2dPartStep(B2D_PART *part)
{
    B2D_KEY *key = part->cur;
    int      num = 0;
    int      den = 0;

    part->frames++;
    if (part->frames >= key->frames)
    {
        part->frames = 0;
        part->start  = key->value;
        part->value  = key->value;
        part->cur    = key->next;
        return 0;
    }

    switch (key->flags & B2D_KEY_EASE_MASK)
    {
    case 1:
        num = part->frames;
        den = key->frames;
        break;
    case 2:
        return 0;
    case 3:
        num = 0x1000 - B2dSin(((part->frames * 0x400 / key->frames) >> 2) + 0x100);
        den = 0x1000;
        break;
    case 4:
        num = B2dSin((part->frames * 0x400 / key->frames) >> 2);
        den = 0x1000;
        break;
    default:
        break;
    }

    switch (part->type)
    {
    case 0:
        // PC: FF3 divides by zero here for ease-0 keys (break 7).
        if (den != 0)
            part->value = (key->value - part->start) * num / den + part->start;
        break;
    case 1:
    {
        float from, to, now;

        memcpy(&from, &part->start, sizeof(from));
        memcpy(&to, &key->value, sizeof(to));
        now = ((to - from) * (float)num) / (float)den + from;
        memcpy(&part->value, &now, sizeof(now));
        break;
    }
    case 3:
        B2dPartLerpRgba(part, num, den);
        break;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Steps every playing part of the item. On the first frames after a
// restart (b2d_play_frame < 2) each part first seeks to frame `frame` of
// its key list. Returns the number of parts stepped (frozen parts do not
// count). FF3 0x00101610.

int B2dItemStep(B2D_ITEM *item, int frame)
{
    int n = 0;

    for (int i = 0; i < B2D_ITEM_PARTS; i++)
    {
        B2D_PART *part = &item->part[i];
        B2D_KEY  *key  = part->keys;

        if (part->cur == nullptr)
            continue;

        if (b2d_play_frame < 2)
        {
            u_int rest = (u_int)frame;

            if ((u_int)key->frames < rest)
            {
                B2D_KEY *next = key->next;

                for (;;)
                {
                    rest -= key->frames;
                    if (next == nullptr)
                        break;
                    part->frames = 0;
                    part->start  = key->value;
                    part->value  = key->value;
                    part->cur    = key->next;
                    key = key->next;
                    if (rest <= (u_int)key->frames)
                        break;
                    next = key->next;
                }
                part->frames = (int)rest;
            }
            else
            {
                part->frames = frame;
            }
        }

        if (!(part->flags & 1))
        {
            n++;
            B2dPartStep(part);
        }
    }
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0: a sprite. Sets the texture through the texture-set hook when
// part 0 differs from the last one, applies the parent's position, scale,
// alpha, rotation and colour, then: one untiled, unrotated sprite through
// the item's hook or the sprite hook (at the item rect scaled and moved by
// the entry); rotated -> B2dDrawSpriteRot; tiled -> hook 4 when set, else
// B2dDrawSpriteTiles. FF3 0x001016f8.

static int B2dDrawSpriteItem(int handle, B2D_ITEM *item, const B2D_ITEM *parent, const B2D_PART *parts)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);
    int        x     = parts[B2D_PART_X].value;
    int        y     = parts[B2D_PART_Y].value;
    int        sx    = parts[B2D_PART_SX].value;
    int        sy    = parts[B2D_PART_SY].value;
    int        alpha = parts[B2D_PART_ALPHA].value;
    u_int      rgb   = (u_int)parts[B2D_PART_RGB].value;
    int        rot   = parts[B2D_PART_ROT].value;

    if (parts[B2D_PART_TEX].value != b2d_cur_tex && b2d_tex_set_func != nullptr)
    {
        B2D_HOOK_ARGS args = {};

        args.a0 = handle;
        args.a1 = parts[B2D_PART_TEX].value;
        b2d_cur_tex = b2d_tex_set_func(&args);
    }

    if (parent != nullptr)
    {
        const B2D_PART *pp = parent->part;

        alpha = B2dAlphaMul(alpha, pp[B2D_PART_ALPHA].value);
        sx    = B2dScale(sx, pp[B2D_PART_SX].value);
        sy    = B2dScale(sy, pp[B2D_PART_SY].value);
        x    += pp[B2D_PART_X].value;
        y    += pp[B2D_PART_Y].value;
        rot  += pp[B2D_PART_ROT].value;
        rgb   = B2dRgbaMul(rgb, (u_int)pp[B2D_PART_RGB].value);
    }

    if (parts[B2D_PART_REPX].value == 1 && parts[B2D_PART_REPY].value == 1)
    {
        if (rot != 0)
        {
            B2dDrawSpriteRot(item, parent, entry->x, entry->y, entry->sx, entry->sy, entry->alpha);
            return 0;
        }

        B2D_ITEM_HOOK_FUNC func = (item->hook != nullptr) ? item->hook : b2d_sprite_func;
        B2D_RECT           r;
        B2D_HOOK_ARGS      args = {};

        B2dItemGetRect(&r, item, parent);
        args.a0       = parts[B2D_PART_TEX].value;
        args.a1       = parts[B2D_PART_U].value;
        args.a2       = parts[B2D_PART_V].value;
        args.a3       = parts[B2D_PART_W].value;
        args.t0       = parts[B2D_PART_H].value;
        args.t1       = B2dScale(r.x, entry->sx) + entry->x;
        args.t2       = B2dScale(r.y, entry->sy) + entry->y;
        args.t3       = B2dScale(sx, entry->sx);
        args.stack[0] = B2dScale(sy, entry->sy);
        args.stack[1] = B2dAlphaMul(alpha, entry->alpha);
        args.stack[2] = rgb;
        args.stack[3] = B2dItemDrawFlags(parts);
        func(&args);
    }
    else if (b2d_draw_func4 == nullptr)
    {
        B2dDrawSpriteTiles(item, parent, entry->x, entry->y, entry->sx, entry->sy, entry->alpha);
    }
    else
    {
        B2D_HOOK_ARGS args = {};

        args.a0       = parts[B2D_PART_TEX].value;
        args.a1       = parts[B2D_PART_U].value;
        args.a2       = parts[B2D_PART_V].value;
        args.a3       = parts[B2D_PART_W].value;
        args.t0       = parts[B2D_PART_H].value;
        args.t1       = B2dScale(x, entry->sx) + entry->x;
        args.t2       = B2dScale(y, entry->sy) + entry->y;
        args.t3       = B2dScale(sx, entry->sx);
        args.stack[0] = B2dScale(sy, entry->sy);
        args.stack[1] = B2dAlphaMul(alpha, entry->alpha);
        args.stack[2] = rgb;
        args.stack[3] = parts[B2D_PART_REPX].value;
        args.stack[4] = parts[B2D_PART_REPY].value;
        args.stack[5] = B2dItemDrawFlags(parts);
        b2d_draw_func4(&args);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 1: an untextured rect in part 1's colour bytes, through the item's
// hook or the rect hook; rotated -> B2dDrawRectRot. -1 without a rect hook.
// FF3 0x00101ad0.

static int B2dDrawRectItem(B2D_ENTRY *entry, B2D_ITEM *item, const B2D_ITEM *parent, const B2D_PART *parts)
{
    int sx    = parts[B2D_PART_SX].value;
    int sy    = parts[B2D_PART_SY].value;
    int alpha = parts[B2D_PART_ALPHA].value;
    int rot   = parts[B2D_PART_ROT].value;

    if (b2d_rect_func == nullptr)
        return -1;

    if (parent != nullptr)
    {
        const B2D_PART *pp = parent->part;

        sx     = B2dScale(sx, pp[B2D_PART_SX].value);
        sy     = B2dScale(sy, pp[B2D_PART_SY].value);
        alpha  = B2dAlphaMul(alpha, pp[B2D_PART_ALPHA].value);
        rot   += pp[B2D_PART_ROT].value;
    }

    if (rot != 0)
    {
        B2dDrawRectRot(item, parent, entry->x, entry->y, entry->sx, entry->sy, entry->alpha);
        return 0;
    }

    B2D_ITEM_HOOK_FUNC func = (item->hook != nullptr) ? item->hook : b2d_rect_func;
    B2D_RECT           r;
    B2D_HOOK_ARGS      args = {};

    B2dItemGetRect(&r, item, parent);
    args.a0       = (intptr_t)&parts[B2D_PART_U].value;     // the colour bytes
    args.a1       = parts[B2D_PART_W].value;
    args.a2       = parts[B2D_PART_H].value;
    args.a3       = B2dScale(r.x, entry->sx) + entry->x;
    args.t0       = B2dScale(r.y, entry->sy) + entry->y;
    args.t1       = B2dScale(sx, entry->sx);
    args.t2       = B2dScale(sy, entry->sy);
    args.t3       = B2dAlphaMul(alpha, entry->alpha);
    args.stack[0] = B2dItemDrawFlags(parts);
    func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The item quad on screen: scaled by the entry (rounded) and moved by it.
// Inline in FF3 (B2dDrawKind2Item, B2dDrawKind3Item).

static void B2dQuadToScreen(int *xy, const B2D_ENTRY *entry)
{
    for (int i = 0; i < 4; i++)
    {
        xy[i * 2]     = B2dScale(xy[i * 2], entry->sx) + entry->x;
        xy[i * 2 + 1] = B2dScale(xy[i * 2 + 1], entry->sy) + entry->y;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Kind 2: hands the item's screen quad and its values to hook 5 (or the
// item's hook). -1 without hook 5. FF3 0x00101cc0.

static int B2dDrawKind2Item(B2D_ENTRY *entry, B2D_ITEM *item, const B2D_ITEM *parent, const B2D_PART *parts)
{
    int   sx    = parts[B2D_PART_SX].value;
    int   sy    = parts[B2D_PART_SY].value;
    int   alpha = parts[B2D_PART_ALPHA].value;
    int   rot   = parts[B2D_PART_ROT].value;
    u_int rgb   = (u_int)parts[B2D_PART_RGB].value;
    int   xy[8];

    if (b2d_draw_func5 == nullptr)
        return -1;

    B2dItemGetQuad(xy, item, parent);
    B2dQuadToScreen(xy, entry);
    if (parent != nullptr)
    {
        const B2D_PART *pp = parent->part;

        sx     = B2dScale(sx, pp[B2D_PART_SX].value);
        sy     = B2dScale(sy, pp[B2D_PART_SY].value);
        alpha  = B2dAlphaMul(alpha, pp[B2D_PART_ALPHA].value);
        rot   += pp[B2D_PART_ROT].value;
        rgb    = B2dRgbaMul(rgb, (u_int)pp[B2D_PART_RGB].value);
    }

    B2D_ITEM_HOOK_FUNC func = (item->hook != nullptr) ? item->hook : b2d_draw_func5;
    B2D_HOOK_ARGS      args = {};

    args.a0       = (intptr_t)item->name;
    args.a1       = parts[B2D_PART_W].value;
    args.a2       = parts[B2D_PART_H].value;
    args.a3       = parts[B2D_PART_REPX].value;
    args.t0       = parts[B2D_PART_REPY].value;
    args.t1       = (intptr_t)xy;
    args.t2       = rot;
    args.t3       = rgb;
    args.stack[0] = sx;
    args.stack[1] = sy;
    args.stack[2] = B2dAlphaMul(alpha, entry->alpha);
    args.stack[3] = B2dItemDrawFlags(parts);
    func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 3: as kind 2 through hook 6, fewer values and no parent (the quad
// is the item's own). -1 without hook 6. FF3 0x00101eb8.

static int B2dDrawKind3Item(B2D_ENTRY *entry, B2D_ITEM *item, const B2D_ITEM *parent, const B2D_PART *parts)
{
    u_int rgb   = (u_int)parts[B2D_PART_RGB].value;
    int   sx    = parts[B2D_PART_SX].value;
    int   sy    = parts[B2D_PART_SY].value;
    int   alpha = parts[B2D_PART_ALPHA].value;
    int   rot   = parts[B2D_PART_ROT].value;
    int   xy[8];

    (void)parent;
    if (b2d_draw_func6 == nullptr)
        return -1;

    B2dItemGetQuad(xy, item, nullptr);
    B2dQuadToScreen(xy, entry);

    B2D_ITEM_HOOK_FUNC func = (item->hook != nullptr) ? item->hook : b2d_draw_func6;
    B2D_HOOK_ARGS      args = {};

    args.a0 = (intptr_t)item->name;
    args.a1 = (intptr_t)xy;
    args.a2 = rot;
    args.a3 = rgb;
    args.t0 = sx;
    args.t1 = sy;
    args.t2 = B2dAlphaMul(alpha, entry->alpha);
    func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Steps and draws the handle's items. Play mode 4 steps every frame (and
// counts b2d_play_frame), mode 2 only seeks on the first two frames after
// a restart, mode 1 restarts the count and does not step. Hidden items
// (kind bit 4, or a hidden parent) and items with alpha 0 are not drawn.
// Returns the number of parts still stepping (0: every animation ended),
// -1 for a bad handle, -2 before the sprite hook is installed.
// FF3 0x00101fe8.

// PC: what the mouse points at (reipan_menu_mouse.h). The rect is the one the
// draw below uses: the item's, moved and scaled by the parent and the entry.
static void B2dNoteDrawnItem(int handle, const B2D_ENTRY *entry, const B2D_ITEM *item,
                             const B2D_ITEM *parent)
{
    B2D_RECT r;
    int      alpha = item->part[B2D_PART_ALPHA].value;
    int      rot   = item->part[B2D_PART_ROT].value;

    if (parent != nullptr)
    {
        alpha = B2dAlphaMul(alpha, parent->part[B2D_PART_ALPHA].value);
        rot  += parent->part[B2D_PART_ROT].value;
    }
    B2dItemGetRect(&r, item, parent);
    if (rot != 0)
    {
        // Turned, it covers the box around its corners.
        int xy[8];

        B2dItemGetQuad(xy, item, parent);
        int x0 = xy[0], x1 = xy[0], y0 = xy[1], y1 = xy[1];

        for (int i = 1; i < 4; i++)
        {
            x0 = xy[i * 2] < x0 ? xy[i * 2] : x0;
            x1 = xy[i * 2] > x1 ? xy[i * 2] : x1;
            y0 = xy[i * 2 + 1] < y0 ? xy[i * 2 + 1] : y0;
            y1 = xy[i * 2 + 1] > y1 ? xy[i * 2 + 1] : y1;
        }
        r.x = x0;
        r.y = y0;
        r.w = x1 - x0;
        r.h = y1 - y0;
    }
    ReiPan_MenuMouseNoteItem(handle, item->id, item->part[B2D_PART_PARENT].value, item->name,
                             (float)(B2dScale(r.x, entry->sx) + entry->x),
                             (float)(B2dScale(r.y, entry->sy) + entry->y),
                             (float)B2dScale(r.w, entry->sx),
                             (float)B2dScale(r.h, entry->sy),
                             B2dAlphaMul(alpha, entry->alpha));
}

int B2dCtlDraw(int handle, int frame)
{
    int n = 0;

    if (b2d_sprite_func == nullptr)
        return -2;

    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;

    if (b2d_play_mode & 4)
        b2d_play_frame++;
    else if (!(b2d_play_mode & 2) && (b2d_play_mode & 1))
        b2d_play_frame = 0;

    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        if ((b2d_play_mode & 4) || ((b2d_play_mode & 2) && b2d_play_frame < 2))
            n += B2dItemStep(item, frame);
    }

    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        B2D_ITEM *parent = nullptr;

        b2d_draw_item = item;
        if (item->part[B2D_PART_PARENT].value != -1)
        {
            parent = B2dCtlFindItem(handle, item->part[B2D_PART_PARENT].value);
            // PC: FF3 reads parent->kind without the check.
            if (parent != nullptr && (parent->kind & 0x10))
                continue;
        }
        if (item->kind & 0x10)
            continue;
        if (item->part[B2D_PART_ALPHA].value == 0)
            continue;

        B2dNoteDrawnItem(handle, entry, item, parent);     // PC
        switch (item->kind & 0xf)
        {
        case 0: B2dDrawSpriteItem(handle, item, parent, item->part); break;
        case 1: B2dDrawRectItem(entry, item, parent, item->part);    break;
        case 2: B2dDrawKind2Item(entry, item, parent, item->part);   break;
        case 3: B2dDrawKind3Item(entry, item, parent, item->part);   break;
        }
    }
    b2d_draw_item = nullptr;
    B2dResetCurTex();
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// The part restarts its current key from its present value.
// FF3 0x00102200.

void B2dPartReset(B2D_PART *part)
{
    part->frames = 0;
    part->start  = part->value;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00102210.

int B2dItemClearPartKeys(B2D_ITEM *item, int part)
{
    B2dPartReset(&item->part[part]);
    B2dPartFreeKeys(item, (u_int)part);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00102268.

int B2dCtlClearPartKeys(int handle, int id, int part)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return -1;
    return B2dItemClearPartKeys(item, part);
}

// ──────────────────────────────────────────────────────────────────────
// Returns a key to the pool (frames -1 marks it free). FF3 0x001022b0.

void B2dKeyFree(B2D_KEY *key)
{
    key->value  = 0;
    key->frames = -1;
    key->next   = nullptr;
    key->flags  = 0;
}

// ──────────────────────────────────────────────────────────────────────
// A new unlinked key from the handle's pool. FF3 0x001022c8.

B2D_KEY *B2dKeyNew(int handle, u_int flags, const int *value, int frames)
{
    B2D_KEY *key = B2dKeyAlloc(handle);

    if (key == nullptr)
        return nullptr;
    key->value  = *value;
    key->flags  = flags;
    key->frames = frames;
    key->next   = nullptr;
    return key;
}

// ──────────────────────────────────────────────────────────────────────
// Unlinks `key` from the part (the next key takes over its frames), cuts
// a loop-back link to it and links the loop end back to the head when its
// link was cut, then frees it. Returns the key before it (the old head
// when it was first); nullptr and an error print when the part has no keys
// (0x5b5), the key is not there (0x5c6) or lies past the loop end (0x5ca).
// FF3 0x00102328.

B2D_KEY *B2dPartRemoveKey(B2D_PART *part, B2D_KEY *key)
{
    B2D_KEY *prev = part->keys;

    if (prev == nullptr)
    {
        B2dErrPrint(-1, "B2dCtl.c", 0x5b5, 0);
        return nullptr;
    }

    if (prev == key)
    {
        part->keys = prev->next;
    }
    else
    {
        for (B2D_KEY *k = prev->next; k != key; k = k->next)
        {
            if (k == nullptr)
            {
                B2dErrPrint(-2, "B2dCtl.c", 0x5c6, 0);
                return nullptr;
            }
            if (prev->flags & B2D_KEY_LOOP_END)
            {
                B2dErrPrint(-3, "B2dCtl.c", 0x5ca, 0);
                return nullptr;
            }
            prev = k;
        }

        B2D_KEY *next = key->next;

        if (next != nullptr)
            next->frames += key->frames;
        prev->next = next;
    }

    // PC: FF3 walks from part->keys unchecked; it is nullptr when the only
    // key went, and FF3 then reads low memory.
    B2D_KEY *k = part->keys;
    B2D_KEY *n = (k != nullptr) ? k->next : nullptr;

    while (n != nullptr)
    {
        if (n == key)
        {
            k->next = nullptr;
            n = nullptr;
        }
        if (k->flags & B2D_KEY_LOOP_END)
        {
            if (n == nullptr)
                k->next = part->keys;
            break;
        }
        if (n == nullptr)       // PC: FF3 reads n->next from low memory here
            break;
        k = n;
        n = n->next;
    }

    B2dKeyFree(key);
    return prev;
}

// ──────────────────────────────────────────────────────────────────────
// Frees all keys of item part `part`. -1 (and error 0x198) for part >= 18.
// FF3 0x00102450.
// callcheck: B2dCtlFindItem (Ghidra's range runs on over B2dCtlFreePartKeys
// 0x1024d8, which has no function)

int B2dPartFreeKeys(B2D_ITEM *item, u_int part)
{
    B2D_PART *p = nullptr;

    if (part < B2D_ITEM_PARTS)
        p = &item->part[part];
    else
        B2dErrPrint(-1, "B2dCtl.c", 0x198, 0);

    if (p == nullptr)
        return -1;
    if (p->keys != nullptr)
    {
        B2D_KEY *key = p->keys;

        do
        {
            B2D_KEY *next = key->next;

            B2dKeyFree(key);
            key = next;
        } while (key != nullptr);
        p->keys = nullptr;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// B2dPartFreeKeys on item `id`; -2 without it. FF3 0x001024d8 (no Ghidra
// function; decoded from the bytes).

int B2dCtlFreePartKeys(int handle, int id, u_int part)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return -2;
    return B2dPartFreeKeys(item, part);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00102520.

void B2dCtlClearItemKeys(int handle, int id)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return;
    for (int i = 0; i < B2D_ITEM_PARTS; i++)
        B2dPartFreeKeys(item, (u_int)i);
}

// ──────────────────────────────────────────────────────────────────────
// Appends a new key to item part `part`. The first key starts the part
// (its value too, unless B2D_KEY_NO_START); later ones go after the last
// key, which loses B2D_KEY_LOOP_END, and a new loop-end key links back to
// the last B2D_KEY_LOOP_START key (else the head). 0, -1 bad part, -2 pool
// empty, -3 bad handle, -1 (error 0x679) for a list longer than the pool.
// The key is taken from the pool before the part is checked, as in FF3.
// FF3 0x00102578.
// callcheck: B2dCtlFindItem (Ghidra's range runs on over B2dCtlAddPartKey
// 0x102700, which has no function)

int B2dPartAddKey(int handle, B2D_ITEM *item, u_int part, u_int flags, const int *value, int frames)
{
    B2D_PART *p = nullptr;

    if (part < B2D_ITEM_PARTS)
        p = &item->part[part];
    else
        B2dErrPrint(-1, "B2dCtl.c", 0x198, 0);

    B2D_KEY *key = B2dKeyNew(handle, flags, value, frames);

    if (p == nullptr)
        return -1;
    if (key == nullptr)
        return -2;

    B2D_KEY *last = p->keys;

    if (last == nullptr)
    {
        p->keys   = key;
        p->cur    = key;
        p->frames = 0;
        if (!(flags & B2D_KEY_NO_START))
        {
            p->start = key->value;
            p->value = key->value;
        }
        return 0;
    }

    B2D_ENTRY *entry = B2dCtlGetEntry(handle);
    B2D_KEY   *loop  = last;
    int        n     = 0;
    u_int      lflags;

    if (entry == nullptr)
        return -3;

    lflags = last->flags;
    while (last->next != nullptr)
    {
        if (lflags & B2D_KEY_LOOP_START)
            loop = last;
        if (lflags & B2D_KEY_LOOP_END)
            break;
        n++;
        last = last->next;
        if (entry->nkey < n)
        {
            B2dErrPrint(-1, "B2dCtl.c", 0x679, 0);
            return -1;
        }
        lflags = last->flags;
    }
    last->next  = key;
    last->flags = lflags & ~B2D_KEY_LOOP_END;
    if (key->flags & B2D_KEY_LOOP_END)
        key->next = loop;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// B2dPartAddKey on item `id`; -1 without it. FF3 0x00102700 (no Ghidra
// function; decoded from the bytes).

int B2dCtlAddPartKey(int handle, int id, u_int part, u_int flags, const int *value, int frames)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return -1;
    return B2dPartAddKey(handle, item, part, flags, value, frames);
}

// ──────────────────────────────────────────────────────────────────────
// The position of `key` in item part `part`'s list (the list length when
// it is not there; `part` unchecked). FF3 0x00102798 (no Ghidra function;
// decoded from the bytes).

int B2dPartGetKeyIndex(B2D_ITEM *item, int part, const B2D_KEY *key)
{
    const B2D_KEY *k = item->part[part].keys;
    int            i = 0;

    while (k != key)
    {
        if (k == nullptr)
            return i;
        k = k->next;
        i++;
    }
    return i;
}

// ──────────────────────────────────────────────────────────────────────
// The n-th key of item part `part` (nullptr for n < 0; `part` unchecked).
// FF3 0x001027d0 (no Ghidra function; decoded from the bytes).
// PC: FF3 follows next n times without checking for the list's end.

B2D_KEY *B2dPartGetKeyByIndex(B2D_ITEM *item, int part, int n)
{
    if (n < 0)
        return nullptr;

    B2D_KEY *k = item->part[part].keys;

    for (; n > 0 && k != nullptr; n--)
        k = k->next;
    return k;
}

// ──────────────────────────────────────────────────────────────────────
// Appends `item` to the entry's item list. FF3 0x00102818.
// callcheck: B2dErrPrint (Ghidra's range runs on over B2dCtlRemoveItem
// 0x102860, which has no function)

int B2dCtlAppendItem(B2D_ENTRY *entry, B2D_ITEM *item)
{
    B2D_ITEM *last = entry->list;

    if (last == nullptr)
    {
        entry->list = item;
        return 0;
    }
    while (last->next != nullptr)
        last = last->next;
    last->next = item;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Unlinks `item` from the entry's item list: -1 (error 0x73e) when the
// list is empty, -2 (0x74f) when the item is not listed. FF3 0x00102860
// (no Ghidra function; decoded from the bytes). 0x1028f8 after it is a
// lone `return 0` with no known caller, not reconstructed.

int B2dCtlRemoveItem(B2D_ENTRY *entry, B2D_ITEM *item)
{
    B2D_ITEM *prev = entry->list;

    if (prev == nullptr)
    {
        B2dErrPrint(-1, "B2dCtl.c", 0x73e, 0);
        return -1;
    }
    if (prev == item)
    {
        entry->list = item->next;
        return 0;
    }
    for (B2D_ITEM *it = prev->next; it != nullptr; it = it->next)
    {
        if (it == item)
        {
            prev->next = item->next;
            return 0;
        }
        prev = it;
    }
    B2dErrPrint(-2, "B2dCtl.c", 0x74f, 0);
    return -2;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00102900.

void B2dPartInit(B2D_PART *part)
{
    part->value  = 0;
    part->frames = 0;
    part->cur    = nullptr;
    part->keys   = nullptr;
    part->start  = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The item `id` of the entry's array (*found = 1), else the first free slot
// (id -1), which gets its index and loses its hooks; nullptr and error
// 0x79c when the array is full. FF3 0x00102918.
// callcheck: B2dCtlFindItem, B2dCtlGetEntry, B2dPartFreeKeys, FUN_00102860
// (Ghidra's range runs on over B2dCtlDeleteItem 0x1029f0, which has no
// function; FUN_00102860 is B2dCtlRemoveItem)

B2D_ITEM *B2dCtlFindOrAllocItem(B2D_ENTRY *entry, int id, int *found)
{
    *found = 0;
    for (int i = 0; i < entry->nitem; i++)
    {
        if (entry->items[i].id == id)
        {
            *found = 1;
            return &entry->items[i];
        }
    }
    for (int i = 0; i < entry->nitem; i++)
    {
        B2D_ITEM *item = &entry->items[i];

        if (item->id == -1)
        {
            item->no    = (u_short)i;
            item->hook  = nullptr;
            item->hook2 = nullptr;
            return item;
        }
    }
    B2dErrPrint(-1, "B2dCtl.c", 0x79c, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes item `id`: a kind-3 item first detaches its children (their
// parent id becomes -1), then its keys are freed, it leaves the list and
// its slot is freed. 0, -1 no item, -2 bad handle. FF3 0x001029f0 (no
// Ghidra function; decoded from the bytes).

int B2dCtlDeleteItem(int handle, int id)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -2;

    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return -1;
    if ((item->kind & 0xf) == 3)
    {
        for (B2D_ITEM *it = entry->list; it != nullptr; it = it->next)
        {
            if (it->part[B2D_PART_PARENT].value == item->id)
            {
                it->part[B2D_PART_PARENT].value = -1;
                it->part[B2D_PART_PARENT].start = -1;
            }
        }
    }
    for (int i = 0; i < B2D_ITEM_PARTS; i++)
        B2dPartFreeKeys(item, (u_int)i);
    B2dCtlRemoveItem(entry, item);
    item->id = -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The part fields every item creator sets (start and value alike): scale
// 1.0, anchors 0, parent `parent`. Then the item joins the list unless
// B2dCtlNewItem reused a listed one. FF3 inlines this in each creator.

static int B2dCtlFinishNewItem(B2D_ENTRY *entry, B2D_ITEM *item, int found, int parent)
{
    B2D_PART *p = item->part;

    p[B2D_PART_PARENT].start  = parent;
    p[B2D_PART_PARENT].value  = parent;
    p[B2D_PART_SX].start      = 0x400;
    p[B2D_PART_SX].value      = 0x400;
    p[B2D_PART_SY].start      = 0x400;
    p[B2D_PART_SY].value      = 0x400;
    p[B2D_PART_ANCHORX].start = 0;
    p[B2D_PART_ANCHORX].value = 0;
    p[B2D_PART_ANCHORY].start = 0;
    p[B2D_PART_ANCHORY].value = 0;
    if (found == 0)
        B2dCtlAppendItem(entry, item);
    return (short)item->no;
}

static void B2dPartSetValue(B2D_PART *part, int value)
{
    part->start = value;
    part->value = value;
}

// ──────────────────────────────────────────────────────────────────────
// A kind-0 (sprite) item: texture, u, v, w, h and part 11. FF3 0x00102ad0
// (`name` is the ninth argument, on the EE stack).
// callcheck: B2dCtlAppendItem (in B2dCtlFinishNewItem, inlined by FF3)

int B2dCtlAddSpriteItem(int handle, int id, int tex, int u, int v, int w, int h, int text, const char *name)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;

    int       found;
    B2D_ITEM *item = B2dCtlNewItem(entry, id, &found, name);

    if (item == nullptr)
        return -1;
    B2dPartSetValue(&item->part[B2D_PART_TEX], tex);
    B2dPartSetValue(&item->part[B2D_PART_U], u);
    B2dPartSetValue(&item->part[B2D_PART_V], v);
    B2dPartSetValue(&item->part[B2D_PART_W], w);
    B2dPartSetValue(&item->part[B2D_PART_H], h);
    B2dPartSetValue(&item->part[B2D_PART_TEXT], text);
    return B2dCtlFinishNewItem(entry, item, found, -1);
}

// ──────────────────────────────────────────────────────────────────────
// A kind-1 (rect) item without a name: colour in part 1, w, h, part 11.
// FF3 0x00102bf8.
// callcheck: B2dCtlAppendItem (in B2dCtlFinishNewItem, inlined by FF3)

int B2dCtlAddRectItem(int handle, int id, int rgba, int w, int h, int text)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;

    int       found;
    B2D_ITEM *item = B2dCtlNewItem(entry, id, &found, nullptr);

    if (item == nullptr)
        return -1;
    B2dPartSetValue(&item->part[B2D_PART_U], rgba);
    B2dPartSetValue(&item->part[B2D_PART_W], w);
    B2dPartSetValue(&item->part[B2D_PART_H], h);
    B2dPartSetValue(&item->part[B2D_PART_TEXT], text);
    item->kind |= 1;
    return B2dCtlFinishNewItem(entry, item, found, -1);
}

// ──────────────────────────────────────────────────────────────────────
// A kind-2 item (drawn by hook 5): w, h, part 11. FF3 0x00102d00.
// callcheck: B2dCtlAppendItem (in B2dCtlFinishNewItem, inlined by FF3)

int B2dCtlAddHookItem(int handle, int id, int w, int h, int text, const char *name)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;

    int       found;
    B2D_ITEM *item = B2dCtlNewItem(entry, id, &found, name);

    if (item == nullptr)
        return -1;
    B2dPartSetValue(&item->part[B2D_PART_W], w);
    B2dPartSetValue(&item->part[B2D_PART_H], h);
    B2dPartSetValue(&item->part[B2D_PART_TEXT], text);
    item->kind |= 2;
    return B2dCtlFinishNewItem(entry, item, found, -1);
}

// ──────────────────────────────────────────────────────────────────────
// A kind-3 item (drawn by hook 6; B2dCtlDeleteItem detaches the items that
// name it as parent): size 0, part 11 = 100, its own id as parent.
// FF3 0x00102e00.
// callcheck: B2dCtlAppendItem (in B2dCtlFinishNewItem, inlined by FF3)

int B2dCtlAddGroupItem(int handle, int id, const char *name)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;

    int       found;
    B2D_ITEM *item = B2dCtlNewItem(entry, id, &found, name);

    if (item == nullptr)
        return -1;
    B2dPartSetValue(&item->part[B2D_PART_W], 0);
    B2dPartSetValue(&item->part[B2D_PART_H], 0);
    B2dPartSetValue(&item->part[B2D_PART_TEXT], 100);
    item->kind |= 3;
    return B2dCtlFinishNewItem(entry, item, found, item->id);
}

// ──────────────────────────────────────────────────────────────────────
// Keeps only the part's first key: copies it, frees the list and adds the
// copy back. -2 (error 0x8be) without keys. FF3 0x00102ee0.
// callcheck: B2dCtlFindItem (Ghidra's range runs on over B2dCtlKeepFirstKey
// 0x102fc0, which has no function)

int B2dPartKeepFirstKey(int handle, B2D_ITEM *item, u_int part)
{
    B2D_PART *p = nullptr;

    if (part < B2D_ITEM_PARTS)
        p = &item->part[part];
    else
        B2dErrPrint(-1, "B2dCtl.c", 0x198, 0);

    if (p == nullptr)
        return -1;
    if (p->keys == nullptr)
    {
        B2dErrPrint(-2, "B2dCtl.c", 0x8be, 0);
        return -2;
    }

    B2D_KEY first;

    memcpy(&first, p->keys, sizeof(first));
    B2dPartFreeKeys(item, part);
    return B2dPartAddKey(handle, item, part, first.flags, &first.value, first.frames);
}

// ──────────────────────────────────────────────────────────────────────
// B2dPartKeepFirstKey on item `id`; -1 without it. FF3 0x00102fc0 (no
// Ghidra function; decoded from the bytes).

int B2dCtlKeepFirstKey(int handle, int id, u_int part)
{
    B2D_ITEM *item = B2dCtlFindItem(handle, id);

    if (item == nullptr)
        return -1;
    return B2dPartKeepFirstKey(handle, item, part);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00103020.

int B2dCtlKeepFirstKeys(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;
    for (B2D_ITEM *item = entry->list; item != nullptr; item = item->next)
    {
        for (int i = 0; i < B2D_ITEM_PARTS; i++)
            B2dPartKeepFirstKey(handle, item, (u_int)i);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The entry's item array and key pool. FF3 0x001030a8.

void B2dCtlGetPool(int handle, B2D_ITEM **items, int *nitem, B2D_KEY **keys, int *nkey)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return;
    *items = entry->items;
    *keys  = entry->keys;
    *nitem = entry->nitem;
    *nkey  = entry->nkey;
}

// FF3 0x00103118.
void B2dCtlSetPool(int handle, B2D_ITEM *items, int nitem, B2D_KEY *keys, int nkey)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return;
    entry->items    = items;
    entry->keys     = keys;
    entry->nitem    = nitem;
    entry->nkey     = nkey;
    entry->key_next = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Frees every pool key, marks every item free, empties the list and resets
// the transform. FF3 0x00103180.

int B2dCtlReset(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return -1;
    if (entry->keys != nullptr)
    {
        for (int i = 0; i < entry->nkey; i++)
            B2dKeyFree(&entry->keys[i]);
    }
    if (entry->items != nullptr)
    {
        for (int i = 0; i < entry->nitem; i++)
        {
            entry->items[i].id   = -1;
            entry->items[i].kind = 0;
        }
    }
    entry->list  = nullptr;
    entry->sy    = 0x400;
    entry->alpha = 0x80;
    entry->x     = 0;
    entry->y     = 0;
    entry->sx    = 0x400;
    B2dResetCurTex();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Binds entry `handle` to the layout work: nitem items, then the nkey-key
// pool. -1 (error 0x951) for handle >= 45. FF3 0x00103278.
// PC: the pool starts after nitem host-sized items (0x1c8 bytes each on
// the EE); B2dFileGetWorkSize sizes the work the same way.

int B2dCtlSetup(int handle, void *work, int nitem, int nkey)
{
    if ((u_int)handle >= B2D_ENTRY_MAX)
    {
        B2dErrPrint(-1, "B2dCtl.c", 0x951, 0);
        return -1;
    }

    B2D_ENTRY *entry = &b2d_entry[handle];

    entry->id       = handle;
    entry->keys     = (B2D_KEY *)((B2D_ITEM *)work + nitem);
    entry->nitem    = nitem;
    entry->nkey     = nkey;
    entry->key_next = 0;
    entry->data     = work;
    entry->list     = nullptr;
    entry->items    = (B2D_ITEM *)work;
    B2dCtlReset(handle);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Clears every entry and the play state. FF3 0x00103308.

int B2dCtlInit(void)
{
    b2d_play_mode = 1;
    memset(b2d_entry, 0, sizeof(b2d_entry));
    for (int i = 0; i < B2D_ENTRY_MAX; i++)
    {
        b2d_entry[i].id    = -1;
        b2d_entry[i].alpha = 0x80;
    }
    b2d_sprite_func  = nullptr;
    b2d_tex_set_func = nullptr;
    b2d_draw_func4   = nullptr;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A free pool key: searches from the cursor, then once more from the start;
// nullptr (error 0x54e, code = handle) when the pool is full.
// FF3 0x00103380.

B2D_KEY *B2dKeyAlloc(int handle)
{
    B2D_ENTRY *entry = B2dCtlGetEntry(handle);

    if (entry == nullptr)
        return nullptr;
    for (;;)
    {
        int from = entry->key_next;

        for (int i = from; i < entry->nkey; i++)
        {
            if (entry->keys[i].frames == -1)
            {
                entry->key_next = i + 1;
                return &entry->keys[i];
            }
        }
        if (from == 0)
            break;
        entry->key_next = 0;
    }
    B2dErrPrint(handle, "B2dCtl.c", 0x54e, 0);
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// A fresh item `id` named `name` (B2dCtlFindOrAllocItem): kind 0, unlinked,
// every part cleared with its type from b2d_part_type_tbl.
// FF3 0x00103430.

B2D_ITEM *B2dCtlNewItem(B2D_ENTRY *entry, int id, int *found, const char *name)
{
    B2D_ITEM *item = B2dCtlFindOrAllocItem(entry, id, found);

    if (item == nullptr)
        return nullptr;
    item->id   = id;
    item->name = name;
    item->kind = 0;
    item->next = nullptr;
    for (int i = 0; i < B2D_ITEM_PARTS; i++)
    {
        B2D_PART *part = &item->part[i];

        B2dPartInit(part);
        part->type = 0;
        for (int t = 0; b2d_part_type_tbl[t][0] > 0; t++)
        {
            if (i == b2d_part_type_tbl[t][0])
            {
                part->type = (u_short)b2d_part_type_tbl[t][1];
                break;
            }
        }
    }
    return item;
}

// ──────────────────────────────────────────────────────────────────────
// The item rect in layout space: position plus the parent's, size scaled
// by the item's and the parent's scale times the tile counts, then moved
// back by the anchor. FF3 0x00103520.

int B2dItemGetRect(B2D_RECT *out, const B2D_ITEM *item, const B2D_ITEM *parent)
{
    const B2D_PART *p   = item->part;
    int             px  = 0;
    int             py  = 0;
    int             psx = 0x400;
    int             psy = 0x400;

    if (parent != nullptr)
    {
        psy = parent->part[B2D_PART_SY].value;
        px  = parent->part[B2D_PART_X].value;
        py  = parent->part[B2D_PART_Y].value;
        psx = parent->part[B2D_PART_SX].value;
    }

    out->x = p[B2D_PART_X].value + px;
    out->y = p[B2D_PART_Y].value + py;

    int w = B2dScale(B2dScale(p[B2D_PART_W].value, p[B2D_PART_SX].value), psx);
    int h = B2dScale(B2dScale(p[B2D_PART_H].value, p[B2D_PART_SY].value), psy);

    out->w = w * abs(p[B2D_PART_REPX].value);
    out->h = h * abs(p[B2D_PART_REPY].value);
    out->x -= B2dScale(out->w, p[B2D_PART_ANCHORX].value);
    out->y -= B2dScale(out->h, p[B2D_PART_ANCHORY].value);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The item rect's corners (top-left, bottom-left, bottom-right, top-right),
// rotated around the anchor by the item's and the parent's rotation.
// FF3 0x00103658.

void B2dItemGetQuad(int *xy, const B2D_ITEM *item, const B2D_ITEM *parent)
{
    int      rot = item->part[B2D_PART_ROT].value;
    B2D_RECT r;

    if (parent != nullptr)
        rot += parent->part[B2D_PART_ROT].value;

    B2dItemGetRect(&r, item, parent);
    xy[0] = r.x;
    xy[1] = r.y;
    xy[2] = r.x;
    xy[3] = r.y + r.h;
    xy[4] = r.x + r.w;
    xy[5] = r.y + r.h;
    xy[6] = r.x + r.w;
    xy[7] = r.y;
    if (rot != 0)
    {
        B2dRotatePoints(xy, 4,
                     B2dScale(r.w, item->part[B2D_PART_ANCHORX].value) + r.x,
                     B2dScale(r.h, item->part[B2D_PART_ANCHORY].value) + r.y, rot);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Modulates two packed RGBA colours: per byte a * b / 128 (sdata 0x36b544
// = 1/128), clamped to 0..255 (0x36b548). FF3 0x00103750.

u_int B2dRgbaMul(u_int a, u_int b)
{
    u_int out = 0;

    for (int shift = 24; shift >= 0; shift -= 8)
    {
        float f = (float)((a >> shift) & 0xff) * (float)((b >> shift) & 0xff) * (1.0f / 128.0f);
        float c = 255.0f;

        if (f < 255.0f)
        {
            c = 0.0f;
            if (0.0f < f)
                c = f;
        }
        out = (out << 8) | ((u_int)(int)c & 0xff);
    }
    return out;
}

// ──────────────────────────────────────────────────────────────────────
// The sprite's texel corners from parts 1..4, in B2dItemGetQuad's order.
// FF3 0x001037f0.

static void B2dItemGetUvQuad(int *uv, const B2D_PART *parts)
{
    int u = parts[B2D_PART_U].value;
    int v = parts[B2D_PART_V].value;
    int w = parts[B2D_PART_W].value;
    int h = parts[B2D_PART_H].value;

    uv[0] = u;
    uv[1] = v;
    uv[2] = u;
    uv[3] = v + h;
    uv[4] = u + w;
    uv[5] = v + h;
    uv[6] = u + w;
    uv[7] = v;
}

// ──────────────────────────────────────────────────────────────────────
// A tile's mirror flags: odd rows toggle u / v mirroring for tile bits
// 0x40000 / 0x80000, odd columns for 0x10000 / 0x20000. FF3 0x00103830.

static u_int B2dTileFlipFlags(int row, int col, u_int tile, u_int flags)
{
    if (row % 2 == 1)
    {
        if (tile & 0x40000)
            flags ^= 0x1000;
        if (tile & 0x80000)
            flags ^= 0x2000;
    }
    if (col % 2 == 1)
    {
        if (tile & 0x10000)
            flags ^= 0x1000;
        if (tile & 0x20000)
            flags ^= 0x2000;
    }
    return flags;
}

// ──────────────────────────────────────────────────────────────────────
// Tiles an unrotated sprite nx * ny times from the item rect's corner,
// each tile through the item's hook or the sprite hook. The tile step is
// scaled by the entry; the position is only moved by it.
// FF3 0x001038b0.

static void B2dDrawSpriteTiled(B2D_ITEM *item, const B2D_ITEM *parent, int nx, int ny,
                               int ex, int ey, int esx, int esy, int ealpha)
{
    const B2D_PART    *parts = item->part;
    u_int              flip  = B2dPartKeyFlags(&parts[B2D_PART_X], B2D_KEY_FLIP_MASK);
    u_int              tile  = B2dPartKeyFlags(&parts[B2D_PART_REPX], B2D_KEY_TILE_MASK);
    int                sx    = parts[B2D_PART_SX].value;
    int                sy    = parts[B2D_PART_SY].value;
    int                alpha = parts[B2D_PART_ALPHA].value;
    u_int              rgb   = (u_int)parts[B2D_PART_RGB].value;
    B2D_ITEM_HOOK_FUNC func  = item->hook;
    B2D_RECT           r;

    if (func == nullptr)
        func = B2dGetSpriteFunc();
    B2dItemGetRect(&r, item, parent);

    int tw = B2dScale(B2dScale(parts[B2D_PART_W].value, parts[B2D_PART_SX].value), esx);
    int th = B2dScale(B2dScale(parts[B2D_PART_H].value, parts[B2D_PART_SY].value), esy);

    if (parent != nullptr)
    {
        const B2D_PART *pp = parent->part;

        sx    = B2dScale(sx, pp[B2D_PART_SX].value);
        sy    = B2dScale(sy, pp[B2D_PART_SY].value);
        alpha = B2dAlphaMul(alpha, pp[B2D_PART_ALPHA].value);
        rgb   = B2dRgbaMul(rgb, (u_int)pp[B2D_PART_RGB].value);
        th    = B2dScale(th, pp[B2D_PART_SY].value);
        tw    = B2dScale(tw, pp[B2D_PART_SX].value);
    }

    int y = r.y + ey;

    for (int row = 0; row < ny; row++)
    {
        int x = r.x + ex;

        for (int col = 0; col < nx; col++)
        {
            B2D_HOOK_ARGS args = {};

            args.a0       = parts[B2D_PART_TEX].value;
            args.a1       = parts[B2D_PART_U].value;
            args.a2       = parts[B2D_PART_V].value;
            args.a3       = parts[B2D_PART_W].value;
            args.t0       = parts[B2D_PART_H].value;
            args.t1       = x;
            args.t2       = y;
            args.t3       = sx;
            args.stack[0] = sy;
            args.stack[1] = B2dAlphaMul(alpha, ealpha);
            args.stack[2] = rgb;
            args.stack[3] = B2dTileFlipFlags(row, col, tile, flip)
                          | B2dPartKeyFlags(&parts[B2D_PART_ALPHA], B2D_KEY_BLEND_MASK);
            func(&args);
            x += tw;
        }
        y += th;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The point (i / n across, j / m down) of a quad in B2dItemGetQuad's
// corner order. FF3 0x00103ba0.

static int B2dQuadLerp(int *out, const int *q, int n, int m, int i, int j)
{
    int lx = (q[2] - q[0]) * j / m + q[0];
    int ly = (q[3] - q[1]) * j / m + q[1];
    int dx = ((q[4] - q[6]) * j / m + q[6]) - lx;
    int dy = ((q[5] - q[7]) * j / m + q[7]) - ly;

    out[0] = dx * i / n + lx;
    out[1] = dy * i / n + ly;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A rotated tiled sprite: the item's screen quad cut into nx * ny quads,
// each drawn with the whole sprite's texels through the item's hook2 or the
// quad hook. FF3 0x00103c88.

static void B2dDrawSpriteTiledRot(B2D_ITEM *item, const B2D_ITEM *parent, int nx, int ny,
                                  int ex, int ey, int esx, int esy, int ealpha)
{
    const B2D_PART    *parts = item->part;
    u_int              flip  = B2dPartKeyFlags(&parts[B2D_PART_X], B2D_KEY_FLIP_MASK);
    u_int              tile  = B2dPartKeyFlags(&parts[B2D_PART_REPX], B2D_KEY_TILE_MASK);
    int                alpha = parts[B2D_PART_ALPHA].value;
    u_int              rgb   = (u_int)parts[B2D_PART_RGB].value;
    B2D_ITEM_HOOK_FUNC func  = item->hook2;
    int                q[8];
    int                uv[8];

    if (func == nullptr)
        func = B2dGetQuadFunc();
    B2dItemGetQuad(q, item, parent);
    for (int i = 0; i < 4; i++)
    {
        q[i * 2]     = B2dScaleTrunc(q[i * 2], esx) + ex;
        q[i * 2 + 1] = B2dScaleTrunc(q[i * 2 + 1], esy) + ey;
    }
    if (parent != nullptr)
    {
        alpha = B2dAlphaMul(alpha, parent->part[B2D_PART_ALPHA].value);
        rgb   = B2dRgbaMul(rgb, (u_int)parent->part[B2D_PART_RGB].value);
    }
    B2dItemGetUvQuad(uv, parts);

    for (int row = 0; row < ny; row++)
    {
        for (int col = 0; col < nx; col++)
        {
            int           c[8];
            B2D_HOOK_ARGS args = {};

            B2dQuadLerp(&c[0], q, nx, ny, col, row);
            B2dQuadLerp(&c[2], q, nx, ny, col, row + 1);
            B2dQuadLerp(&c[4], q, nx, ny, col + 1, row + 1);
            B2dQuadLerp(&c[6], q, nx, ny, col + 1, row);
            args.a0 = parts[B2D_PART_TEX].value;
            args.a1 = (intptr_t)c;
            args.a2 = (intptr_t)uv;
            args.a3 = B2dAlphaMul(alpha, ealpha);
            args.t0 = rgb;
            args.t1 = B2dTileFlipFlags(row, col, tile, flip)
                    | B2dPartKeyFlags(&parts[B2D_PART_ALPHA], B2D_KEY_BLEND_MASK);
            func(&args);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// A tiled sprite: |part 12| x |part 13| tiles, rotated or not.
// FF3 0x00103f08.

static int B2dDrawSpriteTiles(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha)
{
    int rot = item->part[B2D_PART_ROT].value;
    int nx  = abs(item->part[B2D_PART_REPX].value);
    int ny  = abs(item->part[B2D_PART_REPY].value);

    if (parent != nullptr)
        rot += parent->part[B2D_PART_ROT].value;

    if (rot != 0)
        B2dDrawSpriteTiledRot(item, parent, nx, ny, ex, ey, esx, esy, ealpha);
    else
        B2dDrawSpriteTiled(item, parent, nx, ny, ex, ey, esx, esy, ealpha);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A rotated sprite: the item's screen quad (scaled by the entry,
// truncated) and texel quad through the item's hook2 or the quad hook.
// FF3 0x00103fa8.

static int B2dDrawSpriteRot(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha)
{
    const B2D_PART    *parts = item->part;
    int                alpha = parts[B2D_PART_ALPHA].value;
    u_int              rgb   = (u_int)parts[B2D_PART_RGB].value;
    B2D_ITEM_HOOK_FUNC func  = item->hook2;
    int                q[8];
    int                uv[8];
    B2D_HOOK_ARGS      args = {};

    if (func == nullptr)
        func = B2dGetQuadFunc();
    B2dItemGetQuad(q, item, parent);
    for (int i = 0; i < 4; i++)
    {
        q[i * 2]     = B2dScaleTrunc(q[i * 2], esx) + ex;
        q[i * 2 + 1] = B2dScaleTrunc(q[i * 2 + 1], esy) + ey;
    }
    if (parent != nullptr)
    {
        alpha = B2dAlphaMul(alpha, parent->part[B2D_PART_ALPHA].value);
        rgb   = B2dRgbaMul(rgb, (u_int)parent->part[B2D_PART_RGB].value);
    }
    B2dItemGetUvQuad(uv, parts);

    args.a0 = parts[B2D_PART_TEX].value;
    args.a1 = (intptr_t)q;
    args.a2 = (intptr_t)uv;
    args.a3 = B2dAlphaMul(alpha, ealpha);
    args.t0 = rgb;
    args.t1 = B2dPartKeyFlags(&parts[B2D_PART_X], B2D_KEY_FLIP_MASK)
            | B2dPartKeyFlags(&parts[B2D_PART_ALPHA], B2D_KEY_BLEND_MASK);
    func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A rotated rect: the item's screen quad in part 1's colour bytes through
// the item's hook2 or the flat quad hook. FF3 0x00104138.
// FF3 quirk kept: with a parent the alpha becomes the parent's alpha times
// the entry's (the item's own is dropped), then times the entry's again.

static int B2dDrawRectRot(B2D_ITEM *item, const B2D_ITEM *parent, int ex, int ey, int esx, int esy, int ealpha)
{
    const B2D_PART    *parts = item->part;
    int                alpha = parts[B2D_PART_ALPHA].value;
    B2D_ITEM_HOOK_FUNC func  = item->hook2;
    int                q[8];
    B2D_HOOK_ARGS      args = {};

    if (func == nullptr)
        func = B2dGetQuadFlatFunc();
    B2dItemGetQuad(q, item, parent);
    for (int i = 0; i < 4; i++)
    {
        q[i * 2]     = B2dScaleTrunc(q[i * 2], esx) + ex;
        q[i * 2 + 1] = B2dScaleTrunc(q[i * 2 + 1], esy) + ey;
    }
    if (parent != nullptr)
        alpha = B2dAlphaMul(parent->part[B2D_PART_ALPHA].value, ealpha);

    args.a0 = (intptr_t)&parts[B2D_PART_U].value;           // the colour bytes
    args.a1 = (intptr_t)q;
    args.a2 = B2dAlphaMul(alpha, ealpha);
    args.a3 = B2dPartKeyFlags(&parts[B2D_PART_X], B2D_KEY_FLIP_MASK)
            | B2dPartKeyFlags(&parts[B2D_PART_ALPHA], B2D_KEY_BLEND_MASK);
    func(&args);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001042a0.

void B2dSetErrPrintFunc(B2D_ITEM_HOOK_FUNC func)
{
    b2d_err_print_func = func;
}

// ──────────────────────────────────────────────────────────────────────
// Reports a core error through the callback (BpCtl installs one that
// prints nothing). FF3 0x001042a8.
// callcheck: FUN_002a1478 (newlib printf: vfprintf on _impure_ptr's stdout)

void B2dErrPrint(int code, const char *file, int line, int a3)
{
    if (b2d_err_print_func == nullptr)
        return;

    B2D_HOOK_ARGS args = {};

    printf(" s_B2dErrPrintCallback = %x\n", (u_int)(uintptr_t)b2d_err_print_func);  /* 0x34bd70 */
    args.a0 = code;
    args.a1 = (intptr_t)file;
    args.a2 = line;
    args.a3 = a3;
    b2d_err_print_func(&args);
}
