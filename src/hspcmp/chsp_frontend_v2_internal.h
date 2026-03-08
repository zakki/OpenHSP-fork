#pragma once

#include <memory>
#include <string>
#include <vector>

class CLogger;
class CMemBuf;

namespace chspv2
{

enum class ChspDirectiveKind
{
	None,
	Module,
	ModuleEnd,
	DefFunc,
	DefCFunc,
	End,
};

enum class ChspFuncKind
{
	DefFunc,
	DefCFunc,
};

struct ChspLexedToken
{
	int line = 0;
	int kind = 0;
	int value = 0;
	double value_double = 0.0;
	std::string text;
};

struct ChspSourceLine
{
	int line = 0;
	ChspDirectiveKind directive = ChspDirectiveKind::None;
	std::string text;
	std::vector<ChspLexedToken> tokens;
};

struct ChspParam
{
	std::string original_type;
	std::string base_type;
	std::string name;
	bool is_array = false;
	bool is_local = false;
	int array_length = 0;
};

struct ChspExpr;
struct ChspStmt;

struct ChspFunction
{
	ChspFuncKind kind = ChspFuncKind::DefFunc;
	std::string return_type = "void";
	std::string name;
	int line = 0;
	std::vector<ChspParam> params;
	std::vector<ChspSourceLine> body_lines;
	std::vector<std::unique_ptr<ChspStmt>> body_stmts;
};

struct ChspModule
{
	std::string name;
	int line = 0;
	std::vector<ChspFunction> functions;
};

struct ChspProgram
{
	std::vector<ChspSourceLine> passthrough_lines;
	std::vector<ChspModule> modules;
};

enum class ChspExprKind
{
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

struct ChspExpr
{
	ChspExprKind kind = ChspExprKind::Identifier;
	int line = 0;
	int token_kind = 0;
	int int_value = 0;
	double double_value = 0.0;
	std::string text;
	std::vector<std::unique_ptr<ChspExpr>> children;
};

enum class ChspStmtKind
{
	Empty,
	Return,
	Repeat,
	Loop,
	If,
	Else,
	Assignment,
	Command,
	BlockMarker,
};

struct ChspStmt
{
	ChspStmtKind kind = ChspStmtKind::Empty;
	int line = 0;
	int token_kind = 0;
	std::string text;
	std::unique_ptr<ChspExpr> lhs;
	std::unique_ptr<ChspExpr> rhs;
	std::vector<std::unique_ptr<ChspExpr>> exprs;
	std::vector<std::unique_ptr<ChspStmt>> children;
};

inline std::string Trim( const std::string &src )
{
	const auto begin = src.find_first_not_of( " \t\r\n" );
	if ( begin == std::string::npos ) {
		return "";
	}
	const auto end = src.find_last_not_of( " \t\r\n" );
	return src.substr( begin, end - begin + 1 );
}

inline bool StartsWith( const std::string &src, const char *prefix )
{
	return src.rfind( prefix, 0 ) == 0;
}

std::vector<ChspSourceLine> BuildSourceIndex( const char *input_text, const std::shared_ptr<CLogger> &logger );
bool ParseProgram( const std::vector<ChspSourceLine> &lines, ChspProgram &program, CLogger &logger, const char *source_name );
int GenerateProgramOutput( const std::vector<ChspSourceLine> &lines, const ChspProgram &program, CLogger &logger, CMemBuf &hsp_out,
						   CMemBuf &cpp_out, const char *source_name );

} // namespace chspv2
