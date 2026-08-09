//
//	UTF-8 path and file I/O support
//
#ifndef __hsp3pathio_h
#define __hsp3pathio_h

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#if defined(HSPWIN) || defined(_WIN32)
#include <wchar.h>
#endif

// The default path representation follows the target's internal string
// representation. POSIX paths and the compiler's UTF-8 path build use UTF-8;
// the legacy Windows runtime uses the system ACP.
#if defined(HSPCMP_PATH_UTF8) || defined(HSPUTF8) || (!defined(HSPWIN) && !defined(_WIN32))
#define HSP_PATHIO_DEFAULT_UTF8 1
#else
#define HSP_PATHIO_DEFAULT_UTF8 0
#endif

namespace hsp_path {

struct utf8_tag {};
struct ansi_tag {};

template <typename Encoding>
class view {
public:
	explicit view(const char* value) : value_(value) {}

	const char* c_str() const { return value_; }

private:
	const char* value_;
};

template <typename Encoding>
class owned {
public:
	explicit owned(char* value = NULL) : value_(value) {}

	owned(owned&& other) : value_(other.value_)
	{
		other.value_ = NULL;
	}

	owned& operator=(owned&& other)
	{
		if (this != &other) {
			free(value_);
			value_ = other.value_;
			other.value_ = NULL;
		}
		return *this;
	}

	~owned()
	{
		free(value_);
	}

	owned(const owned&) = delete;
	owned& operator=(const owned&) = delete;

	const char* c_str() const { return value_; }
	char* data() { return value_; }
	hsp_path::view<Encoding> as_view() const { return hsp_path::view<Encoding>(value_); }
	char* release()
	{
		char* result = value_;
		value_ = NULL;
		return result;
	}
	explicit operator bool() const { return value_ != NULL; }

private:
	char* value_;
};

typedef view<utf8_tag> utf8_view;
typedef view<ansi_tag> ansi_view;

#if HSP_PATHIO_DEFAULT_UTF8
typedef utf8_view path_view;
#else
typedef ansi_view path_view;
#endif

typedef owned<utf8_tag> utf8_string;
typedef owned<ansi_tag> ansi_string;

}

// Open a file whose path is a NUL-terminated UTF-8 string.
// On Windows, the path is converted to UTF-16 before calling _wfopen.
// On POSIX systems, the validated UTF-8 byte sequence is passed to fopen.
FILE* hsp_path_fopen_utf8(hsp_path::utf8_view path, const char* mode);

#if defined(HSPWIN) || defined(_WIN32)
// Return the current module filename or directory as UTF-8.
int hsp_path_get_module_filename_utf8(std::string& result);
int hsp_path_get_module_directory_utf8(std::string& result);
int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::utf8_view name);
int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::ansi_view name);
#endif

// Open a file using the target's default internal path representation.
FILE* hsp_path_fopen(hsp_path::path_view path, const char* mode);

// Return the size of a regular file, or -1 when the path is invalid or unavailable.
int64_t hsp_path_filesize_utf8(hsp_path::utf8_view path);
int64_t hsp_path_filesize(hsp_path::path_view path);

// Return nonzero when a filesystem entry exists at the path.
int hsp_path_file_exists_utf8(hsp_path::utf8_view path);
int hsp_path_file_exists(hsp_path::path_view path);

// Remove a filesystem entry at the path, returning zero on success.
int hsp_path_remove_utf8(hsp_path::utf8_view path);
int hsp_path_remove(hsp_path::path_view path);

// Enumerate entries matching a UTF-8 filesystem pattern. The callback receives
// each entry's UTF-8 basename and returns zero to continue.
typedef int (*hsp_path_list_callback)(hsp_path::utf8_view name, void* user_data);
int hsp_path_dirlist_utf8(hsp_path::utf8_view pattern, int flags, hsp_path_list_callback callback, void* user_data);

// Convert a Windows ACP string at a compatibility boundary to UTF-8.
// The returned value owns a malloc-allocated buffer.
hsp_path::utf8_string hsp_path_from_ansi(hsp_path::ansi_view path);

// Convert a Windows wide string at a UTF-16 boundary to UTF-8.
// The returned value owns a malloc-allocated buffer.
#if defined(HSPWIN) || defined(_WIN32)
hsp_path::utf8_string hsp_path_utf8_from_wide(const wchar_t* text);

// Launch a process using a UTF-8 command line. Returns a WinExec-compatible
// success value (>= 32) or zero on failure.
int hsp_path_exec_utf8(hsp_path::utf8_view command);
#endif

// Convert an internal UTF-8 path to the legacy Windows ACP contract.
// Returns an invalid value when the path cannot be represented without loss.
hsp_path::ansi_string hsp_path_to_ansi(hsp_path::utf8_view path);

// Extract a path component using the getpath-compatible mode flags.
// Returns nonzero on success and zero for invalid UTF-8 or a small output buffer.
int hsp_path_getpath_utf8(hsp_path::utf8_view path, char* output, size_t output_size, int mode);

// Extract a path component using the target's default internal path
// representation. Returns nonzero on success and zero for an invalid path or
// an output buffer that is too small.
int hsp_path_getpath(hsp_path::path_view path, char* output, size_t output_size, int mode);

#endif
