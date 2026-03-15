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

#include <utility>

#include "../../hsp3/hsp3config.h"
#include "../../hsp3/hsp3debug.h"
#include "../../hsp3/hsp3struct.h"

#include "../label.h"
#include "../membuf.h"
#include "../supio.h"
#include "../tagstack.h"
#include "codegen_lexer.h"

#include "../errormsg.h"

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

namespace
{
inline unsigned char to_uchar( char c )
{
	return static_cast<unsigned char>( c );
}
} // namespace

const char *CCgLexer::PickLongStringCG( const char *str, std::string &out )
{
	//		指定文字列をmembufへ展開する
	//		( 複数行対応 {"～"} )
	//
	const char *psrc = str;
	const char *ps = nullptr;
	out.clear();

	while ( true ) {
		PickStringCG2( out, &psrc );
		if ( *psrc != 0 ) {
			break;
		}
		ps = NextLine();
		if ( ps == nullptr ) {
			throw CGERROR_MULTILINE_STR;
		}
		psrc = ps;

		out += "\r\n";
	}
	if ( *psrc != '}' ) {
		throw CGERROR_MULTILINE_STR;
	}

	if ( compopt->cg_debug() ) {
		// TODO PutDI( 254, 0, cg_orgline ); // ラインだけをデバッグ情報として登録
	}

	psrc++;
	return psrc;
}


const char *CCgLexer::PickStringCG( const char *str, int sep, std::string &out )
{
	//		指定文字列をスキップして終端コードを付加する
	//			sep = 区切り文字
	//
	const char *vs;
	char a1;
	int skip;
	int i;
	vs = str;
	out.clear();

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == sep ) {
			vs++;
			break;
		}
		if ( a1 == 0x5c ) { // '\'チェック
			vs++;
			a1 = static_cast<char>( tolower( to_uchar( *vs ) ) );
			if ( a1 < 32 ) {
				continue;
			}
			switch ( a1 ) {
			case 'n':
				out.push_back( 13 );
				a1 = 10;
				break;
			case 't':
				a1 = 9;
				break;
			case 'r':
				a1 = 13;
				break;
			}
		}
		skip = SkipMultiByte( to_uchar( a1 ) ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				out += a1;
				vs++;
				a1 = *vs;
			}
		}
		vs++;
		out += a1;
	}
	return vs;
}


const char *CCgLexer::PickStringCG2( std::string &out, const char **strsrc )
{
	//		指定文字列をスキップして終端コードを付加する
	//			sep = 区切り文字
	//
	const char *vs;
	char a1;
	int skip;
	int i;
	vs = *strsrc;

	while ( true ) {
		a1 = *vs;
		// Alertf("%d(%c)",a1,a1);
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 0x22 ) {
			vs++;
			if ( *vs == '}' ) {
				break;
			}
			out += static_cast<char>( a1 );
			continue;
		}
		if ( a1 == 0x5c ) { // '\'チェック
			vs++;
			a1 = static_cast<char>( tolower( to_uchar( *vs ) ) );
			if ( a1 < 32 ) {
				continue;
			}
			switch ( a1 ) {
			case 'n':
				out.push_back( 13 );
				a1 = 10;
				break;
			case 't':
				a1 = 9;
				break;
			case 'r':
				a1 = 13;
				break;
			case 0x22:
				a1 = 0x22;
				break;
			}
		}
		skip = SkipMultiByte( to_uchar( a1 ) ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				out += a1;
				vs++;
				a1 = *vs;
			}
		}
		vs++;
		out += a1;
	}
	*strsrc = vs;
	return *strsrc;
}


CCgToken CCgLexer::GetTokenCG( int option )
{
	cg_ptr_bak = cg_ptr;
	auto result = GetTokenCG( cg_ptr, option );
	cg_ptr = result.first; // cg_ptr を更新
	return result.second;
}

int CCgLexer::PickNextCodeCG()
{
	//		次のコード(１文字を返す)
	//		(終端の場合は0を返す)
	//
	const char *vs;
	char a1;
	vs = cg_ptr;
	if ( vs == nullptr ) {
		return 0;
	}
	while ( true ) {
		a1 = *vs;
		if ( ( a1 != 32 ) && ( a1 != '\t' ) ) { // space,tab以外か?
			break;
		}
		vs++;
	}
	return (int)a1;
}

std::pair<const char *, CCgToken> CCgLexer::GetTokenCG( const char *str, int option )
{
	//		stringデータのタイプと内容を返す
	//		(次のptrを返す)
	//		(ttypeにタイプを、val,val_d,cg_strに内容を書き込みます)
	//
	CCgToken token; // ローカルのCCgTokenインスタンス
	char s2[4096];

	const char *vs;
	char a1;
	char a2;
	int a;
	int b;
	int chk;
	int labmode;
	int skip;
	int i;

	token.line = line;
	token.val = 0;
	token.ttype = TK_NONE;
	token.cg_str.clear();

	vs = str;
	if ( vs == nullptr ) {
		token.ttype = TK_EOF;
		return std::make_pair( nullptr, token ); // already end
	}

	while ( true ) {
		a1 = *vs;
		if ( ( a1 != 32 ) && ( a1 != '\t' ) ) { // space,tab以外か?
			break;
		}
		vs++;
	}

	if ( a1 == 0 ) { // end
		token.ttype = TK_EOL;
		return std::make_pair( vs, token );
	}

	if ( a1 < 0x20 ) { // 無効なコード
		token.ttype = TK_ERROR;
		throw CGERROR_UNKNOWN;
	}

	if ( a1 == 0x22 ) { // "～"
		vs++;
		token.ttype = TK_STRING;
		const char *p = PickStringCG( reinterpret_cast<const char *>( vs ), 0x22, token.cg_str );
		return std::make_pair( p, token );
	}

	if ( a1 == '{' ) { // {"～"}
		if ( vs[1] == 0x22 ) {
			vs += 2;
			if ( *vs == 0 ) {
				vs = NextLine();
				if ( vs == nullptr ) {
					return std::make_pair( nullptr, token );
				}
			}
			token.ttype = TK_STRING;
			const char *p = PickLongStringCG( reinterpret_cast<const char *>( vs ), token.cg_str );
			return std::make_pair( p, token );
		}
	}

	if ( a1 == 0x27 ) { // '～'
		vs++;
		const char *p = PickStringCG( reinterpret_cast<const char *>( vs ), 0x27, token.cg_str );
		token.ttype = TK_NUM;
		token.val = to_uchar( token.cg_str[0] );
		return std::make_pair( p, token );
	}

	if ( ( a1 == ':' ) || ( a1 == '{' ) || ( a1 == '}' ) ) { // multi statement
		token.ttype = TK_SEPARATE;
		token.cg_str = a1;
		return std::make_pair( vs + 1, token );
	}

	if ( a1 == '0' ) {
		a2 = static_cast<char>( tolower( to_uchar( vs[1] ) ) );
		if ( a2 == 'x' ) {
			vs++;
			a1 = '$';
		} // when hex code (0x)
		if ( a2 == 'b' ) {
			vs++;
			a1 = '%';
		} // when bin code (0b)
	}

	if ( a1 == '$' ) { // when hex code ($)
		vs++;
		token.val = 0;
		a = 0;
		while ( true ) {
			a1 = static_cast<char>( toupper( to_uchar( *vs ) ) );
			b = -1;
			if ( a1 == 0 ) {
				break;
			}
			if ( ( a1 >= 0x30 ) && ( a1 <= 0x39 ) ) {
				b = a1 - 0x30;
			}
			if ( ( a1 >= 0x41 ) && ( a1 <= 0x46 ) ) {
				b = a1 - 55;
			}
			if ( a1 == '_' ) {
				b = -2;
			}
			if ( b == -1 ) {
				break;
			}
			if ( b >= 0 ) {
				s2[a++] = a1;
				token.val = ( token.val << 4 ) + b;
			}
			vs++;
		}
		s2[a] = 0;
		token.cg_str = (char *)s2;
		token.ttype = TK_NUM;
		return std::make_pair( vs, token );
	}

	if ( a1 == '%' ) { // when bin code (%)
		vs++;
		token.val = 0;
		a = 0;
		while ( true ) {
			a1 = *vs;
			b = -1;
			if ( a1 == 0 ) {
				break;
			}
			if ( ( a1 >= 0x30 ) && ( a1 <= 0x31 ) ) {
				b = a1 - 0x30;
			}
			if ( a1 == '_' ) {
				b = -2;
			}
			if ( b == -1 ) {
				break;
			}
			if ( b >= 0 ) {
				s2[a++] = a1;
				token.val = ( token.val << 1 ) + b;
			}
			vs++;
		}
		s2[a] = 0;
		token.cg_str = (char *)s2;
		token.ttype = TK_NUM;
		return std::make_pair( vs, token );
	}

	chk = 0;
	labmode = 0;
	if ( a1 < 0x30 ) {
		chk++;
	}
	if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
		chk++;
	}
	if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
		chk++;
	}
	if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
		chk++;
	}

	if ( ( option & ( GETTOKEN_EXPRBEG | GETTOKEN_LABEL ) ) != 0 ) {
		if ( a1 == '*' ) { // ラベル
			a2 = vs[1];
			b = 0;
			if ( a2 < 0x30 ) {
				b++;
			}
			if ( ( a2 >= 0x30 ) && ( a2 <= 0x3f ) ) {
				b++;
			}
			if ( ( a2 >= 0x5b ) && ( a2 <= 0x5e ) ) {
				b++;
			}
			if ( ( a2 >= 0x7b ) && ( a2 <= 0x7f ) ) {
				b++;
			}
			if ( b == 0 ) {
				chk = 0;
				labmode = 1;
				vs++;
				a1 = *vs;
			}
		}
	}

	int is_negative_number = 0;
	if ( ( ( option & GETTOKEN_EXPRBEG ) != 0 ) && a1 == '-' ) {
		is_negative_number = isdigit( to_uchar( vs[1] ) );
	}

	if ( ( is_negative_number != 0 ) || ( isdigit( to_uchar( a1 ) ) != 0 ) ) { // when 0-9 numerical
		a = 0;
		chk = 0;
		if ( is_negative_number != 0 ) {
			vs++;
		}
		while ( true ) {
			a1 = *vs;
			if ( ( option & GETTOKEN_NOFLOAT ) != 0 ) {
				if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
					break;
				}
			} else {
				if ( a1 == '.' ) {
					chk++;
					if ( chk > 1 ) {
						token.ttype = TK_ERROR;
						throw CGERROR_FLOATEXP;
					}
				} else {
					if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
						break;
					}
				}
			}
			s2[a++] = a1;
			vs++;
		}
		if ( ( a1 == 'f' ) || ( a1 == 'd' ) ) {
			chk = 1;
			vs++;
		}
		if ( a1 == 'e' ) { // 指数部を取り込む
			chk = 1;
			s2[a++] = 'e';
			vs++;
			a1 = *vs;
			if ( ( a1 == '-' ) || ( a1 == '+' ) ) {
				s2[a++] = a1;
				vs++;
			}
			while ( true ) {
				a1 = *vs;
				if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
					break;
				}
				s2[a++] = a1;
				vs++;
			}
		}

		s2[a] = 0;

		switch ( chk ) {
		case 1:
			token.val_d = atof( (char *)s2 );
			if ( is_negative_number != 0 ) {
				token.val_d = -token.val_d;
			}
			token.ttype = TK_DNUM;
			break;
		default:
			token.val = atoi_allow_overflow( (char *)s2 );
			if ( is_negative_number != 0 ) {
				token.val = -token.val;
			}
			token.ttype = TK_NUM;
			break;
		}
		return std::make_pair( vs, token );
	}

	if ( chk != 0 ) { // 記号
		vs++;
		a2 = *vs;
		switch ( a1 ) {
		case '-':
			if ( a2 == '>' ) {
				vs++;
				a1 = 0x65;
			} // '->'
			break;
		case '!':
			if ( a2 == '=' ) {
				vs++;
			}
			break;
		case '<':
			if ( a2 == '<' ) {
				vs++;
				a1 = 0x63;
			} // '<<'
			if ( a2 == '=' ) {
				vs++;
				a1 = 0x61;
			} // '<='
			break;
		case '>':
			if ( a2 == '>' ) {
				vs++;
				a1 = 0x64;
			} // '>>'
			if ( a2 == '=' ) {
				vs++;
				a1 = 0x62;
			} // '>='
			break;
		case '=':
			if ( a2 == '=' ) {
				vs++;
			} // '=='
			break;
		case '|':
			if ( a2 == '|' ) {
				vs++;
			} // '||'
			break;
		case '&':
			if ( a2 == '&' ) {
				vs++;
			} // '&&'
			break;
		}
		token.ttype = TK_NONE;
		token.val = (int)a1;
		return std::make_pair( vs, token );
	}

	a = 0;
	while ( true ) { // シンボル取り出し
		a1 = *vs;

		skip = SkipMultiByte( to_uchar( a1 ) ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < ( skip + 1 ); i++ ) {
				if ( a < OBJNAME_MAX ) {
					s2[a++] = a1;
					vs++;
					a1 = *vs;
				} else {
					vs++;
				}
			}
			continue;
		}

		chk = 0;
		if ( a1 < 0x30 ) {
			chk++;
		}
		if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
			chk++;
		}
		if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
			chk++;
		}
		if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
			chk++;
		}
		if ( chk != 0 ) {
			break;
		}
		vs++;
		if ( a < OBJNAME_MAX ) {
			s2[a++] = a1;
		}
	}
	s2[a] = 0;

	//		シンボル
	//
	if ( labmode != 0 ) {
		token.ttype = TK_LABEL;
	} else {
		token.ttype = TK_OBJ;
	}
	token.cg_str = (char *)s2;
	return std::make_pair( vs, token );
}


std::string CCgLexer::GetSymbolCG( const char *str )
{
	//		stringデータのシンボル内容を返す
	//
	const char *vs;
	char a1;
	int a;
	int chk;
	int skip;
	int i;
	std::string s2_local;

	vs = str;
	if ( vs == nullptr ) {
		return ""; // already end
	}

	while ( true ) {
		a1 = *vs;
		if ( ( a1 != 32 ) && ( a1 != '\t' ) ) { // space,tab以外か?
			break;
		}
		vs++;
	}

	if ( a1 < 0x20 ) { // 無効なコード
		return "";
	}

	chk = 0;
	if ( a1 < 0x30 ) {
		chk++;
	}
	if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
		chk++;
	}
	if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
		chk++;
	}
	if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
		chk++;
	}

	if ( chk != 0 ) { // 記号
		return "";
	}

	a = 0;
	while ( true ) { // シンボル取り出し
		a1 = *vs;

		skip = SkipMultiByte( to_uchar( a1 ) ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < ( skip + 1 ); i++ ) {
				if ( a < OBJNAME_MAX ) {
					s2_local += a1;
					vs++;
					a1 = *vs;
				} else {
					vs++;
				}
			}
			continue;
		}

		chk = 0;
		if ( a1 < 0x30 ) {
			chk++;
		}
		if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
			chk++;
		}
		if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
			chk++;
		}
		if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
			chk++;
		}
		if ( chk != 0 ) {
			break;
		}
		vs++;
		if ( a < OBJNAME_MAX ) {
			s2_local += a1;
		}
	}
	return s2_local;
}


//-----------------------------------------------------------------------------

int CCgLexer::GetParameterTypeCG( const std::string &name ) const
{
	//		パラメーター名を認識する(deffunc)
	//
	if ( name == "int" ) {
		return MPTYPE_INUM;
	}
	if ( name == "var" ) {
		return MPTYPE_SINGLEVAR;
	}
	if ( name == "val" ) {
#ifdef JPNMSG
		logger->Mesf( "警告:古いdeffunc表記があります 行%d.[%s]", cg_orgline, name.c_str() );
#else
		logger->Mesf( "Warning:Old deffunc expression at %d.[%s]", cg_orgline, name.c_str() );
#endif
		return MPTYPE_SINGLEVAR;
	}
	if ( name == "str" ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( name == "double" ) {
		return MPTYPE_DNUM;
	}
	if ( name == "label" ) {
		return MPTYPE_LABEL;
	}
	if ( name == "local" ) {
		return MPTYPE_LOCALVAR;
	}
	if ( name == "array" ) {
		return MPTYPE_ARRAYVAR;
	}
	if ( name == "modvar" ) {
		return MPTYPE_MODULEVAR;
	}
	if ( name == "modinit" ) {
		return MPTYPE_IMODULEVAR;
	}
	if ( name == "modterm" ) {
		return MPTYPE_TMODULEVAR;
	}

	return MPTYPE_NONE;
}


int CCgLexer::GetParameterStructTypeCG( const std::string &name ) const
{
	//		パラメーター名を認識する(struct)
	//
	if ( name == "int" ) {
		return MPTYPE_INUM;
	}
	if ( name == "var" ) {
		return MPTYPE_LOCALVAR;
	}
	if ( name == "str" ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( name == "double" ) {
		return MPTYPE_DNUM;
	}
	if ( name == "label" ) {
		return MPTYPE_LABEL;
	}
	if ( name == "float" ) {
		return MPTYPE_FLOAT;
	}
	return MPTYPE_NONE;
}


int CCgLexer::GetParameterFuncTypeCG( const std::string &name ) const
{
	//		パラメーター名を認識する(func)
	//
	if ( name == "int" ) {
		return MPTYPE_INUM;
	}
	if ( name == "var" ) {
		return MPTYPE_PVARPTR;
	}
	if ( name == "str" ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( name == "double" ) {
		return MPTYPE_DNUM;
	}
	//	if ( !strcmp( name,"label" ) ) return MPTYPE_LABEL;
	if ( name == "float" ) {
		return MPTYPE_FLOAT;
	}
	if ( name == "pval" ) {
		return MPTYPE_PPVAL;
	}
	if ( name == "bmscr" ) {
		return MPTYPE_PBMSCR;
	}

	if ( name == "comobj" ) {
		return MPTYPE_IOBJECTVAR;
	}
	if ( name == "wstr" ) {
		return MPTYPE_LOCALWSTR;
	}

	if ( name == "sptr" ) {
		return MPTYPE_FLEXSPTR;
	}
	if ( name == "wptr" ) {
		return MPTYPE_FLEXWPTR;
	}

	if ( name == "prefstr" ) {
		return MPTYPE_PTR_REFSTR;
	}
	if ( name == "pexinfo" ) {
		return MPTYPE_PTR_EXINFO;
	}
	if ( name == "nullptr" ) {
		return MPTYPE_NULLPTR;
	}

	//	if ( !strcmp( name,"hwnd" ) ) return MPTYPE_PTR_HWND;
	//	if ( !strcmp( name,"hdc" ) ) return MPTYPE_PTR_HDC;
	//	if ( !strcmp( name,"hinst" ) ) return MPTYPE_PTR_HINST;

	return MPTYPE_NONE;
}


int CCgLexer::GetParameterResTypeCG( const std::string &name ) const
{
	//		戻り値のパラメーター名を認識する(defcfunc)
	//
	if ( name == "int" ) {
		return MPTYPE_INUM;
	}
	if ( name == "str" ) {
		return MPTYPE_STRING;
	}
	if ( name == "double" ) {
		return MPTYPE_DNUM;
	}
	if ( name == "label" ) {
		return MPTYPE_LABEL;
	}
	if ( name == "float" ) {
		return MPTYPE_FLOAT;
	}
	return MPTYPE_NONE;
}

const char *CCgLexer::NextLine()
{
	//		次の行へ移動
	//
	cg_ptr = GetLineCG();
	cg_orgline++;

	return cg_ptr;
}

const char *CCgLexer::GetLineCG()
{
	//		vs_wpから１行を取得する
	//
	const char *p;
	char a1;
	int skip;
	p = cg_wp;
	if ( p == nullptr ) {
		return nullptr;
	}

	a1 = *p;
	if ( a1 == 0 ) {
		cg_wp = nullptr;
		return nullptr;
	}
	current_line_buffer.clear();
	while ( true ) {
		a1 = *p;

		skip = SkipMultiByte( to_uchar( a1 ) ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( int i = 0; i <= skip; ++i ) {
				current_line_buffer += *p;
				p++;
			}
			continue;
		}

		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 13 ) {
			p++;
			line++;
			if ( *p == 10 ) {
				p++;
			}
			break;
		}
		if ( a1 == 10 ) {
			p++;
			line++;
			break;
		}
		current_line_buffer += a1;
		p++;
	}
	cg_wp = p;
	return current_line_buffer.c_str();
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CCgLexer::CCgLexer( const std::shared_ptr<CompileOptions> &compopt, std::shared_ptr<CLogger> log )
	: CSourceTextUtil( compopt ), logger( std::move( log ) ), cg_orgline( 0 )
{
	cg_orgfile.clear();
	cg_orgfilefull.clear();
}


CCgLexer::~CCgLexer() = default;
