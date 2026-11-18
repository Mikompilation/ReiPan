#ifndef REIPAN_SAVE_PATHS_H
#define REIPAN_SAVE_PATHS_H
#include <filesystem>
#include <string>
namespace ReiPanSave
{
// SDL and ImGui use UTF-8; filesystem paths use UTF-16 on Windows.
inline std::string PathUtf8(const std::filesystem::path &path)
{
    const auto text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}
} // namespace ReiPanSave
#endif
