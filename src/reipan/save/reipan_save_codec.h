#ifndef REIPAN_SAVE_CODEC_H
#define REIPAN_SAVE_CODEC_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ReiPanSave
{
using Bytes = std::vector<uint8_t>;
constexpr size_t Ps2Size = 63470;
constexpr size_t HeadSize = 184;
constexpr const char *FolderName = "BESLES-53825ZERO-BD";
size_t NativeSize();
bool Empty(const Bytes &data);
bool Checksum(const Bytes &data);
void Seal(Bytes &data);
Bytes EmptySlot(size_t size);
size_t BlockOffset(unsigned block, bool native);
bool Validate(const Bytes &data, std::string &error);
bool ToNative(const Bytes &ps2, Bytes &native, std::string &error);
bool ToPs2(const Bytes &native, Bytes &ps2, std::string &error);
// Called only after EventDataLoad has completed, before scattering imported state.
bool PrepareLoaded(Bytes &native, const uint8_t *events, size_t event_size, std::string &error);
} // namespace ReiPanSave
#endif
