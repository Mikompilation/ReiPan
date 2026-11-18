#include "reipan_ui_save_preview.h"
#include "ingame/map/MapSnap.h"
#include "main/gphase.h"
#include "reipan/io/reipan_fileload.h"
#include "system/eeiop/cd_file_dat.h"
#include "system/eeiop/cddat.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <map>
namespace ReiPanUiSavePreview
{
namespace
{
SDL_GPUDevice *device;
struct Texture
{
    SDL_GPUTexture *gpu = nullptr;
    int width = 0, height = 0;
};
std::map<int, Texture> textures;
ReiPanSave::Bytes room_table;
unsigned U16(const uint8_t *p) { return unsigned(p[0]) | unsigned(p[1]) << 8; }
unsigned U32(const uint8_t *p) { return U16(p) | U16(p + 2) << 16; }
ReiPanSave::Bytes LoadAsset(int file)
{
    const unsigned bytes = GetFileSize(file);
    if (!bytes || bytes > 4 * 1024 * 1024)
        return {};
    ReiPanSave::Bytes input(bytes);
    char path[256];
    GetFileNameBuffer(file, path);
    std::string named = path;
    std::string raw = named;
    if (cddatIsCmpFile(file))
    {
        const auto dot = raw.find_last_of('.');
        if (dot != std::string::npos)
            raw.replace(dot, std::string::npos, ".tm2");
    }
    ReiPan_FileLoadReq req{};
    req.ee_adrs = reinterpret_cast<uintptr_t>(input.data());
    req.request_size = int(bytes);
    req.start_sector = GetFileStartSector(file);
    req.cmp_file_name = named.c_str();
    req.raw_file_name = raw.c_str();
    req.raw_size = bytes;
    req.wrk_size = int(bytes);
    req.is_cmp = cddatIsCmpFile(file);
    req.file_no = file;
    ReiPan_FileLoadServe(&req);
    return input;
}
Texture Load(int file)
{
    Texture texture;
    ReiPanSave::Bytes rgba;
    const auto input = LoadAsset(file);
    if (!Decode(input, texture.width, texture.height, rgba))
        return texture;
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = unsigned(texture.width);
    info.height = unsigned(texture.height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    texture.gpu = SDL_CreateGPUTexture(device, &info);
    SDL_GPUTransferBufferCreateInfo upload_info{};
    upload_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    upload_info.size = unsigned(rgba.size());
    auto *upload = SDL_CreateGPUTransferBuffer(device, &upload_info);
    void *mapped = upload ? SDL_MapGPUTransferBuffer(device, upload, false) : nullptr;
    if (!texture.gpu || !mapped)
    {
        if (texture.gpu)
            SDL_ReleaseGPUTexture(device, texture.gpu);
        if (upload)
            SDL_ReleaseGPUTransferBuffer(device, upload);
        texture.gpu = nullptr;
        return texture;
    }
    std::memcpy(mapped, rgba.data(), rgba.size());
    SDL_UnmapGPUTransferBuffer(device, upload);
    auto *command = SDL_AcquireGPUCommandBuffer(device);
    auto *copy = command ? SDL_BeginGPUCopyPass(command) : nullptr;
    if (copy)
    {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = upload;
        SDL_GPUTextureRegion target{};
        target.texture = texture.gpu;
        target.w = info.width;
        target.h = info.height;
        target.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &target, false);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(command))
        {
            SDL_ReleaseGPUTexture(device, texture.gpu);
            texture.gpu = nullptr;
        }
    }
    else
    {
        if (command)
            SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTexture(device, texture.gpu);
        texture.gpu = nullptr;
    }
    SDL_ReleaseGPUTransferBuffer(device, upload);
    return texture;
}
} // namespace
bool Decode(const ReiPanSave::Bytes &file, int &width, int &height, ReiPanSave::Bytes &rgba)
{
    if (file.size() < 64 || file[5] > 1 || std::memcmp(file.data(), "TIM2", 4) != 0)
        return false;
    const size_t offset = file[5] == 1 ? 128 : 16;
    if (offset + 48 > file.size())
        return false;
    const auto *p = file.data() + offset;
    const unsigned header = U16(p + 12), image = U32(p + 8), clut = U32(p + 4);
    const unsigned w = U16(p + 20), h = U16(p + 22), type = p[19];
    if (header < 48 || w == 0 || h == 0 || w > 1024 || h > 1024 ||
        size_t(header) + image + clut > file.size() - offset)
        return false;
    const unsigned bpp = type == 1   ? 16
                         : type == 2 ? 24
                         : type == 3 ? 32
                         : type == 4 ? 4
                         : type == 5 ? 8
                                     : 0;
    if (!bpp || image < (size_t(w) * h * bpp + 7) / 8)
        return false;
    const auto *pixels = p + header, *palette = pixels + image;
    const unsigned colors = U16(p + 14), palette_type = p[18] & 0x3f;
    const unsigned palette_bytes = palette_type == 1   ? 2
                                   : palette_type == 2 ? 3
                                   : palette_type == 3 ? 4
                                                       : 0;
    if (bpp <= 8 &&
        (!palette_bytes || colors != (bpp == 4 ? 16u : 256u) || clut < colors * palette_bytes))
        return false;
    ReiPanSave::Bytes out(size_t(w) * h * 4);
    for (size_t i = 0; i < size_t(w) * h; ++i)
    {
        const uint8_t *color = nullptr;
        unsigned color_bytes = bpp / 8;
        if (bpp <= 8)
        {
            unsigned index = bpp == 4 ? (pixels[i / 2] >> ((i % 2) * 4)) & 15 : pixels[i];
            if (bpp == 8 && !(p[18] & 0x80))
                index = (index & 0xe7) | ((index & 8) << 1) | ((index & 16) >> 1);
            color = palette + index * palette_bytes;
            color_bytes = palette_bytes;
        }
        else
            color = pixels + i * color_bytes;
        if (color_bytes == 2)
        {
            const unsigned rgb = U16(color);
            for (unsigned c = 0; c < 3; ++c)
                out[i * 4 + c] = uint8_t(((rgb >> (5 * c)) & 31) * 255 / 31);
        }
        else
            std::copy_n(color, 3, out.data() + i * 4);
        out[i * 4 + 3] = 255;
    }
    width = int(w);
    height = int(h);
    rgba = std::move(out);
    return true;
}
void Init(SDL_GPUDevice *gpu) { device = gpu; }
void Shutdown()
{
    if (device)
        for (auto &[id, t] : textures)
            if (t.gpu)
                SDL_ReleaseGPUTexture(device, t.gpu);
    textures.clear();
    room_table.clear();
    device = nullptr;
}
void Draw(const ReiPanSave::SlotInfo &slot, ImVec2 size)
{
    int file = -1;
    const auto *phase = GetGPhaseCtrl()->leaf;
    // The language screen has loaded the file table; earlier boot may not have it yet.
    if (device && slot.occupied && phase && phase->id >= 2)
    {
        if (slot.finder)
            file = CLEARDATA_TM2;
        else
        {
            if (room_table.empty())
                room_table = LoadAsset(MAP_MENU_ECB);
            if (room_table.size() >= 32 && std::memcmp(room_table.data(), "ecb", 3) == 0)
            {
                const unsigned rows = U32(room_table.data() + 8),
                               stride = U32(room_table.data() + 12);
                if (stride >= 12 && rows <= (room_table.size() - 32) / stride)
                    for (unsigned i = 0; i < rows; ++i)
                    {
                        const auto *row = room_table.data() + 32 + i * stride;
                        if (int(row[4]) == slot.area && int(int16_t(U16(row + 6))) == slot.place)
                        {
                            const int index = int(int16_t(U16(row + 8)));
                            if (index >= 0)
                                file = index + RS101_R01_TM2;
                            break;
                        }
                    }
            }
        }
        if (file >= 0 && file < GetFileNum() && !textures.count(file))
            textures.emplace(file, Load(file));
    }
    const auto found = textures.find(file);
    if (found != textures.end() && found->second.gpu)
        ImGui::Image((ImTextureID)(intptr_t)found->second.gpu, size);
    else
    {
        const auto at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y),
                                                  IM_COL32(32, 35, 38, 255));
        ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + 5, at.y + size.y / 2 - 6),
                                            IM_COL32(140, 145, 150, 255),
                                            slot.occupied ? "No preview" : "Empty");
        ImGui::Dummy(size);
    }
}
} // namespace ReiPanUiSavePreview
