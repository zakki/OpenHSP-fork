#pragma once

#include <string>
#include <unordered_map>

#include "chsp_ast.h"
#include "chsp_frontend.h"

class CMemBuf;

namespace chsp
{

std::string ModuleLibraryName( const std::string &file_stem );
std::string DefaultModuleFileStem( const char *source_name, size_t module_index );
std::string ModuleFileStem( const ChspAstModule &module, const char *source_name, size_t module_index );

void WriteModuleHeaderToHsp( CMemBuf &buf, const std::string &module_tag, const std::string &file_stem,
							 ChspNativeTarget target );
void WriteModuleFooterToHsp( CMemBuf &buf, const std::string &module_tag );
void WriteOriginalSourceMarker( CMemBuf &buf, int line_number, const char *source_name );

void WriteFunctionDeclToHsp( CMemBuf &buf, const ChspAstFunction &func, const std::string &cpp_name );
std::string HspInternalFunctionDeclName( const ChspAstFunction &func );
void WriteFunctionDeclToHspInternal( CMemBuf &buf, const ChspAstFunction &func, const std::string &name,
									 const std::string &cpp_name,
									 const std::unordered_map<std::string, bool> &function_array_metadata_needs );
void WriteFunctionWrapperToHsp( CMemBuf &buf, const ChspAstFunction &func, const std::string &internal_name,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs );
void WritePluginFunctionDeclToHsp( CMemBuf &buf, const ChspAstFunction &func, int command_id );

} // namespace chsp
