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

void hsp_path_cut_extension(std::string& path)
{
	try {
		fs::path fs_path = fs::u8path(path);
		fs_path.replace_extension();
		path = fs_path.u8string();
	}
	catch (const std::exception&) {
		// Fall back to a manual strip so a conversion failure never leaves
		// the extension in place (which could make the compiled output
		// silently overwrite the source file).
		size_t separator = path.find_last_of("/\\");
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) {
			path.erase(dot);
		}
	}
}

int hsp_path_is_absolute(const char* path)
{
	if (path == NULL || path[0] == 0) return 0;
	if (path[0] == '/') return 1;
#if defined(HSPWIN) || defined(_WIN32)
	if (path[0] == '\\') return 1;
	if (path[1] == ':') return 1;
#endif
	return 0;
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

int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::utf8_view directory,
	hsp_path::utf8_view name)
{
	if (directory.c_str() == NULL || name.c_str() == NULL ||
		!hsp_path_utf8_is_valid((const unsigned char*)directory.c_str()) ||
		!hsp_path_utf8_is_valid((const unsigned char*)name.c_str())) return -1;
	result = directory.c_str();
	if (!result.empty() && result.back() != '/' && result.back() != '\\') result += '/';
	result += name.c_str();
	return 0;
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

// Shared by hsp_path_utf8_from_wide and hsp_path_to_ansi: both narrow a wide
// string through a *_strict converter of matching signature.
static int hsp_path_narrow_from_wide_strict(std::string& result, const wchar_t* text,
	int (*converter)(char*, const void*, int))
{
	result.clear();
	if (text == NULL) return -1;
	int length = converter(NULL, text, 0);
	if (length <= 0) return -1;
	result.resize((size_t)length, '\0');
	if (converter(&result[0], text, length) == 0) {
		result.clear();
		return -1;
	}
	result.resize((size_t)length - 1);
	return 0;
}

int hsp_path_utf8_from_wide(std::string& result, const wchar_t* text)
{
	return hsp_path_narrow_from_wide_strict(result, text, utf16_to_utf8_strict);
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
		std::string name;
		if (selected && hsp_path_utf8_from_wide(name, data.cFileName) == 0 &&
			!name.empty() && strcmp(name.c_str(), ".") != 0 && strcmp(name.c_str(), "..") != 0) {
			++count;
			if (callback(hsp_path::utf8_view(name.c_str()), user_data) != 0) {
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

	return hsp_path_utf8_from_wide(result, module_path.data());
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

	return hsp_path_utf8_from_wide(result, current_directory.data());
}

int hsp_path_from_ansi(std::string& result, hsp_path::ansi_view path)
{
	result.clear();
	if (path.c_str() == NULL) return -1;

	int wide_length = ansi_to_utf16_strict(NULL, path.c_str(), 0);
	if (wide_length <= 0) return -1;
	std::vector<wchar_t> wide_path((size_t)wide_length);
	if (ansi_to_utf16_strict(wide_path.data(), path.c_str(), wide_length) == 0) {
		return -1;
	}

	return hsp_path_utf8_from_wide(result, wide_path.data());
}

int hsp_path_to_ansi(std::string& result, hsp_path::utf8_view path)
{
	result.clear();
	wchar_t* wide_path = hsp_path_utf8_to_wide(path.c_str());
	if (wide_path == NULL) return -1;
	int status = hsp_path_narrow_from_wide_strict(result, wide_path, utf16_to_ansi_strict);
	free(wide_path);
	return status;
}

// Shared by the hsp_path_get_*() wrappers below: run a UTF-8 accessor and,
// unless UTF-8 is already the target's default representation, convert its
// result to ANSI.
static int hsp_path_convert_to_default(std::string& result, int (*to_utf8)(std::string&))
{
#if HSP_PATHIO_DEFAULT_UTF8
	return to_utf8(result);
#else
	std::string utf8_path;
	if (to_utf8(utf8_path) != 0) return -1;
	return hsp_path_to_ansi(result, hsp_path::utf8_view(utf8_path.c_str()));
#endif
}

int hsp_path_get_module_filename(std::string& result)
{
	return hsp_path_convert_to_default(result, hsp_path_get_module_filename_utf8);
}

int hsp_path_get_module_directory(std::string& result)
{
	return hsp_path_convert_to_default(result, hsp_path_get_module_directory_utf8);
}

int hsp_path_get_current_directory(std::string& result)
{
	return hsp_path_convert_to_default(result, hsp_path_get_current_directory_utf8);
}

int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::utf8_view name)
{
	if (name.c_str() == NULL || !hsp_path_utf8_is_valid((const unsigned char*)name.c_str())) return -1;
	std::string directory;
	if (hsp_path_get_module_directory_utf8(directory) != 0) return -1;
	directory += "\\hsptv";
	return hsp_path_get_hsptv_path_utf8(result, hsp_path::utf8_view(directory.c_str()), name);
}

int hsp_path_get_hsptv_path_utf8(std::string& result, hsp_path::ansi_view name)
{
	std::string utf8_name;
	if (hsp_path_from_ansi(utf8_name, name) != 0) return -1;
	return hsp_path_get_hsptv_path_utf8(result, hsp_path::utf8_view(utf8_name.c_str()));
}

int hsp_path_get_hsptv_path(std::string& result, hsp_path::path_view name)
{
#if HSP_PATHIO_DEFAULT_UTF8
	return hsp_path_get_hsptv_path_utf8(result, name);
#else
	std::string utf8_path;
	if (hsp_path_get_hsptv_path_utf8(utf8_path, name) != 0) return -1;
	return hsp_path_to_ansi(result, hsp_path::utf8_view(utf8_path.c_str()));
#endif
}

#else

#include <dirent.h>
#include <sys/stat.h>

static bool hsp_path_wildcard_match(const char* text, const char* pattern)
{
	if (pattern[0] == '\0' && text[0] == '\0') return true;
	if (pattern[0] == '*') {
		if (text[0] == '\0' && pattern[1] == '\0') return true;
		if (text[0] == '\0') return false;
		if (pattern[1] == text[0] || pattern[1] == '*') {
			if (hsp_path_wildcard_match(text, pattern + 1)) return true;
		}
		return hsp_path_wildcard_match(text + 1, pattern);
	}
	if (text[0] != '\0' && pattern[0] == text[0]) {
		return hsp_path_wildcard_match(text + 1, pattern + 1);
	}
	return false;
}

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
	DIR* directory = opendir(".");
	if (directory == NULL) return -1;

	int count = 0;
	struct dirent* entry;
	while ((entry = readdir(directory)) != NULL) {
		const char* name = entry->d_name;
		bool selected = true;
		if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) selected = false;

		if (selected && flags != 0) {
			struct stat status;
			if (stat(name, &status) != 0) {
				selected = false;
			}
			else {
				unsigned int filter_mask = 0;
				if (flags & 4) {
					if (S_ISREG(status.st_mode) && name[0] != '.') {
						selected = false;
					}
					else {
						filter_mask = 3;
					}
				}
				if (selected && ((flags ^ filter_mask) & 1) && S_ISDIR(status.st_mode)) selected = false;
				if (selected && ((flags ^ filter_mask) & 2) && name[0] == '.') selected = false;
			}
		}

		if (!selected || !hsp_path_wildcard_match(name, pattern.c_str())) continue;
		++count;
		if (callback(hsp_path::utf8_view(name), user_data) != 0) {
			closedir(directory);
			return -1;
		}
	}
	closedir(directory);
	return count;
}

int hsp_path_from_ansi(std::string& result, hsp_path::ansi_view path)
{
	result.clear();
	if (path.c_str() == NULL) return -1;
	result = path.c_str();
	return 0;
}

int hsp_path_to_ansi(std::string& result, hsp_path::utf8_view path)
{
	result.clear();
	if (path.c_str() == NULL || !hsp_path_utf8_is_valid((const unsigned char*)path.c_str())) return -1;
	result = path.c_str();
	return 0;
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
