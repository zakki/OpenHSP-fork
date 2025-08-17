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

#include "../hsp3/hsp3config.h"
#include "../hsp3/hsp3debug.h"
#include "../hsp3/hsp3struct.h"

#include "codegen_lexer.h"
#include "label.h"
#include "membuf.h"
#include "supio.h"
#include "tagstack.h"

#include "errormsg.h"

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

char *CCgLexer::PickLongStringCG( char *str )
{
	//		指定文字列をmembufへ展開する
	//		( 複数行対応 {"～"} )
	//
	char *p;
	char *psrc;
	char *ps;
	p = psrc = str;

	while ( true ) {
		p = PickStringCG2( p, &psrc );
		if ( *psrc != 0 ) {
			break;
		}
		ps = GetLineCG();
		if ( ps == nullptr ) {
			throw CGERROR_MULTILINE_STR;
		}
		psrc = ps;
		cg_orgline++;

		//		行の終端にある0を改行に置き換える
		p[0] = 13;
		p[1] = 10;
		p += 2;
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


char *CCgLexer::PickStringCG( char *str, int sep )
{
	//		指定文字列をスキップして終端コードを付加する
	//			sep = 区切り文字
	//
	unsigned char *vs;
	unsigned char *pp;
	unsigned char a1;
	int skip;
	int i;
	vs = (unsigned char *)str;
	pp = vs;

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
			a1 = tolower( *vs );
			if ( a1 < 32 ) {
				continue;
			}
			switch ( a1 ) {
			case 'n':
				*pp++ = 13;
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
		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				*pp++ = a1;
				vs++;
				a1 = *vs;
			}
		}
		vs++;
		*pp++ = a1;
	}
	*pp = 0;
	return (char *)vs;
}


char *CCgLexer::PickStringCG2( char *str, char **strsrc )
{
	//		指定文字列をスキップして終端コードを付加する
	//			sep = 区切り文字
	//
	unsigned char *vs;
	unsigned char *pp;
	unsigned char a1;
	int skip;
	int i;
	vs = (unsigned char *)*strsrc;
	pp = (unsigned char *)str;

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
			*pp++ = a1;
			continue;
		}
		if ( a1 == 0x5c ) { // '\'チェック
			vs++;
			a1 = tolower( *vs );
			if ( a1 < 32 ) {
				continue;
			}
			switch ( a1 ) {
			case 'n':
				*pp++ = 13;
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
		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				*pp++ = a1;
				vs++;
				a1 = *vs;
			}
		}
		vs++;
		*pp++ = a1;
	}
	*pp = 0;
	*strsrc = (char *)vs;
	return (char *)pp;
}


char *CCgLexer::GetTokenCG( int option )
{
	cg_ptr_bak = cg_ptr;
	cg_ptr = GetTokenCG( cg_ptr, option );
	return cg_ptr;
}

int CCgLexer::PickNextCodeCG()
{
	//		次のコード(１文字を返す)
	//		(終端の場合は0を返す)
	//
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)cg_ptr;
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

char *CCgLexer::GetTokenCG( const char *str, int option )
{
	//		stringデータのタイプと内容を返す
	//		(次のptrを返す)
	//		(ttypeにタイプを、val,val_d,cg_strに内容を書き込みます)
	//
	const unsigned char *vs;
	unsigned char a1;
	unsigned char a2;
	int a;
	int b;
	int chk;
	int labmode;
	int skip;
	int i;

	vs = (const unsigned char *)str;
	if ( vs == nullptr ) {
		token.ttype = TK_EOF;
		return nullptr; // already end
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
		return (char *)vs;
		// return GetLineCG();
	}

	if ( a1 < 0x20 ) { // 無効なコード
		token.ttype = TK_ERROR;
		throw CGERROR_UNKNOWN;
	}

	if ( a1 == 0x22 ) { // "～"
		vs++;
		token.ttype = TK_STRING;
		token.cg_str = (char *)vs;
		return PickStringCG( (char *)vs, 0x22 );
	}

	if ( a1 == '{' ) { // {"～"}
		if ( vs[1] == 0x22 ) {
			vs += 2;
			if ( *vs == 0 ) {
				vs = (unsigned char *)GetLineCG();
				cg_orgline++;
				if ( vs == nullptr ) {
					return nullptr;
				}
			}
			token.ttype = TK_STRING;
			token.cg_str = (char *)vs;
			return PickLongStringCG( (char *)vs );
		}
	}

	if ( a1 == 0x27 ) { // '～'
		char *p;
		vs++;
		token.cg_str = (char *)vs;
		p = PickStringCG( (char *)vs, 0x27 );
		token.ttype = TK_NUM;
		token.val = ( (unsigned char *)token.cg_str )[0];
		return p;
	}

	if ( ( a1 == ':' ) || ( a1 == '{' ) || ( a1 == '}' ) ) { // multi statement
		token.cg_str = (char *)s2;
		token.ttype = TK_SEPARATE;
		token.cg_str[0] = a1;
		token.cg_str[1] = 0;
		return (char *)vs + 1;
	}

	if ( a1 == '0' ) {
		a2 = tolower( vs[1] );
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
		token.cg_str = (char *)s2;
		a = 0;
		while ( true ) {
			a1 = toupper( *vs );
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
				token.cg_str[a++] = (char)a1;
				token.val = ( token.val << 4 ) + b;
			}
			vs++;
		}
		token.cg_str[a] = 0;
		token.ttype = TK_NUM;
		return (char *)vs;
	}

	if ( a1 == '%' ) { // when bin code (%)
		vs++;
		token.val = 0;
		token.cg_str = (char *)s2;
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
				token.cg_str[a++] = (char)a1;
				token.val = ( token.val << 1 ) + b;
			}
			vs++;
		}
		token.cg_str[a] = 0;
		token.ttype = TK_NUM;
		return (char *)vs;
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
		is_negative_number = isdigit( vs[1] );
	}

	if ( ( is_negative_number != 0 ) || ( isdigit( a1 ) != 0 ) ) { // when 0-9 numerical
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
						return (char *)vs;
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
		return (char *)vs;
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
		return (char *)vs;
	}

	a = 0;
	while ( true ) { // シンボル取り出し
		a1 = *vs;

		skip = SkipMultiByte( a1 ); // 全角文字チェック
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
	return (char *)vs;
}


char *CCgLexer::GetSymbolCG( char *str )
{
	//		stringデータのシンボル内容を返す
	//
	unsigned char *vs;
	unsigned char a1;
	int a;
	int chk;
	int skip;
	int i;

	vs = (unsigned char *)str;
	if ( vs == nullptr ) {
		return nullptr; // already end
	}

	while ( true ) {
		a1 = *vs;
		if ( ( a1 != 32 ) && ( a1 != '\t' ) ) { // space,tab以外か?
			break;
		}
		vs++;
	}

	if ( a1 < 0x20 ) { // 無効なコード
		return nullptr;
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
		return nullptr;
	}

	a = 0;
	while ( true ) { // シンボル取り出し
		a1 = *vs;

		skip = SkipMultiByte( a1 ); // 全角文字チェック
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
	return (char *)s2;
}


//-----------------------------------------------------------------------------

int CCgLexer::GetParameterTypeCG( const char *name ) const
{
	//		パラメーター名を認識する(deffunc)
	//
	if ( strcmp( token.cg_str, "int" ) == 0 ) {
		return MPTYPE_INUM;
	}
	if ( strcmp( token.cg_str, "var" ) == 0 ) {
		return MPTYPE_SINGLEVAR;
	}
	if ( strcmp( token.cg_str, "val" ) == 0 ) {
#ifdef JPNMSG
		logger->Mesf( "警告:古いdeffunc表記があります 行%d.[%s]", cg_orgline, name );
#else
		logger->Mesf( "Warning:Old deffunc expression at %d.[%s]", cg_orgline, name );
#endif
		return MPTYPE_SINGLEVAR;
	}
	if ( strcmp( token.cg_str, "str" ) == 0 ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( strcmp( token.cg_str, "double" ) == 0 ) {
		return MPTYPE_DNUM;
	}
	if ( strcmp( token.cg_str, "label" ) == 0 ) {
		return MPTYPE_LABEL;
	}
	if ( strcmp( token.cg_str, "local" ) == 0 ) {
		return MPTYPE_LOCALVAR;
	}
	if ( strcmp( token.cg_str, "array" ) == 0 ) {
		return MPTYPE_ARRAYVAR;
	}
	if ( strcmp( token.cg_str, "modvar" ) == 0 ) {
		return MPTYPE_MODULEVAR;
	}
	if ( strcmp( token.cg_str, "modinit" ) == 0 ) {
		return MPTYPE_IMODULEVAR;
	}
	if ( strcmp( token.cg_str, "modterm" ) == 0 ) {
		return MPTYPE_TMODULEVAR;
	}

	return MPTYPE_NONE;
}


int CCgLexer::GetParameterStructTypeCG( const char *name ) const
{
	//		パラメーター名を認識する(struct)
	//
	if ( strcmp( token.cg_str, "int" ) == 0 ) {
		return MPTYPE_INUM;
	}
	if ( strcmp( token.cg_str, "var" ) == 0 ) {
		return MPTYPE_LOCALVAR;
	}
	if ( strcmp( token.cg_str, "str" ) == 0 ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( strcmp( token.cg_str, "double" ) == 0 ) {
		return MPTYPE_DNUM;
	}
	if ( strcmp( token.cg_str, "label" ) == 0 ) {
		return MPTYPE_LABEL;
	}
	if ( strcmp( token.cg_str, "float" ) == 0 ) {
		return MPTYPE_FLOAT;
	}
	return MPTYPE_NONE;
}


int CCgLexer::GetParameterFuncTypeCG( const char *name ) const
{
	//		パラメーター名を認識する(func)
	//
	if ( strcmp( token.cg_str, "int" ) == 0 ) {
		return MPTYPE_INUM;
	}
	if ( strcmp( token.cg_str, "var" ) == 0 ) {
		return MPTYPE_PVARPTR;
	}
	if ( strcmp( token.cg_str, "str" ) == 0 ) {
		return MPTYPE_LOCALSTRING;
	}
	if ( strcmp( token.cg_str, "double" ) == 0 ) {
		return MPTYPE_DNUM;
	}
	//	if ( !strcmp( token.cg_str,"label" ) ) return MPTYPE_LABEL;
	if ( strcmp( token.cg_str, "float" ) == 0 ) {
		return MPTYPE_FLOAT;
	}
	if ( strcmp( token.cg_str, "pval" ) == 0 ) {
		return MPTYPE_PPVAL;
	}
	if ( strcmp( token.cg_str, "bmscr" ) == 0 ) {
		return MPTYPE_PBMSCR;
	}

	if ( strcmp( token.cg_str, "comobj" ) == 0 ) {
		return MPTYPE_IOBJECTVAR;
	}
	if ( strcmp( token.cg_str, "wstr" ) == 0 ) {
		return MPTYPE_LOCALWSTR;
	}

	if ( strcmp( token.cg_str, "sptr" ) == 0 ) {
		return MPTYPE_FLEXSPTR;
	}
	if ( strcmp( token.cg_str, "wptr" ) == 0 ) {
		return MPTYPE_FLEXWPTR;
	}

	if ( strcmp( token.cg_str, "prefstr" ) == 0 ) {
		return MPTYPE_PTR_REFSTR;
	}
	if ( strcmp( token.cg_str, "pexinfo" ) == 0 ) {
		return MPTYPE_PTR_EXINFO;
	}
	if ( strcmp( token.cg_str, "nullptr" ) == 0 ) {
		return MPTYPE_NULLPTR;
	}

	//	if ( !strcmp( token.cg_str,"hwnd" ) ) return MPTYPE_PTR_HWND;
	//	if ( !strcmp( token.cg_str,"hdc" ) ) return MPTYPE_PTR_HDC;
	//	if ( !strcmp( token.cg_str,"hinst" ) ) return MPTYPE_PTR_HINST;

	return MPTYPE_NONE;
}


int CCgLexer::GetParameterResTypeCG( const char *name ) const
{
	//		戻り値のパラメーター名を認識する(defcfunc)
	//
	if ( strcmp( token.cg_str, "int" ) == 0 ) {
		return MPTYPE_INUM;
	}
	if ( strcmp( token.cg_str, "str" ) == 0 ) {
		return MPTYPE_STRING;
	}
	if ( strcmp( token.cg_str, "double" ) == 0 ) {
		return MPTYPE_DNUM;
	}
	if ( strcmp( token.cg_str, "label" ) == 0 ) {
		return MPTYPE_LABEL;
	}
	if ( strcmp( token.cg_str, "float" ) == 0 ) {
		return MPTYPE_FLOAT;
	}
	return MPTYPE_NONE;
}

char *CCgLexer::GetLineCG()
{
	//		vs_wpから１行を取得する
	//
	char *pp;
	unsigned char *p;
	unsigned char a1;
	int skip;
	p = cg_wp;
	if ( p == nullptr ) {
		return nullptr;
	}

	pp = (char *)p;
	a1 = *p;
	if ( a1 == 0 ) {
		cg_wp = nullptr;
		return nullptr;
	}
	while ( true ) {
		a1 = *p;

		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
			p += skip + 1;
			continue;
		}

		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 13 ) {
			*p = 0;
			p++;
			token.line++;
			if ( *p == 10 ) {
				*p = 0;
				p++;
			}
			break;
		}
		if ( a1 == 10 ) {
			*p = 0;
			p++;
			token.line++;
			break;
		}
		p++;
	}
	cg_wp = p;
	return pp;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CCgLexer::CCgLexer( const std::shared_ptr<CompileOptions> &compopt, std::shared_ptr<CLogger> log )
	: CCompilerUtil( compopt ), token(), logger( std::move( log ) ), cg_orgline( 0 )
{

	cg_orgfile.clear();
	cg_orgfilefull.clear();
}


CCgLexer::~CCgLexer() = default;
