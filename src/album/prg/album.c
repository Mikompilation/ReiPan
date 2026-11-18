/* ==========================================================================
 *  album/prg/album.c
 *
 *  The album's data: card slot infos, the two album sides with their photo
 *  records and pixel buffers, and the photo operations the album edit
 *  pages use (search, copy, erase, protect, sort). Opened in game, side 0
 *  is the camera's own album: AlbumSetIngame snapshots it, AlbumEditInit
 *  puts the snapshot on side 0, AlbumEditEnd writes side 0 back.
 *  FF3 0x1211f0..0x121cd8, FF3's own code (FF2 kept the camera album in
 *  ingame/photo/photo.c; the sorts are still there in FF3, 0x22d130..).
 * ======================================================================== */
#include "album/prg/album.h"

#include <string.h>                                     // memcpy, memset

#include "album/prg/album_disp.h"                       // AlbumDispGetCurAlbum
#include "album/prg/album_edit.h"                       // album_open_arg (sdata 0x36b920, album_edit.c's by sdata order)
#include "common/cmn_time.h"                            // CdClockReadInt, CLOCK_DATE_NUM
#include "ingame/photo/photo_file.h"                    // CopyPFileWrk, GetCamPhotoFile, SortPhotoData_*
#include "reipan/reipan_memory.h"                       // PC: ReiPan_GetHostPointer
#include "system/b2d/BpCtl.h"                           // BpArenaAlloc

// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

// Side 0's pixel buffer in game (EE address; side 1 takes 0xe8000 of the Bp
// arena).
#define ALBUM_CAM_PHOTO_BUF     0x1207000

ALBUM_INFO album_slot_info[ALBUM_SLOT_NUM];             // bss 0x390560
ALBUM_INFO album_cur_info[ALBUM_SIDE_NUM];              // bss 0x3905b0
ALBUM_FILE album_file[ALBUM_SIDE_NUM];                  // bss 0x3905d0
PFILE_WRK  album_cam_snapshot;                          // bss 0x3909e8
u_char    *album_photo_buf[ALBUM_SIDE_NUM];             // sdata 0x372c08
ALBUM_INGAME_SAVE album_ingame_save;                    // sdata 0x372c00 (AlbumIngameSaveGet)

static u_char album_in_game;                            // sdata 0x372c10 (gp-0x8e0)
s_char        album_disp_ctrl[2];                       // sdata 0x372c11 / 0x372c12 (album_disp.c)

// ──────────────────────────────────────────────────────────────────────
// Stamp the current side's info: the clock, its kind and photo count
// (before a save).
// FF3 0x001211f0.

void AlbumCurInfoStamp(void)
{
    int         date[CLOCK_DATE_NUM];   // the full date; the info keeps year, month, day
    s_char      cur;
    ALBUM_INFO *info;

    CdClockReadInt(date);
    cur  = AlbumDispGetCurAlbum();
    info = &album_cur_info[cur];
    info->date[0]   = date[0];
    info->date[1]   = date[1];
    info->date[2]   = date[2];
    info->kind      = AlbumInfoGetKind(cur);
    info->photo_num = AlbumGetPhotoNum(cur);
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00121268.

void AlbumSlotInfoClear(int no)
{
    ALBUM_INFO *info = &album_slot_info[no];

    info->date[0]   = -1;
    info->photo_num = -1;
    info->date[1]   = -1;
    info->date[2]   = -1;
    info->kind      = 0;
}

// FF3 0x001212a8.

void AlbumSlotInfoClearAll(void)
{
    int i;

    for (i = 0; i < ALBUM_SLOT_NUM; i++)
        AlbumSlotInfoClear(i);
}

// FF3 0x001212e0.

void AlbumSlotInfoSetCur(int no)
{
    memcpy(&album_slot_info[no], &album_cur_info[AlbumDispGetCurAlbum()], sizeof(ALBUM_INFO));
}

// FF3 0x00121330.

void AlbumCurInfoSetSlot(int no)
{
    memcpy(&album_cur_info[AlbumDispGetCurAlbum()], &album_slot_info[no], sizeof(ALBUM_INFO));
}

// FF3 0x00121380.

const int *AlbumSlotInfoGet(int no)
{
    return album_slot_info[no].date;
}

// FF3 0x00121398.

s_char AlbumSlotInfoGetKind(int no)
{
    return album_slot_info[no].kind;
}

// FF3 0x001213b0.

s_char AlbumSlotInfoGetPhotoNum(int no)
{
    return album_slot_info[no].photo_num;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001213c8.

void AlbumIngameFlagClear(void)
{
    album_ingame_save.flag = 0;
}

// Before the album opens: in game (the pause menu, the save point) take a
// snapshot of the camera album for side 0.
// FF3 0x001213d0.

void AlbumSetIngame(int in_game, int arg)
{
    album_in_game = (u_char)in_game;
    if (album_in_game != 0)
        CopyPFileWrk(&album_cam_snapshot);
    album_open_arg = arg;
}

// Both sides empty (each record on its own pixel slot), side 0 the camera
// album's snapshot in game, and the pixel buffers.
// FF3 0x00121410.

void AlbumEditInit(void)
{
    s_char side;
    s_char i;
    u_int  addr;

    for (side = 0; side < ALBUM_SIDE_NUM; side++)
    {
        memset(&album_file[side], 0, sizeof(ALBUM_FILE));
        for (i = 0; i < ALBUM_PHOTO_MAX; i++)
            album_file[side].file.pic[i].adr_no = i;
    }

    album_file[0].kind = (album_in_game == 0) ? ALBUM_KIND_FILE : ALBUM_KIND_CAMERA;
    album_file[1].kind = ALBUM_KIND_FILE;
    if (album_in_game != 0)
        album_file[0].file = album_cam_snapshot;

    // PC: the sdata table holds EE addresses in FF3.
    album_photo_buf[0] = (u_char *)ReiPan_GetHostPointer(ALBUM_CAM_PHOTO_BUF);
    addr = BpArenaAlloc(ALBUM_PHOTO_BUF_SIZE);
    album_photo_buf[1] = addr != 0 ? (u_char *)ReiPan_GetHostPointer(addr) : nullptr;
}

// In game, side 0 goes back to the camera album, packed.
// FF3 0x00121528.

void AlbumEditEnd(void)
{
    PFILE_WRK *cam;

    if (album_in_game != 0)
    {
        cam = GetCamPhotoFile();
        memcpy(cam, &album_file[0].file, sizeof(PFILE_WRK));
        SortPhotoData_Before(cam);
    }
    album_photo_buf[0] = nullptr;
    album_photo_buf[1] = nullptr;
}

// FF3 0x00121588.

u_char AlbumIsIngame(void)
{
    return album_in_game;
}

// FF3 0x00121590.

void AlbumIngameFlagSet(void)
{
    if (album_in_game != 0)
        album_ingame_save.flag = 1;
}

// FF3 0x001215a8.

int AlbumIngameFlagGet(void)
{
    if (album_in_game == 1)
        return album_ingame_save.flag;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x001215c0.

ALBUM_FILE *AlbumGetPFile(s_char side)
{
    return &album_file[side];
}

// FF3 0x001215e8.

s_char AlbumInfoGetKind(s_char side)
{
    return album_file[side].kind;
}

// FF3 0x00121610.

void AlbumInfoSetKind(s_char side, s_char kind)
{
    s_char i;

    album_file[side].kind = kind;
    if (kind == -1)
    {
        memset(&album_file[side], 0, sizeof(ALBUM_FILE));
        for (i = 0; i < ALBUM_PHOTO_MAX; i++)
            album_file[side].file.pic[i].adr_no = i;
        album_file[side].kind = ALBUM_KIND_FILE;
    }
}

// FF3 0x001216b8.

u_char *AlbumGetPhotoBuf(s_char side)
{
    return album_photo_buf[side];
}

// FF3 0x001216d8; was SpCmnTexMemReleaseSub (wrong FF2 match).

int AlbumInfoIsCamera(s_char side)
{
    return AlbumInfoGetKind(side) == ALBUM_KIND_CAMERA;
}

// FF3 0x00121700.

u_char AlbumGetPhotoNum(s_char side)
{
    return AlbumGetPFile(side)->file.pic_num;
}

// FF3 0x00121728.

void AlbumSetPhotoNum(s_char side, u_char num)
{
    AlbumGetPFile(side)->file.pic_num = num;
}

// FF3 0x00121758.

PICTURE_WRK *AlbumGetPhoto(s_char side, s_char no)
{
    return &AlbumGetPFile(side)->file.pic[no];
}

// FF3 0x00121798.

u_char AlbumPhotoGetAdrNo(s_char side, s_char no)
{
    return AlbumGetPhoto(side, no)->adr_no;
}

// FF3 0x001217c8.

int AlbumPhotoIsFlag3(s_char side, s_char no)
{
    return AlbumGetPhoto(side, no)->mode != 0;
}

// FF3 0x001217f8.

int AlbumPhotoIsUsed(s_char side, s_char no)
{
    return AlbumGetPhoto(side, no)->status & PICTURE_STATUS_USED;
}

// FF3 0x00121828; was acsSetRopeCollision (wrong FF2 match).

int AlbumSearchUsedPhoto(s_char side)
{
    s_char i;

    for (i = 0; i < ALBUM_PHOTO_MAX; i++)
    {
        if (AlbumPhotoIsUsed(side, i))
            return i;
    }
    return -1;
}

// FF3 0x00121890; was acsDelRopeCollision (wrong FF2 match).

int AlbumSearchFreePhoto(s_char side)
{
    s_char i;

    for (i = 0; i < ALBUM_PHOTO_MAX; i++)
    {
        if (!AlbumPhotoIsUsed(side, i))
            return i;
    }
    return -1;
}

// FF3 0x001218f8.

int AlbumPhotoIsProtected(s_char side, s_char no)
{
    return (AlbumGetPhoto(side, no)->status & PICTURE_STATUS_PROTECT) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Copy a used photo (pixels, thumbnail and record) onto another record;
// the record keeps its own pixel slot. 1 when copied.
// FF3 0x00121930.

int AlbumPhotoCopy(s_char src_side, s_char src_no, s_char dst_side, s_char dst_no)
{
    PICTURE_WRK *src = AlbumGetPhoto(src_side, src_no);
    PICTURE_WRK *dst = AlbumGetPhoto(dst_side, dst_no);
    u_char      *src_buf;
    u_char      *dst_buf;
    s_char       i;

    if (!AlbumPhotoIsUsed(src_side, src_no))
        return 0;

    src_buf = AlbumGetPhotoBuf(src_side);
    dst_buf = AlbumGetPhotoBuf(dst_side);
    memcpy(dst_buf + dst->adr_no * ALBUM_PHOTO_PIX_SIZE + ALBUM_PHOTO_PIX_OFS,
           src_buf + src->adr_no * ALBUM_PHOTO_PIX_SIZE + ALBUM_PHOTO_PIX_OFS, ALBUM_PHOTO_PIX_SIZE);
    memcpy(dst_buf + dst->adr_no * ALBUM_PHOTO_THUMB_SIZE,
           src_buf + src->adr_no * ALBUM_PHOTO_THUMB_SIZE, ALBUM_PHOTO_THUMB_SIZE);

    if (!AlbumPhotoIsUsed(dst_side, dst_no))
        AlbumSetPhotoNum(dst_side, AlbumGetPhotoNum(dst_side) + 1);

    dst->chp_no = src->chp_no;
    dst->mode   = src->mode;
    for (i = 0; i < PICTURE_SUBJECT_MAX; i++)
        dst->subject[i] = src->subject[i];
    dst->score  = src->score;
    dst->time   = src->time;
    dst->status = PICTURE_STATUS_USED;
    dst->room   = src->room;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Erase a used, unprotected photo (FF2 DeletePhotoData without the
// packing). 1 when erased.
// FF3 0x00121ad8.

int AlbumPhotoErase(s_char side, s_char no)
{
    if (!AlbumPhotoIsUsed(side, no))
        return 0;
    if (AlbumPhotoIsProtected(side, no))
        return 0;

    AlbumGetPhoto(side, no)->status = 0;
    AlbumSetPhotoNum(side, AlbumGetPhotoNum(side) - 1);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sort a side: key 0 by time, 1 by score, 2 protected first; order 0 / 1
// picks the direction.
// FF3 0x00121b70 (the table is built on the stack).

int AlbumSortPhoto(s_char side, s_char key, int order)
{
    static void (*const sort_func[6])(PFILE_WRK *) =
    {
        SortPhotoData_NewTime,  SortPhotoData_OldTime,
        SortPhotoData_BigScore, SortPhotoData_SmallScore,
        SortPhotoData_Protect,  SortPhotoData_NonProtect,
    };

    sort_func[key * 2 + order](&AlbumGetPFile(side)->file);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Protect / unprotect a used photo; at most ALBUM_PROTECT_MAX protected.
// 1 when changed.
// FF3 0x00121c08; was acsRopeMoveVib (wrong FF2 match). FF2 had
// SetFilePhotoProtect / DelFilePhotoProtect.

int AlbumPhotoProtectToggle(s_char side, s_char no)
{
    ALBUM_FILE  *af;
    PICTURE_WRK *pic;

    if (!AlbumPhotoIsUsed(side, no))
        return 0;

    af  = AlbumGetPFile(side);
    pic = AlbumGetPhoto(side, no);
    if ((pic->status & PICTURE_STATUS_PROTECT) == 0)
    {
        if (af->file.protect_num >= ALBUM_PROTECT_MAX)
            return 0;
        pic->status |= PICTURE_STATUS_PROTECT;
        af->file.protect_num++;
    }
    else
    {
        pic->status &= ~PICTURE_STATUS_PROTECT;
        af->file.protect_num--;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FF3 0x00121cc8.

void AlbumDispCtrlInit(void)
{
    album_disp_ctrl[0] = -1;
    album_disp_ctrl[1] = -1;
}
