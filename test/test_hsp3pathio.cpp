#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/hsp3/hsp3pathio.h"

static int count_matching_path(const char* name, void* user_data)
{
	int* count = (int*)user_data;
	if (strcmp(name, "hsp3pathio-日本語-😀.tmp") == 0) ++*count;
	return 0;
}

int main()
{
	const char* path = "hsp3pathio-日本語-😀.tmp";
	const char* expected = "UTF-8 path I/O\n";

	FILE* output = hsp_fopen_utf8(path, "wb");
	assert(output != NULL);
	assert(fwrite(expected, 1, strlen(expected), output) == strlen(expected));
	assert(fclose(output) == 0);
	assert(hsp_file_exists_utf8(path));
	assert(hsp_filesize_utf8(path) == (int64_t)strlen(expected));

	char buffer[64] = {};
	FILE* input = hsp_fopen_utf8(path, "rb");
	assert(input != NULL);
	assert(fread(buffer, 1, sizeof(buffer) - 1, input) == strlen(expected));
	assert(fclose(input) == 0);
	assert(strcmp(buffer, expected) == 0);
	int matching_paths = 0;
	assert(hsp_dirlist_utf8("hsp3pathio-*.tmp", 1, count_matching_path, &matching_paths) >= 1);
	assert(matching_paths == 1);

	const char invalid_utf8[] = "hsp3pathio-\xf0\x28\x8c\x28.tmp";
	assert(hsp_fopen_utf8(invalid_utf8, "rb") == NULL);
	assert(!hsp_file_exists_utf8(invalid_utf8));
	assert(hsp_filesize_utf8(invalid_utf8) < 0);
	char* compatibility_path = hsp_path_from_ansi("compatibility-日本語");
	assert(compatibility_path != NULL);
	assert(strcmp(compatibility_path, "compatibility-日本語") == 0);
	char* compatibility_output = hsp_path_to_ansi(compatibility_path);
	assert(compatibility_output != NULL);
	assert(strcmp(compatibility_output, compatibility_path) == 0);
	free(compatibility_output);
	free(compatibility_path);

	char component[64];
	assert(hsp_getpath_utf8("dir/日本語-😀.hsp", component, sizeof(component), 8));
	assert(strcmp(component, "日本語-😀.hsp") == 0);
	assert(hsp_getpath_utf8("dir/日本語-😀.hsp", component, sizeof(component), 1 | 8));
	assert(strcmp(component, "日本語-😀") == 0);
	assert(hsp_getpath_utf8("dir/日本語-😀.hsp", component, sizeof(component), 2));
	assert(strcmp(component, ".hsp") == 0);
	assert(hsp_getpath_utf8("dir/日本語-😀.hsp", component, sizeof(component), 32));
	assert(strcmp(component, "dir/") == 0);
	assert(!hsp_getpath_utf8("dir/日本語-😀.hsp", component, 4, 8));
	assert(!hsp_getpath_utf8(invalid_utf8, component, sizeof(component), 8));

	assert(hsp_remove_utf8(path) == 0);
	assert(!hsp_file_exists_utf8(path));
	return 0;
}
