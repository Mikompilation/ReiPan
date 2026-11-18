// graphics/graph3d/eeFile.cpp -- CFile / CStdioFile / CFileName, FF3 0x154df0..0x155660
// (+ the linkonce bodies 0x2add80..0x2ae7a0). From MioPan's eeFile.cpp, checked
// against FF3: the class gained a vptr in front (every member moves up 4), the
// asserts are gone, the path split is an out-of-line helper (FileSplitPath)
// instead of FF2's inlined block, and ErrorString is not emitted. No FF3 code calls any of it (ghidra.py refs: no
// jal / j / data word to the ctors or Open), so it is reconstructed for
// completeness and never runs.

#include "graphics/graph3d/eeFile.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>                     // abs
#include <string.h>

#include "sdk/fileio.h"                 // sceOpen / sceRead / sceWrite / sceClose / sceLseek

// The handle starts closed; the error string starts empty (the shared empty
// rep 0x41e4ac). FF3 0x00154e30 is the base-object twin, same body.
// FF3 0x00154df0
CFile::CFile()
{
    m_iHandle = -1;
}

// Close any open handle, then the error string goes. Base-object twin 0x154e70
// (CStdioFile's dtors call it), deleting 0x154f20.
// FF3 0x00154ec8
// callcheck: string_Rep_M_destroy (m_strError's destructor)
CFile::~CFile()
{
    Close();
}

// OpenFlags -> sceOpen flags: read -> SCE_RDONLY, write -> SCE_WRONLY, the
// other two modes -> 0; modeCreate adds SCE_CREAT. On success the path is kept
// for the Get* queries; on failure the error is strerror of the fd's magnitude.
// FF3 0x00154f80
int CFile::Open(const char *pFileName, unsigned int uiOpenFlags)
{
    int iRW;
    int iFlag;

    iRW = 0;
    if ((uiOpenFlags & RWMASK) == modeRead)
    {
        iRW = SCE_RDONLY;
    }
    else if ((uiOpenFlags & RWMASK) == modeWrite)
    {
        iRW = SCE_WRONLY;
    }

    iFlag = iRW;
    if (uiOpenFlags & WRITEMASK)
    {
        iFlag = iRW | SCE_CREAT;
    }

    m_iHandle = sceOpen(pFileName, iFlag);
    if (m_iHandle < 0)
    {
        return OnError(strerror(abs(m_iHandle)));
    }

    strcpy(m_strFileName, pFileName);
    return 1;
}

// Returns sceRead's count, also when it failed (after recording the error).
// FF3 0x00155030
unsigned int CFile::Read(void *lpBuf, unsigned int uiCount)
{
    int iBytesRead;

    iBytesRead = sceRead(m_iHandle, lpBuf, uiCount);
    if (iBytesRead < 0)
    {
        OnError(strerror(abs(iBytesRead)));
    }

    return iBytesRead;
}

// 1 when something was written, 0 when nothing was or it failed.
// FF3 0x00155090
int CFile::Write(const void *lpBuf, unsigned int uiCount)
{
    int iBytesWritten;

    iBytesWritten = sceWrite(m_iHandle, lpBuf, uiCount);
    if (iBytesWritten < 0)
    {
        return OnError(strerror(abs(iBytesWritten)));
    }

    return iBytesWritten != 0;
}

// FF3 0x001550f0
void CFile::Close()
{
    if (m_iHandle != -1)
    {
        sceClose(m_iHandle);
        m_iHandle = -1;
    }
}

// FF3 0x00155138
int CFile::Seek(int iOffset, int iFrom)
{
    return sceLseek(m_iHandle, iOffset, iFrom);
}

// The size by seeking to the end, then back where the file was.
// FF3 0x00155150
unsigned int CFile::GetLength()
{
    int iCurPos;
    int iLength;

    iCurPos = sceLseek(m_iHandle, 0, SCE_SEEK_CUR);
    iLength = sceLseek(m_iHandle, 0, SCE_SEEK_END);
    sceLseek(m_iHandle, iCurPos, SCE_SEEK_SET);

    return iLength;
}

// FF3 0x001551c0
unsigned int CFile::SeekToEnd()
{
    return Seek(0, end);
}

// FF3 0x001551e0
void CFile::SeekToBegin()
{
    Seek(0, begin);
}

// Split the path into the members: four zeroed 260-byte buffers filled by
// FileSplitPath, assigned to drive / dir / file / ext, then drive + dir and
// file + ext. FF3 inlines this block into GetFileName and GetFileTitle (the
// c_str() before the split is old libstdc++'s terminator store).
void CFileName::Split()
{
    char acDrive[260];
    char acDir[260];
    char acFile[260];
    char acExt[260];

    memset(acDrive, 0, sizeof(acDrive));
    memset(acDir, 0, sizeof(acDir));
    memset(acFile, 0, sizeof(acFile));
    memset(acExt, 0, sizeof(acExt));

    FileSplitPath(c_str(), acDrive, acDir, acFile, acExt);

    m_strDrive.assign(acDrive, strlen(acDrive));
    m_strDir.assign(acDir, strlen(acDir));
    m_strFile.assign(acFile, strlen(acFile));
    m_strExt.assign(acExt, strlen(acExt));

    m_strDriveDir = m_strDrive;
    m_strDriveDir += m_strDir;

    m_strFileExt = m_strFile;
    m_strFileExt += m_strExt;
}

// "file.ext" of the opened path.
// FF3 0x00155200
// callcheck: FileNameCtor, FileSplitPath, memset, strlen, string_assign_cstr_n, string_assign, string_append, string_ctor_cstr2, string_dtor, string_Rep_M_dispose
// (CFileName's constructor and the split that FF3 inlines here are in CFileName /
// CFileName::Split; the string calls are std::string's members)
std::string CFile::GetFileName()
{
    CFileName fn(m_strFileName);

    fn.Split();

    return std::string(fn.m_strFileExt.c_str());
}

// "file" of the opened path, without its extension.
// FF3 0x00155410
// callcheck: FileNameCtor, FileSplitPath, memset, strlen, string_assign_cstr_n, string_assign, string_append, string_ctor_cstr2, string_dtor, string_Rep_M_dispose
// (as GetFileName)
std::string CFile::GetFileTitle()
{
    CFileName fn(m_strFileName);

    fn.Split();

    return std::string(fn.m_strFile.c_str());
}

// The path as given to Open.
// FF3 0x00155620
// callcheck: string_ctor_cstr2 (std::string(const char *))
std::string CFile::GetFilePath()
{
    return std::string(m_strFileName);
}

// Format the message (256 bytes on the EE) into the error string. Always 0, so
// a failing member can `return OnError(...)`.
// FF3 0x002ae1b8
// callcheck: vsprintf, string_assign_cstr_n (vsnprintf on the host, bounded to the EE buffer; std::string::assign)
int CFile::OnError(const char *pStr, ...)
{
    char    str[256];
    va_list args;

    va_start(args, pStr);
    vsnprintf(str, sizeof(str), pStr, args);
    va_end(args);

    m_strError.assign(str, strlen(str));
    return 0;
}

// Empty in FF3, as in FF2.
// FF3 0x00155650
void CStdioFile::WriteString(const char *lpsz)
{
    (void)lpsz;
}

// FF3 0x00155658
char *CStdioFile::ReadString(char *lpsz, unsigned int nMax)
{
    (void)lpsz;
    (void)nMax;
    return nullptr;
}

// Deleting twin 0x2ae248.
// FF3 0x002ae220
// callcheck: CFileDtor (the base class's destructor, implicit)
CStdioFile::~CStdioFile()
{
}

// "X:" up to the last ':', "dir/" between the drive and the last '/' or '\'
// (whichever is later), the file between that separator and the last '.',
// ".ext" from the last '.'. A null out is skipped; strncpy does not
// terminate, so callers pass zeroed buffers.
// FF3 0x002ae280
void FileSplitPath(const char *pPath, char *pDrive, char *pDir, char *pFile, char *pExt)
{
    const char *pColon;
    const char *pSep;
    const char *pYen;
    const char *pDot;
    const char *pTop;
    int         iLen;

    pColon = strrchr(pPath, ':');
    if (pDrive != nullptr)
    {
        *pDrive = '\0';
        if (pColon != nullptr)
        {
            strncpy(pDrive, pPath, pColon - pPath + 1);
        }
    }

    pSep = strrchr(pPath, '/');
    pYen = strrchr(pPath, '\\');
    if (!(pYen < pSep))
    {
        pSep = pYen;
    }

    if (pDir != nullptr)
    {
        pTop = (pColon != nullptr) ? pColon + 1 : pPath;
        iLen = (pSep != nullptr) ? (int)(pSep - pTop) : 0;
        *pDir = '\0';
        if (iLen > 0)
        {
            strncpy(pDir, pTop, iLen);
            strcat(pDir, "/");          // rdata 0x36dbf0
        }
    }

    pDot = strrchr(pPath, '.');
    if (pFile != nullptr)
    {
        *pFile = '\0';
        pTop = (pSep != nullptr) ? pSep + 1 : pPath;
        if (pDot == nullptr)
        {
            iLen = (int)strlen(pTop);
        }
        else
        {
            iLen = (int)(pDot - pTop);
        }
        // PC guard: a '.' before the last separator ("./dir/file") gives a
        // negative length, which the EE's strncpy runs with as a huge count.
        if (iLen > 0)
        {
            strncpy(pFile, pTop, iLen);
        }
    }

    if (pExt != nullptr)
    {
        *pExt = '\0';
        if (pDot != nullptr)
        {
            strcpy(pExt, pDot);
        }
    }
}

// The string base takes the path; the seven member strings start empty.
// FF3 0x002ae3e0
// callcheck: string_ctor_cstr (the base string's constructor)
CFileName::CFileName(const char *pStr)
    : std::string(pStr)
{
}

// Members, then the base string. Deleting twin 0x2ae5c8.
// FF3 0x002ae498
// callcheck: string_Rep_M_destroy (the member and base strings' destructors)
CFileName::~CFileName()
{
}

// ──────────────────────────────────────────────────────────────────────
// The libstdc++ basic_string<char> instances FF3 emitted for this file (refcounted
// rep: length -0xc, capacity -8, refcount -4; the shared empty rep 0x41e4ac, its
// refcount bumped through 0x41e4a8; max_size 0x3ffffffc). On the host they are
// std::string's own members, nothing to reconstruct:
//   FF3 0x002add80 string_ctor_cstr2     basic_string(const char *, const allocator &)
//   FF3 0x002ae738 string_ctor_cstr      the same constructor's twin
//   FF3 0x002adde8 string_assign         assign(const basic_string &)
//   FF3 0x002ade80 string_append         append(const basic_string &)
//   FF3 0x002adf00 string_S_construct    _S_construct<const char *>(beg, end, alloc)
//   FF3 0x002adfa8 string_Rep_M_clone    _Rep::_M_clone(alloc, res)
//   FF3 0x002ae058 string_reserve        reserve(n)
//   FF3 0x002ae110 string_M_replace_safe _M_replace_safe(i1, i2, k1, k2)
//   FF3 0x002ae700 string_dtor           ~basic_string()
//   FF3 0x002abcc8 string_assign_cstr_n  assign(const char *, size_type) (emitted with
//                                        CDebugEdit's instances, called most from here)
