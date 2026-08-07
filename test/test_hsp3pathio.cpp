#include <assert.h>
#include <stdio.h>
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

	remove(path);
	return 0;
}
