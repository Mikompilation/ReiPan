#include "reipan_save_codec.h"
#include "ingame/cat/cat_ctrl.h"
#include "ingame/event/prg/ev_change.h"
#include "ingame/event/prg/ev_exe.h"
#include "ingame/event/prg/ev_open.h"
#include "ingame/ingame_wrk.h"
#include "ingame/map/MapLBuff.h"
#include "ingame/photo/n_plyr_camera.h"
#include "ingame/photo/photo_obj.h"
#include "ingame/plyr/player.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace ReiPanSave
{
namespace
{
constexpr std::array<size_t, 60> sizes = {
    414, 66,  8,    164,   80,   360,  5040, 3600, 3600, 2400, 484, 7136, 96,  508,  8,
    6,   8,   20,   16,    16,   132,  434,  16,   16,   44,   448, 64,   76,  2544, 24,
    320, 780, 224,  462,   8,    112,  1,    1,    8,    12,   11,  28,   12,  480,  20,
    8,   12,  2560, 12288, 1028, 3604, 9728, 768,  2048, 688,  192, 7,    200, 16,   12};
size_t Size(unsigned i, bool native)
{
    if (!native)
        return sizes[i];
    switch (i)
    {
    case 7:
        return sizeof(EV_EXE_CTRL) * 150;
    case 8:
        return sizeof(EV_CTRL_CENTER) * 450;
    case 27:
        return sizeof(INGAME_WRK);
    case 28:
        return sizeof(PLYR_WRK) * 3;
    case 30:
        return sizeof(PHOTO_OBJ_SLOT) * 4;
    case 51:
        return sizeof(MAP_LBUFF) * 32;
    case 54:
        return sizeof(PLYR_CAMERA_SAVE);
    case 55:
        return sizeof(CAT_WRK);
    default:
        return sizes[i];
    }
}
uint64_t Get(const uint8_t *p, size_t n)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i)
        v |= uint64_t(p[i]) << (i * 8);
    return v;
}
void Put(uint8_t *p, uint64_t v, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        p[i] = uint8_t(v >> (i * 8));
}
int32_t I32(const uint8_t *p) { return int32_t(uint32_t(Get(p, 4))); }
int16_t I16(const uint8_t *p) { return int16_t(uint16_t(Get(p, 2))); }
bool Fail(std::string &error, const char *message)
{
    error = message;
    return false;
}
// Spans describe meaningful fields, not the host's extra alignment padding.
struct Span
{
    size_t ee, host, length;
};
void Copy(uint8_t *out, const uint8_t *in, bool to_native, std::initializer_list<Span> spans)
{
    for (const auto &s : spans)
        std::memcpy(out + (to_native ? s.host : s.ee), in + (to_native ? s.ee : s.host), s.length);
}
void Cursor(uint8_t *out, const uint8_t *in, bool to_native, size_t ee, size_t host)
{
    Put(out + (to_native ? host : ee),
        Get(in + (to_native ? ee : host), to_native ? 4 : sizeof(void *)),
        to_native ? sizeof(void *) : 4);
}
void Record(unsigned block, uint8_t *out, const uint8_t *in, bool to_native)
{
    switch (block)
    {
    case 7:
        Copy(out, in, to_native, {{0, 0, 8}, {16, offsetof(EV_EXE_CTRL, work), 8}});
        if (I32(in) >= 0)
            Cursor(out, in, to_native, 8, offsetof(EV_EXE_CTRL, event_addr));
        break;
    case 8:
        Copy(out, in, to_native, {{0, 0, 4}});
        if (I16(in + 2) >= 0)
            Cursor(out, in, to_native, 4, offsetof(EV_CTRL_CENTER, exe_addr));
        break;
    case 27:
        Copy(out, in, to_native, {{0, 0, 0x2c}, {0x30, offsetof(INGAME_WRK, lock_tbl), 0x1c}});
        if (!to_native)
            Put(out + 0x2c, 0x32f4a0u + out[0] * 0x350u, 4);
        break;
    case 28:
        Copy(out, in, to_native,
             {{0, 0, 0x26c},
              {0x280, offsetof(PLYR_WRK, req280), 0x60},
              {0x2e4, offsetof(PLYR_WRK, _2e4), 0x60},
              {0x348, offsetof(PLYR_WRK, _348), 8}});
        break;
    case 30:
        // Room loading recreates glow slots from the separate persistent object flags.
        Put(out, 0xffffffffu, 4);
        break;
    case 51:
        Copy(out, in, to_native, {{0, 0, 0x120}, {0x124, offsetof(MAP_LBUFF, _124), 12}});
        break;
    case 54:
        Copy(out, in, to_native, {{0, 0, 0x5c}, {0x70, offsetof(PLYR_CAMERA_WRK, move), 0xe0}});
        break;
    case 55:
        Copy(out, in, to_native, {{0, 0, 0x34}, {0x40, offsetof(CAT_WRK, pos), 0x80}});
        break;
    }
}
bool Convert(const Bytes &in, Bytes &out, bool to_native, std::string &error)
{
    if (!Validate(in, error))
        return false;
    const bool input_native = in.size() == NativeSize();
    if (input_native == to_native)
    {
        out = in;
        return true;
    }
    Bytes result(to_native ? NativeSize() : Ps2Size, 0);
    for (unsigned b = 0; b < sizes.size(); ++b)
    {
        const auto *src = in.data() + BlockOffset(b, input_native);
        auto *dst = result.data() + BlockOffset(b, to_native);
        unsigned count = 1;
        size_t ee = Size(b, false), host = Size(b, true);
        switch (b)
        {
        case 7:
            count = 150;
            break;
        case 8:
            count = 450;
            break;
        case 28:
            count = 3;
            break;
        case 30:
            count = 4;
            break;
        case 51:
            count = 32;
            break;
        case 54:
            std::memcpy(dst, src, 0x10);
            src += 0x10;
            dst += 0x10;
            ee -= 0x10;
            host -= 0x10;
            count = 2;
            break;
        }
        if (Size(b, false) == Size(b, true))
        {
            std::memcpy(dst, src, Size(b, false));
            continue;
        }
        for (unsigned i = 0; i < count; ++i)
            Record(b, dst + i * (to_native ? host : ee) / count,
                   src + i * (to_native ? ee : host) / count, to_native);
    }
    Seal(result);
    out = std::move(result);
    return true;
}
} // namespace
size_t NativeSize()
{
    size_t n = 4;
    for (unsigned i = 0; i < sizes.size(); ++i)
        n += Size(i, true);
    return n;
}
size_t BlockOffset(unsigned block, bool native)
{
    size_t n = 0;
    for (unsigned i = 0; i < block; ++i)
        n += Size(i, native);
    return n;
}
bool Empty(const Bytes &d)
{
    return d.size() >= 4 && Get(d.data() + d.size() - 4, 4) == 0xffffffffu &&
           std::all_of(d.begin(), d.end() - 4, [](uint8_t b) { return b == 0; });
}
bool Checksum(const Bytes &d)
{
    if (d.size() < 4)
        return false;
    uint32_t sum = 0;
    for (size_t i = 0; i < d.size() - 4; ++i)
        sum += int8_t(d[i]);
    return sum == Get(d.data() + d.size() - 4, 4);
}
void Seal(Bytes &d)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < d.size() - 4; ++i)
        sum += int8_t(d[i]);
    Put(d.data() + d.size() - 4, sum, 4);
}
Bytes EmptySlot(size_t size)
{
    Bytes d(size, 0);
    Put(d.data() + size - 4, 0xffffffffu, 4);
    return d;
}
bool Validate(const Bytes &d, std::string &error)
{
    if (sizeof(void *) != 8)
        return Fail(error, "Save conversion currently requires a 64-bit build.");
    if (d.size() != Ps2Size && d.size() != NativeSize())
        return Fail(error, "Unknown gameplay save size/version.");
    if (!Checksum(d) || Empty(d))
        return Fail(error, "Invalid checksum or empty gameplay slot.");
    const bool native = d.size() == NativeSize();
    const auto *game = d.data() + BlockOffset(27, native);
    if (game[0] >= 3 || game[1] > 15 || game[3] > 1 || I32(game + 0x20) < 0 || I32(game + 0x20) > 3)
        return Fail(error, "Invalid player, chapter, world or difficulty.");
    const auto *area = d.data() + BlockOffset(34, native);
    if (I32(area) < 1 || I32(area) > 52 || I32(area + 4) < 0 || I32(area + 4) > 1)
        return Fail(error, "Invalid saved room or side.");
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto *p =
            d.data() + BlockOffset(28, native) + i * (native ? sizeof(PLYR_WRK) : 0x350);
        if (p[offsetof(PLYR_WRK, mode)] == 5)
            return Fail(error, "Mid-ladder saves are not supported; save at a normal save point.");
        for (size_t at : {offsetof(PLYR_WRK, pos), offsetof(PLYR_WRK, rot)})
            for (unsigned c = 0; c < 4; ++c)
            {
                float f;
                std::memcpy(&f, p + at + c * 4, 4);
                if (!std::isfinite(f))
                    return Fail(error, "Invalid saved player transform.");
            }
    }
    for (unsigned block : {7u, 8u})
    {
        const auto *p = d.data() + BlockOffset(block, native);
        const size_t stride = Size(block, native) / (block == 7 ? 150 : 450);
        for (unsigned i = 0; i < (block == 7 ? 150u : 450u); ++i, p += stride)
        {
            int event = block == 7 ? I32(p) : I16(p + 2);
            if (event < -1 || event >= 1781)
                return Fail(error, "Invalid saved event ID.");
            if (event < 0)
                continue;
            if (block == 8 && (I16(p) < 0 || I16(p) >= 72))
                return Fail(error, "Invalid saved condition ID.");
            const size_t at = block == 7 ? 8 : (native ? offsetof(EV_CTRL_CENTER, exe_addr) : 4);
            const auto offset = Get(p + at, native ? sizeof(void *) : 4);
            if (offset >= 0x4b000 || offset % 4)
                return Fail(error, "Invalid saved event cursor.");
        }
    }
    return true;
}
bool ToNative(const Bytes &ps2, Bytes &native, std::string &error)
{
    return Convert(ps2, native, true, error);
}
bool ToPs2(const Bytes &native, Bytes &ps2, std::string &error)
{
    return Convert(native, ps2, false, error);
}
bool PrepareLoaded(Bytes &native, const uint8_t *events, size_t size, std::string &error)
{
    if (!Validate(native, error) || native.size() != NativeSize())
        return false;
    if (size < 28 || size > 0x4b000)
        return Fail(error, "Event data is not ready for imported save restoration.");
    std::vector<uint32_t> starts;
    std::array<uint32_t, 7> tables{};
    for (unsigned t = 0; t < 7; ++t)
    {
        tables[t] = uint32_t(Get(events + t * 4, 4));
        if (tables[t] > size || size - tables[t] < 1781 * 4)
            return Fail(error, "Invalid event asset tables.");
        for (unsigned e = 0; e < 1781; ++e)
        {
            auto a = uint32_t(Get(events + tables[t] + e * 4, 4));
            if (a < size)
                starts.push_back(a);
        }
    }
    starts.push_back(uint32_t(size));
    std::sort(starts.begin(), starts.end());
    const auto *states = native.data() + BlockOffset(11, true);
    for (unsigned block : {7u, 8u})
    {
        const auto *p = native.data() + BlockOffset(block, true);
        const size_t stride = Size(block, true) / (block == 7 ? 150 : 450);
        for (unsigned i = 0; i < (block == 7 ? 150u : 450u); ++i, p += stride)
        {
            int event = block == 7 ? I32(p) : I16(p + 2);
            if (event < 0)
                continue;
            auto state = states[offsetof(EV_WRK, ev_state) + event * sizeof(EVENT_STATE) +
                                offsetof(EVENT_STATE, state)];
            unsigned table = block == 7 ? (state == 1   ? 1
                                           : state == 5 ? 4
                                                        : 2)
                                        : (state == 3 ? 3 : 0);
            auto start = uint32_t(Get(events + tables[table] + event * 4, 4));
            auto end = std::upper_bound(starts.begin(), starts.end(), start);
            const size_t at =
                block == 7 ? offsetof(EV_EXE_CTRL, event_addr) : offsetof(EV_CTRL_CENTER, exe_addr);
            auto cursor = Get(p + at, sizeof(void *));
            if (start >= size || end == starts.end() || cursor >= *end - start)
                return Fail(error, "Saved event cursor does not match the EU event assets.");
        }
    }
    auto *game = native.data() + BlockOffset(27, true);
    PLYR_WRK *player = &plyr_wrk[game[0]];
    std::memcpy(game + offsetof(INGAME_WRK, plyr_wrk), &player, sizeof(player));
    Seal(native);
    return true;
}
} // namespace ReiPanSave
