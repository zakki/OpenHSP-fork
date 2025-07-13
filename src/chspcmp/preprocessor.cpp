
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

#include "../hsp3/hsp3config.h"
#include "../hsp3/strnote.h"
#include "ahtobj.h"
#include "label.h"
#include "lexer_util.h"
#include "membuf.h"
#include "preprocessor.h"
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
//		Routines
//-------------------------------------------------------------

void CPreProcessor::SetPackfileOut( CMemBuf *pack )
{
	//		packfile出力バッファ登録
	//
	packbuf = pack;
	packbuf->PutStr( ";\r\n;\tsource generated packfile\r\n;\r\n" );
}


int CPreProcessor::AddPackfile( char *name, int mode )
{
	//		packfile出力
	//			0=name/1=+name/2=other
	//
	CStrNote note;
	int i;
	int max;
	char fname[HSP_MAX_PATH];
	char p_fdir[HSP_MAX_PATH];
	char p_fname[HSP_MAX_PATH];
	char packadd[1024];
	char tmp[1024];
	char *s;
	char *findptr;
	bool absolutePath = false; // 絶対パスか?

	getpath( name, p_fdir, 32 );
	getpath( name, p_fname, 8 );

#ifdef HSPWIN
	strchr3( p_fdir, ':', 0, &findptr ); // ドライブ文字があった
	if ( findptr != NULL ) {
		absolutePath = true;
	}
	if ( *p_fdir == '\\' ) {
		absolutePath = true;
	}
#endif
	if ( *p_fdir == '/' ) {
		absolutePath = true;
	}

	if ( !absolutePath ) {
		strcpy( fname, search_path );
		strcat( fname, p_fdir );
		strcpy( p_fdir, fname );
		strcat( fname, p_fname );
	} else {
		strcpy( fname, name );
	}

	strcpy( packadd, fname );

	if ( mode < 2 ) {
#ifdef HSPWIN
		strcase( packadd );
#endif
		note.Select( packbuf->GetBuffer() );
		max = note.GetMaxLine();
		for ( i = 0; i < max; i++ ) {
			note.GetLine( tmp, i );
			s = tmp;
			if ( *s == '+' ) {
				s++;
			}
			if ( strcmp( s, packadd ) == 0 ) {
				return -1;
			}
		}
		if ( mode == 1 ) {
			packbuf->PutStr( "+" );
		}
		packbuf->PutStr( ">" );
	}
	packbuf->PutStr( packadd );
	packbuf->PutStr( "\r\n" );
	return 0;
}


int CPreProcessor::AddPackfileOrig( char *name, int mode )
{
	//		packfile出力
	//			0=name/1=+name/2=other
	//
	CStrNote note;
	int i;
	int max;
	char fname[HSP_MAX_PATH];
	char p_fdir[HSP_MAX_PATH];
	char p_fname[HSP_MAX_PATH];
	char packadd[1024];
	char tmp[1024];
	char *s;
	char *findptr;
	bool absolutePath = false; // 絶対パスか?

	getpath( name, p_fdir, 32 );
	getpath( name, p_fname, 8 );

#ifdef HSPWIN
	strchr3( p_fdir, ':', 0, &findptr ); // ドライブ文字があった
	if ( findptr != NULL ) {
		absolutePath = true;
	}
	if ( *p_fdir == '\\' ) {
		absolutePath = true;
	}
#endif
	if ( *p_fdir == '/' ) {
		absolutePath = true;
	}

	if ( !absolutePath ) {
		strcpy( fname, search_path );
		strcat( fname, p_fdir );
		strcpy( p_fdir, fname );
		strcat( fname, p_fname );
	} else {
		strcpy( fname, name );
	}

	strcpy( packadd, fname );
	if ( mode < 2 ) {
#ifdef HSPWIN
		strcase( packadd );
#endif
		note.Select( packbuf->GetBuffer() );
		max = note.GetMaxLine();
		for ( i = 0; i < max; i++ ) {
			note.GetLine( tmp, i );
			s = tmp;
			if ( *s == '+' ) {
				s++;
			}
			if ( strcmp( s, packadd ) == 0 ) {
				return -1;
			}
		}
		if ( mode == 1 ) {
			packbuf->PutStr( "+" );
		}
	}
	packbuf->PutStr( packadd );
	packbuf->PutStr( "\r\n" );
	return 0;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CPreProcessor::CPreProcessor( const std::shared_ptr<CompileOptions> &compopt, const std::shared_ptr<CLogger> &logger )
	: CCompilerUtil( compopt ), lexer( compopt, logger ), logger( logger ), token( &lexer.token ),
	  tstack( std::make_unique<CTagStack>() ), packbuf( nullptr ), ahtmodel( nullptr ), ahtbuf( nullptr ),
	  labbuf( nullptr ), wrtbuf( nullptr )
{
	compopt->hed_cmpmode = CMPMODE_OPTCODE | CMPMODE_OPTPRM | CMPMODE_SKIPJPSPC;


	ResetCompiler();
}


CPreProcessor::~CPreProcessor() = default;


void CPreProcessor::SetLabelListBuffer( CMemBuf *buf, int mode, char *match, int line, char *filename )
{
	labbuf = buf;
	cg_labout_mode = mode;
	cg_labout_match = match;
	cg_labout_line = line;
	if ( filename == nullptr ) {
		*cg_labout_orgfile = 0;
	} else {
		strcpy( cg_labout_orgfile, filename );
	}

	static char *p[] = { "---", "fnc", "mac", "lab", "var", "exv", "cmd", "exc", nullptr };
	cg_labout_header = p;
}


char *CPreProcessor::GetLabelListHeader( int flag )
{
	int i = flag & ( LABBUF_FLAG_REFER - 1 );
	if ( ( i < 0 ) || ( i > LABBUF_FLAG_EXCMD ) ) {
		i = 0;
	}
	return cg_labout_header[i];
}


void CPreProcessor::ResetCompiler()
{
	lexer.ResetCompiler();
	incinf = 0;
	swsp = 0;
	swmode = 0;
	swlevel = 0;
	SetModuleName( "" );
	modgc = 0;
	search_path[0] = 0;
	symtab.lb->Reset();
	fileadd = 0;
	pp_orgline = 0;
	pp_orgfile[0] = 0;
	pp_orgfilefull[0] = 0;

	//		reset header info
	compopt->Reset();

	//		reset labout
	cg_labout_mode = 0;
	cg_labout_line = 0;
	cg_labout_caseflag = 0;
	cg_labout_match = nullptr;
	*cg_labout_modname = 0;
}


void CPreProcessor::SetAHT( AHTMODEL *aht )
{
	ahtmodel = aht;
}


void CPreProcessor::SetAHTBuffer( CMemBuf *aht )
{
	ahtbuf = aht;
}


int CPreProcessor::CheckModuleName( char *name )
{
	int a;
	unsigned char *p;
	unsigned char a1;

	a = 0;
	p = (unsigned char *)name;
	while ( true ) { // normal object name
		a1 = *p;
		if ( a1 == 0 ) {
			return 0;
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

		p++;
		p += SkipMultiByte( a1 ); // 全角文字チェック
	}
	return -1;
}

//-----------------------------------------------------------------------------

void CPreProcessor::Calc_token()
{
	lasttoken = (char *)lexer.wp;
	ttype = lexer.GetToken();
}

void CPreProcessor::Calc_factor( CALCVAR &v )
{
	CALCVAR v1;
	int id;
	int type;
	char *ptr_dval;
	if ( ttype == TK_NUM ) {
		v = (CALCVAR)token->val;
		Calc_token();
		return;
	}
	if ( ttype == TK_DNUM ) {
		v = (CALCVAR)token->val_d;
		Calc_token();
		return;
	}
	if ( ttype == TK_OBJ ) {
		id = symtab.lb->Search( (char *)token->s3 );
		if ( id == -1 ) {
			ttype = TK_CALCERROR;
			return;
		}
		type = symtab.lb->GetType( id );
		if ( type != LAB_TYPE_PPVAL ) {
			ttype = TK_CALCERROR;
			return;
		}
		ptr_dval = symtab.lb->GetData2( id );
		if ( ptr_dval == nullptr ) {
			v = (CALCVAR)symtab.lb->GetOpt( id );
		} else {
			v = *(CALCVAR *)ptr_dval;
		}
		Calc_token();
		return;
	}
	if ( ttype != '(' ) {
		ttype = TK_ERROR;
		return;
	}
	Calc_token();
	Calc_start( v1 );
	if ( ttype != ')' ) {
		ttype = TK_CALCERROR;
		return;
	}
	Calc_token();
	v = v1;
}

void CPreProcessor::Calc_unary( CALCVAR &v )
{
	CALCVAR v1;
	int op;
	if ( ttype == '-' ) {
		op = ttype;
		Calc_token();
		Calc_unary( v1 );
		v1 = -v1;
	} else {
		Calc_factor( v1 );
	}
	v = v1;
}

void CPreProcessor::Calc_muldiv( CALCVAR &v )
{
	CALCVAR v1, v2;
	int op;
	Calc_unary( v1 );
	while ( ( ttype == '*' ) || ( ttype == '/' ) || ( ttype == 0x5c ) ) {
		op = ttype;
		Calc_token();
		Calc_unary( v2 );
		if ( op == '*' ) {
			v1 *= v2;
		} else if ( op == '/' ) {
			if ( v2 == 0.0 ) {
				ttype = TK_CALCERROR;
				return;
			}
			v1 /= v2;
		} else if ( op == 0x5c ) {
			if ( (int)v2 == 0 ) {
				ttype = TK_CALCERROR;
				return;
			}
			v1 = fmod( v1, v2 );
		}
	}
	v = v1;
}

void CPreProcessor::Calc_addsub( CALCVAR &v )
{
	CALCVAR v1, v2;
	int op;
	Calc_muldiv( v1 );
	while ( ( ttype == '+' ) || ( ttype == '-' ) ) {
		op = ttype;
		Calc_token();
		Calc_muldiv( v2 );
		if ( op == '+' ) {
			v1 += v2;
		} else if ( op == '-' ) {
			v1 -= v2;
		}
	}
	v = v1;
}


void CPreProcessor::Calc_compare( CALCVAR &v )
{
	CALCVAR v1, v2;
	int v1i;
	int v2i;
	int op;
	Calc_addsub( v1 );
	while ( ( ttype == '<' ) || ( ttype == '>' ) || ( ttype == '=' ) ) {
		op = ttype;
		if ( op == '=' ) {
			Calc_token();
			Calc_addsub( v2 );
			v1i = static_cast<int>( v1 == v2 );
			v1 = (CALCVAR)v1i;
			continue;
		}
		if ( op == '<' ) {
			if ( *lexer.wp == '=' ) {
				lexer.wp++;
				Calc_token();
				Calc_addsub( v2 );
				v1i = static_cast<int>( v1 <= v2 );
				v1 = (CALCVAR)v1i;
				continue;
			}
			if ( *lexer.wp == '<' ) {
				lexer.wp++;
				Calc_token();
				Calc_addsub( v2 );
				v1i = (int)v1;
				v2i = (int)v2;
				v1i <<= v2i;
				v1 = (CALCVAR)v1i;
				continue;
			}
			Calc_token();
			Calc_addsub( v2 );
			v1i = static_cast<int>( v1 < v2 );
			v1 = (CALCVAR)v1i;
			continue;
		}
		if ( op == '>' ) {
			if ( *lexer.wp == '=' ) {
				lexer.wp++;
				Calc_token();
				Calc_addsub( v2 );
				v1i = static_cast<int>( v1 >= v2 );
				v1 = (CALCVAR)v1i;
				continue;
			}
			if ( *lexer.wp == '>' ) {
				lexer.wp++;
				Calc_token();
				Calc_addsub( v2 );
				v1i = (int)v1;
				v2i = (int)v2;
				v1i >>= v2i;
				v1 = (CALCVAR)v1i;
				continue;
			}
			Calc_token();
			Calc_addsub( v2 );
			v1i = static_cast<int>( v1 > v2 );
			v1 = (CALCVAR)v1i;
			continue;
		}
		v1 = (CALCVAR)v1i;
	}
	v = v1;
}


void CPreProcessor::Calc_bool2( CALCVAR &v )
{
	CALCVAR v1, v2;
	int v1i;
	int v2i;
	Calc_compare( v1 );
	while ( ttype == '!' ) {
		Calc_token();
		Calc_compare( v2 );
		v1i = (int)v1;
		v2i = (int)v2;
		v1i = static_cast<int>( v1i != v2i );
		v1 = (CALCVAR)v1i;
	}
	v = v1;
}


void CPreProcessor::Calc_bool( CALCVAR &v )
{
	CALCVAR v1, v2;
	int op;
	int v1i;
	int v2i;
	Calc_bool2( v1 );
	while ( ( ttype == '&' ) || ( ttype == '|' ) || ( ttype == '^' ) ) {
		op = ttype;
		Calc_token();
		Calc_bool2( v2 );
		v1i = (int)v1;
		v2i = (int)v2;
		if ( op == '&' ) {
			v1i &= v2i;
		} else if ( op == '|' ) {
			v1i |= v2i;
		} else if ( op == '^' ) {
			v1i ^= v2i;
		}
		v1 = (CALCVAR)v1i;
	}
	v = v1;
}


void CPreProcessor::Calc_start( CALCVAR &v )
{
	//		entry point
	Calc_bool( v );
}

int CPreProcessor::Calc( CALCVAR &val )
{
	CALCVAR v;
	Calc_token();
	Calc_start( v );
	if ( ttype == TK_CALCERROR ) {
		logger->SetError( "abnormal calculation" );
		return -1;
	}
	if ( ttype != TK_NONE ) {
		logger->SetError( "expression syntax error" );
		return -1;
	}
	if ( lexer.wp == nullptr ) {
		val = v;
		return 0;
	}
	if ( *lexer.wp == 0 ) {
		val = v;
		return 0;
	}
	logger->SetError( "expression syntax error" );
	return -1;
}

//-----------------------------------------------------------------------------

char *CPreProcessor::ExpandStr( char *str, int opt )
{
	//		指定文字列をmembufへ展開する
	//			opt:0=行末までスキップ/1="まで/2='まで
	//
	int a;
	unsigned char *vs;
	unsigned char a1;
	unsigned char sep;
	int skip;
	int i;
	vs = (unsigned char *)str;
	a = 0;
	sep = 0;
	if ( opt == 1 ) {
		sep = 0x22;
	}
	if ( opt == 2 ) {
		sep = 0x27;
	}
	token->s3[a++] = sep;

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == sep ) {
			vs++;
			break;
		}
		if ( ( a1 < 32 ) && ( a1 != 9 ) ) {
			break;
		}
		token->s3[a++] = a1;
		vs++;
		if ( a1 == 0x5c ) { // '\'チェック
			token->s3[a++] = *vs++;
		}

		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				token->s3[a++] = *vs++;
			}
		}
	}
	token->s3[a++] = sep;
	token->s3[a] = 0;
	if ( opt != 0 ) {
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutData( token->s3, a );
		}
	}
	return (char *)vs;
}


char *CPreProcessor::ExpandAhtStr( char *str )
{
	//		コメントを展開する
	//		( ;;に続くAHT指定文字列用 )
	//
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)str;

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			break;
		}
		if ( ( a1 < 32 ) && ( a1 != 9 ) ) {
			break;
		}
		vs++;
	}
	return (char *)vs;
}


char *CPreProcessor::ExpandStrEx( char *str )
{
	//		指定文字列をmembufへ展開する
	//		( 複数行対応 {"～"} )
	//
	int a;
	unsigned char *vs;
	unsigned char a1;
	int skip;
	int i;
	vs = (unsigned char *)str;
	a = 0;
	// token->s3[a++]=0x22;

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			// token->s3[a++]=13; token->s3[a++]=10;
			break;
		}
		if ( a1 == 13 ) {
			token->s3[a++] = 0x5c;
			token->s3[a++] = 'n';
			vs++;
			if ( *vs == 10 ) {
				vs++;
			}
			continue;
		}
#ifdef HSPLINUX
		if ( a1 == 10 ) {
			token->s3[a++] = 0x5c;
			token->s3[a++] = 'n';
			vs++;
			continue;
		}
#endif
		//		if ((a1<32)&&(a1!=9)) break;
		if ( a1 == 0x22 ) {
			if ( vs[1] == '}' ) {
				token->s3[a++] = 0x22;
				token->s3[a++] = '}';
				mulstr = LMODE_ON;
				vs += 2;
				break;
			}
			token->s3[a++] = 0x5c;
			token->s3[a++] = 0x22;
			vs++;
			continue;
		}
		token->s3[a++] = a1;
		vs++;
		if ( a1 == 0x5c ) { // '\'チェック
			if ( *vs >= 32 ) {
				token->s3[a++] = *vs;
				vs++;
			}
		}

		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {
			for ( i = 0; i < skip; i++ ) {
				token->s3[a++] = *vs++;
			}
		}
	}
	// token->s3[a++]=0x22;
	token->s3[a] = 0;
	if ( wrtbuf != nullptr ) {
		wrtbuf->PutData( token->s3, a );
	}
	return (char *)vs;
}


char *CPreProcessor::ExpandStrComment( char *str, int opt )
{
	//		/*～*/ コメントを展開する
	//
	int a;
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)str;
	a = 0;

	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			// s3[a++]=13; s3[a++]=10;
			break;
		}
		if ( a1 == '*' ) {
			if ( vs[1] == '/' ) {
				mulstr = LMODE_ON;
				vs += 2;
				break;
			}
			vs++;
			continue;
		}
		vs++;
		vs += SkipMultiByte( a1 ); // 全角文字チェック
	}
	token->s3[a] = 0;
	if ( opt == 0 ) {
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutData( token->s3, a );
		}
	}
	return (char *)vs;
}


char *CPreProcessor::ExpandHex( char *str, int *val )
{
	//		16進数文字列をmembufへ展開する
	//
	int a;
	int b;
	int num;
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)str;

	token->s3[0] = '$';
	a = 1;
	num = 0;
	while ( true ) {
		a1 = toupper( *vs );
		b = -1;
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
			token->s3[a++] = a1;
			num = ( num << 4 ) + b;
		}
		vs++;
	}
	token->s3[a] = 0;
	if ( wrtbuf != nullptr ) {
		wrtbuf->PutData( token->s3, a );
	}
	*val = num;
	return (char *)vs;
}


char *CPreProcessor::ExpandBin( char *str, int *val )
{
	//		2進数文字列をmembufへ展開する
	//
	int a;
	int b;
	int num;
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)str;

	token->s3[0] = '%';
	a = 1;
	num = 0;
	while ( true ) {
		a1 = *vs;
		b = -1;
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
			token->s3[a++] = a1;
			num = ( num << 1 ) + b;
		}
		vs++;
	}
	token->s3[a] = 0;
	if ( wrtbuf != nullptr ) {
		wrtbuf->PutData( token->s3, a );
	}
	return (char *)vs;
}


char *CPreProcessor::ExpandToken( char *str, int *type, int ppmode )
{
	//		stringデータをmembufへ展開する
	//			ppmode : 0=通常、1=プリプロセッサ時
	//
	int a;
	int chk;
	int id;
	int ltype;
	int opt;
	int flcnt;
	unsigned char *vs;
	unsigned char *vs_bak;
	unsigned char a1;
	unsigned char a2;
	unsigned char *vs_modbrk;
	char cnvstr[80];
	char fixname[256];
	char *macptr;

	vs = (unsigned char *)str;
	if ( vs == nullptr ) {
		*type = TK_EOF;
		return nullptr; // already end
	}

	a1 = *vs;
	if ( a1 == 0 ) { // end
		*type = TK_EOF;
		return nullptr;
	}
	if ( a1 == 10 ) { // Unix改行
		vs++;
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutStr( "\r\n" );
		}
		*type = TK_EOL;
		return (char *)vs;
	}
	if ( a1 == 13 ) { // 改行
		vs++;
		if ( *vs == 10 ) {
			vs++;
		}
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutStr( "\r\n" );
		}
		*type = TK_EOL;
		return (char *)vs;
	}
	if ( a1 == ';' ) { // コメント
		*type = TK_VOID;
		*vs = 0;
		vs++;
		if ( *vs == ';' ) {
			vs++;
			if ( ahtmodel != nullptr ) {
				ahtkeyword = (char *)vs;
			}
		}
		return ExpandStr( (char *)vs, 0 );
	}
	if ( a1 == '/' ) { // Cコメント
		if ( vs[1] == '/' ) {
			*type = TK_VOID;
			*vs = 0;
			return ExpandStr( (char *)vs + 2, 0 );
		}
		if ( vs[1] == '*' ) {
			mulstr = LMODE_COMMENT;
			*type = TK_VOID;
			*vs = 0;
			return ExpandStrComment( (char *)vs + 2, 0 );
		}
	}
	if ( a1 == 0x22 ) { // "～"
		*type = TK_STRING;
		return ExpandStr( (char *)vs + 1, 1 );
	}
	if ( a1 == 0x27 ) { // '～'
		*type = TK_STRING;
		return ExpandStr( (char *)vs + 1, 2 );
	}
	if ( a1 == '{' ) { // {"～"}
		if ( vs[1] == 0x22 ) {
			if ( wrtbuf != nullptr ) {
				wrtbuf->PutStr( "{\"" );
			}
			mulstr = LMODE_STR;
			*type = TK_STRING;
			char *pp = ExpandStrEx( (char *)vs + 2 );
			return pp;
		}
	}

	if ( a1 == '0' ) {
		a2 = vs[1];
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
		*type = TK_OBJ;
		return ExpandHex( (char *)vs + 1, &a );
	}

	if ( a1 == '%' ) { // when bin code (%)
		*type = TK_OBJ;
		return ExpandBin( (char *)vs + 1, &a );
	}

	if ( a1 < 0x30 ) { // space,tab
		*type = TK_CODE;
		vs++;
		if ( wrtbuf != nullptr ) {
			wrtbuf->Put( (char)a1 );
		}
		return (char *)vs;
	}

#ifdef HSPWIN
	if ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) {
		if ( compopt->pp_utf8 ) {
			if ( a1 == 0xe3 && vs[1] == 0x80 && vs[2] == 0x80 ) { // 全角スペースを半角スペースに変換する(UTF8)
				*type = TK_CODE;
				vs += 3;
				if ( wrtbuf != NULL ) {
					wrtbuf->Put( (char)0x20 );
				}
				return (char *)vs;
			}
		} else {
			if ( a1 == 0x81 && vs[1] == 0x40 ) { // 全角スペースを半角スペースに変換する(SJIS)
				*type = TK_CODE;
				vs += 2;
				if ( wrtbuf != NULL ) {
					wrtbuf->Put( (char)0x20 );
				}
				return (char *)vs;
			}
		}
	}
#endif

	chk = 0;
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
		vs++;
		if ( wrtbuf != nullptr ) {
			wrtbuf->Put( (char)a1 ); // 記号
		}
		*type = a1;
		return (char *)vs;
	}

	if ( ( a1 >= 0x30 ) && ( a1 <= 0x39 ) ) { // when 0-9 numerical
		a = 0;
		flcnt = 0;
		while ( true ) {
			a1 = *vs;
			if ( a1 == '.' ) {
				flcnt++;
				if ( flcnt > 1 ) {
					break;
				}
			} else {
				if ( ( a1 < 0x30 ) || ( a1 > 0x39 ) ) {
					break;
				}
			}
			s2[a++] = a1;
			vs++;
		}
		if ( ( a1 == 'k' ) || ( a1 == 'f' ) || ( a1 == 'd' ) ) {
			s2[a++] = a1;
			vs++;
		}
		if ( a1 == 'e' ) {
			s2[a++] = a1;
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
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutData( s2, a );
		}
		*type = TK_OBJ;
		return (char *)vs;
	}

	a = 0;
	vs_modbrk = nullptr;

	/*
		if ( ppmode ) {					// プリプロセッサ時は#を含めてキーワードとする
			s2[a++]='#';
		}
	*/

	//		半角スペースの検出
	//
#ifdef HSPWIN
	if ( ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) == 0 ) {
		if ( strncmp( (char *)s2, "　", 2 ) == 0 ) {
			logger->SetError( "SJIS space code error" );
			*type = TK_ERROR;
			return (char *)vs;
		}
	}
#endif


	//	 シンボル取り出し
	//
	while ( true ) {
		int skip;
		int i;
		a1 = *vs;
		// if ((a1>='A')&&(a1<='Z')) a1+=0x20;		// to lower case

		skip = SkipMultiByte( a1 ); // 全角文字チェック
		if ( skip != 0 ) {

#ifdef HSPWIN
			if ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) {
				if ( a1 == 0x81 && vs[1] == 0x40 ) { // 全角スペースは終端と判断
					break;
				}
			}
#endif
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

		//		if ( a1=='@' ) if ( *vs==0 ) {
		//			vs_modbrk = s2+a;
		//		}
		if ( a < OBJNAME_MAX ) {
			s2[a++] = a1;
		}
	}
	s2[a] = 0;

	if ( *s2 == '@' ) {
		if ( wrtbuf != nullptr ) {
			wrtbuf->PutData( s2, a );
		}
		*type = TK_CODE;
		return (char *)vs;
	}


	//		シンボル検索
	//
	strcase2( (char *)s2, fixname );

	//	if ( vs_modbrk != NULL ) *vs_modbrk = 0;
	FixModuleName( (char *)s2 );
	AddModuleName( fixname );

	id = symtab.lb->SearchLocal( (char *)s2, fixname );
	if ( id != -1 ) {
		ltype = symtab.lb->GetType( id );
		switch ( ltype ) {
		case LAB_TYPE_PPVAL: {
			//		constマクロ展開
			GenerateLabelListAndTagRefPP( fixname, LABBUF_FLAG_MACRO );
			char *ptr_dval;
			ptr_dval = symtab.lb->GetData2( id );
			if ( ptr_dval == nullptr ) {
				sprintf( cnvstr, "%d", symtab.lb->GetOpt( id ) );
			} else {
				sprintf( cnvstr, "%.16f", *(CALCVAR *)ptr_dval );
			}
			chk = ReplaceLineBuf( str, (char *)vs, cnvstr, 0, nullptr );
			break;
		}

		case LAB_TYPE_PPINTMAC:
			//		内部マクロ
			//

			if ( ppmode != 0 ) { //	プリプロセッサ時はそのまま展開
				if ( wrtbuf != nullptr ) {
					FixModuleName( (char *)s2 );
					wrtbuf->PutStr( (char *)s2 );
				}
				*type = TK_OBJ;
				return (char *)vs;
			}

		case LAB_TYPE_PPMAC:
			//		マクロ展開
			//
			vs_bak = vs;
			while ( true ) { // 直後のspace/tabを除去
				a1 = *vs_bak;
				if ( ( a1 != 32 ) && ( a1 != 9 ) ) {
					break;
				}
				vs_bak++;
			}
			opt = symtab.lb->GetOpt( id );
			if ( ( a1 == '=' ) && ( ( opt & PRM_MASK ) != 0 ) ) { // マクロに代入しようとした場合のエラー
				logger->SetError( "Reserved word syntax error" );
				*type = TK_ERROR;
				return (char *)vs;
			}
			//
			macptr = symtab.lb->GetData( id );
			if ( macptr == nullptr ) {
				*cnvstr = 0;
				macptr = cnvstr;
			}
			if ( ltype == LAB_TYPE_PPMAC ) {
				GenerateLabelListAndTagRefPP( fixname, LABBUF_FLAG_MACRO );
			}
			chk = ReplaceLineBuf( str, (char *)vs, macptr, opt, (MACDEF *)symtab.lb->GetData2( id ) );
			break;
		case LAB_TYPE_PPDLLFUNC:
			//		モジュール名付き展開キーワード
			if ( wrtbuf != nullptr ) {
				//				AddModuleName( (char *)s2 );
				if ( symtab.lb->GetEternal( id ) != 0 ) {
					FixModuleName( (char *)s2 );
					wrtbuf->PutStr( (char *)s2 );
				} else {
					wrtbuf->PutStr( fixname );
				}
			}
			*type = TK_OBJ;
			if ( *modname == 0 ) {
				symtab.lb->AddReference( id );
			} else {
				int i;
				i = symtab.lb->Search( GetModuleName() );
				if ( symtab.lb->SearchRelation( id, i ) == 0 ) {
					symtab.lb->AddRelation( id, i );
				}
			}
			return (char *)vs;
			break;
		case LAB_TYPE_COMVAR:
			//		COMキーワードを展開
			if ( wrtbuf != nullptr ) {
				if ( symtab.lb->GetEternal( id ) != 0 ) {
					FixModuleName( (char *)s2 );
					wrtbuf->PutStr( (char *)s2 );
				} else {
					wrtbuf->PutStr( fixname );
				}
			}
			*type = TK_OBJ;
			symtab.lb->AddReference( id );
			return (char *)vs;

		case LAB_TYPE_PPMODFUNC:
		default:
			//		通常キーワードはそのまま展開
			if ( wrtbuf != nullptr ) {
				if ( symtab.lb->GetEternal( id ) == 0 ) { // local func
					strcpy( (char *)s2, symtab.lb->GetName( id ) );
				}
				FixModuleName( (char *)s2 );
				wrtbuf->PutStr( (char *)s2 );
			}
			*type = TK_OBJ;
			symtab.lb->AddReference( id );
			return (char *)vs;
		}
		if ( chk != 0 ) {
			*type = TK_ERROR;
			return str;
		}
		*type = TK_OBJ;
		return str;
	}

	//		登録されていないキーワードを展開
	//
	if ( wrtbuf != nullptr ) {
		//		AddModuleName( (char *)s2 );
		if ( strcmp( (char *)s2, fixname ) != 0 ) {
			//	後ろで定義されている関数の呼び出しのために
			//	モジュール内で@をつけていない識別子の位置を記録する
			undefined_symbol_t sym;
			sym.pos = wrtbuf->GetSize();
			sym.len_include_modname = (int)strlen( fixname );
			sym.len = (int)strlen( (char *)s2 );
			undefined_symbols.push_back( sym );
		}
		wrtbuf->PutStr( fixname );
		//		wrtbuf->Put( '?' );
	}
	*type = TK_OBJ;
	return (char *)vs;
}


char *CPreProcessor::SkipLine( char *str, int *pline )
{
	//		strから改行までをスキップする
	//		( 行末に「\」で次行を接続 )
	//
	unsigned char *vs;
	unsigned char a1;
	unsigned char a2;
	vs = (unsigned char *)str;
	a2 = 0;
	while ( true ) {
		a1 = *vs;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 13 ) {
			pline[0]++;
			vs++;
			if ( *vs == 10 ) {
				vs++;
			}
			if ( a2 != 0x5c ) {
				break;
			}
			continue;
		}
		if ( a1 == 10 ) {
			pline[0]++;
			vs++;
			if ( a2 != 0x5c ) {
				break;
			}
			continue;
		}
		if ( ( a1 < 32 ) && ( a1 != 9 ) ) {
			break;
		}
		vs++;
		a2 = a1;
	}
	return (char *)vs;
}


char *CPreProcessor::SendLineBuf( char *str )
{
	//		１行分のデータをlinebufに転送
	//
	char *p;
	char *w;
	char a1;
	p = str;
	w = linebuf;
	while ( true ) {
		a1 = *p;
		if ( a1 == 0 ) {
			break;
		}
		p++;
		if ( a1 == 10 ) {
			break;
		}
		if ( a1 == 13 ) {
			if ( *p == 10 ) {
				p++;
			}
			break;
		}
		*w++ = a1;
	}
	*w = 0;
	return p;
}


char *CPreProcessor::SendLineBufPP( char *str, int *lines )
{
	//		１行分のデータをlinebufに転送
	//			(行末の'\'は継続 linesに行数を返す)
	//
	unsigned char *p;
	unsigned char *w;
	unsigned char a1;
	unsigned char a2;
	int ln;
	int i;
	int skip;
	p = (unsigned char *)str;
	w = (unsigned char *)linebuf;
	a2 = 0;
	ln = 0;
	while ( true ) {
		a1 = *p;
		if ( a1 == 0 ) {
			break;
		}
		p++;
		if ( a1 == 10 ) {
			if ( a2 == 0x5c ) {
				ln++;
				w--;
				a2 = 0;
				continue;
			}
			break;
		}
		if ( a1 == 13 ) {
			if ( a2 == 0x5c ) {
				if ( *p == 10 ) {
					p++;
				}
				ln++;
				w--;
				a2 = 0;
				continue;
			}
			if ( *p == 10 ) {
				p++;
			}
			break;
		}
		*w++ = a1;
		a2 = a1;
		//	skip multibyte
		skip = SkipMultiByte( a1 );
		if ( skip > 0 ) {
			for ( i = 0; i < skip; i++ ) {
				a1 = *p;
				if ( a1 == 0 ) {
					break;
				}
				p++;
				*w++ = a1;
			}
			a2 = 0;
		}
	}
	*w = 0;
	*lines = ln;
	return (char *)p;
}


char *CPreProcessor::ExpandStrComment2( char *str )
{
	//		"*/" で終端していない場合は NULL を返す
	//
	int mulstr_bak = mulstr;
	mulstr = LMODE_COMMENT;
	char *result = ExpandStrComment( str, 1 );
	if ( mulstr == LMODE_COMMENT ) {
		result = nullptr;
	}
	mulstr = mulstr_bak;
	return result;
}


int CPreProcessor::ReplaceLineBuf( char *str1, char *str2, char *repl, int opt, MACDEF *macdef )
{
	//		linebufのキーワードを置き換え
	//		(linetmpを破壊します)
	//			str1 : 置き換え元キーワード先頭(linebuf内)
	//			str2 : 置き換え元キーワード次ptr(linebuf内)
	//			repl : 置き換えキーワード
	//			macopt : マクロ添字の数
	//
	//		return : 0=ok/1=error
	//
	char *w;
	char *w2;
	char *p;
	char *endp;
	char *prm[32];
	char *prme[32];
	char *last;
	char *macbuf;
	char *macbuf2;
	char a1;
	char dummy[4];
	char mactmp[128];
	int i;
	int flg;
	int type;
	int cnvfnc;
	int tagid;
	int stklevel;
	int macopt;
	int ctype;
	int noprm;
	int kakko;

	i = 0;
	flg = 1;
	cnvfnc = 0;
	ctype = 0;
	kakko = 0;
	macopt = opt & PRM_MASK;
	if ( ( opt & PRM_FLAG_CTYPE ) != 0 ) {
		ctype = 1;
	}
	*dummy = 0;
	strcpy( linetmp, str2 );
	lexer.wp = (unsigned char *)linetmp;
	if ( ( ( macopt ) != 0 ) || ( ( ctype ) != 0 ) ) {
		p = (char *)lexer.wp;
		type = lexer.GetToken();
		if ( ctype != 0 ) {
			if ( type != '(' ) {
#ifdef JPNMSG
				logger->SetError( "ctypeマクロの直後には、丸括弧でくくられた引数リストが必要です" );
#else
				logger->SetError( "C-Type macro syntax error" );
#endif
				return 4;
			}
			p = (char *)lexer.wp;
			type = lexer.GetToken();
		}
		if ( type != TK_NONE ) {
			lexer.wp = (unsigned char *)p;
			prm[i] = p;
			while ( true ) { // マクロパラメータを取り出す
				p = (char *)lexer.wp;
				type = lexer.GetToken();
				if ( type == ';' ) {
					type = TK_SEPARATE;
				}
				if ( type == '}' ) {
					type = TK_SEPARATE;
				}
				if ( type == '/' ) { // Cコメント??
					if ( *lexer.wp == '/' ) {
						type = TK_SEPARATE;
					}
					if ( *lexer.wp == '*' ) {
						char *start = (char *)lexer.wp - 1;
						char *end = ExpandStrComment2( start + 2 );
						if ( end == nullptr ) { // 範囲コメントが次の行まで続いている
							type = TK_SEPARATE;
						} else {
							lexer.wp = (unsigned char *)end;
						}
					}
				}
				if ( flg != 0 ) {
					flg = 0;
					prm[i] = p;
					if ( type == TK_NONE ) {
						prme[i++] = p;
						break;
					}
				}
				if ( type == TK_SEPARATE ) {
					lexer.wp = (unsigned char *)p;
					prme[i++] = (char *)lexer.wp;
					break;
				}
				if ( lexer.wp == nullptr ) {
					prme[i++] = nullptr;
					break;
				}
				if ( type == ',' ) {
					if ( kakko == 0 ) { // カッコに囲まれている場合は無視する
						prme[i] = p;
						flg = 1;
						i++;
					}
				}
				if ( ctype == 0 ) { // 通常時のカッコ処理
					if ( type == '(' ) {
						kakko++;
					}
					if ( type == ')' ) {
						kakko--;
					}
				} else { // Cタイプ時のカッコ処理
					if ( type == '(' ) {
						kakko++;
						ctype++;
					}
					if ( type == ')' ) {
						kakko--;
						if ( ctype == 1 ) {
							lexer.wp = (unsigned char *)p;
							prme[i++] = (char *)lexer.wp;
							while ( true ) {
								if ( ( *lexer.wp != 32 ) && ( *lexer.wp != 9 ) ) {
									break;
								}
								lexer.wp++;
							}
							*lexer.wp = 32; // ')'をspaceに
							break;
						}
						ctype--;
					}
				}
			}
		}

		if ( i > macopt ) {
			noprm = 1;
			if ( ( ( ctype ) != 0 ) && ( i == 1 ) && ( macopt == 0 ) && ( prm[0] == prme[0] ) ) {
				noprm = 0;
			}
			if ( noprm != 0 ) {
#ifdef JPNMSG
				logger->SetError( "マクロの引数が多すぎます" );
#else
				logger->SetError( "too many macro parameters" );
#endif
				return 3;
			}
		}
		while ( true ) { // 省略パラメータを補完
			if ( i >= macopt ) {
				break;
			}
			prm[i] = dummy;
			prme[i] = dummy;
			i++;
		}
		//		{ int a;for(a=0;a<i;a++) {
		//			sprintf( errtmp,"[%d][%s]",a,prm[a] );Alert( errtmp );
		//		} }
	}
	last = (char *)lexer.wp;

	tagid = 0x10000;
	w = str1;
	lexer.wp = (unsigned char *)repl;
	while ( true ) { // マクロ置き換え
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( w >= linetmp ) {
			logger->SetError( "macro buffer overflow" );
			return 4;
		}
		a1 = *lexer.wp++;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == '%' ) {
			if ( *lexer.wp == '%' ) {
				*w++ = a1;
				lexer.wp++;
				continue;
			}
			type = lexer.GetToken();
			int prmval = token->val;
			if ( type == TK_OBJ ) { // 特殊コマンドラベル処理
				macbuf = mactmp;
				*mactmp = 0;
				a1 = tolower( (int)*token->s3 );
				switch ( a1 ) {
				case 't': // %tタグ名
					tagid = tstack->GetTagID( (char *)( token->s3 + 1 ) );
					break;
				case 'i':
					tstack->GetTagUniqueName( tagid, mactmp );
					tstack->PushTag( tagid, mactmp );
					if ( token->s3[1] == '0' ) {
						*mactmp = 0;
					}
					break;
				case 's':
					prmval = (int)( token->s3[1] - 48 ) - 1;
					if ( 0 > prmval || prmval > macopt - 1 ) {
						logger->SetError( "illegal macro parameter %s" );
						return 2;
					}
					w2 = mactmp;
					p = prm[prmval];
					endp = prme[prmval];
					if ( p == endp ) { // 値省略時
						macbuf2 = macdef->data + macdef->index[prmval];
						while ( true ) {
							a1 = *macbuf2++;
							if ( a1 == 0 ) {
								break;
							}
							*w2++ = a1;
						}
					} else {
						while ( true ) { // %numマクロ展開
							if ( p == endp ) {
								break;
							}
							a1 = *p++;
							if ( a1 == 0 ) {
								break;
							}
							*w2++ = a1;
						}
					}
					*w2 = 0;
					tstack->PushTag( tagid, mactmp );
					*mactmp = 0;
					break;
				case 'n':
					tstack->GetTagUniqueName( tagid, mactmp );
					break;
				case 'p':
					stklevel = (int)( token->s3[1] - 48 );
					if ( ( stklevel < 0 ) || ( stklevel > 9 ) ) {
						stklevel = 0;
					}
					macbuf = tstack->LookupTag( tagid, stklevel );
					break;
				case 'o':
					if ( token->s3[1] != '0' ) {
						macbuf = tstack->PopTag( tagid );
					} else {
						tstack->PopTag( tagid );
					}
					break;
				case 'c':
					mactmp[0] = 0x0d;
					mactmp[1] = 0x0a;
					mactmp[2] = 0;
					break;
				default:
					macbuf = nullptr;
					break;
				}
				if ( macbuf == nullptr ) {
					sprintf( mactmp, "macro syntax error [%s]", tstack->GetTagName( tagid ) );
					logger->SetError( mactmp );
					return 2;
				}
				while ( true ) { // mactmp展開
					a1 = *macbuf++;
					if ( a1 == 0 ) {
						break;
					}
					*w++ = a1;
				}
				if ( lexer.wp != nullptr ) {
					a1 = *lexer.wp;
					if ( a1 == ' ' ) {
						lexer.wp++; // マクロ後のspace除去
					}
				}
				continue;
			}
			if ( type != TK_NUM ) {
				logger->SetError( "macro parameter invalid" );
				return 1;
			}
			prmval--;
			if ( 0 > prmval || prmval > macopt - 1 ) {
				logger->SetError( "illegal macro parameter" );
				return 2;
			}
			p = prm[prmval];
			endp = prme[prmval];
			if ( p == endp ) { // 値省略時
				macbuf = macdef->data + macdef->index[prmval];
				if ( *macbuf == 0 ) {
#ifdef JPNMSG
					logger->SetError( "デフォルトパラメータのないマクロの引数は省略できません" );
#else
					logger->SetError( "no default parameter" );
#endif
					return 5;
				}
				while ( true ) {
					a1 = *macbuf++;
					if ( a1 == 0 ) {
						break;
					}
					*w++ = a1;
				}
				continue;
			}
			while ( true ) { // %numマクロ展開
				if ( p == endp ) {
					break;
				}
				a1 = *p++;
				if ( a1 == 0 ) {
					break;
				}
				*w++ = a1;
			}
			continue;
		}
		*w++ = a1;
	}
	*w = 0;
	if ( last != nullptr ) {
		if ( w + strlen( last ) + 1 >= linetmp ) {
			logger->SetError( "macro buffer overflow" );
			return 4;
		}
		strcpy( w, last );
	}
	return 0;
}


ppresult_t CPreProcessor::PP_SwitchStart( int sw )
{
	if ( swsp == 0 ) {
		swflag = 1;
		swlevel = LMODE_ON;
	}
	if ( swsp >= SWSTACK_MAX ) {
		logger->SetError( "#if nested too deeply" );
		return PPRESULT_ERROR;
	}
	swstack[swsp] = swflag;	  // 有効フラグ
	swstack2[swsp] = swmode;  // elseモード
	swstack3[swsp] = swlevel; // ON/OFF
	swsp++;
	swmode = 0;
	if ( swflag == 0 ) {
		return PPRESULT_SUCCESS;
	}
	if ( sw == 0 ) {
		swlevel = LMODE_OFF;
	} else {
		swlevel = LMODE_ON;
	}
	mulstr = swlevel;
	if ( mulstr == LMODE_OFF ) {
		swflag = 0;
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_SwitchEnd()
{
	if ( swsp == 0 ) {
		logger->SetError( "#endif without #if" );
		return PPRESULT_ERROR;
	}
	swsp--;
	swflag = swstack[swsp];
	swmode = swstack2[swsp];
	swlevel = swstack3[swsp];
	if ( swflag != 0 ) {
		mulstr = swlevel;
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_SwitchReverse()
{
	if ( swsp == 0 ) {
		logger->SetError( "#else without #if" );
		return PPRESULT_ERROR;
	}
	if ( swmode != 0 ) {
		logger->SetError( "#else after #else" );
		return PPRESULT_ERROR;
	}
	if ( swstack[swsp - 1] == 0 ) {
		return PPRESULT_SUCCESS; // 上のスタックが無効なら無視
	}
	swmode = 1;
	if ( swlevel == LMODE_ON ) {
		swlevel = LMODE_OFF;
	} else {
		swlevel = LMODE_ON;
	}
	mulstr = swlevel;
	swflag ^= 1;
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_IncludeSub( char *word, int is_addition )
{
	char tmp_spath[HSP_MAX_PATH];
	int add_bak;

	incinf++;
	if ( incinf > 32 ) {
		logger->SetError( "too many include level" );
		return PPRESULT_ERROR;
	}
	strcpy( tmp_spath, search_path );
	if ( is_addition != 0 ) {
		add_bak = SetAdditionMode( 1 );
	}
	int res = ExpandFile( wrtbuf, word, word );
	if ( is_addition != 0 ) {
		SetAdditionMode( add_bak );
	}
	strcpy( search_path, tmp_spath );
	incinf--;
	if ( res != 0 ) {
		if ( ( is_addition != 0 ) && res == -1 ) {
			return PPRESULT_SUCCESS;
		}
		return PPRESULT_ERROR;
	}
	return PPRESULT_INCLUDED;
}


ppresult_t CPreProcessor::PP_Include( int is_addition )
{
	char *word = (char *)token->s3;
	int type = lexer.GetToken();
	switch ( type ) {
	case TK_STRING:
		return PP_IncludeSub( word, is_addition );
	case TK_OBJ:
		strcat( word, ".as" );
		return PP_IncludeSub( word, is_addition );
	default:
		break;
	}

	if ( is_addition != 0 ) {
		logger->SetError( "invalid addition suffix" );
	} else {
		logger->SetError( "invalid include suffix" );
	}
	return PPRESULT_ERROR;
}


ppresult_t CPreProcessor::PP_use()
{
	char strtmp[1024];
	char *word = (char *)token->s3;
	int i;
	ppresult_t myres = PPRESULT_SUCCESS;
	CMemBuf inclist;
	CMemBuf *bak_wrtbuf;
	CStrNote note;

	while ( true ) {
		i = lexer.GetToken();
		switch ( i ) {
		case TK_OBJ:
			inclist.PutStr( word );
			inclist.PutCR();
			break;
		default:
			sprintf( strtmp, "invalid use suffix [%s]", word );
			logger->SetError( strtmp );
			return PPRESULT_ERROR;
		}
		if ( lexer.wp == nullptr ) {
			break;
		}

		i = lexer.GetToken();
		if ( i == TK_NONE ) {
			break;
		}
		if ( i != ',' ) {
			logger->SetError( "invalid use syntax" );
			return PPRESULT_ERROR;
		}
	}
	inclist.Put( 0 );

	bak_wrtbuf = wrtbuf;

	note.Select( inclist.GetBuffer() );
	int max = note.GetMaxLine();
	for ( i = 0; i < max; i++ ) {
		note.GetLine( strtmp, i );
		strcat( strtmp, ".as" );
		ppresult_t res = PP_IncludeSub( strtmp, 1 );
		if ( res == PPRESULT_SUCCESS ) {
			note.GetLine( strtmp, i );
			strcat( strtmp, ".hsp" );
			res = PP_IncludeSub( strtmp, 1 );
		}
		wrtbuf = bak_wrtbuf;
		if ( ( res == PPRESULT_ERROR ) || ( res == PPRESULT_SUCCESS ) ) {
			return PPRESULT_ERROR;
		}
		myres = PPRESULT_INCLUDED;
	}
	return myres;
}

ppresult_t CPreProcessor::PP_Const()
{
	//		#const解析
	//
	enum ConstType
	{
		Indeterminate,
		Double,
		Int
	};
	ConstType valuetype = ConstType::Indeterminate;

	char *word;
	int id;
	int res;
	int glmode;
	char keyword[256];
	char strtmp[512];
	CALCVAR cres;
	glmode = 0;
	word = (char *)token->s3;
	if ( lexer.GetToken() != TK_OBJ ) {
		sprintf( strtmp, "invalid symbol [%s]", word );
		logger->SetError( strtmp );
		return PPRESULT_ERROR;
	}

	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad global syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
		strcase( word );
	}

	// 型指定キーワード
	if ( tstrcmp( word, "double" ) != 0 ) {
		valuetype = ConstType::Double;
	} else if ( tstrcmp( word, "int" ) != 0 ) {
		valuetype = ConstType::Int;
	}
	if ( valuetype != ConstType::Indeterminate ) {
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad #const syntax" );
			return PPRESULT_ERROR;
		}
		strcase( word );
	}

	strcpy( keyword, word );
	if ( glmode != 0 ) {
		FixModuleName( keyword );
	} else {
		AddModuleName( keyword );
	}
	res = symtab.lb->Search( keyword );
	if ( res != -1 ) {
		SetErrorSymbolOverdefined( keyword, res );
		return PPRESULT_ERROR;
	}
	GenerateLabelListAndTagPP( keyword, LABBUF_FLAG_MACRO );

	if ( Calc( cres ) != 0 ) {
		return PPRESULT_ERROR;
	}

	//		AHT keyword check
	if ( ahtkeyword != nullptr ) {

		if ( ahtbuf != nullptr ) { // AHT出力時
			AHTPROP *prop;
			CALCVAR dbval;
			prop = ahtmodel->GetProperty( keyword );
			if ( prop != nullptr ) {
				id = symtab.lb->Regist( keyword, LAB_TYPE_PPVAL, prop->GetValueInt() );
				if ( cres != floor( cres ) ) {
					dbval = prop->GetValueDouble();
					symtab.lb->SetData2( id, (char *)( &dbval ), sizeof( CALCVAR ) );
				}
				if ( glmode != 0 ) {
					symtab.lb->SetEternal( id );
				}
				return PPRESULT_SUCCESS;
			}
		} else { // AHT読み出し時
			if ( cres != floor( cres ) ) {
				ahtmodel->SetPropertyDefaultDouble( keyword, (double)cres );
			} else {
				ahtmodel->SetPropertyDefaultInt( keyword, (int)cres );
			}
			if ( ahtmodel->SetAHTPropertyString( keyword, ahtkeyword ) != 0 ) {
				logger->SetError( "AHT parameter syntax error" );
				return PPRESULT_ERROR;
			}
		}
	}


	id = symtab.lb->Regist( keyword, LAB_TYPE_PPVAL, (int)cres );
	if ( valuetype == ConstType::Double || ( valuetype == ConstType::Indeterminate && cres != floor( cres ) ) ) {
		symtab.lb->SetData2( id, (char *)( &cres ), sizeof( CALCVAR ) );
	}
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}

	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_Enum()
{
	//		#enum解析
	//
	char *word;
	int id;
	int res;
	int glmode;
	CALCVAR cres;
	char keyword[256];
	char strtmp[512];
	glmode = 0;
	word = (char *)token->s3;
	if ( lexer.GetToken() != TK_OBJ ) {
		sprintf( strtmp, "invalid symbol [%s]", word );
		logger->SetError( strtmp );
		return PPRESULT_ERROR;
	}

	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad global syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
		strcase( word );
	}

	strcpy( keyword, word );
	if ( glmode != 0 ) {
		FixModuleName( keyword );
	} else {
		AddModuleName( keyword );
	}
	res = symtab.lb->Search( keyword );
	if ( res != -1 ) {
		SetErrorSymbolOverdefined( keyword, res );
		return PPRESULT_ERROR;
	}
	GenerateLabelListAndTagPP( keyword, LABBUF_FLAG_MACRO );

	if ( lexer.GetToken() == '=' ) {
		if ( Calc( cres ) != 0 ) {
			return PPRESULT_ERROR;
		}
		enumgc = (int)cres;
	}
	res = enumgc++;
	id = symtab.lb->Regist( keyword, LAB_TYPE_PPVAL, res );
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_VarFix( char *rootword )
{
	//		#var解析
	//
	int i;
	int prm;
	int glmode;
	char *word;
	word = (char *)token->s3;

	wrtbuf->PutStrf( "#%s ", rootword );
	prm = 0;
	glmode = 0;

	while ( true ) {
		i = lexer.GetToken();
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid variable name" );
			return PPRESULT_ERROR;
		}
		strcase( word );
		if ( prm == 0 ) {
			if ( tstrcmp( word, "global" ) != 0 ) { // global macro
				glmode = 1;
				i = lexer.GetToken();
				if ( lexer.wp == nullptr ) {
					break;
				}
				if ( i != TK_OBJ ) {
					logger->SetError( "invalid variable name" );
					return PPRESULT_ERROR;
				}
				strcase( word );
			}
		}
		if ( glmode != 0 ) {
			FixModuleName( word );
		} else {
			AddModuleName( word );
		}
		if ( prm > 0 ) {
			wrtbuf->PutStrf( "," );
		}
		wrtbuf->PutStrf( word );

		if ( lexer.wp == nullptr ) {
			break;
		}

		i = lexer.GetToken();
		if ( i == TK_NONE ) {
			break;
		}
		if ( i != ',' ) {
			logger->SetError( "invalid variable syntax" );
			return PPRESULT_ERROR;
		}
		prm++;
	}

	wrtbuf->PutCR();
	return PPRESULT_WROTE_LINE;
}


/*
	rev 54
	mingw : warning : 比較は常に…
	に対処。
*/

char *CPreProcessor::CheckValidWord()
{
	//		行末までにコメントがあるか調べる
	//			( return : 有効文字列の先頭ポインタ )
	//
	char *res;
	char *p;
	char *p2;
	unsigned char a1;
	int qqflg;
	int qqchr;
	res = (char *)lexer.wp;
	if ( res == nullptr ) {
		return res;
	}
	qqflg = 0;
	p = res;
	while ( true ) {
		a1 = *p;
		if ( a1 == 0 ) {
			break;
		}


		if ( qqflg == 0 ) { // コメント検索フラグ

			if ( a1 == 0x22 ) {
				qqflg = 1;
				qqchr = a1;
			}
			if ( a1 == 0x27 ) {
				qqflg = 1;
				qqchr = a1;
			}
			if ( a1 == ';' ) { // コメント
				*p = 0;
				break;
			}
			if ( a1 == '/' ) { // Cコメント
				if ( p[1] == '/' ) {
					*p = 0;
					break;
				}
				if ( p[1] == '*' ) {
					mulstr = LMODE_COMMENT;
					p2 = ExpandStrComment( (char *)p + 2, 1 );
					while ( true ) {
						if ( p >= p2 ) {
							break;
						}
						*p++ = 32; // コメント部分をspaceに
					}
					continue;
				}
			}
		} else {				// 文字列中はコメント検索せず
			if ( a1 == 0x5c ) { // '\'チェック
				p++;
				a1 = *p;
				if ( a1 >= 32 ) {
					p++;
				}
				continue;
			}
			if ( a1 == qqchr ) {
				qqflg = 0;
			}
		}

		p += SkipMultiByte( a1 ); // 全角文字チェック
		p++;
	}
	return res;
}


ppresult_t CPreProcessor::PP_Define()
{
	//		#define解析
	//
	char *word;
	char *wdata;
	int id;
	int res;
	int type;
	int prms;
	int flg;
	int glmode;
	int ctype;
	char a1;
	MACDEF *macdef;
	int macptr;
	char *macbuf;
	char keyword[256];
	char strtmp[512];

	glmode = 0;
	ctype = 0;
	word = (char *)token->s3;
	if ( lexer.GetToken() != TK_OBJ ) {
		sprintf( strtmp, "invalid symbol [%s]", word );
		logger->SetError( strtmp );
		return PPRESULT_ERROR;
	}

	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad macro syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
		strcase( word );
	}
	if ( tstrcmp( word, "ctype" ) != 0 ) { // C-type macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad macro syntax" );
			return PPRESULT_ERROR;
		}
		ctype = 1;
		strcase( word );
		if ( tstrcmp( word, "global" ) != 0 ) { // global macro
			if ( lexer.GetToken() != TK_OBJ || glmode == 1 ) {
				logger->SetError( "bad macro syntax" );
				return PPRESULT_ERROR;
			}
			glmode = 1;
			strcase( word );
		}
	}
	strcpy( keyword, word );
	if ( glmode != 0 ) {
		FixModuleName( keyword );
	} else {
		AddModuleName( keyword );
	}
	res = symtab.lb->Search( keyword );
	if ( res != -1 ) {
		SetErrorSymbolOverdefined( keyword, res );
		return PPRESULT_ERROR;
	}
	GenerateLabelListAndTagPP( keyword, LABBUF_FLAG_MACRO );

	//		skip space,tab code
	if ( lexer.wp == nullptr ) {
		a1 = 0;
	} else {
		a1 = *lexer.wp;
		if ( a1 != '(' ) {
			a1 = 0;
		}
	}

	if ( a1 == 0 ) { // no parameters
		prms = 0;
		if ( ctype != 0 ) {
			prms |= PRM_FLAG_CTYPE;
		}
		wdata = CheckValidWord();

		//		AHT keyword check
		if ( ahtkeyword != nullptr ) {
			if ( ahtbuf != nullptr ) { // AHT出力時
				AHTPROP *prop;
				prop = ahtmodel->GetProperty( keyword );
				if ( prop != nullptr ) {
					wdata = prop->GetOutValue();
				}
			} else { // AHT読み込み時
				AHTPROP *prop;
				prop = ahtmodel->SetPropertyDefault( keyword, wdata );
				if ( ahtmodel->SetAHTPropertyString( keyword, ahtkeyword ) != 0 ) {
					logger->SetError( "AHT parameter syntax error" );
					return PPRESULT_ERROR;
				}
				if ( ( prop->ahtmode & AHTMODE_OUTPUT_RAW ) != 0 ) {
					ahtmodel->SetPropertyDefaultStr( keyword, wdata );
				}
			}
		}

		id = symtab.lb->Regist( keyword, LAB_TYPE_PPMAC, prms );
		symtab.lb->SetData( id, wdata );
		if ( glmode != 0 ) {
			symtab.lb->SetEternal( id );
		}

		return PPRESULT_SUCCESS;
	}

	//		パラメータ定義取得
	//
	macdef = (MACDEF *)linetmp;
	macdef->data[0] = 0;
	macptr = 1; // デフォルトマクロデータ参照オフセット
	lexer.wp++;
	prms = 0;
	flg = 0;
	while ( true ) {
		if ( lexer.wp == nullptr ) {
			goto bad_macro_param_expr;
		}
		a1 = *lexer.wp++;
		if ( a1 == ')' ) {
			if ( flg == 0 ) {
				goto bad_macro_param_expr;
			}
			prms++;
			break;
		}
		switch ( a1 ) {
		case 9:
		case 32:
			break;
		case ',':
			if ( flg == 0 ) {
				goto bad_macro_param_expr;
			}
			prms++;
			flg = 0;
			break;
		case '%':
			if ( flg != 0 ) {
				goto bad_macro_param_expr;
			}
			type = lexer.GetToken();
			if ( type != TK_NUM ) {
				goto bad_macro_param_expr;
			}
			if ( token->val != ( prms + 1 ) ) {
				goto bad_macro_param_expr;
			}
			flg = 1;
			macdef->index[prms] = 0; // デフォルト(初期値なし)
			break;
		case '=':
			if ( flg != 1 ) {
				goto bad_macro_param_expr;
			}
			flg = 2;
			macdef->index[prms] = macptr; // 初期値ポインタの設定
			type = lexer.GetToken();
			switch ( type ) {
			case TK_NUM:
#ifdef HSPWIN
				_itoa( token->val, word, 10 );
#else
				sprintf( word, "%d", token->val );
#endif
				break;
			case TK_DNUM:
				strcpy( word, (char *)token->s3 );
				break;
			case TK_STRING:
				sprintf( strtmp, "\"%s\"", word );
				strcpy( word, strtmp );
				break;
			case TK_OBJ:
				break;
			case '-':
				type = lexer.GetToken();
				if ( type == TK_DNUM ) {
					sprintf( strtmp, "-%s", token->s3 );
					strcpy( word, strtmp );
					break;
				}
				if ( type != TK_NUM ) {
					logger->SetError( "bad default value" );
					return PPRESULT_ERROR;
				}
				//_itoa( val, word, 10 );
				sprintf( word, "-%d", token->val );
				break;
			default:
				logger->SetError( "bad default value" );
				return PPRESULT_ERROR;
			}
			macbuf = ( macdef->data ) + macptr;
			res = (int)strlen( word );
			strcpy( macbuf, word );
			macptr += res + 1;
			break;
		default:
			goto bad_macro_param_expr;
		}
	}

	//		skip space,tab code
	if ( lexer.wp == nullptr ) {
		a1 = 0;
	} else {
		while ( true ) {
			a1 = *lexer.wp;
			if ( a1 == 0 ) {
				break;
			}
			if ( ( a1 != 9 ) && ( a1 != 32 ) ) {
				break;
			}
			lexer.wp++;
		}
	}
	if ( a1 == 0 ) {
		logger->SetError( "macro contains no data" );
		return PPRESULT_ERROR;
	}
	if ( ctype != 0 ) {
		prms |= PRM_FLAG_CTYPE;
	}

	//		データ定義
	id = symtab.lb->Regist( keyword, LAB_TYPE_PPMAC, prms );
	wdata = CheckValidWord();
	symtab.lb->SetData( id, wdata );
	symtab.lb->SetData2( id, (char *)macdef, macptr + sizeof( macdef->index ) );
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}

	// sprintf( keyword,"[%d]-[%s]",id,wdata );Alert( keyword );
	return PPRESULT_SUCCESS;

bad_macro_param_expr:
	logger->SetError( "bad macro parameter expression" );
	return PPRESULT_ERROR;
}


ppresult_t CPreProcessor::PP_Defcfunc( int mode )
{
	//		#defcfunc解析
	//			mode : 0 = 通常cfunc
	//			       1 = modcfunc
	//
	int i;
	int id;
	char *word;
	char *mod;
	char fixname[128];
	int glmode;
	int premode;

	word = (char *)token->s3;
	mod = GetModuleName();
	id = -1;
	glmode = 0;
	premode = LAB_TYPE_PPMODFUNC;

	i = lexer.GetToken();
	if ( i == TK_OBJ ) {
		strcase( word );
		if ( tstrcmp( word, "local" ) != 0 ) { // local option
			if ( *mod == 0 ) {
				logger->SetError( "module name not found" );
				return PPRESULT_ERROR;
			}
			glmode = 1;
			i = lexer.GetToken();
		}
		if ( tstrcmp( word, "prep" ) != 0 ) { // prepare option
			premode = LAB_TYPE_PP_PREMODFUNC;
			i = lexer.GetToken();
		}
	}

	strcase2( word, fixname );
	if ( i != TK_OBJ ) {
		logger->SetError( "invalid func name" );
		return PPRESULT_ERROR;
	}
	i = symtab.lb->Search( fixname );
	if ( i != -1 ) {
		if ( symtab.lb->GetFlag( i ) != LAB_TYPE_PP_PREMODFUNC ) {
			SetErrorSymbolOverdefined( fixname, i );
			return PPRESULT_ERROR;
		}
		id = i;
	}

	if ( glmode != 0 ) {
		AddModuleName( fixname );
	}

	if ( premode == LAB_TYPE_PP_PREMODFUNC ) {
		wrtbuf->PutStrf( "#defcfunc prep %s ", fixname );
	} else {
		wrtbuf->PutStrf( "#defcfunc %s ", fixname );
	}

	if ( id == -1 ) {
		id = symtab.lb->Regist( fixname, premode, 0, pp_orgfilefull, pp_orgline );
		if ( glmode == 0 ) {
			symtab.lb->SetEternal( id );
		}
		if ( *mod != 0 ) {
			symtab.lb->AddRelation( mod, id );
		} // モジュールラベルに依存を追加
	} else {
		symtab.lb->SetFlag( id, premode );
	}

	if ( mode != 0 ) {
		if ( mode == 1 ) {
			wrtbuf->PutStr( "modvar " );
		} else {
			wrtbuf->PutStr( "modinit " );
		}
		if ( *mod == 0 ) {
			logger->SetError( "module name not found" );
			return PPRESULT_ERROR;
		}
		wrtbuf->PutStr( mod );
		if ( lexer.wp != nullptr ) {
			wrtbuf->Put( ',' );
		}
	}

	/*
	char resname[512];
	i = lexer.GetToken();
	if ( i != TK_OBJ ) { logger->SetError("invalid result name"); return PPRESULT_ERROR; }
	strcpy( resname, word );
	*/

	while ( true ) {

		i = lexer.GetToken();
		if ( i == TK_OBJ ) {
			wrtbuf->PutStr( word );
		}
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid func param" );
			return PPRESULT_ERROR;
		}

		i = lexer.GetToken();
		if ( i == TK_OBJ ) {
			strcase2( word, fixname );
			AddModuleName( fixname );
			wrtbuf->Put( ' ' );
			wrtbuf->PutStr( fixname );
			i = lexer.GetToken();
		}
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != ',' ) {
			logger->SetError( "invalid func param" );
			return PPRESULT_ERROR;
		}
		wrtbuf->Put( ',' );
	}

	// wrtbuf->PutStr( linebuf );
	wrtbuf->PutCR();
	//
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Deffunc( int mode )
{
	//		#deffunc解析
	//			mode : 0 = 通常func
	//			       1 = modfunc
	//			       2 = modinit
	//			       3 = modterm
	int i;
	int id;
	char *word;
	char *mod;
	char fixname[128];
	int glmode;
	int premode;

	word = (char *)token->s3;
	mod = GetModuleName();
	id = -1;
	glmode = 0;
	premode = LAB_TYPE_PPMODFUNC;

	if ( mode < 2 ) {
		i = lexer.GetToken();
		if ( i == TK_OBJ ) {
			strcase( word );
			if ( tstrcmp( word, "local" ) != 0 ) { // local option
				if ( *mod == 0 ) {
					logger->SetError( "module name not found" );
					return PPRESULT_ERROR;
				}
				glmode = 1;
				i = lexer.GetToken();
			}
			if ( tstrcmp( word, "prep" ) != 0 ) { // prepare option
				premode = LAB_TYPE_PP_PREMODFUNC;
				i = lexer.GetToken();
			}
		}

		strcase2( word, fixname );
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid func name" );
			return PPRESULT_ERROR;
		}
		i = symtab.lb->Search( fixname );
		if ( i != -1 ) {
			if ( symtab.lb->GetFlag( i ) != LAB_TYPE_PP_PREMODFUNC ) {
				SetErrorSymbolOverdefined( fixname, i );
				return PPRESULT_ERROR;
			}
			id = i;
		}

		if ( glmode != 0 ) {
			AddModuleName( fixname );
		}

		if ( premode == LAB_TYPE_PP_PREMODFUNC ) {
			wrtbuf->PutStrf( "#deffunc prep %s ", fixname );
		} else {
			wrtbuf->PutStrf( "#deffunc %s ", fixname );
		}

		if ( id == -1 ) {
			id = symtab.lb->Regist( fixname, premode, 0, pp_orgfilefull, pp_orgline );
			if ( glmode == 0 ) {
				symtab.lb->SetEternal( id );
			}
			if ( *mod != 0 ) {
				symtab.lb->AddRelation( mod, id );
			} // モジュールラベルに依存を追加
		} else {
			symtab.lb->SetFlag( id, premode );
		}

		if ( mode != 0 ) {
			wrtbuf->PutStr( "modvar " );
			if ( *mod == 0 ) {
				logger->SetError( "module name not found" );
				return PPRESULT_ERROR;
			}
			wrtbuf->PutStr( mod );
			if ( lexer.wp != nullptr ) {
				wrtbuf->Put( ',' );
			}
		}

	} else {
		if ( mode == 2 ) {
			wrtbuf->PutStr( "#deffunc __init modinit " );
		} else {
			wrtbuf->PutStr( "#deffunc __term modterm " );
		}
		if ( *mod == 0 ) {
			logger->SetError( "module name not found" );
			return PPRESULT_ERROR;
		}
		wrtbuf->PutStr( mod );
		if ( lexer.wp != nullptr ) {
			wrtbuf->Put( ',' );
		}
	}

	while ( true ) {

		i = lexer.GetToken();
		if ( i == TK_OBJ ) {
			wrtbuf->PutStr( word );
			strcase( word );
			if ( tstrcmp( word, "onexit" ) != 0 ) { // onexitは参照済みにする
				symtab.lb->AddReference( id );
			}
		}

		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid func param" );
			return PPRESULT_ERROR;
		}

		i = lexer.GetToken();
		if ( i == TK_OBJ ) {
			strcase2( word, fixname );
			AddModuleName( fixname );
			wrtbuf->Put( ' ' );
			wrtbuf->PutStr( fixname );
			i = lexer.GetToken();
		}
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != ',' ) {
			logger->SetError( "invalid func param" );
			return PPRESULT_ERROR;
		}
		wrtbuf->Put( ',' );
	}

	// wrtbuf->PutStr( linebuf );
	wrtbuf->PutCR();
	//
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Struct()
{
	//		#struct解析
	//
	char *word;
	int i;
	int id;
	int res;
	int glmode;
	char keyword[256];
	char tagname[256];
	char strtmp[0x4000];
	glmode = 0;
	word = (char *)token->s3;
	if ( lexer.GetToken() != TK_OBJ ) {
		sprintf( strtmp, "invalid symbol [%s]", word );
		logger->SetError( strtmp );
		return PPRESULT_ERROR;
	}

	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad global syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
		strcase( word );
	}

	strcpy( tagname, word );
	if ( glmode != 0 ) {
		FixModuleName( tagname );
	} else {
		AddModuleName( tagname );
	}
	res = symtab.lb->Search( tagname );
	if ( res != -1 ) {
		SetErrorSymbolOverdefined( tagname, res );
		return PPRESULT_ERROR;
	}
	id = symtab.lb->Regist( tagname, LAB_TYPE_PPDLLFUNC, 0 );
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}

	wrtbuf->PutStrf( "#struct %s ", tagname );

	while ( true ) {

		i = lexer.GetToken();
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid struct param" );
			return PPRESULT_ERROR;
		}
		wrtbuf->PutStr( word );
		wrtbuf->Put( ' ' );

		i = lexer.GetToken();
		if ( i != TK_OBJ ) {
			logger->SetError( "invalid struct param" );
			return PPRESULT_ERROR;
		}

		sprintf( keyword, "%s_%s", tagname, word );
		if ( glmode != 0 ) {
			FixModuleName( keyword );
		} else {
			AddModuleName( keyword );
		}
		res = symtab.lb->Search( keyword );
		if ( res != -1 ) {
			SetErrorSymbolOverdefined( keyword, res );
			return PPRESULT_ERROR;
		}
		id = symtab.lb->Regist( keyword, LAB_TYPE_PPDLLFUNC, 0 );
		if ( glmode != 0 ) {
			symtab.lb->SetEternal( id );
		}
		wrtbuf->PutStr( keyword );

		i = lexer.GetToken();
		if ( lexer.wp == nullptr ) {
			break;
		}
		if ( i != ',' ) {
			logger->SetError( "invalid struct param" );
			return PPRESULT_ERROR;
		}
		wrtbuf->Put( ',' );
	}

	wrtbuf->PutCR();
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Func( char *name )
{
	//		#func解析
	//
	int i;
	int id;
	int glmode;
	char *word;
	word = (char *)token->s3;
	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "invalid func name" );
		return PPRESULT_ERROR;
	}

	glmode = 0;
	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad global syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
	}

	if ( glmode != 0 ) {
		FixModuleName( word );
	} else {
		AddModuleName( word );
	}
	// AddModuleName( word );
	i = symtab.lb->Search( word );
	if ( i != -1 ) {
		SetErrorSymbolOverdefined( word, i );
		return PPRESULT_ERROR;
	}
	id = symtab.lb->Regist( word, LAB_TYPE_PPDLLFUNC, 0 );
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}
	//
	wrtbuf->PutStrf( "#%s %s%s", name, word, (char *)lexer.wp );
	wrtbuf->PutCR();
	//
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Cmd( char *name )
{
	//		#cmd解析
	//
	int i;
	int id;
	char *word;
	word = (char *)token->s3;
	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "invalid func name" );
		return PPRESULT_ERROR;
	}
	i = symtab.lb->Search( word );
	if ( i != -1 ) {
		SetErrorSymbolOverdefined( word, i );
		return PPRESULT_ERROR;
	}

	id = symtab.lb->Regist( word, LAB_TYPE_PPINTMAC, 0 ); // 内部マクロとして定義
	strcat( word, "@hsp" );
	symtab.lb->SetData( id, word );
	symtab.lb->SetEternal( id );

	// AddModuleName( word );
	// id = symtab.lb->Regist( word, LAB_TYPE_PPDLLFUNC, 0 );
	// symtab.lb->SetEternal( id );
	//
	wrtbuf->PutStrf( "#%s %s%s", name, word, (char *)lexer.wp );
	wrtbuf->PutCR();
	//
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Usecom()
{
	//		#usecom解析
	//
	int i;
	int id;
	int glmode;
	char *word;
	word = (char *)token->s3;
	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "invalid COM symbol name" );
		return PPRESULT_ERROR;
	}

	glmode = 0;
	strcase( word );
	if ( tstrcmp( word, "global" ) != 0 ) { // global macro
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "bad global syntax" );
			return PPRESULT_ERROR;
		}
		glmode = 1;
	}

	i = symtab.lb->Search( word );
	if ( i != -1 ) {
		SetErrorSymbolOverdefined( word, i );
		return PPRESULT_ERROR;
	}
	if ( glmode != 0 ) {
		FixModuleName( word );
	} else {
		AddModuleName( word );
	}
	id = symtab.lb->Regist( word, LAB_TYPE_COMVAR, 0 );
	if ( glmode != 0 ) {
		symtab.lb->SetEternal( id );
	}
	//
	wrtbuf->PutStrf( "#usecom %s%s", word, (char *)lexer.wp );
	wrtbuf->PutCR();
	//
	return PPRESULT_WROTE_LINE;
}


ppresult_t CPreProcessor::PP_Module()
{
	//		#module解析
	//
	int res;
	int i;
	int id;
	int fl;
	char *word;
	char tagname[MODNAME_MAX + 1];
	// char tmp[0x4000];

	word = (char *)token->s3;
	fl = 0;
	i = lexer.GetToken();
	if ( ( i == TK_OBJ ) || ( i == TK_STRING ) ) {
		fl = 1;
	}
	if ( i == TK_NONE ) {
		sprintf( word, "M%d", modgc );
		modgc++;
		fl = 1;
	}
	if ( fl == 0 ) {
		logger->SetError( "invalid module name" );
		return PPRESULT_ERROR;
	}
	if ( IsGlobalMode() == 0 ) {
		logger->SetError( "not in global mode" );
		return PPRESULT_ERROR;
	}
	if ( CheckModuleName( word ) != 0 ) {
		logger->SetError( "bad module name" );
		return PPRESULT_ERROR;
	}
	sprintf( tagname, "%.*s", MODNAME_MAX, word );
	res = symtab.lb->Search( tagname );
	if ( res != -1 ) {
		SetErrorSymbolOverdefined( tagname, res );
		return PPRESULT_ERROR;
	}
	id = symtab.lb->Regist( tagname, LAB_TYPE_PPDLLFUNC, 0 );
	symtab.lb->SetEternal( id );
	SetModuleName( tagname );

	wrtbuf->PutStrf( "#module %s", tagname );
	wrtbuf->PutCR();
	wrtbuf->PutStrf( "goto@hsp *_%s_exit", tagname );
	wrtbuf->PutCR();

	if ( lexer.PeekToken() != TK_NONE ) {
		wrtbuf->PutStrf( "#struct %s ", tagname );
		while ( true ) {

			i = lexer.GetToken();
			if ( i != TK_OBJ ) {
				logger->SetError( "invalid module param" );
				return PPRESULT_ERROR;
			}
			AddModuleName( word );
			res = symtab.lb->Search( word );
			if ( res != -1 ) {
				SetErrorSymbolOverdefined( word, res );
				return PPRESULT_ERROR;
			}
			id = symtab.lb->Regist( word, LAB_TYPE_PPDLLFUNC, 0 );
			wrtbuf->PutStr( "var " );
			wrtbuf->PutStr( word );

			i = lexer.GetToken();
			if ( lexer.wp == nullptr ) {
				break;
			}
			if ( i != ',' ) {
				logger->SetError( "invalid module param" );
				return PPRESULT_ERROR;
			}
			wrtbuf->Put( ',' );
		}
		wrtbuf->PutCR();
	}

	return PPRESULT_WROTE_LINES;
}


ppresult_t CPreProcessor::PP_Global()
{
	//		#global解析
	//
	if ( IsGlobalMode() != 0 ) {
#ifdef JPNMSG
		logger->SetError( "#module と対応していない #global があります" );
#else
		logger->SetError( "already in global mode" );
#endif
		return PPRESULT_ERROR;
	}
	//
	wrtbuf->PutStrf( "*_%s_exit", GetModuleName() );
	wrtbuf->PutCR();
	wrtbuf->PutStr( "#global" );
	wrtbuf->PutCR();
	SetModuleName( "" );
	return PPRESULT_WROTE_LINES;
}


ppresult_t CPreProcessor::PP_Aht()
{
	//		#aht解析
	//
	int i;
	char tmp[512];
	if ( ahtmodel == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	if ( ahtbuf != nullptr ) {
		return PPRESULT_SUCCESS; // AHT出力時は無視する
	}

	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "invalid AHT option name" );
		return PPRESULT_ERROR;
	}
	strcpy2( tmp, (char *)token->s3, 512 );
	i = lexer.GetToken();
	if ( ( i != TK_STRING ) && ( i != TK_NUM ) ) {
		logger->SetError( "invalid AHT option value" );
		return PPRESULT_ERROR;
	}
	ahtmodel->SetAHTOption( tmp, (char *)token->s3 );

	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_Ahtout()
{
	//		#ahtout解析
	//
	if ( ahtmodel == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	if ( ahtbuf == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	if ( lexer.wp == nullptr ) {
		return PPRESULT_SUCCESS;
	}

	ahtbuf->PutStr( (char *)lexer.wp );
	ahtbuf->PutCR();
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_Ahtmes()
{
	//		#ahtmes解析
	//
	int i;
	int addprm;

	if ( ahtmodel == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	if ( ahtbuf == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	if ( lexer.wp == nullptr ) {
		return PPRESULT_SUCCESS;
	}
	addprm = 0;

	while ( true ) {

		if ( lexer.wp == nullptr ) {
			break;
		}

		i = lexer.GetToken();
		if ( i == TK_NONE ) {
			break;
		}
		if ( ( i != TK_OBJ ) && ( i != TK_NUM ) && ( i != TK_STRING ) ) {
			logger->SetError( "illegal ahtmes parameter" );
			return PPRESULT_ERROR;
		}
		ahtbuf->PutStr( (char *)token->s3 );

		if ( lexer.wp == nullptr ) {
			addprm = 0;
			break;
		}

		i = lexer.GetToken();
		if ( i != '+' ) {
			logger->SetError( "invalid ahtmes format" );
			return PPRESULT_ERROR;
		}
		addprm++;
	}
	if ( addprm == 0 ) {
		ahtbuf->PutCR();
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_Pack( int mode )
{
	//		#pack,#epack解析
	//			(mode:0=normal/1=encrypt)
	int i;
	if ( packbuf != nullptr ) {
		i = lexer.GetToken();
		if ( i != TK_STRING ) {
			logger->SetError( "invalid pack name" );
			return PPRESULT_ERROR;
		}
		if ( ( mode & 2 ) != 0 ) {
			AddPackfile( (char *)token->s3, mode & 1 );
		} else {
			AddPackfileOrig( (char *)token->s3, mode );
		}
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_PackOpt()
{
	//		#packopt解析
	//
	int i;
	char tmp[1024];
	char optname[1024];
	if ( packbuf != nullptr ) {
		i = lexer.GetToken();
		if ( i != TK_OBJ ) {
			logger->SetError( "illegal option name" );
			return PPRESULT_ERROR;
		}
		strncpy( optname, (char *)token->s3, 128 );
		i = lexer.GetToken();
		if ( ( i != TK_OBJ ) && ( i != TK_NUM ) && ( i != TK_STRING ) ) {
			logger->SetError( "illegal option parameter" );
			return PPRESULT_ERROR;
		}
		sprintf( tmp, ";!%s=%s", optname, (char *)token->s3 );
		AddPackfile( tmp, 2 );
	}
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_CmpOpt()
{
	//		#cmpopt解析
	//
	int i;
	char optname[1024];

	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "illegal option name" );
		return PPRESULT_ERROR;
	}
	strcase2( (char *)token->s3, optname );

	i = lexer.GetToken();
	if ( i != TK_NUM ) {
		logger->SetError( "illegal option parameter" );
		return PPRESULT_ERROR;
	}

	i = 0;
	if ( tstrcmp( optname, "ppout" ) != 0 ) { // preprocessor out sw
		i = CMPMODE_PPOUT;
	}
	if ( tstrcmp( optname, "optcode" ) != 0 ) { // code optimization sw
		i = CMPMODE_OPTCODE;
	}
	if ( tstrcmp( optname, "case" ) != 0 ) { // case sensitive sw
		i = CMPMODE_CASE;
	}
	if ( tstrcmp( optname, "optinfo" ) != 0 ) { // optimization info sw
		i = CMPMODE_OPTINFO;
	}
	if ( tstrcmp( optname, "varname" ) != 0 ) { // VAR name out sw
		i = CMPMODE_PUTVARS;
	}
	if ( tstrcmp( optname, "varinit" ) != 0 ) { // VAR initalize check
		i = CMPMODE_VARINIT;
	}
	if ( tstrcmp( optname, "optprm" ) != 0 ) { // parameter optimization sw
		i = CMPMODE_OPTPRM;
	}
	if ( tstrcmp( optname, "skipjpspc" ) != 0 ) { // skip Japanese Space Code sw
		i = CMPMODE_SKIPJPSPC;
	}
	if ( tstrcmp( optname, "utf8" ) != 0 ) { // UTF-8 string output sw
		i = CMPMODE_UTF8OUT;
	}

	if ( i == 0 ) {
		logger->SetError( "illegal option name" );
		return PPRESULT_ERROR;
	}

	if ( token->val != 0 ) {
		compopt->hed_cmpmode |= i;
	} else {
		compopt->hed_cmpmode &= ~i;
	}
	// Alertf("%s(%d)",optname,val);
	// wrtbuf->PutCR();
	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_RuntimeOpt()
{
	//		#runtime解析
	//
	int i;
	char tmp[1024];

	i = lexer.GetToken();
	if ( i != TK_STRING ) {
		logger->SetError( "illegal runtime name" );
		return PPRESULT_ERROR;
	}
	strncpy( compopt->hed_runtime, (char *)token->s3, sizeof( compopt->hed_runtime ) );
	compopt->hed_runtime[sizeof( compopt->hed_runtime ) - 1] = '\0';

	if ( packbuf != nullptr ) {
		sprintf( tmp, ";!runtime=%s.hrt", compopt->hed_runtime );
		AddPackfile( tmp, 2 );
	}

	compopt->hed_option |= HEDINFO_RUNTIME;

	sprintf( tmp, "\"%s\"", compopt->hed_runtime );
	RegistExtMacro( "__runtime__", tmp ); // ランタイム名マクロを更新

	return PPRESULT_SUCCESS;
}


ppresult_t CPreProcessor::PP_BootOpt()
{
	//		#bootopt解析
	//
	int i;
	char optname[1024];

	i = lexer.GetToken();
	if ( i != TK_OBJ ) {
		logger->SetError( "illegal option name" );
		return PPRESULT_ERROR;
	}
	strcase2( (char *)token->s3, optname );

	i = lexer.GetToken();
	if ( i != TK_NUM ) {
		logger->SetError( "illegal option parameter" );
		return PPRESULT_ERROR;
	}

	i = 0;
	if ( tstrcmp( optname, "notimer" ) != 0 ) { // No MMTimer sw
		i = HEDINFO_NOMMTIMER;
		compopt->hed_autoopt_timer = -1;
	}
	if ( tstrcmp( optname, "nogdip" ) != 0 ) { // No GDI+ sw
		i = HEDINFO_NOGDIP;
	}
	if ( tstrcmp( optname, "float32" ) != 0 ) { // float32 sw
		i = HEDINFO_FLOAT32;
	}
	if ( tstrcmp( optname, "orgrnd" ) != 0 ) { // standard random sw
		i = HEDINFO_ORGRND;
	}
	if ( tstrcmp( optname, "utf8" ) != 0 ) { // UTF-8 string sw
		i = HEDINFO_UTF8;
	}
	if ( tstrcmp( optname, "hsp64" ) != 0 ) { // 64bit runtime sw
		i = HEDINFO_HSP64;
	}
	if ( tstrcmp( optname, "ioresume" ) != 0 ) { // File I/O resume sw
		i = HEDINFO_IORESUME;
	}

	if ( i == 0 ) {
		logger->SetError( "illegal option name" );
		return PPRESULT_ERROR;
	}

	if ( token->val != 0 ) {
		compopt->hed_option |= i;
	} else {
		compopt->hed_option &= ~i;
	}
	return PPRESULT_SUCCESS;
}


void CPreProcessor::PreprocessCommentCheck( char *str )
{
	int qmode;
	unsigned char *vs;
	unsigned char a1;
	vs = (unsigned char *)str;
	qmode = 0;

	while ( true ) {
		a1 = *vs++;
		if ( a1 == 0 ) {
			break;
		}
		if ( qmode == 0 ) {
			if ( ( a1 == ';' ) && ( *vs == ';' ) ) {
				vs++;
				ahtkeyword = (char *)vs;
			}
		}
		if ( a1 == 0x22 ) {
			qmode ^= 1;
		}
		vs += SkipMultiByte( a1 ); // 全角文字チェック
	}
}


ppresult_t CPreProcessor::PreprocessNM( char *str )
{
	//		プリプロセスの実行(マクロ展開なし)
	//
	char *word;
	int id;
	int type;
	ppresult_t res;
	char fixname[128];

	word = (char *)token->s3;
	lexer.wp = (unsigned char *)str;
	unsigned char topUChar = *lexer.wp;
	*token->s3 = 0;

	if ( ahtmodel != nullptr ) {
		PreprocessCommentCheck( str );
	}

	type = lexer.GetToken();
	if ( topUChar != *token->s3 ) {
		return PPRESULT_UNKNOWN_DIRECTIVE;
	}
	if ( type != TK_OBJ ) {
		return PPRESULT_UNKNOWN_DIRECTIVE;
	}

	//		ソース生成コントロール
	//
	if ( tstrcmp( word, "ifdef" ) != 0 ) { // generate control
		if ( mulstr == LMODE_OFF ) {
			res = PP_SwitchStart( 0 );
		} else {
			res = PPRESULT_ERROR;
			type = lexer.GetToken();
			if ( type == TK_OBJ ) {
				strcase2( word, fixname );
				AddModuleName( fixname );
				id = symtab.lb->SearchLocal( word, fixname );

				// id = symtab.lb->Search( word );
				res = PP_SwitchStart( static_cast<int>( id != -1 ) );
			}
		}
		return res;
	}
	if ( tstrcmp( word, "ifndef" ) != 0 ) { // generate control
		if ( mulstr == LMODE_OFF ) {
			res = PP_SwitchStart( 0 );
		} else {
			res = PPRESULT_ERROR;
			type = lexer.GetToken();
			if ( type == TK_OBJ ) {
				strcase2( word, fixname );
				AddModuleName( fixname );
				id = symtab.lb->SearchLocal( word, fixname );

				// id = symtab.lb->Search( word );
				res = PP_SwitchStart( static_cast<int>( id == -1 ) );
			}
		}
		return res;
	}
	if ( tstrcmp( word, "else" ) != 0 ) { // generate control
		return PP_SwitchReverse();
	}
	if ( tstrcmp( word, "endif" ) != 0 ) { // generate control
		return PP_SwitchEnd();
	}

	//		これ以降は#off時に実行しません
	//
	if ( mulstr == LMODE_OFF ) {
		return PPRESULT_UNKNOWN_DIRECTIVE;
	}

	if ( tstrcmp( word, "define" ) != 0 ) { // keyword define
		return PP_Define();
	}

	if ( tstrcmp( word, "undef" ) != 0 ) { // keyword terminate
		if ( lexer.GetToken() != TK_OBJ ) {
			logger->SetError( "invalid symbol" );
			return PPRESULT_ERROR;
		}

		strcase2( word, fixname );
		AddModuleName( fixname );
		id = symtab.lb->SearchLocal( word, fixname );

		// id = symtab.lb->Search( word );
		if ( id >= 0 ) {
			symtab.lb->SetFlag( id, -1 );
		}
		return PPRESULT_SUCCESS;
	}

	return PPRESULT_UNKNOWN_DIRECTIVE;
}


ppresult_t CPreProcessor::Preprocess( char *str )
{
	//		プリプロセスの実行
	//
	char *word;
	int type;
	int a;
	ppresult_t res;
	CALCVAR cres;

	word = (char *)token->s3;
	*token->s3 = 0;
	lexer.wp = (unsigned char *)str;
	unsigned char topUChar = *lexer.wp;

	type = lexer.GetToken();
	if ( topUChar != *token->s3 ) {
		type = TK_NONE;
	}
	//		ソース生成コントロール
	//
	if ( type == TK_OBJ ) {
		if ( tstrcmp( word, "if" ) != 0 ) { // generate control
			if ( mulstr == LMODE_OFF ) {
				res = PP_SwitchStart( 0 );
			} else {
				res = PPRESULT_SUCCESS;
				if ( Calc( cres ) == 0 ) {
					a = (int)cres;
					res = PP_SwitchStart( a );
				} else {
					res = PPRESULT_ERROR;
				}
			}
			return res;
		}
	}
	//		これ以降は#off時に実行しません
	//
	if ( mulstr == LMODE_OFF ) {
		return PPRESULT_SUCCESS;
	}

	//		コード生成コントロール
	//
	if ( type == TK_OBJ ) {
		if ( tstrcmp( word, "include" ) != 0 ) { // text include
			res = PP_Include( 0 );
			return res;
		}
		if ( tstrcmp( word, "addition" ) != 0 ) { // text include
			res = PP_Include( 1 );
			return res;
		}
		if ( tstrcmp( word, "const" ) != 0 ) { // constant define
			res = PP_Const();
			return res;
		}
		if ( tstrcmp( word, "enum" ) != 0 ) { // constant enum define
			res = PP_Enum();
			return res;
		}
		if ( tstrcmp( word, "use" ) != 0 ) { // multiple include
			res = PP_use();
			return res;
		}
		if ( tstrcmp( word, "module" ) != 0 ) { // module define
			res = PP_Module();
			return res;
		}
		if ( tstrcmp( word, "global" ) != 0 ) { // module exit
			res = PP_Global();
			return res;
		}
		if ( tstrcmp( word, "deffunc" ) != 0 ) { // module function
			res = PP_Deffunc( 0 );
			return res;
		}
		if ( tstrcmp( word, "defcfunc" ) != 0 ) { // module function (1)
			res = PP_Defcfunc( 0 );
			return res;
		}
		if ( tstrcmp( word, "modfunc" ) != 0 ) { // module function (2)
			res = PP_Deffunc( 1 );
			return res;
		}
		if ( tstrcmp( word, "modcfunc" ) != 0 ) { // module function (2+)
			res = PP_Defcfunc( 1 );
			return res;
		}
		if ( tstrcmp( word, "modinit" ) != 0 ) { // module function (3)
			res = PP_Deffunc( 2 );
			return res;
		}
		if ( tstrcmp( word, "modterm" ) != 0 ) { // module function (4)
			res = PP_Deffunc( 3 );
			return res;
		}
		if ( tstrcmp( word, "struct" ) != 0 ) { // struct define
			res = PP_Struct();
			return res;
		}
		if ( tstrcmp( word, "func" ) != 0 ) { // DLL function
			res = PP_Func( "func" );
			return res;
		}
		if ( tstrcmp( word, "cfunc" ) != 0 ) { // DLL function
			res = PP_Func( "cfunc" );
			return res;
		}
		if ( tstrcmp( word, "cmd" ) != 0 ) { // DLL function (3.0)
			res = PP_Cmd( "cmd" );
			return res;
		}
		if ( tstrcmp( word, "comfunc" ) != 0 ) { // COM Object function
			res = PP_Func( "comfunc" );
			return res;
		}
		if ( tstrcmp( word, "aht" ) != 0 ) { // AHT definition
			res = PP_Aht();
			return res;
		}
		if ( tstrcmp( word, "ahtout" ) != 0 ) { // AHT command line output
			res = PP_Ahtout();
			return res;
		}
		if ( tstrcmp( word, "ahtmes" ) != 0 ) { // AHT command line output (mes)
			res = PP_Ahtmes();
			return res;
		}
		if ( tstrcmp( word, "pack" ) != 0 ) { // packfile process
			res = PP_Pack( 0 );
			return res;
		}
		if ( tstrcmp( word, "epack" ) != 0 ) { // packfile process
			res = PP_Pack( 1 );
			return res;
		}
		if ( tstrcmp( word, "packdir" ) != 0 ) { // packfile process
			res = PP_Pack( 2 );
			return res;
		}
		if ( tstrcmp( word, "epackdir" ) != 0 ) { // packfile process
			res = PP_Pack( 3 );
			return res;
		}
		if ( tstrcmp( word, "packopt" ) != 0 ) { // packfile process
			res = PP_PackOpt();
			return res;
		}
		if ( tstrcmp( word, "runtime" ) != 0 ) { // runtime process
			res = PP_RuntimeOpt();
			return res;
		}
		if ( tstrcmp( word, "bootopt" ) != 0 ) { // boot option process
			res = PP_BootOpt();
			return res;
		}
		if ( tstrcmp( word, "cmpopt" ) != 0 ) { // compile option process
			res = PP_CmpOpt();
			return res;
		}
		if ( tstrcmp( word, "usecom" ) != 0 ) { // COM definition
			res = PP_Usecom();
			return res;
		}
		if ( tstrcmp( word, "var" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
		if ( tstrcmp( word, "varint" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
		if ( tstrcmp( word, "varlabel" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
		if ( tstrcmp( word, "varstr" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
		if ( tstrcmp( word, "vardouble" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
		if ( tstrcmp( word, "varmod" ) != 0 ) { // VAR definition
			res = PP_VarFix( word );
			return res;
		}
	}
	//		登録キーワード以外はコンパイラに渡す
	//
	wrtbuf->Put( (char)'#' );
	wrtbuf->PutStr( linebuf );
	wrtbuf->PutCR();
	// wrtbuf->PutStr( (char *)token->s3 );
	return PPRESULT_WROTE_LINE;
}


int CPreProcessor::ExpandTokens( char *vp, CMemBuf *buf, int *lineext, int is_preprocess_line )
{
	//		マクロを展開
	//
	*lineext = 0;	 // 1行->複数行にマクロ展開されたか?
	int macloop = 0; // マクロ展開無限ループチェック用カウンタ
	while ( true ) {
		if ( mulstr == LMODE_OFF ) { // １行無視
			if ( wrtbuf != nullptr ) {
				wrtbuf->PutCR(); // 行末CR/LFを追加
			}
			break;
		}

		// {"～"}の処理
		//
		if ( mulstr == LMODE_STR ) {
			wrtbuf = buf;
			vp = ExpandStrEx( vp );
			if ( *vp != 0 ) {
				continue;
			}
		}

		// /*～*/の処理
		//
		if ( mulstr == LMODE_COMMENT ) {
			vp = ExpandStrComment( vp, 0 );
			if ( *vp != 0 ) {
				continue;
			}
		}

		char *vp_bak = vp;
		int type;
		vp = ExpandToken( vp, &type, is_preprocess_line );
		if ( type < 0 ) {
			return type;
		}
		if ( type == TK_EOL ) {
			( *lineext )++;
		}
		if ( type == TK_EOF ) {
			if ( wrtbuf != nullptr ) {
				wrtbuf->PutCR(); // 行末CR/LFを追加
			}
			break;
		}
		if ( vp_bak == vp ) {
			macloop++;
			if ( macloop > 999 ) {
				logger->SetError( "Endless macro loop" );
				return -1;
			}
		}
	}
	return 0;
}


int CPreProcessor::ExpandLine( CMemBuf *buf, CMemBuf *src, char *refname )
{
	//		stringデータをmembufへ展開する
	//
	char *p = src->GetBuffer();
	int pline = 1;
	enumgc = 0;
	mulstr = LMODE_ON;
	logger->ClearError();
	unsigned char a1;

	a1 = *(unsigned char *)p;
	if ( a1 == 0xef ) { // BOMをチェック
		a1 = (unsigned char)p[1];
		if ( a1 == 0xbb ) {
			a1 = (unsigned char)p[2];
			if ( a1 == 0xbf ) {
				if ( compopt->pp_utf8 == 0 ) {
#ifdef JPNMSG
					logger->Mesf( "#ファイルに予期しない BOM があります [%s]", refname );
#else
					logger->Mesf( "#Unexpected BOM in file.[%s]", refname );
#endif
				}
				p += 3;
			}
		}
	}

	while ( true ) {
		RegistExtMacro( "__line__", pline ); // 行番号マクロを更新
		pp_orgline = pline;

		if ( cg_labout_line == pline ) { // 解析用のラインか?
			if ( strcmp( cg_labout_orgfile, refname ) == 0 ) {
				//	解析ラインの状態を保存する
				// Alertf( "#Module %s in file[%s] line %d.", modname, refname, pline );
				strcpy( cg_labout_modname, modname );
				if ( ( compopt->hed_cmpmode & CMPMODE_CASE ) != 0 ) { // 将来のため
					cg_labout_caseflag = 1;
				}
			}
		}

		while ( true ) {
			a1 = *(unsigned char *)p;
			if ( a1 == ' ' || a1 == '\t' ) {
				p++;
				continue;
			}
#ifdef HSPWIN
			if ( compopt->hed_cmpmode & CMPMODE_SKIPJPSPC ) {
				if ( a1 == 0x81 && p[1] == 0x40 ) { // 全角スペースチェック
					p += 2;
					continue;
				}
			}
#endif
			break;
		}

		if ( *p == 0 ) {
			break; // 終了(EOF)
		}
		ahtkeyword = nullptr; // AHTキーワードをリセットする

		int is_preprocess_line = static_cast<int>( *p == '#' && mulstr != LMODE_STR && mulstr != LMODE_COMMENT );

		//		行データをlinebufに展開
		int mline;
		if ( is_preprocess_line != 0 ) {
			p = SendLineBufPP( p + 1, &mline ); // 行末までを取り出す('\'継続)
			wrtbuf = nullptr;
		} else {
			p = SendLineBuf( p ); // 行末までを取り出す
			mline = 0;
			wrtbuf = buf;
		}

		//		logger->Mesf("%d:%s", pline, src->GetFileName() );
		//		sprintf( mestmp,"%d:%s:%s(%d)", pline, src->GetFileName(), linebuf, is_preprocess_line );
		//		Alert( mestmp );
		//		buf->PutStr( mestmp );

		//		マクロ展開前に処理されるプリプロセッサ
		if ( is_preprocess_line != 0 ) {
			ppresult_t res = PreprocessNM( linebuf );
			if ( res == PPRESULT_ERROR ) {
				logger->LineError( pline, refname );
				return 1;
			}
			if ( res == PPRESULT_SUCCESS ) { // プリプロセッサで処理された時
				mline++;
				pline += mline;
				for ( int i = 0; i < mline; i++ ) {
					buf->PutCR();
				}
				continue;
			}
			assert( res == PPRESULT_UNKNOWN_DIRECTIVE );
		}

		//		if ( wrtbuf!=NULL ) {
		//			char ss[64];
		//			sprintf( ss,"__%d:",pline );
		//			wrtbuf->PutStr( ss );
		//		}

		//		マクロを展開
		int lineext; // 1行->複数行にマクロ展開されたか?
		int res = ExpandTokens( linebuf, buf, &lineext, is_preprocess_line );
		if ( res != 0 ) {
			logger->LineError( pline, refname );
			return res;
		}

		//		プリプロセッサ処理
		if ( is_preprocess_line != 0 ) {
			wrtbuf = buf;
			ppresult_t res = Preprocess( linebuf );
			if ( res == PPRESULT_INCLUDED ) { // include後の処理
				pline += 1 + mline;

				char *fname_literal = to_hsp_string_literal( refname, true );
				RegistExtMacro( "__file__", fname_literal ); // ファイル名マクロを更新
				strcpy( pp_orgfile, refname );

				wrtbuf = buf;
				wrtbuf->PutStrf( "##%d %s\r\n", pline - 1, fname_literal );
				free( fname_literal );
				continue;
			}
			if ( res == PPRESULT_WROTE_LINES ) { // プリプロセスで行が増えた後の処理
				pline += mline;
				wrtbuf->PutStrf( "##%d\r\n", pline );
				pline++;
				continue;
			}
			if ( res == PPRESULT_ERROR ) {
				logger->LineError( pline, refname );
				return 1;
			}
			pline += 1 + mline;
			if ( res != PPRESULT_WROTE_LINE ) {
				mline++;
			}
			for ( int i = 0; i < mline; i++ ) {
				buf->PutCR();
			}
			assert( res == PPRESULT_SUCCESS || res == PPRESULT_WROTE_LINE );
			continue;
		}

		//		マクロ展開後に行数が変わった場合の処理
		pline += 1 + mline;
		if ( lineext != mline ) {
			wrtbuf->PutStrf( "##%d\r\n", pline );
		}
	}
	return 0;
}


int CPreProcessor::ExpandFile( CMemBuf *buf, char *fname, char *refname )
{
	//		ソースファイルをmembufへ展開する
	//
	int res;
	char cname[HSP_MAX_PATH];
	char purename[HSP_MAX_PATH];
	char foldername[HSP_MAX_PATH];
	char refname_copy[HSP_MAX_PATH];
	char vaild_file[HSP_MAX_PATH];

	char org_filename[HSP_MAX_PATH];
	char org_filenamefull[HSP_MAX_PATH];
	char org_fileline;

	CMemBuf fbuf;

	//	元の情報を保存する
	strcpy( org_filename, pp_orgfile );
	strcpy( org_filenamefull, pp_orgfilefull );
	org_fileline = pp_orgline;

	getpath( fname, purename, 8 );
	getpath( fname, foldername, 32 );
	if ( *foldername != 0 ) {
		strcpy( search_path, foldername );
	}

	strcpy( vaild_file, refname );
	if ( fbuf.PutFile( fname ) < 0 ) {
		strcpy( cname, compopt->common_path );
		strcat( cname, purename );
		strcpy( vaild_file, compopt->common_path );
		strcat( vaild_file, refname );
		if ( fbuf.PutFile( cname ) < 0 ) {
			strcpy( cname, search_path );
			strcat( cname, purename );
			strcpy( vaild_file, search_path );
			strcat( vaild_file, refname );
			if ( fbuf.PutFile( cname ) < 0 ) {
				strcpy( cname, compopt->common_path );
				strcat( cname, search_path );
				strcat( cname, purename );
				strcpy( vaild_file, compopt->common_path );
				strcat( vaild_file, search_path );
				strcat( vaild_file, refname );
				if ( fbuf.PutFile( cname ) < 0 ) {
					if ( fileadd == 0 ) {
#ifdef JPNMSG
						logger->Mesf( "#スクリプトファイルが見つかりません [%s]", purename );
#else
						logger->Mesf( "#Source file not found.[%s]", purename );
#endif
					}
					return -1;
				}
			}
		}
	}
	fbuf.Put( (char)0 );
	strcpy( pp_orgfilefull, vaild_file );

	if ( fileadd != 0 ) {
		logger->Mesf( "#Use file [%s]", purename );
	}

	char *fname_literal = to_hsp_string_literal( refname, true );
	RegistExtMacro( "__file__", fname_literal ); // ファイル名マクロを更新
	strcpy( pp_orgfile, refname );

	fname_literal = to_hsp_string_literal( pp_orgfilefull, true );
	buf->PutStrf( "##0 %s\r\n", fname_literal );
	free( fname_literal );

	strcpy2( refname_copy, refname, sizeof refname_copy );
	res = ExpandLine( buf, &fbuf, refname_copy );

	//	元の情報に復帰させる
	strcpy( pp_orgfile, org_filename );
	strcpy( pp_orgfilefull, org_filenamefull );
	pp_orgline = org_fileline;

	if ( res == 0 ) {
		//		プリプロセス後チェック
		//
		res = tstack->StackCheck( linebuf );
		if ( res != 0 ) {
#ifdef JPNMSG
			logger->Mesf( "#スタックが空になっていないマクロタグが%d個あります [%s]", res, refname_copy );
#else
			logger->Mesf( "#%d unresolved macro(s).[%s]", res, refname_copy );
#endif
			logger->Mes( linebuf );
		}
	}

	if ( res != 0 ) {

#ifdef JPNMSG
		logger->Mes( "#重大なエラーが検出されています" );
#else
		logger->Mes( "#Fatal error reported." );
#endif
		return -2;
	}
	return 0;
}


int CPreProcessor::SetAdditionMode( int mode )
{
	//		Additionによるファイル追加モード設定(1=on/0=off)
	//
	int i;
	i = fileadd;
	fileadd = mode;
	return i;
}


void CPreProcessor::FinishPreprocess( CMemBuf *buf )
{
	//	後ろで定義された関数がある場合、それに書き換える
	//
	//	この関数では foo@modname を foo に書き換えるなどバッファサイズが小さくなる変更しか行わない
	//
	int read_pos = 0;
	int write_pos = 0;
	size_t len = undefined_symbols.size();
	char *p = buf->GetBuffer();
	for ( size_t i = 0; i < len; i++ ) {
		undefined_symbol_t sym = undefined_symbols[i];
		int pos = sym.pos;
		int len_include_modname = sym.len_include_modname;
		int len = sym.len;
		int id;
		memmove( p + write_pos, p + read_pos, pos - read_pos );
		write_pos += pos - read_pos;
		read_pos = pos;
		// @modname を消した名前の関数が存在したらそれに書き換え
		p[pos + len] = '\0';
		id = symtab.lb->Search( p + pos );
		if ( id >= 0 && symtab.lb->GetType( id ) == LAB_TYPE_PPMODFUNC ) {
			memmove( p + write_pos, p + pos, len );
			write_pos += len;
			read_pos += len_include_modname;
		}
		p[pos + len] = '@';
	}
	memmove( p + write_pos, p + read_pos, buf->GetSize() - read_pos );
	buf->ReduceSize( buf->GetSize() - ( read_pos - write_pos ) );
}


int CPreProcessor::RegistExtMacroPath( char *keyword, char *str )
{
	//		マクロを外部から登録(path用)
	//
	int id;
	int res;
	char path[1024];
	char mm[512];
	unsigned char *p;
	unsigned char *src;
	unsigned char a1;

	p = (unsigned char *)path;
	src = (unsigned char *)str;
	while ( true ) {
		a1 = *src++;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == 0x5c ) {
			*p++ = a1;
		}				   // '\'チェック
		if ( a1 >= 129 ) { // 全角文字チェック
			if ( a1 <= 159 ) {
				*p++ = a1;
				a1 = *src++;
			} else if ( a1 >= 224 ) {
				*p++ = a1;
				a1 = *src++;
			}
		}
		*p++ = a1;
	}
	*p = 0;

	strcpy( mm, keyword );
	FixModuleName( mm );
	res = symtab.lb->Search( mm );
	if ( res != -1 ) { // すでにある場合は上書き
		symtab.lb->SetData( res, path );
		return -1;
	}
	//		データ定義
	id = symtab.lb->Regist( mm, LAB_TYPE_PPMAC, 0 );
	symtab.lb->SetData( id, path );
	symtab.lb->SetEternal( id );
	return 0;
}


int CPreProcessor::RegistExtMacro( char *keyword, char *str )
{
	//		マクロを外部から登録
	//
	int id;
	int res;
	char mm[512];
	strcpy( mm, keyword );
	FixModuleName( mm );
	res = symtab.lb->Search( mm );
	if ( res != -1 ) { // すでにある場合は上書き
		symtab.lb->SetData( res, str );
		return -1;
	}
	//		データ定義
	id = symtab.lb->Regist( mm, LAB_TYPE_PPMAC, 0 );
	symtab.lb->SetData( id, str );
	symtab.lb->SetEternal( id );
	return 0;
}


int CPreProcessor::RegistExtMacro( char *keyword, int val )
{
	//		マクロを外部から登録(数値)
	//
	int id;
	int res;
	char mm[512];
	strcpy( mm, keyword );
	FixModuleName( mm );
	res = symtab.lb->Search( mm );
	if ( res != -1 ) { // すでにある場合は上書き
		symtab.lb->SetOpt( res, val );
		return -1;
	}
	//		データ定義
	id = symtab.lb->Regist( mm, LAB_TYPE_PPVAL, val );
	symtab.lb->SetEternal( id );
	return 0;
}


int CPreProcessor::LabelDump( CMemBuf &out, int option, char *match )
{
	//		登録されているラベル情報をerrbufに展開
	//
	int a;
	int max;
	max = symtab.lb->GetCount();
	for ( a = 0; a < max; a++ ) {
		symtab.lb->DumpHSPLabelById( a, linebuf, option );
		if ( match != nullptr ) {
			//	matchが含まれていない場合は無視する
			if ( strstr2( linebuf, match ) == nullptr ) {
				*linebuf = 0;
			}
		}
		if ( *linebuf != 0 ) {
			out.PutStr( linebuf );
		}
	}
	return 0;
}


void CPreProcessor::SetModuleName( char *name )
{
	//		モジュール名を設定
	//
	if ( *name == 0 ) {
		modname[0] = 0;
		return;
	}
	sprintf( modname, "@%.*s", MODNAME_MAX, name );
	strcase( modname + 1 );
}


char *CPreProcessor::GetModuleName()
{
	//		モジュール名を取得
	//
	if ( *modname == 0 ) {
		return modname;
	}
	return modname + 1;
}


void CPreProcessor::AddModuleName( char *str )
{
	//		キーワードにモジュール名を付加(モジュール依存ラベル用)
	//
	unsigned char a1;
	unsigned char *wp;
	wp = (unsigned char *)str;
	while ( true ) {
		a1 = *wp;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == '@' ) {
			a1 = wp[1];
			if ( a1 == 0 ) {
				*wp = 0;
			}
			return;
		}
		if ( a1 >= 129 ) {
			wp++;
		}
		wp++;
	}
	if ( *modname == 0 ) {
		return;
	}
	strcpy( (char *)wp, modname );
}


void CPreProcessor::FixModuleName( char *str )
{
	//		キーワードのモジュール名を正規化(モジュール非依存ラベル用)
	//
	//	char *lexer.wp;
	//	lexer.wp = str + ( strlen(str)-1 );
	//	if ( *wp=='@' ) *wp=0;

	unsigned char a1;
	unsigned char *wp;
	wp = (unsigned char *)str;
	while ( true ) {
		a1 = *wp;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 == '@' ) {
			a1 = wp[1];
			if ( a1 == 0 ) {
				*wp = 0;
			}
			return;
		}
		if ( a1 >= 129 ) {
			wp++;
		}
		wp++;
	}
}


int CPreProcessor::IsGlobalMode()
{
	//		モジュール内(0)か、グローバル(1)かを返す
	//
	if ( *modname == 0 ) {
		return 1;
	}
	return 0;
}


int CPreProcessor::GetLabelBufferSize()
{
	//		ラベルバッファサイズを得る
	//
	return symtab.lb->GetSymbolSize();
}


void CPreProcessor::SetErrorSymbolOverdefined( char *keyword, int label_id )
{
	// 識別子の多重定義エラー

	char strtmp[0x100];
#ifdef JPNMSG
	sprintf( strtmp, "定義済みの識別子は使用できません [%s]", keyword );
#else
	sprintf( strtmp, "symbol in use [%s]", keyword );
#endif
	logger->SetError( strtmp );
}


void CPreProcessor::GenerateLabelListAndTagPP( char *name, int flag )
{
	if ( labbuf == nullptr ) {
		return;
	}
	GenerateLabelTag( name, flag, 0, pp_orgfilefull, pp_orgline );
}


void CPreProcessor::GenerateLabelListAndTagRefPP( char *name, int flag )
{
	if ( labbuf == nullptr ) {
		return;
	}
	if ( ( cg_labout_mode & LABLIST_MODE_REFERENCE ) == 0 ) {
		return;
	}
	GenerateLabelTag( name, flag | LABBUF_FLAG_REFER, 0, pp_orgfilefull, pp_orgline );
}


char *CPreProcessor::GetLabelListLineModule()
{
	if ( labbuf == nullptr ) {
		return "";
	}
	return cg_labout_modname;
}


int CPreProcessor::GetLabelListLineCaseFlag()
{
	if ( labbuf == nullptr ) {
		return 0;
	}
	return cg_labout_caseflag;
}


void CPreProcessor::GenerateLabelTag( char *name, int flag, int type, char *fname, int line )
{
	//	クロスリファレンス用のメッセージを出力する
	if ( labbuf == nullptr ) {
		return;
	}

	char textbf[4096];
	if ( cg_labout_match != nullptr ) {
		if ( ( cg_labout_mode & LABLIST_MODE_PARTMATCH ) != 0 ) {
			if ( strstr2( name, cg_labout_match ) == nullptr ) {
				return;
			}
		} else {
			if ( strcmp( name, cg_labout_match ) != 0 ) {
				return;
			}
		}
	}
	int pflag = flag & ( LABBUF_FLAG_REFER - 1 );

	switch ( cg_labout_mode & ( LABLIST_MODE_REFERENCE - 1 ) ) {
	case LABLIST_MODE_LABEL:
		if ( ( pflag != LABBUF_FLAG_LABEL ) && ( pflag != LABBUF_FLAG_FUNC ) ) {
			return;
		}
		break;
	case LABLIST_MODE_VAR:
		if ( ( pflag != LABBUF_FLAG_VAR ) && ( pflag != LABBUF_FLAG_EXVAR ) ) {
			return;
		}
		break;
	case LABLIST_MODE_ALL:
	default:
		break;
	}

	sprintf( textbf, "d%s %s %d:%s\r\n", GetLabelListHeader( flag ), name, line, fname );
	if ( ( flag & LABBUF_FLAG_REFER ) != 0 ) {
		textbf[0] = 'r';
	}

	labbuf->PutStr( textbf );
}
