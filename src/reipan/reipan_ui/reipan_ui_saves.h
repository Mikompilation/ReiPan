#ifndef REIPAN_UI_SAVES_H
#define REIPAN_UI_SAVES_H
struct SDL_Window;
struct SDL_GPUDevice;
namespace ReiPanUiSaves
{
void Init(SDL_GPUDevice *device);
void Shutdown();
void MenuItem();
void Draw(SDL_Window *window);
} // namespace ReiPanUiSaves
#endif
