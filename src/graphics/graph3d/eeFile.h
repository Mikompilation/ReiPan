/* ==========================================================================
 *  graphics/graph3d/eeFile.h
 *
 *  CFile (FF2 eeFile.cpp; RTTI typeinfo 0x3729e0, vtable 0x341c68): a thin
 *  file object over the EE fileio calls (sceOpen / sceRead / sceWrite /
 *  sceClose / sceLseek) that keeps the last error as a string and splits its
 *  path through CFileName. CStdioFile (vtable 0x341c40) adds two empty text
 *  helpers. Nothing in FF3 constructs either class or calls a member: the
 *  file came along with the g3d library (FF2's tool code) and is dead in the
 *  retail game. FileSplitPath is shared with eeFindFile.cpp.
 *
 *  FF3 0x154df0..0x155660 + the linkonce bodies 0x2add80..0x2ae7a0 (the
 *  std::string instances first emitted here, OnError, the CStdioFile and
 *  CFileName ctors / dtors, FileSplitPath).
 *
 *  layout: host -- every class here has a vtable (EE vptr 4 bytes, host 8)
 *  and std::string members (EE: one pointer to the chars of a refcounted
 *  libstdc++ rep, 4 bytes), so the EE offsets are noted per member and not
 *  asserted.
 * ======================================================================== */
#ifndef _GRAPHICS_GRAPH3D_EEFILE_H
#define _GRAPHICS_GRAPH3D_EEFILE_H

#include <string>

#include "sdk/sifdev.h"                 // sce_stat

// Split a path into "X:" / "dir/" / "file" / ".ext" (each out may be null; the
// buffers are 260 bytes in every caller). FF3 0x002ae280
void FileSplitPath(const char *pPath, char *pDrive, char *pDir, char *pFile, char *pExt);

// layout: host -- vtable + std::string. EE size 0x150.
class CFile
{
public:
    // Open's flags (FF2 eeFile.h; kept in the class here, as MFC's CFile does,
    // so begin / end do not land in the global namespace).
    enum OpenFlags
    {
        modeRead   = 0x0000,
        modeWrite  = 0x0001,
        modeCreate = 0x1000,
        RWMASK     = 0x0003,
        WRITEMASK  = 0x1000
    };

    // Seek's origins (sceLseek's SCE_SEEK_*).
    enum SeekPosition
    {
        begin   = 0,
        current = 1,
        end     = 2
    };

protected:
    // FF3 0x002ae1b8 (inline in FF2's IError.h, linkonce in FF3)
    int OnError(const char *pStr, ...);

public:
    CFile();                            // FF3 0x00154df0 (twin 0x154e30)
    virtual ~CFile();                   // slot 0 FF3 0x00154ec8, deleting slot 1 0x154f20

    int          Open(const char *pFileName, unsigned int uiOpenFlags);  // FF3 0x00154f80
    unsigned int Read(void *lpBuf, unsigned int uiCount);                 // FF3 0x00155030
    int          Write(const void *lpBuf, unsigned int uiCount);          // FF3 0x00155090
    void         Close();                                                  // FF3 0x001550f0

    int          Seek(int iOffset, int iFrom);                            // FF3 0x00155138
    unsigned int GetLength();                                              // FF3 0x00155150
    unsigned int SeekToEnd();                                              // FF3 0x001551c0
    void         SeekToBegin();                                            // FF3 0x001551e0

    virtual std::string GetFileName();  // +0x08, FF3 0x00155200
    virtual std::string GetFileTitle(); // +0x0c, FF3 0x00155410
    virtual std::string GetFilePath();  // +0x10, FF3 0x00155620

protected:
    std::string m_strError;             // EE +0x04, the last error message

private:
    int         m_iHandle;              // EE +0x08, sceOpen's fd, -1 closed
    sce_stat    m_Status;               // EE +0x0c, never written
    char        m_strFileName[260];     // EE +0x4c, the path given to Open
};

// layout: host -- vtable. EE size 0x150. FF3 emits no constructor (nothing
// builds one); the destructors are linkonce.
class CStdioFile : public CFile
{
public:
    CStdioFile() {}
    virtual ~CStdioFile();              // slot 0 FF3 0x002ae220, deleting slot 1 0x2ae248

    virtual void  WriteString(const char *lpsz);                // +0x14, FF3 0x00155650
    virtual char *ReadString(char *lpsz, unsigned int nMax);    // +0x18, FF3 0x00155658
};

// The pieces of one path. RTTI typeinfo 0x341c80 (vmi: the string base sits at
// +4, after the vptr).
// layout: host -- vtable + std::string. EE size 0x24.
class CFileName : public std::string
{
    friend class CFile;

public:
    CFileName(const char *pStr);        // FF3 0x002ae3e0
    virtual ~CFileName();               // slot 0 FF3 0x002ae498, deleting slot 1 0x2ae5c8

protected:
    void Split();                       // inline in FF3 (GetFileName / GetFileTitle)

    std::string m_strDrive;             // EE +0x08
    std::string m_strDir;               // EE +0x0c
    std::string m_strFile;              // EE +0x10
    std::string m_strExt;               // EE +0x14
    std::string m_strDriveDir;          // EE +0x18, drive + dir
    std::string m_strParentDir;         // EE +0x1c, never set in FF3
    std::string m_strFileExt;           // EE +0x20, file + ext
};

#endif /* _GRAPHICS_GRAPH3D_EEFILE_H */
