#include "reipan_menu_mouse.h"

#include "reipan_input.h"

#include "../rendering/reipan_renderer.h"
#include "ingame/menu/menu_mouse.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Presses waiting their turn.  Each takes two pad reads, down then up, so a
 * fast flick of the wheel is capped rather than playing out for a second
 * after the hand has stopped. */
#define MENU_MOUSE_QUEUE_MAX 16
#define MENU_MOUSE_BUTTONS   16

/* Layout items drawn in a frame.  The busiest screens draw a few hundred;
 * past the end an item is simply not a target. */
#define MENU_MOUSE_ITEM_MAX  2048
#define MENU_MOUSE_NAME_MAX  32

/* An item this faint is on its way in or out, not something to point at. */
#define MENU_MOUSE_ALPHA_MIN 8

/* Boxes the menu tested in a frame, for the dump. */
#define MENU_MOUSE_TESTED_MAX 128

/* Scripted steps of a smoke run. */
#define MENU_MOUSE_SCRIPT_MAX 256

namespace
{
struct DrawnItem
{
    int            handle;
    int            id;
    int            parent;
    int            alpha;
    ReiPan_MenuBox box;
    char           name[MENU_MOUSE_NAME_MAX];
};

struct DrawnItems
{
    DrawnItem item[MENU_MOUSE_ITEM_MAX];
    int       count;
};

/* One step of REIPAN_AUTOMOUSE. */
struct ScriptStep
{
    int   read;
    float x;
    float y;
    int   has_pos;
    char  action;   /* 0, L left, R right, U wheel up, D wheel down, E Enter,
                       B Backspace */
};

struct MenuMouseState
{
    /* Gathered from events since the last read. */
    int   pending_left;
    int   pending_right;
    int   pending_wheel_x;
    int   pending_wheel_y;
    int   pending_enter;
    int   pending_back;
    int   pending_moved;
    float wheel_travel_x;
    float wheel_travel_y;

    /* The pointer, in window points, from the last event that placed it. */
    float        pointer_x;
    float        pointer_y;
    SDL_WindowID pointer_window;
    int          pointer_valid;
    int          left_down;

    /* What this frame's menu sees. */
    ReiPan_MenuMouse frame;
    int              available;
    int              reads;

    /* The menu that asked last, and the one before it. */
    int claimed;
    int claimed_prev;
    int claim_flags;

    /* Presses in order, and the one down for the read now running. */
    int queue[MENU_MOUSE_QUEUE_MAX];
    int queue_len;
    int active;             /* button + 1, 0 for none */

    /* Last frame's drawing, and this frame's as it happens. */
    DrawnItems  items_a;
    DrawnItems  items_b;
    DrawnItems *drawn;      /* the finished frame the menus read */
    DrawnItems *drawing;

    /* For the dump. */
    ReiPan_MenuBox tested[MENU_MOUSE_TESTED_MAX];
    int            tested_count;
    int            tested_poll;     /* of them, the poll's: hints, Yes / No */
    int            tested_prev;     /* the menu's own, last frame */

    /* A scripted run: the pointer is in game coordinates, no window. */
    ScriptStep script[MENU_MOUSE_SCRIPT_MAX];
    int        script_count;
    int        script_next;
    int        script_reads;
    int        script_active;
    float      script_x;
    float      script_y;
    int        script_inited;
};

MenuMouseState g_mm;

void Queue(int button, int count)
{
    if (button < 0 || button >= MENU_MOUSE_BUTTONS)
    {
        return;
    }
    for (int i = 0; i < count && g_mm.queue_len < MENU_MOUSE_QUEUE_MAX; i++)
    {
        g_mm.queue[g_mm.queue_len++] = button;
    }
}

bool Busy()
{
    return g_mm.queue_len != 0 || g_mm.active != 0;
}

/* Where the presses already made will have moved a cursor stepped by `dec` /
 * `inc`: the one down this read has not reached the menu yet when its pad
 * handler asks, and the queued ones are still to come. */
int PendingSteps(int dec, int inc, bool queued)
{
    int steps = 0;
    if (g_mm.active == inc + 1)
    {
        steps++;
    }
    else if (g_mm.active == dec + 1)
    {
        steps--;
    }
    for (int i = 0; queued && i < g_mm.queue_len; i++)
    {
        steps += g_mm.queue[i] == inc ? 1 : g_mm.queue[i] == dec ? -1 : 0;
    }
    return steps;
}

/* Re-aim: drop the steps still queued and queue the ones from where the
 * cursor will be once the press now down has landed. */
void SeekTo(int cursor, int target, int dec, int inc)
{
    int kept = 0;
    for (int i = 0; i < g_mm.queue_len; i++)
    {
        if (g_mm.queue[i] != dec && g_mm.queue[i] != inc)
        {
            g_mm.queue[kept++] = g_mm.queue[i];
        }
    }
    g_mm.queue_len = kept;
    const int steps = target - (cursor + PendingSteps(dec, inc, false));
    Queue(steps > 0 ? inc : dec, steps > 0 ? steps : -steps);
}

/* The wheel as presses, per the flags of the menu that has it. */
void QueueWheel(int flags, int x, int y)
{
    const int notches = y > 0 ? y : -y;
    if (y != 0)
    {
        if (flags & REIPAN_MM_WHEEL_VERTICAL)
        {
            Queue(y > 0 ? REIPAN_MENU_UP : REIPAN_MENU_DOWN, notches);
        }
        else if (flags & REIPAN_MM_WHEEL_HORIZONTAL)
        {
            Queue(y > 0 ? REIPAN_MENU_LEFT : REIPAN_MENU_RIGHT, notches);
        }
        else if (flags & REIPAN_MM_WHEEL_SHOULDERS)
        {
            Queue(y > 0 ? REIPAN_MENU_L1 : REIPAN_MENU_R1, notches);
        }
    }
    if (x != 0 && (flags & REIPAN_MM_WHEEL_HORIZONTAL))
    {
        Queue(x < 0 ? REIPAN_MENU_LEFT : REIPAN_MENU_RIGHT, x < 0 ? -x : x);
    }
}

/* The pointer, in the ROM's 640x448 screen: window points to output pixels,
 * then the picture's place in the output, then the 2D frame's place in the
 * picture -- ReiPan_RendererGetViewBounds(), which is how far the widened
 * screen runs past 0 and 640.  The position is the last event's rather than
 * SDL's polled state, which is the same thing for a real mouse and lets an
 * injected event be a pointer too. */
void ReadPointer(ReiPan_MenuMouse *out)
{
    if (g_mm.script_active)
    {
        out->x = g_mm.script_x;
        out->y = g_mm.script_y;
        out->inside = 1;
        return;
    }

    SDL_Window *window = g_mm.pointer_valid
                             ? SDL_GetWindowFromID(g_mm.pointer_window)
                             : nullptr;
    if (window == nullptr)
    {
        return;
    }
    const float wx = g_mm.pointer_x;
    const float wy = g_mm.pointer_y;
    float density = SDL_GetWindowPixelDensity(window);
    if (!(density > 0.0f))
    {
        density = 1.0f;
    }

    int rx = 0;
    int ry = 0;
    int rw = 0;
    int rh = 0;
    ReiPan_RendererGetPresentRect(&rx, &ry, &rw, &rh);
    if (rw <= 0 || rh <= 0)
    {
        SDL_GetWindowSizeInPixels(window, &rw, &rh);
        rx = 0;
        ry = 0;
    }
    if (rw <= 0 || rh <= 0)
    {
        return;
    }

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 640.0f;
    float y1 = 448.0f;
    ReiPan_RendererGetViewBounds(&x0, &y0, &x1, &y1);

    const float px = wx * density - (float)rx;
    const float py = wy * density - (float)ry;
    out->x = x0 + px / (float)rw * (x1 - x0);
    out->y = y0 + py / (float)rh * (y1 - y0);
    out->inside = px >= 0.0f && py >= 0.0f && px < (float)rw && py < (float)rh;
    out->left_held = g_mm.left_down;
}

void NoteTested(float x, float y, float w, float h)
{
    if (g_mm.tested_count > 0)
    {
        const ReiPan_MenuBox &last = g_mm.tested[g_mm.tested_count - 1];
        if (last.x == x && last.y == y && last.w == w && last.h == h)
        {
            return;
        }
    }
    if (g_mm.tested_count < MENU_MOUSE_TESTED_MAX)
    {
        ReiPan_MenuBox &box = g_mm.tested[g_mm.tested_count++];
        box.x = x;
        box.y = y;
        box.w = w;
        box.h = h;
    }
}

/* "<read>:<x>,<y>[:L|R|U|D|E|B]" or "<read>:L", comma... separated by ';'. */
void ParseScript()
{
    g_mm.script_inited = 1;
    const char *env = getenv("REIPAN_AUTOMOUSE");
    if (env == nullptr || env[0] == '\0')
    {
        return;
    }
    const char *p = env;
    while (*p != '\0' && g_mm.script_count < MENU_MOUSE_SCRIPT_MAX)
    {
        ScriptStep step;
        memset(&step, 0, sizeof(step));
        char *end = nullptr;
        step.read = (int)strtol(p, &end, 10);
        if (end == p)
        {
            break;
        }
        p = end;
        while (*p == ':')
        {
            p++;
            if (*p >= 'A' && *p <= 'Z')
            {
                step.action = *p++;
                continue;
            }
            step.x = strtof(p, &end);
            p = end;
            if (*p == ',')
            {
                p++;
            }
            step.y = strtof(p, &end);
            p = end;
            step.has_pos = 1;
        }
        g_mm.script[g_mm.script_count++] = step;
        while (*p == ';' || *p == ' ')
        {
            p++;
        }
    }
    g_mm.script_active = g_mm.script_count > 0;
    fprintf(stderr, "[menu mouse] REIPAN_AUTOMOUSE: %d steps\n", g_mm.script_count);
}

void RunScript()
{
    g_mm.script_reads++;
    while (g_mm.script_next < g_mm.script_count &&
           g_mm.script[g_mm.script_next].read <= g_mm.script_reads)
    {
        const ScriptStep &step = g_mm.script[g_mm.script_next++];
        if (step.has_pos)
        {
            g_mm.script_x = step.x;
            g_mm.script_y = step.y;
            g_mm.pending_moved = 1;
        }
        switch (step.action)
        {
        case 'L': g_mm.pending_left++; break;
        case 'R': g_mm.pending_right++; break;
        case 'U': g_mm.pending_wheel_y++; break;
        case 'D': g_mm.pending_wheel_y--; break;
        case 'E': g_mm.pending_enter++; break;
        case 'B': g_mm.pending_back++; break;
        default:  break;
        }
    }
}

/* A group's box: what it holds, as drawn. */
int GroupBox(const DrawnItem *group, ReiPan_MenuBox *out)
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    int   found = 0;

    for (int i = 0; i < g_mm.drawn->count; i++)
    {
        const DrawnItem &item = g_mm.drawn->item[i];
        if (item.handle != group->handle || item.parent != group->id ||
            item.id == group->id || item.alpha < MENU_MOUSE_ALPHA_MIN ||
            item.box.w <= 0.0f || item.box.h <= 0.0f)
        {
            continue;
        }
        const float ix1 = item.box.x + item.box.w;
        const float iy1 = item.box.y + item.box.h;
        x0 = found && x0 < item.box.x ? x0 : item.box.x;
        y0 = found && y0 < item.box.y ? y0 : item.box.y;
        x1 = found && x1 > ix1 ? x1 : ix1;
        y1 = found && y1 > iy1 ? y1 : iy1;
        found = 1;
    }
    if (found)
    {
        out->x = x0;
        out->y = y0;
        out->w = x1 - x0;
        out->h = y1 - y0;
    }
    return found;
}

int ItemFoundBox(const DrawnItem *item, ReiPan_MenuBox *out)
{
    if (item == nullptr)
    {
        return 0;
    }
    ReiPan_MenuBox box = item->box;
    if ((box.w <= 0.0f || box.h <= 0.0f) && !GroupBox(item, &box))
    {
        return 0;
    }
    if (out != nullptr)
    {
        *out = box;
    }
    return 1;
}

const DrawnItem *FindItem(int handle, int id, const char *name)
{
    if (g_mm.drawn == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < g_mm.drawn->count; i++)
    {
        const DrawnItem &item = g_mm.drawn->item[i];
        if (item.handle != handle || item.alpha < MENU_MOUSE_ALPHA_MIN)
        {
            continue;
        }
        if (name != nullptr ? strcmp(item.name, name) == 0 : item.id == id)
        {
            return &item;
        }
    }
    return nullptr;
}
}

extern "C" {

int ReiPan_MenuMouseEvent(const SDL_Event *event, int free_pointer, int free_keys)
{
    /* A scripted run is the script's alone. */
    if (event == nullptr || g_mm.script_active)
    {
        return 0;
    }
    switch (event->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        /* Touch is not a pointer, and zero-length motion is SDL putting the
         * cursor back after relative mode, not the player. */
        if (event->motion.which == SDL_TOUCH_MOUSEID)
        {
            return 0;
        }
        g_mm.pointer_x = event->motion.x;
        g_mm.pointer_y = event->motion.y;
        g_mm.pointer_window = event->motion.windowID;
        g_mm.pointer_valid = 1;
        if (event->motion.xrel != 0.0f || event->motion.yrel != 0.0f)
        {
            g_mm.pending_moved = 1;
        }
        return 0;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.which != SDL_TOUCH_MOUSEID &&
            event->button.button == SDL_BUTTON_LEFT)
        {
            g_mm.left_down = 0;
        }
        return 0;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        /* A click still on its way keeps the place it was made. */
        if (g_mm.pending_left == 0)
        {
            g_mm.pointer_valid = 0;
        }
        return 0;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.which == SDL_TOUCH_MOUSEID)
        {
            return 0;
        }
        g_mm.pointer_x = event->button.x;
        g_mm.pointer_y = event->button.y;
        g_mm.pointer_window = event->button.windowID;
        g_mm.pointer_valid = 1;
        if (event->button.button == SDL_BUTTON_LEFT)
        {
            g_mm.left_down = 1;
        }
        if (!free_pointer || ReiPan_MenuMouseClaimed() == 0)
        {
            return 0;
        }
        if (event->button.button == SDL_BUTTON_LEFT)
        {
            g_mm.pending_left++;
        }
        else if (event->button.button == SDL_BUTTON_RIGHT)
        {
            g_mm.pending_right++;
        }
        return 1;
    case SDL_EVENT_MOUSE_WHEEL:
    {
        if (!free_pointer || event->wheel.which == SDL_TOUCH_MOUSEID ||
            ReiPan_MenuMouseClaimed() == 0)
        {
            return 0;
        }
        float x = event->wheel.x;
        float y = event->wheel.y;
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
        {
            x = -x;
            y = -y;
        }
        g_mm.wheel_travel_x += x;
        g_mm.wheel_travel_y += y;
        while (g_mm.wheel_travel_y >= 1.0f)
        {
            g_mm.pending_wheel_y++;
            g_mm.wheel_travel_y -= 1.0f;
        }
        while (g_mm.wheel_travel_y <= -1.0f)
        {
            g_mm.pending_wheel_y--;
            g_mm.wheel_travel_y += 1.0f;
        }
        while (g_mm.wheel_travel_x >= 1.0f)
        {
            g_mm.pending_wheel_x++;
            g_mm.wheel_travel_x -= 1.0f;
        }
        while (g_mm.wheel_travel_x <= -1.0f)
        {
            g_mm.pending_wheel_x--;
            g_mm.wheel_travel_x += 1.0f;
        }
        return 1;
    }
    case SDL_EVENT_KEY_DOWN:
        if (!free_keys || event->key.repeat || ReiPan_MenuMouseClaimed() == 0)
        {
            return 0;
        }
        /* Alt+Enter is the window's. */
        if ((event->key.mod & SDL_KMOD_ALT) != 0)
        {
            return 0;
        }
        if (event->key.scancode == SDL_SCANCODE_RETURN ||
            event->key.scancode == SDL_SCANCODE_KP_ENTER)
        {
            g_mm.pending_enter++;
            return 1;
        }
        if (event->key.scancode == SDL_SCANCODE_BACKSPACE)
        {
            g_mm.pending_back++;
            return 1;
        }
        return 0;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        g_mm.left_down = 0;
        g_mm.queue_len = 0;
        g_mm.active = 0;
        g_mm.pending_left = g_mm.pending_right = 0;
        g_mm.pending_enter = g_mm.pending_back = 0;
        g_mm.pending_wheel_x = g_mm.pending_wheel_y = 0;
        return 0;
    default:
        return 0;
    }
}

void ReiPan_MenuMousePoll(void)
{
    g_mm.reads++;
    if (!g_mm.script_inited)
    {
        ParseScript();
    }
    if (g_mm.script_active)
    {
        RunScript();
    }

    /* What the menu that just ran asked for turns into presses now. */
    if (ReiPan_MenuMouseClaimed() != 0)
    {
        const int flags = g_mm.claim_flags;
        if ((flags & REIPAN_MM_NO_BACK) == 0)
        {
            Queue(REIPAN_MENU_TRIANGLE, g_mm.pending_right + g_mm.pending_back);
        }
        Queue(REIPAN_MENU_CROSS, g_mm.pending_enter);
        QueueWheel(flags, g_mm.pending_wheel_x, g_mm.pending_wheel_y);
    }

    ReiPan_MenuMouse frame;
    memset(&frame, 0, sizeof(frame));
    ReadPointer(&frame);
    frame.moved = g_mm.pending_moved;
    frame.left = g_mm.pending_left > 0;
    frame.right = g_mm.pending_right > 0;
    frame.wheel = g_mm.pending_wheel_y;
    g_mm.frame = frame;
    g_mm.available = 0;

    g_mm.pending_left = g_mm.pending_right = 0;
    g_mm.pending_enter = g_mm.pending_back = 0;
    g_mm.pending_wheel_x = g_mm.pending_wheel_y = 0;
    g_mm.pending_moved = 0;

    g_mm.claimed_prev = g_mm.claimed;
    g_mm.claimed = 0;
    g_mm.tested_prev = g_mm.tested_count - g_mm.tested_poll;
    g_mm.tested_count = 0;

    /* Last frame's drawing is this frame's screen. */
    if (g_mm.drawing == nullptr)
    {
        g_mm.drawing = &g_mm.items_a;
        g_mm.drawn = &g_mm.items_b;
    }
    DrawnItems *finished = g_mm.drawing;
    g_mm.drawing = g_mm.drawn;
    g_mm.drawn = finished;
    g_mm.drawing->count = 0;

    /* What needs no screen of its own: the hints, Yes / No. */
    MenuMousePoll();
    g_mm.tested_poll = g_mm.tested_count;

    /* Down for one read, all up for the next, so presses in a row -- the
     * same button or not -- are each their own edge. */
    if (g_mm.active != 0)
    {
        g_mm.active = 0;
    }
    else if (g_mm.queue_len > 0)
    {
        g_mm.active = g_mm.queue[0] + 1;
        g_mm.queue_len--;
        memmove(g_mm.queue, g_mm.queue + 1, sizeof(int) * (size_t)g_mm.queue_len);
    }
}

int ReiPan_MenuMouseReadCount(void)
{
    return g_mm.reads;
}

int ReiPan_MenuMouseHeld(int index)
{
    return g_mm.active != 0 && g_mm.active - 1 == index ? 1 : 0;
}

int ReiPan_MenuMouseClaimed(void)
{
    return (g_mm.claimed != 0 || g_mm.claimed_prev != 0) ? 1 : 0;
}

int ReiPan_MenuMouseBegin(int flags, ReiPan_MenuMouse *out)
{
    const int free = ReiPan_InputMenuPointerFree() != 0;
    g_mm.available = free && g_mm.frame.inside != 0;
    if (free)
    {
        g_mm.claim_flags = g_mm.claimed != 0 ? (g_mm.claim_flags | flags) : flags;
        g_mm.claimed = 1;
    }
    if (out != nullptr)
    {
        if (g_mm.available)
        {
            *out = g_mm.frame;
        }
        else
        {
            memset(out, 0, sizeof(*out));
        }
    }
    return g_mm.available;
}

int ReiPan_MenuMouseIn(float x, float y, float w, float h)
{
    NoteTested(x, y, w, h);
    return g_mm.available && g_mm.frame.x >= x && g_mm.frame.y >= y &&
           g_mm.frame.x < x + w && g_mm.frame.y < y + h;
}

void ReiPan_MenuMousePress(int button)
{
    Queue(button, 1);
}

int ReiPan_MenuMouseHover(float x, float y, float w, float h)
{
    return ReiPan_MenuMouseIn(x, y, w, h) && g_mm.frame.moved && !Busy();
}

int ReiPan_MenuMouseClick(float x, float y, float w, float h)
{
    if (!ReiPan_MenuMouseIn(x, y, w, h) || !g_mm.frame.left)
    {
        return 0;
    }
    g_mm.frame.left = 0;
    return 1;
}

int ReiPan_MenuMouseClicked(void)
{
    if (!g_mm.available || !g_mm.frame.left)
    {
        return 0;
    }
    g_mm.frame.left = 0;
    return 1;
}

int ReiPan_MenuMouseHasTargets(void)
{
    return (g_mm.tested_prev > 0 || g_mm.tested_count > g_mm.tested_poll) ? 1 : 0;
}

int ReiPan_MenuMouseSeekList(int cursor, float x, float y, float w, float h,
                             float dx, float dy, int count, int dec, int inc,
                             int confirm)
{
    int hit = -1;
    for (int i = 0; i < count; i++)
    {
        if (ReiPan_MenuMouseIn(x + (float)i * dx, y + (float)i * dy, w, h) &&
            hit < 0)
        {
            hit = i;
        }
    }
    if (hit < 0)
    {
        return -1;
    }
    if (g_mm.frame.left)
    {
        g_mm.frame.left = 0;
        SeekTo(cursor, hit, dec, inc);
        if (confirm >= 0)
        {
            Queue(confirm, 1);
        }
    }
    else if (g_mm.frame.moved &&
             hit != cursor + PendingSteps(dec, inc, true))
    {
        SeekTo(cursor, hit, dec, inc);
    }
    return hit;
}

int ReiPan_MenuMouseSeekBox(int cursor, int index, const ReiPan_MenuBox *box,
                            int dec, int inc, int confirm)
{
    if (box == nullptr || !ReiPan_MenuMouseIn(box->x, box->y, box->w, box->h))
    {
        return 0;
    }
    if (g_mm.frame.left)
    {
        g_mm.frame.left = 0;
        SeekTo(cursor, index, dec, inc);
        if (confirm >= 0)
        {
            Queue(confirm, 1);
        }
    }
    else if (g_mm.frame.moved &&
             index != cursor + PendingSteps(dec, inc, true))
    {
        SeekTo(cursor, index, dec, inc);
    }
    return 1;
}

int ReiPan_MenuMouseSeekGrid(int col, int row, float x, float y, float w,
                             float h, float dx, float dy, int cols, int rows,
                             int confirm)
{
    int hit_col = -1;
    int hit_row = -1;
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (ReiPan_MenuMouseIn(x + (float)c * dx, y + (float)r * dy, w, h) &&
                hit_row < 0)
            {
                hit_col = c;
                hit_row = r;
            }
        }
    }
    if (hit_row < 0)
    {
        return -1;
    }
    const int up = REIPAN_MENU_UP;
    const int down = REIPAN_MENU_DOWN;
    const int left = REIPAN_MENU_LEFT;
    const int right = REIPAN_MENU_RIGHT;
    if (g_mm.frame.left)
    {
        g_mm.frame.left = 0;
        SeekTo(row, hit_row, up, down);
        SeekTo(col, hit_col, left, right);
        if (confirm >= 0)
        {
            Queue(confirm, 1);
        }
    }
    else if (g_mm.frame.moved &&
             (hit_row != row + PendingSteps(up, down, true) ||
              hit_col != col + PendingSteps(left, right, true)))
    {
        SeekTo(row, hit_row, up, down);
        SeekTo(col, hit_col, left, right);
    }
    return hit_row * cols + hit_col;
}

int ReiPan_MenuMouseButton(float x, float y, float w, float h, int button)
{
    if (!ReiPan_MenuMouseIn(x, y, w, h))
    {
        return 0;
    }
    if (g_mm.frame.left)
    {
        g_mm.frame.left = 0;
        ReiPan_MenuMousePress(button);
    }
    return 1;
}

void ReiPan_MenuMouseConfirm(void)
{
    if (ReiPan_MenuMouseBegin(0, nullptr) != 0 && ReiPan_MenuMouseClicked() != 0)
    {
        ReiPan_MenuMousePress(REIPAN_MENU_CROSS);
    }
}

void ReiPan_MenuMouseBack(void)
{
    if (ReiPan_MenuMouseBegin(0, nullptr) != 0 && ReiPan_MenuMouseClicked() != 0)
    {
        ReiPan_MenuMousePress(REIPAN_MENU_TRIANGLE);
    }
}

void ReiPan_MenuMouseStart(void)
{
    if (ReiPan_MenuMouseBegin(0, nullptr) != 0 && ReiPan_MenuMouseClicked() != 0)
    {
        ReiPan_MenuMousePress(REIPAN_MENU_START);
    }
}

void ReiPan_MenuMouseNoteItem(int handle, int id, int parent, const char *name,
                              float x, float y, float w, float h, int alpha)
{
    DrawnItems *items = g_mm.drawing;
    if (items == nullptr || items->count >= MENU_MOUSE_ITEM_MAX)
    {
        return;
    }
    DrawnItem &item = items->item[items->count++];
    item.handle = handle;
    item.id = id;
    item.parent = parent;
    item.alpha = alpha;
    item.box.x = x;
    item.box.y = y;
    item.box.w = w;
    item.box.h = h;
    item.name[0] = '\0';
    if (name != nullptr)
    {
        strncpy(item.name, name, MENU_MOUSE_NAME_MAX - 1);
        item.name[MENU_MOUSE_NAME_MAX - 1] = '\0';
    }
}

int ReiPan_MenuMouseItemBox(int handle, int id, ReiPan_MenuBox *out)
{
    return ItemFoundBox(FindItem(handle, id, nullptr), out);
}

int ReiPan_MenuMouseNameBox(int handle, const char *name, ReiPan_MenuBox *out)
{
    return ItemFoundBox(name != nullptr ? FindItem(handle, 0, name) : nullptr, out);
}

int ReiPan_MenuMouseDrawnItem(int n, int *handle, int *id, const char **name,
                              ReiPan_MenuBox *box, int *alpha)
{
    if (g_mm.drawn == nullptr || n < 0 || n >= g_mm.drawn->count)
    {
        return 0;
    }
    const DrawnItem &item = g_mm.drawn->item[n];
    if (handle != nullptr)
    {
        *handle = item.handle;
    }
    if (id != nullptr)
    {
        *id = item.id;
    }
    if (name != nullptr)
    {
        *name = item.name;
    }
    if (box != nullptr)
    {
        *box = item.box;
    }
    if (alpha != nullptr)
    {
        *alpha = item.alpha;
    }
    return 1;
}

void ReiPan_MenuMouseDumpFrame(const char *path)
{
    FILE *file = fopen(path, "w");
    if (file == nullptr)
    {
        return;
    }
    fprintf(file, "read %d claimed %d flags 0x%x pointer %.1f %.1f inside %d\n",
            g_mm.script_reads, ReiPan_MenuMouseClaimed(), g_mm.claim_flags,
            g_mm.frame.x, g_mm.frame.y, g_mm.frame.inside);
    for (int i = 0; i < g_mm.tested_count; i++)
    {
        const ReiPan_MenuBox &box = g_mm.tested[i];
        fprintf(file, "target %.1f %.1f %.1f %.1f\n", box.x, box.y, box.w, box.h);
    }
    for (int i = 0; g_mm.drawn != nullptr && i < g_mm.drawn->count; i++)
    {
        const DrawnItem &item = g_mm.drawn->item[i];
        fprintf(file, "item %d 0x%x a=%d %.0f %.0f %.0f %.0f \"%s\"\n", item.handle,
                item.id, item.alpha, item.box.x, item.box.y, item.box.w, item.box.h,
                item.name);
    }
    fclose(file);
}
}
