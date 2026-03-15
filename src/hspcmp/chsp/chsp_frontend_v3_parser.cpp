//
//		Token analysis class ( HSP3 code generator )
//			onion software/onitama 2004/4
//
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../hsp3/hsp3config.h"
#include "../../hsp3/hsp3debug.h"
#include "../../hsp3/hsp3struct.h"
#include "../../hsp3/strnote.h"

#include "chsp_frontend_v2_internal.h"
#include "chsp_frontend_v3_parser.h"
#include "codegen_lexer.h"
// #include "codegen_writer.h"
#include "../comutil.h"
#include "../label.h"
#include "../membuf.h"
#include "../supio.h"
#include "../tagstack.h"
#include "../token_def.h"

#include "../errormsg.h"

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

void CChspParser::CalcCG_token()
{
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype == TK_NONE ) {
		token.ttype = token.val;
	}
}

void CChspParser::CalcCG_token_exprbeg()
{
	token = lexer.GetTokenCG( GETTOKEN_EXPRBEG );
	if ( token.ttype == TK_NONE ) {
		token.ttype = token.val;
	}
}

void CChspParser::CalcCG_token_exprbeg_redo()
{
	//		GETTOKEN_EXPRBEG でトークンを取得し直す
	//
	//		先頭が単項演算子の場合だけ読み直す
	//
	if ( token.ttype == TK_NONE ) {
		token.ttype = token.val;
	}
	if ( token.ttype == '-' || token.ttype == '*' ) {
		lexer.cg_ptr = lexer.cg_ptr_bak;
		CalcCG_token_exprbeg();
	}
}

static int is_statement_end( int type )
{
	return static_cast<int>( ( type == TK_SEPARATE ) || ( type == TK_EOL ) || ( type == TK_EOF ) );
}

static chspv3::ChspV3AstStmtKind detect_ast_stmt_kind( int statement_kind, const std::string &text )
{
	std::string normalized = text;
	const size_t at = normalized.find( '@' );
	if ( at != std::string::npos ) {
		normalized = normalized.substr( 0, at );
	}
	if ( statement_kind == CG_LASTCMD_LET ) {
		return chspv3::ChspV3AstStmtKind::Assignment;
	}
	if ( normalized == "return" ) {
		return chspv3::ChspV3AstStmtKind::Return;
	}
	if ( normalized == "repeat" ) {
		return chspv3::ChspV3AstStmtKind::Repeat;
	}
	if ( normalized == "loop" ) {
		return chspv3::ChspV3AstStmtKind::Loop;
	}
	if ( normalized == "if" ) {
		return chspv3::ChspV3AstStmtKind::If;
	}
	if ( normalized == "else" ) {
		return chspv3::ChspV3AstStmtKind::Else;
	}
	if ( statement_kind == CG_LASTCMD_CMD ) {
		return chspv3::ChspV3AstStmtKind::Command;
	}
	return chspv3::ChspV3AstStmtKind::Unknown;
}

static chspv3::ChspV3SourceDirectiveKind detect_source_directive_kind( const std::string &line )
{
	const auto trimmed = chspv2::NormalizeIdentifier( chspv2::Trim( line ) );
	if ( chspv2::StartsWith( trimmed, "#chsp_module_end" ) ) {
		return chspv3::ChspV3SourceDirectiveKind::ModuleEnd;
	}
	if ( chspv2::StartsWith( trimmed, "#chsp_module" ) ) {
		return chspv3::ChspV3SourceDirectiveKind::Module;
	}
	if ( chspv2::StartsWith( trimmed, "#chsp_defcfunc" ) ) {
		return chspv3::ChspV3SourceDirectiveKind::DefCFunc;
	}
	if ( chspv2::StartsWith( trimmed, "#chsp_deffunc" ) ) {
		return chspv3::ChspV3SourceDirectiveKind::DefFunc;
	}
	if ( trimmed == "#chsp_end" ) {
		return chspv3::ChspV3SourceDirectiveKind::End;
	}
	return chspv3::ChspV3SourceDirectiveKind::None;
}

static std::vector<std::unique_ptr<chspv3::ChspV3AstStmt>> fold_ast_statements(
	std::vector<std::unique_ptr<chspv3::ChspV3AstStmt>> &flat, size_t &index, int base_if_depth, int base_repeat_depth )
{
	std::vector<std::unique_ptr<chspv3::ChspV3AstStmt>> out;
	while ( index < flat.size() ) {
		auto &peek = flat[index];
		if ( peek == nullptr ) {
			++index;
			continue;
		}
		if ( peek->if_depth < base_if_depth || peek->repeat_depth < base_repeat_depth ) {
			break;
		}
		if ( peek->if_depth != base_if_depth || peek->repeat_depth != base_repeat_depth ) {
			break;
		}

		auto stmt = std::move( flat[index++] );
		switch ( stmt->kind ) {
		case chspv3::ChspV3AstStmtKind::If:
			stmt->children = fold_ast_statements( flat, index, stmt->if_depth + 1, stmt->repeat_depth );
			break;
		case chspv3::ChspV3AstStmtKind::Else:
			stmt->children = fold_ast_statements( flat, index, stmt->if_depth, stmt->repeat_depth );
			break;
		case chspv3::ChspV3AstStmtKind::Repeat:
			stmt->children = fold_ast_statements( flat, index, stmt->if_depth, stmt->repeat_depth + 1 );
			break;
		default:
			break;
		}
		out.push_back( std::move( stmt ) );
	}
	return out;
}

static void finalize_ast_program( chspv3::ChspV3AstProgram &program )
{
	for ( auto &module : program.modules ) {
		for ( auto &function : module.functions ) {
			size_t index = 0;
			function.body_stmts = fold_ast_statements( function.body_stmts, index, 0, 0 );
		}
	}
	size_t index = 0;
	program.top_level_stmts = fold_ast_statements( program.top_level_stmts, index, 0, 0 );
}

static std::unique_ptr<chspv3::ChspV3AstExpr> make_int_literal_expr( int line, int value )
{
	auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
	expr->kind = chspv3::ChspV3AstExprKind::IntLiteral;
	expr->line = line;
	expr->token_kind = TK_NUM;
	expr->text = std::to_string( value );
	return expr;
}

static void wrap_ast_target_as_call( std::unique_ptr<chspv3::ChspV3AstExpr> *target, int token_kind,
									 std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> &args )
{
	if ( target == nullptr || *target == nullptr ) {
		return;
	}
	if ( token_kind != '(' && args.empty() ) {
		return;
	}

	auto call = std::make_unique<chspv3::ChspV3AstExpr>();
	call->kind = chspv3::ChspV3AstExprKind::Call;
	call->line = ( *target )->line;
	call->token_kind = token_kind;
	call->text = ( *target )->text;
	call->children.push_back( std::move( *target ) );
	for ( auto &arg : args ) {
		call->children.push_back( std::move( arg ) );
	}
	*target = std::move( call );
}


void CChspParser::CalcCG_regmark( int mark )
{
	//		演算子を登録する
	//
	// CG: writer->PutCSMark( mark, texflag );
	if ( current_stmt != nullptr ) {
		auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
		expr->kind = chspv3::ChspV3AstExprKind::Binary;
		expr->line = lexer.cg_orgline;
		expr->token_kind = mark;
		expr->operator_kind = mark;
		expr->text = std::string( 1, static_cast<char>( mark ) );
		if ( expression_stack.size() >= 2 ) {
			auto rhs = std::move( expression_stack.back() );
			expression_stack.pop_back();
			auto lhs = std::move( expression_stack.back() );
			expression_stack.pop_back();
			expr->children.push_back( std::move( lhs ) );
			expr->children.push_back( std::move( rhs ) );
		}
		CaptureAstExpr( std::move( expr ) );
	}
	calccount++;
}

void CChspParser::CalcCG_factor()
{
	int id;

	cs_lasttype = token.ttype;
	switch ( token.ttype ) {
	case TK_NUM:
		// CG: writer->PutCSInteger( token.val, texflag );
		if ( current_stmt != nullptr ) {
			auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::IntLiteral;
			expr->line = lexer.cg_orgline;
			expr->token_kind = TK_NUM;
			expr->text = std::to_string( token.val );
			CaptureAstExpr( std::move( expr ) );
		}
		texflag = 0;
		CalcCG_token();
		calccount++;
		return;
	case TK_DNUM:
		// CG: writer->PutCSDouble( token.val_d, texflag );
		if ( current_stmt != nullptr ) {
			auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::DoubleLiteral;
			expr->line = lexer.cg_orgline;
			expr->token_kind = TK_DNUM;
			expr->text = std::to_string( token.val_d );
			CaptureAstExpr( std::move( expr ) );
		}
		texflag = 0;
		CalcCG_token();
		calccount++;
		return;
	case TK_STRING:
		// CG: writer->PutCSString( token.cg_str.c_str(), texflag );
		if ( current_stmt != nullptr ) {
			auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::StringLiteral;
			expr->line = lexer.cg_orgline;
			expr->token_kind = TK_STRING;
			expr->text = token.cg_str;
			CaptureAstExpr( std::move( expr ) );
		}
		texflag = 0;
		CalcCG_token();
		calccount++;
		return;
	case TK_LABEL:
		if ( current_stmt != nullptr ) {
			auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::Label;
			expr->line = lexer.cg_orgline;
			expr->token_kind = TK_LABEL;
			expr->text = token.cg_str;
			CaptureAstExpr( std::move( expr ) );
		}
		GenerateCodeLabel( token.cg_str, texflag );
		texflag = 0;
		CalcCG_token();
		calccount++;
		return;
	case TK_OBJ: {
		id = SetVarsFixed( token.cg_str, cg_defvarfix );
		std::unique_ptr<chspv3::ChspV3AstExpr> expr;
		if ( current_stmt != nullptr ) {
			expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::Identifier;
			expr->line = lexer.cg_orgline;
			expr->token_kind = TK_OBJ;
			expr->text = token.cg_str;
		}
		/*CG
		if ( symtab->lb->GetType( id ) == TYPE_VAR ) {
			if ( symtab->lb->GetInitFlag( id ) == LAB_INIT_NO ) {
#ifdef JPNMSG
				logger->Mesf( "#未初期化の変数があります(%s)", token.cg_str.c_str() );
#else
				logger->Mesf( "#Uninitalized variable (%s).", token.cg_str.c_str() );
#endif
				if ( ( compopt->hed_cmpmode & CMPMODE_VARINIT ) != 0 ) {
					throw CGERROR_VAR_NOINIT;
				}
				symtab->lb->SetInitFlag( id, LAB_INIT_DONE );
			}
		}
		*/
		GenerateCodeVAR( id, texflag, expr != nullptr ? &expr : nullptr );
		if ( expr != nullptr ) {
			CaptureAstExpr( std::move( expr ) );
		}
		texflag = 0;
		if ( token.ttype == TK_NONE ) {
			token.ttype = token.val; // CalcCG_token()に合わせるため
		}
		calccount++;
		return;
	}
	case TK_SEPARATE:
	case TK_EOL:
	case TK_EOF:
		return;
	default:
		break;
	}

	if ( token.ttype != '(' ) {
		// logger->Mesf("#Invalid%d", token.ttype);
		token.ttype = TK_CALCERROR;
		return;
	}

	//		カッコの処理
	//
	CalcCG_token_exprbeg();
	CalcCG_start();
	if ( token.ttype != ')' ) {
		token.ttype = TK_CALCERROR;
		return;
	}
	if ( current_stmt != nullptr && !expression_stack.empty() ) {
		auto expr = std::make_unique<chspv3::ChspV3AstExpr>(); // FIXME 後でEmitterで対応にする
		expr->kind = chspv3::ChspV3AstExprKind::Group;
		expr->line = lexer.cg_orgline;
		expr->token_kind = '(';
		expr->text = "()";
		expr->children.push_back( std::move( expression_stack.back() ) );
		expression_stack.pop_back();
		CaptureAstExpr( std::move( expr ) );
	}

	CalcCG_token();
}

void CChspParser::CalcCG_unary()
{
	//		単項演算子
	//
	if ( token.ttype == '-' ) {
		const int op = token.ttype;
		const int line = lexer.cg_orgline;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_unary();
		texflag = 0;
		// CG: writer->PutCS( TYPE_INUM, -1, texflag );
		if ( current_stmt != nullptr && !expression_stack.empty() ) {
			auto expr = std::make_unique<chspv3::ChspV3AstExpr>();
			expr->kind = chspv3::ChspV3AstExprKind::Unary;
			expr->line = line;
			expr->token_kind = op;
			expr->operator_kind = op;
			expr->text = std::string( 1, static_cast<char>( op ) );
			expr->children.push_back( std::move( expression_stack.back() ) );
			expression_stack.pop_back();
			CaptureAstExpr( std::move( expr ) );
			calccount++;
		} else {
			CalcCG_regmark( '*' );
		}
	} else {
		CalcCG_factor();
	}
}

void CChspParser::CalcCG_muldiv()
{
	int op;
	CalcCG_unary();

	while ( ( token.ttype == '*' ) || ( token.ttype == '/' ) || ( token.ttype == '\\' ) ) {
		op = token.ttype;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_unary();
		CalcCG_regmark( op );
	}
}

void CChspParser::CalcCG_addsub()
{
	int op;
	CalcCG_muldiv();

	while ( ( token.ttype == '+' ) || ( token.ttype == '-' ) ) {
		op = token.ttype;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_muldiv();
		CalcCG_regmark( op );
	}
}


void CChspParser::CalcCG_shift()
{
	int op;
	CalcCG_addsub();

	while ( ( token.ttype == 0x63 ) || ( token.ttype == 0x64 ) ) {
		op = token.ttype;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_addsub();
		CalcCG_regmark( op );
	}
}


void CChspParser::CalcCG_compare()
{
	int op;
	CalcCG_shift();

	while ( ( token.ttype == '<' ) || ( token.ttype == '>' ) || ( token.ttype == '=' ) || ( token.ttype == '!' ) ||
			( token.ttype == 0x61 ) || ( token.ttype == 0x62 ) ) {
		op = token.ttype;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_shift();
		CalcCG_regmark( op );
	}
}


void CChspParser::CalcCG_bool()
{
	int op;
	CalcCG_compare();

	while ( ( token.ttype == '&' ) || ( token.ttype == '|' ) || ( token.ttype == '^' ) ) {
		op = token.ttype;
		CalcCG_token_exprbeg();
		if ( is_statement_end( token.ttype ) != 0 ) {
			throw CGERROR_CALCEXP;
		}
		CalcCG_compare();
		CalcCG_regmark( op );
	}
}


void CChspParser::CalcCG_start()
{
	//		entry point
	CalcCG_bool();
}

void CChspParser::CalcCG( int ex )
{
	//		パラメーターの式を評価する
	//		(結果は逆ポーランドでコードを出力する)
	//
	texflag = ex;
	// CG: cs_lastptr = writer->cs_buf->GetSize();
	calccount = 0;

	CalcCG_token_exprbeg_redo();

	CalcCG_start();

	if ( token.ttype == TK_CALCERROR ) {
		throw CGERROR_CALCEXP;
	}
}

//-----------------------------------------------------------------------------

void CChspParser::GenerateCodePRM()
{
	//		HSP3Codeを展開する(パラメーター)
	//
	int ex;
	ex = 0;
	while ( true ) {

		if ( token.ttype == TK_NONE ) {
			if ( token.val == ',' ) { // 先頭が','の場合は省略
				if ( ( ex & EXFLG_2 ) != 0 ) {
					// CG: writer->PutCS( TYPE_MARK, '?', EXFLG_2 );
				}
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				ex |= EXFLG_2;
				continue;
			}
		}

		expression_stack.clear();
		CalcCG( ex ); // 式の評価
		if ( current_stmt != nullptr ) {
			auto expr = TakeCapturedExpression();
			if ( expr != nullptr ) {
				switch ( current_stmt->kind ) {
				case chspv3::ChspV3AstStmtKind::Assignment:
				case chspv3::ChspV3AstStmtKind::Return:
				case chspv3::ChspV3AstStmtKind::Repeat:
					if ( current_stmt->rhs == nullptr ) {
						current_stmt->rhs = std::move( expr );
					} else {
						current_stmt->exprs.push_back( std::move( expr ) );
					}
					break;
				case chspv3::ChspV3AstStmtKind::If:
					current_stmt->exprs.push_back( std::move( expr ) );
					break;
				default:
					current_stmt->exprs.push_back( std::move( expr ) );
					break;
				}
			}
		}
		// logger->Mesf( "#count %d", calccount );

		if ( ( compopt->hed_cmpmode & CMPMODE_OPTPRM ) != 0 ) {
			if ( calccount == 1 ) { // パラメーターが単一項目の時
				switch ( cs_lasttype ) {
				case TK_NUM:
				case TK_DNUM:
				case TK_STRING: {
					unsigned short *cstmp;
					// CG: cstmp = (unsigned short *)( writer->cs_buf->GetBuffer() + cs_lastptr );
					// CG: *cstmp |= EXFLG_0; // 単一項目フラグを立てる
					break;
				}
				default:
					break;
				}
			}
		}

		if ( token.ttype >= TK_SEPARATE ) {
			break;
		}
		if ( token.ttype != ',' ) {
			throw CGERROR_CALCEXP;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		ex |= EXFLG_2;

		if ( token.ttype >= TK_SEPARATE ) {
			// CG: writer->PutCS( TYPE_MARK, '?', EXFLG_2 );
			break;
		}
	}
}


int CChspParser::GenerateCodePRMF( std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> *ast_args )
{
	//		HSP3Codeを展開する(カッコ内のパラメーター)
	//		(戻り値 : exflg)
	//
	int ex;
	ex = 0;
	while ( true ) {

		if ( token.ttype == TK_NONE ) {
			if ( token.val == ')' ) { // ')'の場合は終了
				if ( ex != 0 ) {
					// CG: writer->PutCS( TYPE_MARK, '?', EXFLG_2 );
				}
				return ex;
			}
			if ( token.val == ',' ) { // 先頭が','の場合は省略
				if ( ( ex & EXFLG_2 ) != 0 ) {
					// CG: writer->PutCS( TYPE_MARK, '?', EXFLG_2 );
				}
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				ex |= EXFLG_2;
				continue;
			}
		}

		CalcCG( ex );
		if ( ast_args != nullptr ) {
			auto expr = TakeCapturedExpression();
			if ( expr != nullptr ) {
				ast_args->push_back( std::move( expr ) );
			}
		}

		if ( token.ttype >= TK_SEPARATE ) {
			throw CGERROR_PRMEND;
		}

		if ( token.ttype == ')' ) {
			break;
		}
		if ( token.ttype != ',' ) {
			throw CGERROR_CALCEXP;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		ex |= EXFLG_2;
	}
	return 0;
}


void CChspParser::GenerateCodePRMF2( std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> *ast_args )
{
	//		HSP3Codeを展開する('.'から始まる配列内のパラメーター)
	//
	int ex = 0;
	while ( true ) {
		if ( token.ttype >= TK_SEPARATE ) {
			break;
		}

		// logger->Mesf( "(type:%d val:%d) line:%d", token.ttype, token.val, lexer.cg_orgline );

		switch ( token.ttype ) {
		case TK_NONE:
			if ( token.val == '(' ) {
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				CalcCG( ex );
				if ( ast_args != nullptr ) {
					auto expr = TakeCapturedExpression();
					if ( expr != nullptr ) {
						ast_args->push_back( std::move( expr ) );
					}
				}
				if ( token.ttype != ')' ) {
					throw CGERROR_CALCEXP;
				}
			} else {
				throw CGERROR_ARRAYEXP;
			}
			token = lexer.GetTokenCG( GETTOKEN_NOFLOAT );
			break;
		case TK_NUM:
			// CG: writer->PutCS( TYPE_INUM, token.val, ex );
			if ( ast_args != nullptr ) {
				ast_args->push_back( make_int_literal_expr( lexer.cg_orgline, token.val ) );
			}
			token = lexer.GetTokenCG( GETTOKEN_NOFLOAT );
			break;
		case TK_OBJ: {
			int id = SetVarsFixed( token.cg_str, cg_defvarfix );
			std::unique_ptr<chspv3::ChspV3AstExpr> expr;
			if ( ast_args != nullptr ) {
				expr = std::make_unique<chspv3::ChspV3AstExpr>();
				expr->kind = chspv3::ChspV3AstExprKind::Identifier;
				expr->line = lexer.cg_orgline;
				expr->token_kind = TK_OBJ;
				expr->text = token.cg_str;
			}
			/*CG
			int t = symtab->lb->GetType( id );
			if ( ( t == TYPE_XLABEL ) || ( t == TYPE_LABEL ) ) {
				throw CGERROR_LABELNAME;
			}
			writer->PutCSSymbol( id, ex );
			*/
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

			if ( token.ttype == TK_NONE ) {
				if ( token.val == '(' ) { // '(' 配列指定
					token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
					// CG: writer->PutCS( TYPE_MARK, '(', 0 );
					std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> nested_args;
					GenerateCodePRMF( expr != nullptr ? &nested_args : nullptr );
					if ( expr != nullptr ) {
						wrap_ast_target_as_call( &expr, '(', nested_args );
					}
					// CG: writer->PutCS( TYPE_MARK, ')', 0 );
					token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				}
			}

			// GenerateCodeVAR( id, ex );
			(void)id;
			if ( ast_args != nullptr && expr != nullptr ) {
				ast_args->push_back( std::move( expr ) );
			}
			break;
		}
		default:
			throw CGERROR_ARRAYEXP;
		}

		if ( token.ttype >= TK_SEPARATE ) {
			return;
		}
		if ( token.ttype != TK_NONE && token.ttype != '.' ) {
			throw CGERROR_ARRAYEXP;
		}
		if ( token.val != '.' ) {
			return;
		}

		token = lexer.GetTokenCG( GETTOKEN_NOFLOAT );
		ex |= EXFLG_2;
	}
}


void CChspParser::GenerateCodePRMF3()
{
	//		HSP3Codeを展開する('['から始まる構造体参照元のパラメーター)
	//
	int id;
	int ex;
	ex = 0;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_BAD_STRUCT_SOURCE;
	}

	id = SetVarsFixed( token.cg_str, cg_defvarfix );
	GenerateCodeVAR( id, ex );

	if ( token.ttype != TK_NONE ) {
		throw CGERROR_PP_BAD_STRUCT_SOURCE;
	}
	if ( token.val != ']' ) {
		throw CGERROR_PP_BAD_STRUCT_SOURCE;
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
}


int CChspParser::GenerateCodePRMF4( int t, std::unique_ptr<chspv3::ChspV3AstExpr> *ast_target )
{
	//		HSP3Codeを展開する(構造体/配列指定パラメーター)
	//
	if ( token.ttype == TK_NONE ) {
		if ( token.val == '.' ) {
			token = lexer.GetTokenCG( GETTOKEN_NOFLOAT );
			// CG: writer->PutCS( TYPE_MARK, '(', 0 ); // '.' 配列指定
			std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> args;
			GenerateCodePRMF2( ast_target != nullptr ? &args : nullptr );
			wrap_ast_target_as_call( ast_target, '.', args );
			// CG: writer->PutCS( TYPE_MARK, ')', 0 );
			return 1;
		}
		if ( token.val == '(' ) { // '(' 配列指定
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			// CG: writer->PutCS( TYPE_MARK, '(', 0 );
			std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> args;
			GenerateCodePRMF( ast_target != nullptr ? &args : nullptr );
			wrap_ast_target_as_call( ast_target, '(', args );
			// CG: writer->PutCS( TYPE_MARK, ')', 0 );
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			return 1;
		}
		if ( t == TYPE_STRUCT ) {
			if ( token.val == '[' ) { // '[' ソース指定
				// CG: writer->PutCS( TYPE_MARK, '[', 0 );
				GenerateCodePRMF3();
				return 0;
			}
		}
	}
	return 0;
}


void CChspParser::GenerateCodeMethod()
{
	//		HSP3Codeを展開する(->に続くメソッド名)
	//
	int id;
	int ex;
	ex = 0;

	if ( token.ttype >= TK_SEPARATE ) {
		throw CGERROR_SYNTAX;
	}
	switch ( token.ttype ) {
	case TK_NUM:
		// CG: writer->PutCS( TYPE_INUM, token.val, ex );
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		break;
	case TK_STRING:
		// CG: writer->PutCS( TYPE_STRING, writer->PutDS( token.cg_str.c_str() ), ex );
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		break;
	case TK_OBJ:
		id = SetVarsFixed( token.cg_str, cg_defvarfix );
		GenerateCodeVAR( id, ex );
		break;
	default:
		throw CGERROR_SYNTAX;
	}

	ex |= EXFLG_2;
	while ( true ) {

		if ( token.ttype == TK_NONE ) {
			if ( token.val == ',' ) { // 先頭が','の場合は省略
				if ( ( ex & EXFLG_2 ) != 0 ) {
					// CG: writer->PutCS( TYPE_MARK, '?', EXFLG_2 );
				}
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				ex |= EXFLG_2;
				continue;
			}
		}

		CalcCG( ex ); // 式の評価

		if ( token.ttype >= TK_SEPARATE ) {
			break;
		}
		if ( token.ttype != ',' ) {
			throw CGERROR_CALCEXP;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		ex |= EXFLG_2;
	}
}

void CChspParser::GenerateCodeLabel( const std::string &keyname, int ex )
{
	//		HSP3Codeを展開する(ラベル)
	//
	char lname[128];

	const char *name = keyname.c_str();
	if ( *name == '@' ) {
		int i;
		switch ( tolower( name[1] ) ) {
		case 'f':
			i = cg_locallabel;
			break;
		case 'b':
			i = cg_locallabel - 1;
			break;
		default:
			throw CGERROR_LABELNAME;
		}
		sprintf( lname, "@l%d", i ); // local label
		name = lname;
	}

	/*CG
	int id = writer->PutCSLabel( name, ex );

	GenerateLabelListAndTagRef( id, LABBUF_FLAG_LABEL );
	*/
}


void CChspParser::GenerateCodeVAR( int id, int ex, std::unique_ptr<chspv3::ChspV3AstExpr> *ast_target )
{
	//		HSP3Codeを展開する(変数ほか)
	//		(idはlabel ID)
	//
	int t;
	t = symtab->lb->GetType( id );
	if ( ( t == TYPE_XLABEL ) || ( t == TYPE_LABEL ) ) {
		throw CGERROR_LABELNAME;
	}
	/*CG
	GenerateLabelListAndTagRef( id, LABBUF_FLAG_VAR );

	//
	writer->PutCSSymbol( id, ex );
	*/
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( t == TYPE_SYSVAR ) {
		return;
	}
	GenerateCodePRMF4( t, ast_target ); // 構造体/配列のチェック
}


void CChspParser::CheckCMDIF_Set( int mode )
{
	//		set 'if'&'else' command additional code
	//			mode/ 0=if 1=else
	//
	if ( iflev >= CG_IFLEV_MAX ) {
		throw CGERROR_IF_OVERFLOW;
	}

	iftype[iflev] = mode;
	// CG: ifptr[iflev] = writer->ReserveCSAddress();
	// CG: ifmode[iflev] = writer->GetCS();
	ifscope[iflev] = CG_IFCHECK_LINE;
	ifterm[iflev] = 0;
	iflev++;
	// sprintf(tmp,"#IF BEGIN [L=%d(%d)]\n",cline,iflev);
	// prt(tmp);
}


void CChspParser::CheckCMDIF_Fin( int mode )
{
	//		finish 'if'&'else' command
	//			mode/ 0=if 1=else
	//
	int a;
	short *p;
	if ( iflev == 0 ) {
		return;
	}
finag:
	iflev--;
	// CG: a = writer->GetCS() - ifmode[iflev];
	if ( mode != 0 ) { // when 'else'
		a++;
	}

	if ( ifterm[iflev] == 0 ) {
		ifterm[iflev] = 1;
		// CG: writer->PatchCSAddress( ifptr[iflev], a );
	}

	// sprintf(tmp,"#IF FINISH [L=%d(%d)] [skip%d]\n",cline,iflev,a);
	// prt(tmp);

	// logger->Mesf( "lev%d : %d: line%d", iflev, ifscope[iflev], lexer.cg_orgline );

	if ( mode == 0 ) {
		if ( iflev != 0 ) {
			if ( ifscope[iflev - 1] == CG_IFCHECK_LINE ) {
				goto finag;
			}
		}
	}
}


void CChspParser::CheckInternalIF( int opt )
{
	//		内蔵命令生成時チェック
	//
	if ( opt != 0 ) { // 'else'+offset
		if ( iflev == 0 ) {
			throw CGERROR_ELSE_WO_IF;
		}
		CheckCMDIF_Fin( 1 );
		CheckCMDIF_Set( 1 );
		return;
	}
	CheckCMDIF_Set( 0 ); // normal if
}


void CChspParser::CheckInternalListenerCMD( int opt )
{
	//		命令生成時チェック(命令+命令セット)
	//
	int i;
	int t;
	int o;
	if ( token.ttype != TK_OBJ ) {
		return;
	}
	i = symtab->lb->Search( token.cg_str.data() );
	if ( i < 0 ) {
		return;
	}
	t = symtab->lb->GetType( i );
	o = symtab->lb->GetOpt( i );
	if ( t != TYPE_PROGCMD ) {
		return;
	}
	if ( o != 0x001 && o != 0x000 ) {
		return; // not either gosub or goto
	}
	if ( o == 0x001 ) { // gosub
						// CG: writer->PutCS( t, o & 0xffff, 0 );
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
}


int CChspParser::CheckInternalProgCMD( int opt, int orgcs )
{
	//		内蔵プログラム命令生成時チェック
	//
	int i;
	switch ( opt ) {

	case 0x03: // repeat break
	case 0x06: // repeat continue
		if ( replev == 0 ) {
			if ( opt == 0x03 ) {
				throw CGERROR_BREAK_WO_REPEAT;
			}
			throw CGERROR_CONT_WO_REPEAT;
		}
		i = repend[replev];
		if ( i == -1 ) {
			// CG: i = writer->PutOT( -1 );
			repend[replev] = i;
		}
		// CG: writer->PutCS( TK_LABEL, i, 0 );
		break;
	case 0x04: // repeat start
	case 0x0b: // (foreach)
		if ( replev == CG_REPLEV_MAX ) {
			throw CGERROR_REPEAT_OVERFLOW;
		}
		replev++;
		i = repend[replev];
		if ( i == -1 ) {
			// CG: i = writer->PutOT( -1 );
			repend[replev] = i;
		}
		// CG: writer->PutCS( TK_LABEL, i, 0 );
		if ( opt == 0x0b ) {
			// CG: writer->PutCS( TYPE_PROGCMD, 0x0c, EXFLG_1 );
			// CG: writer->PutCS( TK_LABEL, i, 0 );
		}
		break;
	case 0x05: // repeat end
		if ( replev == 0 ) {
			throw CGERROR_LOOP_WO_REPEAT;
		}
		i = repend[replev];
		if ( i != -1 ) {
			// CG: writer->SetOT( i, writer->GetCS() );
			repend[replev] = -1;
		}
		replev--;
		break;
	case 0x11: // stop
		// CG: i = writer->PutOT( orgcs );
		// CG: writer->PutCS( TYPE_PROGCMD, 0, EXFLG_1 );
		// CG: writer->PutCS( TYPE_LABEL, i, 0 );
		break;
	case 0x19: // on
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		CalcCG( 0 ); // 式の評価
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_SYNTAX;
		}
		i = symtab->lb->Search( token.cg_str.data() );
		if ( i < 0 ) {
			throw CGERROR_SYNTAX;
		}
		{
			int labelType = symtab->lb->GetType( i );
			int labelOpt = symtab->lb->GetOpt( i );
			if ( ( labelType == TYPE_PROGCMD ) && ( labelOpt == 0 || labelOpt == 1 ) ) {
				// goto or gosub
				// CG: writer->PutCS( labelType, labelOpt, EXFLG_2 );
			} else {
				throw CGERROR_SYNTAX;
			}
		}
		break;

	case 0x08: // await
		//	await命令の出現をカウントする(HEDINFO_NOMMTIMER自動設定のため)
		if ( compopt->hed_autoopt_timer >= 0 ) {
			compopt->hed_autoopt_timer++;
		}
		break;

	case 0x09: // dim
	case 0x0a: // sdim
	case 0x0d: // dimtype
	case 0x0e: // dup
	case 0x0f: // dupptr
	{
		auto firstSymbolName = lexer.GetSymbolCG( lexer.cg_ptr );
		if ( firstSymbolName.empty() || ( isdigit( firstSymbolName[0] ) != 0 ) ) {
			break;
		}
		i = SetVarsFixed( firstSymbolName, cg_defvarfix );
		//	変数の初期化フラグをセットする
		// CG: symtab->lb->SetInitFlag( i, LAB_INIT_DONE );
		// CG* GenerateLabelListAndTag( i, LABBUF_FLAG_VAR );
		symtab->lb->SetSkipLabList( i ); // 次回のラベル参照リスト生成をスキップする
		// logger->Mesf( "#initflag set [%s]", token.cg_str );
		return 1;
	}

	case 0x20: // strexchange
		//	strexchange命令の出現をカウントする(ヘッダ自動設定のため)
		if ( compopt->hed_autoopt_strexchange >= 0 ) {
			compopt->hed_autoopt_strexchange++;
		}
		break;
	}
	return 0;
}


void CChspParser::GenerateCodeCMD( int id )
{
	//		HSP3Codeを展開する(コマンド)
	//		(idはlabel ID)
	//
	int t;
	int opt;
	int orgcs;
	t = symtab->lb->GetType( id );
	opt = symtab->lb->GetOpt( id );
	BeginAstStatement( CG_LASTCMD_CMD, t, symtab->lb->GetName( id ) );
	// CG: orgcs = writer->GetCS();
	// CG: writer->PutCSSymbol( id, EXFLG_1 );

	int labtype = LABBUF_FLAG_CMD;
	switch ( t ) {
	case TYPE_PROGCMD:
		CheckInternalProgCMD( opt, orgcs );
		break;
	case TYPE_CMPCMD:
		CheckInternalIF( opt );
		break;
	case TYPE_INTCMD:
	case TYPE_EXTCMD:
		break;
	case TYPE_MODCMD:
		labtype = LABBUF_FLAG_FUNC;
		break;
	default:
		labtype = LABBUF_FLAG_EXCMD;
		break;
	}

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( ( opt & 0x10000 ) != 0 ) {
		CheckInternalListenerCMD( opt );
	}

	GenerateCodePRM();
	if ( current_stmt != nullptr && current_stmt->kind == chspv3::ChspV3AstStmtKind::Loop ) {
		current_stmt = nullptr;
	}
	cg_lastcmd = CG_LASTCMD_CMD;
	cg_lasttype = t;
	cg_lastval = opt;

	// CG: GenerateLabelListAndTagRef( id, labtype );
}


void CChspParser::GenerateCodeLET( int id, bool first )
{
	//		HSP3Codeを展開する(代入)
	//		(idはlabel ID)
	//
	int op;
	int t;

	t = symtab->lb->GetType( id );
	BeginAstStatement( CG_LASTCMD_LET, t, symtab->lb->GetName( id ) );
	if ( current_stmt != nullptr ) {
		auto lhs = std::make_unique<chspv3::ChspV3AstExpr>();
		lhs->kind = chspv3::ChspV3AstExprKind::Identifier;
		lhs->line = lexer.cg_orgline;
		lhs->token_kind = t;
		lhs->text = symtab->lb->GetName( id );
		current_stmt->lhs = std::move( lhs );
	}
	if ( ( t == TYPE_XLABEL ) || ( t == TYPE_LABEL ) ) {
		throw CGERROR_LABELNAME;
	}

	if ( first ) {
		// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_VAR );
	} else {
		// CG: GenerateLabelListAndTagRef( id, LABBUF_FLAG_VAR );
	}

	//
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( ( token.ttype == TK_NONE ) && ( token.val == 0x65 ) ) { // ->が続いているか?
		// CG: writer->PutCS( TYPE_PROGCMD, 0x1a, EXFLG_1 );			 // 'mcall'コマンドに置き換える
		// CG: writer->PutCS( t, symtab->lb->GetOpt( id ), 0 );		 // 変数パラメーター
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		GenerateCodeMethod(); // パラメーター展開
		return;
	}

	// CG: writer->PutCS( t, symtab->lb->GetOpt( id ), EXFLG_1 ); // 通常の変数代入
	GenerateCodePRMF4( t, current_stmt != nullptr ? &current_stmt->lhs : nullptr ); // 構造体/配列のチェック

	if ( token.ttype != TK_NONE ) {
		throw CGERROR_SYNTAX;
	}

	op = token.val;
	// PutCS( TK_NONE, op, 0 );
	texflag = 0;
	CalcCG_regmark( op );
	if ( current_stmt != nullptr ) {
		current_stmt->token_kind = op;
	}

	cg_lastcmd = CG_LASTCMD_LET;
	cg_lastval = op;

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	switch ( op ) {
	case '+': // '++'
	case '-': // '--'
		if ( token.ttype >= TK_SEPARATE ) {
			return;
		}
		if ( token.ttype == TK_NONE ) {
			if ( token.val == op ) {
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
				if ( token.ttype >= TK_SEPARATE ) {
					return;
				}
				throw CGERROR_SYNTAX;
			}
		}
		break;
	case '=': // 変数=prm
		GenerateCodePRM();
		return;
	default:
		break;
	}

	if ( ( token.ttype == TK_NONE ) && ( token.val == '=' ) ) {
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	}
	GenerateCodePRM();
}


void CChspParser::GenerateCodePP_regcmd()
{
	//		HSP3Codeを展開する(regcmd)
	//
	char cmd[1024];
	char cmd2[1024];
	cg_pptype = cg_typecnt;
	cmd[0] = 0;

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	switch ( token.ttype ) {
	case TK_STRING:
		strcpy( cmd, token.cg_str.c_str() );
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_NONE ) {
			throw CGERROR_PP_NO_REGCMD;
		}
		if ( token.val != ',' ) {
			throw CGERROR_PP_NO_REGCMD;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_STRING ) {
			throw CGERROR_PP_NO_REGCMD;
		}
		strcpy( cmd2, token.cg_str.c_str() );

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype == TK_NONE ) {
			if ( token.val != ',' ) {
				throw CGERROR_PP_NO_REGCMD;
			}
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			if ( token.ttype != TK_NUM ) {
				throw CGERROR_PP_NO_REGCMD;
			}
			cg_varhpi += token.val;
		}

		// CG: writer->PutHPI( HPIDAT_FLAG_TYPEFUNC, 0, cmd2, cmd );
		cg_typecnt++;
		break;
	case TK_NUM:
		// CG: writer->PutHPI( HPIDAT_FLAG_SELFFUNC, 0, "", "" );
		cg_pptype = token.val;
		break;
	case TK_NONE:
		if ( token.val == '*' ) {
			cg_typecnt++;
		}
		break;
	default:
		throw CGERROR_PP_NO_REGCMD;
	}
	// logger->Mesf( "#regcmd [%d][%s]",cg_pptype, cmd );
}


void CChspParser::GenerateCodePP_cmd()
{
	//		HSP3Codeを展開する(cmd)
	//
	int id;
	char cmd[1024];
	if ( cg_pptype < 0 ) {
		throw CGERROR_PP_NO_REGCMD;
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NO_REGCMD;
	}
	strcpy( cmd, token.cg_str.c_str() );

	// if ( ttype != TK_NONE ) throw CGERROR_PP_NO_REGCMD;
	// if ( val != ',' ) throw CGERROR_PP_NO_REGCMD;
	// token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_NUM ) {
		throw CGERROR_PP_NO_REGCMD;
	}
	id = token.val;

	id = symtab->lb->Regist( cmd, cg_pptype, id, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
	// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_EXCMD );
	//  logger->Mesf( "#%x:%d [%s]",cg_pptype, id, cmd );
}


void CChspParser::GenerateCodePP_uselib()
{
	//		HSP3Codeを展開する(uselib)
	//
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	cg_libname[0] = 0;
	if ( token.ttype == TK_STRING ) {
		strncpy( cg_libname, token.cg_str.c_str(), 1023 );
	} else if ( token.ttype < TK_VOID ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	cg_libmode = CG_LIBMODE_DLLNEW;
}


void CChspParser::GenerateCodePP_usecom()
{
	//		HSP3Codeを展開する(usecom)
	//
	int i;
	int prmid;
	char libname[1024];
	char clsname[128];
	char iidname[128];

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	strncpy( libname, token.cg_str.c_str(), 1023 );

	i = symtab->lb->Search( libname );
	if ( i >= 0 ) {
		CG_MesLabelDefinition( i );
		throw CGERROR_PP_ALREADY_USE_PARAM;
	}

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_STRING ) {
		throw CGERROR_PP_BAD_IMPORT_IID;
	}
	strncpy( iidname, token.cg_str.c_str(), 127 );

	*clsname = 0;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype < TK_EOL ) {
		if ( token.ttype != TK_STRING ) {
			throw CGERROR_PP_BAD_IMPORT_IID;
		}
		strncpy( clsname, token.cg_str.c_str(), 127 );
	}

	// CG: cg_libindex = writer->PutLIB( LIBDAT_FLAG_COMOBJ, iidname );
	if ( cg_libindex < 0 ) {
		throw CGERROR_PP_BAD_IMPORT_IID;
	}

	// CG: writer->SetLIBIID( cg_libindex, clsname );
	cg_libmode = CG_LIBMODE_COM;

	/*CG:
	writer->PutStructStart();
	prmid = writer->PutStructEndDll( "*", cg_libindex, STRUCTPRM_SUBID_COMOBJ, -1 );
	*/
	prmid = 0xff; // CG:
	int id = symtab->lb->Regist( libname, TYPE_DLLCTRL, prmid | TYPE_OFFSET_COMOBJ, lexer.cg_orgfilefull.c_str(),
								 lexer.cg_orgline );
	// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_EXCMD );

	// logger->Mesf( "#usecom %s [%s][%s]",libname,clsname,iidname );
}


void CChspParser::GenerateCodePP_func( int deftype )
{
	//		HSP3Codeを展開する(func)
	//
	int warn;
	int i;
	int t;
	int subid;
	int otflag;
	int ref;
	char fbase[1024];
	char fname[1024];

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	strncpy( fbase, token.cg_str.c_str(), 1023 );

	ref = -1;
	/*CG:
	if ( ( ( compopt->hed_cmpmode & CMPMODE_OPTCODE ) != 0 ) &&
		 ( symtab->tmp_lb != nullptr ) ) { // プリプロセス情報から最適化を行なう
		i = symtab->tmp_lb->Search( fbase );
		if ( i >= 0 ) {
			ref = symtab->tmp_lb->GetReference( i );
			// logger->Mesf( "#func %s [use%d]", fbase, ref );
		}
	}
	*/

	warn = 0;
	otflag = deftype;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( token.ttype == TK_OBJ ) {
		if ( token.cg_str == "onexit" ) {
			otflag |= STRUCTDAT_OT_CLEANUP;
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		}
	}

	if ( ref == 0 && ( otflag & STRUCTDAT_OT_CLEANUP ) == 0 ) {
		if ( ( compopt->hed_cmpmode & CMPMODE_OPTINFO ) != 0 ) {
#ifdef JPNMSG
			logger->Mesf( "#未使用の外部DLL関数の登録を削除しました %s", fbase );
#else
			logger->Mesf( "#Delete func %s", fbase );
#endif
		}
		// CG: GenerateLabelListAndTag( fbase, LABBUF_FLAG_EXCMD );
		return;
	}

	if ( cg_libmode == CG_LIBMODE_DLLNEW ) { // 初回はDLL名を登録する
		// CG: cg_libindex = writer->PutLIB( LIBDAT_FLAG_DLL, cg_libname );
		cg_libmode = CG_LIBMODE_DLL;
	}
	if ( cg_libmode != CG_LIBMODE_DLL ) {
		throw CGERROR_PP_NO_USELIB;
	}

	switch ( token.ttype ) {
	case TK_OBJ:
		sprintf( fname, "_%s@16", token.cg_str.c_str() );
		warn = 1;
		break;
	case TK_STRING:
		strncpy( fname, token.cg_str.c_str(), 1023 );
		break;
	case TK_NONE:
		if ( token.val == '*' ) {
			break;
		}
		throw CGERROR_PP_BAD_IMPORT_NAME;
	default:
		throw CGERROR_PP_BAD_IMPORT_NAME;
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	// CG: writer->PutStructStart();
	if ( token.ttype == TK_NUM ) {
		int p1;
		int p2;
		int p3;
		int p4;
		int c1;
		warn = 1;
		p1 = p2 = p3 = p4 = MPTYPE_INUM;
		c1 = token.val & 3;
		if ( c1 == 1 ) {
			p1 = MPTYPE_PVARPTR;
		}
		if ( c1 == 2 ) {
			p1 = MPTYPE_PBMSCR;
		}
		if ( c1 == 3 ) {
			if ( ( token.val & 0x80 ) == 0 ) {
				throw CGERROR_PP_INCOMPATIBLE_IMPORT;
			}
			p1 = MPTYPE_PPVAL;
		}
		if ( ( token.val & 4 ) != 0 ) {
			p2 = MPTYPE_LOCALSTRING;
		}
		if ( ( token.val & 0x10 ) != 0 ) {
			p4 = MPTYPE_PTR_REFSTR;
		}
		if ( ( token.val & 0x20 ) != 0 ) {
			p4 = MPTYPE_PTR_DPMINFO;
		}
		if ( ( token.val & 0x100 ) != 0 ) {
			otflag |= STRUCTDAT_OT_CLEANUP;
		}
		if ( ( token.val & 0x200 ) != 0 ) {
			if ( ( token.val & 3 ) != 2 ) {
				throw CGERROR_PP_INCOMPATIBLE_IMPORT;
			}
			p1 = MPTYPE_PTR_EXINFO;
			p2 = p3 = MPTYPE_NULLPTR;
			if ( ( token.val & 0x30 ) == 0 ) {
				p4 = MPTYPE_NULLPTR;
			}
		}
		//		if ( token.val & 0x220 ) throw CGERROR_PP_INCOMPATIBLE_IMPORT;

		//		logger->Mesf("#oldfunc %d,%d,%d,%d",p1,p2,p3,p4);

		// CG: writer->PutStructParam( p1, STRUCTPRM_SUBID_STID );
		// CG: writer->PutStructParam( p2, STRUCTPRM_SUBID_STID );
		// CG: writer->PutStructParam( p3, STRUCTPRM_SUBID_STID );
		// CG: writer->PutStructParam( p4, STRUCTPRM_SUBID_STID );

	} else {
		while ( true ) {
			if ( token.ttype >= TK_EOL ) {
				break;
			}
			if ( token.ttype != TK_OBJ ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			t = lexer.GetParameterFuncTypeCG( token.cg_str );
			if ( t == MPTYPE_NONE ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			// CG: writer->PutStructParam( t, STRUCTPRM_SUBID_STID );
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

			if ( token.ttype >= TK_EOL ) {
				break;
			}
			if ( token.ttype != TK_NONE ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			if ( token.val != ',' ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		}
	}

	i = symtab->lb->Search( fbase );
	if ( i >= 0 ) {
		CG_MesLabelDefinition( i );
		throw CGERROR_PP_ALREADY_USE_FUNCNAME;
	}
	subid = STRUCTPRM_SUBID_DLL;
	if ( warn != 0 ) {
		subid = STRUCTPRM_SUBID_OLDDLL;
		// logger->Mesf( "Warning:Old func expression [%s]", fbase );
	}
	// CG: i = writer->PutStructEndDll( fname, cg_libindex, subid, otflag );
	int id = symtab->lb->Regist( fbase, TYPE_DLLFUNC, i, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
	// CG GenerateLabelListAndTag( id, LABBUF_FLAG_EXCMD );

	// logger->Mesf( "#func [%s][%s][%d]",fbase, fname, i );
}

void CChspParser::GenerateCodePP_comfunc()
{
	//		HSP3Codeを展開する(comfunc)
	//
	int i;
	int t;
	int subid;
	int imp_index;
	char fbase[1024];

	if ( cg_libmode != CG_LIBMODE_COM ) {
		throw CGERROR_PP_NO_USECOM;
	}

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	strncpy( fbase, token.cg_str.c_str(), 1023 );

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( token.ttype != TK_NUM ) {
		throw CGERROR_PP_BAD_IMPORT_INDEX;
	}
	imp_index = token.val;

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	// CG: writer->PutStructStart();
	// CG: writer->PutStructParam( MPTYPE_IOBJECTVAR, STRUCTPRM_SUBID_STID );

	while ( true ) {
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		t = lexer.GetParameterFuncTypeCG( token.cg_str );
		if ( t == MPTYPE_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		// CG: writer->PutStructParam( t, STRUCTPRM_SUBID_STID );
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( token.val != ',' ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	}

	i = symtab->lb->Search( fbase );
	if ( i >= 0 ) {
		CG_MesLabelDefinition( i );
		throw CGERROR_PP_ALREADY_USE_TAGNAME;
	}
	subid = STRUCTPRM_SUBID_COMOBJ;
	// CG: i = writer->PutStructEndDll( "*", cg_libindex, subid, imp_index );
	int id = symtab->lb->Regist( fbase, TYPE_DLLCTRL, i | TYPE_OFFSET_COMOBJ, lexer.cg_orgfilefull.c_str(),
								 lexer.cg_orgline );
	// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_EXCMD );

	// logger->Mesf( "#comfunc [%s][%d][%d]",fbase, imp_index, i );
}

#define GET_FI_SIZE() ( (int)( writer->fi_buf->GetSize() / sizeof( HED_STRUCTDAT ) ) )
#define GET_FI( n ) ( ( (HED_STRUCTDAT *)writer->fi_buf->GetBuffer() ) + ( n ) )
#define STRUCTDAT_INDEX_DUMMY ( (short)0x8000 )


void CChspParser::GenerateCodePP_deffunc0( int is_command )
{
	//		HSP3Codeを展開する(deffunc / defcfunc)
	//
	int i;
	int t;
	int ot;
	int prmid;
	int subid;
	int index;
	int funcflag;
	int regflag;
	int prep;
	char funcname[1024];
	STRUCTPRM *prm;
	HED_STRUCTDAT *st;

	prep = 0;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}

	if ( ( is_command != 0 ) && ( token.cg_str == "prep" ) ) { // プロトタイプ宣言
		prep = 1;
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_NAMEREQUIRED;
		}
	}

	strncpy( funcname, token.cg_str.c_str(), 1023 );

	for ( i = 0; i < cg_localcur; i++ ) {
		symtab->lb->SetFlag( cg_localstruct[i], -1 ); // 以前に指定されたパラメーター名を削除する
	}
	cg_localcur = 0;
	funcflag = 0;
	regflag = 1;

	index = -1;
	int label_id = symtab->lb->Search( funcname );
	if ( label_id >= 0 ) {
		if ( symtab->lb->GetType( label_id ) != TYPE_MODCMD ) {
			CG_MesLabelDefinition( label_id );
			throw CGERROR_PP_ALREADY_USE_FUNC;
		}
		index = symtab->lb->GetOpt( label_id );
		/*CG:
		if ( index >= 0 && GET_FI( index )->index != STRUCTDAT_INDEX_DUMMY ) {
			CG_MesLabelDefinition( label_id );
			throw CGERROR_PP_ALREADY_USE_FUNC;
		}
		*/
	}

	// CG: writer->PutStructStart();
	while ( true ) {
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}

		if ( ( is_command != 0 ) && ( token.cg_str == "onexit" ) ) {
			funcflag |= STRUCTDAT_FUNCFLAG_CLEANUP;
			break;
		}

		t = lexer.GetParameterTypeCG( token.cg_str );
		if ( t == MPTYPE_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( ( t == MPTYPE_MODULEVAR ) || ( t == MPTYPE_IMODULEVAR ) || ( t == MPTYPE_TMODULEVAR ) ) {
			//	モジュール名指定
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			if ( token.ttype != TK_OBJ ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			i = symtab->lb->Search( token.cg_str.data() );
			if ( i < 0 ) {
				throw CGERROR_PP_BAD_STRUCT;
			}
			if ( symtab->lb->GetType( i ) != TYPE_STRUCT ) {
				throw CGERROR_PP_BAD_STRUCT;
			}
			// CG: prm = (STRUCTPRM *)writer->mi_buf->GetBuffer();
			// CG: subid = prm[symtab->lb->GetOpt( i )].subid;
			//  logger->Mesf( "%s:struct%d", token.cg_str,subid );
			if ( t == MPTYPE_IMODULEVAR ) {
				/*CG:
				if ( prm[symtab->lb->GetOpt( i )].offset != -1 ) {
					throw CGERROR_PP_MODINIT_USED;
				}
				CG: prm[symtab->lb->GetOpt( i )].offset = GET_FI_SIZE();
				*/
				regflag = 0;
			}
			if ( t == MPTYPE_TMODULEVAR ) {
				/*CG:
				st = (HED_STRUCTDAT *)writer->fi_buf->GetBuffer();
				if ( st[subid].otindex != 0 ) {
					throw CGERROR_PP_MODTERM_USED;
				}
				st[subid].otindex = GET_FI_SIZE();
				*/
				regflag = 0;
			}
			// CG: prmid = writer->PutStructParam( t, subid );
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

		} else {
			// CG: prmid = writer->PutStructParam( t, STRUCTPRM_SUBID_STACK );
			prmid = 0xff; // CG
			// logger->Mesf( "%d:type%d",prmid,t );

			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			if ( token.ttype == TK_OBJ ) {
				//	引数のエイリアス
				i = symtab->lb->Search( token.cg_str.data() );
				if ( i >= 0 ) {
					CG_MesLabelDefinition( i );
					throw CGERROR_PP_ALREADY_USE_PARAM;
				}
				i = symtab->lb->Regist( token.cg_str.c_str(), TYPE_STRUCT, prmid, lexer.cg_orgfilefull.c_str(),
										lexer.cg_orgline );
				cg_localstruct[cg_localcur++] = i;
				token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			}
		}

		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( token.val != ',' ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
	}

	// CG: ot = writer->PutOT( writer->GetCS() );
	if ( index == -1 ) {
		// CG: index = GET_FI_SIZE();
		// CG: writer->fi_buf->PreparePtr( sizeof( HED_STRUCTDAT ) );
		if ( regflag != 0 ) {
			symtab->lb->Regist( funcname, TYPE_MODCMD, index, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
		}
	}
	if ( label_id >= 0 ) {
		symtab->lb->SetOpt( label_id, index );
	}
	int dat_index = is_command != 0 ? STRUCTDAT_INDEX_FUNC : STRUCTDAT_INDEX_CFUNC;
	// CG: writer->PutStructEnd( index, funcname, dat_index, ot, funcflag );
}


void CChspParser::GenerateCodePP_deffunc()
{
	GenerateCodePP_deffunc0( 1 );
}


void CChspParser::GenerateCodePP_defcfunc()
{
	GenerateCodePP_deffunc0( 0 );
}


void CChspParser::ClearLocalStructAliases()
{
	for ( int i = 0; i < cg_localcur; ++i ) {
		symtab->lb->SetFlag( cg_localstruct[i], -1 );
	}
	cg_localcur = 0;
}


void CChspParser::RegisterLocalStructAlias( const std::string &name )
{
	int id = symtab->lb->Search( const_cast<char *>( name.c_str() ) );
	if ( id >= 0 ) {
		CG_MesLabelDefinition( id );
		throw CGERROR_PP_ALREADY_USE_PARAM;
	}
	id = symtab->lb->Regist( name.c_str(), TYPE_STRUCT, 0, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
	cg_localstruct[cg_localcur++] = id;
}


void CChspParser::ParseChspSignatureType( bool allow_extended_type, chspv3::ChspV3AstParam *param_ast )
{
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_WRONG_PARAM_NAME;
	}

	const std::string type_name = token.cg_str;
	if ( param_ast != nullptr ) {
		param_ast->type_name = type_name;
		param_ast->base_type = type_name;
		param_ast->is_array = false;
		param_ast->is_local = false;
		param_ast->array_length = 0;
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	if ( type_name == "array" || type_name == "local" ) {
		if ( !( token.ttype == TK_NONE && token.val == '[' ) ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		const std::string base_type = token.cg_str;
		if ( param_ast != nullptr ) {
			param_ast->base_type = base_type;
			param_ast->type_name = type_name + "[" + base_type;
			param_ast->is_array = ( type_name == "array" );
			param_ast->is_local = ( type_name == "local" );
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype == TK_NONE && token.val == '[' ) {
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			if ( token.ttype != TK_NUM ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			if ( param_ast != nullptr ) {
				param_ast->type_name += "[" + std::to_string( token.val ) + "]";
				param_ast->array_length = token.val;
				param_ast->is_array = true;
			}
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
			if ( !( token.ttype == TK_NONE && token.val == ']' ) ) {
				throw CGERROR_PP_WRONG_PARAM_NAME;
			}
			token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		}
		if ( !( token.ttype == TK_NONE && token.val == ']' ) ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( param_ast != nullptr ) {
			param_ast->type_name += "]";
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		return;
	}

	if ( allow_extended_type && type_name == "float" ) {
		return;
	}

	if ( type_name == "int" || type_name == "double" || type_name == "str" || type_name == "var" ||
		 type_name == "label" ) {
		return;
	}

	throw CGERROR_PP_WRONG_PARAM_NAME;
}


void CChspParser::GenerateCodePP_chsp_deffunc0( int is_command )
{
	ClearLocalStructAliases();

	chspv3::ChspV3AstFunction function_ast;
	function_ast.line = lexer.cg_orgline;
	function_ast.token_kind = 0;
	function_ast.return_type = "void";

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( is_command == 0 ) {
		function_ast.return_type = token.cg_str;
		ParseChspSignatureType( false );
	}

	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	const std::string funcname = token.cg_str;
	function_ast.name = funcname;

	int label_id = symtab->lb->Search( const_cast<char *>( funcname.c_str() ) );
	if ( label_id >= 0 && symtab->lb->GetType( label_id ) != TYPE_MODCMD ) {
		CG_MesLabelDefinition( label_id );
		throw CGERROR_PP_ALREADY_USE_FUNC;
	}
	if ( label_id < 0 ) {
		label_id =
			symtab->lb->Regist( funcname.c_str(), TYPE_MODCMD, 0, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
	}

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	while ( token.ttype < TK_EOL ) {
		chspv3::ChspV3AstParam param_ast;
		param_ast.line = lexer.cg_orgline;
		param_ast.token_kind = token.ttype;
		ParseChspSignatureType( true, &param_ast );
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		param_ast.name = token.cg_str;
		current_function = &function_ast;
		current_function->params.push_back( std::move( param_ast ) );
		RegisterLocalStructAlias( token.cg_str );
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_NONE || token.val != ',' ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	}

	if ( current_module != nullptr ) {
		current_module->functions.emplace_back( std::move( function_ast ) );
		current_function = &current_module->functions.back();
	}
}


void CChspParser::GenerateCodePP_chsp_deffunc()
{
	GenerateCodePP_chsp_deffunc0( 1 );
}


void CChspParser::GenerateCodePP_chsp_defcfunc()
{
	GenerateCodePP_chsp_deffunc0( 0 );
}


void CChspParser::GenerateCodePP_chsp_module()
{
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_STRING ) {
		throw CGERROR_PP_SYNTAX;
	}
	chspv3::ChspV3AstModule module_ast;
	module_ast.line = lexer.cg_orgline;
	module_ast.token_kind = 0;
	module_ast.name = token.cg_str;

	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	while ( token.ttype < TK_EOL ) {
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_SYNTAX;
		}
		const std::string key = token.cg_str;

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_NONE || token.val != '=' ) {
			throw CGERROR_PP_SYNTAX;
		}

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_SYNTAX;
		}

		if ( key == "target" ) {
			if ( token.cg_str != "c" && token.cg_str != "plugin" ) {
				throw CGERROR_PP_SYNTAX;
			}
			module_ast.target = token.cg_str == "c" ? ChspNativeTarget::C : ChspNativeTarget::Plugin;
		} else {
			throw CGERROR_PP_SYNTAX;
		}

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	}

	ast_program.modules.emplace_back( std::move( module_ast ) );
	current_module = &ast_program.modules.back();
	current_function = nullptr;
}


void CChspParser::GenerateCodePP_chsp_module_end()
{
	ClearLocalStructAliases();
	current_function = nullptr;
	current_module = nullptr;
}


void CChspParser::GenerateCodePP_module()
{
	//		HSP3Codeを展開する(module)
	//
	int i;
	int ref;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	auto &modname = token.cg_str;

	/*CG
	if ( ( ( compopt->hed_cmpmode & CMPMODE_OPTCODE ) != 0 ) &&
		 ( symtab->tmp_lb != nullptr ) ) { // プリプロセス情報から最適化を行なう
		i = symtab->tmp_lb->Search( modname );
		if ( i >= 0 ) {
			ref = symtab->tmp_lb->GetReference( i );
			if ( ref == 0 ) {
				cg_flag = CG_FLAG_DISABLE;
				if ( ( compopt->hed_cmpmode & CMPMODE_OPTINFO ) != 0 ) {
#ifdef JPNMSG
					logger->Mesf( "#未使用のモジュールを削除しました %s", modname.c_str() );
#else
					logger->Mesf( "#Delete module %s", modname.c_str() );
#endif
				}
				return;
			}
		}
	}
	*/
}


void CChspParser::GenerateCodePP_struct()
{
	//		HSP3Codeを展開する(struct)
	//
	int i;
	int t;
	int prmid;
	char funcname[1024];
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
	if ( token.ttype != TK_OBJ ) {
		throw CGERROR_PP_NAMEREQUIRED;
	}
	strncpy( funcname, token.cg_str.c_str(), 1023 );
	i = symtab->lb->Search( funcname );
	if ( i >= 0 ) {
		CG_MesLabelDefinition( i );
		throw CGERROR_PP_ALREADY_USE_PARAM;
	}

	// CG: writer->PutStructStart();
	// CG: prmid = writer->PutStructParamTag(); // modinit用のTAG
	prmid = 0xff; // CG:
	symtab->lb->Regist( funcname, TYPE_STRUCT, prmid, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
	// logger->Mesf( "%d:%s",prmid, funcname );

	while ( true ) {
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		t = lexer.GetParameterStructTypeCG( token.cg_str );
		if ( t == MPTYPE_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		// CG: prmid = writer->PutStructParam( t, STRUCTPRM_SUBID_STID );
		//  logger->Mesf( "%d:type%d",prmid,t );

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}

		i = symtab->lb->Search( token.cg_str.data() );
		if ( i >= 0 ) {
			CG_MesLabelDefinition( i );
			throw CGERROR_PP_ALREADY_USE_PARAM;
		}
		symtab->lb->Regist( token.cg_str.c_str(), TYPE_STRUCT, prmid, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( token.val != ',' ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
	}
	// CG: writer->PutStructEnd( funcname, STRUCTDAT_INDEX_STRUCT, 0, 0 );
}


void CChspParser::GenerateCodePP_defvars( int fixedvalue )
{
	//		HSP3Codeを展開する(var,varint,vardouble,varstr)
	//
	int id;
	int prms;
	prms = 0;
	while ( true ) {
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_OBJ ) {
			throw CGERROR_WRONG_VARIABLE;
		}

		id = symtab->lb->Search( token.cg_str.data() );
		if ( id >= 0 ) {
			throw CGERROR_ALREADY_DEFINED_VARS;
		}
		id = SetVarsFixed( token.cg_str, fixedvalue );
		if ( symtab->lb->GetType( id ) != TYPE_VAR ) {
			throw CGERROR_WRONG_VARIABLE;
		}
		symtab->lb->SetInitFlag( id, LAB_INIT_DONE );
		// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_VAR );
		prms++;
		// logger->Mesf( "name:%s(%d) fixed:%d", token.cg_str, id, fixedvalue );

		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype >= TK_EOL ) {
			break;
		}
		if ( token.ttype != TK_NONE ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
		if ( token.val != ',' ) {
			throw CGERROR_PP_WRONG_PARAM_NAME;
		}
	}

	if ( prms == 0 ) {
		cg_defvarfix = fixedvalue;
	}
}


int CChspParser::SetVarsFixed( const std::string &varname, int fixedvalue )
{
	//		変数の固定型を設定する
	//
	int id;
	id = symtab->lb->Search( (char *)varname.c_str() );
	if ( id < 0 ) {
		id = symtab->lb->Regist( varname.c_str(), TYPE_VAR, cg_valcnt, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
		cg_valcnt++;
	}
	if ( fixedvalue != LAB_TYPEFIX_NONE ) {
		if ( symtab->lb->GetForceType( id ) != LAB_TYPEFIX_NONE ) {
			throw CGERROR_FORCED_VARIABLE;
		}
		symtab->lb->SetForceType( id, fixedvalue );
	}
	return id;
}


void CChspParser::GenerateCodePP( const char *buf )
{
	//		HSP3Codeを展開する(プリプロセスコマンド)
	//
	int i;
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT ); // 最初の'#'を読み飛ばし
	if ( *lexer.cg_ptr != lexer.PickNextCodeCG() ) {
		// preprocesser command "#"
		throw CGERROR_UNKNOWN;
	}
	token = lexer.GetTokenCG( GETTOKEN_DEFAULT );

	// logger->Mesf( "token %d: str:'%s' val:'%d'", token.ttype, token.cg_str.c_str(), (int) token.val );

	if ( token.ttype == TK_NONE ) { // プリプロセッサから渡される行情報
		if ( token.val != '#' ) {
			throw CGERROR_UNKNOWN;
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype != TK_NUM ) {
			throw CGERROR_UNKNOWN;
		}
		lexer.cg_orgline = token.val;
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		if ( token.ttype == TK_STRING ) {
			lexer.cg_orgfilefull = token.cg_str;
			char temp_orgfile[HSP_MAX_PATH];
			getpath( lexer.cg_orgfilefull.data(), temp_orgfile, 8 );
			lexer.cg_orgfile = temp_orgfile;
			if ( compopt->cg_debug() ) {
				// CG: i = writer->PutDSBuf( token.cg_str.c_str() );
				// CG: writer->PutDI( 254, i, lexer.cg_orgline ); // ファイル名をデバッグ情報として登録
			}
		} else {
			if ( compopt->cg_debug() ) {
				// CG: writer->PutDI( 254, 0, lexer.cg_orgline ); // ラインだけをデバッグ情報として登録
			}
		}
		// logger->Mesf( "#%d [%s]",lexer.cg_orgline, token.cg_str );
		return;
	}

	if ( token.ttype != TK_OBJ ) { // その他はエラー
		throw CGERROR_PP_SYNTAX;
	}

	if ( token.cg_str == "global" ) {
		cg_flag = CG_FLAG_ENABLE;
		return;
	}

	if ( cg_flag != CG_FLAG_ENABLE ) { // 最適化による出力抑制
		return;
	}

	if ( token.cg_str == "regcmd" ) {
		GenerateCodePP_regcmd();
		return;
	}
	if ( token.cg_str == "cmd" ) {
		GenerateCodePP_cmd();
		return;
	}
	if ( token.cg_str == "uselib" ) {
		GenerateCodePP_uselib();
		return;
	}
	if ( token.cg_str == "func" ) {
		GenerateCodePP_func( STRUCTDAT_OT_STATEMENT | STRUCTDAT_OT_FUNCTION );
		return;
	}
	if ( token.cg_str == "cfunc" ) {
		GenerateCodePP_func( STRUCTDAT_OT_FUNCTION );
		return;
	}
	if ( token.cg_str == "deffunc" ) {
		GenerateCodePP_deffunc();
		return;
	}
	if ( token.cg_str == "defcfunc" ) {
		GenerateCodePP_defcfunc();
		return;
	}
	if ( token.cg_str == "chsp_deffunc" ) {
		GenerateCodePP_chsp_deffunc();
		return;
	}
	if ( token.cg_str == "chsp_defcfunc" ) {
		GenerateCodePP_chsp_defcfunc();
		return;
	}
	if ( token.cg_str == "chsp_module" ) {
		GenerateCodePP_chsp_module();
		return;
	}
	if ( token.cg_str == "chsp_module_end" ) {
		GenerateCodePP_chsp_module_end();
		return;
	}
	if ( token.cg_str == "module" ) {
		GenerateCodePP_module();
		return;
	}
	if ( token.cg_str == "chsp_end" ) {
		ClearLocalStructAliases();
		current_function = nullptr;
		return;
	}
	if ( token.cg_str == "struct" ) {
		GenerateCodePP_struct();
		return;
	}
	if ( token.cg_str == "usecom" ) {
		GenerateCodePP_usecom();
		return;
	}
	if ( token.cg_str == "comfunc" ) {
		GenerateCodePP_comfunc();
		return;
	}
	if ( token.cg_str == "var" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_NONE );
		return;
	}
	if ( token.cg_str == "varint" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_INT );
		return;
	}
	if ( token.cg_str == "varlabel" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_LABEL );
		return;
	}
	if ( token.cg_str == "varstr" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_STR );
		return;
	}
	if ( token.cg_str == "vardouble" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_DOUBLE );
		return;
	}
	if ( token.cg_str == "varmod" ) {
		GenerateCodePP_defvars( LAB_TYPEFIX_STRUCT );
		return;
	}
}


int CChspParser::GenerateCodeSub()
{
	//		文字列(１行単位)からHSP3Codeを展開する
	//		(エラー発生時は例外が発生します)
	//
	cg_errline = lexer.line;
	cg_lastcmd = CG_LASTCMD_NONE;

	// logger->Mesf( "%s(%d): %s", lexer.cg_orgfile.c_str(), lexer.cg_orgline, lexer.cg_ptr );

	if ( lexer.cg_ptr == nullptr ) {
		return TK_EOF;
	}

	RecordSourceLine( lexer.cg_ptr );

	if ( *lexer.cg_ptr == '#' ) {
		GenerateCodePP( lexer.cg_ptr );
		return TK_EOL;
	}

	if ( cg_flag != CG_FLAG_ENABLE ) {
		return TK_EOL; // 最適化による出力抑制
	}

	token = lexer.GetTokenCG( GETTOKEN_LABEL );
	if ( token.ttype >= TK_SEPARATE ) {
		return token.ttype;
	}

	switch ( token.ttype ) {
	case TK_OBJ: {
		cg_lastcmd = CG_LASTCMD_LET;
		int i = symtab->lb->Search( token.cg_str.data() );
		if ( i < 0 ) {
			// logger->Mesf( "[%s][%d]", token.cg_str, cg_valcnt );
			i = SetVarsFixed( token.cg_str, cg_defvarfix );
			// CG: symtab->lb->SetInitFlag( i, LAB_INIT_DONE ); //	変数の初期化フラグをセットする
			GenerateCodeLET( i, true );
		} else {
			int t = symtab->lb->GetType( i );
			switch ( t ) {
			case TYPE_VAR:
			case TYPE_STRUCT:
				GenerateCodeLET( i );
				break;
			case TYPE_LABEL:
			case TYPE_XLABEL:
				throw CGERROR_LABELNAME;
				break;
			default:
				GenerateCodeCMD( i );
				break;
			}
		}
		//			sprintf( tmp,"#obj:%s (%d)", token.cg_str,i );
		//			logger->Mes( tmp );
		break;
	}
	case TK_LABEL: {
		// logger->Mesf( "#lab:%s", token.cg_str );
		if ( token.cg_str[0] == '@' ) {
			token.cg_str = "@l" + std::to_string( cg_locallabel ); // local label
			cg_locallabel++;
		}

		int i = symtab->lb->Search( token.cg_str.data() );
		if ( i >= 0 ) {
			LABOBJ *lab;
			lab = symtab->lb->GetLabel( i );
			if ( lab->type != TYPE_XLABEL ) {
				throw CGERROR_LABELEXIST;
			}
			symtab->lb->SetDefinition( i, lexer.cg_orgfilefull.c_str(), lexer.cg_orgline );
			// CG: GenerateLabelListAndTag( i, LABBUF_FLAG_LABEL );
			// CG: writer->SetOT( symtab->lb->GetOpt( i ), writer->GetCS() );
			lab->type = TYPE_LABEL;
		} else {
			/*CG
			i = symtab->lb->Regist( token.cg_str, TYPE_LABEL, writer->ot_buf->GetSize() / sizeof( int ),
									lexer.cg_orgfilefull, lexer.cg_orgline );
			*/
			i = symtab->lb->Regist( token.cg_str.c_str(), TYPE_LABEL, 1 );
			// CG: GenerateLabelListAndTag( i, LABBUF_FLAG_LABEL );
			// CG: writer->PutOT( writer->GetCS() );
		}
		token = lexer.GetTokenCG( GETTOKEN_DEFAULT );
		break;
	}
	default:
		throw CGERROR_SYNTAX;
	}
	//	}

	if ( token.ttype < TK_SEPARATE ) {
		throw CGERROR_SYNTAX;
	}
	return token.ttype;
}


int CChspParser::GenerateCodeBlock()
{
	//		プロック単位でHSP3Codeを展開する
	//		(エラー発生時は例外が発生します)
	//
	int res;
	int id;
	int ff;
	char a1;
	res = GenerateCodeSub();
	if ( res == TK_EOF ) {
		return res;
	}
	if ( res == TK_EOL ) {
		lexer.NextLine();
		if ( iflev != 0 ) {
			if ( ifscope[iflev - 1] == CG_IFCHECK_LINE ) {
				CheckCMDIF_Fin( 0 ); // 'if' jump support
			}
		}
		if ( compopt->cg_debug() ) {
			// CG: writer->PutDI();
		}
	}
	if ( res == TK_SEPARATE ) {
		a1 = token.cg_str[0];
		if ( a1 == '{' ) { // when '{'
			if ( iflev == 0 ) {
				throw CGERROR_BLOCKEXP;
			}
			if ( ifscope[iflev - 1] == CG_IFCHECK_SCOPE ) {
				throw CGERROR_BLOCKEXP;
			}
			ifscope[iflev - 1] = CG_IFCHECK_SCOPE;
		} else if ( a1 == '}' ) { // when '}'
			if ( iflev == 0 ) {
				throw CGERROR_BLOCKEXP;
			}
			if ( ifscope[iflev - 1] != CG_IFCHECK_SCOPE ) {
				throw CGERROR_BLOCKEXP;
			}

			ff = 0;
			auto np = lexer.GetTokenCG( lexer.cg_ptr, GETTOKEN_DEFAULT );
			token = np.second;

			if ( token.ttype == TK_EOL ) { // 次行のコマンドがelseかどうか調べる
				if ( lexer.cg_wp != nullptr ) {
					auto sym = lexer.GetSymbolCG( lexer.cg_wp );
					if ( !sym.empty() ) {
						id = symtab->lb->Search( sym.data() );
						if ( id >= 0 ) {
							if ( ( symtab->lb->GetType( id ) == TYPE_CMPCMD ) && ( symtab->lb->GetOpt( id ) == 1 ) ) {
								ff = 1;
							}
						}
					}
				}
			} else if ( token.ttype == TK_OBJ ) { // 次のコマンドがelseかどうか調べる
				id = symtab->lb->Search( token.cg_str.data() );
				if ( id >= 0 ) {
					if ( ( symtab->lb->GetType( id ) == TYPE_CMPCMD ) && ( symtab->lb->GetOpt( id ) == 1 ) ) {
						// ifscope[iflev-1] = CG_IFCHECK_LINE;					// line scope on
						ff = 1;
					}
				}
			}
			if ( ff == 0 ) {
				CheckCMDIF_Fin( 0 );
			}
		}
	}
	return res;
}


void CChspParser::RegisterFuncLabels()
{
	//		プリプロセス時のラベル情報から関数を定義
	//
	if ( symtab->tmp_lb == nullptr ) {
		return;
	}
	int len = symtab->tmp_lb->GetCount();
	for ( int i = 0; i < len; i++ ) {
		if ( symtab->tmp_lb->GetType( i ) == LAB_TYPE_PPMODFUNC && symtab->tmp_lb->GetFlag( i ) >= 0 ) {
			auto name = symtab->tmp_lb->GetName( i );
			if ( symtab->lb->Search( name ) >= 0 ) {
				throw CGERROR_PP_ALREADY_USE_FUNC;
			}
			LABOBJ *lab = symtab->tmp_lb->GetLabel( i );
			int id = symtab->lb->Regist( name, TYPE_MODCMD, -1, lab->def_file, lab->def_line );
			// CG: GenerateLabelListAndTag( id, LABBUF_FLAG_FUNC );
			symtab->lb->SetData2( id, (char *)&i, sizeof i );
		}
	}
}


void CChspParser::ResetGenerator( const char *ptr )
{
	cg_flag = CG_FLAG_ENABLE;
	cg_valcnt = 0;
	cg_typecnt = HSP3_TYPE_USER;
	cg_pptype = -1;
	cg_iflev = 0;
	lexer.line = 0;
	lexer.cg_orgline = 0;
	lexer.cg_wp = ptr;
	lexer.NextLine();
	lexer.cg_orgfile.clear();
	lexer.cg_orgfilefull.clear();
	cg_libindex = -1;
	cg_libmode = CG_LIBMODE_NONE;
	cg_localcur = 0;
	cg_locallabel = 0;
	cg_varhpi = 0;
	cg_defvarfix = LAB_TYPEFIX_NONE;

	iflev = 0;
	replev = 0;
	for ( int &a : repend ) {
		a = -1;
	}
	ResetAstBuilder();
}


int CChspParser::GenerateCodeMain( CMemBuf *buf )
{
	//		ソースをHSP3Codeに展開する
	//
	ResetGenerator( const_cast<const char *>( buf->GetBuffer() ) );
	int a;

	try {
		RegisterFuncLabels();

		while ( true ) {
			if ( GenerateCodeBlock() == TK_EOF ) {
				break;
			}
		}

		cg_errline = -1; // エラーの行番号は該当なし

		//		コンパイル後の後始末チェック
		if ( replev != 0 ) {
			throw CGERROR_LOOP_NOTFOUND;
		}

		//		ラベル未処理チェック
		int errend;
		errend = 0;
		for ( a = 0; a < symtab->lb->GetCount(); a++ ) {
			if ( symtab->lb->GetType( a ) == TYPE_XLABEL ) {
#ifdef JPNMSG
				logger->Mesf( "#ラベルの定義が存在しません [%s]", symtab->lb->GetName( a ) );
#else
				logger->Mesf( "#Label definition not found [%s]", symtab->lb->GetName( a ) );
#endif
				errend++;
			}
		}

		//		関数未処理チェック
		/*CG:
		for ( a = 0; a < GET_FI_SIZE(); a++ ) {
			if ( GET_FI( a )->index == STRUCTDAT_INDEX_DUMMY ) {
#ifdef JPNMSG
				logger->Mesf( "#関数が定義されていません [%s]", symtab->lb->GetName( GET_FI( a )->otindex ) );
#else
				logger->Mesf( "#Function not found [%s]", symtab->lb->GetName( GET_FI( a )->otindex ) );
#endif
				errend++;
			}
		}
		*/

		//      ブレース対応チェック
		if ( iflev > 0 ) {
#ifdef JPNMSG
			logger->Mesf( "#波括弧が閉じられていません" );
#else
			logger->Mesf( "#Missing closing braces" );
#endif
			errend++;
		}

		if ( errend != 0 ) {
			throw CGERROR_FATAL;
		}
	} catch ( CGERROR code ) {
		return (int)code;
	}

	finalize_ast_program( ast_program );
	return 0;
}


int CChspParser::GenerateCodeMainSkipError( CMemBuf *buf )
{
	//		ソースをHSP3Codeに展開する(エラースキップ)
	//
	ResetGenerator( const_cast<const char *>( buf->GetBuffer() ) );

	try {
		RegisterFuncLabels();
	} catch ( CGERROR code ) {
		logger->Mesf( "#Skip error (%d).\r\n", (int)code );
	}

	while ( true ) {
		try {
			if ( GenerateCodeBlock() == TK_EOF ) {
				break;
			}
		} catch ( CGERROR code ) {
			logger->Mesf( "#Skip error (%d).\r\n", (int)code );
			lexer.NextLine();
		}
	}
	return 0;
}


int CChspParser::GenerateCode( const std::string &fname, const std::string &oname, int mode )
{
	CMemBuf srcbuf;
	if ( srcbuf.PutFile( (char *)fname.c_str() ) < 0 ) {
		logger->Mes( "#No file." );
		return -1;
	}
	return GenerateCode( &srcbuf, oname, mode );
}


int CChspParser::GenerateCode( CMemBuf *srcbuf, const std::string &oname, int mode )
{
	//		ファイルをHSP3Codeに展開する
	//		mode			COMP_MODE_DEBUG Debug code (0=off 1=on)
	//						COMP_MODE_UTF8  utf-8 out (0=off 1=on)
	//
	int res;
	CMemBuf bakbuf; // プリプロセッサソース保存用バッファ

	// CG: writer = std::make_unique<CCodeWriter>( compopt, logger, symtab );

	bakbuf.PutStr( srcbuf->GetBuffer() ); // プリプロセッサソースを保存する

	compopt->SetMode( mode );

	if ( compopt->cg_utf8out() ) {
		logger->Mes( "#use UTF-8 strings." );
	}
	if ( compopt->cg_strmap() ) {
		logger->Mes( "#output string map." );
	}

	// CG: writer->ds_buf->AddIndexBuffer();

	cg_putvars = compopt->hed_cmpmode & CMPMODE_PUTVARS;

	if ( compopt->cg_skiperror() ) {
		res = GenerateCodeMainSkipError( srcbuf );
	} else {
		res = GenerateCodeMain( srcbuf );
	}

	if ( res != 0 ) {
		//		エラー終了
		char tmp[512];
		CStrNote note;
		CMemBuf srctmp;
#ifdef JPNMSG
		logger->Mesf( "%s(%d) : error %d : %s (%d行目)", lexer.cg_orgfile.cg_orgfile.c_str(), lexer.cg_orgline, res,
					  cg_geterror( (CGERROR)res ), lexer.cg_orgline );
#else
		logger->Mesf( "%s(%d) : error %d : %s (line %d)", lexer.cg_orgfile.c_str(), lexer.cg_orgline, res,
					  cg_geterror( (CGERROR)res ), lexer.cg_orgline );
#endif
		if ( cg_errline > 0 ) {
			note.Select( bakbuf.GetBuffer() );
			note.GetLine( tmp, cg_errline - 1, 510 );
			logger->Mesf( "--> %s", tmp );
		}
	} else {
		// CG: res = writer->Write( oname.c_str(), mode, cg_valcnt, cg_varhpi, cg_putvars );
	}

	// CG: writer = nullptr;

	return res;
}


void CChspParser::CG_MesLabelDefinition( int label_id )
{
	if ( compopt->cg_debug() ) {
		return;
	}

	LABOBJ *const labobj = symtab->lb->GetLabel( label_id );
	if ( labobj->def_file ) {
#ifdef JPNMSG
		logger->Mesf( "#識別子「%s」の定義位置: line %d in [%s]", symtab->lb->GetName( label_id ), labobj->def_line,
					  labobj->def_file );
#else
		logger->Mesf( "#Identifier '%s' has already defined in line %d in [%s]", symtab->lb->GetName( label_id ),
					  labobj->def_line, labobj->def_file );
#endif
	}
}


void CChspParser::ResetCompiler()
{
	ResetAstBuilder();
}

void CChspParser::ResetAstBuilder()
{
	ast_program = chspv3::ChspV3AstProgram();
	current_module = nullptr;
	current_function = nullptr;
	current_stmt = nullptr;
	expression_stack.clear();
}

void CChspParser::RecordSourceLine( const char *text )
{
	if ( text == nullptr ) {
		return;
	}
	chspv3::ChspV3SourceLine line;
	line.line = lexer.cg_orgline;
	line.text = lexer.CurrentLineText();
	line.directive = detect_source_directive_kind( line.text );
	if ( !ast_program.source_lines.empty() ) {
		const auto &last = ast_program.source_lines.back();
		if ( last.line == line.line && last.text == line.text ) {
			return;
		}
	}
	ast_program.source_lines.push_back( std::move( line ) );
}

void CChspParser::BeginAstStatement( int statement_kind, int token_kind, const std::string &text )
{
	auto stmt = std::make_unique<chspv3::ChspV3AstStmt>();
	stmt->kind = detect_ast_stmt_kind( statement_kind, text );
	stmt->line = lexer.cg_orgline;
	stmt->statement_kind = statement_kind;
	stmt->token_kind = token_kind;
	stmt->if_depth = iflev;
	stmt->repeat_depth = replev;
	stmt->text = text;

	if ( current_function != nullptr ) {
		current_function->body_stmts.push_back( std::move( stmt ) );
		current_stmt = current_function->body_stmts.back().get();
	} else {
		ast_program.top_level_stmts.push_back( std::move( stmt ) );
		current_stmt = ast_program.top_level_stmts.back().get();
	}
	expression_stack.clear();
}

void CChspParser::CaptureAstExpr( std::unique_ptr<chspv3::ChspV3AstExpr> expr )
{
	if ( current_stmt == nullptr || expr == nullptr ) {
		return;
	}
	expression_stack.push_back( std::move( expr ) );
}

std::unique_ptr<chspv3::ChspV3AstExpr> CChspParser::TakeCapturedExpression()
{
	if ( expression_stack.empty() ) {
		return nullptr;
	}
	auto expr = std::move( expression_stack.back() );
	expression_stack.pop_back();
	return expr;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CChspParser::CChspParser( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log )
	: CSourceTextUtil( compopt ), logger( log ), lexer( compopt, log )
{
	symtab = std::make_shared<CSymbolTable>();
	ResetCompiler();
}


CChspParser::~CChspParser() = default;
