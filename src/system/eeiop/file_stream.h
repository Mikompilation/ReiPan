/* ==========================================================================
 *  system/eeiop/file_stream.h
 *
 *  The EE half of the file streamer (FF2 file_stream.c; FF3
 *  0x270a48..0x270cf8): RPC service 4 asks the IOP to keep a ring of sector
 *  buffers filled from one file and hands slices of it over on request. The
 *  movie reader (system/playpss my_strfile.c, MyStrRead) is its only user.
 * ======================================================================== */
#ifndef _SYSTEM_EEIOP_FILE_STREAM_H
#define _SYSTEM_EEIOP_FILE_STREAM_H

void FileStreamInit(void);                                          // 0x270a48: binds RPC 4

// Open file_no as a stream of ring_buf_num buffers of one_buf_sector sectors.
// Always 1.
int  FileStreamStart(int file_no, int ring_buf_num, int one_buf_sector);    // 0x270b18

// Read size bytes (whole sectors) into buff. block_read 0 posts the request
// and returns (FileStreamIsAct says when it landed). Returns the byte count
// the IOP's reply holds.
int  FileStreamRead(void *buff, int size, int block_read);          // 0x270bc8

int  FileStreamIsAct(void);                                         // 0x270c68: 1 while a read is in flight
int  FileStreamStop(void);                                          // 0x270c90: always 1

#endif /* _SYSTEM_EEIOP_FILE_STREAM_H */
