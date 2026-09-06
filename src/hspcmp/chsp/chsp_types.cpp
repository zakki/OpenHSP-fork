#include "chsp_types.h"

#include <cstdio>
#include <cstdlib>

namespace chsp
{

std::vector<std::string> FixedDimsToDimensionExprs( const std::vector<int> &dims )
{
	std::vector<std::string> out = { "0", "0", "0", "0" };
	for ( size_t i = 0; i < dims.size() && i < 4; ++i ) {
		out[i] = std::to_string( dims[i] );
	}
	return out;
}

int FlatArraySize( const std::vector<int> &dims )
{
	if ( dims.empty() ) {
		return 0;
	}
	int size = 1;
	for ( int dim : dims ) {
		size *= dim;
	}
	return size;
}

std::string NormalizeScopedName( const std::string &name )
{
	const auto at = name.find( '@' );
	if ( at == std::string::npos ) {
		return chsputil::NormalizeIdentifier( name );
	}
	return chsputil::NormalizeIdentifier( name.substr( 0, at ) );
}

bool IsDefCFunc( const ChspAstFunction &func )
{
	return func.return_type != "void";
}

std::string SanitizeForCppIdentifier( const std::string &name )
{
	std::string out;
	for ( unsigned char ch : name ) {
		if ( ch >= 'a' && ch <= 'z' ) {
			out.push_back( static_cast<char>( ch ) );
		} else if ( ch >= '0' && ch <= '9' ) {
			out.push_back( static_cast<char>( ch ) );
		} else if ( ch == '_' ) {
			out += "__";
		} else {
			char buf[4];
			std::snprintf( buf, sizeof( buf ), "_%02x", static_cast<unsigned int>( ch ) );
			out += buf;
		}
	}
	if ( out.empty() ) {
		out = "id";
	}
	return out;
}

std::string MakeFunctionCppName( const ChspAstFunction &func )
{
	return "chsp_func_" + SanitizeForCppIdentifier( NormalizeScopedName( func.name ) );
}

std::unordered_map<std::string, std::string> BuildFunctionCppNames( const ChspAstProgram &program )
{
	std::unordered_map<std::string, std::string> names;
	for ( const auto &module : program.modules ) {
		for ( const auto &function : module.functions ) {
			names[NormalizeScopedName( function.name )] = MakeFunctionCppName( function );
		}
	}
	return names;
}

std::unordered_map<std::string, const ChspAstFunction *> BuildFunctionDefs( const ChspAstProgram &program )
{
	std::unordered_map<std::string, const ChspAstFunction *> defs;
	for ( const auto &module : program.modules ) {
		for ( const auto &function : module.functions ) {
			defs[NormalizeScopedName( function.name )] = &function;
		}
	}
	return defs;
}

std::unordered_map<std::string, std::string> BuildIdentifierCppNames( const ChspAstFunction &func )
{
	std::unordered_map<std::string, std::string> names;
	const std::string func_key = SanitizeForCppIdentifier( func.name );
	for ( size_t i = 0; i < func.params.size(); ++i ) {
		const auto &param = func.params[i];
		const auto normalized_name = NormalizeScopedName( param.name );
		names[normalized_name] =
			"chsp_var_" + func_key + "_" + std::to_string( i ) + "_" + SanitizeForCppIdentifier( normalized_name );
	}
	return names;
}

std::string LookupCppIdentifier( const TranslateContext &ctx, const std::string &name )
{
	const auto it = ctx.identifier_cpp_names.find( name );
	if ( it != ctx.identifier_cpp_names.end() ) {
		return it->second;
	}
	return name;
}

std::string ArrayDimensionCppName( const ChspAstFunction &func, size_t param_index, int dimension_index )
{
	return "chsp_len_" + SanitizeForCppIdentifier( NormalizeScopedName( func.name ) ) + "_" +
		   std::to_string( param_index ) + "_" + std::to_string( dimension_index );
}

std::vector<std::string> ArrayDimensionExprsForName( const TranslateContext &ctx, const std::string &name )
{
	const auto it = ctx.array_dimension_exprs.find( name );
	if ( it == ctx.array_dimension_exprs.end() ) {
		return { "0", "0", "0", "0" };
	}
	return it->second;
}

static bool ExprUsesArrayMetadata( const ChspAstExpr &expr, const std::unordered_set<std::string> &array_names )
{
	if ( expr.kind == ChspAstExprKind::Call && !expr.children.empty() && expr.children[0] != nullptr &&
		 expr.children[0]->kind == ChspAstExprKind::Identifier ) {
		const auto name = NormalizeScopedName( expr.children[0]->text );
		if ( name == "length" || name == "length2" || name == "length3" || name == "length4" ) {
			return true;
		}
		if ( array_names.find( name ) != array_names.end() && expr.children.size() > 2 ) {
			return true;
		}
	}
	for ( const auto &child : expr.children ) {
		if ( child != nullptr && ExprUsesArrayMetadata( *child, array_names ) ) {
			return true;
		}
	}
	return false;
}

static bool StmtUsesArrayMetadata( const ChspAstStmt &stmt, const std::unordered_set<std::string> &array_names )
{
	if ( stmt.lhs != nullptr && ExprUsesArrayMetadata( *stmt.lhs, array_names ) ) {
		return true;
	}
	if ( stmt.rhs != nullptr && ExprUsesArrayMetadata( *stmt.rhs, array_names ) ) {
		return true;
	}
	for ( const auto &expr : stmt.exprs ) {
		if ( expr != nullptr && ExprUsesArrayMetadata( *expr, array_names ) ) {
			return true;
		}
	}
	for ( const auto &child : stmt.children ) {
		if ( child != nullptr && StmtUsesArrayMetadata( *child, array_names ) ) {
			return true;
		}
	}
	return false;
}

std::unordered_set<std::string> BuildFunctionArrayNames( const ChspAstFunction &func )
{
	std::unordered_set<std::string> array_names;
	for ( const auto &param : func.params ) {
		if ( param.is_array ) {
			array_names.insert( NormalizeScopedName( param.name ) );
		}
	}
	return array_names;
}

bool FunctionUsesArrayMetadataDirect( const ChspAstFunction &func )
{
	const auto array_names = BuildFunctionArrayNames( func );
	for ( const auto &stmt : func.body_stmts ) {
		if ( stmt != nullptr && StmtUsesArrayMetadata( *stmt, array_names ) ) {
			return true;
		}
	}
	return false;
}

static bool ExprForwardsArrayMetadata( const ChspAstExpr &expr, const std::unordered_set<std::string> &array_names,
									   const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
									   const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	if ( expr.kind == ChspAstExprKind::Call && !expr.children.empty() && expr.children[0] != nullptr &&
		 expr.children[0]->kind == ChspAstExprKind::Identifier ) {
		const auto callee_name = NormalizeScopedName( expr.children[0]->text );
		const auto needs_it = function_array_metadata_needs.find( callee_name );
		const auto def_it = function_defs.find( callee_name );
		if ( needs_it != function_array_metadata_needs.end() && needs_it->second && def_it != function_defs.end() ) {
			const auto &callee = *def_it->second;
			for ( size_t i = 1; i < expr.children.size() && ( i - 1 ) < callee.params.size(); ++i ) {
				if ( !callee.params[i - 1].is_array || expr.children[i] == nullptr ||
					 expr.children[i]->kind != ChspAstExprKind::Identifier ) {
					continue;
				}
				if ( array_names.find( NormalizeScopedName( expr.children[i]->text ) ) != array_names.end() ) {
					return true;
				}
			}
		}
	}
	for ( const auto &child : expr.children ) {
		if ( child != nullptr &&
			 ExprForwardsArrayMetadata( *child, array_names, function_defs, function_array_metadata_needs ) ) {
			return true;
		}
	}
	return false;
}

static bool StmtForwardsArrayMetadata( const ChspAstStmt &stmt, const std::unordered_set<std::string> &array_names,
									   const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
									   const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	if ( stmt.kind == ChspAstStmtKind::Command ) {
		const auto callee_name = NormalizeScopedName( stmt.text );
		const auto needs_it = function_array_metadata_needs.find( callee_name );
		const auto def_it = function_defs.find( callee_name );
		if ( needs_it != function_array_metadata_needs.end() && needs_it->second && def_it != function_defs.end() ) {
			const auto &callee = *def_it->second;
			for ( size_t i = 0; i < stmt.exprs.size() && i < callee.params.size(); ++i ) {
				if ( !callee.params[i].is_array || stmt.exprs[i] == nullptr ||
					 stmt.exprs[i]->kind != ChspAstExprKind::Identifier ) {
					continue;
				}
				if ( array_names.find( NormalizeScopedName( stmt.exprs[i]->text ) ) != array_names.end() ) {
					return true;
				}
			}
		}
	}
	if ( stmt.lhs != nullptr &&
		 ExprForwardsArrayMetadata( *stmt.lhs, array_names, function_defs, function_array_metadata_needs ) ) {
		return true;
	}
	if ( stmt.rhs != nullptr &&
		 ExprForwardsArrayMetadata( *stmt.rhs, array_names, function_defs, function_array_metadata_needs ) ) {
		return true;
	}
	for ( const auto &expr : stmt.exprs ) {
		if ( expr != nullptr &&
			 ExprForwardsArrayMetadata( *expr, array_names, function_defs, function_array_metadata_needs ) ) {
			return true;
		}
	}
	for ( const auto &child : stmt.children ) {
		if ( child != nullptr &&
			 StmtForwardsArrayMetadata( *child, array_names, function_defs, function_array_metadata_needs ) ) {
			return true;
		}
	}
	return false;
}

std::unordered_map<std::string, bool> BuildFunctionArrayMetadataNeeds(
	const ChspAstProgram &program, const std::unordered_map<std::string, const ChspAstFunction *> &function_defs )
{
	std::unordered_map<std::string, bool> needs;
	for ( const auto &module : program.modules ) {
		for ( const auto &func : module.functions ) {
			needs[NormalizeScopedName( func.name )] = FunctionUsesArrayMetadataDirect( func );
		}
	}
	bool changed = true;
	while ( changed ) {
		changed = false;
		for ( const auto &module : program.modules ) {
			for ( const auto &func : module.functions ) {
				const auto name = NormalizeScopedName( func.name );
				if ( needs[name] ) {
					continue;
				}
				const auto array_names = BuildFunctionArrayNames( func );
				for ( const auto &stmt : func.body_stmts ) {
					if ( stmt != nullptr && StmtForwardsArrayMetadata( *stmt, array_names, function_defs, needs ) ) {
						needs[name] = true;
						changed = true;
						break;
					}
				}
			}
		}
	}
	return needs;
}

bool FunctionNeedsArrayMetadata( const std::unordered_map<std::string, bool> &function_array_metadata_needs,
								 const ChspAstFunction &func )
{
	const auto it = function_array_metadata_needs.find( NormalizeScopedName( func.name ) );
	return it != function_array_metadata_needs.end() && it->second;
}

std::string ToHspParamType( const ChspAstParam &param )
{
	if ( param.is_array ) {
		return "var";
	}
	if ( param.base_type == "int" ) {
		return "int";
	}
	if ( param.base_type == "int64" ) {
		return "int64";
	}
	if ( param.base_type == "double" ) {
		return "double";
	}
	return "var";
}

std::string ToPublicHspParamType( const ChspAstParam &param )
{
	if ( param.is_array ) {
		return "array";
	}
	if ( param.base_type == "int" ) {
		return "int";
	}
	if ( param.base_type == "int64" ) {
		return "int64";
	}
	if ( param.base_type == "double" ) {
		return "double";
	}
	return "var";
}

std::string PluginCommandName( const ChspAstFunction &func )
{
	return NormalizeScopedName( func.name );
}

std::string ToCppType( const ChspAstParam &param )
{
	if ( param.is_array ) {
		return param.base_type + " *";
	}
	return param.base_type;
}

std::string ToNativeType( const ChspAstParam &param, ChspNativeTarget )
{
	return ToCppType( param );
}

std::string DefaultReturnExpr( const std::string &type )
{
	if ( type == "double" ) {
		return "0.0";
	}
	if ( type == "int" || type == "int64" ) {
		return "0";
	}
	return "";
}

std::string BuiltinTarget( const std::string &name, size_t arg_count, ChspNativeTarget target )
{
	if ( target == ChspNativeTarget::C || target == ChspNativeTarget::Plugin ) {
		if ( name == "abs" ) {
			return "chsp_hsp_abs";
		}
		if ( name == "absf" || name == "chsp_fabs" ) {
			return "chsp_hsp_absf";
		}
		if ( name == "atan" ) {
			return "chsp_hsp_atan";
		}
		if ( name == "cos" ) {
			return "chsp_hsp_cos";
		}
		if ( name == "double" ) {
			return "chsp_hsp_double";
		}
		if ( name == "expf" ) {
			return "chsp_hsp_expf";
		}
		if ( name == "int" ) {
			return "chsp_hsp_int";
		}
		if ( name == "int64" ) {
			return "chsp_hsp_int64";
		}
		if ( name == "limit" ) {
			return "chsp_hsp_limit";
		}
		if ( name == "limitf" ) {
			return "chsp_hsp_limitf";
		}
		if ( name == "logf" ) {
			return "chsp_hsp_logf";
		}
		if ( name == "powf" ) {
			return "chsp_hsp_powf";
		}
		if ( name == "randomize" ) {
			return arg_count == 0 ? "chsp_randomize" : "chsp_randomize_seed";
		}
		if ( name == "rnd" ) {
			return "chsp_rnd";
		}
		if ( name == "sin" ) {
			return "chsp_hsp_sin";
		}
		if ( name == "sqrt" || name == "chsp_sqrt" ) {
			return "chsp_hsp_sqrt";
		}
		if ( name == "tan" ) {
			return "chsp_hsp_tan";
		}
		return "";
	}
	static const std::map<std::string, std::string> builtins = {
		{ "abs", "chsp::hsp_abs" },		   { "absf", "chsp::hsp_absf" },	   { "atan", "chsp::hsp_atan" },
		{ "chsp_fabs", "chsp::hsp_absf" }, { "chsp_sqrt", "chsp::hsp_sqrt" },  { "cos", "chsp::hsp_cos" },
		{ "double", "chsp::hsp_double" },  { "expf", "chsp::hsp_expf" },	   { "int", "chsp::hsp_int" },
		{ "limit", "chsp::hsp_limit" },	   { "limitf", "chsp::hsp_limitf" },   { "logf", "chsp::hsp_logf" },
		{ "powf", "chsp::hsp_powf" },	   { "randomize", "chsp::randomize" }, { "rnd", "chsp::rnd" },
		{ "sin", "chsp::hsp_sin" },		   { "sqrt", "chsp::hsp_sqrt" },	   { "tan", "chsp::hsp_tan" },
	};
	const auto it = builtins.find( name );
	if ( it == builtins.end() ) {
		return "";
	}
	return it->second;
}

std::string OperatorText( int op )
{
	switch ( op ) {
	case '\\':
		return "%";
	case '!':
		return "!=";
	case '=':
		return "==";
	case 0x61:
		return "<=";
	case 0x62:
		return ">=";
	case 0x63:
		return "<<";
	case 0x64:
		return ">>";
	default:
		return std::string( 1, static_cast<char>( op ) );
	}
}

int ExprPrecedence( const ChspAstExpr &expr )
{
	switch ( expr.kind ) {
	case ChspAstExprKind::Binary:
		switch ( expr.token_kind ) {
		case '&':
		case '|':
		case '^':
			return 10;
		case '<':
		case '>':
		case '=':
		case '!':
		case 0x61:
		case 0x62:
			return 20;
		case 0x63:
		case 0x64:
			return 30;
		case '+':
		case '-':
			return 40;
		case '*':
		case '/':
		case '\\':
			return 50;
		default:
			return 5;
		}
	case ChspAstExprKind::Unary:
		return 60;
	case ChspAstExprKind::Call:
	case ChspAstExprKind::Identifier:
	case ChspAstExprKind::Label:
	case ChspAstExprKind::IntLiteral:
	case ChspAstExprKind::DoubleLiteral:
	case ChspAstExprKind::StringLiteral:
	case ChspAstExprKind::Group:
		return 70;
	default:
		return 0;
	}
}

TranslateContext BuildTranslateContext( const ChspAstFunction &func, const ChspAstModule &module,
										const std::unordered_map<std::string, std::string> &function_cpp_names,
										const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
										const std::unordered_map<std::string, bool> &function_array_metadata_needs,
										ChspNativeTarget target, const ChspBuiltinMap &builtin_map )
{
	TranslateContext ctx;
	ctx.target = target;
	ctx.builtin_map = &builtin_map;
	ctx.function_cpp_names = function_cpp_names;
	ctx.function_defs = function_defs;
	ctx.function_array_metadata_needs = function_array_metadata_needs;
	ctx.identifier_cpp_names = BuildIdentifierCppNames( func );
	for ( const auto &name : module.declared_native_functions ) {
		ctx.declared_native_functions[chsputil::NormalizeIdentifier( name )] = name;
	}
	for ( const auto &param : func.params ) {
		if ( !param.is_array ) {
			continue;
		}
		const auto normalized_name = NormalizeScopedName( param.name );
		ctx.array_names.insert( normalized_name );
		if ( param.is_local ) {
			ctx.array_dimension_exprs[normalized_name] = FixedDimsToDimensionExprs( param.array_dims );
		} else {
			const size_t param_index = &param - func.params.data();
			ctx.array_dimension_exprs[normalized_name] = {
				ArrayDimensionCppName( func, param_index, 1 ),
				ArrayDimensionCppName( func, param_index, 2 ),
				ArrayDimensionCppName( func, param_index, 3 ),
				ArrayDimensionCppName( func, param_index, 4 ),
			};
		}
	}
	return ctx;
}

} // namespace chsp
