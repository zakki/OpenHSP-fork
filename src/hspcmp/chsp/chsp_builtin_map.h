#pragma once

#include <string>
#include <vector>

#include "chsp_frontend_v2.h"

struct ChspBuiltinEntry
{
	std::string hsp_name;
	std::string c_target;
	std::string cpp_target;
	int min_args = -1; // -1 = any
	int max_args = -1; // -1 = no upper bound
};

using ChspBuiltinMap = std::vector<ChspBuiltinEntry>;

// Load from {compath}/chsp/chsp_builtins.tsv
// Returns error message on failure, empty string on success
std::string LoadChspBuiltinMap( const char *compath, ChspBuiltinMap &out );

// Returns target function name, or "" if not found
std::string LookupChspBuiltin( const ChspBuiltinMap &map, const std::string &name, size_t arg_count,
							   ChspNativeTarget target );
