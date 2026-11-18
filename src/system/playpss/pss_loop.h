/* ==========================================================================
 *  system/playpss/pss_loop.h
 *
 *  The PSS loop reader: a stream reader for playPssStart that loops a movie
 *  without a pause at the seam. It reads file `file` from the disc
 *  (MyStrRead) and keeps file + 1, the movie's head, loaded whole in the
 *  system heap; at the end of the stream PssLoopRewind switches the reads to
 *  that in-memory head while the disc stream seeks back to the top.
 *  FF3 0x219b48..0x219e60, FF3-only (file name descriptive; inferred as
 *  ingame/photo/n_equip_tray.c by link order). Used by the map sky's movie
 *  player (ingame/map/MapSky.c, MAP_SKY.loop).
 * ======================================================================== */
#ifndef _SYSTEM_PLAYPSS_PSS_LOOP_H
#define _SYSTEM_PLAYPSS_PSS_LOOP_H

void PssLoopHeadLoad(int file);                         // FF3 0x00219b48: loads file + 1 into the heap
int  PssLoopHeadIsLoaded(void);                         // FF3 0x00219bc0
void PssLoopHeadSetChunk(int size);                     // FF3 0x00219c08: chunk count, once
int  PssLoopHeadRewind(void);                           // FF3 0x00219c38: read position 0; 1 when it was unset
int  PssLoopHeadRead(void *buff, int size);             // FF3 0x00219c50: next chunk of the head, 0 past it
void PssLoopHeadRelease(void);                          // FF3 0x00219cb0
int  PssLoopSet(int file, int ring_buf_num, int one_buf_sector);    // FF3 0x00219d18: always 1
int  PssLoopStart(void);                                // FF3 0x00219d48: 1 once the head is in and the stream runs
int  PssLoopRead(void *buff, int size);                 // FF3 0x00219d90: playPssStart's reader
void PssLoopRewind(void);                               // FF3 0x00219df0
void PssLoopEnd(void);                                  // FF3 0x00219e40

#endif /* _SYSTEM_PLAYPSS_PSS_LOOP_H */
