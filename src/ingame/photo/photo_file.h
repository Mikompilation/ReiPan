/* ==========================================================================
 *  ingame/photo/photo_file.h
 *
 *  The camera's album (FF2 photo.c's album half; FF3 0x22ca90..0x22ec88):
 *  pfile_wrk, sixteen PICTURE_WRK records each pinned to a photo-data slot
 *  (adr_no), and the record operations the shot, the film menu and the
 *  album use: file a picture, protect / unprotect, delete, the six sorts.
 *  The types are in photo.h.
 * ======================================================================== */
#ifndef _INGAME_PHOTO_PHOTO_FILE_H
#define _INGAME_PHOTO_PHOTO_FILE_H

#include "ingame/photo/photo.h"                 // PFILE_WRK, PICTURE_WRK
#include "ingame/plyr/plyr_shot.h"              // SUBJECT_WRK
#include "sdk/libcdvd.h"                        // sceCdCLOCK

extern PFILE_WRK pfile_wrk;                     // data 0x32f290: the camera's album

void InitPhotoWrk(void);                        // FF3 0x0022ca90: empty album, pic[i].adr_no = i (PhotoDatInit)
int  GetSavePhotoNo(void);                      // FF3 0x0022cae8: the slot the next shot is written to
void MovePhotoDataTop(PFILE_WRK *photo_file, u_int no);        // FF3 0x0022cb88
int  AddPhotoData(int adr_no, int score, int room_no, int chapter_no, int mode,
                  sceCdCLOCK rtc, SUBJECT_WRK *obj, int obj_num); // FF3 0x0022cce0
void DeletePhotoData(u_char no);                // FF3 0x0022cf50: frees record no (no shift in FF3)
void CopyPFileWrk(PFILE_WRK *copy_wrk);         // FF3 0x0022cf88: copy of the camera album
int  GetFilePhotoMode(u_char no);               // FF3 0x0022cfe8: the photo mode of record no
int  GetFilePhotoState(u_char no);              // FF3 0x0022d010: PICTURE_STATUS_* of record no
int  GetFilePhotoAdrNo(u_char no);              // FF3 0x0022d038
int  SetFilePhotoProtect(u_char no);            // FF3 0x0022d060: 1 when protected (at most 15)
void DelFilePhotoProtect(u_char no);            // FF3 0x0022d0c0
PICTURE_WRK *GetPhotoData(u_char no);           // FF3 0x0022d0f8
int  GetFilePhotoNum(void);                     // FF3 0x0022d120: pic_num

void SortPhotoData_Before(PFILE_WRK *photo_file);      // FF3 0x0022d130: used records first
void SortPhotoData_Protect(PFILE_WRK *photo_file);     // FF3 0x0022d2a0: protected, used, free
void SortPhotoData_NonProtect(PFILE_WRK *photo_file);  // FF3 0x0022d488: used, protected, free
void SortPhotoData_NewTime(PFILE_WRK *photo_file);     // FF3 0x0022d688: newest first
void SortPhotoData_OldTime(PFILE_WRK *photo_file);     // FF3 0x0022e028: oldest first
void SortPhotoData_BigScore(PFILE_WRK *photo_file);    // FF3 0x0022e9c8: highest score first
void SortPhotoData_SmallScore(PFILE_WRK *photo_file);  // FF3 0x0022eb20: lowest score first
PFILE_WRK *GetCamPhotoFile(void);               // FF3 0x0022ec78: &pfile_wrk

#endif /* _INGAME_PHOTO_PHOTO_FILE_H */
