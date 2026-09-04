//
//      cHSP v3 AST emitter surface
//
#include "chsp_frontend_v3_emitter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../membuf.h"
#include "logger.h"

namespace chspv3
{
namespace
{

struct TranslateContext
{
	ChspNativeTarget target = ChspNativeTarget::Plugin;
	const ChspBuiltinMap *builtin_map = nullptr;
	std::unordered_set<std::string> array_names;
	std::unordered_map<std::string, std::string> declared_native_functions; // normalized name -> original case name
	std::unordered_map<std::string, std::string> identifier_cpp_names;
	std::unordered_map<std::string, std::string> function_cpp_names;
	std::unordered_map<std::string, const ChspV3AstFunction *> function_defs;
	std::unordered_map<std::string, bool> function_array_metadata_needs;
	std::unordered_map<std::string, std::vector<std::string>> array_dimension_exprs;
	std::vector<std::string> loop_stack;
	int temp_var_count = 0;
};

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

bool IsDefCFunc( const ChspV3AstFunction &func )
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

std::string MakeFunctionCppName( const ChspV3AstFunction &func )
{
	return "chsp_func_" + SanitizeForCppIdentifier( NormalizeScopedName( func.name ) );
}

std::unordered_map<std::string, std::string> BuildFunctionCppNames( const ChspV3AstProgram &program )
{
	std::unordered_map<std::string, std::string> names;
	for ( const auto &module : program.modules ) {
		for ( const auto &function : module.functions ) {
			names[NormalizeScopedName( function.name )] = MakeFunctionCppName( function );
		}
	}
	return names;
}

std::unordered_map<std::string, const ChspV3AstFunction *> BuildFunctionDefs( const ChspV3AstProgram &program )
{
	std::unordered_map<std::string, const ChspV3AstFunction *> defs;
	for ( const auto &module : program.modules ) {
		for ( const auto &function : module.functions ) {
			defs[NormalizeScopedName( function.name )] = &function;
		}
	}
	return defs;
}

std::unordered_map<std::string, std::string> BuildIdentifierCppNames( const ChspV3AstFunction &func )
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

std::string ArrayDimensionCppName( const ChspV3AstFunction &func, size_t param_index, int dimension_index )
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

bool AppendArrayArgumentMetadata( const ChspV3AstExpr &arg_expr, const TranslateContext &ctx, bool &ok,
								  std::string &out )
{
	if ( arg_expr.kind != ChspV3AstExprKind::Identifier ) {
		ok = false;
		return false;
	}
	const auto name = NormalizeScopedName( arg_expr.text );
	if ( ctx.array_names.find( name ) == ctx.array_names.end() ) {
		ok = false;
		return false;
	}
	for ( const auto &dim_expr : ArrayDimensionExprsForName( ctx, name ) ) {
		out += ", ";
		out += dim_expr;
	}
	return true;
}

std::string TranslateExpr( const ChspV3AstExpr &expr, const TranslateContext &ctx, bool &ok, int parent_precedence,
						   bool paren_on_equal );

bool ExprUsesArrayMetadata( const ChspV3AstExpr &expr, const std::unordered_set<std::string> &array_names )
{
	if ( expr.kind == ChspV3AstExprKind::Call && !expr.children.empty() && expr.children[0] != nullptr &&
		 expr.children[0]->kind == ChspV3AstExprKind::Identifier ) {
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

bool StmtUsesArrayMetadata( const ChspV3AstStmt &stmt, const std::unordered_set<std::string> &array_names )
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

std::unordered_set<std::string> BuildFunctionArrayNames( const ChspV3AstFunction &func )
{
	std::unordered_set<std::string> array_names;
	for ( const auto &param : func.params ) {
		if ( param.is_array ) {
			array_names.insert( NormalizeScopedName( param.name ) );
		}
	}
	return array_names;
}

bool FunctionUsesArrayMetadataDirect( const ChspV3AstFunction &func )
{
	const auto array_names = BuildFunctionArrayNames( func );
	for ( const auto &stmt : func.body_stmts ) {
		if ( stmt != nullptr && StmtUsesArrayMetadata( *stmt, array_names ) ) {
			return true;
		}
	}
	return false;
}

bool ExprForwardsArrayMetadata( const ChspV3AstExpr &expr, const std::unordered_set<std::string> &array_names,
								const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	if ( expr.kind == ChspV3AstExprKind::Call && !expr.children.empty() && expr.children[0] != nullptr &&
		 expr.children[0]->kind == ChspV3AstExprKind::Identifier ) {
		const auto callee_name = NormalizeScopedName( expr.children[0]->text );
		const auto needs_it = function_array_metadata_needs.find( callee_name );
		const auto def_it = function_defs.find( callee_name );
		if ( needs_it != function_array_metadata_needs.end() && needs_it->second && def_it != function_defs.end() ) {
			const auto &callee = *def_it->second;
			for ( size_t i = 1; i < expr.children.size() && ( i - 1 ) < callee.params.size(); ++i ) {
				if ( !callee.params[i - 1].is_array || expr.children[i] == nullptr ||
					 expr.children[i]->kind != ChspV3AstExprKind::Identifier ) {
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

bool StmtForwardsArrayMetadata( const ChspV3AstStmt &stmt, const std::unordered_set<std::string> &array_names,
								const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	if ( stmt.kind == ChspV3AstStmtKind::Command ) {
		const auto callee_name = NormalizeScopedName( stmt.text );
		const auto needs_it = function_array_metadata_needs.find( callee_name );
		const auto def_it = function_defs.find( callee_name );
		if ( needs_it != function_array_metadata_needs.end() && needs_it->second && def_it != function_defs.end() ) {
			const auto &callee = *def_it->second;
			for ( size_t i = 0; i < stmt.exprs.size() && i < callee.params.size(); ++i ) {
				if ( !callee.params[i].is_array || stmt.exprs[i] == nullptr ||
					 stmt.exprs[i]->kind != ChspV3AstExprKind::Identifier ) {
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
	const ChspV3AstProgram &program, const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs )
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
								 const ChspV3AstFunction &func )
{
	const auto it = function_array_metadata_needs.find( NormalizeScopedName( func.name ) );
	return it != function_array_metadata_needs.end() && it->second;
}

std::string TranslateArrayAccess( const ChspV3AstExpr &expr, const TranslateContext &ctx, bool &ok )
{
	if ( expr.children.empty() || expr.children[0] == nullptr ||
		 expr.children[0]->kind != ChspV3AstExprKind::Identifier ) {
		ok = false;
		return "";
	}
	const std::string name = NormalizeScopedName( expr.children[0]->text );
	const std::string cpp_name = LookupCppIdentifier( ctx, name );
	const auto dimensions = ArrayDimensionExprsForName( ctx, name );
	if ( expr.children.size() == 2 ) {
		return cpp_name + "[" + TranslateExpr( *expr.children[1], ctx, ok, 0, false ) + "]";
	}

	std::string offset = TranslateExpr( *expr.children[1], ctx, ok, 0, false );
	for ( size_t i = 2; i < expr.children.size(); ++i ) {
		std::string multiplier = "1";
		for ( size_t dim = 0; dim + 1 < i && dim < dimensions.size(); ++dim ) {
			if ( dimensions[dim] == "0" ) {
				continue;
			}
			multiplier = "(" + multiplier + ") * (" + dimensions[dim] + ")";
		}
		offset =
			"(" + offset + ") + (" + TranslateExpr( *expr.children[i], ctx, ok, 0, false ) + ") * (" + multiplier + ")";
	}
	return cpp_name + "[" + offset + "]";
}

std::string ToHspParamType( const ChspV3AstParam &param )
{
	if ( param.is_array ) {
		return "var";
	}
	if ( param.base_type == "int" ) {
		return "int";
	}
	if ( param.base_type == "double" ) {
		return "double";
	}
	return "var";
}

std::string PluginCommandName( const ChspV3AstFunction &func )
{
	return NormalizeScopedName( func.name );
}

std::string ToCppType( const ChspV3AstParam &param )
{
	if ( param.is_array ) {
		return param.base_type + " *";
	}
	return param.base_type;
}

std::string ToNativeType( const ChspV3AstParam &param, ChspNativeTarget )
{
	return ToCppType( param );
}

std::string DefaultReturnExpr( const std::string &type )
{
	if ( type == "double" ) {
		return "0.0";
	}
	if ( type == "int" ) {
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

int ExprPrecedence( const ChspV3AstExpr &expr )
{
	switch ( expr.kind ) {
	case ChspV3AstExprKind::Binary:
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
	case ChspV3AstExprKind::Unary:
		return 60;
	case ChspV3AstExprKind::Call:
	case ChspV3AstExprKind::Identifier:
	case ChspV3AstExprKind::Label:
	case ChspV3AstExprKind::IntLiteral:
	case ChspV3AstExprKind::DoubleLiteral:
	case ChspV3AstExprKind::StringLiteral:
	case ChspV3AstExprKind::Group:
		return 70;
	default:
		return 0;
	}
}

TranslateContext BuildTranslateContext( const ChspV3AstFunction &func, const ChspV3AstModule &module,
										const std::unordered_map<std::string, std::string> &function_cpp_names,
										const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
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

std::string TranslateExpr( const ChspV3AstExpr &expr, const TranslateContext &ctx, bool &ok, int parent_precedence = 0,
						   bool paren_on_equal = false )
{
	const auto wrap_if_needed = [&]( std::string text, int self_precedence ) {
		if ( self_precedence < parent_precedence || ( paren_on_equal && self_precedence == parent_precedence ) ) {
			return std::string( "(" ) + text + ")";
		}
		return text;
	};

	switch ( expr.kind ) {
	case ChspV3AstExprKind::IntLiteral:
		return expr.text;
	case ChspV3AstExprKind::DoubleLiteral:
		return expr.text;
	case ChspV3AstExprKind::StringLiteral:
		return "\"" + expr.text + "\"";
	case ChspV3AstExprKind::Label:
		return expr.text;
	case ChspV3AstExprKind::Identifier:
		if ( NormalizeScopedName( expr.text ) == "cnt" && !ctx.loop_stack.empty() ) {
			return ctx.loop_stack.back();
		}
		if ( ctx.array_names.find( NormalizeScopedName( expr.text ) ) == ctx.array_names.end() &&
			 ctx.identifier_cpp_names.find( NormalizeScopedName( expr.text ) ) == ctx.identifier_cpp_names.end() ) {
			const auto function_it = ctx.function_cpp_names.find( NormalizeScopedName( expr.text ) );
			if ( function_it != ctx.function_cpp_names.end() ) {
				return function_it->second + "()";
			}
		}
		return LookupCppIdentifier( ctx, NormalizeScopedName( expr.text ) );
	case ChspV3AstExprKind::Unary:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( "-" + TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspV3AstExprKind::Binary:
		if ( expr.children.size() != 2 || expr.children[0] == nullptr || expr.children[1] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), false ) + " " +
								   OperatorText( expr.token_kind ) + " " +
								   TranslateExpr( *expr.children[1], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspV3AstExprKind::Group:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return "(" + TranslateExpr( *expr.children[0], ctx, ok, 0, false ) + ")";
	case ChspV3AstExprKind::Call:
		break;
	default:
		ok = false;
		return "";
	}

	if ( expr.children.empty() || expr.children[0] == nullptr ||
		 expr.children[0]->kind != ChspV3AstExprKind::Identifier ) {
		ok = false;
		return "";
	}

	const std::string name = NormalizeScopedName( expr.children[0]->text );
	const auto function_it = ctx.function_defs.find( name );
	const ChspV3AstFunction *callee = function_it != ctx.function_defs.end() ? function_it->second : nullptr;
	if ( ctx.array_names.find( name ) != ctx.array_names.end() ) {
		return TranslateArrayAccess( expr, ctx, ok );
	}

	if ( name == "length" || name == "length2" || name == "length3" || name == "length4" ) {
		if ( expr.children.size() != 2 || expr.children[1] == nullptr ||
			 expr.children[1]->kind != ChspV3AstExprKind::Identifier ) {
			ok = false;
			return "";
		}
		const auto array_name = NormalizeScopedName( expr.children[1]->text );
		if ( ctx.array_names.find( array_name ) == ctx.array_names.end() ) {
			ok = false;
			return "";
		}
		const auto dimensions = ArrayDimensionExprsForName( ctx, array_name );
		if ( name == "length" )
			return dimensions[0];
		if ( name == "length2" )
			return dimensions[1];
		if ( name == "length3" )
			return dimensions[2];
		return dimensions[3];
	}

	std::string target = LookupChspBuiltin( *ctx.builtin_map, name, expr.children.size() - 1, ctx.target );
	if ( target.empty() ) {
		const auto function_it = ctx.function_cpp_names.find( name );
		if ( function_it != ctx.function_cpp_names.end() ) {
			target = function_it->second;
		} else {
			const auto native_it = ctx.declared_native_functions.find( name );
			if ( native_it != ctx.declared_native_functions.end() ) {
				target = native_it->second;
			} else {
				ok = false;
				return "";
			}
		}
	}
	std::string out = target + "(";
	for ( size_t i = 1; i < expr.children.size(); ++i ) {
		if ( expr.children[i] == nullptr ) {
			ok = false;
			return "";
		}
		if ( i != 1 ) {
			out += ", ";
		}
		out += TranslateExpr( *expr.children[i], ctx, ok, 0, false );
		if ( callee != nullptr && FunctionNeedsArrayMetadata( ctx.function_array_metadata_needs, *callee ) &&
			 ( i - 1 ) < callee->params.size() && callee->params[i - 1].is_array ) {
			if ( !AppendArrayArgumentMetadata( *expr.children[i], ctx, ok, out ) ) {
				return "";
			}
		}
	}
	out += ")";
	return out;
}

std::string MakeIndent( int level )
{
	return std::string( level * 4, ' ' );
}

bool IsBlockOpeningStmt( const ChspV3AstStmt &stmt )
{
	if ( stmt.kind == ChspV3AstStmtKind::Repeat ) {
		return true;
	}
	if ( stmt.kind != ChspV3AstStmtKind::If && stmt.kind != ChspV3AstStmtKind::Else ) {
		return false;
	}
	return stmt.children.size() > 1;
}

bool HasTrailingBlockElseChild( const ChspV3AstStmt &stmt, size_t &else_index )
{
	if ( stmt.kind != ChspV3AstStmtKind::If ) {
		return false;
	}
	for ( size_t i = 0; i < stmt.children.size(); ++i ) {
		if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspV3AstStmtKind::Else ) {
			else_index = i;
			return true;
		}
	}
	return false;
}

std::string RenderStatementInline( const ChspV3AstStmt &stmt, TranslateContext &ctx, bool &ok );

const char *StatementKindName( ChspV3AstStmtKind kind )
{
	switch ( kind ) {
	case ChspV3AstStmtKind::Unknown:
		return "unknown";
	case ChspV3AstStmtKind::Return:
		return "return";
	case ChspV3AstStmtKind::Repeat:
		return "repeat";
	case ChspV3AstStmtKind::Loop:
		return "loop";
	case ChspV3AstStmtKind::If:
		return "if";
	case ChspV3AstStmtKind::Else:
		return "else";
	case ChspV3AstStmtKind::Assignment:
		return "assignment";
	case ChspV3AstStmtKind::Command:
		return "command";
	case ChspV3AstStmtKind::BlockMarker:
		return "block";
	default:
		return "statement";
	}
}

bool ReportUnsupportedStmt( CLogger &logger, const ChspV3AstFunction &func, const ChspV3AstStmt &stmt,
							const char *reason )
{
	logger.Mesf( "#Error:cHSP frontend v3 emitter does not support %s in function '%s' at line %d%s%s%s",
				 StatementKindName( stmt.kind ), NormalizeScopedName( func.name ).c_str(), stmt.line,
				 reason != nullptr ? " (" : "", reason != nullptr ? reason : "", reason != nullptr ? ")" : "" );
	return false;
}

std::string RenderCommandCall( const ChspV3AstStmt &stmt, TranslateContext &ctx, bool &ok )
{
	const std::string name = NormalizeScopedName( stmt.text );
	const auto function_it = ctx.function_defs.find( name );
	const ChspV3AstFunction *callee = function_it != ctx.function_defs.end() ? function_it->second : nullptr;
	std::string target = LookupChspBuiltin( *ctx.builtin_map, name, stmt.exprs.size(), ctx.target );
	if ( target.empty() ) {
		const auto cpp_it = ctx.function_cpp_names.find( name );
		if ( cpp_it != ctx.function_cpp_names.end() ) {
			target = cpp_it->second;
		} else {
			const auto native_it = ctx.declared_native_functions.find( name );
			if ( native_it != ctx.declared_native_functions.end() ) {
				target = native_it->second;
			} else {
				ok = false;
				return "";
			}
		}
	}
	std::string out = target + "(";
	for ( size_t i = 0; i < stmt.exprs.size(); ++i ) {
		if ( stmt.exprs[i] == nullptr ) {
			ok = false;
			return "";
		}
		if ( i != 0 ) {
			out += ", ";
		}
		out += TranslateExpr( *stmt.exprs[i], ctx, ok );
		if ( callee != nullptr && FunctionNeedsArrayMetadata( ctx.function_array_metadata_needs, *callee ) &&
			 i < callee->params.size() && callee->params[i].is_array ) {
			if ( !AppendArrayArgumentMetadata( *stmt.exprs[i], ctx, ok, out ) ) {
				return "";
			}
		}
	}
	out += ")";
	return out;
}

std::string RenderStatementInline( const ChspV3AstStmt &stmt, TranslateContext &ctx, bool &ok )
{
	switch ( stmt.kind ) {
	case ChspV3AstStmtKind::Return:
		if ( stmt.rhs == nullptr ) {
			return "return;";
		}
		return "return " + TranslateExpr( *stmt.rhs, ctx, ok ) + ";";
	case ChspV3AstStmtKind::Assignment: {
		if ( stmt.lhs == nullptr ) {
			ok = false;
			return "";
		}
		const auto lhs = TranslateExpr( *stmt.lhs, ctx, ok );
		if ( stmt.rhs == nullptr ) {
			switch ( stmt.token_kind ) {
			case '+':
				return lhs + " += 1;";
			case '-':
				return lhs + " -= 1;";
			default:
				ok = false;
				return "";
			}
		}
		const auto rhs = TranslateExpr( *stmt.rhs, ctx, ok );
		std::string op = "=";
		switch ( stmt.token_kind ) {
		case '+':
		case '-':
		case '*':
		case '/':
			op = std::string( 1, static_cast<char>( stmt.token_kind ) ) + "=";
			break;
		case '=':
			op = "=";
			break;
		default:
			ok = false;
			return "";
		}
		return lhs + " " + op + " " + rhs + ";";
	}
	case ChspV3AstStmtKind::Command:
		if ( NormalizeScopedName( stmt.text ) == "break" ) {
			return "break;";
		}
		if ( NormalizeScopedName( stmt.text ) == "continue" ) {
			return "continue;";
		}
		return RenderCommandCall( stmt, ctx, ok ) + ";";
	case ChspV3AstStmtKind::If:
		if ( stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
			ok = false;
			return "";
		}
		if ( stmt.children.empty() || stmt.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		if ( stmt.children.size() == 1 ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") " +
				   RenderStatementInline( *stmt.children[0], ctx, ok );
		}
		if ( stmt.children.size() == 2 && stmt.children[1] != nullptr &&
			 stmt.children[1]->kind == ChspV3AstStmtKind::Else ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") " +
				   RenderStatementInline( *stmt.children[0], ctx, ok ) + " " +
				   RenderStatementInline( *stmt.children[1], ctx, ok );
		}
		ok = false;
		return "";
	case ChspV3AstStmtKind::Else:
		if ( stmt.children.size() != 1 || stmt.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return "else " + RenderStatementInline( *stmt.children[0], ctx, ok );
	default:
		ok = false;
		return "";
	}
}

bool WriteFunctionStmtToCpp( CMemBuf &buf, const ChspV3AstStmt &stmt, TranslateContext &ctx,
							 const ChspV3AstFunction &func, CLogger &logger, int &indent_level,
							 bool &emitted_explicit_return )
{
	switch ( stmt.kind ) {
	case ChspV3AstStmtKind::Unknown:
		return true;
	case ChspV3AstStmtKind::BlockMarker:
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}" );
		buf.PutCR();
		return true;
	case ChspV3AstStmtKind::Loop:
		if ( !ctx.loop_stack.empty() ) {
			ctx.loop_stack.pop_back();
		}
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}\n" );
		return true;
	case ChspV3AstStmtKind::Repeat: {
		bool ok = true;
		std::string expr = "0";
		if ( stmt.rhs != nullptr ) {
			expr = TranslateExpr( *stmt.rhs, ctx, ok );
		}
		if ( !ok ) {
			return ReportUnsupportedStmt( logger, func, stmt, "repeat count expression could not be translated" );
		}
		const std::string loop_var = "_cnt" + std::to_string( static_cast<int>( ctx.loop_stack.size() ) );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "for (int " );
		buf.PutStr( loop_var.c_str() );
		buf.PutStr( " = 0; " );
		buf.PutStr( loop_var.c_str() );
		buf.PutStr( " < " );
		buf.PutStr( expr.c_str() );
		buf.PutStr( "; ++" );
		buf.PutStr( loop_var.c_str() );
		buf.PutStr( ") {\n" );
		ctx.loop_stack.push_back( loop_var );
		++indent_level;
		for ( const auto &child : stmt.children ) {
			if ( child != nullptr ) {
				if ( !WriteFunctionStmtToCpp( buf, *child, ctx, func, logger, indent_level,
											  emitted_explicit_return ) ) {
					return false;
				}
			}
		}
		return true;
	}
	case ChspV3AstStmtKind::If: {
		bool ok = true;
		if ( stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
			ok = false;
		}
		size_t else_index = stmt.children.size();
		for ( size_t i = 0; i < stmt.children.size(); ++i ) {
			if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspV3AstStmtKind::Else ) {
				else_index = i;
				break;
			}
		}
		if ( else_index != stmt.children.size() || stmt.children.size() > 1 ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( "if (" );
			const auto cond = TranslateExpr( *stmt.exprs[0], ctx, ok );
			buf.PutStr( cond.c_str() );
			buf.PutStr( ") {" );
			buf.PutCR();
			if ( ok ) {
				++indent_level;
				for ( size_t i = 0; i < stmt.children.size(); ++i ) {
					if ( i == else_index ) {
						break;
					}
					if ( stmt.children[i] != nullptr ) {
						if ( !WriteFunctionStmtToCpp( buf, *stmt.children[i], ctx, func, logger, indent_level,
													  emitted_explicit_return ) ) {
							return false;
						}
					}
				}
				indent_level = std::max( 1, indent_level - 1 );
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( "}" );
				buf.PutCR();
				if ( else_index != stmt.children.size() && stmt.children[else_index] != nullptr ) {
					const auto &else_stmt = *stmt.children[else_index];
					bool else_ok = true;
					const auto rendered = RenderStatementInline( else_stmt, ctx, else_ok );
					if ( else_ok && !rendered.empty() ) {
						buf.PutStr( MakeIndent( indent_level ).c_str() );
						buf.PutStr( rendered.c_str() );
						buf.PutCR();
					} else {
						buf.PutStr( MakeIndent( indent_level ).c_str() );
						buf.PutStr( "else {" );
						buf.PutCR();
						++indent_level;
						for ( const auto &child : else_stmt.children ) {
							if ( child != nullptr ) {
								if ( !WriteFunctionStmtToCpp( buf, *child, ctx, func, logger, indent_level,
															  emitted_explicit_return ) ) {
									return false;
								}
							}
						}
						indent_level = std::max( 1, indent_level - 1 );
						buf.PutStr( MakeIndent( indent_level ).c_str() );
						buf.PutStr( "}" );
						buf.PutCR();
					}
				}
				return true;
			}
		} else {
			const auto rendered = RenderStatementInline( stmt, ctx, ok );
			if ( ok ) {
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( rendered.c_str() );
				buf.PutCR();
				if ( chsputil::StartsWith( chsputil::Trim( rendered ), "return" ) ) {
					emitted_explicit_return = true;
				}
				return true;
			}
		}
		return ReportUnsupportedStmt( logger, func, stmt, "conditional structure could not be translated" );
	}
	case ChspV3AstStmtKind::Else: {
		bool ok = true;
		if ( stmt.children.size() > 1 ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( "else {" );
			buf.PutCR();
			++indent_level;
			for ( const auto &child : stmt.children ) {
				if ( child != nullptr ) {
					if ( !WriteFunctionStmtToCpp( buf, *child, ctx, func, logger, indent_level,
												  emitted_explicit_return ) ) {
						return false;
					}
				}
			}
			indent_level = std::max( 1, indent_level - 1 );
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( "}" );
			buf.PutCR();
			return true;
		}
		const auto rendered = RenderStatementInline( stmt, ctx, ok );
		if ( ok ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( rendered.c_str() );
			buf.PutCR();
			if ( chsputil::StartsWith( chsputil::Trim( rendered ), "else return" ) ) {
				emitted_explicit_return = true;
			}
			return true;
		}
		return ReportUnsupportedStmt( logger, func, stmt, "else branch could not be translated" );
	}
	case ChspV3AstStmtKind::Assignment: {
		// Consecutive assignment: a = v1, v2, v3  or  a(n) = v1, v2, v3
		// exprs holds the extra values (v2, v3, ...) beyond the first (rhs = v1).
		if ( stmt.token_kind == '=' && !stmt.exprs.empty() && stmt.lhs != nullptr && stmt.rhs != nullptr ) {
			bool ok = true;
			std::string base_ptr;
			std::string start_offset; // empty → index 0

			if ( stmt.lhs->kind == ChspV3AstExprKind::Identifier &&
				 ctx.array_names.count( NormalizeScopedName( stmt.lhs->text ) ) ) {
				// a = v1, v2, v3
				base_ptr = LookupCppIdentifier( ctx, NormalizeScopedName( stmt.lhs->text ) );
			} else if ( stmt.lhs->kind == ChspV3AstExprKind::Call && stmt.lhs->children.size() == 2 &&
						stmt.lhs->children[0] != nullptr &&
						stmt.lhs->children[0]->kind == ChspV3AstExprKind::Identifier &&
						ctx.array_names.count( NormalizeScopedName( stmt.lhs->children[0]->text ) ) &&
						stmt.lhs->children[1] != nullptr ) {
				// a(n) = v1, v2, v3
				base_ptr = LookupCppIdentifier( ctx, NormalizeScopedName( stmt.lhs->children[0]->text ) );
				start_offset = TranslateExpr( *stmt.lhs->children[1], ctx, ok, 0, false );
			} else {
				ok = false;
			}

			if ( !ok ) {
				return ReportUnsupportedStmt( logger, func, stmt,
											  "consecutive assignment LHS could not be translated" );
			}

			// If start_offset is a non-trivial expression (e.g. a function call with side effects),
			// evaluate it once into a temporary variable to avoid multiple evaluations.
			std::string idx_base;
			if ( start_offset.empty() ) {
				idx_base = "";
			} else {
				const std::string tmp = "_chsp_idx_" + std::to_string( ctx.temp_var_count++ );
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( ( "int " + tmp + " = " + start_offset + ";" ).c_str() );
				buf.PutCR();
				idx_base = tmp;
			}

			auto make_idx = [&]( size_t i ) -> std::string {
				if ( idx_base.empty() ) {
					return std::to_string( i );
				}
				if ( i == 0 ) {
					return idx_base;
				}
				return idx_base + " + " + std::to_string( i );
			};

			const auto rhs0 = TranslateExpr( *stmt.rhs, ctx, ok );
			if ( !ok ) {
				return ReportUnsupportedStmt( logger, func, stmt,
											  "consecutive assignment value could not be translated" );
			}
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( ( base_ptr + "[" + make_idx( 0 ) + "] = " + rhs0 + ";" ).c_str() );
			buf.PutCR();

			for ( size_t i = 0; i < stmt.exprs.size(); ++i ) {
				if ( stmt.exprs[i] == nullptr ) {
					return ReportUnsupportedStmt( logger, func, stmt,
												  "consecutive assignment value is null" );
				}
				const auto val = TranslateExpr( *stmt.exprs[i], ctx, ok );
				if ( !ok ) {
					return ReportUnsupportedStmt( logger, func, stmt,
												  "consecutive assignment value could not be translated" );
				}
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( ( base_ptr + "[" + make_idx( i + 1 ) + "] = " + val + ";" ).c_str() );
				buf.PutCR();
			}
			return true;
		}
		break;
	}
	default:
		break;
	}

	bool ok = true;
	const auto rendered = RenderStatementInline( stmt, ctx, ok );
	buf.PutStr( MakeIndent( indent_level ).c_str() );
	if ( ok && !rendered.empty() ) {
		buf.PutStr( rendered.c_str() );
		buf.PutCR();
		if ( chsputil::StartsWith( chsputil::Trim( rendered ), "return" ) ) {
			emitted_explicit_return = true;
		}
		return true;
	}
	return ReportUnsupportedStmt( logger, func, stmt, "statement could not be translated" );
}

void WriteFunctionDeclToHsp( CMemBuf &buf, const ChspV3AstFunction &func, const std::string &cpp_name )
{
	buf.PutStr( IsDefCFunc( func ) ? "#cfunc " : "#func " );
	buf.PutStr( NormalizeScopedName( func.name ).c_str() );
	buf.PutStr( " \"" );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "\"" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToHspParamType( param ).c_str() );
	}
	buf.PutCR();
}

std::string HspInternalFunctionDeclName( const ChspV3AstFunction &func )
{
	return "chsp_native_wrap_" + SanitizeForCppIdentifier( NormalizeScopedName( func.name ) );
}

std::string ToPublicHspParamType( const ChspV3AstParam &param )
{
	if ( param.is_array ) {
		return "array";
	}
	if ( param.base_type == "int" ) {
		return "int";
	}
	if ( param.base_type == "double" ) {
		return "double";
	}
	return "var";
}

void WriteFunctionWrapperToHsp( CMemBuf &buf, const ChspV3AstFunction &func, const std::string &internal_name,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	buf.PutStr( IsDefCFunc( func ) ? "#defcfunc " : "#deffunc " );
	buf.PutStr( NormalizeScopedName( func.name ).c_str() );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToPublicHspParamType( param ).c_str() );
		buf.PutStr( " " );
		buf.PutStr( NormalizeScopedName( param.name ).c_str() );
	}
	buf.PutCR();
	buf.PutStr( "    " );
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( "return@hsp " );
	}
	buf.PutStr( internal_name.c_str() );
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( "(" );
	}
	bool has_args = false;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		if ( IsDefCFunc( func ) ) {
			buf.PutStr( has_args ? ", " : "" );
		} else {
			buf.PutStr( has_args ? ", " : " " );
		}
		has_args = true;
		const auto public_name = NormalizeScopedName( param.name );
		buf.PutStr( public_name.c_str() );
		if ( needs_array_metadata && param.is_array ) {
			buf.PutStr( ", length@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length2@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length3@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length4@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( ")" );
		}
	}
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( ")" );
	}
	buf.PutCR();
	if ( !IsDefCFunc( func ) ) {
		buf.PutStr( "    return@hsp" );
		buf.PutCR();
	}
}

void WriteLocalDeclsToNative( CMemBuf &buf, const ChspV3AstFunction &func, const TranslateContext &ctx )
{
	for ( const auto &param : func.params ) {
		if ( !param.is_local ) {
			continue;
		}
		buf.PutStr( "    " );
		buf.PutStr( param.base_type.c_str() );
		buf.PutStr( " " );
		buf.PutStr( LookupCppIdentifier( ctx, param.name ).c_str() );
		if ( param.is_array ) {
			const int flat_size = FlatArraySize( param.array_dims );
			buf.PutStr( "[" );
			buf.PutStr( std::to_string( flat_size ).c_str() );
			buf.PutStr( "] = {0};" );
		} else if ( ctx.target == ChspNativeTarget::C || ctx.target == ChspNativeTarget::Plugin ) {
			buf.PutStr( " = 0;" );
		} else {
			buf.PutStr( " {};" );
		}
		buf.PutCR();
	}
}

bool WriteFunctionBodyToNative( CMemBuf &buf, const ChspV3AstFunction &func, TranslateContext &ctx, CLogger &logger )
{
	WriteLocalDeclsToNative( buf, func, ctx );
	int indent_level = 1;
	bool emitted_explicit_return = false;
	for ( size_t i = 0; i < func.body_stmts.size(); ++i ) {
		const auto &stmt = func.body_stmts[i];
		if ( stmt == nullptr ) {
			continue;
		}
		if ( !WriteFunctionStmtToCpp( buf, *stmt, ctx, func, logger, indent_level, emitted_explicit_return ) ) {
			return false;
		}
	}
	const auto ret = DefaultReturnExpr( func.return_type );
	if ( !ret.empty() && !emitted_explicit_return ) {
		buf.PutStr( "    return " );
		buf.PutStr( ret.c_str() );
		buf.PutStr( ";\n" );
	}
	return true;
}

bool WriteFunctionToNative( CMemBuf &buf, const ChspV3AstModule &module, const ChspV3AstFunction &func,
							const std::unordered_map<std::string, std::string> &function_cpp_names,
							const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
							const std::unordered_map<std::string, bool> &function_array_metadata_needs,
							ChspNativeTarget target, CLogger &logger, const ChspBuiltinMap &builtin_map )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	const auto cpp_name_it = function_cpp_names.find( func.name );
	const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
	auto ctx =
		BuildTranslateContext( func, module, function_cpp_names, function_defs, function_array_metadata_needs, target,
					   builtin_map );
	if ( target == ChspNativeTarget::Plugin ) {
		buf.PutStr( "static " );
	} else if ( target == ChspNativeTarget::C ) {
		buf.PutStr( "CHSP_EXPORT " );
	} else {
		buf.PutStr( "extern \"C\" CHSP_EXPORT " );
	}
	buf.PutStr( func.return_type.c_str() );
	buf.PutStr( " " );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "(" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		if ( !first ) {
			buf.PutStr( ", " );
		}
		first = false;
		buf.PutStr( ToNativeType( param, target ).c_str() );
		buf.PutStr( " " );
		buf.PutStr( LookupCppIdentifier( ctx, param.name ).c_str() );
		if ( needs_array_metadata && param.is_array && !param.is_local ) {
			const size_t param_index = &param - func.params.data();
			for ( int dim = 1; dim <= 4; ++dim ) {
				buf.PutStr( ", int " );
				buf.PutStr( ArrayDimensionCppName( func, param_index, dim ).c_str() );
			}
		}
	}
	buf.PutStr( ")" );
	buf.PutCR();
	buf.PutStr( "{\n" );
	if ( !WriteFunctionBodyToNative( buf, func, ctx, logger ) ) {
		return false;
	}
	buf.PutStr( "}\n\n" );
	return true;
}

void WriteFunctionPrototypeToNative( CMemBuf &buf, const ChspV3AstFunction &func, const ChspV3AstModule &module,
									 const std::unordered_map<std::string, std::string> &function_cpp_names,
									 const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
									 const std::unordered_map<std::string, bool> &function_array_metadata_needs,
									 ChspNativeTarget target, const ChspBuiltinMap &builtin_map )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	const auto cpp_name_it = function_cpp_names.find( func.name );
	const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
	auto ctx =
		BuildTranslateContext( func, module, function_cpp_names, function_defs, function_array_metadata_needs, target,
					   builtin_map );
	if ( target == ChspNativeTarget::Plugin ) {
		buf.PutStr( "static " );
	} else if ( target == ChspNativeTarget::C ) {
		buf.PutStr( "CHSP_EXPORT " );
	} else {
		buf.PutStr( "extern \"C\" CHSP_EXPORT " );
	}
	buf.PutStr( func.return_type.c_str() );
	buf.PutStr( " " );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "(" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		if ( !first ) {
			buf.PutStr( ", " );
		}
		first = false;
		buf.PutStr( ToNativeType( param, target ).c_str() );
		buf.PutStr( " " );
		buf.PutStr( LookupCppIdentifier( ctx, param.name ).c_str() );
		if ( needs_array_metadata && param.is_array && !param.is_local ) {
			const size_t param_index = &param - func.params.data();
			for ( int dim = 1; dim <= 4; ++dim ) {
				buf.PutStr( ", int " );
				buf.PutStr( ArrayDimensionCppName( func, param_index, dim ).c_str() );
			}
		}
	}
	buf.PutStr( ");\n" );
}

void WriteFunctionDeclToHspInternal( CMemBuf &buf, const ChspV3AstFunction &func, const std::string &name,
									 const std::string &cpp_name,
									 const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	buf.PutStr( IsDefCFunc( func ) ? "#cfunc " : "#func " );
	buf.PutStr( name.c_str() );
	buf.PutStr( " \"" );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "\"" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToHspParamType( param ).c_str() );
		if ( needs_array_metadata && param.is_array ) {
			for ( int dim = 0; dim < 4; ++dim ) {
				buf.PutStr( ", int" );
			}
		}
	}
	buf.PutCR();
}

void WritePluginFunctionDeclToHsp( CMemBuf &buf, const ChspV3AstFunction &func, int command_id )
{
	const std::string command_name = PluginCommandName( func );
	char idbuf[16];
	std::snprintf( idbuf, sizeof( idbuf ), "$%02x", command_id );
	buf.PutStr( "#cmd " );
	buf.PutStr( command_name.c_str() );
	buf.PutStr( " " );
	buf.PutStr( idbuf );
	buf.PutCR();
}

bool WritePluginNativeDispatch( CMemBuf &buf, const ChspV3AstModule &module,
								const std::unordered_map<std::string, std::string> &function_cpp_names,
								const std::unordered_map<std::string, const ChspV3AstFunction *> &function_defs,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs,
								CLogger &logger, const ChspBuiltinMap &builtin_map )
{
	for ( const auto &func : module.functions ) {
		WriteFunctionPrototypeToNative( buf, func, module, function_cpp_names, function_defs,
										function_array_metadata_needs, ChspNativeTarget::Plugin, builtin_map );
	}
	if ( !module.functions.empty() ) {
		buf.PutCR();
	}
	for ( const auto &func : module.functions ) {
		if ( !WriteFunctionToNative( buf, module, func, function_cpp_names, function_defs,
									 function_array_metadata_needs, ChspNativeTarget::Plugin, logger, builtin_map ) ) {
			return false;
		}
	}

	buf.PutStr( "static int chsp_plugin_ref_int;\n" );
	buf.PutStr( "static double chsp_plugin_ref_double;\n\n" );

	buf.PutStr( "static int cmdfunc( int cmd )\n{\n" );
	buf.PutStr( "    code_next();\n" );
	buf.PutStr( "    switch( cmd ) {\n" );
	for ( size_t i = 0; i < module.functions.size(); ++i ) {
		const auto &func = module.functions[i];
		const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
		const auto cpp_name_it = function_cpp_names.find( func.name );
		const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
		buf.PutStr( "    case " );
		buf.PutStr( std::to_string( i ).c_str() );
		buf.PutStr( ": {\n" );
		for ( const auto &param : func.params ) {
			if ( param.is_local ) {
				continue;
			}
			const std::string var_name = "arg_" + SanitizeForCppIdentifier( param.name );
			if ( param.is_array ) {
				buf.PutStr( "        PVal *pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = NULL; APTR aptr_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = code_getva( &pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " );\n" );
				buf.PutStr( "        " );
				buf.PutStr( param.base_type.c_str() );
				buf.PutStr( " *" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = " );
				buf.PutStr( param.base_type == "int" ? "chsp_plugin_int_ptr" : "chsp_plugin_double_ptr" );
				buf.PutStr( "( pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( ", aptr_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " );\n" );
				if ( needs_array_metadata ) {
					for ( int dim = 1; dim <= 4; ++dim ) {
						buf.PutStr( "        int " );
						buf.PutStr( var_name.c_str() );
						buf.PutStr( "_len" );
						buf.PutStr( std::to_string( dim ).c_str() );
						buf.PutStr( " = pval_" );
						buf.PutStr( var_name.c_str() );
						buf.PutStr( "->len[" );
						buf.PutStr( std::to_string( dim ).c_str() );
						buf.PutStr( "];\n" );
					}
				}
			} else {
				buf.PutStr( "        " );
				buf.PutStr( param.base_type.c_str() );
				buf.PutStr( " " );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = " );
				buf.PutStr( param.base_type == "int" ? "code_getdi(0)" : "exinfo->HspFunc_prm_getdd(0.0)" );
				buf.PutStr( ";\n" );
			}
		}
		buf.PutStr( "        " );
		if ( IsDefCFunc( func ) ) {
			buf.PutStr( func.return_type.c_str() );
			buf.PutStr( " result = " );
		}
		buf.PutStr( cpp_name.c_str() );
		buf.PutStr( "(" );
		bool first = true;
		for ( const auto &param : func.params ) {
			if ( param.is_local ) {
				continue;
			}
			if ( !first ) {
				buf.PutStr( ", " );
			}
			first = false;
			const std::string var_name = "arg_" + SanitizeForCppIdentifier( param.name );
			buf.PutStr( var_name.c_str() );
			if ( needs_array_metadata && param.is_array ) {
				for ( int dim = 1; dim <= 4; ++dim ) {
					buf.PutStr( ", " );
					buf.PutStr( var_name.c_str() );
					buf.PutStr( "_len" );
					buf.PutStr( std::to_string( dim ).c_str() );
				}
			}
		}
		buf.PutStr( ");\n" );
		if ( IsDefCFunc( func ) ) {
			if ( func.return_type == "double" ) {
				buf.PutStr( "        ctx->refdval = result;\n" );
			} else {
				buf.PutStr( "        stat = result;\n" );
			}
		}
		buf.PutStr( "        break;\n" );
		buf.PutStr( "    }\n" );
	}
	buf.PutStr( "    default:\n" );
	buf.PutStr( "        puterror( HSPERR_UNSUPPORTED_FUNCTION );\n" );
	buf.PutStr( "        break;\n" );
	buf.PutStr( "    }\n" );
	buf.PutStr( "    return RUNMODE_RUN;\n" );
	buf.PutStr( "}\n\n" );
	buf.PutStr( "static void *reffunc( int *type_res, int cmd )\n{\n" );
	buf.PutStr( "    if ( *type != TYPE_MARK ) puterror( HSPERR_INVALID_FUNCPARAM );\n" );
	buf.PutStr( "    if ( *val != '(' ) puterror( HSPERR_INVALID_FUNCPARAM );\n" );
	buf.PutStr( "    code_next();\n" );
	buf.PutStr( "    switch( cmd ) {\n" );
	for ( size_t i = 0; i < module.functions.size(); ++i ) {
		const auto &func = module.functions[i];
		if ( !IsDefCFunc( func ) ) {
			continue;
		}
		const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
		const auto cpp_name_it = function_cpp_names.find( func.name );
		const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
		buf.PutStr( "    case " );
		buf.PutStr( std::to_string( i ).c_str() );
		buf.PutStr( ": {\n" );
		for ( const auto &param : func.params ) {
			if ( param.is_local ) {
				continue;
			}
			const std::string var_name = "arg_" + SanitizeForCppIdentifier( param.name );
			if ( param.is_array ) {
				buf.PutStr( "        PVal *pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = NULL; APTR aptr_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = code_getva( &pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " );\n" );
				buf.PutStr( "        " );
				buf.PutStr( param.base_type.c_str() );
				buf.PutStr( " *" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = " );
				buf.PutStr( param.base_type == "int" ? "chsp_plugin_int_ptr" : "chsp_plugin_double_ptr" );
				buf.PutStr( "( pval_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( ", aptr_" );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " );\n" );
				if ( needs_array_metadata ) {
					for ( int dim = 1; dim <= 4; ++dim ) {
						buf.PutStr( "        int " );
						buf.PutStr( var_name.c_str() );
						buf.PutStr( "_len" );
						buf.PutStr( std::to_string( dim ).c_str() );
						buf.PutStr( " = pval_" );
						buf.PutStr( var_name.c_str() );
						buf.PutStr( "->len[" );
						buf.PutStr( std::to_string( dim ).c_str() );
						buf.PutStr( "];\n" );
					}
				}
			} else {
				buf.PutStr( "        " );
				buf.PutStr( param.base_type.c_str() );
				buf.PutStr( " " );
				buf.PutStr( var_name.c_str() );
				buf.PutStr( " = " );
				buf.PutStr( param.base_type == "int" ? "code_geti()" : "exinfo->HspFunc_prm_getd()" );
				buf.PutStr( ";\n" );
			}
		}
		if ( func.return_type == "double" ) {
			buf.PutStr( "        chsp_plugin_ref_double = " );
		} else {
			buf.PutStr( "        chsp_plugin_ref_int = " );
		}
		buf.PutStr( cpp_name.c_str() );
		buf.PutStr( "(" );
		bool first = true;
		for ( const auto &param : func.params ) {
			if ( param.is_local ) {
				continue;
			}
			if ( !first ) {
				buf.PutStr( ", " );
			}
			first = false;
			const std::string var_name = "arg_" + SanitizeForCppIdentifier( param.name );
			buf.PutStr( var_name.c_str() );
			if ( needs_array_metadata && param.is_array ) {
				for ( int dim = 1; dim <= 4; ++dim ) {
					buf.PutStr( ", " );
					buf.PutStr( var_name.c_str() );
					buf.PutStr( "_len" );
					buf.PutStr( std::to_string( dim ).c_str() );
				}
			}
		}
		buf.PutStr( ");\n" );
		buf.PutStr( "        break;\n" );
		buf.PutStr( "    }\n" );
	}
	buf.PutStr( "    default:\n" );
	buf.PutStr( "        puterror( HSPERR_UNSUPPORTED_FUNCTION );\n" );
	buf.PutStr( "        break;\n" );
	buf.PutStr( "    }\n" );
	buf.PutStr( "    if ( *type != TYPE_MARK ) puterror( HSPERR_INVALID_FUNCPARAM );\n" );
	buf.PutStr( "    if ( *val != ')' ) puterror( HSPERR_INVALID_FUNCPARAM );\n" );
	buf.PutStr( "    code_next();\n" );
	buf.PutStr( "    if ( cmd < 0 ) puterror( HSPERR_UNSUPPORTED_FUNCTION );\n" );
	buf.PutStr( "    switch( cmd ) {\n" );
	for ( size_t i = 0; i < module.functions.size(); ++i ) {
		const auto &func = module.functions[i];
		if ( !IsDefCFunc( func ) ) {
			continue;
		}
		buf.PutStr( "    case " );
		buf.PutStr( std::to_string( i ).c_str() );
		buf.PutStr( ":\n" );
		if ( func.return_type == "double" ) {
			buf.PutStr( "        *type_res = HSPVAR_FLAG_DOUBLE;\n" );
			buf.PutStr( "        return &chsp_plugin_ref_double;\n" );
		} else {
			buf.PutStr( "        *type_res = HSPVAR_FLAG_INT;\n" );
			buf.PutStr( "        return &chsp_plugin_ref_int;\n" );
		}
	}
	buf.PutStr( "    default:\n" );
	buf.PutStr( "        puterror( HSPERR_UNSUPPORTED_FUNCTION );\n" );
	buf.PutStr( "        return NULL;\n" );
	buf.PutStr( "    }\n" );
	buf.PutStr( "}\n\n" );
	buf.PutStr( "EXPORT void WINAPI hsp3cmdinit( HSP3TYPEINFO *info )\n{\n" );
	buf.PutStr( "    chsp_plugin_sdk_init( info );\n" );
	buf.PutStr( "    info->cmdfunc = cmdfunc;\n" );
	buf.PutStr( "    info->reffunc = reffunc;\n" );
	buf.PutStr( "    info->termfunc = NULL;\n" );
	buf.PutStr( "}\n" );
	return true;
}

std::string ModuleLibraryName( const std::string &file_stem )
{
#if defined( HSPWIN )
	return file_stem + ".dll";
#elif defined( HSPMAC )
	return file_stem + ".dylib";
#else
	return file_stem + ".so";
#endif
}

std::string DefaultModuleFileStem( const char *source_name, size_t module_index )
{
	std::string base = "chsp_module";
	if ( source_name != nullptr && source_name[0] != '\0' ) {
		std::filesystem::path source_path( source_name );
		if ( source_path.has_stem() ) {
			base = source_path.stem().string();
		}
	}
	return base + "_" + std::to_string( module_index + 1 );
}

std::string ModuleFileStem( const ChspV3AstModule &module, const char *source_name, size_t module_index )
{
	if ( !module.name.empty() ) {
		return module.name;
	}
	return DefaultModuleFileStem( source_name, module_index );
}

void WriteModuleHeaderToHsp( CMemBuf &buf, const std::string &module_tag, const std::string &file_stem,
							 ChspNativeTarget target )
{
	buf.PutStr( "goto@hsp *_" );
	buf.PutStr( module_tag.c_str() );
	buf.PutStr( "_exit" );
	buf.PutCR();
	buf.PutCR();

	if ( target == ChspNativeTarget::Plugin ) {
#if defined( HSPWIN )
		buf.PutStr( "#regcmd \"_hsp3cmdinit@4\", \"" );
#else
		buf.PutStr( "#regcmd \"hsp3cmdinit\", \"" );
#endif
		buf.PutStr( ModuleLibraryName( file_stem ).c_str() );
		buf.PutStr( "\"\n\n" );
	} else {
		buf.PutStr( "#uselib \"" );
		buf.PutStr( ModuleLibraryName( file_stem ).c_str() );
		buf.PutStr( "\"\n\n" );
	}
}

void WriteModuleFooterToHsp( CMemBuf &buf, const std::string &module_tag )
{
	buf.PutCR();
	buf.PutStr( "*_" );
	buf.PutStr( module_tag.c_str() );
	buf.PutStr( "_exit" );
	buf.PutCR();
}

void WriteOriginalSourceMarker( CMemBuf &buf, int line_number, const char *source_name )
{
	buf.PutStr( "##" );
	buf.PutStr( std::to_string( std::max( line_number, 0 ) ).c_str() );
	if ( source_name != nullptr && source_name[0] != '\0' ) {
		buf.PutStr( " \"" );
		buf.PutStr( source_name );
		buf.PutStr( "\"" );
	}
	buf.PutCR();
}

void WriteNativePreamble( CMemBuf &native_out, ChspNativeTarget target )
{
	native_out.PutStr( "// Generated by OpenHSP cHSP frontend. Do not edit this file directly.\n" );
	if ( target == ChspNativeTarget::Plugin ) {
		native_out.PutStr( "#include <stdlib.h>\n" );
		native_out.PutStr( "#include \"common/chsp/chsp_runtime.h\"\n" );
		native_out.PutStr( "#include \"common/chsp/hsp3plugin.h\"\n" );
		native_out.PutStr( "int p1,p2,p3,p4,p5,p6;\n" );
		native_out.PutStr( "int *type;\n" );
		native_out.PutStr( "int *val;\n" );
		native_out.PutStr( "PVal *mpval;\n" );
		native_out.PutStr( "HSPCTX *ctx;\n" );
		native_out.PutStr( "HSPEXINFO *exinfo;\n\n" );
		native_out.PutStr( "static void chsp_plugin_sdk_init( HSP3TYPEINFO *info )\n{\n" );
		native_out.PutStr( "    ctx = info->hspctx;\n" );
		native_out.PutStr( "    exinfo = info->hspexinfo;\n" );
		native_out.PutStr( "    type = exinfo->nptype;\n" );
		native_out.PutStr( "    val = exinfo->npval;\n" );
		native_out.PutStr( "}\n\n" );
		native_out.PutStr(
			"static int *chsp_plugin_int_ptr( PVal *pval, APTR aptr ) { return ((int *)pval->pt) + aptr; }\n" );
		native_out.PutStr( "static double *chsp_plugin_double_ptr( PVal *pval, APTR aptr ) { return ((double "
						   "*)pval->pt) + aptr; }\n\n" );
		return;
	}
	native_out.PutStr( "#include \"common/chsp/chsp_runtime.h\"\n\n" );
	native_out.PutStr( "#if defined(_WIN32)\n" );
	native_out.PutStr( "#define CHSP_EXPORT __declspec(dllexport)\n" );
	native_out.PutStr( "#else\n" );
	native_out.PutStr( "#define CHSP_EXPORT\n" );
	native_out.PutStr( "#endif\n\n" );
}

void WriteNativeSourceBlocks( CMemBuf &native_out, const ChspV3AstModule &module )
{
	for ( const auto &block : module.native_source_blocks ) {
		native_out.PutStr( block.c_str() );
		if ( block.empty() || block.back() != '\n' ) {
			native_out.PutCR();
		}
		native_out.PutCR();
	}
}

} // namespace

int GenerateProgramOutput( const ChspV3AstProgram &ast_program, CLogger &logger, CMemBuf &hsp_out,
						   std::vector<ChspNativeArtifact> &native_outputs, const char *source_name,
						   const ChspBuiltinMap &builtin_map, bool for_preprocessor )
{
	native_outputs.clear();
	native_outputs.reserve( ast_program.modules.size() );
	for ( size_t i = 0; i < ast_program.modules.size(); ++i ) {
		const auto &module = ast_program.modules[i];
		ChspNativeArtifact artifact;
		artifact.module_name = module.name;
		artifact.file_stem = ModuleFileStem( module, source_name, i );
		artifact.target = module.target;
		artifact.linked_libraries = module.linked_libraries;
		artifact.output = std::make_unique<CMemBuf>();
		WriteNativePreamble( *artifact.output, artifact.target );
		WriteNativeSourceBlocks( *artifact.output, module );
		native_outputs.push_back( std::move( artifact ) );
	}

	const auto function_cpp_names = BuildFunctionCppNames( ast_program );
	const auto function_defs = BuildFunctionDefs( ast_program );
	const auto function_array_metadata_needs = BuildFunctionArrayMetadataNeeds( ast_program, function_defs );

	for ( size_t i = 0; i < ast_program.modules.size(); ++i ) {
		auto &artifact = native_outputs[i];
		const auto &module = ast_program.modules[i];
		if ( artifact.target != ChspNativeTarget::C ) {
			continue;
		}
		for ( const auto &func : module.functions ) {
			WriteFunctionPrototypeToNative( *artifact.output, func, module, function_cpp_names, function_defs,
											function_array_metadata_needs, ChspNativeTarget::C, builtin_map );
		}
		if ( !module.functions.empty() ) {
			artifact.output->PutCR();
		}
	}

	size_t module_index = 0;
	size_t function_index = 0;
	bool in_function = false;
	std::string current_module_tag;
	int module_start_line = 0;
	int module_output_line_start = 0;

	auto count_newlines = []( CMemBuf &buf ) -> int {
		const char *p = buf.GetBuffer();
		int sz = buf.GetSize();
		int cnt = 0;
		for ( int j = 0; j < sz; ++j ) {
			if ( p[j] == '\n' ) cnt++;
		}
		return cnt;
	};

	for ( const auto &line : ast_program.source_lines ) {
		const auto trimmed = chsputil::Trim( line.text );
		switch ( line.directive ) {
		case ChspV3SourceDirectiveKind::None:
			if ( in_function ) {
				continue;
			}
			if ( chsputil::StartsWith( trimmed, "##chsp_" ) ) {
				continue;
			}
			if ( chsputil::StartsWith( trimmed, "#define " ) ) {
				for ( auto &artifact : native_outputs ) {
					artifact.output->PutStr( line.text.c_str() );
					artifact.output->PutCR();
				}
			}
			hsp_out.PutStr( line.text.c_str() );
			hsp_out.PutCR();
			break;
		case ChspV3SourceDirectiveKind::Module:
			if ( module_index >= ast_program.modules.size() ) {
				logger.Mesf( "#Error:Internal cHSP module index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			current_module_tag = "chsp_mod_" + std::to_string( module_index );
			module_start_line = line.line;
			module_output_line_start = count_newlines( hsp_out );
			if ( !for_preprocessor ) {
				WriteOriginalSourceMarker( hsp_out, line.line, source_name );
			}
			WriteModuleHeaderToHsp( hsp_out, current_module_tag, native_outputs[module_index].file_stem,
									native_outputs[module_index].target );
			in_function = false;
			function_index = 0;
			break;
		case ChspV3SourceDirectiveKind::ModuleEnd:
			if ( module_index >= native_outputs.size() ) {
				logger.Mesf( "#Error:Internal cHSP module output mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target == ChspNativeTarget::Plugin ) {
				if ( !WritePluginNativeDispatch( *native_outputs[module_index].output,
												 ast_program.modules[module_index], function_cpp_names, function_defs,
												 function_array_metadata_needs, logger, builtin_map ) ) {
					return -1;
				}
			}
			WriteModuleFooterToHsp( hsp_out, current_module_tag );
			if ( !for_preprocessor ) {
				WriteOriginalSourceMarker( hsp_out, line.line + 1, source_name );
			} else {
				int module_end_line = line.line;
				int module_output_line_end = count_newlines( hsp_out );
				int emitted = module_output_line_end - module_output_line_start;
				int expected = module_end_line - module_start_line + 1;
				for ( int pad = emitted; pad < expected; ++pad ) {
					hsp_out.PutCR();
				}
			}
			++module_index;
			in_function = false;
			break;
		case ChspV3SourceDirectiveKind::ChspC:
		case ChspV3SourceDirectiveKind::ChspCDecl:
		case ChspV3SourceDirectiveKind::ChspCLink:
			break;
		case ChspV3SourceDirectiveKind::DefFunc:
		case ChspV3SourceDirectiveKind::DefCFunc:
			in_function = true;
			if ( module_index >= ast_program.modules.size() ||
				 function_index >= ast_program.modules[module_index].functions.size() ) {
				logger.Mesf( "#Error:Internal cHSP function index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target == ChspNativeTarget::Plugin ) {
				WritePluginFunctionDeclToHsp( hsp_out, ast_program.modules[module_index].functions[function_index],
											  static_cast<int>( function_index ) );
			} else {
				const auto &func = ast_program.modules[module_index].functions[function_index];
				const auto cpp_name_it = function_cpp_names.find( func.name );
				const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
				if ( FunctionNeedsArrayMetadata( function_array_metadata_needs, func ) ) {
					const auto internal_name = HspInternalFunctionDeclName( func );
					WriteFunctionDeclToHspInternal( hsp_out, func, internal_name, cpp_name,
													function_array_metadata_needs );
					WriteFunctionWrapperToHsp( hsp_out, func, internal_name, function_array_metadata_needs );
				} else {
					WriteFunctionDeclToHsp( hsp_out, func, cpp_name );
				}
			}
			break;
		case ChspV3SourceDirectiveKind::End:
			if ( module_index >= ast_program.modules.size() ||
				 function_index >= ast_program.modules[module_index].functions.size() ) {
				logger.Mesf( "#Error:Internal cHSP end index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target != ChspNativeTarget::Plugin ) {
				if ( !WriteFunctionToNative( *native_outputs[module_index].output, ast_program.modules[module_index],
											 ast_program.modules[module_index].functions[function_index],
											 function_cpp_names, function_defs, function_array_metadata_needs,
											 native_outputs[module_index].target, logger, builtin_map ) ) {
					return -1;
				}
			}
			++function_index;
			in_function = false;
			break;
		}
	}

	return 0;
}

} // namespace chspv3
