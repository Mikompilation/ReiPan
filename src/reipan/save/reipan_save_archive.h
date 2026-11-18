#ifndef REIPAN_SAVE_ARCHIVE_H
#define REIPAN_SAVE_ARCHIVE_H
#include "reipan_memory_card.h"
namespace ReiPanSave
{
enum class SaveArchiveFormat
{
    None,
    Psu,
    Max,
    Xps,
    Cbs
};
bool ReadSaveArchive(const Bytes &data, std::string &directory, SaveFiles &files,
                     std::string &error);
bool WriteSaveArchive(const std::string &directory, const SaveFiles &files,
                      SaveArchiveFormat format, Bytes &data, std::string &error);
} // namespace ReiPanSave
#endif
