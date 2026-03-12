#include "chsp_frontend_v2_internal.h"

#include <cstring>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../../hsp3/hsp3config.h"

#include "codegen_lexer.h"
#include "logger.h"

namespace chspv2
{
namespace
{

std::vector<std::string> SplitLines( const std::string &src )
{
	std::vector<std::string> lines;
	std::stringstream ss( src );
	std::string line;
	while ( std::getline( ss, line ) ) {
		if ( !line.empty() && line.back() == '\r' ) {
			line.pop_back();
		}
		lines.push_back( line );
	}
	if ( !src.empty() && src.back() == '\n' && ( lines.empty() || !lines.back().empty() ) ) {
		lines.emplace_back();
	}
	return lines;
}

struct ChspInputLine
{
	int line = 0;
	std::string text;
};

std::vector<ChspInputLine> CollapseContinuationLines( const std::vector<std::string> &lines )
{
	std::vector<ChspInputLine> out;
	std::string current;
	int current_line = 0;
	for ( size_t i = 0; i < lines.size(); ++i ) {
		std::string piece = lines[i];
		bool continued = false;
		auto trimmed = Trim( piece );
		if ( !trimmed.empty() && trimmed.back() == '\\' ) {
			continued = true;
			const auto slash_pos = piece.find_last_of( '\\' );
			piece.erase( slash_pos );
		}
		if ( current.empty() ) {
			current = piece;
			current_line = static_cast<int>( i ) + 1;
		} else {
			current += " ";
			current += Trim( piece );
		}
		if ( continued ) {
			continue;
		}
		out.push_back( { current_line, current } );
		current.clear();
		current_line = 0;
	}
	if ( !current.empty() ) {
		out.push_back( { current_line, current } );
	}
	return out;
}

ChspDirectiveKind DetectDirective( const std::string &line )
{
	const auto trimmed = NormalizeIdentifier( Trim( line ) );
	if ( StartsWith( trimmed, "#chsp_module_end" ) ) {
		return ChspDirectiveKind::ModuleEnd;
	}
	if ( StartsWith( trimmed, "#chsp_module" ) ) {
		return ChspDirectiveKind::Module;
	}
	if ( StartsWith( trimmed, "#chsp_defcfunc" ) ) {
		return ChspDirectiveKind::DefCFunc;
	}
	if ( StartsWith( trimmed, "#chsp_deffunc" ) ) {
		return ChspDirectiveKind::DefFunc;
	}
	if ( trimmed == "#chsp_end" ) {
		return ChspDirectiveKind::End;
	}
	return ChspDirectiveKind::None;
}

class ChspTokenCollector
{
public:
	explicit ChspTokenCollector( const std::shared_ptr<CLogger> &logger )
		: opts( std::make_shared<CompileOptions>() ), lexer( opts, logger ), logger( logger )
	{
	}

	std::vector<ChspLexedToken> TokenizeLine( const std::string &line, int line_no )
	{
		std::vector<ChspLexedToken> tokens;
		std::string scratch = line;
		const char *cursor = scratch.c_str();
		try {
			while ( cursor != nullptr ) {
				auto result = lexer.GetTokenCG( cursor, GETTOKEN_DEFAULT );
				auto &next = result.first;
				auto &token = result.second;
				if ( token.ttype == TK_EOL || token.ttype == TK_EOF ) {
					break;
				}
				ChspLexedToken out;
				out.line = line_no;
				out.kind = token.ttype == TK_NONE ? token.val : token.ttype;
				out.value = token.val;
				out.value_double = token.val_d;
				out.text = token.cg_str;
				if ( out.text.empty() && next != nullptr && next > cursor &&
					 ( token.ttype == TK_NUM || token.ttype == TK_DNUM ) ) {
					out.text = Trim( std::string( cursor, next - cursor ) );
				}
				tokens.push_back( std::move( out ) );
				cursor = next;
			}
		} catch ( ... ) {
			if ( logger != nullptr ) {
				logger->Mesf( "#Error:Failed to tokenize cHSP line %d [%s]", line_no, line.c_str() );
			}
			throw;
		}
		return tokens;
	}

private:
	std::shared_ptr<CompileOptions> opts;
	CCgLexer lexer;
	std::shared_ptr<CLogger> logger;
};

class ChspTokenCursor
{
public:
	explicit ChspTokenCursor( const std::vector<ChspLexedToken> &tokens_ ) : tokens( tokens_ )
	{
	}

	const ChspLexedToken *Peek( int offset = 0 ) const
	{
		const size_t index = pos + static_cast<size_t>( offset );
		if ( index >= tokens.size() ) {
			return nullptr;
		}
		return &tokens[index];
	}

	const ChspLexedToken *Consume()
	{
		const auto *token = Peek();
		if ( token != nullptr ) {
			++pos;
		}
		return token;
	}

	bool End() const
	{
		return pos >= tokens.size();
	}

	size_t Position() const
	{
		return pos;
	}

	void SetPosition( size_t next_pos )
	{
		pos = next_pos;
	}

private:
	const std::vector<ChspLexedToken> &tokens;
	size_t pos = 0;
};

std::string BaseScopedName( const std::string &name )
{
	const auto at = name.find( '@' );
	if ( at == std::string::npos ) {
		return NormalizeIdentifier( name );
	}
	return NormalizeIdentifier( name.substr( 0, at ) );
}

bool ConsumeChar( ChspTokenCursor &cursor, int ch )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr || token->kind != ch ) {
		return false;
	}
	cursor.Consume();
	return true;
}

bool ConsumeDirectivePrefix( ChspTokenCursor &cursor, const char *directive_name )
{
	if ( !ConsumeChar( cursor, '#' ) ) {
		return false;
	}
	const auto *token = cursor.Peek();
	if ( token == nullptr || token->kind != TK_OBJ || token->text != directive_name ) {
		return false;
	}
	cursor.Consume();
	return true;
}

bool ConsumeObject( ChspTokenCursor &cursor, std::string &out )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr || token->kind != TK_OBJ ) {
		return false;
	}
	out = NormalizeIdentifier( token->text );
	cursor.Consume();
	return true;
}

bool ConsumeScopedObject( ChspTokenCursor &cursor, std::string &out )
{
	if ( !ConsumeObject( cursor, out ) ) {
		return false;
	}
	out = BaseScopedName( out );
	const auto *at = cursor.Peek();
	if ( at != nullptr && at->kind == '@' ) {
		const auto *scope = cursor.Peek( 1 );
		if ( scope != nullptr && scope->kind == TK_OBJ ) {
			cursor.Consume();
			cursor.Consume();
		}
	}
	return true;
}

bool ConsumeKeyword( ChspTokenCursor &cursor, const char *keyword )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr || token->kind != TK_OBJ || BaseScopedName( token->text ) != keyword ) {
		return false;
	}
	cursor.Consume();
	const auto *at = cursor.Peek();
	if ( at != nullptr && at->kind == '@' ) {
		const auto *scope = cursor.Peek( 1 );
		if ( scope != nullptr && scope->kind == TK_OBJ ) {
			cursor.Consume();
			cursor.Consume();
		}
	}
	return true;
}

bool ConsumeString( ChspTokenCursor &cursor, std::string &out )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr || token->kind != TK_STRING ) {
		return false;
	}
	out = token->text;
	cursor.Consume();
	return true;
}

bool ParseTypeSpec( ChspTokenCursor &cursor, ChspParam &param )
{
	std::string type_name;
	if ( !ConsumeObject( cursor, type_name ) ) {
		return false;
	}
	param.original_type = type_name;
	param.base_type = type_name;

	if ( type_name == "array" || type_name == "local" ) {
		if ( !ConsumeChar( cursor, '[' ) ) {
			return false;
		}
		std::string base_type;
		if ( !ConsumeObject( cursor, base_type ) ) {
			return false;
		}
		param.original_type += "[";
		param.original_type += base_type;
		param.base_type = base_type;
		if ( type_name == "local" ) {
			param.is_local = true;
		} else {
			param.is_array = true;
		}

		if ( ConsumeChar( cursor, '[' ) ) {
			const auto *size_token = cursor.Peek();
			if ( size_token == nullptr || size_token->kind != TK_NUM ) {
				return false;
			}
			param.is_array = true;
			param.array_length = size_token->value;
			param.original_type += "[";
			param.original_type += std::to_string( size_token->value );
			param.original_type += "]";
			cursor.Consume();
			if ( !ConsumeChar( cursor, ']' ) ) {
				return false;
			}
		}

		if ( !ConsumeChar( cursor, ']' ) ) {
			return false;
		}
		param.original_type += "]";
	}

	return true;
}

bool ParseFunctionSignature( const ChspSourceLine &line, ChspFunction &func )
{
	ChspTokenCursor cursor( line.tokens );
	switch ( line.directive ) {
	case ChspDirectiveKind::DefFunc:
		func.kind = ChspFuncKind::DefFunc;
		func.return_type = "void";
		if ( !ConsumeDirectivePrefix( cursor, "chsp_deffunc" ) ) {
			return false;
		}
		break;
	case ChspDirectiveKind::DefCFunc:
		func.kind = ChspFuncKind::DefCFunc;
		if ( !ConsumeDirectivePrefix( cursor, "chsp_defcfunc" ) ) {
			return false;
		}
		if ( !ConsumeObject( cursor, func.return_type ) ) {
			return false;
		}
		break;
	default:
		return false;
	}

	func.line = line.line;
	if ( !ConsumeScopedObject( cursor, func.name ) ) {
		return false;
	}

	while ( !cursor.End() ) {
		ChspParam param;
		if ( !ParseTypeSpec( cursor, param ) ) {
			return false;
		}
		if ( !ConsumeScopedObject( cursor, param.name ) ) {
			return false;
		}
		func.params.push_back( std::move( param ) );
		if ( cursor.End() ) {
			break;
		}
		if ( !ConsumeChar( cursor, ',' ) ) {
			return false;
		}
	}

	return true;
}

bool ParseModuleHeader( const ChspSourceLine &line, ChspModule &module )
{
	ChspTokenCursor cursor( line.tokens );
	if ( !ConsumeDirectivePrefix( cursor, "chsp_module" ) ) {
		return false;
	}
	if ( !cursor.End() ) {
		size_t save_pos = cursor.Position();
		if ( !ConsumeString( cursor, module.name ) ) {
			cursor.SetPosition( save_pos );
		}
	}
	while ( !cursor.End() ) {
		std::string key;
		std::string value;
		if ( !ConsumeObject( cursor, key ) ) {
			return false;
		}
		if ( !ConsumeChar( cursor, '=' ) ) {
			return false;
		}
		if ( !ConsumeObject( cursor, value ) ) {
			return false;
		}
		if ( key == "target" ) {
			if ( value == "plugin" ) {
				module.target = ChspNativeTarget::Plugin;
			} else if ( value == "c" ) {
				module.target = ChspNativeTarget::C;
			} else {
				return false;
			}
			continue;
		}
		return false;
	}
	module.line = line.line;
	return true;
}

bool TokenMatches( const ChspLexedToken *token, int value )
{
	if ( token == nullptr ) {
		return false;
	}
	if ( token->kind == value ) {
		return true;
	}
	if ( token->kind == TK_SEPARATE && token->text.size() == 1 && token->text[0] == value ) {
		return true;
	}
	return false;
}

bool ConsumeToken( ChspTokenCursor &cursor, int value )
{
	if ( !TokenMatches( cursor.Peek(), value ) ) {
		return false;
	}
	cursor.Consume();
	return true;
}

bool IsStatementDelimiter( const ChspLexedToken *token )
{
	return token == nullptr || TokenMatches( token, ':' );
}

std::unique_ptr<ChspExpr> MakeExprNode( ChspExprKind kind, const ChspLexedToken *token )
{
	auto expr = std::make_unique<ChspExpr>();
	expr->kind = kind;
	if ( token != nullptr ) {
		expr->line = token->line;
		expr->token_kind = token->kind;
		expr->text = token->text;
	}
	return expr;
}

std::unique_ptr<ChspStmt> MakeStmtNode( ChspStmtKind kind, int line )
{
	auto stmt = std::make_unique<ChspStmt>();
	stmt->kind = kind;
	stmt->line = line;
	return stmt;
}

std::unique_ptr<ChspExpr> CalcCG_start( ChspTokenCursor &cursor );
std::unique_ptr<ChspExpr> CalcCG_factor( ChspTokenCursor &cursor );
std::unique_ptr<ChspStmt> ParseElseStatement( ChspTokenCursor &cursor, const ChspLexedToken *else_token );

std::unique_ptr<ChspExpr> AppendCallArgument( std::unique_ptr<ChspExpr> expr, std::unique_ptr<ChspExpr> arg,
											  int token_kind )
{
	if ( expr == nullptr || arg == nullptr ) {
		return nullptr;
	}
	if ( expr->kind == ChspExprKind::Call ) {
		expr->children.push_back( std::move( arg ) );
		return expr;
	}

	auto call = std::make_unique<ChspExpr>();
	call->kind = ChspExprKind::Call;
	call->line = expr->line;
	call->token_kind = token_kind;
	call->text = expr->text;
	call->children.push_back( std::move( expr ) );
	call->children.push_back( std::move( arg ) );
	return call;
}

std::unique_ptr<ChspExpr> ParseCallCG( ChspTokenCursor &cursor, std::unique_ptr<ChspExpr> callee )
{
	if ( !ConsumeToken( cursor, '(' ) ) {
		return callee;
	}

	auto call = std::make_unique<ChspExpr>();
	call->kind = ChspExprKind::Call;
	call->line = callee != nullptr ? callee->line : 0;
	call->token_kind = '(';
	call->text = callee != nullptr ? callee->text : "";
	call->children.push_back( std::move( callee ) );

	if ( ConsumeToken( cursor, ')' ) ) {
		return call;
	}

	while ( true ) {
		auto arg = CalcCG_start( cursor );
		if ( arg == nullptr ) {
			return nullptr;
		}
		call->children.push_back( std::move( arg ) );
		if ( ConsumeToken( cursor, ')' ) ) {
			break;
		}
		if ( !ConsumeToken( cursor, ',' ) ) {
			return nullptr;
		}
	}

	return call;
}

std::unique_ptr<ChspExpr> ParseDotIndexCG( ChspTokenCursor &cursor, std::unique_ptr<ChspExpr> base )
{
	if ( !ConsumeToken( cursor, '.' ) ) {
		return base;
	}

	std::unique_ptr<ChspExpr> index_expr;
	if ( ConsumeToken( cursor, '(' ) ) {
		index_expr = CalcCG_start( cursor );
		if ( index_expr == nullptr || !ConsumeToken( cursor, ')' ) ) {
			return nullptr;
		}
	} else {
		index_expr = CalcCG_factor( cursor );
		if ( index_expr == nullptr ) {
			return nullptr;
		}
	}

	return AppendCallArgument( std::move( base ), std::move( index_expr ), '.' );
}

std::unique_ptr<ChspExpr> CalcCG_factor( ChspTokenCursor &cursor )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr ) {
		return nullptr;
	}

	switch ( token->kind ) {
	case TK_NUM: {
		auto expr = MakeExprNode( ChspExprKind::IntLiteral, token );
		expr->int_value = token->value;
		cursor.Consume();
		return expr;
	}
	case TK_DNUM: {
		auto expr = MakeExprNode( ChspExprKind::DoubleLiteral, token );
		expr->double_value = token->value_double;
		cursor.Consume();
		return expr;
	}
	case TK_STRING: {
		auto expr = MakeExprNode( ChspExprKind::StringLiteral, token );
		cursor.Consume();
		return expr;
	}
	case TK_LABEL: {
		auto expr = MakeExprNode( ChspExprKind::Label, token );
		cursor.Consume();
		return expr;
	}
	case TK_OBJ: {
		std::string object_name;
		if ( !ConsumeScopedObject( cursor, object_name ) ) {
			return nullptr;
		}
		auto expr = MakeExprNode( ChspExprKind::Identifier, token );
		expr->text = object_name;
		if ( TokenMatches( cursor.Peek(), '@' ) && TokenMatches( cursor.Peek( 1 ), '(' ) ) {
			cursor.Consume();
		}
		while ( TokenMatches( cursor.Peek(), '(' ) || TokenMatches( cursor.Peek(), '.' ) ) {
			if ( TokenMatches( cursor.Peek(), '(' ) ) {
				expr = ParseCallCG( cursor, std::move( expr ) );
			} else {
				expr = ParseDotIndexCG( cursor, std::move( expr ) );
			}
			if ( expr == nullptr ) {
				return nullptr;
			}
		}
		return expr;
	}
	default:
		break;
	}

	if ( !ConsumeToken( cursor, '(' ) ) {
		return nullptr;
	}

	auto inner = CalcCG_start( cursor );
	if ( inner == nullptr || !ConsumeToken( cursor, ')' ) ) {
		return nullptr;
	}

	auto group = MakeExprNode( ChspExprKind::Group, token );
	group->children.push_back( std::move( inner ) );
	return group;
}

std::unique_ptr<ChspExpr> CalcCG_unary( ChspTokenCursor &cursor )
{
	const auto *token = cursor.Peek();
	if ( TokenMatches( token, '-' ) ) {
		cursor.Consume();
		auto rhs = CalcCG_unary( cursor );
		if ( rhs == nullptr ) {
			return nullptr;
		}
		auto expr = MakeExprNode( ChspExprKind::Unary, token );
		expr->token_kind = '-';
		expr->children.push_back( std::move( rhs ) );
		return expr;
	}
	return CalcCG_factor( cursor );
}

template <typename ParseNext>
std::unique_ptr<ChspExpr> ParseCalcBinary( ChspTokenCursor &cursor, ParseNext parse_next,
										   std::initializer_list<int> ops )
{
	auto lhs = parse_next( cursor );
	if ( lhs == nullptr ) {
		return nullptr;
	}

	while ( true ) {
		const auto *token = cursor.Peek();
		bool matched = false;
		for ( int op : ops ) {
			if ( TokenMatches( token, op ) ) {
				matched = true;
				break;
			}
		}
		if ( !matched ) {
			return lhs;
		}

		cursor.Consume();
		auto rhs = parse_next( cursor );
		if ( rhs == nullptr ) {
			return nullptr;
		}
		auto expr = MakeExprNode( ChspExprKind::Binary, token );
		expr->token_kind = token->kind == TK_SEPARATE ? token->text[0] : token->kind;
		expr->children.push_back( std::move( lhs ) );
		expr->children.push_back( std::move( rhs ) );
		lhs = std::move( expr );
	}
}

std::unique_ptr<ChspExpr> CalcCG_muldiv( ChspTokenCursor &cursor )
{
	return ParseCalcBinary( cursor, CalcCG_unary, { '*', '/', '\\' } );
}

std::unique_ptr<ChspExpr> CalcCG_addsub( ChspTokenCursor &cursor )
{
	return ParseCalcBinary( cursor, CalcCG_muldiv, { '+', '-' } );
}

std::unique_ptr<ChspExpr> CalcCG_shift( ChspTokenCursor &cursor )
{
	return ParseCalcBinary( cursor, CalcCG_addsub, { 0x63, 0x64 } );
}

std::unique_ptr<ChspExpr> CalcCG_compare( ChspTokenCursor &cursor )
{
	return ParseCalcBinary( cursor, CalcCG_shift, { '<', '>', '=', '!', 0x61, 0x62 } );
}

std::unique_ptr<ChspExpr> CalcCG_bool( ChspTokenCursor &cursor )
{
	return ParseCalcBinary( cursor, CalcCG_compare, { '&', '|', '^' } );
}

std::unique_ptr<ChspExpr> CalcCG_start( ChspTokenCursor &cursor )
{
	return CalcCG_bool( cursor );
}

std::unique_ptr<ChspStmt> ParseBodyStatement( ChspTokenCursor &cursor );

bool ParseInlineStatementList( ChspTokenCursor &cursor, std::vector<std::unique_ptr<ChspStmt>> &children )
{
	while ( !cursor.End() ) {
		auto child = ParseBodyStatement( cursor );
		if ( child == nullptr ) {
			return false;
		}
		if ( child->kind != ChspStmtKind::Empty ) {
			children.push_back( std::move( child ) );
		}
	}
	return true;
}

bool ParseInlineBranchStatements( ChspTokenCursor &cursor, std::vector<std::unique_ptr<ChspStmt>> &children,
								  bool allow_else )
{
	while ( true ) {
		while ( TokenMatches( cursor.Peek(), ':' ) ) {
			cursor.Consume();
		}
		if ( cursor.End() ) {
			return true;
		}
		const auto *token = cursor.Peek();
		if ( allow_else && token != nullptr && token->kind == TK_OBJ && BaseScopedName( token->text ) == "else" ) {
			auto else_stmt = ParseElseStatement( cursor, token );
			if ( else_stmt == nullptr ) {
				return false;
			}
			children.push_back( std::move( else_stmt ) );
			return cursor.End();
		}
		auto child = ParseBodyStatement( cursor );
		if ( child == nullptr ) {
			return false;
		}
		if ( child->kind != ChspStmtKind::Empty ) {
			children.push_back( std::move( child ) );
		}
		if ( cursor.End() ) {
			return true;
		}
		if ( !TokenMatches( cursor.Peek(), ':' ) ) {
			return false;
		}
	}
}

bool ParseGenerateCodeLET( ChspTokenCursor &cursor, std::unique_ptr<ChspStmt> &stmt )
{
	const size_t start_pos = cursor.Position();
	auto lhs = CalcCG_factor( cursor );
	if ( lhs == nullptr ) {
		return false;
	}
	if ( lhs->kind != ChspExprKind::Identifier && lhs->kind != ChspExprKind::Call ) {
		cursor.SetPosition( start_pos );
		return false;
	}

	const auto *op_token = cursor.Peek();
	if ( op_token == nullptr ) {
		cursor.SetPosition( start_pos );
		return false;
	}

	int op = 0;
	if ( TokenMatches( op_token, '=' ) || TokenMatches( op_token, '+' ) || TokenMatches( op_token, '-' ) ||
		 TokenMatches( op_token, '*' ) || TokenMatches( op_token, '/' ) ) {
		op = op_token->kind;
		if ( op_token->kind == TK_SEPARATE && !op_token->text.empty() ) {
			op = op_token->text[0];
		}
	} else {
		cursor.SetPosition( start_pos );
		return false;
	}

	cursor.Consume();

	auto assign = MakeStmtNode( ChspStmtKind::Assignment, lhs->line );
	assign->token_kind = op;
	assign->lhs = std::move( lhs );

	if ( op == '+' || op == '-' ) {
		if ( IsStatementDelimiter( cursor.Peek() ) ) {
			assign->text = op == '+' ? "++" : "--";
			stmt = std::move( assign );
			return true;
		}
		if ( ConsumeToken( cursor, op ) ) {
			if ( !IsStatementDelimiter( cursor.Peek() ) ) {
				return false;
			}
			assign->text = op == '+' ? "++" : "--";
			stmt = std::move( assign );
			return true;
		}
	}

	if ( op != '=' ) {
		if ( !ConsumeToken( cursor, '=' ) ) {
			return false;
		}
	}

	assign->rhs = CalcCG_start( cursor );
	if ( assign->rhs == nullptr || !IsStatementDelimiter( cursor.Peek() ) ) {
		return false;
	}

	stmt = std::move( assign );
	return true;
}

bool ParseCommandArguments( ChspTokenCursor &cursor, ChspStmt &stmt )
{
	if ( ConsumeToken( cursor, '(' ) ) {
		if ( ConsumeToken( cursor, ')' ) ) {
			return IsStatementDelimiter( cursor.Peek() );
		}
		while ( true ) {
			auto expr = CalcCG_start( cursor );
			if ( expr == nullptr ) {
				return false;
			}
			stmt.exprs.push_back( std::move( expr ) );
			if ( ConsumeToken( cursor, ')' ) ) {
				return IsStatementDelimiter( cursor.Peek() );
			}
			if ( !ConsumeToken( cursor, ',' ) ) {
				return false;
			}
		}
	}

	while ( !IsStatementDelimiter( cursor.Peek() ) ) {
		auto expr = CalcCG_start( cursor );
		if ( expr == nullptr ) {
			return false;
		}
		stmt.exprs.push_back( std::move( expr ) );
		if ( IsStatementDelimiter( cursor.Peek() ) ) {
			break;
		}
		if ( !ConsumeToken( cursor, ',' ) ) {
			return false;
		}
	}
	return true;
}

std::unique_ptr<ChspStmt> ParseIfStatement( ChspTokenCursor &cursor, const ChspLexedToken *if_token )
{
	if ( !ConsumeKeyword( cursor, "if" ) ) {
		return nullptr;
	}
	auto stmt = MakeStmtNode( ChspStmtKind::If, if_token != nullptr ? if_token->line : 0 );
	auto cond = CalcCG_start( cursor );
	if ( cond == nullptr ) {
		return nullptr;
	}
	stmt->exprs.push_back( std::move( cond ) );

	if ( ConsumeToken( cursor, '{' ) ) {
		stmt->token_kind = '{';
		if ( !cursor.End() ) {
			return nullptr;
		}
		return stmt;
	}

	if ( !ConsumeToken( cursor, ':' ) ) {
		return nullptr;
	}

	if ( !ParseInlineBranchStatements( cursor, stmt->children, true ) ) {
		return nullptr;
	}
	stmt->token_kind = ':';
	return stmt;
}

std::unique_ptr<ChspStmt> ParseElseStatement( ChspTokenCursor &cursor, const ChspLexedToken *else_token )
{
	if ( !ConsumeKeyword( cursor, "else" ) ) {
		return nullptr;
	}
	auto stmt = MakeStmtNode( ChspStmtKind::Else, else_token != nullptr ? else_token->line : 0 );
	stmt->text = "else";

	if ( ConsumeToken( cursor, '{' ) ) {
		stmt->token_kind = '{';
		if ( !cursor.End() ) {
			return nullptr;
		}
		return stmt;
	}

	if ( !ConsumeToken( cursor, ':' ) ) {
		return nullptr;
	}

	if ( !ParseInlineBranchStatements( cursor, stmt->children, false ) ) {
		return nullptr;
	}
	stmt->token_kind = ':';
	return stmt;
}

std::unique_ptr<ChspStmt> ParseBodyStatement( ChspTokenCursor &cursor )
{
	const auto *token = cursor.Peek();
	if ( token == nullptr ) {
		return MakeStmtNode( ChspStmtKind::Empty, 0 );
	}
	if ( TokenMatches( token, ':' ) ) {
		cursor.Consume();
		return MakeStmtNode( ChspStmtKind::Empty, token->line );
	}
	if ( TokenMatches( token, '}' ) ) {
		auto stmt = MakeStmtNode( ChspStmtKind::BlockMarker, token->line );
		stmt->token_kind = '}';
		stmt->text = "}";
		cursor.Consume();
		return stmt;
	}
	if ( token->kind == TK_OBJ && BaseScopedName( token->text ) == "else" ) {
		return ParseElseStatement( cursor, token );
	}
	if ( token->kind == TK_OBJ && BaseScopedName( token->text ) == "if" ) {
		return ParseIfStatement( cursor, token );
	}
	if ( token->kind == TK_OBJ && BaseScopedName( token->text ) == "return" ) {
		if ( !ConsumeKeyword( cursor, "return" ) ) {
			return nullptr;
		}
		auto stmt = MakeStmtNode( ChspStmtKind::Return, token->line );
		stmt->text = "return";
		if ( IsStatementDelimiter( cursor.Peek() ) ) {
			return stmt;
		}
		stmt->rhs = CalcCG_start( cursor );
		if ( stmt->rhs == nullptr || !IsStatementDelimiter( cursor.Peek() ) ) {
			return nullptr;
		}
		return stmt;
	}
	if ( token->kind == TK_OBJ && BaseScopedName( token->text ) == "repeat" ) {
		if ( !ConsumeKeyword( cursor, "repeat" ) ) {
			return nullptr;
		}
		auto stmt = MakeStmtNode( ChspStmtKind::Repeat, token->line );
		stmt->text = "repeat";
		if ( IsStatementDelimiter( cursor.Peek() ) ) {
			return stmt;
		}
		stmt->rhs = CalcCG_start( cursor );
		if ( stmt->rhs == nullptr || !IsStatementDelimiter( cursor.Peek() ) ) {
			return nullptr;
		}
		return stmt;
	}
	if ( token->kind == TK_OBJ && BaseScopedName( token->text ) == "loop" ) {
		if ( !ConsumeKeyword( cursor, "loop" ) ) {
			return nullptr;
		}
		if ( !IsStatementDelimiter( cursor.Peek() ) ) {
			return nullptr;
		}
		auto stmt = MakeStmtNode( ChspStmtKind::Loop, token->line );
		stmt->text = "loop";
		return stmt;
	}

	std::unique_ptr<ChspStmt> stmt;
	if ( ParseGenerateCodeLET( cursor, stmt ) ) {
		return stmt;
	}

	if ( token->kind == TK_OBJ ) {
		std::string command_name;
		if ( !ConsumeScopedObject( cursor, command_name ) ) {
			return nullptr;
		}
		stmt = MakeStmtNode( ChspStmtKind::Command, token->line );
		stmt->text = command_name;
		if ( !ParseCommandArguments( cursor, *stmt ) ) {
			return nullptr;
		}
		return stmt;
	}

	return nullptr;
}

bool ParseFunctionBody( ChspFunction &function, CLogger &logger, const char *source_name )
{
	for ( const auto &line : function.body_lines ) {
		ChspTokenCursor cursor( line.tokens );
		while ( !cursor.End() ) {
			auto stmt = ParseBodyStatement( cursor );
			if ( stmt == nullptr ) {
				logger.Mesf( "#Error:Invalid cHSP statement in line %d [%s] (%s)", line.line,
							 source_name != nullptr ? source_name : "<buffer>", line.text.c_str() );
				return false;
			}
			if ( stmt->kind != ChspStmtKind::Empty ) {
				function.body_stmts.push_back( std::move( stmt ) );
			}
		}
	}
	return true;
}

} // namespace

std::vector<ChspSourceLine> BuildSourceIndex( const char *input_text, const std::shared_ptr<CLogger> &logger )
{
	std::vector<ChspSourceLine> lines_out;
	ChspTokenCollector collector( logger );
	const auto lines = CollapseContinuationLines( SplitLines( input_text != nullptr ? input_text : "" ) );
	bool in_chsp_function = false;
	for ( const auto &input_line : lines ) {
		ChspSourceLine line;
		line.line = input_line.line;
		line.text = input_line.text;
		line.directive = DetectDirective( line.text );
		const auto trimmed = Trim( line.text );
		const bool should_tokenize =
			!trimmed.empty() && !StartsWith( trimmed, "##" ) &&
			( line.directive != ChspDirectiveKind::None || ( in_chsp_function && trimmed[0] != '#' ) );
		if ( should_tokenize ) {
			line.tokens = collector.TokenizeLine( line.text, line.line );
		}
		const ChspDirectiveKind directive = line.directive;
		lines_out.push_back( std::move( line ) );
		if ( directive == ChspDirectiveKind::DefFunc || directive == ChspDirectiveKind::DefCFunc ) {
			in_chsp_function = true;
		} else if ( directive == ChspDirectiveKind::End ) {
			in_chsp_function = false;
		}
	}
	return lines_out;
}

bool ParseProgram( const std::vector<ChspSourceLine> &lines, ChspProgram &program, CLogger &logger,
				   const char *source_name )
{
	ChspModule *current_module = nullptr;
	for ( size_t i = 0; i < lines.size(); ++i ) {
		const auto &line = lines[i];
		switch ( line.directive ) {
		case ChspDirectiveKind::None:
			program.passthrough_lines.push_back( line );
			break;
		case ChspDirectiveKind::Module: {
			ChspModule module;
			if ( !ParseModuleHeader( line, module ) ) {
				logger.Mesf( "#Error:Invalid #chsp_module directive in line %d [%s]", line.line,
							 source_name != nullptr ? source_name : "<buffer>" );
				return false;
			}
			program.modules.push_back( std::move( module ) );
			current_module = &program.modules.back();
			break;
		}
		case ChspDirectiveKind::ModuleEnd:
			current_module = nullptr;
			break;
		case ChspDirectiveKind::DefFunc:
		case ChspDirectiveKind::DefCFunc: {
			if ( current_module == nullptr ) {
				logger.Mesf( "#Error:cHSP function outside module in line %d [%s]", line.line,
							 source_name != nullptr ? source_name : "<buffer>" );
				return false;
			}
			ChspFunction function;
			if ( !ParseFunctionSignature( line, function ) ) {
				logger.Mesf( "#Error:Invalid cHSP function signature in line %d [%s]", line.line,
							 source_name != nullptr ? source_name : "<buffer>" );
				return false;
			}
			size_t body_index = i + 1;
			for ( ; body_index < lines.size(); ++body_index ) {
				if ( lines[body_index].directive == ChspDirectiveKind::End ) {
					break;
				}
				function.body_lines.push_back( lines[body_index] );
			}
			if ( body_index >= lines.size() ) {
				logger.Mesf( "#Error:Missing #chsp_end [%s]", source_name != nullptr ? source_name : "<buffer>" );
				return false;
			}
			if ( !ParseFunctionBody( function, logger, source_name ) ) {
				return false;
			}
			current_module->functions.push_back( std::move( function ) );
			i = body_index;
			break;
		}
		case ChspDirectiveKind::End:
			logger.Mesf( "#Error:Unexpected #chsp_end in line %d [%s]", line.line,
						 source_name != nullptr ? source_name : "<buffer>" );
			return false;
		}
	}
	return true;
}

} // namespace chspv2
