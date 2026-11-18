// graphics/graph3d/eeFindFile.cpp -- CFindFile, FF3 0x155660..0x156e08 (+ the
// linkonce bodies 0x2ae7a0..0x2afd10). From MioPan's eeFindFile.cpp, checked
// against FF3: the class gained a vptr in front (every member moves up 4),
// stricmp is an inlined copy + toupper + strcmp, IsMatchSpec splits the name
// with FileSplitPath and gained a "prefix*.ext" rule, ScanDirectory sorts the
// matched files by name, and the containers are cleared with erase. No FF3 code
// constructs a CFindFile (ghidra.py refs: no jal / j / data word to either
// ctor), so it is reconstructed for completeness and never runs.

#include "graphics/graph3d/eeFindFile.h"

#include <algorithm>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>                     // abs
#include <string.h>

#include "graphics/graph3d/eeFile.h"    // FileSplitPath

// The inlined stricmp: both strings copied into 256-byte buffers, folded with
// toupper (0x2a5dd8), then strcmp. PC guard: the copies are bounded (the
// EE uses strcpy; every caller's strings fit 256 bytes there).
static int IsSameNoCase(const char *pA, const char *pB)
{
    char  acA[256];
    char  acB[256];
    char *p;

    snprintf(acA, sizeof(acA), "%s", pA);
    snprintf(acB, sizeof(acB), "%s", pB);

    for (p = acA; *p != '\0'; p++)
    {
        *p = (char)toupper((unsigned char)*p);
    }
    for (p = acB; *p != '\0'; p++)
    {
        *p = (char)toupper((unsigned char)*p);
    }

    return strcmp(acA, acB) == 0;
}

// m_vMatchFile's order: std::sort by d_name (the introsort loop 0x2aec88, the
// final insertion sort 0x2aee58 and their helpers compare with strcmp of
// d_name, sce_dirent +0x40).
static bool IsDirentNameLess(const sce_dirent &a, const sce_dirent &b)
{
    return strcmp(a.d_name, b.d_name) < 0;
}

// Every member starts empty; the current directory is erased once more (FF3
// keeps the erase, FF2 has no body). FF3 0x00155730 is the base-object twin.
// FF3 0x00155660
// callcheck: vector_dirent_ctor, replace (the vectors' constructors; std::string::erase)
CFindFile::CFindFile()
{
    m_strCurrentDirectory.erase();
}

// Vectors, then the strings, in reverse member order. Base-object twin 0x1557f8,
// deleting 0x155b88.
// FF3 0x001559c0
// callcheck: dirent_destroy_range, vector_dirent_deallocate, string_destroy_range, vector_string_deallocate, string_Rep_M_destroy
// (the member vectors' and strings' destructors, implicit)
CFindFile::~CFindFile()
{
}

// "." and ".." never match. The spec splits on its first '.' into file / ext
// and the name into file / ext by FileSplitPath; then a match is: the whole
// name (no case), "*" or "*.*", "file.*" with the same file, "*.ext" with the
// same ext, or (FF3 only) a spec containing "*.ext" with the same ext whose
// text before the first '(', '.', '|' or '*' occurs in the file part. File
// comparisons ignore case; the "*" checks do not.
// FF3 0x00155d58
// callcheck: strcpy, toupper, strcmp, memset, sscanf, FileSplitPath, sprintf, strstr
// (strcpy + toupper + strcmp are IsSameNoCase; sprintf is snprintf on the host)
int CFindFile::IsMatchSpec(const char *pFileName, const char *strFileSpec)
{
    // PC guard: the two dot buffers are 256 bytes here, 2 on the EE, where a
    // run of dots ("a..b") overflows them.
    char strSrcFile[256];
    char strSrcDot[256];
    char strSrcExt[256];
    char strSpecFile[256];
    char strSpecDot[256];
    char strSpecExt[256];
    char strTemp[256];
    char strExtSpec[256];
    char strPrefix[256];

    if (IsSameNoCase(pFileName, "."))
    {
        return 0;
    }

    if (IsSameNoCase(pFileName, ".."))
    {
        return 0;
    }

    memset(strSrcFile, 0, sizeof(strSrcFile));
    memset(strSrcDot, 0, sizeof(strSrcDot));
    memset(strSrcExt, 0, sizeof(strSrcExt));
    memset(strSpecFile, 0, sizeof(strSpecFile));
    memset(strSpecDot, 0, sizeof(strSpecDot));
    memset(strSpecExt, 0, sizeof(strSpecExt));

    sscanf(strFileSpec, "%[^.]%[.]%s", strSpecFile, strSpecDot, strSpecExt);

    FileSplitPath(pFileName, nullptr, nullptr, strSrcFile, strSrcExt);

    // Drop the '.' of ".ext". The EE scans strSrcExt into itself; the host
    // scans a copy (overlapping sscanf buffers are undefined).
    memcpy(strTemp, strSrcExt, sizeof(strTemp));
    sscanf(strTemp, "%[.]%s", strSrcDot, strSrcExt);

    if (IsSameNoCase(strFileSpec, pFileName))
    {
        return 1;
    }

    if (strcmp(strFileSpec, "*") == 0)
    {
        return 1;
    }

    if (strcmp(strFileSpec, "*.*") == 0)
    {
        return 1;
    }

    // "file.*"
    if (strcmp(strSpecExt, "*") == 0)
    {
        if (IsSameNoCase(strSpecFile, strSrcFile))
        {
            return 1;
        }
    }

    // "*.ext"
    if (strcmp(strSpecFile, "*") == 0)
    {
        if (IsSameNoCase(strSpecExt, strSrcExt))
        {
            return 1;
        }
    }

    // "prefix*.ext": the extension must match, then the prefix must occur in
    // the file part.
    memset(strExtSpec, 0, sizeof(strExtSpec));
    memset(strPrefix, 0, sizeof(strPrefix));
    snprintf(strExtSpec, sizeof(strExtSpec), "*.%s", strSpecExt);
    if (strstr(strFileSpec, strExtSpec) != nullptr)
    {
        if (!IsSameNoCase(strSpecExt, strSrcExt))
        {
            return 0;
        }

        sscanf(strFileSpec, "%[^(.|*)]*.%s", strPrefix, strSpecExt);
        if (strstr(strSrcFile, strPrefix) != nullptr)
        {
            return 1;
        }
    }

    return 0;
}

// Empty the spec list, then push each '|'-separated piece. 0 for a null spec.
// FF3 0x00156200
// callcheck: string_dtor, memset, sscanf, string_ctor_cstr2, vector_string_M_insert_aux, string_ctor_copy, string_Rep_M_destroy, strlen
// (vector<string>::clear / push_back and the temporary string)
int CFindFile::AnalyzeSpec(const char *pFileSpec)
{
    char strSpec[256];
    char strOR[256];                    // PC guard: 2 bytes on the EE ("a||b" overflows it)

    if (pFileSpec == nullptr)
    {
        return 0;
    }

    m_vstrFileSpec.clear();

    for (; *pFileSpec != '\0'; pFileSpec += strlen(strSpec) + strlen(strOR))
    {
        memset(strSpec, 0, sizeof(strSpec));
        memset(strOR, 0, sizeof(strOR));

        if (sscanf(pFileSpec, "%[^|]%[|]", strSpec, strOR) < 1)
        {
            break;
        }

        m_vstrFileSpec.push_back(std::string(strSpec));
    }

    return 1;
}

// FF3 0x00156388
int CFindFile::IsDirectory(const sce_dirent *pDE)
{
    return (pDE->d_stat.st_mode & 0xf000) == 0x1000;
}

// Read the current directory into m_vDirEntry, then: every non-directory
// entry matching a spec (once per matching spec) into m_vMatchFile, sorted by
// name; every directory into m_vSubDirectoryAll and, but "." / "..", into
// m_vSubDirectory. 1, or 0 after OnError when the open or a read fails (the
// lists are emptied first either way).
// FF3 0x001563a0
// callcheck: sceDopen, dirent_copy, dirent_destroy_range2, abs, strerror, FindFileOnError, sceDread, vector_dirent_M_insert_aux, sceDclose, IsMatchSpec, IsDirectory, dirent_introsort_loop, dirent_final_insertion_sort, strcpy, toupper, strcmp
// (vector clear / push_back, std::sort with IsDirentNameLess, IsSameNoCase)
int CFindFile::ScanDirectory()
{
    int        iFD;
    int        iRet;
    int        bRet;
    int        i;
    int        j;
    sce_dirent de;
    const char *pSpec;

    iFD = sceDopen(m_strCurrentDirectory.c_str());

    m_vDirEntry.clear();
    m_vMatchFile.clear();
    m_vSubDirectory.clear();
    m_vSubDirectoryAll.clear();

    if (iFD < 0)
    {
        return OnError("%s:%s", strerror(abs(iFD)), m_strCurrentDirectory.c_str());
    }

    bRet = 1;
    while ((iRet = sceDread(iFD, &de)) != 0)
    {
        if (iRet < 0)
        {
            bRet = OnError(strerror(abs(iRet)));
            break;
        }

        m_vDirEntry.push_back(de);
    }

    sceDclose(iFD);

    for (i = 0; i < (int)m_vstrFileSpec.size(); i++)
    {
        pSpec = m_vstrFileSpec[i].c_str();

        for (j = 0; j < (int)m_vDirEntry.size(); j++)
        {
            if (IsMatchSpec(m_vDirEntry[j].d_name, pSpec) && !IsDirectory(&m_vDirEntry[j]))
            {
                m_vMatchFile.push_back(m_vDirEntry[j]);
            }
        }
    }

    std::sort(m_vMatchFile.begin(), m_vMatchFile.end(), IsDirentNameLess);

    for (i = 0; i < (int)m_vDirEntry.size(); i++)
    {
        if (IsDirectory(&m_vDirEntry[i]))
        {
            m_vSubDirectoryAll.push_back(m_vDirEntry[i]);

            if (!IsSameNoCase(m_vDirEntry[i].d_name, ".") && !IsSameNoCase(m_vDirEntry[i].d_name, ".."))
            {
                m_vSubDirectory.push_back(m_vDirEntry[i]);
            }
        }
    }

    return bRet;
}

// FF3 0x00156a70
// callcheck: FindFileSetDirectory, FindFileSetFileSpec, ScanDirectory
int CFindFile::Scan(const char *pDirName, const char *pFileSpec)
{
    SetDirectory(pDirName);
    SetFileSpec(pFileSpec);
    return ScanDirectory();
}

// std::string::assign(const char *) inlined (length check, then copy in place
// or _M_replace_safe).
// FF3 0x00156ab0
// callcheck: strlen, __throw_length_error, string_M_replace_safe2, memmove, memcpy (std::string::assign)
void CFindFile::SetDirectory(const char *pDirName)
{
    m_strCurrentDirectory = pDirName;
}

// FF3 0x00156bd0
// callcheck: AnalyzeSpec
void CFindFile::SetFileSpec(const char *pFileSpec)
{
    AnalyzeSpec(pFileSpec);
}

// FF3 0x00156be8
// callcheck: ScanDirectory
int CFindFile::Scan()
{
    return ScanDirectory();
}

// Everything back to empty, the error message last.
// FF3 0x00156c00
// callcheck: replace, string_copy, string_destroy_range2, dirent_copy, dirent_destroy_range2
// (std::string::erase, vector::clear)
void CFindFile::Clear()
{
    m_strOriginDirectory.erase();
    m_strCurrentDirectory.erase();
    m_vstrFileSpec.clear();
    m_vDirEntry.clear();
    m_vMatchFile.clear();
    m_vSubDirectory.clear();
    m_vSubDirectoryAll.clear();
    m_strError.erase();
}

// Format the message (256 bytes on the EE) into the error string. Always 0, so
// a failing member can return OnError(...).
// FF3 0x002af9f0
// callcheck: vsprintf, strlen, string_assign_cstr_n (vsnprintf on the host, bounded to the EE buffer; std::string::assign)
int CFindFile::OnError(const char *pStr, ...)
{
    char    str[256];
    va_list args;

    va_start(args, pStr);
    vsnprintf(str, sizeof(str), pStr, args);
    va_end(args);

    m_strError.assign(str, strlen(str));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The libstdc++ instances FF3 emitted for this file; on the host they are
// std::vector / std::sort / std::string's own code, nothing to reconstruct.
// FF3's allocator here is NullFuncRet0 (allocate) / NullFunc (deallocate), so
// on the EE a push_back into an empty vector would write through null.
//   FF3 0x002ae7a0 vector_string_M_insert_aux       vector<string>::_M_insert_aux
//   FF3 0x002ae970 string_ctor_copy                 basic_string(const basic_string &)
//   FF3 0x002ae9c8 vector_dirent_M_insert_aux       vector<sce_dirent>::_M_insert_aux
//   FF3 0x002aec88 dirent_introsort_loop            std::__introsort_loop (by d_name)
//   FF3 0x002aee58 dirent_final_insertion_sort      std::__final_insertion_sort
//   FF3 0x002aef30 string_uninitialized_copy        __uninitialized_copy_aux<string *>
//   FF3 0x002aef98 __uninitialized_copy_aux<sce_dirent*,sce_dirent*>
//   FF3 0x002af000 dirent_partial_sort              std::partial_sort
//   FF3 0x002af1f0 dirent_unguarded_partition       std::__unguarded_partition
//   FF3 0x002af368 dirent_insertion_sort            std::__insertion_sort
//   FF3 0x002af498 dirent_unguarded_linear_insert   std::__unguarded_linear_insert
//   FF3 0x002af5b8 dirent_sort_heap                 std::sort_heap
//   FF3 0x002af6e0 dirent_adjust_heap               std::__adjust_heap
//   FF3 0x002af888 dirent_push_heap                 std::__push_heap
//   FF3 0x002afa58 vector_string_ctor               _Vector_base<string>()
//   FF3 0x002afa68 vector_string_deallocate         _M_deallocate
//   FF3 0x002afa90 string_destroy_range             std::_Destroy(string *, string *)
//   FF3 0x002afad8 vector_dirent_ctor               _Vector_base<sce_dirent>()
//   FF3 0x002afae8 vector_dirent_deallocate         _M_deallocate
//   FF3 0x002afb10 dirent_destroy_range             std::_Destroy(sce_dirent *, sce_dirent *) (empty)
//   FF3 0x002afb40 string_copy                      std::copy(string *)
//   FF3 0x002afba0 string_destroy_range2            std::_Destroy(string *, string *)
//   FF3 0x002afbe8 dirent_copy                      std::copy(sce_dirent *)
//   FF3 0x002afc60 dirent_destroy_range2            std::_Destroy(sce_dirent *, sce_dirent *) (empty)
//   FF3 0x002afc98 dirent_copy_backward             std::copy_backward(sce_dirent *)
//   FF3 0x002abdd8 string_Rep_M_destroy             basic_string::_Rep::_M_destroy (emitted with
//                                                   CDebugEdit's instances; deallocate = NullFunc)
