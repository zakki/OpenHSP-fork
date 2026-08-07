//
//	UTF-8 path and file I/O support
//
#ifndef __hsp3pathio_h
#define __hsp3pathio_h

#include <stdio.h>
#include <stddef.h>

// Open a file whose path is a NUL-terminated UTF-8 string.
// On Windows, the path is converted to UTF-16 before calling _wfopen.
// On POSIX systems, the validated UTF-8 byte sequence is passed to fopen.
FILE* hsp_fopen_utf8(const char* path, const char* mode);

// Extract a path component using the getpath-compatible mode flags.
// Returns nonzero on success and zero for invalid UTF-8 or a small output buffer.
int hsp_getpath_utf8(const char* path, char* output, size_t output_size, int mode);

#endif
