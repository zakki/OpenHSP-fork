//
//	UTF-8 path and file I/O support
//
#ifndef __hsp3pathio_h
#define __hsp3pathio_h

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

// Open a file whose path is a NUL-terminated UTF-8 string.
// On Windows, the path is converted to UTF-16 before calling _wfopen.
// On POSIX systems, the validated UTF-8 byte sequence is passed to fopen.
FILE* hsp_fopen_utf8(const char* path, const char* mode);

// Return the size of a regular file, or -1 when the path is invalid or unavailable.
int64_t hsp_filesize_utf8(const char* path);

// Return nonzero when a filesystem entry exists at the UTF-8 path.
int hsp_file_exists_utf8(const char* path);

// Remove a filesystem entry at the UTF-8 path, returning zero on success.
int hsp_remove_utf8(const char* path);

// Convert a Windows ANSI string at a compatibility boundary to UTF-8.
// The returned buffer is allocated with malloc and must be released with free.
char* hsp_path_from_ansi(const char* path);

// Convert an internal UTF-8 path to the legacy Windows ANSI contract.
// Returns NULL when the path cannot be represented without loss.
char* hsp_path_to_ansi(const char* path);

// Extract a path component using the getpath-compatible mode flags.
// Returns nonzero on success and zero for invalid UTF-8 or a small output buffer.
int hsp_getpath_utf8(const char* path, char* output, size_t output_size, int mode);

#endif
