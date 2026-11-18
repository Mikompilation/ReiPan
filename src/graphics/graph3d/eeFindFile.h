/* ==========================================================================
 *  graphics/graph3d/eeFindFile.h
 *
 *  CFindFile (FF2 eeFindFile.cpp; RTTI typeinfo 0x3729f8, vtable 0x341cc8):
 *  a directory scanner over the EE directory calls (sceDopen / sceDread /
 *  sceDclose). Given a directory and a '|'-separated list of specs
 *  ("name.ext", '*' wildcards) it reads every entry and sorts them into the
 *  spec-matched files (sorted by name in FF3), all entries, and the
 *  sub-directories with and without "." / "..". Like CFile it came with the
 *  g3d library and nothing in FF3 constructs one: dead in the retail game
 *  (its vectors' allocator is even NullFuncRet0 there).
 *
 *  FF3 0x155660..0x156e08 + the linkonce bodies 0x2ae7a0..0x2afd10 (the
 *  vector<string> / vector<sce_dirent> instances, std::sort over the matched
 *  files, OnError).
 *
 *  layout: host -- a vtable, std::string and std::vector members (EE: string
 *  = one pointer, vector = a 4-byte allocator object + start / finish /
 *  end_of_storage) and sce_dirent's d_private pointer (EE element 0x144
 *  bytes), so the EE offsets are noted per member and not asserted.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_EEFINDFILE_H
#define _GRAPHICS_GRAPH3D_EEFINDFILE_H

#include <string>
#include <vector>

#include "sdk/sifdev.h"                 // sce_dirent

// layout: host -- vtable + std::string / std::vector. EE size 0x60.
class CFindFile
{
public:
    CFindFile();                        // FF3 0x00155660 (base-object twin 0x155730)
    virtual ~CFindFile();               // slot 0 FF3 0x001559c0 (base 0x1557f8), deleting slot 1 0x155b88

    int  Scan(const char *pDirName, const char *pFileSpec);     // FF3 0x00156a70
    int  Scan();                                                 // FF3 0x00156be8

    void SetDirectory(const char *pDirName);                     // FF3 0x00156ab0
    void SetFileSpec(const char *pFileSpec);                     // FF3 0x00156bd0
    void Clear();                                                // FF3 0x00156c00

    // FF2's inline accessors; FF3 emits none of them (nothing calls them).
    const char *ErrorString() { return m_strError.c_str(); }
    const char *CurrentDirectory() { return m_strCurrentDirectory.c_str(); }

    int         GetNumMatchFile() { return (int)m_vMatchFile.size(); }
    sce_dirent &GetMatchFileRef(int iIndex) { return m_vMatchFile[iIndex]; }
    int         GetNumFile() { return (int)m_vDirEntry.size(); }
    sce_dirent &GetFileRef(int iIndex) { return m_vDirEntry[iIndex]; }
    int         GetNumSubDirectory() { return (int)m_vSubDirectory.size(); }
    sce_dirent &GetSubDirectoryRef(int iIndex) { return m_vSubDirectory[iIndex]; }
    int         GetNumSubDirectoryAll() { return (int)m_vSubDirectoryAll.size(); }
    sce_dirent &GetSubDirectoryAllRef(int iIndex) { return m_vSubDirectoryAll[iIndex]; }

protected:
    // FF3 0x002af9f0 (inline in FF2's IError.h, linkonce in FF3)
    int OnError(const char *pStr, ...);

    std::string m_strError;             // EE +0x04, the last error message

private:
    int        ScanDirectory();                                          // FF3 0x001563a0
    int        IsMatchSpec(const char *pFileName, const char *strFileSpec); // FF3 0x00155d58
    int        AnalyzeSpec(const char *pFileSpec);                       // FF3 0x00156200
    static int IsDirectory(const sce_dirent *pDE);                       // FF3 0x00156388

    std::string              m_strOriginDirectory;  // EE +0x08, only cleared
    std::string              m_strCurrentDirectory; // EE +0x0c, the directory scanned
    std::vector<std::string> m_vstrFileSpec;        // EE +0x10, the specs split on '|'
    std::vector<sce_dirent>  m_vDirEntry;           // EE +0x20, every entry read
    std::vector<sce_dirent>  m_vMatchFile;          // EE +0x30, files matching a spec, by name
    std::vector<sce_dirent>  m_vSubDirectory;       // EE +0x40, directories but "." / ".."
    std::vector<sce_dirent>  m_vSubDirectoryAll;    // EE +0x50, every directory
};

#endif /* _GRAPHICS_GRAPH3D_EEFINDFILE_H */
