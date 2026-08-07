//
//	UTF-8 path and file I/O support
//
#include "hsp3pathio.h"

#include <stdlib.h>
#include <string.h>
#include <string>

static bool hsp_utf8_is_valid(const unsigned char* text)
{
	if (text == NULL) return false;

	while (*text != 0) {
		unsigned char c = *text++;
		if (c <= 0x7f) continue;

		if (c >= 0xc2 && c <= 0xdf) {
			if (text[0] < 0x80 || text[0] > 0xbf) return false;
			text += 1;
			continue;
		}

		if (c == 0xe0) {
			if (text[0] < 0xa0 || text[0] > 0xbf ||
				text[1] < 0x80 || text[1] > 0xbf) return false;
			text += 2;
			continue;
		}
		if ((c >= 0xe1 && c <= 0xec) || (c >= 0xee && c <= 0xef)) {
			if (text[0] < 0x80 || text[0] > 0xbf ||
				text[1] < 0x80 || text[1] > 0xbf) return false;
			text += 2;
			continue;
		}
		if (c == 0xed) {
			if (text[0] < 0x80 || text[0] > 0x9f ||
				text[1] < 0x80 || text[1] > 0xbf) return false;
			text += 2;
			continue;
		}

		if (c == 0xf0) {
			if (text[0] < 0x90 || text[0] > 0xbf ||
				text[1] < 0x80 || text[1] > 0xbf ||
				text[2] < 0x80 || text[2] > 0xbf) return false;
			text += 3;
			continue;
		}
		if (c >= 0xf1 && c <= 0xf3) {
			if (text[0] < 0x80 || text[0] > 0xbf ||
				text[1] < 0x80 || text[1] > 0xbf ||
				text[2] < 0x80 || text[2] > 0xbf) return false;
			text += 3;
			continue;
		}
		if (c == 0xf4) {
			if (text[0] < 0x80 || text[0] > 0x8f ||
				text[1] < 0x80 || text[1] > 0xbf ||
				text[2] < 0x80 || text[2] > 0xbf) return false;
			text += 3;
			continue;
		}

		return false;
	}

	return true;
}

static bool hsp_path_is_separator(char c)
{
#if defined(HSPWIN) || defined(_WIN32)
	return c == '/' || c == '\\';
#else
	return c == '/';
#endif
}

int hsp_getpath_utf8(const char* path, char* output, size_t output_size, int mode)
{
	if (output == NULL || output_size == 0 ||
		!hsp_utf8_is_valid((const unsigned char*)path)) return 0;

	std::string source(path);
	if (mode & 16) {
		for (size_t i = 0; i < source.size(); ++i) {
			if (source[i] >= 'A' && source[i] <= 'Z') {
				source[i] = (char)(source[i] - 'A' + 'a');
			}
		}
	}

	size_t separator = std::string::npos;
	for (size_t i = 0; i < source.size(); ++i) {
		if (hsp_path_is_separator(source[i])) separator = i;
	}

	size_t directory_end = separator == std::string::npos ? 0 : separator + 1;
	if (directory_end == 0 && source.size() >= 2 && source[1] == ':') {
		directory_end = 2;
	}

	std::string directory = source.substr(0, directory_end);
	std::string filename = source.substr(directory_end);
	std::string extension;
	std::string name = filename;
	size_t dot = filename.rfind('.');
	if (dot != std::string::npos) {
		name = filename.substr(0, dot);
		extension = filename.substr(dot);
	}

	std::string result;
	if (mode & 8) {
		result = filename;
	}
	else if (mode & 32) {
		result = directory;
	}

	switch (mode & 7) {
	case 1:
		if (mode & 8) {
			result = name;
		}
		else {
			result = source;
			if (!extension.empty() && result.size() >= extension.size() &&
				result.compare(result.size() - extension.size(), extension.size(), extension) == 0) {
				result.erase(result.size() - extension.size());
			}
		}
		break;
	case 2:
		result = extension;
		break;
	default:
		if ((mode & (8 | 32)) == 0) result = source;
		break;
	}

	if (result.size() + 1 > output_size) return 0;
	memcpy(output, result.c_str(), result.size() + 1);
	return 1;
}

#if defined(HSPWIN) || defined(_WIN32)

#include <windows.h>
#include <sys/stat.h>

static wchar_t* hsp_utf8_to_wide(const char* text)
{
	if (!hsp_utf8_is_valid((const unsigned char*)text)) return NULL;

	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
	if (length <= 0) return NULL;

	wchar_t* result = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)length);
	if (result == NULL) return NULL;
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, length) == 0) {
		free(result);
		return NULL;
	}
	return result;
}

static char* hsp_wide_to_utf8(const wchar_t* text)
{
	if (text == NULL) return NULL;
	int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
	if (length <= 0) return NULL;
	char* result = (char*)malloc((size_t)length);
	if (result == NULL) return NULL;
	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result, length, NULL, NULL) == 0) {
		free(result);
		return NULL;
	}
	return result;
}

int hsp_dirlist_utf8(const char* pattern, int flags, hsp_path_list_callback callback, void* user_data)
{
	if (callback == NULL) return -1;
	wchar_t* wide_pattern = hsp_utf8_to_wide(pattern);
	if (wide_pattern == NULL) return -1;

	WIN32_FIND_DATAW data;
	HANDLE handle = FindFirstFileW(wide_pattern, &data);
	free(wide_pattern);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	DWORD attribute_mask = 0;
	if (flags & 1) attribute_mask |= FILE_ATTRIBUTE_DIRECTORY;
	if (flags & 2) attribute_mask |= FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
	int count = 0;
	int result = 0;
	for (;;) {
		bool selected = (data.dwFileAttributes & attribute_mask) != 0;
		if ((flags & 4) == 0) selected = !selected;
		char* name = selected ? hsp_wide_to_utf8(data.cFileName) : NULL;
		if (name != NULL && name[0] != 0 && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
			++count;
			if (callback(name, user_data) != 0) {
				result = -1;
				free(name);
				break;
			}
		}
		free(name);
		if (!FindNextFileW(handle, &data)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) break;
			result = -1;
			break;
		}
	}
	FindClose(handle);
	return result < 0 ? result : count;
}

int64_t hsp_filesize_utf8(const char* path)
{
	wchar_t* wide_path = hsp_utf8_to_wide(path);
	if (wide_path == NULL) return -1;

	struct _stat64 status;
	int result = _wstat64(wide_path, &status);
	free(wide_path);
	if (result != 0 || (status.st_mode & _S_IFMT) != _S_IFREG) return -1;
	return (int64_t)status.st_size;
}

int hsp_file_exists_utf8(const char* path)
{
	wchar_t* wide_path = hsp_utf8_to_wide(path);
	if (wide_path == NULL) return 0;

	struct _stat64 status;
	int result = _wstat64(wide_path, &status);
	free(wide_path);
	return result == 0;
}

int hsp_remove_utf8(const char* path)
{
	wchar_t* wide_path = hsp_utf8_to_wide(path);
	if (wide_path == NULL) return -1;
	int result = _wremove(wide_path);
	free(wide_path);
	return result;
}

FILE* hsp_fopen_utf8(const char* path, const char* mode)
{
	if (path == NULL || mode == NULL) return NULL;
	wchar_t* wide_path = hsp_utf8_to_wide(path);
	wchar_t* wide_mode = hsp_utf8_to_wide(mode);
	if (wide_path == NULL || wide_mode == NULL) {
		free(wide_path);
		free(wide_mode);
		return NULL;
	}

	FILE* result = _wfopen(wide_path, wide_mode);
	free(wide_path);
	free(wide_mode);
	return result;
}

char* hsp_path_from_ansi(const char* path)
{
	if (path == NULL) return NULL;

	int wide_length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
	if (wide_length <= 0) return NULL;
	wchar_t* wide_path = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)wide_length);
	if (wide_path == NULL) return NULL;
	if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, wide_path, wide_length) == 0) {
		free(wide_path);
		return NULL;
	}

	int utf8_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, -1, NULL, 0, NULL, NULL);
	if (utf8_length <= 0) {
		free(wide_path);
		return NULL;
	}
	char* result = (char*)malloc((size_t)utf8_length);
	if (result == NULL) {
		free(wide_path);
		return NULL;
	}
	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, -1, result, utf8_length, NULL, NULL) == 0) {
		free(wide_path);
		free(result);
		return NULL;
	}
	free(wide_path);
	return result;
}

char* hsp_path_to_ansi(const char* path)
{
	wchar_t* wide_path = hsp_utf8_to_wide(path);
	if (wide_path == NULL) return NULL;

	BOOL used_default = FALSE;
	int ansi_length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide_path, -1, NULL, 0, NULL, &used_default);
	if (ansi_length <= 0 || used_default) {
		free(wide_path);
		return NULL;
	}
	char* result = (char*)malloc((size_t)ansi_length);
	if (result == NULL) {
		free(wide_path);
		return NULL;
	}
	used_default = FALSE;
	if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide_path, -1, result, ansi_length, NULL, &used_default) == 0 || used_default) {
		free(wide_path);
		free(result);
		return NULL;
	}
	free(wide_path);
	return result;
}

#else

#include <sys/stat.h>
#include <glob.h>

int64_t hsp_filesize_utf8(const char* path)
{
	if (!hsp_utf8_is_valid((const unsigned char*)path)) return -1;

	struct stat status;
	if (stat(path, &status) != 0 || !S_ISREG(status.st_mode)) return -1;
	return (int64_t)status.st_size;
}

int hsp_file_exists_utf8(const char* path)
{
	if (!hsp_utf8_is_valid((const unsigned char*)path)) return 0;
	struct stat status;
	return stat(path, &status) == 0;
}

int hsp_remove_utf8(const char* path)
{
	if (path == NULL || !hsp_utf8_is_valid((const unsigned char*)path)) return -1;
	return remove(path);
}

int hsp_dirlist_utf8(const char* pattern, int flags, hsp_path_list_callback callback, void* user_data)
{
	if (callback == NULL || !hsp_utf8_is_valid((const unsigned char*)pattern)) return -1;
	glob_t matches;
	memset(&matches, 0, sizeof(matches));
	int glob_result = glob(pattern, 0, NULL, &matches);
	if (glob_result == GLOB_NOMATCH) {
		globfree(&matches);
		return 0;
	}
	if (glob_result != 0) {
		globfree(&matches);
		return -1;
	}

	int count = 0;
	int result = 0;
	for (size_t i = 0; i < matches.gl_pathc; ++i) {
		const char* full_path = matches.gl_pathv[i];
		struct stat status;
		if (stat(full_path, &status) != 0) continue;
		bool is_directory = (status.st_mode & S_IFMT) == S_IFDIR;
		const char* basename = strrchr(full_path, '/');
		basename = basename != NULL ? basename + 1 : full_path;
		int attributes = is_directory ? 1 : 0;
		if (basename[0] == '.') attributes |= 2;
		bool selected = (attributes & (flags & 3)) != 0;
		if ((flags & 4) == 0) selected = !selected;
		if (!selected || basename[0] == 0 || strcmp(basename, ".") == 0 || strcmp(basename, "..") == 0) continue;
		++count;
		if (callback(basename, user_data) != 0) {
			result = -1;
			break;
		}
	}
	globfree(&matches);
	return result < 0 ? result : count;
}

char* hsp_path_from_ansi(const char* path)
{
	if (path == NULL) return NULL;
	size_t length = strlen(path) + 1;
	char* result = (char*)malloc(length);
	if (result != NULL) memcpy(result, path, length);
	return result;
}

char* hsp_path_to_ansi(const char* path)
{
	if (path == NULL || !hsp_utf8_is_valid((const unsigned char*)path)) return NULL;
	size_t length = strlen(path) + 1;
	char* result = (char*)malloc(length);
	if (result != NULL) memcpy(result, path, length);
	return result;
}

FILE* hsp_fopen_utf8(const char* path, const char* mode)
{
	if (path == NULL || mode == NULL || !hsp_utf8_is_valid((const unsigned char*)path)) return NULL;
	return fopen(path, mode);
}

#endif
