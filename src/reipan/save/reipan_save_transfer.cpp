#include "reipan_save_transfer.h"
#include "reipan/io/reipan_paths.h"
#include "reipan_memory_card.h"
#include "reipan_save_paths.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace ReiPanSave
{
namespace
{
namespace fs = std::filesystem;
constexpr const char *Icon = "zero-b-game.ico";
std::string Name(int slot) { return "Zero3Play" + std::to_string(slot); }
uint32_t U32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
std::string Unique()
{
    static std::atomic<unsigned> n{0};
    return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
           std::to_string(n++);
}
Bytes Read(const fs::path &p, size_t limit = 1024 * 1024)
{
    if (fs::is_symlink(p) || !fs::is_regular_file(p))
        throw std::runtime_error("Missing or unsafe save file: " + PathUtf8(p.filename()));
    auto size = fs::file_size(p);
    if (size > limit)
        throw std::runtime_error("Save file is too large: " + PathUtf8(p.filename()));
    std::ifstream f(p, std::ios::binary);
    Bytes data(size);
    if (!f || !f.read(reinterpret_cast<char *>(data.data()), size))
        throw std::runtime_error("Cannot read save file: " + PathUtf8(p.filename()));
    return data;
}
void Write(const fs::path &p, const Bytes &bytes, bool exclusive = false)
{
#ifdef _WIN32
    FILE *f = _wfopen(p.c_str(), exclusive ? L"wbx" : L"wb");
#else
    FILE *f = std::fopen(p.c_str(), exclusive ? "wbx" : "wb");
#endif
    if (!f)
        throw std::runtime_error("Cannot create staged file: " + PathUtf8(p.filename()));
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size() && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok)
        ok = _commit(_fileno(f)) == 0;
#else
    if (ok)
        ok = fsync(fileno(f)) == 0;
#endif
    if (std::fclose(f) != 0)
        ok = false;
    if (!ok)
    {
        if (exclusive)
        {
            std::error_code ec;
            fs::remove(p, ec);
        }
        throw std::runtime_error("Writing save data failed; original files were retained.");
    }
}
void SyncDirectory(const fs::path &path)
{
#ifndef _WIN32
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        throw std::runtime_error("Cannot sync save directory.");
    const int result = fsync(fd);
    const int error = errno;
    close(fd);
    // Some removable/network filesystems cannot flush directory handles.
    if (result != 0 && error != EINVAL && error != ENOTSUP)
        throw std::runtime_error("Cannot sync save directory.");
#endif
}
Folder Fresh(const Folder &source, bool ps2, SaveRegion region)
{
    Folder out;
    out.region = region;
    for (auto name : {"Zero3System", "icon.sys", Icon})
        out.files[name] = source.files.at(name);
    out.files[RegionFolder(region)] = Bytes(HeadSize, 0);
    Seal(out.files[RegionFolder(region)]);
    for (int i = 0; i < 5; ++i)
        out.files[Name(i)] = EmptySlot(ps2 ? RegionalSize(region) : NativeSize());
    return out;
}
void WriteFolder(const fs::path &path, const Folder &folder)
{
    fs::create_directories(path);
    for (const auto &f : folder.files)
        Write(path / f.first, f.second);
    SyncDirectory(path);
    SyncDirectory(path.parent_path());
}
} // namespace
std::filesystem::path PcFolder()
{
    return std::filesystem::u8path(ReiPan_PathUserDir()) / "memcard/mc0-0" / FolderName;
}
bool Inspect(const fs::path &path, bool native_allowed, Folder &folder, std::string &error)
{
    try
    {
        if (fs::is_symlink(path) || !fs::is_directory(path)) throw std::runtime_error("Choose an extracted Fatal Frame III save folder.");
        SaveFiles files;
        std::set<std::string> names = {"Zero3System", "icon.sys", Icon};
        for (auto region : {SaveRegion::EU, SaveRegion::US, SaveRegion::JP}) names.insert(RegionFolder(region));
        for (int i = 0; i < 5; ++i) names.insert(Name(i));
        for (const auto &name : names) if (fs::exists(path / name)) files[name] = Read(path / name);
        return InspectFiles(files, native_allowed, folder, error);
    }
    catch (const std::exception &e) { error = e.what(); return false; }
}
bool InspectFiles(const SaveFiles &files, bool native_allowed, Folder &folder, std::string &error)
{
    Folder result;
    unsigned regions = 0;
    for (auto region : {SaveRegion::EU, SaveRegion::US, SaveRegion::JP})
        if (files.count(RegionFolder(region))) { result.region = region; ++regions; }
    try
    {
        if (regions != 1) throw std::runtime_error("Choose a save with one supported Fatal Frame III region.");
        result.files = files;
        for (auto name : {RegionFolder(result.region), "Zero3System", "icon.sys", Icon})
            if (!files.count(name)) throw std::runtime_error(std::string("Missing game file: ") + name);
        const auto &head = files.at(RegionFolder(result.region));
        const auto &sys = files.at("Zero3System");
        const auto &icon = files.at("icon.sys");
        if (head.size() != HeadSize || !Checksum(head) || sys.size() != 20 || (!Checksum(sys) && !Empty(sys)))
            throw std::runtime_error("Invalid save header or system file checksum/layout.");
        if (icon.size() != 964 || std::memcmp(icon.data(), "PS2D", 4) || files.at(Icon).empty())
            throw std::runtime_error("Missing or invalid PS2 save icons.");
        for (int i = 0; i < 5; ++i)
        {
            auto &slot = result.slots[i];
            const auto *h = head.data() + i * 36;
            slot.occupied = h[33] != 0;
            slot.area = int(U32(h + 24)); slot.place = int(U32(h + 20)); slot.finder = h[32] != 0;
            slot.hours = U32(h); slot.minutes = U32(h + 4); slot.seconds = U32(h + 8);
            if (h[33] > 1 || h[30] >= 3 || slot.minutes >= 60 || slot.seconds >= 60)
                slot.issue = "Invalid slot metadata or availability flag.";
            const auto found = files.find(Name(i));
            if (found == files.end()) { slot.issue = "Missing gameplay slot file."; slot.description = "Unavailable save"; continue; }
            const auto &d = found->second;
            if (d.size() != RegionalSize(result.region) && !(native_allowed && result.region == SaveRegion::EU && d.size() == NativeSize()))
                slot.issue = "Unsupported slot size for the declared region.";
            if (!slot.occupied)
            {
                if (!Empty(d)) slot.issue = "Empty slot metadata does not match its gameplay file.";
                slot.description = "Empty"; continue;
            }
            Bytes native;
            if (slot.issue.empty() && !ImportRegion(d, result.region, native, slot.issue, slot.hz))
            {
                if (result.region == SaveRegion::EU && ImportRegion(d, result.region, native, slot.issue, 30)) slot.hz = 30;
            }
            if (slot.issue.empty())
            {
                const auto *g = native.data() + BlockOffset(27, true);
                const auto *area = native.data() + BlockOffset(34, true);
                if (h[30] != g[0] || h[31] != U32(g + 0x20) || U32(h + 24) != U32(area))
                    slot.issue = "Slot metadata does not match gameplay data.";
                const char *players[] = {"Rei", "Miku", "Kei"};
                slot.player = players[g[0]]; slot.chapter = g[1];
            }
            slot.notice = RegionNotice(result.region, SaveRegion::EU);
            slot.description = slot.issue.empty() ? slot.player + " | chapter " + std::to_string(slot.chapter) + " | room " + std::to_string(slot.area) : "Unavailable save";
        }
        folder = std::move(result); error.clear(); return true;
    }
    catch (const std::exception &e)
    {
        if (regions == 1)
        {
            result.files = files; result.issue = e.what();
            for (int i = 0; i < 5; ++i)
            {
                auto &slot = result.slots[i]; auto found = files.find(Name(i));
                slot.occupied = found != files.end() && !Empty(found->second);
                slot.issue = result.issue; slot.description = "Unavailable save";
            }
            folder = std::move(result); error.clear(); return true;
        }
        error = e.what(); return false;
    }
}
} // namespace ReiPanSave

namespace ReiPanSave
{
bool ReadStorage(const fs::path &path, StorageKind kind, bool native_allowed, Storage &storage,
                 std::string &error)
{
    try
    {
        Storage result;
        result.kind = kind;
        result.path = path;
        if (kind == StorageKind::BlankCard)
        {
            storage = std::move(result);
            error.clear();
            return true;
        }
        result.exists = fs::exists(path);
        if (result.exists)
        {
            if (kind == StorageKind::Card)
            {
                result.card = Read(path, 8650752);
                SaveFiles files;
                if (result.card.size() == 8388608 || result.card.size() == 8650752)
                {
                    for (auto region : {SaveRegion::EU, SaveRegion::US, SaveRegion::JP})
                    {
                        SaveFiles candidate; bool found = false;
                        if (!ReadMemoryCard(result.card, candidate, found, error, RegionFolder(region))) return false;
                        if (found)
                        {
                            if (result.has_save) throw std::runtime_error("This card has saves for multiple FF3 regions. Open an extracted regional folder/archive.");
                            result.has_save = true; files = std::move(candidate);
                        }
                    }
                }
                else
                {
                    std::string directory;
                    if (!ReadSaveArchive(result.card, directory, files, error))
                        return false;
                    // Archive display names may differ from the original game directory.
                    // The internal regional header identifies the actual save.
                    result.has_save = true;
                }
                if (result.has_save && !InspectFiles(files, false, result.save, error))
                    return false;
            }
            else
            {
                if (!Inspect(path, native_allowed, result.save, error))
                    return false;
                result.has_save = true;
            }
        }
        else if (kind == StorageKind::Card)
            throw std::runtime_error("Choose an existing memory-card file.");
        storage = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
bool BuildBatch(const Folder &source, const Folder *destination,
                const std::vector<SlotMapping> &slots, bool export_ps2, Folder &result,
                std::string &error, const RegionOptions &options)
{
    try
    {
        if (slots.empty() || slots.size() > 5) throw std::runtime_error("Select between one and five saves.");
        Folder checked_source, checked_destination;
        if (!InspectFiles(source.files, export_ps2, checked_source, error)) return false;
        if (!checked_source.issue.empty()) throw std::runtime_error(checked_source.issue);
        if (destination)
        {
            if (!InspectFiles(destination->files, !export_ps2, checked_destination, error)) return false;
            if (!checked_destination.issue.empty()) throw std::runtime_error(checked_destination.issue);
        }
        auto region = export_ps2 ? options.region : SaveRegion::EU;
        if (destination && checked_destination.region != region) throw std::runtime_error("Destination region differs from the selected output region.");
        Folder next = destination ? checked_destination : Fresh(checked_source, export_ps2, region);
        std::set<int> sources, targets;
        for (const auto &slot : slots)
        {
            if (slot.source < 0 || slot.source >= 5 || slot.destination < 0 || slot.destination >= 5)
                throw std::runtime_error("Assign a destination slot to each selected save.");
            if (!sources.insert(slot.source).second || !targets.insert(slot.destination).second)
                throw std::runtime_error("Each source and destination slot can be used only once.");
            const auto &info = checked_source.slots[slot.source];
            if (!info.occupied) throw std::runtime_error("Cannot transfer an empty slot.");
            if (!info.issue.empty()) throw std::runtime_error(info.issue);
            Bytes payload;
            if (!ImportRegion(checked_source.files.at(Name(slot.source)), checked_source.region, payload, error,
                              options.source_hz ? options.source_hz : info.hz)) return false;
            if (export_ps2 && !ExportRegion(payload, region, payload, error, options.target_hz)) return false;
            next.files[Name(slot.destination)] = std::move(payload);
            std::memcpy(next.files[RegionFolder(region)].data() + slot.destination * 36,
                        checked_source.files.at(RegionFolder(checked_source.region)).data() + slot.source * 36, 36);
        }
        Seal(next.files[RegionFolder(region)]);
        if (!InspectFiles(next.files, !export_ps2, result, error)) return false;
        if (!result.issue.empty()) throw std::runtime_error(result.issue);
        for (const auto &slot : slots) if (!result.slots[slot.destination].issue.empty()) throw std::runtime_error(result.slots[slot.destination].issue);
        return true;
    }
    catch (const std::exception &e) { error = e.what(); return false; }
}

namespace
{
bool SameStorage(const Storage &expected, bool native, std::string &error)
{
    Storage current;
    return ReadStorage(expected.path, expected.kind, native, current, error) &&
           current.exists == expected.exists && current.has_save == expected.has_save &&
           current.card == expected.card && current.save.files == expected.save.files;
}
void CommitFolder(const Storage &expected, const Folder &folder, bool native, std::string &error)
{
    const auto dest = fs::absolute(expected.path).lexically_normal();
    if (fs::is_symlink(dest))
        throw std::runtime_error("Destination cannot be a symbolic link.");
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".reipan-transfer-" + Unique());
    const auto rollback = dest.parent_path() / (".reipan-rollback-" + Unique());
    bool moved = false, installed = false;
    try
    {
        if (!fs::create_directory(stage))
            throw std::runtime_error("Cannot create staging directory.");
        if (expected.exists) fs::copy(dest, stage, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        WriteFolder(stage, folder);
        Folder verified;
        if (!Inspect(stage, native, verified, error) || verified.files != folder.files)
            throw std::runtime_error("Staged save verification failed. " + error);
        if (!SameStorage(expected, native, error))
            throw std::runtime_error("Destination changed; refresh before transferring.");
        if (expected.exists)
        {
            fs::rename(dest, rollback);
            moved = true;
        }
        fs::rename(stage, dest);
        installed = true;
        SyncDirectory(dest.parent_path());
    }
    catch (...)
    {
        std::error_code ec;
        if (installed)
        {
            fs::rename(dest, stage, ec);
            if (ec)
                throw std::runtime_error(
                    "Could not roll back the transfer; previous folder remains at " +
                    PathUtf8(rollback));
        }
        if (moved)
        {
            fs::rename(rollback, dest, ec);
            if (ec)
                throw std::runtime_error("Could not restore the previous folder; it remains at " +
                                         PathUtf8(rollback));
        }
        fs::remove_all(stage, ec);
        throw;
    }
    std::error_code ec;
    if (moved)
        fs::remove_all(rollback, ec);
    if (ec)
        error = "Transfer complete; could not remove temporary folder " + PathUtf8(rollback);
}
void CommitCard(const fs::path &path, const Bytes &bytes)
{
    if (path.empty() || fs::exists(path) || fs::is_symlink(path))
        throw std::runtime_error(
            "Choose a new output filename; existing cards are never overwritten.");
    auto dest = fs::absolute(path).lexically_normal();
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".reipan-card-" + Unique());
    bool published = false;
    try
    {
        Write(stage, bytes);
        // Copy the validated bytes into a new file; exclusive creation refuses overwrites.
        Write(dest, bytes, true);
        published = true;
        fs::remove(stage);
        SyncDirectory(dest.parent_path());
    }
    catch (...)
    {
        std::error_code ec;
        fs::remove(stage, ec);
        if (published)
            fs::remove(dest, ec);
        throw;
    }
}
} // namespace
bool TransferBatch(const Storage &source, const Storage &destination,
                   const std::vector<SlotMapping> &slots, bool export_ps2,
                   const fs::path &card_output, std::string &error, bool ecc, SaveArchiveFormat archive, const RegionOptions &options)
{
    try
    {
        if (source.kind == StorageKind::BlankCard || !source.has_save)
            throw std::runtime_error("Source contains no supported Fatal Frame III saves.");
        if ((!export_ps2 && destination.kind != StorageKind::Folder) ||
            (export_ps2 && source.kind != StorageKind::Folder))
            throw std::runtime_error("Choose PC saves on the PC side of the transfer.");
        if (destination.kind == StorageKind::Folder && source.kind == StorageKind::Folder &&
            fs::weakly_canonical(source.path) == fs::weakly_canonical(destination.path))
            throw std::runtime_error("Source and destination must be different folders.");
        if (!SameStorage(source, export_ps2, error) ||
            !SameStorage(destination, !export_ps2, error))
            throw std::runtime_error(
                "Source or destination changed; refresh and review the slots again. " + error);
        Folder next;
        if (!BuildBatch(source.save, destination.has_save ? &destination.save : nullptr, slots,
                        export_ps2, next, error, options))
            return false;
        error.clear();
        if (destination.kind == StorageKind::Folder)
            CommitFolder(destination, next, !export_ps2, error);
        else
        {
            Bytes output;
            if (archive == SaveArchiveFormat::None)
            {
                if (!WriteMemoryCard(destination.card, next.files, output, error, ecc, RegionFolder(next.region)))
                    return false;
            }
            else if (!WriteSaveArchive(RegionFolder(next.region), next.files, archive, output, error))
                return false;
            if (!SameStorage(source, export_ps2, error) || !SameStorage(destination, false, error))
                throw std::runtime_error(
                    "Save files changed while preparing the card; refresh before exporting.");
            CommitCard(card_output, output);
        }
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
} // namespace ReiPanSave
