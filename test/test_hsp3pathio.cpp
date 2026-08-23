#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <string>
#include <vector>

#include "../src/hsp3/hsp3pathio.h"

static int count_matching_path(hsp_path::utf8_view name, void* user_data)
{
	int* count = (int*)user_data;
	if (strcmp(name.c_str(), "hsp3pathio-日本語-😀.tmp") == 0) ++*count;
	return 0;
}

int main()
{
	static_assert(!std::is_same<hsp_path::utf8_view, hsp_path::ansi_view>::value, "UTF-8 and ANSI views must differ");

	const char* path = "hsp3pathio-日本語-😀.tmp";
	const char* expected = "UTF-8 path I/O\n";

	FILE* output = hsp_path_fopen(hsp_path::path_view(path), "wb");
	assert(output != NULL);
	assert(fwrite(expected, 1, strlen(expected), output) == strlen(expected));
	assert(fclose(output) == 0);
	assert(hsp_path_file_exists(hsp_path::path_view(path)));
	assert(hsp_path_filesize(hsp_path::path_view(path)) == (int64_t)strlen(expected));

	char buffer[64] = {};
	FILE* input = hsp_path_fopen(hsp_path::path_view(path), "rb");
	assert(input != NULL);
	assert(fread(buffer, 1, sizeof(buffer) - 1, input) == strlen(expected));
	assert(fclose(input) == 0);
	assert(strcmp(buffer, expected) == 0);
	int matching_paths = 0;
	assert(hsp_path_dirlist_utf8(hsp_path::utf8_view("hsp3pathio-*.tmp"), 1, count_matching_path, &matching_paths) >= 1);
	assert(matching_paths == 1);

	const char invalid_utf8[] = "hsp3pathio-\xf0\x28\x8c\x28.tmp";
	assert(hsp_path_fopen_utf8(hsp_path::utf8_view(invalid_utf8), "rb") == NULL);
	assert(!hsp_path_file_exists_utf8(hsp_path::utf8_view(invalid_utf8)));
	assert(hsp_path_filesize_utf8(hsp_path::utf8_view(invalid_utf8)) < 0);
	std::string compatibility_path;
	assert(hsp_path_from_ansi(compatibility_path,
		hsp_path::ansi_view("compatibility-日本語")) == 0);
	assert(strcmp(compatibility_path.c_str(), "compatibility-日本語") == 0);
	std::string compatibility_output;
	assert(hsp_path_to_ansi(compatibility_output,
		hsp_path::utf8_view(compatibility_path.c_str())) == 0);
	assert(strcmp(compatibility_output.c_str(), compatibility_path.c_str()) == 0);
	assert(hsp_path_from_ansi(compatibility_path, hsp_path::ansi_view("")) == 0);
	assert(compatibility_path.empty());
	assert(hsp_path_from_ansi(compatibility_path, hsp_path::ansi_view(NULL)) != 0);

#if defined(HSPWIN) || defined(_WIN32)
	const wchar_t wide_path[] = L"wide-日本語-😀";
	std::string wide_path_utf8;
	assert(hsp_path_utf8_from_wide(wide_path_utf8, wide_path) == 0);
	assert(strcmp(wide_path_utf8.c_str(), "wide-日本語-😀") == 0);
#endif

	char component[64];
	assert(hsp_path_getpath(hsp_path::path_view("dir/日本語-😀.hsp"), component, sizeof(component), 8));
	assert(strcmp(component, "日本語-😀.hsp") == 0);
	assert(hsp_path_getpath(hsp_path::path_view("dir/日本語-😀.hsp"), component, sizeof(component), 1 | 8));
	assert(strcmp(component, "日本語-😀") == 0);
	assert(hsp_path_getpath(hsp_path::path_view("dir/日本語-😀.hsp"), component, sizeof(component), 2));
	assert(strcmp(component, ".hsp") == 0);
	assert(hsp_path_getpath(hsp_path::path_view("dir/日本語-😀.hsp"), component, sizeof(component), 32));
	assert(strcmp(component, "dir/") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view(".bashrc"), component, sizeof(component), 1 | 8));
	assert(strcmp(component, "") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view(".bashrc"), component, sizeof(component), 2));
	assert(strcmp(component, ".bashrc") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view(".bashrc"), component, sizeof(component), 1));
	assert(strcmp(component, "") == 0);
	std::string hsptv_path;
	assert(hsp_path_get_hsptv_path_utf8(hsptv_path,
		hsp_path::utf8_view("/runtime/hsptv"), hsp_path::utf8_view("素材-日本語.dat")) == 0);
	assert(hsptv_path == "/runtime/hsptv/素材-日本語.dat");
	assert(hsp_path_get_hsptv_path_utf8(hsptv_path,
		hsp_path::utf8_view(""), hsp_path::utf8_view("素材-日本語.dat")) == 0);
	assert(hsptv_path == "素材-日本語.dat");
	const char invalid_hsptv_name[] = "素材-\xf0\x28\x8c\x28.dat";
	assert(hsp_path_get_hsptv_path_utf8(hsptv_path,
		hsp_path::utf8_view("/runtime/hsptv"), hsp_path::utf8_view(invalid_hsptv_name)) != 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view("C:foo.hsp"), component, sizeof(component), 8));
	assert(strcmp(component, "foo.hsp") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view("C:"), component, sizeof(component), 32));
	assert(strcmp(component, "C:") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view("C:dir/foo.hsp"), component, sizeof(component), 8));
	assert(strcmp(component, "foo.hsp") == 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view("C:dir/foo.hsp"), component, sizeof(component), 32));
	assert(strcmp(component, "C:dir/") == 0);
#if defined(HSPWIN) || defined(_WIN32)
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view("dir\\file.hsp"), component, sizeof(component), 32));
	assert(strcmp(component, "dir\\") == 0);
#endif
	assert(!hsp_path_getpath(hsp_path::path_view("dir/日本語-😀.hsp"), component, 4, 8));
	assert(!hsp_path_getpath_utf8(hsp_path::utf8_view(invalid_utf8), component, sizeof(component), 8));
	const char truncated_utf8_3[] = "truncated-\xe2\x82";
	const char truncated_utf8_4[] = "truncated-\xf0\x9f\x98";
	assert(!hsp_path_getpath_utf8(hsp_path::utf8_view(truncated_utf8_3), component, sizeof(component), 8));
	assert(!hsp_path_getpath_utf8(hsp_path::utf8_view(truncated_utf8_4), component, sizeof(component), 8));
	std::string long_path(512, 'a');
	long_path += "/long-日本語-😀.hsp";
	std::vector<char> long_component(long_path.size() + 1, 0);
	assert(hsp_path_getpath_utf8(hsp_path::utf8_view(long_path.c_str()),
		long_component.data(), long_component.size(), 8));
	assert(strcmp(long_component.data(), "long-日本語-😀.hsp") == 0);

	assert(hsp_path_remove(hsp_path::path_view(path)) == 0);
	assert(!hsp_path_file_exists(hsp_path::path_view(path)));
	return 0;
}
