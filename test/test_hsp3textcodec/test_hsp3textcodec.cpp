#include <assert.h>
#include <string.h>

#include <string>

#include "../../src/hsp3/hsp3textcodec.h"

int main()
{
	const char utf8[] = "日本語\n";
	const unsigned char cp932_bytes[] = {
		0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea, 0x0a
	};
	const unsigned char eucjp_bytes[] = {
		0xc6, 0xfc, 0xcb, 0xdc, 0xb8, 0xec, 0x0a
	};
	std::string converted;

	assert(hsp_text_decode("CP932", (const char *)cp932_bytes,
		sizeof(cp932_bytes), converted));
	assert(converted == utf8);
	assert(hsp_text_encode("CP932", utf8, strlen(utf8), converted));
	assert(converted.size() == sizeof(cp932_bytes));
	assert(memcmp(converted.data(), cp932_bytes, sizeof(cp932_bytes)) == 0);

	assert(hsp_text_decode("EUC-JP", (const char *)eucjp_bytes,
		sizeof(eucjp_bytes), converted));
	assert(converted == utf8);
	assert(hsp_text_encode("EUC-JP", utf8, strlen(utf8), converted));
	assert(converted.size() == sizeof(eucjp_bytes));
	assert(memcmp(converted.data(), eucjp_bytes, sizeof(eucjp_bytes)) == 0);

	const unsigned char invalid_utf8[] = { 0xe3, 0x81 };
	assert(!hsp_text_decode("UTF-8", (const char *)invalid_utf8,
		sizeof(invalid_utf8), converted));
	assert(!hsp_text_encode("US-ASCII", utf8, strlen(utf8), converted));
	assert(!hsp_text_decode("not-an-encoding", "text", 4, converted));
	return 0;
}
