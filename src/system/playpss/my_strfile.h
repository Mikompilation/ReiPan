/* ==========================================================================
 *  system/playpss/my_strfile.h
 *
 *  The PSS stream reader (FF2 my_strfile.c, over system/eeiop/file_stream.c).
 *  Minimal so far: what reconstructed code calls.
 * ======================================================================== */
#ifndef _SYSTEM_PLAYPSS_MY_STRFILE_H
#define _SYSTEM_PLAYPSS_MY_STRFILE_H

int MyStrStart(int file_no, int ring_buf_num, int one_buf_sector);  // FF3 0x00271c30: 0 while the stream is busy
int MyStrRead(void *buff, int size);                    // FF3 0x00271c48: bytes read, 0 at the end
int MyStrStop(void);                                    // FF3 0x00271ca0

#endif /* _SYSTEM_PLAYPSS_MY_STRFILE_H */
