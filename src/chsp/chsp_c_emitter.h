#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "chsp_ast.h"
#include "chsp_builtin_map.h"
#include "chsp_frontend.h"
#include "chsp_types.h"

class CLogger;
class CMemBuf;

namespace chsp
{

void WriteNativePreamble( CMemBuf &native_out, ChspNativeTarget target );
void WriteNativeSourceBlocks( CMemBuf &native_out, const ChspAstModule &module );
void WriteFunctionPrototypeToNative( CMemBuf &buf, const ChspAstFunction &func, const ChspAstModule &module,
									 const std::unordered_map<std::string, std::string> &function_cpp_names,
									 const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
									 const std::unordered_map<std::string, bool> &function_array_metadata_needs,
									 ChspNativeTarget target, const ChspBuiltinMap &builtin_map );
bool WriteFunctionToNative( CMemBuf &buf, const ChspAstModule &module, const ChspAstFunction &func,
							const std::unordered_map<std::string, std::string> &function_cpp_names,
							const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
							const std::unordered_map<std::string, bool> &function_array_metadata_needs,
							ChspNativeTarget target, CLogger &logger, const ChspBuiltinMap &builtin_map );
bool WritePluginNativeDispatch( CMemBuf &buf, const ChspAstModule &module,
								const std::unordered_map<std::string, std::string> &function_cpp_names,
								const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs,
								CLogger &logger, const ChspBuiltinMap &builtin_map );

} // namespace chsp
