
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
#include "ahtobj.h"
#include "label.h"
#include "logger.h"
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


//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

void CLogger::Mes( const char *mes )
{
	//		メッセージ登録
	//
	errbuf->PutStr( mes );
	errbuf->PutStr( "\r\n" );
}


void CLogger::Mesf( const char *format, ... )
{
	//		メッセージ登録
	//		(フォーマット付き)
	//
	char textbf[1024];
	va_list args;
	va_start( args, format );
	vsprintf( textbf, format, args );
	va_end( args );
	errbuf->PutStr( textbf );
	errbuf->PutStr( "\r\n" );
}


void CLogger::Error( const char *mes )
{
	//		エラーメッセージ登録
	//
	char tmp[256];
	sprintf( tmp, "#Error:%s\r\n", mes );
	errbuf->PutStr( tmp );
}

void CLogger::LineError( int line, const char *fname )
{
	//		エラーメッセージ登録(line/filename)
	//
	char *mes = errtmp;
	char tmp[256];
	sprintf( tmp, "#Error:%s in line %d [%s]\r\n", mes, line, fname );
	errbuf->PutStr( tmp );
}


void CLogger::SetErrorBuf( const std::shared_ptr<CMemBuf> &buf )
{
	//		エラーメッセージバッファ登録
	//
	errbuf = buf;
}


void CLogger::SetError( const char *mes )
{
	//		エラーメッセージ仮登録
	//
	strcpy( errtmp, mes );
}
