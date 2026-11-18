#ifndef REIPAN_SAVE_REGION_H
#define REIPAN_SAVE_REGION_H
#include "reipan_save_codec.h"
namespace ReiPanSave
{
enum class SaveRegion { EU, US, JP };
struct RegionOptions
{
    SaveRegion region = SaveRegion::EU;
    unsigned source_hz = 0, target_hz = 25;
};
const char *RegionFolder(SaveRegion region);
const char *RegionLabel(SaveRegion region);
size_t RegionalSize(SaveRegion region);
bool ImportRegion(const Bytes &data, SaveRegion region, Bytes &native, std::string &error,
                  unsigned eu_hz = 25);
bool ExportRegion(const Bytes &native, SaveRegion region, Bytes &data, std::string &error,
                  unsigned eu_hz = 25);
std::string RegionNotice(SaveRegion source, SaveRegion target);
} // namespace ReiPanSave
#endif
