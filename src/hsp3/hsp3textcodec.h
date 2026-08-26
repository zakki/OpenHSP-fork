//
// Text file encoding conversion for note commands.
//
#ifndef __hsp3textcodec_h
#define __hsp3textcodec_h

#include <stddef.h>
#include <string>

// Text encoding conversion is available when the HSP string representation
// is UTF-8.  POSIX targets use UTF-8 through hsp3config.h as well, but keep
// this condition independent so the header can also be used by utilities.
#if defined(HSPUTF8) || (!defined(HSPWIN) && !defined(_WIN32))
#define HSP_TEXTCODEC_SUPPORTED 1
#else
#define HSP_TEXTCODEC_SUPPORTED 0
#endif

// Convert bytes in the named ICU encoding to the runtime's UTF-8 string.
// The conversion is strict: malformed input returns false.
bool hsp_text_decode(const char *encoding, const char *input, size_t input_size,
	std::string &output);

// Convert the runtime's UTF-8 string to bytes in the named ICU encoding.
// The conversion is strict: unrepresentable characters return false.
bool hsp_text_encode(const char *encoding, const char *input, size_t input_size,
	std::string &output);

#endif
