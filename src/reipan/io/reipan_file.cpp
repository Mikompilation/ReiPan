#include "reipan_file.h"

#include <stdlib.h>

#include <SDL3/SDL_iostream.h>

#include <string>
#include <vector>

namespace
{
/* See ReiPan_FileMapRegion().  Written before any reader exists, so the readers
 * need no lock.  A disc has a handful of files, so a list is all this needs. */
struct Region
{
    std::string name;
    std::string host_path;
    uint64_t    offset = 0;
    uint64_t    size = 0;
};
std::vector<Region> g_regions;

const Region *FindRegion(const char *path)
{
    for (const Region &region : g_regions)
    {
        if (region.name == path)
        {
            return &region;
        }
    }
    return nullptr;
}
}

extern "C" {

void ReiPan_FileMapRegion(const char *name, const char *host_path, uint64_t offset,
                          uint64_t size)
{
    Region *region = const_cast<Region *>(FindRegion(name));
    if (region == nullptr)
    {
        g_regions.emplace_back();
        region = &g_regions.back();
    }
    region->name = name;
    region->host_path = host_path;
    region->offset = offset;
    region->size = size;
}

int ReiPan_FileReadAt(const char *host_path, uint64_t offset, void *buf, size_t size)
{
    SDL_IOStream *io;
    unsigned char *dst;
    size_t         remaining;
    size_t         readable = size;

    if (host_path == 0 || buf == 0)
    {
        return 0;
    }

    /* The end of a region is the end of the file: past it, a read comes up
     * short here just as it would on a copy of that file of its own. */
    const Region *region = FindRegion(host_path);
    if (region != nullptr)
    {
        const uint64_t left = offset < region->size ? region->size - offset : 0;
        readable = (uint64_t)size < left ? size : (size_t)left;
        host_path = region->host_path.c_str();
        offset += region->offset;
    }

    io = SDL_IOFromFile(host_path, "rb");
    if (io == 0)
    {
        return 0;
    }

    if (offset != 0 && SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0)
    {
        SDL_CloseIO(io);
        return 0;
    }

    dst = (unsigned char *)buf;
    remaining = readable;
    while (remaining > 0)
    {
        size_t got = SDL_ReadIO(io, dst, remaining);
        if (got == 0)
        {
            break;
        }
        dst += got;
        remaining -= got;
    }

    SDL_CloseIO(io);

    // A short read means the caller did NOT get the bytes it asked for -- the
    // offset was past EOF, or the file is truncated.  Reporting success here
    // makes the loader believe the buffer is valid, so it skips its zero-fill
    // and "file load missing" diagnostic and the game silently parses zeros.
    if (remaining != 0 || readable != size)
    {
        return 0;
    }

    return 1;
}

int64_t ReiPan_FileSize(const char *host_path)
{
    SDL_IOStream *io;
    Sint64        size;

    if (host_path == 0)
    {
        return -1;
    }

    const Region *region = FindRegion(host_path);
    if (region != nullptr)
    {
        return (int64_t)region->size;
    }

    io = SDL_IOFromFile(host_path, "rb");
    if (io == 0)
    {
        return -1;
    }

    size = SDL_GetIOSize(io);
    SDL_CloseIO(io);
    return (int64_t)size;
}

// ---- handle-based stream API -----------------------------------------------

struct ReiPan_File
{
    SDL_IOStream *io;
    /* Opened by a region's name: where it starts in `io`, and its length. */
    int      is_region;
    int64_t  region_offset;
    int64_t  region_size;
};

ReiPan_File *ReiPan_FileOpen(const char *host_path, const char *mode)
{
    if (host_path == 0 || mode == 0)
    {
        return 0;
    }

    const Region *region = FindRegion(host_path);
    SDL_IOStream *io = region != nullptr
                           ? SDL_IOFromFile(region->host_path.c_str(), "rb")
                           : SDL_IOFromFile(host_path, mode);
    if (io == 0)
    {
        return 0;
    }

    ReiPan_File *f = (ReiPan_File *)malloc(sizeof(ReiPan_File));
    if (f == 0)
    {
        SDL_CloseIO(io);
        return 0;
    }
    f->io = io;
    f->is_region = region != nullptr;
    f->region_offset = region != nullptr ? (int64_t)region->offset : 0;
    f->region_size = region != nullptr ? (int64_t)region->size : 0;
    if (region != nullptr && SDL_SeekIO(io, f->region_offset, SDL_IO_SEEK_SET) < 0)
    {
        ReiPan_FileCloseHandle(f);
        return 0;
    }
    return f;
}

size_t ReiPan_FileRead(ReiPan_File *f, void *buf, size_t size)
{
    if (f == 0 || f->io == 0 || buf == 0)
    {
        return 0;
    }
    if (f->is_region)
    {
        const int64_t left =
            f->region_offset + f->region_size - (int64_t)SDL_TellIO(f->io);
        if (left <= 0)
        {
            return 0;
        }
        if ((uint64_t)size > (uint64_t)left)
        {
            size = (size_t)left;
        }
    }
    return SDL_ReadIO(f->io, buf, size);
}

size_t ReiPan_FileWrite(ReiPan_File *f, const void *buf, size_t size)
{
    if (f == 0 || f->io == 0 || buf == 0)
    {
        return 0;
    }
    return SDL_WriteIO(f->io, buf, size);
}

int ReiPan_FileSeek(ReiPan_File *f, int64_t offset, int whence)
{
    SDL_IOWhence w;

    if (f == 0 || f->io == 0)
    {
        return -1;
    }

    if (whence == REIPAN_SEEK_CUR)
    {
        w = SDL_IO_SEEK_CUR;
    }
    else if (whence == REIPAN_SEEK_END)
    {
        w = SDL_IO_SEEK_END;
    }
    else
    {
        w = SDL_IO_SEEK_SET;
    }

    /* Either end of a region is somewhere in the file it is a part of. */
    if (f->is_region && w != SDL_IO_SEEK_CUR)
    {
        offset += f->region_offset + (w == SDL_IO_SEEK_END ? f->region_size : 0);
        w = SDL_IO_SEEK_SET;
    }

    return (SDL_SeekIO(f->io, (Sint64)offset, w) < 0) ? -1 : 0;
}

int64_t ReiPan_FileTell(ReiPan_File *f)
{
    if (f == 0 || f->io == 0)
    {
        return -1;
    }
    return (int64_t)SDL_TellIO(f->io) - f->region_offset;
}

int64_t ReiPan_FileStreamSize(ReiPan_File *f)
{
    if (f == 0 || f->io == 0)
    {
        return -1;
    }
    return f->is_region ? f->region_size : (int64_t)SDL_GetIOSize(f->io);
}

void ReiPan_FileCloseHandle(ReiPan_File *f)
{
    if (f == 0)
    {
        return;
    }
    if (f->io != 0)
    {
        SDL_CloseIO(f->io);
    }
    free(f);
}

}
