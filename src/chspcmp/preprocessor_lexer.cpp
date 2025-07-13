//
//		Token analysis class
//			onion software/onitama 2002/2
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
#include "../hsp3/strnote.h"
#include "label.h"
#include "lexer_util.h"
#include "membuf.h"
#include "preprocessor_lexer.h"
#include "supio.h"
#include "tagstack.h"

enum
{
	s3size = 0x8000
};

#ifdef HSPWIN
#include <shlobj.h>
#include <windows.h>
#endif

static inline int issjisleadbyte( unsigned char c )
{
	return static_cast<int>( ( c >= 0x81 && c <= 0x9F ) || ( c >= 0xE0 && c <= 0xFC ) );
}

static inline int tstrcmp( const char *str1, const char *str2 )
{
	if ( strcmp( str1, str2 ) == 0 ) {
		return -1;
	}
	return 0;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CPpLexer::CPpLexer( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> logger )
	: CCompilerUtil( compopt ), logger( std::move( logger ) )
{
	token.s3 = (unsigned char *)malloc( s3size );
	compopt->hed_cmpmode = CMPMODE_OPTCODE | CMPMODE_OPTPRM | CMPMODE_SKIPJPSPC;
	ResetCompiler();
}


CPpLexer::~CPpLexer()
{
	if ( token.s3 != nullptr ) {
		free( token.s3 );
		token.s3 = nullptr;
	}
	//	buffer = NULL;
}

void CPpLexer::ResetCompiler()
{
	line = 1;
	token.fpbit = 256.0;

	//		reset header info
	compopt->Reset();
}


void CPpLexer::SetLook( char *buf )
{
	wp = (unsigned char *)buf;
}


char *CPpLexer::GetLook()
{
	return (char *)wp;
}


char *CPpLexer::GetLookResult()
{
	return (char *)token.s3;
}


int CPpLexer::GetLookResultInt()
{
	return token.val;
}


void CPpLexer::Pickstr()
{
	//		Strings pick sub
	//
	int a = 0;
	unsigned char a1;
	int skip;
	int i;
	while ( true ) {

	pickag:
		a1 = (unsigned char)*wp;
#if 0
		if (a1>=0x81) {
			if (a1<0xa0) {				// s-jis code
				token.s3[a++]=a1;wp++;
				token.s3[a++]=*wp;wp++;
				continue;
			}
			else if (a1>=0xe0) {		// s-jis code2
				token.s3[a++]=a1;wp++;
				token.s3[a++]=*wp;wp++;
				continue;
			}
		}
#endif

		if ( a1 == 0x5c ) { // '\' extra control
			wp++;
			a1 = tolower( *wp );
			switch ( a1 ) {
			case 'n':
				token.s3[a++] = 13;
				a1 = 10;
				break;
			case 't':
				a1 = 9;
				break;
			case 'r':
				token.s3[a++] = 13;
				wp++;
				goto pickag;
			case 0x22:
				token.s3[a++] = a1;
				wp++;
				goto pickag;
			}
		}
		if ( a1 == 0 ) {
			wp = nullptr;
			break;
		}
#ifdef HSPLINUX
		if ( a1 == 10 ) {
			wp++;
			line++;
			break;
		}
#endif
		if ( a1 == 13 ) {
			wp++;
			if ( *wp == 10 ) {
				wp++;
			}
			line++;
			break;
		}
		if ( a1 == 0x22 ) {
			wp++;
			if ( *wp == 0 ) {
				wp = nullptr;
			}
			break;
		}

#if 0
		token.s3[a++] = a1; wp++;
#else
		skip = SkipMultiByte( a1 ); // 全角文字チェック
		for ( i = 0; i <= skip; i++ ) {
			token.s3[a++] = a1;
			wp++;
			a1 = *wp;
		}
#endif
	}
	token.s3[a] = 0;
}


char *CPpLexer::Pickstr2( char *str )
{
	//		Strings pick sub '～'
	//
	unsigned char *vs;
	unsigned char *pp;
	unsigned char a1;
	int skip;
	int i;
	vs = (unsigned char *)str;
	pp = token.s3;

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 0x27 ) {
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

		skip = SkipMultiByte( a1 );
		if ( skip != 0 ) { // 全角文字チェック
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


int CPpLexer::GetToken()
{
	//
	//	get new word from wp ( result:s3 )
	//			result : word type
	//
	int rval;
	int a;
	int b;
	int minmode;
	unsigned char a1;
	unsigned char a2;
	unsigned char an;
	int fpflag;
	//	int *fpival;
	unsigned char *wp_bak;
	int ft_bak;

	if ( wp == nullptr ) {
		return TK_NONE;
	}

	a = 0;
	minmode = 0;
	rval = TK_OBJ;

	while ( true ) {
		a1 = *wp;

#ifdef HSPWIN
		if ( a1 == 0x81 ) {
			if ( wp[1] == 0x40 ) { // 全角スペースは無視
				if ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) {
					wp += 2;
					continue;
				}
			}
		}
#endif

		if ( ( a1 != 32 ) && ( a1 != 9 ) ) {
			break; // Skip Space & Tab
		}
		wp++;
	}

	if ( a1 == 0 ) {
		wp = nullptr;
		return TK_NONE;
	}				  // End of Source
	if ( a1 == 13 ) { // Line Break
		wp++;
		if ( *wp == 10 ) {
			wp++;
		}
		line++;
		return TK_NONE;
	}
	if ( a1 == 10 ) { // Unix Line Break
		wp++;
		line++;
		return TK_NONE;
	}

	//	Check Extra Character
	if ( a1 < 0x30 ) {
		rval = TK_NONE;
	}
	if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
		rval = TK_NONE;
	}
	if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
		rval = TK_NONE;
	}
	if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
		rval = TK_NONE;
	}

	if ( a1 == ':' ) { // multi statement
		wp++;
		return TK_SEPARATE;
	}

	if ( a1 == '0' ) {
		a2 = wp[1];
		if ( a2 == 'x' ) {
			wp++;
			a1 = '$';
		} // when hex code (0x)
		if ( a2 == 'b' ) {
			wp++;
			a1 = '%';
		} // when bin code (0b)
	}
	if ( a1 == '$' ) { // when hex code ($)
		wp++;
		token.val = 0;
		while ( true ) {
			a1 = toupper( *wp );
			b = -1;
			if ( a1 == 0 ) {
				wp = nullptr;
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
				token.s3[a++] = a1;
				token.val = ( token.val << 4 ) + b;
			}
			wp++;
		}
		token.s3[a] = 0;
		return TK_NUM;
	}

	if ( a1 == '%' ) { // when bin code (%)
		wp++;
		token.val = 0;
		while ( true ) {
			a1 = *wp;
			b = -1;
			if ( a1 == 0 ) {
				wp = nullptr;
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
				token.s3[a++] = a1;
				token.val = ( token.val << 1 ) + b;
			}
			wp++;
		}
		token.s3[a] = 0;
		return TK_NUM;
	}
	/*
		if (a1=='-') {							// minus operator (-)
			wp++;an=*wp;
			if ((an<0x30)||(an>0x39)) {
				token.s3[0]=a1; token.s3[1]=0;
				return a1;
			}
			minmode++;
			a1=an;						// 次が数値ならばそのまま継続
		}
	*/
	if ( ( a1 >= 0x30 ) && ( a1 <= 0x39 ) ) { // when 0-9 numerical
		fpflag = 0;
		ft_bak = 0;
		while ( true ) {
			a1 = *wp;
			if ( a1 == 0 ) {
				wp = nullptr;
				break;
			}
			if ( a1 == '.' ) {
				if ( fpflag != 0 ) {
					break;
				}
				a2 = *( wp + 1 );
				if ( ( a2 < 0x30 ) || ( a2 > 0x39 ) ) {
					break;
				}
				wp_bak = wp;
				ft_bak = a;
				fpflag = 3;
				// fpflag = -1;
				token.s3[a++] = a1;
				wp++;
				continue;
			}
			if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
				break;
			}
			token.s3[a++] = a1;
			wp++;
		}
		token.s3[a] = 0;
		if ( wp != nullptr ) {
			if ( *wp == 'k' ) {
				fpflag = 1;
				wp++;
			}
			if ( *wp == 'f' ) {
				fpflag = 2;
				wp++;
			}
			if ( *wp == 'd' ) {
				fpflag = 3;
				wp++;
			}
			if ( *wp == 'e' ) {
				fpflag = 4;
				wp++;
			}
		}

		if ( fpflag < 0 ) { // 小数値でない時は「.」までで終わり
			token.s3[ft_bak] = 0;
			wp = wp_bak;
			fpflag = 0;
		}

		switch ( fpflag ) {
		case 0: // 通常の整数
			token.val = atoi_allow_overflow( (char *)token.s3 );
			if ( minmode != 0 ) {
				token.val = -token.val;
			}
			break;
		case 1: // int固定小数
			token.val_d = atof( (char *)token.s3 );
			token.val = (int)( token.val_d * token.fpbit );
			if ( minmode != 0 ) {
				token.val = -token.val;
			}
			break;
		case 2: // int形式のfloat値を返す
#if 0
			token.val_f = (float)atof( (char *)token.s3 );
			if ( minmode ) token.val_f=-token.val_f;
			fpival = (int *)&token.val_f;
			token.val = *fpival;
			break;
#endif
			token.val_d = atof( (char *)token.s3 );
			if ( minmode != 0 ) {
				token.val_d = -token.val_d;
			}
			return TK_DNUM;
		case 4: // double値(指数表記)
			token.s3[a++] = 'e';
			a1 = *wp;
			if ( ( a1 == '-' ) || ( a1 == '+' ) ) {
				token.s3[a++] = a1;
				wp++;
			}
			while ( true ) {
				a1 = *wp;
				if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
					break;
				}
				token.s3[a++] = a1;
				wp++;
			}
			token.s3[a] = 0;
		case 3: // double値
			token.val_d = atof( (char *)token.s3 );
			if ( minmode != 0 ) {
				token.val_d = -token.val_d;
			}
			return TK_DNUM;
		}
		return TK_NUM;
	}

	if ( a1 == 0x22 ) { // when "string"
		wp++;
		Pickstr();
		return TK_STRING;
	}

	if ( a1 == 0x27 ) { // when 'char'
		wp++;
		wp = (unsigned char *)Pickstr2( (char *)wp );
		token.val = *(unsigned char *)token.s3;
		return TK_NUM;
	}

	if ( rval == TK_NONE ) { // token code
		wp++;
		an = *wp;
		if ( a1 == '!' ) {
			if ( an == '=' ) {
				wp++;
			}
		}
		/*
				else if (a1=='<') {
					if (an=='<') { wp++;a1=0x63; }	// '<<'
					if (an=='=') { wp++;a1=0x61; }	// '<='
				}
				else if (a1=='>') {
					if (an=='>') { wp++;a1=0x64; }	// '>>'
					if (an=='=') { wp++;a1=0x62; }	// '>='
				}
		*/
		else if ( a1 == '=' ) {
			if ( an == '=' ) {
				wp++;
			} // '=='
		} else if ( a1 == '|' ) {
			if ( an == '|' ) {
				wp++;
			} // '||'
		} else if ( a1 == '&' ) {
			if ( an == '&' ) {
				wp++;
			} // '&&'
		}
		token.s3[0] = a1;
		token.s3[1] = 0;
		return a1;
	}

	while ( true ) { // normal object name
		int skip;
		int i;
		a1 = *wp;
		if ( a1 == 0 ) {
			wp = nullptr;
			break;
		}
		if ( a1 < 0x30 ) {
			break;
		}
		if ( ( a1 >= 0x3a ) && ( a1 <= 0x3f ) ) {
			break;
		}
		if ( ( a1 >= 0x5b ) && ( a1 <= 0x5e ) ) {
			break;
		}
		if ( ( a1 >= 0x7b ) && ( a1 <= 0x7f ) ) {
			break;
		}

		if ( a >= OBJNAME_MAX ) {
			break;
		}

		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
#ifdef HSPWIN
			if ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) {
				if ( a1 == 0x81 ) {
					if ( wp[1] == 0x40 ) { // 全角スペースは終端として処理
						break;
					}
				}
			}
#endif
			for ( i = 0; i < skip; i++ ) {
				token.s3[a++] = a1;
				wp++;
				a1 = *wp;
			}
		}
		token.s3[a++] = a1;
		wp++;
	}
	token.s3[a] = 0;
	return TK_OBJ;
}


int CPpLexer::PeekToken()
{
	// 戻すのは wp のみ。
	// s3, val, val_f, val_d などは戻されない
	unsigned char *wp_bak = wp;
	int result = GetToken();
	wp = wp_bak;
	return result;
}
