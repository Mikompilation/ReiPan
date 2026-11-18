#include "reipan_save_region.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
namespace ReiPanSave
{
namespace
{
struct Resource { uint32_t id[3]; };
struct CursorMap { uint16_t event; uint8_t table; uint32_t eu, other, eu_wait, other_wait; };
struct Condition { uint16_t event; uint8_t table, label; uint32_t offset; };
struct NpcSpan { uint32_t eu, jp, size; };
#include "reipan_save_region_data.inc"
using Blocks = std::array<Bytes, 60>;
unsigned Index(SaveRegion r)
{
    unsigned n = unsigned(r);
    if (n > 2) throw std::runtime_error("Unknown save region.");
    return n;
}
uint32_t Get(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void Put(uint8_t *p, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(n >> (i * 8));
}
int16_t Short(const uint8_t *p) { return int16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8); }
void PutShort(uint8_t *p, int n) { p[0] = uint8_t(n); p[1] = uint8_t(n >> 8); }
Blocks Split(const Bytes &data, SaveRegion region)
{
    const size_t *sizes[] = {euSizes, usSizes, jpSizes};
    Blocks blocks;
    size_t at = 0;
    for (unsigned b = 0; b < blocks.size(); ++b)
    {
        size_t n = sizes[Index(region)][b];
        if (at > data.size() || n > data.size() - at) throw std::runtime_error("Truncated regional save.");
        blocks[b] = Bytes(data.begin() + at, data.begin() + at + n);
        at += n;
    }
    if (at + 4 != data.size()) throw std::runtime_error("Invalid regional save layout.");
    return blocks;
}
Bytes Join(const Blocks &blocks)
{
    Bytes result;
    for (const auto &b : blocks) result.insert(result.end(), b.begin(), b.end());
    result.resize(result.size() + 4);
    Seal(result);
    return result;
}
int CanonicalEvent(int event, SaveRegion region)
{
    if (event < -1 || event >= (region == SaveRegion::JP ? 1780 : 1781))
        throw std::runtime_error("Invalid regional event ID.");
    return region == SaveRegion::JP && event >= 87 ? event + 1 : event;
}
int TargetEvent(int event, SaveRegion region)
{
    if (event < -1 || event >= 1781) throw std::runtime_error("Invalid event ID.");
    if (region != SaveRegion::JP || event < 87) return event;
    return event == 87 ? -1 : event - 1;
}
uint32_t File(uint32_t id, SaveRegion source, SaveRegion target)
{
    if (id == 0xffffffffu || source == target) return id;
    for (const auto &row : resources)
        if (row.id[Index(source)] == id) return row.id[Index(target)];
    throw std::runtime_error("A saved audio resource has no verified regional equivalent.");
}
uint32_t Object(uint32_t id, SaveRegion source, SaveRegion target)
{
    if (source == SaveRegion::JP && target != SaveRegion::JP && id >= 55006 && id <= 55010) return id + 1;
    if (target == SaveRegion::JP && source != SaveRegion::JP)
    {
        if (id == 55006) throw std::runtime_error("An active target references an object absent in Japan. Save after the scene finishes.");
        if (id >= 55007 && id <= 55011) return id - 1;
    }
    return id;
}
template<size_t N>
uint32_t Cursor(uint32_t pc, int event, unsigned table, uint32_t &wait,
                const CursorMap (&rows)[N], bool to_eu)
{
    const auto first = std::lower_bound(std::begin(rows), std::end(rows), std::pair<int, unsigned>{event, table},
        [](const CursorMap &row, const auto &key) { return std::pair<int, unsigned>{row.event, row.table} < key; });
    uint32_t result = 0, next_wait = wait;
    bool found = false;
    for (auto it = first; it != std::end(rows) && it->event == event && it->table == table; ++it)
    {
        if ((to_eu ? it->other : it->eu) != pc) continue;
        uint32_t candidate = to_eu ? it->eu : it->other;
        if (found && result != candidate) throw std::runtime_error("Ambiguous regional event position.");
        const auto from = to_eu ? it->other_wait : it->eu_wait;
        const auto to = to_eu ? it->eu_wait : it->other_wait;
        if (from && wait)
        {
            if (wait > from || !to) throw std::runtime_error("Invalid active event countdown.");
            next_wait = uint32_t((uint64_t(wait) * to + from / 2) / from);
        }
        found = true;
        result = candidate;
    }
    if (!found) throw std::runtime_error("This active script position differs between regions. Save after the scene finishes.");
    wait = next_wait;
    return result;
}
uint32_t MapCursor(uint32_t pc, int event, unsigned table, uint32_t &wait, SaveRegion source, SaveRegion target)
{
    if (source == target) return pc;
    if (source == SaveRegion::US) pc = Cursor(pc, event, table, wait, usCursors, true);
    if (source == SaveRegion::JP) pc = Cursor(pc, event, table, wait, jpCursors, true);
    if (target == SaveRegion::US) pc = Cursor(pc, event, table, wait, usCursors, false);
    if (target == SaveRegion::JP) pc = Cursor(pc, event, table, wait, jpCursors, false);
    return pc;
}
std::vector<Condition> Conditions(int event, unsigned table, SaveRegion region)
{
    const Condition *begin[] = {std::begin(euConditions), std::begin(usConditions), std::begin(jpConditions)};
    const Condition *end[] = {std::end(euConditions), std::end(usConditions), std::end(jpConditions)};
    std::vector<Condition> result;
    const auto i = Index(region);
    auto at = std::lower_bound(begin[i], end[i], std::pair<int, unsigned>{event, table},
        [](const Condition &row, const auto &key) { return std::pair<int, unsigned>{row.event, row.table} < key; });
    for (; at != end[i] && at->event == event && at->table == table; ++at) result.push_back(*at);
    return result;
}
uint32_t NpcCursor(uint32_t pc, SaveRegion source, SaveRegion target, bool instruction)
{
    if ((source == SaveRegion::JP) == (target == SaveRegion::JP)) return pc;
    bool found = false;
    uint32_t result = 0;
    for (const auto &row : npcSpans)
    {
        const auto from = source == SaveRegion::JP ? row.jp : row.eu;
        const auto to = target == SaveRegion::JP ? row.jp : row.eu;
        if (pc < from || pc - from > row.size || (instruction && pc != from)) continue;
        const auto candidate = to + pc - from;
        if (found && result != candidate) throw std::runtime_error("Ambiguous regional NPC script position.");
        found = true;
        result = candidate;
    }
    if (!found) throw std::runtime_error("This active NPC action differs in Japan. Save after the conversation finishes.");
    return result;
}
void Adapt(Blocks &blocks, SaveRegion source, SaveRegion target)
{
    if (source == target) return;
    // Canonical event states let every saved reference use the same identities.
    if (source == SaveRegion::JP)
    {
        auto &state = blocks[11];
        state.insert(state.begin() + 8 + 87 * 4, 4, 0);
        // Event 150 temporarily changes flag 85 without its normal flag-87 counterpart.
        const auto interaction = state[8 + 150 * 4];
        if (interaction != 0 && interaction != 5)
            throw std::runtime_error("An active Japanese interaction cannot be reconstructed safely. Save after it finishes.");
        const auto flag = state[8 + 85 * 4];
        if (flag != 0 && flag != 5) throw std::runtime_error("The regional interaction flag is in an unsupported state.");
        state[8 + 87 * 4] = flag == 5 ? 0 : 5;
    }
    if (target == SaveRegion::JP)
    {
        const auto interaction = blocks[11][8 + 150 * 4];
        if (interaction != 0 && interaction != 5)
            throw std::runtime_error("An active interaction differs in Japan. Save after it finishes.");
    }
    const auto &states = blocks[11];
    for (unsigned i = 0; i < 30; ++i)
    {
        auto *p = blocks[5].data() + i * 12;
        int changed = int32_t(Get(p + 4));
        if (changed < 0) continue;
        int owner = TargetEvent(CanonicalEvent(int32_t(Get(p)), source), target);
        changed = TargetEvent(CanonicalEvent(changed, source), target);
        if (owner < 0 && changed >= 0) throw std::runtime_error("An active state change has no regional equivalent.");
        Put(p, uint32_t(owner)); Put(p + 4, uint32_t(changed));
    }
    for (unsigned i = 0; i < 150; ++i)
    {
        auto *p = blocks[7].data() + i * 24;
        int event = CanonicalEvent(int32_t(Get(p)), source);
        if (event < 0) continue;
        int next = TargetEvent(event, target);
        if (next < 0) throw std::runtime_error("An active event is absent in the target region.");
        const unsigned state = states[8 + event * 4];
        const unsigned table = state == 1 ? 1 : state == 5 ? 4 : 2;
        auto wait = Get(p + 4);
        Put(p + 8, MapCursor(Get(p + 8), event, table, wait, source, target));
        Put(p + 4, wait); Put(p, uint32_t(next)); Put(p + 12, 0);
    }
    std::map<int, std::set<uint32_t>> posted;
    for (unsigned i = 0; i < 450; ++i)
    {
        const auto *p = blocks[8].data() + i * 8;
        int event = CanonicalEvent(Short(p + 2), source);
        if (event < 0) continue;
        unsigned table = states[8 + event * 4] == 3 ? 3 : 0;
        const auto expected = Conditions(event, table, source);
        if (!std::any_of(expected.begin(), expected.end(), [&](const Condition &c) { return c.offset == Get(p + 4) && c.label == Short(p); }))
            throw std::runtime_error("A saved condition does not match its original regional script.");
        // Original saves can post the same group twice; either copy tests the same condition.
        posted[event].insert(Get(p + 4));
    }
    Bytes conditions(3600);
    for (unsigned i = 0; i < 450; ++i) PutShort(conditions.data() + i * 8 + 2, -1);
    unsigned used = 0;
    for (const auto &[event, groups] : posted)
    {
        const unsigned table = states[8 + event * 4] == 3 ? 3 : 0;
        if (groups.size() != Conditions(event, table, source).size())
            throw std::runtime_error("Incomplete saved condition groups.");
        const int next = TargetEvent(event, target);
        if (next < 0) continue;
        auto rows = Conditions(event, table, target);
        if (rows.empty()) throw std::runtime_error("This active condition changes event flow between regions. Save after the scene finishes.");
        for (const auto &row : rows)
        {
            if (used >= 450) throw std::runtime_error("Regional condition table capacity exceeded.");
            auto *p = conditions.data() + used++ * 8;
            PutShort(p, row.label); PutShort(p + 2, next); Put(p + 4, row.offset);
        }
    }
    blocks[8] = std::move(conditions);
    int current = int32_t(Get(blocks[11].data() + 4));
    if (current >= 0)
    {
        int canonical = CanonicalEvent(current, source);
        Put(blocks[11].data() + 4, uint32_t(canonical == 87 && target == SaveRegion::JP ? 87 : TargetEvent(canonical, target)));
    }
    for (unsigned i = 0; i < 16; ++i)
    {
        auto *p = blocks[26].data() + i * 4;
        if (Short(p) == 1)
        {
            int id = TargetEvent(CanonicalEvent(Short(p + 2), source), target);
            if (id < 0) throw std::runtime_error("A pending regional sleep event cannot be converted safely.");
            PutShort(p + 2, id);
        }
    }
    auto count = Get(blocks[50].data());
    if (count > 450) throw std::runtime_error("Invalid saved event priority count.");
    unsigned kept = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        auto *p = blocks[50].data() + 4 + i * 8;
        int event = TargetEvent(CanonicalEvent(int32_t(Get(p)), source), target);
        if (event < 0) continue;
        Put(p, uint32_t(event));
        std::memmove(blocks[50].data() + 4 + kept++ * 8, p, 8);
    }
    Put(blocks[50].data(), kept);
    for (unsigned i = kept; i < 450; ++i)
    {
        auto *p = blocks[50].data() + 4 + i * 8;
        std::memset(p, 0, 8); Put(p, 0xffffffffu);
    }
    for (unsigned i = 0; i < 300; ++i)
    {
        auto *p = blocks[9].data() + i * 8;
        Put(p, File(Get(p), source, target));
    }
    for (unsigned i = 0; i < 125; ++i)
    {
        auto *p = blocks[13].data() + i * 4;
        Put(p, File(Get(p), source, target));
    }
    for (unsigned i = 0; i < 2; ++i)
    {
        auto *p = blocks[12].data() + i * 48;
        if (!p[0]) continue;
        Put(p + 8, File(Get(p + 8), source, target));
        if (Get(p + 16) == 8 || Get(p + 16) == 9) Put(p + 20, Object(Get(p + 20), source, target));
    }
    if (source == SaveRegion::JP || target == SaveRegion::JP)
    {
        const uint32_t source_base = source == SaveRegion::JP ? 0x12d7000u : 0x12f7000u;
        const uint32_t target_base = target == SaveRegion::JP ? 0x12d7000u : 0x12f7000u;
        for (unsigned i = 0; i < 2; ++i)
        {
            auto *p = blocks[54].data() + 16 + i * 336;
            if (!Get(p + 4)) continue;
            if (Get(p) != source_base) throw std::runtime_error("Invalid saved NPC script base.");
            Put(p, target_base);
            Put(p + 4, NpcCursor(Get(p + 4), source, target, false));
            for (unsigned at : {8u, 16u})
                if (Get(p + at))
                {
                    auto value = Get(p + at);
                    if (value < source_base) throw std::runtime_error("Invalid saved NPC script address.");
                    Put(p + at, target_base + NpcCursor(value - source_base, source, target, at == 16 && p[0x15] != 0));
                }
            auto jump = Short(p + 0x1c);
            if (jump > 0) PutShort(p + 0x1c, int(NpcCursor(uint16_t(jump), source, target, true)));
        }
    }
    if ((source == SaveRegion::EU) != (target == SaveRegion::EU))
    {
        auto &bonus = blocks[20];
        if (target == SaveRegion::EU) bonus.erase(bonus.begin() + 26 * 4, bonus.begin() + 27 * 4);
        else bonus.insert(bonus.begin() + 26 * 4, 4, 0);
    }
    for (unsigned i = 0; i < blocks[20].size(); i += 4)
        if (Get(blocks[20].data() + i) > 2) throw std::runtime_error("Invalid bonus-shop unlock state.");
    auto player = blocks[27][0];
    if (player >= 3) throw std::runtime_error("Invalid saved player.");
    Put(blocks[27].data() + 0x2c, playerBases[Index(target)] + player * 0x350);
    if (target == SaveRegion::JP) blocks[11].erase(blocks[11].begin() + 8 + 87 * 4, blocks[11].begin() + 8 + 88 * 4);
}
void Timing(Bytes &native, unsigned from, unsigned to)
{
    if ((from != 25 && from != 30) || (to != 25 && to != 30)) throw std::runtime_error("Choose 50 Hz PAL or 60 Hz timing.");
    if (from == to) return;
    for (unsigned mission : timedMissions)
    {
        auto *p = native.data() + BlockOffset(25, true) + mission * 8 + 4;
        int value = int32_t(Get(p));
        if (value == -1) continue;
        if (value < 0) throw std::runtime_error("Invalid mission-record time.");
        const auto scaled = (uint64_t(value) * to + from / 2) / from;
        if (scaled > 0x7fffffffu) throw std::runtime_error("Invalid mission-record time.");
        Put(p, uint32_t(scaled));
    }
    Seal(native);
}
} // namespace
const char *RegionFolder(SaveRegion region)
{
    const char *names[] = {"BESLES-53825ZERO-BD", "BASLUS-21244ZERO-BD", "BISLPS-25544ZERO-BD"};
    return names[Index(region)];
}
const char *RegionLabel(SaveRegion region)
{
    const char *names[] = {"Europe", "USA", "Japan"};
    return names[Index(region)];
}
size_t RegionalSize(SaveRegion region)
{
    Index(region);
    return region == SaveRegion::US ? 63474 : 63470;
}
bool ImportRegion(const Bytes &data, SaveRegion region, Bytes &native, std::string &error, unsigned eu_hz)
{
    try
    {
        Index(region);
        if (data.size() == NativeSize() && region == SaveRegion::EU) return ToNative(data, native, error);
        if (data.size() != RegionalSize(region) || !Checksum(data) || Empty(data)) throw std::runtime_error("Invalid regional slot size or checksum.");
        auto blocks = Split(data, region);
        Adapt(blocks, region, SaveRegion::EU);
        Bytes result;
        if (!ToNative(Join(blocks), result, error)) return false;
        Timing(result, region == SaveRegion::EU ? eu_hz : 30, 30);
        native = std::move(result); error.clear(); return true;
    }
    catch (const std::exception &e) { error = e.what(); return false; }
}
bool ExportRegion(const Bytes &native, SaveRegion region, Bytes &data, std::string &error, unsigned eu_hz)
{
    try
    {
        Index(region);
        Bytes canonical;
        if (!ToNative(native, canonical, error)) return false;
        Timing(canonical, 30, region == SaveRegion::EU ? eu_hz : 30);
        Bytes ps2;
        if (!ToPs2(canonical, ps2, error)) return false;
        auto blocks = Split(ps2, SaveRegion::EU);
        Adapt(blocks, SaveRegion::EU, region);
        data = Join(blocks); error.clear(); return true;
    }
    catch (const std::exception &e) { error = e.what(); return false; }
}
std::string RegionNotice(SaveRegion source, SaveRegion target)
{
    if (source == target) return {};
    std::string note;
    if (source == SaveRegion::EU || target == SaveRegion::EU)
        note = "US/Japan have an extra Trailer bonus-shop unlock absent from Europe/ReiPan. Its separate state cannot be retained; exports leave it unobtained.";
    if (source == SaveRegion::JP || target == SaveRegion::JP)
        note += " Regional interactions follow the target game; unsupported active scripts/conversations cannot be transferred.";
    return note;
}
} // namespace ReiPanSave
