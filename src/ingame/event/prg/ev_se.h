/* ==========================================================================
 *  ingame/event/prg/ev_se.h
 *
 *  Per-room event SE banks (FF2 ev_se.c): FF3 0x19c658..0x19ca60.
 *
 *  Each of the 60 rooms registers up to five SE bank files (SOUND_LOAD,
 *  SOUND_PRELOAD_REGIST, EV_SOUND_RELEASE); entering a room releases the
 *  current half of a double-buffered loaded table, loads the room's files
 *  into it and flips ev_se_toggle, so ev_seIsReady polls the half just
 *  loaded (ev_se_toggle ^ 1).
 *
 *  FF3 differs from FF2: files go straight to snd_ctrl's SE bank table by
 *  file number (SndSeBankLoad / IsLoaded / Release) instead of SndBankNew
 *  ids, every slot carries a wait flag (ev_seIsReady only waits for flagged
 *  files), the table has 60 rooms (FF2 66), the asserts are gone and
 *  ev_seGetBankID is dropped.
 * ======================================================================== */
#ifndef _INGAME_EVENT_PRG_EV_SE_H
#define _INGAME_EVENT_PRG_EV_SE_H

#define EV_SE_ROOM_MAX  60
#define EV_SE_FILE_MAX  5

// One registered (or loaded) SE bank file.
struct EV_SE_FILE
{
    int file_no;                        // 0x0: CD_FILE_DAT file, -1 = free
    int wait;                           // 0x4: ev_seIsReady waits for it to load
};
static_assert(sizeof(EV_SE_FILE) == 0x8, "EV_SE_FILE size");

int  ev_seSetSave(void **addr);         // FF3 0x0019c658: {ev_se_reg_files, 0x960} (save block 9)
void ev_seInit(void);                   // FF3 0x0019c730: all rooms and both halves empty (EventInit)
void ev_seRelease(void);                // FF3 0x0019c7b8: both halves released (EventRelease, GameSystemEnd)
void ev_seRegisterFile(int room_id, int file_no, int wait);    // FF3 0x0019c7e0: SOUND_LOAD / SOUND_PRELOAD_REGIST
void ev_seDeleteFile(int room_id, int file_no);                // FF3 0x0019c830: EV_SOUND_RELEASE
void ev_seChangeRoom(int room_id);      // FF3 0x0019c8b8: entering a room (MapLoadSetNowRoom)
int  ev_seIsReady(void);                // FF3 0x0019c988: 0 while a waited-for file of the new half loads
void ev_seFlipToggle(void);             // FF3 0x0019ca20: MapDoor back into the door's own room

#endif /* _INGAME_EVENT_PRG_EV_SE_H */
