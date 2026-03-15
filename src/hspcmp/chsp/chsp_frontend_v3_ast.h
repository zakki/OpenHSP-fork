//
//	cHSP v3 codegen-aligned AST containers
//
#pragma once

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "chsp_frontend_v2.h"

namespace chspv3
{

enum class ChspV3SourceDirectiveKind
{
	None,
	Module,
	ModuleEnd,
	DefFunc,
	DefCFunc,
	End,
};

struct ChspV3SourceLine
{
	int line = 0;
	ChspV3SourceDirectiveKind directive = ChspV3SourceDirectiveKind::None;
	std::string text;
};

enum class ChspV3AstExprKind
{
	Unknown,
	IntLiteral,
	DoubleLiteral,
	StringLiteral,
	Identifier,
	Label,
	Unary,
	Binary,
	Call,
	Group,
};

enum class ChspV3AstStmtKind
{
	Unknown,
	Return,
	Repeat,
	Loop,
	If,
	Else,
	Assignment,
	Command,
	BlockMarker,
};

inline void AppendJsonEscaped( std::string &out, const std::string &text )
{
	out.push_back( '"' );
	for ( unsigned char ch : text ) {
		switch ( ch ) {
		case '\\':
			out += "\\\\";
			break;
		case '"':
			out += "\\\"";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if ( ch < 0x20 ) {
				char buf[7];
				std::snprintf( buf, sizeof( buf ), "\\u%04x", static_cast<unsigned int>( ch ) );
				out += buf;
			} else {
				out.push_back( static_cast<char>( ch ) );
			}
			break;
		}
	}
	out.push_back( '"' );
}

struct ChspV3AstExpr
{
	ChspV3AstExprKind kind = ChspV3AstExprKind::Unknown;
	int line = 0;
	int token_kind = 0;
	int operator_kind = 0;
	std::string text;
	std::vector<std::unique_ptr<ChspV3AstExpr>> children;
};

struct ChspV3AstStmt
{
	ChspV3AstStmtKind kind = ChspV3AstStmtKind::Unknown;
	int line = 0;
	int token_kind = 0;
	int statement_kind = 0;
	int if_depth = 0;
	int repeat_depth = 0;
	std::string text;
	std::unique_ptr<ChspV3AstExpr> lhs;
	std::unique_ptr<ChspV3AstExpr> rhs;
	std::vector<std::unique_ptr<ChspV3AstExpr>> exprs;
	std::vector<std::unique_ptr<ChspV3AstStmt>> children;
};

struct ChspV3AstParam
{
	int line = 0;
	int token_kind = 0;
	std::string type_name;
	std::string base_type;
	std::string name;
	bool is_array = false;
	bool is_local = false;
	std::vector<int> array_dims;
};

struct ChspV3AstFunction
{
	int line = 0;
	int token_kind = 0;
	std::string return_type;
	std::string name;
	std::vector<ChspV3AstParam> params;
	std::vector<std::unique_ptr<ChspV3AstStmt>> body_stmts;
};

struct ChspV3AstModule
{
	int line = 0;
	int token_kind = 0;
	std::string name;
	ChspNativeTarget target = ChspNativeTarget::Plugin;
	std::vector<ChspV3AstFunction> functions;
};

struct ChspV3AstProgram
{
	std::vector<ChspV3AstModule> modules;
	std::vector<std::unique_ptr<ChspV3AstStmt>> top_level_stmts;
	std::vector<ChspV3SourceLine> source_lines;
};

inline void AppendJsonExpr( std::string &out, const ChspV3AstExpr &expr );
inline void AppendJsonStmt( std::string &out, const ChspV3AstStmt &stmt );

inline void AppendJsonExprArray( std::string &out, const std::vector<std::unique_ptr<ChspV3AstExpr>> &exprs )
{
	out.push_back( '[' );
	for ( size_t i = 0; i < exprs.size(); ++i ) {
		if ( i != 0 ) {
			out.push_back( ',' );
		}
		if ( exprs[i] != nullptr ) {
			AppendJsonExpr( out, *exprs[i] );
		} else {
			out += "null";
		}
	}
	out.push_back( ']' );
}

inline void AppendJsonStmtArray( std::string &out, const std::vector<std::unique_ptr<ChspV3AstStmt>> &stmts )
{
	out.push_back( '[' );
	for ( size_t i = 0; i < stmts.size(); ++i ) {
		if ( i != 0 ) {
			out.push_back( ',' );
		}
		if ( stmts[i] != nullptr ) {
			AppendJsonStmt( out, *stmts[i] );
		} else {
			out += "null";
		}
	}
	out.push_back( ']' );
}

inline void AppendJsonExpr( std::string &out, const ChspV3AstExpr &expr )
{
	out += "{\"kind\":";
	switch ( expr.kind ) {
	case ChspV3AstExprKind::IntLiteral:
		AppendJsonEscaped( out, "int_literal" );
		break;
	case ChspV3AstExprKind::DoubleLiteral:
		AppendJsonEscaped( out, "double_literal" );
		break;
	case ChspV3AstExprKind::StringLiteral:
		AppendJsonEscaped( out, "string_literal" );
		break;
	case ChspV3AstExprKind::Identifier:
		AppendJsonEscaped( out, "identifier" );
		break;
	case ChspV3AstExprKind::Label:
		AppendJsonEscaped( out, "label" );
		break;
	case ChspV3AstExprKind::Unary:
		AppendJsonEscaped( out, "unary" );
		break;
	case ChspV3AstExprKind::Binary:
		AppendJsonEscaped( out, "binary" );
		break;
	case ChspV3AstExprKind::Call:
		AppendJsonEscaped( out, "call" );
		break;
	case ChspV3AstExprKind::Group:
		AppendJsonEscaped( out, "group" );
		break;
	default:
		AppendJsonEscaped( out, "unknown" );
		break;
	}
	out += ",\"line\":";
	out += std::to_string( expr.line );
	out += ",\"token_kind\":";
	out += std::to_string( expr.token_kind );
	out += ",\"operator_kind\":";
	out += std::to_string( expr.operator_kind );
	out += ",\"text\":";
	AppendJsonEscaped( out, expr.text );
	out += ",\"children\":";
	AppendJsonExprArray( out, expr.children );
	out.push_back( '}' );
}

inline void AppendJsonStmt( std::string &out, const ChspV3AstStmt &stmt )
{
	out += "{\"kind\":";
	switch ( stmt.kind ) {
	case ChspV3AstStmtKind::Return:
		AppendJsonEscaped( out, "return" );
		break;
	case ChspV3AstStmtKind::Repeat:
		AppendJsonEscaped( out, "repeat" );
		break;
	case ChspV3AstStmtKind::Loop:
		AppendJsonEscaped( out, "loop" );
		break;
	case ChspV3AstStmtKind::If:
		AppendJsonEscaped( out, "if" );
		break;
	case ChspV3AstStmtKind::Else:
		AppendJsonEscaped( out, "else" );
		break;
	case ChspV3AstStmtKind::Assignment:
		AppendJsonEscaped( out, "assignment" );
		break;
	case ChspV3AstStmtKind::Command:
		AppendJsonEscaped( out, "command" );
		break;
	case ChspV3AstStmtKind::BlockMarker:
		AppendJsonEscaped( out, "block_marker" );
		break;
	default:
		AppendJsonEscaped( out, "unknown" );
		break;
	}
	out += ",\"line\":";
	out += std::to_string( stmt.line );
	out += ",\"token_kind\":";
	out += std::to_string( stmt.token_kind );
	out += ",\"statement_kind\":";
	out += std::to_string( stmt.statement_kind );
	out += ",\"if_depth\":";
	out += std::to_string( stmt.if_depth );
	out += ",\"repeat_depth\":";
	out += std::to_string( stmt.repeat_depth );
	out += ",\"text\":";
	AppendJsonEscaped( out, stmt.text );
	out += ",\"lhs\":";
	if ( stmt.lhs != nullptr ) {
		AppendJsonExpr( out, *stmt.lhs );
	} else {
		out += "null";
	}
	out += ",\"rhs\":";
	if ( stmt.rhs != nullptr ) {
		AppendJsonExpr( out, *stmt.rhs );
	} else {
		out += "null";
	}
	out += ",\"exprs\":";
	AppendJsonExprArray( out, stmt.exprs );
	out += ",\"children\":";
	AppendJsonStmtArray( out, stmt.children );
	out.push_back( '}' );
}

inline std::string SerializeAstProgramJson( const ChspV3AstProgram &program )
{
	std::string out;
	out += "{\"modules\":[";
	for ( size_t i = 0; i < program.modules.size(); ++i ) {
		if ( i != 0 ) {
			out.push_back( ',' );
		}
		const auto &module = program.modules[i];
		out += "{\"line\":";
		out += std::to_string( module.line );
		out += ",\"token_kind\":";
		out += std::to_string( module.token_kind );
		out += ",\"name\":";
		AppendJsonEscaped( out, module.name );
		out += ",\"target\":";
		AppendJsonEscaped( out, module.target == ChspNativeTarget::C ? "c" : "plugin" );
		out += ",\"functions\":[";
		for ( size_t j = 0; j < module.functions.size(); ++j ) {
			if ( j != 0 ) {
				out.push_back( ',' );
			}
			const auto &function = module.functions[j];
			out += "{\"line\":";
			out += std::to_string( function.line );
			out += ",\"token_kind\":";
			out += std::to_string( function.token_kind );
			out += ",\"return_type\":";
			AppendJsonEscaped( out, function.return_type );
			out += ",\"name\":";
			AppendJsonEscaped( out, function.name );
			out += ",\"params\":[";
			for ( size_t k = 0; k < function.params.size(); ++k ) {
				if ( k != 0 ) {
					out.push_back( ',' );
				}
				const auto &param = function.params[k];
				out += "{\"line\":";
				out += std::to_string( param.line );
				out += ",\"token_kind\":";
				out += std::to_string( param.token_kind );
				out += ",\"type_name\":";
				AppendJsonEscaped( out, param.type_name );
				out += ",\"base_type\":";
				AppendJsonEscaped( out, param.base_type );
				out += ",\"is_array\":";
				out += param.is_array ? "true" : "false";
				out += ",\"is_local\":";
				out += param.is_local ? "true" : "false";
				out += ",\"array_dims\":[";
				for ( size_t m = 0; m < param.array_dims.size(); ++m ) {
					if ( m != 0 ) {
						out.push_back( ',' );
					}
					out += std::to_string( param.array_dims[m] );
				}
				out.push_back( ']' );
				out += ",\"name\":";
				AppendJsonEscaped( out, param.name );
				out.push_back( '}' );
			}
			out += "],\"body_stmts\":";
			AppendJsonStmtArray( out, function.body_stmts );
			out.push_back( '}' );
		}
		out += "]}";
	}
	out += "],\"top_level_stmts\":";
	AppendJsonStmtArray( out, program.top_level_stmts );
	out.push_back( '}' );
	return out;
}

} // namespace chspv3
