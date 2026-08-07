//
//	UTF-8 path and file I/O support
//
#include "hsp3pathio.h"

#include <stdlib.h>

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

#else

FILE* hsp_fopen_utf8(const char* path, const char* mode)
{
	if (path == NULL || mode == NULL || !hsp_utf8_is_valid((const unsigned char*)path)) return NULL;
	return fopen(path, mode);
}

#endif
