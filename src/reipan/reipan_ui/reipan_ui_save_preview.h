#ifndef REIPAN_UI_SAVE_PREVIEW_H
#define REIPAN_UI_SAVE_PREVIEW_H
#include "reipan/save/reipan_save_transfer.h"
#include <imgui.h>
struct SDL_GPUDevice;
namespace ReiPanUiSavePreview
{
void Init(SDL_GPUDevice *device);
void Shutdown();
void Draw(const ReiPanSave::SlotInfo &slot, ImVec2 size);
// Pure TIM2 decoding; previews never upload into the game's GS memory.
bool Decode(const ReiPanSave::Bytes &file, int &width, int &height, ReiPanSave::Bytes &rgba);
} // namespace ReiPanUiSavePreview
#endif
