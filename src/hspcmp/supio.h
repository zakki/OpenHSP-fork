
//
//	supio.cpp functions
//
#ifndef __supio_h
#define __supio_h

#include "../hsp3/hsp3config.h"
#include "../hsp3/hsp3pathio.h"

#ifdef HSPUTF8
#define STRLEN utf8strlen
#else
#define STRLEN strlen
#endif

#ifdef HSPWIN
#ifdef HSPUTF8
#include "../hsp3/win32gui/supio_win_unicode.h"
#else
#include "../hsp3/win32gui/supio_win.h"
#endif
#endif

#ifdef HSPIOS
#include "../hsp3/ios/supio_ios.h"
#endif

#ifdef HSPNDK
#include "../hsp3/ndk/supio_ndk.h"
#endif

#ifdef HSPLINUX
#include "../hsp3/linux/supio_linux.h"
#endif

#ifdef HSPEMSCRIPTEN
#include "../hsp3/emscripten/supio_emscripten.h"
#endif

#ifdef HSPMAC
#include "../hsp3/linux/supio_linux.h"
#endif

static inline int hspcmp_getpath(const char* source, char* output, int mode, size_t output_size = HSP_MAX_PATH)
{
	return hsp_path_getpath(hsp_path::path_view(source), output, output_size, mode);
}

#ifdef HSPCMP_DLL
// Convert a UTF-8 path to the legacy ACP contract for DLL error/message output.
class hspcmp_message_path {
public:
	explicit hspcmp_message_path(const char* path)
		: converted_(path != NULL ? hsp_path_to_ansi(hsp_path::utf8_view(path)) : hsp_path::ansi_string())
	{
		if (path == NULL) value_ = "<null path>";
		else value_ = converted_ ? converted_.c_str() : "<unrepresentable UTF-8 path>";
	}

	const char* c_str() const { return value_; }

private:
	hsp_path::ansi_string converted_;
	const char* value_;
};
#endif

#endif
