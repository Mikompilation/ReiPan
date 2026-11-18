#ifndef REIPAN_PATHS_H
#define REIPAN_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Where everything lives.
 *
 * The port needs three unrelated kinds of file and the only thing they have in
 * common is that none of them is "next to the executable":
 *
 *   USER    what the port writes -- reipan.ini, memcard/, logs, debug dumps.
 *           SDL_GetPrefPath("Mikompilation", "ReiPan"), i.e.
 *             %APPDATA%\Mikompilation\ReiPan\            (Windows)
 *             ~/.local/share/Mikompilation/ReiPan/       (Linux, XDG)
 *             ~/Library/Application Support/Mikompilation/ReiPan/  (macOS)
 *           Deliberately NOT the executable's directory: an installed copy
 *           lives somewhere the player cannot write (Program Files, /usr/bin,
 *           a signed .app bundle), and a build tree is the only layout where
 *           writing beside the binary happens to work.
 *
 *   DATA    the game's own files -- ZERO3.FHD beside IMG_BD.BIN or an extracted
 *           bin/data tree, or a disc image holding them, which is read straight
 *           out of the image.  Set by `[paths] data_folder` in reipan.ini so it
 *           can point anywhere, which is the only practical answer on a phone or
 *           a console where the data is not next to the binary and cannot be.
 *           Auto-discovered when the key is empty, and the discovered value is
 *           written back so the file always shows where the game is reading
 *           from.  A player with none of it is asked for it before the game
 *           starts (reipan_rml_gamefiles.h).
 *
 *   ASSETS  the port's own shipped resources/ -- compiled shader bytecode and
 *           gamecontrollerdb.txt.  Read-only, installed with the executable,
 *           and on Android read straight out of the APK.
 *
 * Every path the game asks for goes through one of the three Resolve calls
 * below.  There is no fourth mechanism and nothing else calls SDL_GetBasePath.
 */

/* The three roots, each with a trailing separator.  Never NULL: a root that
 * cannot be determined falls back to "./" so the caller still has something to
 * open rather than a special case to handle. */
const char *ReiPan_PathUserDir(void);
const char *ReiPan_PathDataDir(void);
const char *ReiPan_PathAssetsDir(void);

/* Which of the sources the data root came from, for the settings UI to show:
 * "reipan.ini", "disc image", "REIPAN_DATA_DIR", "found automatically" or "not
 * found".  Never NULL; resolves the root if that has not happened yet.  With a
 * disc image the root is the folder the image is in. */
const char *ReiPan_PathDataDirSource(void);

/* Does `dir` hold the game's files -- ZERO3.FHD, which nothing starts without,
 * beside IMG_BD.BIN or an extracted bin/data or bin2/data tree, each file with
 * or without the ";1" it has when copied straight off the disc -- or is it a
 * disc image holding them?  This is the same test discovery uses, exposed so the
 * settings UI can tell the player a folder is wrong while they are choosing it
 * rather than after the next restart.  Returns 1 if it does. */
int ReiPan_PathLooksLikeDataDir(const char *dir);

/*
 * Would the data root have anything to read?  Whether the setting holds the game
 * or discovery would find it, without deciding the root: that is still decided
 * once, the first time it is asked for, and this is what lets a player be asked
 * first.  A REIPAN_DATA_DIR counts whatever it names: it is a one-off by intent,
 * and nothing chosen now would outrank it.  Returns 1 if so.
 */
int ReiPan_PathHasGameFiles(void);

/* What came of ReiPan_PathUseGameFile(). */
#define REIPAN_GAMEFILE_OK            0
#define REIPAN_GAMEFILE_UNREADABLE    1 /* not there, or not readable              */
#define REIPAN_GAMEFILE_NOT_AN_IMAGE  2 /* no ISO 9660 volume in it                */
#define REIPAN_GAMEFILE_NOT_THE_GAME  3 /* a disc image without the game's files   */
#define REIPAN_GAMEFILE_CUT_SHORT     4 /* the game's files run past the end of it */
#define REIPAN_GAMEFILE_OTHER_RELEASE 5 /* ZERO3.FHD is not the table of SLES-53825 */
#define REIPAN_GAMEFILE_NO_TABLE      6 /* IMG_BD.BIN without ZERO3.FHD beside it  */

/*
 * The file a player chose as their copy of the game: a disc image, or IMG_BD.BIN,
 * which saves the folder it is in.
 *
 * Checks it the whole way, down to whether the file table is the one this port's
 * file numbers come from and fits the archive beside it, and on REIPAN_GAMEFILE_OK
 * saves it in [paths] data_folder.  Only before the data root is decided: that is
 * the game-files screen, ahead of the game.
 */
int ReiPan_PathUseGameFile(const char *path);

/* Decide the data root now rather than on the first file read, so the log names
 * where the game's files come from (or that they are missing) before anything
 * tries to load one, and so a disc image's files are mapped before there is a
 * reader to race.  Once the game-files screen has had its say. */
void ReiPan_PathLogDataRoot(void);

/*
 * Resolve a PS2-style game-data path ("host0:\dir\file;1", "../bin/data/x.pk2")
 * against the data root: strip the device prefix, leading slashes and the
 * ";version" suffix, turn '\' into '/', then look under the root.  A leading
 * "../" is dropped if the direct join misses, and "bin/" is retried as "bin2/",
 * because both extracted-tree layouts are in circulation.  Each candidate is
 * also tried with the ISO ";1" suffix kept on the host file name ("ZERO3.FHD;1"),
 * which is how a disc copied file by file arrives.
 *
 * Playing from a disc image, the files in the disc's root come back as names that
 * only reipan_file can open, and nothing else is looked for beside the image.
 *
 * Returns 1 with `out` filled, 0 if the file is not there.
 */
int ReiPan_PathResolveData(const char *ps2_name, char *out, size_t out_size);

/* Resolve a shipped resource ("resources/shaders/spirv/mesh.vert.spv").
 * Returns 1 on success, 0 when the file is missing from the install. */
int ReiPan_PathResolveAsset(const char *relative, char *out, size_t out_size);

/*
 * Build a path under the user directory.  `relative` is taken as relative: any
 * device prefix, leading separator or ".." component is discarded, so a caller
 * cannot escape the user directory with a name the ROM handed it.  The file need
 * not exist and nothing is created.  Returns 1 on success, 0 only if the name is
 * empty or the result does not fit.
 */
int ReiPan_PathResolveUser(const char *relative, char *out, size_t out_size);

/*
 * As above, and additionally create the parent directories so a writer can just
 * open the result.  This is the one to use before writing; the plain resolve is
 * for reading, where creating a directory to then find nothing in it would leave
 * a mirror of every path that missed.
 */
int ReiPan_PathPrepareUser(const char *relative, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* REIPAN_PATHS_H */
