#include "chsp_c_emitter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../membuf.h"
#include "chsp_types.h"
#include "logger.h"

namespace chsp
{
namespace
{

bool AppendArrayArgumentMetadata( const ChspAstExpr &arg_expr, const TranslateContext &ctx, bool &ok,
								  std::string &out )
{
	if ( arg_expr.kind != ChspAstExprKind::Identifier ) {
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

std::string TranslateExpr( const ChspAstExpr &expr, const TranslateContext &ctx, bool &ok, int parent_precedence,
						   bool paren_on_equal );


std::string TranslateArrayAccess( const ChspAstExpr &expr, const TranslateContext &ctx, bool &ok )
{
	if ( expr.children.empty() || expr.children[0] == nullptr ||
		 expr.children[0]->kind != ChspAstExprKind::Identifier ) {
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

std::string TranslateExpr( const ChspAstExpr &expr, const TranslateContext &ctx, bool &ok, int parent_precedence = 0,
						   bool paren_on_equal = false )
{
	const auto wrap_if_needed = [&]( std::string text, int self_precedence ) {
		if ( self_precedence < parent_precedence || ( paren_on_equal && self_precedence == parent_precedence ) ) {
			return std::string( "(" ) + text + ")";
		}
		return text;
	};

	switch ( expr.kind ) {
	case ChspAstExprKind::IntLiteral:
		return expr.text;
	case ChspAstExprKind::DoubleLiteral:
		return expr.text;
	case ChspAstExprKind::StringLiteral:
		return "\"" + expr.text + "\"";
	case ChspAstExprKind::Label:
		return expr.text;
	case ChspAstExprKind::Identifier:
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
	case ChspAstExprKind::Unary:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( "-" + TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspAstExprKind::Binary:
		if ( expr.children.size() != 2 || expr.children[0] == nullptr || expr.children[1] == nullptr ) {
			ok = false;
			return "";
		}
		return wrap_if_needed( TranslateExpr( *expr.children[0], ctx, ok, ExprPrecedence( expr ), false ) + " " +
								   OperatorText( expr.token_kind ) + " " +
								   TranslateExpr( *expr.children[1], ctx, ok, ExprPrecedence( expr ), true ),
							   ExprPrecedence( expr ) );
	case ChspAstExprKind::Group:
		if ( expr.children.size() != 1 || expr.children[0] == nullptr ) {
			ok = false;
			return "";
		}
		return "(" + TranslateExpr( *expr.children[0], ctx, ok, 0, false ) + ")";
	case ChspAstExprKind::Call:
		break;
	default:
		ok = false;
		return "";
	}

	if ( expr.children.empty() || expr.children[0] == nullptr ||
		 expr.children[0]->kind != ChspAstExprKind::Identifier ) {
		ok = false;
		return "";
	}

	const std::string name = NormalizeScopedName( expr.children[0]->text );
	const auto function_it = ctx.function_defs.find( name );
	const ChspAstFunction *callee = function_it != ctx.function_defs.end() ? function_it->second : nullptr;
	if ( ctx.array_names.find( name ) != ctx.array_names.end() ) {
		return TranslateArrayAccess( expr, ctx, ok );
	}

	if ( name == "length" || name == "length2" || name == "length3" || name == "length4" ) {
		if ( expr.children.size() != 2 || expr.children[1] == nullptr ||
			 expr.children[1]->kind != ChspAstExprKind::Identifier ) {
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
		if ( callee != nullptr && ctx.target == ChspNativeTarget::Plugin &&
			 ( i - 1 ) < callee->params.size() && callee->params[i - 1].is_array ) {
			const auto arg_norm = NormalizeScopedName( expr.children[i]->text );
			if ( ctx.argument_array_names.count( arg_norm ) ) {
				out += ", pval_" + LookupCppIdentifier( ctx, expr.children[i]->text );
			} else {
				out += ", NULL";
			}
		}
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

bool IsBlockOpeningStmt( const ChspAstStmt &stmt )
{
	if ( stmt.kind == ChspAstStmtKind::Repeat ) {
		return true;
	}
	if ( stmt.kind != ChspAstStmtKind::If && stmt.kind != ChspAstStmtKind::Else ) {
		return false;
	}
	return stmt.children.size() > 1;
}

bool HasTrailingBlockElseChild( const ChspAstStmt &stmt, size_t &else_index )
{
	if ( stmt.kind != ChspAstStmtKind::If ) {
		return false;
	}
	for ( size_t i = 0; i < stmt.children.size(); ++i ) {
		if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspAstStmtKind::Else ) {
			else_index = i;
			return true;
		}
	}
	return false;
}

std::string RenderStatementInline( const ChspAstStmt &stmt, TranslateContext &ctx, bool &ok );

const char *StatementKindName( ChspAstStmtKind kind )
{
	switch ( kind ) {
	case ChspAstStmtKind::Unknown:
		return "unknown";
	case ChspAstStmtKind::Return:
		return "return";
	case ChspAstStmtKind::Repeat:
		return "repeat";
	case ChspAstStmtKind::Loop:
		return "loop";
	case ChspAstStmtKind::If:
		return "if";
	case ChspAstStmtKind::Else:
		return "else";
	case ChspAstStmtKind::Assignment:
		return "assignment";
	case ChspAstStmtKind::Command:
		return "command";
	case ChspAstStmtKind::BlockMarker:
		return "block";
	default:
		return "statement";
	}
}

bool ReportUnsupportedStmt( CLogger &logger, const ChspAstFunction &func, const ChspAstStmt &stmt,
							const char *reason )
{
	logger.Mesf( "#Error:cHSP frontend v3 emitter does not support %s in function '%s' at line %d%s%s%s",
				 StatementKindName( stmt.kind ), NormalizeScopedName( func.name ).c_str(), stmt.line,
				 reason != nullptr ? " (" : "", reason != nullptr ? reason : "", reason != nullptr ? ")" : "" );
	return false;
}

std::string RenderCommandCall( const ChspAstStmt &stmt, TranslateContext &ctx, bool &ok )
{
	const std::string name = NormalizeScopedName( stmt.text );
	const auto function_it = ctx.function_defs.find( name );
	const ChspAstFunction *callee = function_it != ctx.function_defs.end() ? function_it->second : nullptr;
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
		if ( callee != nullptr && ctx.target == ChspNativeTarget::Plugin &&
			 i < callee->params.size() && callee->params[i].is_array ) {
			const auto arg_norm = NormalizeScopedName( stmt.exprs[i]->text );
			if ( ctx.argument_array_names.count( arg_norm ) ) {
				out += ", pval_" + LookupCppIdentifier( ctx, stmt.exprs[i]->text );
			} else {
				out += ", NULL";
			}
		}
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

std::string RenderStatementInline( const ChspAstStmt &stmt, TranslateContext &ctx, bool &ok )
{
	switch ( stmt.kind ) {
	case ChspAstStmtKind::Return:
		if ( stmt.rhs == nullptr ) {
			return "return;";
		}
		return "return " + TranslateExpr( *stmt.rhs, ctx, ok ) + ";";
	case ChspAstStmtKind::Assignment: {
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
	case ChspAstStmtKind::Command:
		if ( NormalizeScopedName( stmt.text ) == "break" ) {
			return "break;";
		}
		if ( NormalizeScopedName( stmt.text ) == "continue" ) {
			return "continue;";
		}
		return RenderCommandCall( stmt, ctx, ok ) + ";";
	case ChspAstStmtKind::If:
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
			 stmt.children[1]->kind == ChspAstStmtKind::Else ) {
			return "if (" + TranslateExpr( *stmt.exprs[0], ctx, ok ) + ") " +
				   RenderStatementInline( *stmt.children[0], ctx, ok ) + " " +
				   RenderStatementInline( *stmt.children[1], ctx, ok );
		}
		ok = false;
		return "";
	case ChspAstStmtKind::Else:
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

bool WriteFunctionStmtToCpp( CMemBuf &buf, const ChspAstStmt &stmt, TranslateContext &ctx,
							 const ChspAstFunction &func, CLogger &logger, int &indent_level,
							 bool &emitted_explicit_return )
{
	switch ( stmt.kind ) {
	case ChspAstStmtKind::Unknown:
		return true;
	case ChspAstStmtKind::BlockMarker:
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}" );
		buf.PutCR();
		return true;
	case ChspAstStmtKind::Loop:
		if ( !ctx.loop_stack.empty() ) {
			ctx.loop_stack.pop_back();
		}
		indent_level = std::max( 1, indent_level - 1 );
		buf.PutStr( MakeIndent( indent_level ).c_str() );
		buf.PutStr( "}\n" );
		return true;
	case ChspAstStmtKind::Repeat: {
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
	case ChspAstStmtKind::If: {
		bool ok = true;
		if ( stmt.exprs.size() != 1 || stmt.exprs[0] == nullptr ) {
			ok = false;
		}
		size_t else_index = stmt.children.size();
		for ( size_t i = 0; i < stmt.children.size(); ++i ) {
			if ( stmt.children[i] != nullptr && stmt.children[i]->kind == ChspAstStmtKind::Else ) {
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
			if ( ok && !rendered.empty() ) {
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( rendered.c_str() );
				buf.PutCR();
				if ( chsputil::StartsWith( chsputil::Trim( rendered ), "return" ) ) {
					emitted_explicit_return = true;
				}
				return true;
			}
			if ( stmt.children.size() == 1 && stmt.children[0] != nullptr ) {
				bool cond_ok = true;
				const auto cond = TranslateExpr( *stmt.exprs[0], ctx, cond_ok );
				if ( cond_ok ) {
					buf.PutStr( MakeIndent( indent_level ).c_str() );
					buf.PutStr( ( "if (" + cond + ") {\n" ).c_str() );
					++indent_level;
					if ( !WriteFunctionStmtToCpp( buf, *stmt.children[0], ctx, func, logger, indent_level,
												  emitted_explicit_return ) ) {
						return false;
					}
					indent_level = std::max( 1, indent_level - 1 );
					buf.PutStr( MakeIndent( indent_level ).c_str() );
					buf.PutStr( "}\n" );
					return true;
				}
			}
		}
		return ReportUnsupportedStmt( logger, func, stmt, "conditional structure could not be translated" );
	}
	case ChspAstStmtKind::Else: {
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
	case ChspAstStmtKind::Assignment: {
		// Consecutive assignment: a = v1, v2, v3  or  a(n) = v1, v2, v3
		// exprs holds the extra values (v2, v3, ...) beyond the first (rhs = v1).
		if ( stmt.token_kind == '=' && !stmt.exprs.empty() && stmt.lhs != nullptr && stmt.rhs != nullptr ) {
			bool ok = true;
			std::string base_ptr;
			std::string start_offset; // empty → index 0

			if ( stmt.lhs->kind == ChspAstExprKind::Identifier &&
				 ctx.array_names.count( NormalizeScopedName( stmt.lhs->text ) ) ) {
				// a = v1, v2, v3
				base_ptr = LookupCppIdentifier( ctx, NormalizeScopedName( stmt.lhs->text ) );
			} else if ( stmt.lhs->kind == ChspAstExprKind::Call && stmt.lhs->children.size() == 2 &&
						stmt.lhs->children[0] != nullptr &&
						stmt.lhs->children[0]->kind == ChspAstExprKind::Identifier &&
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
	case ChspAstStmtKind::Command: {
		const auto cmd_name = NormalizeScopedName( stmt.text );
		if ( cmd_name == "dim" || cmd_name == "dimtype" ) {
			if ( ctx.target != ChspNativeTarget::Plugin ) {
				return ReportUnsupportedStmt( logger, func, stmt,
											  ( cmd_name + " is only supported in target=plugin" ).c_str() );
			}
			if ( stmt.exprs.empty() || stmt.exprs[0] == nullptr ||
				 stmt.exprs[0]->kind != ChspAstExprKind::Identifier ) {
				return ReportUnsupportedStmt( logger, func, stmt,
											  ( cmd_name + " requires a variable identifier" ).c_str() );
			}
			const std::string var_name = NormalizeScopedName( stmt.exprs[0]->text );
			const ChspAstParam *target_param = nullptr;
			for ( const auto &param : func.params ) {
				if ( NormalizeScopedName( param.name ) == var_name ) {
					target_param = &param;
					break;
				}
			}
			if ( target_param == nullptr ) {
				return ReportUnsupportedStmt(
					logger, func, stmt,
					( "variable '" + var_name + "' not found in function parameters" ).c_str() );
			}
			if ( target_param->is_local ) {
				return ReportUnsupportedStmt(
					logger, func, stmt, ( cmd_name + " cannot be used on local array '" + var_name + "'" ).c_str() );
			}
			if ( !target_param->is_array ) {
				return ReportUnsupportedStmt(
					logger, func, stmt,
					( cmd_name + " cannot be used on non-array variable '" + var_name + "'" ).c_str() );
			}

			int flag = 0;
			std::string ptr_func;
			size_t dim_start = 1;

			if ( cmd_name == "dim" ) {
				if ( target_param->base_type != "int" ) {
					return ReportUnsupportedStmt( logger, func, stmt,
												  ( "type mismatch: dim requires array[int], but '" + var_name +
													"' is array[" + target_param->base_type + "]" )
													  .c_str() );
				}
				flag = 4; // HSPVAR_FLAG_INT
				ptr_func = "chsp_plugin_int_ptr";
				dim_start = 1;
			} else { // dimtype
				if ( stmt.exprs.size() < 2 || stmt.exprs[1] == nullptr ) {
					return ReportUnsupportedStmt( logger, func, stmt,
												  "dimtype requires type parameter" );
				}
				if ( stmt.exprs[1]->kind != ChspAstExprKind::IntLiteral ) {
					return ReportUnsupportedStmt(
						logger, func, stmt,
						"dimtype type parameter must be a constant integer (dynamic typing is not permitted)" );
				}
				const std::string type_str = stmt.exprs[1]->text;
				if ( type_str == "4" ) {
					if ( target_param->base_type != "int" ) {
						return ReportUnsupportedStmt( logger, func, stmt,
													  ( "type mismatch: dimtype for type 4 requires array[int], but '" +
														var_name + "' is array[" + target_param->base_type + "]" )
														  .c_str() );
					}
					flag = 4;
					ptr_func = "chsp_plugin_int_ptr";
				} else if ( type_str == "3" ) {
					if ( target_param->base_type != "double" ) {
						return ReportUnsupportedStmt(
							logger, func, stmt,
							( "type mismatch: ddim/dimtype for type 3 requires array[double], but '" + var_name +
							  "' is array[" + target_param->base_type + "]" )
								.c_str() );
					}
					flag = 3;
					ptr_func = "chsp_plugin_double_ptr";
				} else if ( type_str == "8" ) {
					if ( target_param->base_type != "int64" ) {
						return ReportUnsupportedStmt(
							logger, func, stmt,
							( "type mismatch: lldim/dimtype for type 8 requires array[int64], but '" + var_name +
							  "' is array[" + target_param->base_type + "]" )
								.c_str() );
					}
					flag = 8;
					ptr_func = "chsp_plugin_int64_ptr";
				} else {
					return ReportUnsupportedStmt(
						logger, func, stmt,
						( "unsupported vartype " + type_str + " in dimtype" ).c_str() );
				}
				dim_start = 2;
			}

			// Dimensions
			std::string d_expr[4] = { "0", "0", "0", "0" };
			if ( dim_start >= stmt.exprs.size() ) {
				d_expr[0] = "1";
			} else {
				for ( size_t d = 0; d < 4; ++d ) {
					if ( dim_start + d < stmt.exprs.size() && stmt.exprs[dim_start + d] != nullptr ) {
						bool ok = true;
						d_expr[d] = TranslateExpr( *stmt.exprs[dim_start + d], ctx, ok );
						if ( !ok ) {
							return ReportUnsupportedStmt( logger, func, stmt,
														  "dimension expression could not be translated" );
						}
					}
				}
			}

			std::string d_vars[4];
			for ( int d = 0; d < 4; ++d ) {
				d_vars[d] = "_chsp_dim_" + std::to_string( ctx.temp_var_count++ );
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( ( "int " + d_vars[d] + " = " + d_expr[d] + ";\n" ).c_str() );
			}

			const std::string cpp_arr = LookupCppIdentifier( ctx, var_name );
			const std::string pval_name = "pval_" + cpp_arr;
			const auto dim_exprs = ArrayDimensionExprsForName( ctx, var_name );

			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( ( "if ( " + pval_name + " == NULL ) puterror( HSPERR_ILLEGAL_FUNCTION );\n" ).c_str() );

			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( ( "exinfo->HspFunc_dim( " + pval_name + ", " + std::to_string( flag ) + ", 0, " +
						  d_vars[0] + ", " + d_vars[1] + ", " + d_vars[2] + ", " + d_vars[3] + " );\n" )
							.c_str() );

			buf.PutStr( MakeIndent( indent_level ).c_str() );
			buf.PutStr( ( cpp_arr + " = " + ptr_func + "( " + pval_name + ", 0 );\n" ).c_str() );

			for ( int d = 0; d < 4 && d < static_cast<int>( dim_exprs.size() ); ++d ) {
				buf.PutStr( MakeIndent( indent_level ).c_str() );
				buf.PutStr( ( dim_exprs[d] + " = " + pval_name + "->len[" + std::to_string( d + 1 ) + "];\n" ).c_str() );
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

void WriteFunctionDeclToHsp( CMemBuf &buf, const ChspAstFunction &func, const std::string &cpp_name )
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


void WriteLocalDeclsToNative( CMemBuf &buf, const ChspAstFunction &func, const TranslateContext &ctx )
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

bool WriteFunctionBodyToNative( CMemBuf &buf, const ChspAstFunction &func, TranslateContext &ctx, CLogger &logger )
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



} // namespace

void WriteFunctionPrototypeToNative( CMemBuf &buf, const ChspAstFunction &func, const ChspAstModule &module,
									 const std::unordered_map<std::string, std::string> &function_cpp_names,
									 const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
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
		if ( target == ChspNativeTarget::Plugin && param.is_array && !param.is_local ) {
			buf.PutStr( ", PVal *pval_" );
			buf.PutStr( LookupCppIdentifier( ctx, param.name ).c_str() );
		}
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


bool WriteFunctionToNative( CMemBuf &buf, const ChspAstModule &module, const ChspAstFunction &func,
							const std::unordered_map<std::string, std::string> &function_cpp_names,
							const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
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
		if ( target == ChspNativeTarget::Plugin && param.is_array && !param.is_local ) {
			buf.PutStr( ", PVal *pval_" );
			buf.PutStr( LookupCppIdentifier( ctx, param.name ).c_str() );
		}
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


bool WritePluginNativeDispatch( CMemBuf &buf, const ChspAstModule &module,
								const std::unordered_map<std::string, std::string> &function_cpp_names,
								const std::unordered_map<std::string, const ChspAstFunction *> &function_defs,
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
	buf.PutStr( "static double chsp_plugin_ref_double;\n" );
	buf.PutStr( "static int64_t chsp_plugin_ref_int64;\n\n" );

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
				buf.PutStr( param.base_type == "int64" ? "chsp_plugin_int64_ptr"
							: ( param.base_type == "double" ? "chsp_plugin_double_ptr" : "chsp_plugin_int_ptr" ) );
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
				buf.PutStr( param.base_type == "int64" ? "code_getdl(0)"
							: ( param.base_type == "double" ? "exinfo->HspFunc_prm_getdd(0.0)" : "code_getdi(0)" ) );
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
			if ( param.is_array ) {
				buf.PutStr( ", pval_" );
				buf.PutStr( var_name.c_str() );
			}
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
				buf.PutStr( param.base_type == "int64" ? "chsp_plugin_int64_ptr"
							: ( param.base_type == "double" ? "chsp_plugin_double_ptr" : "chsp_plugin_int_ptr" ) );
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
				buf.PutStr( param.base_type == "int64" ? "code_getl()"
							: ( param.base_type == "double" ? "exinfo->HspFunc_prm_getd()" : "code_geti()" ) );
				buf.PutStr( ";\n" );
			}
		}
		if ( func.return_type == "double" ) {
			buf.PutStr( "        chsp_plugin_ref_double = " );
		} else if ( func.return_type == "int64" ) {
			buf.PutStr( "        chsp_plugin_ref_int64 = " );
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
			if ( param.is_array ) {
				buf.PutStr( ", pval_" );
				buf.PutStr( var_name.c_str() );
			}
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
		} else if ( func.return_type == "int64" ) {
			buf.PutStr( "        *type_res = HSPVAR_FLAG_INT64;\n" );
			buf.PutStr( "        return &chsp_plugin_ref_int64;\n" );
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
						   "*)pval->pt) + aptr; }\n" );
		native_out.PutStr( "static int64_t *chsp_plugin_int64_ptr( PVal *pval, APTR aptr ) { return ((int64_t "
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

void WriteNativeSourceBlocks( CMemBuf &native_out, const ChspAstModule &module )
{
	for ( const auto &block : module.native_source_blocks ) {
		native_out.PutStr( block.c_str() );
		if ( block.empty() || block.back() != '\n' ) {
			native_out.PutCR();
		}
		native_out.PutCR();
	}
}



} // namespace chsp
