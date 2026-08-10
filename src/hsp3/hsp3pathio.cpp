//
//	UTF-8 path and file I/O support
//
#include "hsp3pathio.h"
#if defined(HSPWIN) || defined(_WIN32)
#include "hsp3utfcnv.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <string>
#include <vector>

static bool hsp_path_utf8_is_valid(const unsigned char* text);

namespace {

namespace fs = std::filesystem;

static bool hsp_path_make_fs_path(const char* text, fs::path& result)
{
	if (text == NULL || !hsp_path_utf8_is_valid((const unsigned char*)text)) return false;
	try {
		result = fs::u8path(text);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

static std::string hsp_path_to_utf8(const fs::path& path)
{
	try {
		return path.u8string();
	}
	catch (const std::exception&) {
		return std::string();
	}
}

static int hsp_path_copy_result(const std::string& result, char* output, size_t output_size)
{
	if (output == NULL || output_size == 0 || result.size() + 1 > output_size) return 0;
	memcpy(output, result.c_str(), result.size() + 1);
	return 1;
}

}

#if !HSP_PATHIO_DEFAULT_UTF8 && (defined(HSPWIN) || defined(_WIN32))
#if defined(HSPWIN)
#include "supio.h"
#else
extern void getpath(char* source, char* output, int mode);
#endif
#endif

static bool hsp_path_utf8_is_valid(const unsigned char* text)
{
	if (text == NULL) return false;
	auto valid_continuations = [](const unsigned char* bytes, size_t available, size_t count,
		unsigned char first_min, unsigned char first_max) {
		if (available < count) return false;
		for (size_t i = 0; i < count; ++i) {
			unsigned char byte = bytes[i];
			if (byte == 0) return false;
			if (i == 0) {
				if (byte < first_min || byte > first_max) return false;
			}
			else if (byte < 0x80 || byte > 0xbf) {
				return false;
			}
		}
		return true;
	};

	size_t remaining = strlen((const char*)text);
	while (remaining != 0) {
		unsigned char c = *text++;
		--remaining;
		if (c <= 0x7f) continue;

		if (c >= 0xc2 && c <= 0xdf) {
			if (!valid_continuations(text, remaining, 1, 0x80, 0xbf)) return false;
			text += 1;
			remaining -= 1;
			continue;
		}

		if (c == 0xe0) {
			if (!valid_continuations(text, remaining, 2, 0xa0, 0xbf)) return false;
			text += 2;
			remaining -= 2;
			continue;
		}
		if ((c >= 0xe1 && c <= 0xec) || (c >= 0xee && c <= 0xef)) {
			if (!valid_continuations(text, remaining, 2, 0x80, 0xbf)) return false;
			text += 2;
			remaining -= 2;
			continue;
		}
		if (c == 0xed) {
			if (!valid_continuations(text, remaining, 2, 0x80, 0x9f)) return false;
			text += 2;
			remaining -= 2;
			continue;
		}

		if (c == 0xf0) {
			if (!valid_continuations(text, remaining, 3, 0x90, 0xbf)) return false;
			text += 3;
			remaining -= 3;
			continue;
		}
		if (c >= 0xf1 && c <= 0xf3) {
			if (!valid_continuations(text, remaining, 3, 0x80, 0xbf)) return false;
			text += 3;
			remaining -= 3;
			continue;
		}
		if (c == 0xf4) {
			if (!valid_continuations(text, remaining, 3, 0x80, 0x8f)) return false;
			text += 3;
			remaining -= 3;
			continue;
		}

		return false;
	}

	return true;
}

int hsp_path_getpath_utf8(hsp_path::utf8_view path, char* output, size_t output_size, int mode)
{
	fs::path fs_source;
	if (output == NULL || output_size == 0 || !hsp_path_make_fs_path(path.c_str(), fs_source)) return 0;

	std::string source = hsp_path_to_utf8(fs_source);
	if (source.empty() && path.c_str()[0] != 0) return 0;
	if (mode & 16) {
		for (char& character : source) {
			if (character >= 'A' && character <= 'Z') character = (char)(character - 'A' + 'a');
		}
		if (!hsp_path_make_fs_path(source.c_str(), fs_source)) return 0;
	}

	std::string filename = hsp_path_to_utf8(fs_source.filename());
	std::string extension = hsp_path_to_utf8(fs_source.extension());
	std::string name = hsp_path_to_utf8(fs_source.stem());
	std::string directory = hsp_path_to_utf8(fs_source.parent_path());
	bool drive_relative = source.size() >= 2 && source[1] == ':' &&
		(source.size() == 2 || (source[2] != '/' && source[2] != '\\'));
	if (drive_relative) {
		size_t separator = source.find_last_of("/\\");
		if (separator == std::string::npos || separator < 2) {
			directory = source.substr(0, 2);
			filename = source.substr(2);
		}
		else {
			directory = source.substr(0, separator + 1);
			filename = source.substr(separator + 1);
		}
		std::string dot_filename = filename;
		size_t dot = dot_filename.rfind('.');
		if (dot == 0) {
			extension = dot_filename;
			name.clear();
		}
		else {
			name = dot == std::string::npos ? dot_filename : dot_filename.substr(0, dot);
			extension = dot == std::string::npos ? std::string() : dot_filename.substr(dot);
		}
	}
	else if (filename.size() > 0 && filename[0] == '.' && filename.find('.', 1) == std::string::npos) {
		extension = filename;
		name.clear();
	}
	if (!directory.empty() && directory.back() != '/' && directory.back() != '\\') {
		char separator = '/';
		for (const char* character = path.c_str(); *character != 0; ++character) {
			if (*character == '/' || *character == '\\') separator = *character;
		}
		if (!drive_relative) directory.push_back(separator);
	}

	// Select the base component first (filename-only, directory-only, or the
	// full source), then let the mode&7 selector operate uniformly on that
	// base so every mode-bit combination is honored instead of one branch
	// silently discarding an earlier selection.
	std::string base;
	if (mode & 8) {
		base = filename;
	}
	else if (mode & 32) {
		base = directory;
	}
	else {
		base = source;
	}

	std::string result;
	switch (mode & 7) {
	case 1:
		if (mode & 8) {
			result = name;
		}
		else {
			result = base;
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
		result = base;
		break;
	}

	return hsp_path_copy_result(result, output, output_size);
}

int64_t hsp_path_filesize_utf8(hsp_path::utf8_view path)
{
	fs::path fs_path;
	std::error_code error;
	if (!hsp_path_make_fs_path(path.c_str(), fs_path) || !fs::is_regular_file(fs_path, error)) return -1;
	uintmax_t size = fs::file_size(fs_path, error);
	return error ? -1 : (int64_t)size;
}

int hsp_path_file_exists_utf8(hsp_path::utf8_view path)
{
	fs::path fs_path;
	std::error_code error;
	return hsp_path_make_fs_path(path.c_str(), fs_path) && fs::exists(fs_path, error) && !error;
}

#if defined(HSPWIN) || defined(_WIN32)

#include <windows.h>
#include <sys/stat.h>

static wchar_t* hsp_path_utf8_to_wide(const char* text)
{
	if (text == NULL || !hsp_path_utf8_is_valid((const unsigned char*)text)) return NULL;
	int length = utf8_to_utf16_strict(NULL, text, 0);
	if (length <= 0) return NULL;
	wchar_t* result = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)length);
	if (result == NULL) return NULL;
	if (utf8_to_utf16_strict(result, text, length) == 0) {
		free(result);
		return NULL;
	}
	return result;
}

int hsp_path_exec_utf8(hsp_path::utf8_view command)
{
	wchar_t* wide_command = hsp_path_utf8_to_wide(command.c_str());
	if (wide_command == NULL) return 0;

	STARTUPINFOW startup_info = {};
	PROCESS_INFORMATION process_info = {};
	startup_info.cb = sizeof(startup_info);
	startup_info.dwFlags = STARTF_USESHOWWINDOW;
	startup_info.wShowWindow = SW_SHOW;
	BOOL result = CreateProcessW(NULL, wide_command, NULL, NULL, FALSE, 0,
		NULL, NULL, &startup_info, &process_info);
	if (result) {
		CloseHandle(process_info.hThread);
		CloseHandle(process_info.hProcess);
	}
	free(wide_command);
	return result ? 33 : 0;
}

hsp_path::utf8_string hsp_path_utf8_from_wide(const wchar_t* text)
{
	if (text == NULL) return hsp_path::utf8_string();
	int length = utf16_to_utf8_strict(NULL, text, 0);
	if (length <= 0) return hsp_path::utf8_string();
	char* result = (char*)malloc((size_t)length);
	if (result == NULL) return hsp_path::utf8_string();
	if (utf16_to_utf8_strict(result, text, length) == 0) {
		free(result);
		return hsp_path::utf8_string();
	}
	return hsp_path::utf8_string(result);
}

int hsp_path_dirlist_utf8(hsp_path::utf8_view pattern, int flags, hsp_path_list_callback callback, void* user_data)
{
	if (callback == NULL) return -1;
	wchar_t* wide_pattern = hsp_path_utf8_to_wide(pattern.c_str());
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
		hsp_path::utf8_string name = selected ? hsp_path_utf8_from_wide(data.cFileName) : hsp_path::utf8_string();
		if (name && name.c_str()[0] != 0 && strcmp(name.c_str(), ".") != 0 && strcmp(name.c_str(), "..") != 0) {
			++count;
			if (callback(name.as_view(), user_data) != 0) {
				result = -1;
				break;
			}
		}
		if (!FindNextFileW(handle, &data)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) break;
			result = -1;
			break;
		}
	}
	FindClose(handle);
	return result < 0 ? result : count;
}

int hsp_path_remove_utf8(hsp_path::utf8_view path)
{
	fs::path fs_path;
	if (!hsp_path_make_fs_path(path.c_str(), fs_path)) return -1;
#if defined(HSPWIN) || defined(_WIN32)
	return _wremove(fs_path.c_str());
#else
	std::error_code error;
	if (!fs::remove(fs_path, error) || error) return -1;
	return 0;
#endif
}

FILE* hsp_path_fopen_utf8(hsp_path::utf8_view path, const char* mode)
{
	if (path.c_str() == NULL || mode == NULL) return NULL;
	fs::path fs_path;
	if (!hsp_path_make_fs_path(path.c_str(), fs_path)) return NULL;
	std::wstring wide_mode;
	for (const char* character = mode; *character != 0; ++character) {
		wide_mode.push_back((wchar_t)(unsigned char)*character);
	}
	return _wfopen(fs_path.c_str(), wide_mode.c_str());
}

int hsp_path_get_module_filename_utf8(std::string& result)
{
	std::vector<wchar_t> module_path(256);
	DWORD length;
	do {
		length = GetModuleFileNameW(NULL, module_path.data(), (DWORD)module_path.size());
		if (length == 0) return -1;
		if (length + 1 < module_path.size()) break;
		module_path.resize(module_path.size() * 2);
	} while (true);

	hsp_path::utf8_string utf8_path = hsp_path_utf8_from_wide(module_path.data());
	if (!utf8_path) return -1;
	result = utf8_path.c_str();
	return 0;
}

int hsp_path_get_module_directory_utf8(std::string& result)
{
	if (hsp_path_get_module_filename_utf8(result) != 0) return -1;
	try {
		result = fs::u8path(result).parent_path().u8string();
		return 0;
	}
	catch (const std::exception&) {
		result.clear();
		return -1;
	}
}

int hsp_path_get_current_directory_utf8(std::string& result)
{
	result.clear();
	DWORD length = GetCurrentDirectoryW(0, NULL);
	if (length == 0) return -1;
	std::vector<wchar_t> current_directory((size_t)length);
	DWORD actual_length = GetCurrentDirectoryW(length, current_directory.data());
	if (actual_length == 0 || actual_length >= length) return -1;

	hsp_path::utf8_string utf8_path = hsp_path_utf8_from_wide(current_directory.data());
	if (!utf8_path) return -1;
	result = utf8_path.c_str();
	return 0;
}

hsp_path::utf8_string hsp_path_from_ansi(hsp_path::ansi_view path)
{
	if (path.c_str() == NULL) return hsp_path::utf8_string();

	int wide_length = ansi_to_utf16_strict(NULL, path.c_str(), 0);
	if (wide_length <= 0) return hsp_path::utf8_string();
	wchar_t* wide_path = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)wide_length);
	if (wide_path == NULL) return hsp_path::utf8_string();
	if (ansi_to_utf16_strict(wide_path, path.c_str(), wide_length) == 0) {
		free(wide_path);
		return hsp_path::utf8_string();
	}

	hsp_path::utf8_string result = hsp_path_utf8_from_wide(wide_path);
	free(wide_path);
	return result;
}

hsp_path::ansi_string hsp_path_to_ansi(hsp_path::utf8_view path)
{
	wchar_t* wide_path = hsp_path_utf8_to_wide(path.c_str());
	if (wide_path == NULL) return hsp_path::ansi_string();

	int ansi_length = utf16_to_ansi_strict(NULL, wide_path, 0);
	if (ansi_length <= 0) {
		free(wide_path);
		return hsp_path::ansi_string();
	}
	char* result = (char*)malloc((size_t)ansi_length);
	if (result == NULL) {
		free(wide_path);
		return hsp_path::ansi_string();
	}
	if (utf16_to_ansi_strict(result, wide_path, ansi_length) == 0) {
		free(wide_path);
		free(result);
		return hsp_path::ansi_string();
	}
	free(wide_path);
	return hsp_path::ansi_string(result);
}

int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::utf8_view name)
{
	if (name.c_str() == NULL || !hsp_path_utf8_is_valid((const unsigned char*)name.c_str())) return -1;
	if (hsp_path_get_module_directory_utf8(result) != 0) return -1;
	result += "\\hsptv\\";
	result += name.c_str();
	return 0;
}

int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::ansi_view name)
{
	hsp_path::utf8_string utf8_name = hsp_path_from_ansi(name);
	if (!utf8_name) return -1;
	return hsp_path_get_hsptv_path_utf8(result, utf8_name.as_view());
}

#else

#include <sys/stat.h>
#include <glob.h>

int hsp_path_remove_utf8(hsp_path::utf8_view path)
{
	fs::path fs_path;
	std::error_code error;
	if (!hsp_path_make_fs_path(path.c_str(), fs_path) || !fs::remove(fs_path, error) || error) return -1;
	return 0;
}

int hsp_path_dirlist_utf8(hsp_path::utf8_view pattern, int flags, hsp_path_list_callback callback, void* user_data)
{
	if (callback == NULL || !hsp_path_utf8_is_valid((const unsigned char*)pattern.c_str())) return -1;
	glob_t matches;
	memset(&matches, 0, sizeof(matches));
	int glob_result = glob(pattern.c_str(), 0, NULL, &matches);
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
		if (callback(hsp_path::utf8_view(basename), user_data) != 0) {
			result = -1;
			break;
		}
	}
	globfree(&matches);
	return result < 0 ? result : count;
}

hsp_path::utf8_string hsp_path_from_ansi(hsp_path::ansi_view path)
{
	if (path.c_str() == NULL) return hsp_path::utf8_string();
	size_t length = strlen(path.c_str()) + 1;
	char* result = (char*)malloc(length);
	if (result != NULL) memcpy(result, path.c_str(), length);
	return hsp_path::utf8_string(result);
}

hsp_path::ansi_string hsp_path_to_ansi(hsp_path::utf8_view path)
{
	if (path.c_str() == NULL || !hsp_path_utf8_is_valid((const unsigned char*)path.c_str())) return hsp_path::ansi_string();
	size_t length = strlen(path.c_str()) + 1;
	char* result = (char*)malloc(length);
	if (result != NULL) memcpy(result, path.c_str(), length);
	return hsp_path::ansi_string(result);
}

FILE* hsp_path_fopen_utf8(hsp_path::utf8_view path, const char* mode)
{
	if (mode == NULL) return NULL;
	fs::path fs_path;
	if (!hsp_path_make_fs_path(path.c_str(), fs_path)) return NULL;
	return fopen(fs_path.c_str(), mode);
}

#endif

FILE* hsp_path_fopen(hsp_path::path_view path, const char* mode)
{
#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_fopen_utf8(path, mode);
#else
	if (path.c_str() == NULL || mode == NULL) return NULL;
	return fopen(path.c_str(), mode);
#endif
}

int64_t hsp_path_filesize(hsp_path::path_view path)
{
#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_filesize_utf8(path);
#else
	struct _stat64 status;
	if (path.c_str() == NULL || _stat64(path.c_str(), &status) != 0 ||
		(status.st_mode & _S_IFMT) != _S_IFREG) return -1;
	return (int64_t)status.st_size;
#endif
}

int hsp_path_file_exists(hsp_path::path_view path)
{
#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_file_exists_utf8(path);
#else
	struct _stat64 status;
	if (path.c_str() == NULL) return 0;
	return _stat64(path.c_str(), &status) == 0;
#endif
}

int hsp_path_remove(hsp_path::path_view path)
{
#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_remove_utf8(path);
#else
	if (path.c_str() == NULL) return -1;
	return remove(path.c_str());
#endif
}

int hsp_path_getpath(hsp_path::path_view path, char* output, size_t output_size, int mode)
{
	if (path.c_str() == NULL || output == NULL || output_size == 0) return 0;

#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_getpath_utf8(path, output, output_size, mode);
#else
	// The legacy implementation accepts a mutable input and has no output
	// size parameter. Work on private buffers so the generic API does not
	// inherit input mutation, then enforce its bounded-output contract here.
	if (strlen(path.c_str()) >= _MAX_PATH) return 0;
	std::string source(path.c_str());
	source.push_back('\0');
	std::string result(source.size(), '\0');
	getpath(&source[0], &result[0], mode);
	size_t result_length = strlen(result.c_str());
	if (result_length + 1 > output_size) return 0;
	memcpy(output, result.c_str(), result_length + 1);
	return 1;
#endif
}
