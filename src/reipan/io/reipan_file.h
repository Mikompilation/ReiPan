#ifndef REIPAN_FILE_H
#define REIPAN_FILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Host file-system wrapper.  Confines all host file I/O (SDL_IOStream / fopen)
// to the reipan layer, so the SDK shims (libcdvd, fileio) don't call SDL/stdio
// directly.  Previously each of those shims carried its own near-identical
// NormalizeXxxPath + root-search.
//
// This is bytes only -- every path handed to it is already absolute.  Deciding
// WHERE a file is belongs to reipan_paths.h, which is the only place that knows
// about data folders, shipped resources and the user directory.

// Seek origins (match SEEK_SET/CUR/END semantics, backend-independent).
#define REIPAN_SEEK_SET 0
#define REIPAN_SEEK_CUR 1
#define REIPAN_SEEK_END 2

// One-shot read of `size` bytes starting at `offset` from an already-resolved
// host path.  Zero-fills nothing; returns 1 if the stream opened (a short read
// stops early), 0 if the file could not be opened.
int ReiPan_FileReadAt(const char *host_path, uint64_t offset, void *buf,
                      size_t size);

// Byte size of a host file, or -1 on failure.
int64_t ReiPan_FileSize(const char *host_path);

// A stretch of a larger file, opened by a name of its own: every call in this
// file then takes `name` in place of a host path and reads `size` bytes of
// `host_path` from `offset`, with the ends where that stretch's ends are.  This
// is how the disc's files are read out of a disc image (reipan_paths.h), so
// their readers need no notion of one.  Read-only.  Set them before anything
// reads through their names, not while something does: readers take no lock.
// Mapping a name again replaces it.
void ReiPan_FileMapRegion(const char *name, const char *host_path, uint64_t offset,
                          uint64_t size);

// ---- handle-based stream API (backs fileio.cpp + libcdvd streaming) --------

typedef struct ReiPan_File ReiPan_File;

// `mode` is an fopen-style string ("rb", "wb", "r+b", "ab", "a+b", "w+b").
ReiPan_File *ReiPan_FileOpen(const char *host_path, const char *mode);
size_t       ReiPan_FileRead(ReiPan_File *f, void *buf, size_t size);
size_t       ReiPan_FileWrite(ReiPan_File *f, const void *buf, size_t size);
int          ReiPan_FileSeek(ReiPan_File *f, int64_t offset, int whence);
int64_t      ReiPan_FileTell(ReiPan_File *f);
int64_t      ReiPan_FileStreamSize(ReiPan_File *f);
void         ReiPan_FileCloseHandle(ReiPan_File *f);

#ifdef __cplusplus
}
#endif

#endif /* REIPAN_FILE_H */
