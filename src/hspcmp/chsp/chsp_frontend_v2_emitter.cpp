#include "chsp_frontend_v2_internal.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../membuf.h"
#include "logger.h"

namespace chspv2
{
namespace
{

struct TranslateContext
{
	ChspNativeTarget target = ChspNativeTarget::Plugin;
	std::unordered_set<std::string> array_names;
	std::unordered_map<std::string, std::string> identifier_cpp_names;
	std::unordered_map<std::string, std::string> function_cpp_names;
	std::map<std::string, int> array_strides;
	std::vector<std::string> loop_stack;
};

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

std::string MakeFunctionCppName( const ChspFunction &func )
{
	return "chsp_func_" + SanitizeForCppIdentifier( func.name );
}

std::unordered_map<std::string, std::string> BuildFunctionCppNames( const ChspProgram &program )
{
	std::unordered_map<std::string, std::string> names;
	for ( const auto &module : program.modules ) {
		for ( const auto &function : module.functions ) {
			names[function.name] = MakeFunctionCppName( function );
		}
	}
	return names;
}

std::unordered_map<std::string, std::string> BuildIdentifierCppNames( const ChspFunction &func )
{
	std::unordered_map<std::string, std::string> names;
	const std::string func_key = SanitizeForCppIdentifier( func.name );
	for ( size_t i = 0; i < func.params.size(); ++i ) {
		const auto &param = func.params[i];
		names[param.name] =
			"chsp_var_" + func_key + "_" + std::to_string( i ) + "_" + SanitizeForCppIdentifier( param.name );
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

std::string ToHspParamType( const ChspParam &param )
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

std::string PluginCommandName( const ChspFunction &func )
{
	return func.name;
}

std::string ToCppType( const ChspParam &param )
{
	if ( param.is_array ) {
		return param.base_type + " *";
	}
	return param.base_type;
}

std::string ToNativeType( const ChspParam &param, ChspNativeTarget )
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

int ExprPrecedence( const ChspExpr &expr )
{
	switch ( expr.kind ) {
	case ChspExprKind::Binary:
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
	case ChspExprKind::Unary:
		return 60;
	case ChspExprKind::Call:
	case ChspExprKind::Identifier:
	case ChspExprKind::Label:
	case ChspExprKind::IntLiteral:
	case ChspExprKind::DoubleLiteral:
	case ChspExprKind::StringLiteral:
	case ChspExprKind::Group:
		return 70;
	default:
		return 0;
	}
}

void CollectArrayStrideFromExpr( const ChspExpr &expr, const std::unordered_set<std::string> &array_names,
								 std::map<std::string, int> &array_strides )
{
	if ( expr.kind == ChspExprKind::Call && !expr.children.empty() ) {
		const auto &callee = expr.children[0];
		if ( callee != nullptr && callee->kind == ChspExprKind::Identifier &&
			 array_names.find( callee->text ) != array_names.end() && expr.children.size() == 3 ) {
			const auto &index_expr = expr.children[2];
			if ( index_expr != nullptr && index_expr->kind == ChspExprKind::IntLiteral ) {
				array_strides[callee->text] = std::max( array_strides[callee->text], index_expr->int_value + 1 );
			}
		}
	}
	for ( const auto &child : expr.children ) {
		if ( child != nullptr ) {
			CollectArrayStrideFromExpr( *child, array_names, array_strides );
		}
	}
}

void CollectArrayStrideFromStmt( const ChspStmt &stmt, const std::unordered_set<std::string> &array_names,
								 std::map<std::string, int> &array_strides )
{
	if ( stmt.lhs != nullptr ) {
		CollectArrayStrideFromExpr( *stmt.lhs, array_names, array_strides );
	}
	if ( stmt.rhs != nullptr ) {
		CollectArrayStrideFromExpr( *stmt.rhs, array_names, array_strides );
	}
	for ( const auto &expr : stmt.exprs ) {
		if ( expr != nullptr ) {
			CollectArrayStrideFromExpr( *expr, array_names, array_strides );
		}
	}
	for ( const auto &child : stmt.children ) {
		if ( child != nullptr ) {
			CollectArrayStrideFromStmt( *child, array_names, array_strides );
		}
	}
}

TranslateContext BuildTranslateContext( const ChspFunction &func,
										const std::unordered_map<std::string, std::string> &function_cpp_names,
										ChspNativeTarget target )
{
	TranslateContext ctx;
	ctx.target = target;
	ctx.function_cpp_names = function_cpp_names;
	ctx.identifier_cpp_names = BuildIdentifierCppNames( func );
	for ( const auto &param : func.params ) {
		if ( param.is_array ) {
			ctx.array_names.insert( param.name );
		}
	}
	for ( const auto &stmt : func.body_stmts ) {
		if ( stmt != nullptr ) {
			CollectArrayStrideFromStmt( *stmt, ctx.array_names, ctx.array_strides );
		}
	}
	return ctx;
}

std::string TranslateExpr( const ChspExpr &expr, const TranslateContext &ctx, bool &ok, int parent_precedence = 0,
						   bool paren_on_equal = false )
{
	const auto wrap_if_needed = [&]( std::string text, int self_precedence ) {
		if ( self_precedence < parent_precedence || ( paren_on_equal && self_precedence == parent_precedence ) ) {
			return std::string( "(" ) + text + ")";
		}
		return text;
	};

	switch ( expr.kind ) {
	case ChspExprKind::IntLiteral:
		return std::to_string( expr.int_value );
	case ChspExprKind::DoubleLiteral:
		if ( !expr.text.empty() ) {
			return expr.text;
		}
		return std::to_string( expr.double_value );
	case ChspExprKind::StringLiteral:
		return "\"" + expr.text + "\"";
	case ChspExprKind::Label:
		return expr.text;
	case ChspExprKind::Identifier:
		if ( expr.text == "cnt" && !ctx.loop_stack.empty() ) {
			return ctx.loop_stack.back();
		}
		return LookupCppIdentifier( ctx, expr.text );
	case ChspExprKind::Unary:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( "-" + TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspExprKind::Binary:
		if ( expr.children.size() != 2 || expr.children[0] == nullptr || expr.children[1] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), false ) + " " +
								   OperatorText( expr.token_kind ) + " " +
								   TranslateExpr( *expr.children[1], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspExprKind::Group:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return "(" + TranslateExpr( *expr.children[0], ctx, ok, 0, false ) + ")";
	case ChspExprKind::Call:
		break;
	}

	if ( expr.children.empty() || expr.children[0] == nullptr || expr.children[0]->kind != ChspExprKind::Identifier ) {
		ok = false;
		return "";
	}

	const std::string name = expr.children[0]->text;
	if ( ctx.array_names.find( name ) != ctx.array_names.end() ) {
		const std::string cpp_name = LookupCppIdentifier( ctx, name );
		if ( expr.children.size() == 2 ) {
			return cpp_name + "[" + TranslateExpr( *expr.children[1], ctx, ok, 0, false ) + "]";
		}
		if ( expr.children.size() == 3 ) {
			int stride = 0;
			const auto stride_it = ctx.array_strides.find( name );
			if ( stride_it != ctx.array_strides.end() ) {
				stride = stride_it->second;
			} else if ( expr.children[2] != nullptr && expr.children[2]->kind == ChspExprKind::IntLiteral &&
						expr.children[2]->int_value >= 0 && expr.children[2]->int_value <= 2 ) {
				stride = 3;
			} else {
				ok = false;
				return "";
			}
			return cpp_name + "[(" + TranslateExpr( *expr.children[1], ctx, ok, 0, false ) + ") * " +
				   std::to_string( stride ) + " + (" + TranslateExpr( *expr.children[2], ctx, ok, 0, false ) + ")]";
		}
		ok = false;
		return "";
	}

	std::string target = BuiltinTarget( name, expr.children.size() - 1, ctx.target );
	if ( target.empty() ) {
		const auto function_it = ctx.function_cpp_names.find( name );
		if ( function_it != ctx.function_cpp_names.end() ) {
			target = function_it->second;
		} else {
			target = LookupCppIdentifier( ctx, name );
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
	}
	out += ")";
	return out;
}

std::string MakeIndent( int level )
{
	return std::string( level * 4, ' ' );
}

bool IsBlockOpeningStmt( const ChspStmt &stmt )
{
	if ( stmt.kind == ChspStmtKind::Repeat ) {
		return true;
	}
	return ( stmt.kind == ChspStmtKind::If || stmt.kind == ChspStmtKind::Else ) && stmt.token_kind == '{';
}

bool HasTrailingBlockElseChild( const ChspStmt &stmt, size_t &else_index )
{
	if ( stmt.kind != ChspStmtKind::If || stmt.token_kind != ':' ) {
		return false;
	}
	for ( size_t i = 0; i < stmt.children.size(); ++i ) {
		if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspStmtKind::Else &&
			 stmt.children[i]->token_kind == '{' ) {
			else_index = i;
			return true;
		}
	}
	return false;
}

std::string RenderStatementInline( const ChspStmt &stmt, TranslateContext &ctx, bool &ok );

std::string RenderCommandCall( const ChspStmt &stmt, TranslateContext &ctx, bool &ok )
{
	std::string target = BuiltinTarget( stmt.text, stmt.exprs.size(), ctx.target );
	if ( target.empty() ) {
		const auto function_it = ctx.function_cpp_names.find( stmt.text );
		if ( function_it != ctx.function_cpp_names.end() ) {
			target = function_it->second;
		} else {
			target = LookupCppIdentifier( ctx, stmt.text );
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
	}
	out += ")";
	return out;
}

std::string RenderStatementInline( const ChspStmt &stmt, TranslateContext &ctx, bool &ok )
{
	switch ( stmt.kind ) {
	case ChspStmtKind::Return:
		if ( stmt.rhs == nullptr ) {
			return "return;";
		}
		return "return " + TranslateExpr( *stmt.rhs, ctx, ok ) + ";";
	case ChspStmtKind::Assignment: {
		if ( stmt.lhs == nullptr ) {
			ok = false;
			return "";
		}
		const auto lhs = TranslateExpr( *stmt.lhs, ctx, ok );
		if ( stmt.text == "++" || stmt.text == "--" ) {
			return lhs + stmt.text + ";";
		}
		if ( stmt.rhs == nullptr ) {
			ok = false;
			return "";
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
	case ChspStmtKind::Command:
		return RenderCommandCall( stmt, ctx, ok ) + ";";
	case ChspStmtKind::If:
		if ( stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
			ok = false;
			return "";
		}
		if ( stmt.token_kind == '{' ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") {";
		}
		if ( stmt.token_kind != ':' || stmt.children.empty() || stmt.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		if ( stmt.children.size() == 1 ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") " +
				   RenderStatementInline( *stmt.children[0], ctx, ok );
		}
		if ( stmt.children.size() == 2 && stmt.children[1] != nullptr &&
			 stmt.children[1]->kind == ChspStmtKind::Else ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") " +
				   RenderStatementInline( *stmt.children[0], ctx, ok ) + " " +
				   RenderStatementInline( *stmt.children[1], ctx, ok );
		}
		ok = false;
		return "";
	case ChspStmtKind::Else:
		if ( stmt.token_kind == '{' ) {
			return "else {";
		}
		if ( stmt.token_kind != ':' || stmt.children.size() != 1 || stmt.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return "else " + RenderStatementInline( *stmt.children[0], ctx, ok );
	default:
		ok = false;
		return "";
	}
}

void WriteFunctionStmtToCpp( CMemBuf &buf, const ChspStmt &stmt, TranslateContext &ctx, int &indent_level,
							 bool &emitted_explicit_return )
{
	switch ( stmt.kind ) {
	case ChspStmtKind::Empty:
		return;
	case ChspStmtKind::BlockMarker:
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}" );
		buf.PutCR();
		return;
	case ChspStmtKind::Loop:
		if ( !ctx.loop_stack.empty() ) {
			ctx.loop_stack.pop_back();
		}
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}\n" );
		return;
	case ChspStmtKind::Repeat: {
		bool ok = true;
		std::string expr = "0";
		if ( stmt.rhs != nullptr ) {
			expr = TranslateExpr( *stmt.rhs, ctx, ok );
		}
		if ( !ok ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( "// unsupported: repeat\n" );
			return;
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
		return;
	}
	case ChspStmtKind::If: {
		bool ok = true;
		if ( stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
			ok = false;
		}
		if ( stmt.token_kind == '{' ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			if ( ok ) {
				buf.PutStr( "if (" );
				const auto cond = TranslateExpr( *stmt.exprs[0], ctx, ok );
				buf.PutStr( cond.c_str() );
				buf.PutStr( ") {" );
				buf.PutCR();
				if ( ok ) {
					++indent_level;
					return;
				}
			}
		} else if ( stmt.token_kind == ':' ) {
			size_t else_index = stmt.children.size();
			for ( size_t i = 0; i < stmt.children.size(); ++i ) {
				if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspStmtKind::Else ) {
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
							WriteFunctionStmtToCpp( buf, *stmt.children[i], ctx, indent_level,
													emitted_explicit_return );
						}
					}
					indent_level = std::max( 1, indent_level - 1 );
					buf.PutStr( MakeIndent( indent_level ).c_str() );
					buf.PutStr( "}" );
					buf.PutCR();
					if ( else_index != stmt.children.size() && stmt.children[else_index] != nullptr ) {
						const auto &else_stmt = *stmt.children[else_index];
						if ( else_stmt.token_kind == '{' ) {
							buf.PutStr( MakeIndent( indent_level ).c_str() );
							buf.PutStr( "else {" );
							buf.PutCR();
							++indent_level;
							for ( const auto &child : else_stmt.children ) {
								if ( child != nullptr ) {
									WriteFunctionStmtToCpp( buf, *child, ctx, indent_level, emitted_explicit_return );
								}
							}
							indent_level = std::max( 1, indent_level - 1 );
							buf.PutStr( MakeIndent( indent_level ).c_str() );
							buf.PutStr( "}" );
							buf.PutCR();
						} else {
							bool else_ok = true;
							const auto rendered = RenderStatementInline( else_stmt, ctx, else_ok );
							if ( else_ok && !rendered.empty() ) {
								buf.PutStr( MakeIndent( indent_level ).c_str() );
								buf.PutStr( rendered.c_str() );
								buf.PutCR();
							} else {
								WriteFunctionStmtToCpp( buf, else_stmt, ctx, indent_level, emitted_explicit_return );
							}
						}
					}
					return;
				}
			} else {
				const auto rendered = RenderStatementInline( stmt, ctx, ok );
				if ( ok ) {
					buf.PutStr( MakeIndent( indent_level ).c_str() );
					buf.PutStr( rendered.c_str() );
					buf.PutCR();
					if ( StartsWith( Trim( rendered ), "return" ) ) {
						emitted_explicit_return = true;
					}
					return;
				}
			}
		}
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "// unsupported: if\n" );
		return;
	}
	case ChspStmtKind::Else: {
		bool ok = true;
		if ( stmt.token_kind == '{' ) {
			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( "else {" );
			buf.PutCR();
			++indent_level;
			return;
		}
		if ( stmt.token_kind == ':' ) {
			if ( stmt.children.size() > 1 ) {
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( "else {" );
				buf.PutCR();
				++indent_level;
				for ( const auto &child : stmt.children ) {
					if ( child != nullptr ) {
						WriteFunctionStmtToCpp( buf, *child, ctx, indent_level, emitted_explicit_return );
					}
				}
				indent_level = std::max( 1, indent_level - 1 );
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( "}" );
				buf.PutCR();
				return;
			}
			const auto rendered = RenderStatementInline( stmt, ctx, ok );
			if ( ok ) {
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( rendered.c_str() );
				buf.PutCR();
				if ( StartsWith( Trim( rendered ), "else return" ) ) {
					emitted_explicit_return = true;
				}
				return;
			}
		}
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "// unsupported: else\n" );
		return;
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
		if ( StartsWith( Trim( rendered ), "return" ) ) {
			emitted_explicit_return = true;
		}
		return;
	}
	buf.PutStr( "// unsupported statement\n" );
}

bool WriteInlineIfWithTrailingBlockElse( CMemBuf &buf, const ChspStmt &stmt,
										 const std::vector<std::unique_ptr<ChspStmt>> &stmts, size_t &index,
										 TranslateContext &ctx, int indent_level, bool &emitted_explicit_return )
{
	size_t else_index = 0;
	if ( !HasTrailingBlockElseChild( stmt, else_index ) || stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
		return false;
	}

	bool ok = true;
	const auto cond = TranslateExpr( *stmt.exprs[0], ctx, ok );
	if ( !ok ) {
		return false;
	}

	buf.PutStr( MakeIndent( indent_level ).c_str() );
	buf.PutStr( "if (" );
	buf.PutStr( cond.c_str() );
	buf.PutStr( ") {" );
	buf.PutCR();

	int if_indent = indent_level + 1;
	for ( size_t i = 0; i < else_index; ++i ) {
		if ( stmt.children[i] != nullptr ) {
			WriteFunctionStmtToCpp( buf, *stmt.children[i], ctx, if_indent, emitted_explicit_return );
		}
	}

	buf.PutStr( MakeIndent( indent_level ).c_str() );
	buf.PutStr( "} else {" );
	buf.PutCR();

	int else_indent = indent_level + 1;
	int depth = 1;
	size_t cursor = index + 1;
	for ( ; cursor < stmts.size(); ++cursor ) {
		if ( stmts[cursor] == nullptr ) {
			continue;
		}
		const auto &next = *stmts[cursor];
		if ( next.kind == ChspStmtKind::Loop || next.kind == ChspStmtKind::BlockMarker ) {
			--depth;
			if ( depth == 0 ) {
				break;
			}
			WriteFunctionStmtToCpp( buf, next, ctx, else_indent, emitted_explicit_return );
			continue;
		}
		WriteFunctionStmtToCpp( buf, next, ctx, else_indent, emitted_explicit_return );
		if ( IsBlockOpeningStmt( next ) ) {
			++depth;
		}
	}
	if ( depth != 0 ) {
		return false;
	}

	buf.PutStr( MakeIndent( indent_level ).c_str() );
	buf.PutStr( "}" );
	buf.PutCR();
	index = cursor;
	return true;
}

void WriteFunctionDeclToHsp( CMemBuf &buf, const ChspFunction &func, const std::string &cpp_name )
{
	buf.PutStr( func.kind == ChspFuncKind::DefCFunc ? "#cfunc " : "#func " );
	buf.PutStr( func.name.c_str() );
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

void WriteLocalDeclsToNative( CMemBuf &buf, const ChspFunction &func, const TranslateContext &ctx )
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
			buf.PutStr( "[" );
			buf.PutStr( std::to_string( param.array_length ).c_str() );
			buf.PutStr( "] = {0};" );
		} else if ( ctx.target == ChspNativeTarget::C || ctx.target == ChspNativeTarget::Plugin ) {
			buf.PutStr( " = 0;" );
		} else {
			buf.PutStr( " {};" );
		}
		buf.PutCR();
	}
}

void WriteFunctionBodyToNative( CMemBuf &buf, const ChspFunction &func, TranslateContext &ctx )
{
	WriteLocalDeclsToNative( buf, func, ctx );
	int indent_level = 1;
	bool emitted_explicit_return = false;
	for ( size_t i = 0; i < func.body_stmts.size(); ++i ) {
		const auto &stmt = func.body_stmts[i];
		if ( stmt == nullptr ) {
			continue;
		}
		if ( WriteInlineIfWithTrailingBlockElse( buf, *stmt, func.body_stmts, i, ctx, indent_level,
												 emitted_explicit_return ) ) {
			continue;
		}
		if ( stmt->kind == ChspStmtKind::BlockMarker && i + 1 < func.body_stmts.size() &&
			 func.body_stmts[i + 1] != nullptr && func.body_stmts[i + 1]->kind == ChspStmtKind::Else ) {
			const auto &else_stmt = *func.body_stmts[i + 1];
			const bool is_else_if = else_stmt.token_kind == ':' && else_stmt.children.size() == 1 &&
									else_stmt.children[0] != nullptr && else_stmt.children[0]->kind == ChspStmtKind::If;
			int line_indent = indent_level;
			if ( !is_else_if ) {
				line_indent = std::max( 1, indent_level - 1 );
			}
			bool ok = true;
			const auto rendered = RenderStatementInline( else_stmt, ctx, ok );
			buf.PutStr( MakeIndent( line_indent ).c_str() );
			if ( ok && !rendered.empty() ) {
				buf.PutStr( "} " );
				buf.PutStr( rendered.c_str() );
				buf.PutCR();
				indent_level = line_indent;
				if ( !rendered.empty() && rendered.back() == '{' ) {
					++indent_level;
				}
			} else {
				buf.PutStr( "}" );
				buf.PutCR();
				indent_level = line_indent;
				WriteFunctionStmtToCpp( buf, else_stmt, ctx, indent_level, emitted_explicit_return );
			}
			++i;
			continue;
		}
		WriteFunctionStmtToCpp( buf, *stmt, ctx, indent_level, emitted_explicit_return );
	}
	const auto ret = DefaultReturnExpr( func.return_type );
	if ( !ret.empty() && !emitted_explicit_return ) {
		buf.PutStr( "    return " );
		buf.PutStr( ret.c_str() );
		buf.PutStr( ";\n" );
	}
}

void WriteFunctionToNative( CMemBuf &buf, const ChspFunction &func,
							const std::unordered_map<std::string, std::string> &function_cpp_names,
							ChspNativeTarget target )
{
	const auto cpp_name_it = function_cpp_names.find( func.name );
	const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
	auto ctx = BuildTranslateContext( func, function_cpp_names, target );
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
	}
	buf.PutStr( ")" );
	buf.PutCR();
	buf.PutStr( "{\n" );
	WriteFunctionBodyToNative( buf, func, ctx );
	buf.PutStr( "}\n\n" );
}

void WritePluginFunctionDeclToHsp( CMemBuf &buf, const ChspFunction &func, int command_id )
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

void WritePluginNativeDispatch( CMemBuf &buf, const ChspModule &module,
								const std::unordered_map<std::string, std::string> &function_cpp_names )
{
	for ( const auto &func : module.functions ) {
		WriteFunctionToNative( buf, func, function_cpp_names, ChspNativeTarget::Plugin );
	}

	buf.PutStr( "static int chsp_plugin_ref_int;\n" );
	buf.PutStr( "static double chsp_plugin_ref_double;\n\n" );

	buf.PutStr( "static int cmdfunc( int cmd )\n{\n" );
	buf.PutStr( "    code_next();\n" );
	buf.PutStr( "    switch( cmd ) {\n" );
	for ( size_t i = 0; i < module.functions.size(); ++i ) {
		const auto &func = module.functions[i];
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
		if ( func.kind == ChspFuncKind::DefCFunc ) {
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
		}
		buf.PutStr( ");\n" );
		if ( func.kind == ChspFuncKind::DefCFunc ) {
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
		if ( func.kind != ChspFuncKind::DefCFunc ) {
			continue;
		}
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
		if ( func.kind != ChspFuncKind::DefCFunc ) {
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
}

std::string ModuleLibraryName( const std::string &module_name )
{
#if defined( HSPWIN )
	return module_name + ".dll";
#elif defined( HSPMAC )
	return module_name + ".dylib";
#else
	return module_name + ".so";
#endif
}

void WriteModuleHeaderToHsp( CMemBuf &buf, const std::string &module_tag, const std::string &module_name, ChspNativeTarget target )
{
	buf.PutStr( "#module " );
	buf.PutStr( module_tag.c_str() );
	buf.PutCR();
	buf.PutStr( "goto@hsp *_" );
	buf.PutStr( module_tag.c_str() );
	buf.PutStr( "_exit" );
	buf.PutCR();
	buf.PutCR();

	if ( target == ChspNativeTarget::Plugin ) {
		buf.PutStr( "#regcmd \"hsp3cmdinit\", \"" );
		buf.PutStr( ModuleLibraryName( module_name ).c_str() );
		buf.PutStr( "\"\n\n" );
	} else {
		buf.PutStr( "#uselib \"" );
		buf.PutStr( ModuleLibraryName( module_name ).c_str() );
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
	buf.PutStr( "#global" );
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
		native_out.PutStr( "static int *chsp_plugin_int_ptr( PVal *pval, APTR aptr ) { return ((int *)pval->pt) + aptr; }\n" );
		native_out.PutStr( "static double *chsp_plugin_double_ptr( PVal *pval, APTR aptr ) { return ((double *)pval->pt) + aptr; }\n\n" );
		return;
	}
	native_out.PutStr( "#include \"common/chsp/chsp_runtime.h\"\n\n" );
	native_out.PutStr( "#if defined(_WIN32)\n" );
	native_out.PutStr( "#define CHSP_EXPORT __declspec(dllexport)\n" );
	native_out.PutStr( "#else\n" );
	native_out.PutStr( "#define CHSP_EXPORT\n" );
	native_out.PutStr( "#endif\n\n" );
}

} // namespace

int GenerateProgramOutput( const std::vector<ChspSourceLine> &lines, const ChspProgram &program, CLogger &logger,
						   CMemBuf &hsp_out, CMemBuf &native_out, const char *source_name, ChspNativeTarget target )
{
	if ( target == ChspNativeTarget::Plugin && program.modules.size() > 1 ) {
		logger.Mesf( "#Error:cHSP plugin backend currently supports exactly one #chsp_module [%s]",
					 source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}
	WriteNativePreamble( native_out, target );

	const auto function_cpp_names = BuildFunctionCppNames( program );

	size_t module_index = 0;
	size_t function_index = 0;
	bool in_function = false;
	std::string current_module_tag;

	for ( const auto &line : lines ) {
		const auto trimmed = Trim( line.text );
		switch ( line.directive ) {
		case ChspDirectiveKind::None:
			if ( in_function ) {
				continue;
			}
			if ( StartsWith( trimmed, "##chsp_" ) ) {
				continue;
			}
			if ( StartsWith( trimmed, "#define " ) ) {
				native_out.PutStr( line.text.c_str() );
				native_out.PutCR();
			}
			hsp_out.PutStr( line.text.c_str() );
			hsp_out.PutCR();
			break;
		case ChspDirectiveKind::Module:
			if ( module_index >= program.modules.size() ) {
				logger.Mesf( "#Error:Internal cHSP module index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			current_module_tag = "m" + std::to_string( module_index );
			WriteModuleHeaderToHsp( hsp_out, current_module_tag, program.modules[module_index].name, target );
			function_index = 0;
			++module_index;
			break;
		case ChspDirectiveKind::ModuleEnd:
			WriteModuleFooterToHsp( hsp_out, current_module_tag );
			current_module_tag.clear();
			break;
		case ChspDirectiveKind::DefFunc:
		case ChspDirectiveKind::DefCFunc:
			in_function = true;
			break;
		case ChspDirectiveKind::End:
			if ( !in_function ) {
				logger.Mesf( "#Error:Unexpected #chsp_end in line %d [%s]", line.line,
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( module_index == 0 || module_index > program.modules.size() ) {
				logger.Mesf( "#Error:Internal cHSP function module mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			{
				const auto &module = program.modules[module_index - 1];
				if ( function_index >= module.functions.size() ) {
					logger.Mesf( "#Error:Internal cHSP function index mismatch [%s]",
								 source_name != nullptr ? source_name : "<buffer>" );
					return -1;
				}
				const auto &function = module.functions[function_index];
				const auto cpp_name_it = function_cpp_names.find( function.name );
				const std::string cpp_name =
					cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : function.name;
				if ( target == ChspNativeTarget::Plugin ) {
					WritePluginFunctionDeclToHsp( hsp_out, function, static_cast<int>( function_index ) );
					if ( function_index + 1 == module.functions.size() ) {
						WritePluginNativeDispatch( native_out, module, function_cpp_names );
					}
				} else {
					WriteFunctionDeclToHsp( hsp_out, function, cpp_name );
					WriteFunctionToNative( native_out, function, function_cpp_names, target );
				}
			}
			++function_index;
			in_function = false;
			break;
		}
	}

	if ( in_function ) {
		logger.Mesf( "#Error:Missing #chsp_end [%s]", source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}
	return 0;
}

} // namespace chspv2
