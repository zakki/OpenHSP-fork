
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
#include "ahtobj.h"
#include "label.h"
#include "lexer_util.h"
#include "membuf.h"
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
//		String Service
//-------------------------------------------------------------

void strcase2( const char *str, char *str2 )
{
	//	string case to lower and copy
	//
	unsigned char a1;
	unsigned char *ss;
	unsigned char *ss2;
	ss = (unsigned char *)str;
	ss2 = (unsigned char *)str2;
	while ( true ) {
		a1 = *ss;
		if ( a1 == 0 ) {
			break;
		}
		if ( a1 >= 0x80 ) {
			ss++;
			*ss2++ = a1;
			a1 = *ss++;
			if ( a1 == 0 ) {
				break;
			}
			*ss2++ = a1;
		} else {
			a1 = tolower( a1 );
			*ss++ = a1;
			*ss2++ = a1;
		}
	}
	*ss2 = 0;
}

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------


CCompilerUtil::CCompilerUtil( std::shared_ptr<CompileOptions> opt ) : compopt( std::move( opt ) )
{
	InitSCNV( SCNVBUF_DEFAULTSIZE );
}

void CCompilerUtil::InitSCNV( size_t size )
{
	//		文字コード変換の初期化
	//
	if ( scnvbuf.size() < size ) {
		scnvbuf.resize( size );
	}
}


char *CCompilerUtil::ExecSCNV( const char *srcbuf, int opt )
{
	//		文字コード変換
	//
	size_t size = strlen( srcbuf );
	InitSCNV( size * 4 + 1 );
	char *buf = scnvbuf.data();

	switch ( opt ) {
	case SCNV_OPT_NONE:
		strcpy( buf, srcbuf );
		break;
	case SCNV_OPT_SJISUTF8:
#ifdef HSPWIN
		ConvSJis2Utf8( srcbuf, buf, scnvsize );
#else
		strcpy( buf, srcbuf );
#endif
		break;
	case SCNV_OPT_UTF8SJIS:
#ifdef HSPWIN
		ConvUtf82SJis( srcbuf, buf, scnvsize );
#else
		strcpy( buf, srcbuf );
#endif
		break;
	default:
		*buf = 0;
		break;
	}

	return buf;
}


int CCompilerUtil::CheckByteSJIS( unsigned char c )
{
	//	SJISの全角1バイト目を判定する
	//  (戻り値は以降に続くbyte数)
	if ( ( ( c >= 0x81 ) && ( c <= 0x9f ) ) || ( ( c >= 0xe0 ) && ( c <= 0xfc ) ) ) {
		return 1;
	}
	return 0;
}


int CCompilerUtil::CheckByteUTF8( unsigned char c )
{
	//	UTF8の全角1バイト目を判定する
	//  (戻り値は以降に続くbyte数)

	if ( c <= 0x7f ) {
		return 0;
	}

	if ( ( c >= 0xc2 ) && ( c <= 0xdf ) ) {
		return 1;
	}
	if ( ( c >= 0xe0 ) && ( c <= 0xef ) ) {
		return 2;
	}
	if ( ( c >= 0xf0 ) && ( c <= 0xf7 ) ) {
		return 3;
	}
	if ( ( c >= 0xf8 ) && ( c <= 0xfb ) ) {
		return 4;
	}
	if ( ( c >= 0xfc ) && ( c <= 0xfd ) ) {
		return 5;
	}
	return 0;
}


int CCompilerUtil::SkipMultiByte( unsigned char byte )
{
	//	マルチバイトコードの2byte目以降をスキップする
	//  ( 1バイト目のcharを渡すと、2byte目以降スキップするbyte数を返す )
	//	( pp_utf8のフラグによってUTF-8とSJISを判断する )
	//
	if ( compopt->pp_utf8 != 0 ) {
		return CheckByteUTF8( byte );
	}
	return CheckByteSJIS( byte );
}


int CCompilerUtil::ConvSJis2Utf8( const char *pSource, const char *pDist, int buffersize )
{
	int size = 0;
	if ( pDist == nullptr ) {
		return -1;
	}

#ifdef HSPWIN
	// ShiftJISからUTF-16へ変換
	const int nSize = ::MultiByteToWideChar( CP_ACP, 0, (LPCSTR)pSource, -1, NULL, 0 );

	BYTE *buffUtf16 = new BYTE[nSize * 2 + 2];
	::MultiByteToWideChar( CP_ACP, 0, (LPCSTR)pSource, -1, (LPWSTR)buffUtf16, nSize );

	// UTF-16からUTF-8へ変換
	size = ::WideCharToMultiByte( CP_UTF8, 0, (LPCWSTR)buffUtf16, -1, NULL, 0, NULL, NULL );
	size *= 2;
	if ( size > buffersize )
		size = buffersize;
	ZeroMemory( pDist, size );
	::WideCharToMultiByte( CP_UTF8, 0, (LPCWSTR)buffUtf16, -1, (LPSTR)pDist, size, NULL, NULL );

	size = lstrlen( (char *)pDist ) + 1;

	delete[] buffUtf16;
#endif
	return size;
}


int CCompilerUtil::ConvUtf82SJis( const char *pSource, char *pDist, int buffersize )
{
	int size = 0;

#ifdef HSPWIN

	// サイズを計算する
	int iLenUnicode = ::MultiByteToWideChar( CP_UTF8, 0, pSource, strlen( pSource ) + 1, NULL, 0 );
	BYTE *buffUtf16 = new BYTE[iLenUnicode * 2 + 2];

	::MultiByteToWideChar( CP_UTF8, 0, pSource, strlen( pSource ) + 1, (LPWSTR)buffUtf16, iLenUnicode );

	size = ::WideCharToMultiByte( CP_ACP, 0, (LPCWSTR)buffUtf16, iLenUnicode, NULL, 0, NULL, NULL );
	if ( size > buffersize )
		size = buffersize;
	::WideCharToMultiByte( CP_ACP, 0, (LPCWSTR)buffUtf16, iLenUnicode, pDist, size, NULL, NULL );

	delete[] buffUtf16;
	pDist[size] = 0;
#endif
	return size;
}


char *CCompilerUtil::to_hsp_string_literal( const char *src, bool filename )
{
	//		文字列をHSPの文字列リテラル形式に
	//		戻り値のメモリは呼び出し側がfreeする必要がある。
	//		HSPの文字列リテラルで表せない文字は
	//		そのまま出力されるので注意。（'\n'など）
	//		ファイル名の場合はSJISと仮定して処理する(Winのみ)
	//
	int skip;
	char *utftmp;
	size_t length = 2;

	utftmp = (char *)src;
#ifdef HSPWIN
	if ( filename ) {
		if ( compopt->pp_utf8 ) { // 入力がUTF8でファイル名の場合は変換する
			int len = strlen( src ) * 4 + 1;
			utftmp = (char *)malloc( len );
			ConvSJis2Utf8( (char *)src, utftmp, len );
		}
	}
#endif
	const unsigned char *s = (unsigned char *)utftmp;
	while ( true ) {
		unsigned char c = *s;
		if ( c == '\0' ) {
			break;
		}
		switch ( c ) {
		case '\r':
			if ( *( s + 1 ) == '\n' ) {
				s++;
			}
			// FALL THROUGH
		case '\t':
		case '"':
		case '\\':
			length += 2;
			break;
		default:
			length++;
			skip = SkipMultiByte( c );
			s += skip;
			length += skip;
			break;
		}
		s++;
	}
	char *dest = (char *)malloc( length + 1 );
	if ( dest == nullptr ) {
		return dest;
	}

	s = (unsigned char *)utftmp;
	auto *d = (unsigned char *)dest;
	*d++ = '"';
	while ( true ) {
		unsigned char c = *s;
		if ( c == '\0' ) {
			break;
		}
		switch ( c ) {
		case '\t':
			*d++ = '\\';
			*d++ = 't';
			break;
		case '\r':
			*d++ = '\\';
			if ( *( s + 1 ) == '\n' ) {
				*d++ = 'n';
				s++;
			} else {
				*d++ = 'r';
			}
			break;
		case '"':
			*d++ = '\\';
			*d++ = '"';
			break;
		case '\\':
			*d++ = '\\';
			*d++ = '\\';
			break;
		default:
			skip = SkipMultiByte( c );
			*d++ = c;
			while ( skip > 0 ) {
				s++;
				*d++ = *s;
				skip--;
			}
		}
		s++;
	}
	*d++ = '"';
	*d = '\0';
#ifdef HSPWIN
	if ( filename ) {
		if ( compopt->pp_utf8 ) {
			free( utftmp );
		}
	}
#endif
	return dest;
}

int CCompilerUtil::atoi_allow_overflow( const char *s )
{
	//		オーバーフローチェックをしないatoi
	//
	int result = 0;
	while ( isdigit( *s ) != 0 ) {
		result = result * 10 + ( *s - '0' );
		s++;
	}
	return result;
}

//-----------------------------------------------------------------------------

CSymbolTable::CSymbolTable() : lb( std::make_unique<CLabel>() ), tmp_lb( nullptr )
{
	lb->Reset();
}


CSymbolTable::~CSymbolTable() = default;


std::unique_ptr<CLabel> CSymbolTable::GetLabelInfo()
{
	//		ラベル情報取り出し
	//		(CLabel *を取得したらそちらで、deleteすること)
	//
	return std::move( lb );
}


void CSymbolTable::SetLabelInfo( std::unique_ptr<CLabel> lbinfo )
{
	//		ラベル情報設定
	//
	tmp_lb = std::move( lbinfo );
}


int CSymbolTable::LabelRegist( char **list, int mode )
{
	//		ラベル情報を登録
	//
	if ( mode != 0 ) {
		return lb->RegistList( list, "@hsp" );
	}
	return lb->RegistList( list, "" );
}


int CSymbolTable::LabelRegist2( char **list )
{
	//		ラベル情報を登録(マクロ)
	//
	return lb->RegistList2( list, "@hsp" );
}


int CSymbolTable::LabelRegist3( char **list )
{
	//		ラベル情報を登録(色分け用)
	//
	return lb->RegistList3( list );
}
