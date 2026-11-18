/* ==========================================================================
 *  album/prg/album.h
 *
 *  FF3 0x1211f0..0x121cd8: the album's data.
 *    - the slot infos (bss 0x390560, five): what the album save/load screens
 *      list for the albums on a card (date, name kind, photo count), and
 *      the current info of each album side (0x3905b0, two);
 *    - the two album sides (0x3905d0, 0x20c each): FF2's PFILE_WRK (sixteen
 *      PICTURE_WRK) plus the side's kind at +0x208 (0: the camera's own
 *      album, 1: an album file); side 0 is the camera's album when the album
 *      is opened in game (AlbumSetIngame), whose snapshot 0x3909e8 is taken
 *      on opening and written back by AlbumEditEnd;
 *    - the photo pixel buffers of the sides (sdata 0x372c08: 16 x 0xd360
 *      from +0x10000, 16 x 0x1000 thumbnails from +0);
 *    - photo helpers: search, copy, erase, protect, sort.
 *  FF3-only file in FF2's album/prg directory (FF2's camera album lived in
 *  ingame/photo/photo.c). Was "outgame/SpriteCmn.c" up to 0x1215c0 (link-
 *  order guess).
 * ======================================================================== */
#ifndef _ALBUM_PRG_ALBUM_H
#define _ALBUM_PRG_ALBUM_H

#include <stddef.h>                     // offsetof

#include "ingame/photo/photo.h"         // PFILE_WRK, PICTURE_WRK
#include "sdk/scetypes.h"

#define ALBUM_SIDE_NUM      2           // album sides (0x3905d0)
#define ALBUM_PHOTO_MAX     PHOTO_FILE_MAX
#define ALBUM_SLOT_NUM      5           // albums on a card (slot infos 0x390560)

#define ALBUM_KIND_CAMERA   0           // ALBUM_FILE.kind: the camera's own album
#define ALBUM_KIND_FILE     1           // an album file (card or empty)

#define ALBUM_PROTECT_MAX   15          // protected photos per album

// Photo pixels in a side's buffer: picture adr_no at ALBUM_PHOTO_PIX_OFS +
// adr_no * ALBUM_PHOTO_PIX_SIZE, its thumbnail at adr_no * ALBUM_PHOTO_THUMB_SIZE.
#define ALBUM_PHOTO_PIX_OFS     0x10000
#define ALBUM_PHOTO_PIX_SIZE    0xd360
#define ALBUM_PHOTO_THUMB_SIZE  0x1000
#define ALBUM_PHOTO_BUF_SIZE    0xe8000

// One album side (0x20c).
struct ALBUM_FILE
{
    /* 0x000 */ PFILE_WRK file;
    /* 0x208 */ s_char    kind;         // ALBUM_KIND_*; AlbumInfoGetKind
    /* 0x209 */ u_char    _209[3];
};
static_assert(sizeof(ALBUM_FILE) == 0x20c, "ALBUM_FILE is 0x20c bytes");
static_assert(offsetof(ALBUM_FILE, kind) == 0x208, "ALBUM_FILE.kind");

// An album's info for the save/load lists (0x10).
struct ALBUM_INFO
{
    /* 0x00 */ int    date[3];          // year, month, day (CdClockReadInt) when saved; -1 after a clear
    /* 0x0c */ s_char kind;             // the album's name message / cover (0 after a clear)
    /* 0x0d */ s_char photo_num;        // photos in it (-1 after a clear)
    /* 0x0e */ u_char _0e[2];
};
static_assert(sizeof(ALBUM_INFO) == 0x10, "ALBUM_INFO is 0x10 bytes");
static_assert(offsetof(ALBUM_INFO, kind) == 0x0c, "ALBUM_INFO.kind");
static_assert(offsetof(ALBUM_INFO, photo_num) == 0x0d, "ALBUM_INFO.photo_num");

// The 8 bytes at sdata 0x372c00 the game save keeps (AlbumIngameSaveGet): the
// flag and the pad word after it.
struct ALBUM_INGAME_SAVE
{
    /* 0x0 */ int flag;                 // AlbumIngameFlagGet: 1 outside the game
    /* 0x4 */ int _04;
};
static_assert(sizeof(ALBUM_INGAME_SAVE) == 0x8, "ALBUM_INGAME_SAVE is 0x8 bytes");

extern ALBUM_INFO album_slot_info[ALBUM_SLOT_NUM];      // bss 0x390560
extern ALBUM_INFO album_cur_info[ALBUM_SIDE_NUM];       // bss 0x3905b0
extern ALBUM_FILE album_file[ALBUM_SIDE_NUM];           // bss 0x3905d0
extern PFILE_WRK  album_cam_snapshot;                   // bss 0x3909e8
extern u_char    *album_photo_buf[ALBUM_SIDE_NUM];      // sdata 0x372c08 (EE addresses there)
extern ALBUM_INGAME_SAVE album_ingame_save;            // sdata 0x372c00 (saved whole: AlbumIngameSaveGet)
extern s_char     album_disp_ctrl[2];                   // sdata 0x372c11: side, no of the photo the decoder holds (-1: none)

// ---- slot infos ----
void   AlbumCurInfoStamp(void);         // FF3 0x001211f0: clock + kind + photo count into the current side's info
void   AlbumSlotInfoClear(int no);      // FF3 0x00121268
void   AlbumSlotInfoClearAll(void);     // FF3 0x001212a8
void   AlbumSlotInfoSetCur(int no);     // FF3 0x001212e0: slot no = the current side's info
void   AlbumCurInfoSetSlot(int no);     // FF3 0x00121330: the current side's info = slot no
const int *AlbumSlotInfoGet(int no);    // FF3 0x00121380: slot no's date words
s_char AlbumSlotInfoGetKind(int no);    // FF3 0x00121398: + 0x82d is its cover picture
s_char AlbumSlotInfoGetPhotoNum(int no);    // FF3 0x001213b0

// ---- opened in game or from the title ----
void   AlbumIngameFlagClear(void);      // FF3 0x001213c8: IngameInit
void   AlbumSetIngame(int in_game, int arg);    // FF3 0x001213d0: title 0, pause / save point 1
void   AlbumEditInit(void);             // FF3 0x00121410
void   AlbumEditEnd(void);              // FF3 0x00121528
u_char AlbumIsIngame(void);             // FF3 0x00121588
void   AlbumIngameFlagSet(void);        // FF3 0x00121590
int    AlbumIngameFlagGet(void);        // FF3 0x001215a8: 1 outside the game

// ---- sides and photos ----
ALBUM_FILE  *AlbumGetPFile(s_char side);            // FF3 0x001215c0
s_char AlbumInfoGetKind(s_char side);               // FF3 0x001215e8: < 0 no album
void   AlbumInfoSetKind(s_char side, s_char kind);  // FF3 0x00121610: -1 clears the side (then kind 1)
u_char *AlbumGetPhotoBuf(s_char side);              // FF3 0x001216b8
int    AlbumInfoIsCamera(s_char side);              // FF3 0x001216d8
u_char AlbumGetPhotoNum(s_char side);               // FF3 0x00121700
void   AlbumSetPhotoNum(s_char side, u_char num);   // FF3 0x00121728
PICTURE_WRK *AlbumGetPhoto(s_char side, s_char no); // FF3 0x00121758
u_char AlbumPhotoGetAdrNo(s_char side, s_char no);  // FF3 0x00121798
int    AlbumPhotoIsFlag3(s_char side, s_char no);   // FF3 0x001217c8: byte +3 != 0
int    AlbumPhotoIsUsed(s_char side, s_char no);    // FF3 0x001217f8: status bit 0
int    AlbumSearchUsedPhoto(s_char side);           // FF3 0x00121828: first used, -1 none
int    AlbumSearchFreePhoto(s_char side);           // FF3 0x00121890: first free, -1 none
int    AlbumPhotoIsProtected(s_char side, s_char no); // FF3 0x001218f8: status bit 1
int    AlbumPhotoCopy(s_char src_side, s_char src_no, s_char dst_side, s_char dst_no); // FF3 0x00121930
int    AlbumPhotoErase(s_char side, s_char no);     // FF3 0x00121ad8: not when protected
int    AlbumSortPhoto(s_char side, s_char key, int order); // FF3 0x00121b70: key 0 time, 1 score, 2 protect
int    AlbumPhotoProtectToggle(s_char side, s_char no); // FF3 0x00121c08
void   AlbumDispCtrlInit(void);                     // FF3 0x00121cc8: gp-0x8df / -0x8de = -1

#endif /* _ALBUM_PRG_ALBUM_H */
