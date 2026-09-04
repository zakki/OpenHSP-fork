#pragma once

#include <memory>
#include <string>
#include <vector>

#include "chsp_frontend_v2.h"

class CLogger;
void strcase2( const char *str, char *str2 );

namespace chsputil
{

inline std::string Trim( const std::string &src )
{
	const auto begin = src.find_first_not_of( " \t\r\n" );
	if ( begin == std::string::npos ) {
		return "";
	}
	const auto end = src.find_last_not_of( " \t\r\n" );
	return src.substr( begin, end - begin + 1 );
}

inline bool StartsWith( const std::string &src, const char *prefix )
{
	return src.rfind( prefix, 0 ) == 0;
}

inline std::string NormalizeIdentifier( const std::string &src )
{
	std::vector<char> buf( src.size() + 1, '\0' );
	strcase2( src.c_str(), buf.data() );
	return std::string( buf.data() );
}

} // namespace chsputil
