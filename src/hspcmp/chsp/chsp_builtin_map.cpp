#include "chsp_builtin_map.h"

#include <filesystem>
#include <fstream>
#include <sstream>

static bool ParseBuiltinLine( const std::string &line, ChspBuiltinEntry &out )
{
	if ( line.empty() || line[0] == '#' ) {
		return false;
	}
	std::istringstream ss( line );
	ChspBuiltinEntry entry;
	if ( !std::getline( ss, entry.hsp_name, '\t' ) || entry.hsp_name.empty() ) {
		return false;
	}
	if ( !std::getline( ss, entry.c_target, '\t' ) || entry.c_target.empty() ) {
		return false;
	}
	if ( !std::getline( ss, entry.cpp_target, '\t' ) || entry.cpp_target.empty() ) {
		return false;
	}
	std::string min_str;
	if ( std::getline( ss, min_str, '\t' ) && !min_str.empty() ) {
		entry.min_args = std::stoi( min_str );
		std::string max_str;
		if ( std::getline( ss, max_str, '\t' ) && !max_str.empty() ) {
			entry.max_args = std::stoi( max_str );
		}
	}
	out = std::move( entry );
	return true;
}

std::string LoadChspBuiltinMap( const char *compath, ChspBuiltinMap &out )
{
	if ( compath == nullptr || compath[0] == '\0' ) {
		return "cHSP: compath is not set; cannot load chsp_builtins.tsv";
	}
	const std::filesystem::path path = std::filesystem::path( compath ) / "chsp" / "chsp_builtins.tsv";
	std::ifstream f( path );
	if ( !f.is_open() ) {
		return "cHSP: cannot open builtin map: " + path.string();
	}
	out.clear();
	std::string line;
	while ( std::getline( f, line ) ) {
		if ( !line.empty() && line.back() == '\r' ) {
			line.pop_back();
		}
		ChspBuiltinEntry entry;
		if ( ParseBuiltinLine( line, entry ) ) {
			out.push_back( std::move( entry ) );
		}
	}
	return "";
}

std::string LookupChspBuiltin( const ChspBuiltinMap &map, const std::string &name, size_t arg_count,
							   ChspNativeTarget target )
{
	for ( const auto &entry : map ) {
		if ( entry.hsp_name != name ) {
			continue;
		}
		if ( entry.min_args >= 0 && static_cast<int>( arg_count ) < entry.min_args ) {
			continue;
		}
		if ( entry.max_args >= 0 && static_cast<int>( arg_count ) > entry.max_args ) {
			continue;
		}
		return ( target == ChspNativeTarget::C || target == ChspNativeTarget::Plugin ) ? entry.c_target
																					   : entry.cpp_target;
	}
	return "";
}
