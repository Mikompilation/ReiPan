// ingame/photo/photo_file.c -- the camera's album (FF2 photo.c's album half;
// FF3 0x22ca90..0x22ec88, see photo_file.h).
//
// Ported from MioPan's photo.c and checked against FF3 function by function.
// FF3 differences from FF2:
//   * InitPhotoWrk clears only the album (FF2 also cleared photo_wrk);
//   * GetSavePhotoNo's fallback scan runs from slot 15 down to slot 0 (FF2:
//     0 up to 14), so the oldest unprotected picture at the back goes first;
//   * AddPhotoData takes the photo mode and keeps it in PICTURE_WRK.mode, and
//     moves the new record to the front (MovePhotoDataTop, FF3's own);
//   * DeletePhotoData only frees the record (FF2 closed the gap behind it);
//   * the date sorts unpack the RTC with CdClockBcdToInt (FF2
//     SetDateInfoType); the range errors and asserts are gone.

#include "ingame/photo/photo_file.h"

#include <string.h>                             // memset

#include "common/cmn_time.h"                    // CdClockBcdToInt


// ──────────────────────────────────────────────────────────────────────
// Data
// ──────────────────────────────────────────────────────────────────────

PFILE_WRK pfile_wrk;                            // data 0x32f290

// ──────────────────────────────────────────────────────────────────────
// The album records
// ──────────────────────────────────────────────────────────────────────

// FF3 0x0022ca90
void InitPhotoWrk(void)
{
    int i;

    memset(&pfile_wrk, 0, sizeof(pfile_wrk));
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        pfile_wrk.pic[i].adr_no = (u_char)i;
    }
}

// A free record first; failing that the last used-but-unprotected one (FF3
// scans from the back); slot 15 when every record is protected.
// FF3 0x0022cae8
int GetSavePhotoNo(void)
{
    int    i;
    u_char adr_no;
    int    get_save_pos;

    adr_no       = 0;
    get_save_pos = 0;

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (pfile_wrk.pic[i].status == 0)
        {
            adr_no       = pfile_wrk.pic[i].adr_no;
            get_save_pos = 1;
            break;
        }
    }

    if (!get_save_pos)
    {
        for (i = PHOTO_FILE_MAX - 1; i >= 0; i--)
        {
            if ((pfile_wrk.pic[i].status & PICTURE_STATUS_PROTECT) == 0 &&
                (pfile_wrk.pic[i].status & PICTURE_STATUS_USED) != 0)
            {
                adr_no       = pfile_wrk.pic[i].adr_no;
                get_save_pos = 1;
                break;
            }
        }

        if (!get_save_pos)
        {
            adr_no = pfile_wrk.pic[PHOTO_FILE_MAX - 1].adr_no;
        }
    }

    return adr_no;
}

// Record no first, the other fifteen behind it in their order.
// FF3 0x0022cb88
void MovePhotoDataTop(PFILE_WRK *photo_file, u_int no)
{
    PICTURE_WRK sort_buff[PHOTO_FILE_MAX];
    PICTURE_WRK *dst;
    u_int       i;

    if (no >= PHOTO_FILE_MAX)
    {
        return;
    }

    memset(sort_buff, 0, sizeof(sort_buff));
    sort_buff[0] = photo_file->pic[no];
    dst          = &sort_buff[1];
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (i != no)
        {
            *dst++ = photo_file->pic[i];
        }
    }

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        photo_file->pic[i] = sort_buff[i];
    }
}

// Files a shot against the slot GetSavePhotoNo handed out. A full album wipes
// the old record of this adr_no and finds it again after the pack; number
// stays 0 if that search fails.
// FF3 0x0022cce0
int AddPhotoData(int adr_no, int score, int room_no, int chapter_no, int mode,
                 sceCdCLOCK rtc, SUBJECT_WRK *obj, int obj_num)
{
    int          i;
    int          number;
    PICTURE_WRK *pic;

    if (pfile_wrk.pic_num + 1 <= PHOTO_FILE_MAX)
    {
        number = pfile_wrk.pic_num++;

        if (pfile_wrk.pic[number].status & PICTURE_STATUS_USED)
        {
            for (i = 0; i < PHOTO_FILE_MAX; i++)
            {
                if (pfile_wrk.pic[i].status == 0)
                {
                    number = i;
                    break;
                }
            }
        }
    }
    else
    {
        for (i = 0; i < PHOTO_FILE_MAX; i++)
        {
            if (pfile_wrk.pic[i].adr_no == (u_char)adr_no)
            {
                memset(&pfile_wrk.pic[i], 0, sizeof(PICTURE_WRK));
                pfile_wrk.pic[i].adr_no = (u_char)adr_no;
                SortPhotoData_Before(&pfile_wrk);
                break;
            }
        }

        number = 0;
        for (i = 0; i < PHOTO_FILE_MAX; i++)
        {
            if (pfile_wrk.pic[i].adr_no == (u_char)adr_no)
            {
                number = i;
                break;
            }
        }
    }

    pic = &pfile_wrk.pic[number];

    pic->adr_no = (u_char)adr_no;
    pic->score  = score;
    pic->room   = (short)room_no;
    pic->chp_no = (u_char)chapter_no;
    pic->status = PICTURE_STATUS_USED;
    pic->time   = rtc;
    pic->mode   = (u_char)mode;

    // SUBJECT_WRK's no goes to obj_no; the first three subjects are kept.
    if (obj_num < PICTURE_SUBJECT_MAX)
    {
        for (i = 0; i < obj_num; i++)
        {
            pic->subject[i].type   = obj[i].type;
            pic->subject[i].obj_no = (short)obj[i].no;
        }

        for (; i < PICTURE_SUBJECT_MAX; i++)
        {
            pic->subject[i].type   = -1;
            pic->subject[i].obj_no = 0;
        }
    }
    else
    {
        for (i = 0; i < PICTURE_SUBJECT_MAX; i++)
        {
            pic->subject[i].type   = obj[i].type;
            pic->subject[i].obj_no = (short)obj[i].no;
        }
    }

    MovePhotoDataTop(&pfile_wrk, number);
    return 1;
}

// The record stays where it is, its adr_no with it; the film menu sorts
// afterwards.
// FF3 0x0022cf50
void DeletePhotoData(u_char no)
{
    if (pfile_wrk.pic[no].status != 0)
    {
        pfile_wrk.pic[no].status = 0;
        pfile_wrk.pic_num--;
    }
}

// FF3 0x0022cf88
void CopyPFileWrk(PFILE_WRK *copy_wrk)
{
    *copy_wrk = pfile_wrk;
}

// FF3 0x0022cfe8
int GetFilePhotoMode(u_char no)
{
    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }
    return pfile_wrk.pic[no].mode;
}

// FF3 0x0022d010
int GetFilePhotoState(u_char no)
{
    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }
    return pfile_wrk.pic[no].status;
}

// FF3 0x0022d038
int GetFilePhotoAdrNo(u_char no)
{
    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }
    return pfile_wrk.pic[no].adr_no;
}

// Fails once fifteen are protected, so GetSavePhotoNo always finds a slot.
// FF3 0x0022d060
int SetFilePhotoProtect(u_char no)
{
    int res;

    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }

    res = 0;
    if ((pfile_wrk.pic[no].status & PICTURE_STATUS_USED) &&
        pfile_wrk.protect_num < PHOTO_FILE_MAX - 1)
    {
        res = 1;
        pfile_wrk.pic[no].status |= PICTURE_STATUS_PROTECT;
        pfile_wrk.protect_num++;
    }
    return res;
}

// Writes the in-use bit back whole rather than masking the protect bit.
// FF3 0x0022d0c0
void DelFilePhotoProtect(u_char no)
{
    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }
    pfile_wrk.pic[no].status = PICTURE_STATUS_USED;
    pfile_wrk.protect_num--;
}

// FF3 0x0022d0f8
PICTURE_WRK *GetPhotoData(u_char no)
{
    if (no >= PHOTO_FILE_MAX)
    {
        no = 0;
    }
    return &pfile_wrk.pic[no];
}

// FF3 0x0022d120
int GetFilePhotoNum(void)
{
    return pfile_wrk.pic_num;
}

// ──────────────────────────────────────────────────────────────────────
// The six orderings. Each starts from SortPhotoData_Before, which packs the
// used records at the front, so the others only order a contiguous run.
// ──────────────────────────────────────────────────────────────────────

static void PhotoSwap(PICTURE_WRK *a, PICTURE_WRK *b)
{
    PICTURE_WRK buff;

    buff = *b;
    *b   = *a;
    *a   = buff;
}

// Used records first, free ones after, both in their existing order.
// FF3 0x0022d130
void SortPhotoData_Before(PFILE_WRK *photo_file)
{
    PICTURE_WRK sort_buff[PHOTO_FILE_MAX];
    int         photo_num;
    int         i;

    memset(sort_buff, 0, sizeof(sort_buff));

    photo_num = 0;
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (photo_file->pic[i].status & PICTURE_STATUS_USED)
        {
            sort_buff[photo_num++] = photo_file->pic[i];
        }
    }
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (photo_file->pic[i].status == 0)
        {
            sort_buff[photo_num++] = photo_file->pic[i];
        }
    }

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        photo_file->pic[i] = sort_buff[i];
    }
}

// Protected first, then the rest of the used ones, then the free slots.
// FF3 0x0022d2a0
void SortPhotoData_Protect(PFILE_WRK *photo_file)
{
    PICTURE_WRK sort_buff[PHOTO_FILE_MAX];
    int         set_count;
    int         i;

    set_count = 0;

    SortPhotoData_Before(photo_file);

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (photo_file->pic[i].status & PICTURE_STATUS_PROTECT)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if ((photo_file->pic[i].status & PICTURE_STATUS_PROTECT) == 0 &&
            (photo_file->pic[i].status & PICTURE_STATUS_USED) != 0)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (photo_file->pic[i].status == 0)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        photo_file->pic[i] = sort_buff[i];
    }
}

// Unprotected used records first, then the protected ones, then the free
// slots; the copy-back runs to set_count (16 whenever the status bits are
// consistent), as in FF2.
// FF3 0x0022d488
void SortPhotoData_NonProtect(PFILE_WRK *photo_file)
{
    PICTURE_WRK sort_buff[PHOTO_FILE_MAX];
    int         set_count;
    int         i;

    set_count = 0;

    SortPhotoData_Before(photo_file);

    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if ((photo_file->pic[i].status & PICTURE_STATUS_PROTECT) == 0 &&
            (photo_file->pic[i].status & PICTURE_STATUS_USED) != 0)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if ((photo_file->pic[i].status & PICTURE_STATUS_PROTECT) != 0 &&
            (photo_file->pic[i].status & PICTURE_STATUS_USED) != 0)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }
    for (i = 0; i < PHOTO_FILE_MAX; i++)
    {
        if (photo_file->pic[i].status == 0)
        {
            sort_buff[set_count++] = photo_file->pic[i];
        }
    }

    for (i = 0; i < set_count; i++)
    {
        photo_file->pic[i] = sort_buff[i];
    }
}

// The date sorts: six exchange passes over the used run, pass k ordering by
// date field k (year, month, day, hour, minute, second) among records whose
// earlier fields are equal. The ROM spells the six passes out; newer > 0
// puts the later date first.
static void SortPhotoDataTime(PFILE_WRK *photo_file, int newer)
{
    int date1[6];
    int date2[6];
    int pass;
    int i;
    int j;
    int k;

    SortPhotoData_Before(photo_file);

    for (pass = 0; pass < 6; pass++)
    {
        for (i = 0; i < photo_file->pic_num - 1; i++)
        {
            for (j = i + 1; j < photo_file->pic_num; j++)
            {
                CdClockBcdToInt(date1, &photo_file->pic[i].time);
                CdClockBcdToInt(date2, &photo_file->pic[j].time);

                for (k = 0; k < pass; k++)
                {
                    if (date1[k] != date2[k])
                    {
                        break;
                    }
                }
                if (k < pass)
                {
                    continue;
                }

                if (newer ? date1[pass] < date2[pass] : date2[pass] < date1[pass])
                {
                    PhotoSwap(&photo_file->pic[i], &photo_file->pic[j]);
                }
            }
        }
    }
}

// Newest first. callcheck: SortPhotoData_Before, CdClockBcdToInt (in SortPhotoDataTime)
// FF3 0x0022d688
void SortPhotoData_NewTime(PFILE_WRK *photo_file)
{
    SortPhotoDataTime(photo_file, 1);
}

// Oldest first: SortPhotoData_NewTime with every comparison reversed.
// callcheck: SortPhotoData_Before, CdClockBcdToInt (in SortPhotoDataTime)
// FF3 0x0022e028
void SortPhotoData_OldTime(PFILE_WRK *photo_file)
{
    SortPhotoDataTime(photo_file, 0);
}

// Highest score first.
// FF3 0x0022e9c8
void SortPhotoData_BigScore(PFILE_WRK *photo_file)
{
    int i;
    int j;

    SortPhotoData_Before(photo_file);

    for (i = 0; i < photo_file->pic_num - 1; i++)
    {
        for (j = i + 1; j < photo_file->pic_num; j++)
        {
            if (photo_file->pic[i].score < photo_file->pic[j].score)
            {
                PhotoSwap(&photo_file->pic[i], &photo_file->pic[j]);
            }
        }
    }
}

// Lowest score first.
// FF3 0x0022eb20
void SortPhotoData_SmallScore(PFILE_WRK *photo_file)
{
    int i;
    int j;

    SortPhotoData_Before(photo_file);

    for (i = 0; i < photo_file->pic_num - 1; i++)
    {
        for (j = i + 1; j < photo_file->pic_num; j++)
        {
            if (photo_file->pic[j].score < photo_file->pic[i].score)
            {
                PhotoSwap(&photo_file->pic[i], &photo_file->pic[j]);
            }
        }
    }
}

// FF3 0x0022ec78
PFILE_WRK *GetCamPhotoFile(void)
{
    return &pfile_wrk;
}
