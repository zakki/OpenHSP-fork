//
// Text file encoding conversion for note commands.
//
#include "hsp3textcodec.h"

#if HSP_TEXTCODEC_SUPPORTED

#include <limits.h>
#include <stdint.h>
#include <vector>

# if defined(HSPWIN) || defined(_WIN32)
#include <icu.h>
#else
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#endif

static bool hsp_textcodec_size_ok(size_t size)
{
	return size <= (size_t)INT32_MAX;
}

static bool hsp_textcodec_open(const char *encoding, UConverter **converter)
{
	if (encoding == NULL || *encoding == 0) return false;
	UErrorCode status = U_ZERO_ERROR;
	*converter = ucnv_open(encoding, &status);
	if (U_FAILURE(status) || *converter == NULL) return false;
	return true;
}

bool hsp_text_decode(const char *encoding, const char *input, size_t input_size,
	std::string &output)
{
	output.clear();
	if (input == NULL && input_size != 0) return false;
	if (!hsp_textcodec_size_ok(input_size)) return false;

	UConverter *converter = NULL;
	if (!hsp_textcodec_open(encoding, &converter)) return false;
	UErrorCode callback_status = U_ZERO_ERROR;
	ucnv_setToUCallBack(converter, UCNV_TO_U_CALLBACK_STOP, NULL, NULL, NULL,
		&callback_status);
	if (U_FAILURE(callback_status)) {
		ucnv_close(converter);
		return false;
	}

	const char *source = input != NULL ? input : "";
	UErrorCode status = U_ZERO_ERROR;
	int32_t source_size = (int32_t)input_size;
	int32_t unicode_size = ucnv_toUChars(converter, NULL, 0, source,
		source_size, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		ucnv_close(converter);
		return false;
	}

	std::vector<UChar> unicode((size_t)unicode_size);
	status = U_ZERO_ERROR;
	ucnv_toUChars(converter, unicode.empty() ? NULL : &unicode[0], unicode_size, source,
		source_size, &status);
	if (U_FAILURE(status)) {
		ucnv_close(converter);
		return false;
	}

	status = U_ZERO_ERROR;
	int32_t utf8_size = 0;
	u_strToUTF8(NULL, 0, &utf8_size, unicode.empty() ? NULL : &unicode[0],
		unicode_size, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		ucnv_close(converter);
		return false;
	}

	output.resize((size_t)utf8_size);
	status = U_ZERO_ERROR;
	u_strToUTF8(output.empty() ? NULL : &output[0], utf8_size, NULL,
		unicode.empty() ? NULL : &unicode[0], unicode_size, &status);
	ucnv_close(converter);
	if (U_FAILURE(status)) {
		output.clear();
		return false;
	}
	return true;
}

bool hsp_text_encode(const char *encoding, const char *input, size_t input_size,
	std::string &output)
{
	output.clear();
	if (input == NULL && input_size != 0) return false;
	if (!hsp_textcodec_size_ok(input_size)) return false;

	const char *source = input != NULL ? input : "";
	UErrorCode status = U_ZERO_ERROR;
	int32_t source_size = (int32_t)input_size;
	int32_t unicode_size = 0;
	u_strFromUTF8(NULL, 0, &unicode_size, source, source_size, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) return false;

	std::vector<UChar> unicode((size_t)unicode_size);
	status = U_ZERO_ERROR;
	u_strFromUTF8(unicode.empty() ? NULL : &unicode[0], unicode_size, NULL, source,
		source_size, &status);
	if (U_FAILURE(status)) return false;

	UConverter *converter = NULL;
	if (!hsp_textcodec_open(encoding, &converter)) return false;
	UErrorCode callback_status = U_ZERO_ERROR;
	ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, NULL, NULL, NULL,
		&callback_status);
	if (U_FAILURE(callback_status)) {
		ucnv_close(converter);
		return false;
	}

	status = U_ZERO_ERROR;
	int32_t output_size = ucnv_fromUChars(converter, NULL, 0,
		unicode.empty() ? NULL : &unicode[0], unicode_size, &status);
	if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
		ucnv_close(converter);
		return false;
	}

	output.resize((size_t)output_size);
	status = U_ZERO_ERROR;
	ucnv_fromUChars(converter, output.empty() ? NULL : &output[0], output_size,
		unicode.empty() ? NULL : &unicode[0], unicode_size, &status);
	ucnv_close(converter);
	if (U_FAILURE(status)) {
		output.clear();
		return false;
	}
	return true;
}

#else

bool hsp_text_decode(const char *, const char *, size_t, std::string &output)
{
	output.clear();
	return false;
}

bool hsp_text_encode(const char *, const char *, size_t, std::string &output)
{
	output.clear();
	return false;
}

#endif
