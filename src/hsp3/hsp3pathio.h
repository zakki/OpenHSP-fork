//
//	UTF-8 path and file I/O support
//
#ifndef __hsp3pathio_h
#define __hsp3pathio_h

#include <stdio.h>

// Open a file whose path is a NUL-terminated UTF-8 string.
// On Windows, the path is converted to UTF-16 before calling _wfopen.
// On POSIX systems, the validated UTF-8 byte sequence is passed to fopen.
FILE* hsp_fopen_utf8(const char* path, const char* mode);

#endif
