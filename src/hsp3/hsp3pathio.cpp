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

#else

char* hsp_path_from_ansi(const char* path)
{
	if (path == NULL) return NULL;
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
