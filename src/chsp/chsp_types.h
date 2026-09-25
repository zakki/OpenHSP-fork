#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "chsp_ast.h"
#include "chsp_builtin_map.h"
#include "chsp_frontend.h"
#include "chsp_util.h"

namespace chsp
{

struct TranslateContext
{
	ChspNativeTarget target = ChspNativeTarget::Plugin;
	const ChspBuiltinMap *builtin_map = nullptr;
	std::unordered_set<std::string> array_names;
	std::unordered_set<std::string> argument_array_names;
	std::unordered_map<std::string, std::string> declared_native_functions; // normalized name -> original case name
	std::unordered_map<std::string, std::string> identifier_cpp_names;
	std::unordered_map<std::string, std::string> function_cpp_names;
	std::unordered_map<std::string, const ChspAstFunction *> function_defs;
	std::unordered_map<std::string, bool> function_array_metadata_needs;
	std::unordered_map<std::string, std::vector<std::string>> array_dimension_exprs;
	std::vector<std::string> loop_stack;
	int temp_var_count = 0;
};

std::vector<std::string> FixedDimsToDimensionExprs( const std::vector<int> &dims );
int FlatArraySize( const std::vector<int> &dims );
std::string NormalizeScopedName( const std::string &name );
bool IsDefCFunc( const ChspAstFunction &func );
std::string SanitizeForCppIdentifier( const std::string &name );
std::string MakeFunctionCppName( const ChspAstFunction &func );
std::unordered_map<std::string, std::string> BuildFunctionCppNames( const ChspAstProgram &program );
std::unordered_map<std::string, const ChspAstFunction *> BuildFunctionDefs( const ChspAstProgram &program );
std::unordered_map<std::string, std::string> BuildIdentifierCppNames( const ChspAstFunction &func );
std::string LookupCppIdentifier( const TranslateContext &ctx, const std::string &name );
std::string ArrayDimensionCppName( const ChspAstFunction &func, size_t param_index, int dimension_index );
std::vector<std::string> ArrayDimensionExprsForName( const TranslateContext &ctx, const std::string &name );
std::unordered_set<std::string> BuildFunctionArrayNames( const ChspAstFunction &func );
bool FunctionUsesArrayMetadataDirect( const ChspAstFunction &func );
std::unordered_map<std::string, bool> BuildFunctionArrayMetadataNeeds(
	const ChspAstProgram &program, const std::unordered_map<std::string, const ChspAstFunction *> &function_defs );
bool FunctionNeedsArrayMetadata( const std::unordered_map<std::string, bool> &function_array_metadata_needs,
								 const ChspAstFunction &func );
std::string ToHspParamType( const ChspAstParam &param );
std::string ToPublicHspParamType( const ChspAstParam &param );
std::string PluginCommandName( const ChspAstFunction &func );
std::string ToCppType( const ChspAstParam &param );
std::string ToNativeType( const ChspAstParam &param, ChspNativeTarget target );
std::string DefaultReturnExpr( const std::string &type );
std::string BuiltinTarget( const std::string &name, size_t arg_count, ChspNativeTarget target );
std::string OperatorText( int op );
int ExprPrecedence( const ChspAstExpr &expr );
TranslateContext BuildTranslateContext( const ChspAstFunction &func, const ChspAstModule &module,
										const std::unordered_map<std::string, std::string> &function_cpp_names,
										const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
										const std::unordered_map<std::string, bool> &function_array_metadata_needs,
										ChspNativeTarget target, const ChspBuiltinMap &builtin_map );

} // namespace chsp
