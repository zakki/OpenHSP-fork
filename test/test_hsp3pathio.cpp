#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/hsp3/hsp3pathio.h"

int main()
{
	const char* path = "hsp3pathio-日本語-😀.tmp";
	const char* expected = "UTF-8 path I/O\n";

	FILE* output = hsp_fopen_utf8(path, "wb");
	assert(output != NULL);
	assert(fwrite(expected, 1, strlen(expected), output) == strlen(expected));
	assert(fclose(output) == 0);

	char buffer[64] = {};
	FILE* input = hsp_fopen_utf8(path, "rb");
	assert(input != NULL);
	assert(fread(buffer, 1, sizeof(buffer) - 1, input) == strlen(expected));
	assert(fclose(input) == 0);
	assert(strcmp(buffer, expected) == 0);

	const char invalid_utf8[] = "hsp3pathio-\xf0\x28\x8c\x28.tmp";
	assert(hsp_fopen_utf8(invalid_utf8, "rb") == NULL);
	char* compatibility_path = hsp_path_from_ansi("compatibility-日本語");
	assert(compatibility_path != NULL);
	assert(strcmp(compatibility_path, "compatibility-日本語") == 0);
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

	remove(path);
	return 0;
}
