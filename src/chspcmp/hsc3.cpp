
//
//		HSP compiler class rev.3
//			onion software/onitama 2002/2
//
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../hsp3/hsp3config.h"
#include "../hsp3/hsp3debug.h"
#include "../hsp3/hsp3struct.h"
#include "../hsp3/strnote.h"

#include "hsc3.h"
#include "supio.h"

#include "codegen.h"
#include "label.h"
#include "localinfo.h"
#include "membuf.h"
#include "preprocessor.h"

extern char *hsp_prestr[];
extern char *hsp_prepp[];

enum
{
	ERRBUF_SIZE = 0x10000
};

//-------------------------------------------------------------
//		Routines
//-------------------------------------------------------------

char *CHsc3::GetError()
{
	return errbuf->GetBuffer();
}


int CHsc3::GetErrorSize()
{
	return errbuf->GetSize() + 1;
}


char *CHsc3::GetAnalysisInfo()
{
	if ( anabuf == nullptr ) {
		return nullptr;
	}
	return anabuf->GetBuffer();
}


int CHsc3::GetAnalysisInfoSize()
{
	if ( anabuf == nullptr ) {
		return 0;
	}
	return anabuf->GetSize() + 1;
}


char *CHsc3::GetAnalysisLineInfo( int type )
{
	char *p = "";
	if ( anabuf == nullptr ) {
		return p;
	}
	switch ( type ) {
	case 0:
		p = analyse_module;
		break;
	case 1:
		if ( analyse_caseflag != 0 ) {
			p = "*";
		}
		break;
	default:
		break;
	}
	return p;
}


void CHsc3::InitAnalysisInfo( int mode, char *match, int line )
{
	DeleteAnalysisInfo();
	anabuf = new CMemBuf;
	analyse_mode = mode;
	analyse_line = line;
	*analyse_keyword = 0;
	*analyse_module = 0;
	analyse_match = nullptr;
	if ( match != nullptr ) {
		if ( *match != 0 ) {
			strncpy( analyse_keyword, match, 255 );
			analyse_match = analyse_keyword;
			strcase( analyse_match );
		}
	}
}


void CHsc3::DeleteAnalysisInfo()
{
	if ( anabuf != nullptr ) {
		delete anabuf;
		anabuf = nullptr;
	}
}


void CHsc3::ResetError()
{
	//		エラーメッセージ消去
	//
	errbuf = std::make_shared<CMemBuf>( ERRBUF_SIZE );
	hed_option = 0;
	hed_runtime[0] = 0;
}


//-------------------------------------------------------------
//		Interfaces
//-------------------------------------------------------------

CHsc3::CHsc3()
	: ahtbuf( nullptr ), anabuf( nullptr ), lb_info( nullptr ), analyse_mode( 0 ), analyse_line( 0 ),
	  analyse_caseflag( 0 ), analyse_match( nullptr )
{
	errbuf = std::make_shared<CMemBuf>( ERRBUF_SIZE );


	addkw = nullptr;
	common_path[0] = 0;
}


CHsc3::~CHsc3()
{
	DeleteAnalysisInfo();
}


void CHsc3::AddSystemMacros( CPreProcessor &lexer, int option )
{
	process_option = option;
	if ( ( option & HSC3_OPT_NOHSPDEF ) == 0 ) {
		CLocalInfo linfo;
		lexer.RegistExtMacro( "__hspver__", vercode );
		lexer.RegistExtMacro( "__hsp30__", "" );
		lexer.RegistExtMacro( "__date__", linfo.CurrentDate() );
		lexer.RegistExtMacro( "__time__", linfo.CurrentTime() );
		lexer.RegistExtMacro( "__line__", 0 );
		lexer.RegistExtMacro( "__file__", "" );
		lexer.RegistExtMacro( "__runtime__", "\"hsp3\"" );
		if ( ( option & HSC3_OPT_UTF8IN ) != 0 ) {
			lexer.RegistExtMacro( "_hsputf8", "" );
		}
		if ( ( option & HSC3_OPT_DEBUGMODE ) != 0 ) {
			lexer.RegistExtMacro( "_debug", "" );
		}

#ifdef HSPWIN // Windows(WIN32) version flag
		lexer.RegistExtMacro( "_hspwin", "" );
#endif
#ifdef HSPMAC // Macintosh version flag
		lexer.RegistExtMacro( "_hspmac", "" );
#endif
#ifdef HSPLINUX // Linux(CLI) version flag
		lexer.RegistExtMacro( "_hsplinux", "" );
#endif
#ifdef HSPIOS // iOS version flag
		lexer.RegistExtMacro( "_hspios", "" );
#endif
#ifdef HSPNDK // android NDK version flag
		lexer.RegistExtMacro( "_hspndk", "" );
#endif
#ifdef HSPEMSCRIPTEN // EMSCRIPTEN version flag
		lexer.RegistExtMacro( "_hspemscripten", "" );
#else
		if ( ( option & HSC3_OPT_EMSCRIPTEN ) != 0 ) {
			lexer.RegistExtMacro( "_hspemscripten", "" );
		}
#endif
	}
}


int CHsc3::PreProcessAht( char *fname, void *ahtoption, int mode )
{
	//		Preprocess execute (AHT)
	//		(終了時にPreProcessEndを呼ぶこと)
	//
	int res;
	char mm[512];
	auto opts = std::make_shared<CompileOptions>();
	auto logger = std::make_shared<CLogger>( errbuf );
	CPreProcessor lexer( opts, logger );

	lb_info = nullptr;
	ahtbuf = nullptr;
	opts->SetCommonPath( common_path );
	lexer.SetAHT( (AHTMODEL *)ahtoption );
	outbuf = new CMemBuf;

	if ( mode != 0 ) {
		ahtbuf = new CMemBuf;
		lexer.SetAHTBuffer( ahtbuf );
	}

	sprintf( mm, "#AHT processor ver%s / onion software 1997-2025(c)", hspver );
	logger->Mes( mm );
	res = lexer.ExpandFile( outbuf, fname, fname );
	if ( res < 0 ) {
		return -1;
	}
	return 0;
}


/*
	rev 54
	mingw : warning : packbuf は未初期化で使用されうる
	問題なさそう、一応対処。
*/

int CHsc3::PreProcess( char *fname, char *outname, int option, char *rname, void *ahtoption )
{
	//		Preprocess execute
	//		(終了時にPreProcessEndを呼ぶこと)
	//			option : bit0=ver2.55 mode(ON)
	//			         bit1=debug mode(ON)
	//			         bit2=make packfile(ON)
	//					 bit3=read AHT file(on)
	//					 bit4=write AHT file(on)
	//					 bit5=UTF8(input)(入力ソースがUTF8であることを示す)
	//					 bit8=Emscripten mode(ON)(Emscripten向けであることを示す)
	//
	int res;
	char mm[512];
	auto opts = std::make_shared<CompileOptions>();
	auto logger = std::make_shared<CLogger>( errbuf );
	// CCodeGenerator tk(opts, logger);
	CPreProcessor lexer( opts, logger );
	CMemBuf *packbuf = nullptr;

	lb_info = nullptr;
	outbuf = new CMemBuf;
	ahtbuf = nullptr;

	opts->SetCommonPath( common_path );
	lexer.symtab.LabelRegist2( hsp_prestr );
	AddSystemMacros( lexer, option );

	if ( ( option & HSC3_OPT_MAKEPACK ) != 0 ) {
		packbuf = new CMemBuf( 0x1000 );
		lexer.SetPackfileOut( packbuf );
	}
	if ( ( option & ( HSC3_OPT_READAHT | HSC3_OPT_MAKEAHT ) ) != 0 ) {
		lexer.SetAHT( (AHTMODEL *)ahtoption );
	}

	if ( ( option & HSC3_OPT_UTF8IN ) != 0 ) {
		opts->SetUTF8Input( 1 );
	}

	sprintf( mm, "#%s ver%s / onion software 1997-2025(c)", HSC3TITLE, hspver );
	logger->Mes( mm );

	if ( anabuf != nullptr ) {
		lexer.SetLabelListBuffer( anabuf, analyse_mode, analyse_match, analyse_line, rname );
	}
	lexer.SetAdditionMode( 1 );
	res = lexer.ExpandFile( outbuf, "hspdef.as", "hspdef.as" );
	lexer.SetAdditionMode( 0 );
	if ( res < -1 ) {
		return -1;
	}
	res = lexer.ExpandFile( outbuf, fname, rname );
	if ( res < 0 ) {
		return -1;
	}
	lexer.FinishPreprocess( outbuf );

	cmpopt = opts->GetCmpOption();
	if ( ( cmpopt & CMPMODE_PPOUT ) != 0 ) {
		res = outbuf->SaveFile( outname );
		if ( res < 0 ) {
#ifdef JPNMSG
			logger->Mes( "#プリプロセッサファイルの出力に失敗しました" );
#else
			logger->Mes( "#Can't write output file." );
#endif
			return -2;
		}
	}
	outbuf->Put( (int)0 );

	if ( anabuf != nullptr ) {
		strcpy( analyse_module, lexer.GetLabelListLineModule() );
		analyse_caseflag = lexer.GetLabelListLineCaseFlag();
	}
#if 0
	//		ソースのラベルを追加(停止中)
	addkw = std::shared_ptr<CMemBuf>( 0x1000 );
	lexer.LabelDump( addkw, DUMPMODE_DLLCMD );
#endif

	// sprintf( mm,"#Macro buffer %x.", tk.GetLabelBufferSize() );
	// tk.Mes( mm );

	if ( ( option & HSC3_OPT_MAKEPACK ) != 0 ) {
		lexer.AddPackfile( "start.ax", 1 );
		res = packbuf->SaveFile( "packfile" );
		delete packbuf;
		if ( res < 0 ) {
#ifdef JPNMSG
			logger->Mes( "#packfileの出力に失敗しました" );
#else
			logger->Mes( "#Can't write packfile." );
#endif
			return -3;
		}
		logger->Mes( "#packfile generated." );
	}

	hed_option = opts->GetHeaderOption();
	if ( ( cmpopt & CMPMODE_UTF8OUT ) != 0 ) {
		hed_option |= HEDINFO_UTF8;
	}

	strcpy( hed_runtime, opts->GetHeaderRuntimeName() );
	lb_info = lexer.symtab.GetLabelInfo();

	return 0;
}


void CHsc3::PreProcessEnd()
{
	lb_info = nullptr;
	if ( outbuf != nullptr ) {
		delete outbuf;
		outbuf = nullptr;
	}
	if ( ahtbuf != nullptr ) {
		delete ahtbuf;
		ahtbuf = nullptr;
	}
}


int CHsc3::Compile( char *fname, char *outname, int mode )
{
	//		Compile
	//

	int res;
	int genmode;
	char mm[512];
	auto opts = std::make_shared<CompileOptions>();
	auto logger = std::make_shared<CLogger>( errbuf );
	CCodeGenerator tk( opts, logger );

	genmode = mode;
	if ( ( cmpopt & CMPMODE_UTF8OUT ) != 0 ) {
		genmode |= HSC3_MODE_UTF8;
	}

	if ( lb_info ) {
		tk.SetLabelInfo( std::move( lb_info ) ); // プリプロセッサのラベル情報
	}

	opts->SetCommonPath( common_path );
	tk.LabelRegist( hsp_prestr, 1 );
	opts->SetHeaderOption( hed_option, hed_runtime );
	opts->SetCmpOption( cmpopt );

	if ( ( process_option & HSC3_OPT_UTF8IN ) != 0 ) {
		opts->SetUTF8Input( 1 );
	}

	sprintf( mm, "#%s ver%s / onion software 1997-2025(c)", HSC3TITLE2, hspver );
	logger->Mes( mm );

	if ( ( genmode & HSC3_MODE_LABOUT ) != 0 ) {
		opts->delCmpMode( CMPMODE_OPTCODE );
		tk.SetLabelListBuffer( anabuf, analyse_mode, analyse_match );
		res = tk.GenerateCode( outbuf, outname, genmode | COMP_MODE_SKIPERROR );
		return res;
	}

	if ( outbuf != nullptr ) {
		res = tk.GenerateCode( outbuf, outname, genmode );
	} else {
		res = tk.GenerateCode( fname, outname, genmode | COMP_MODE_STRMAP );
	}

	return res;
}


int CHsc3::CompileStrMap( char *fname, char *outname, int mode )
{
	return Compile( fname, outname, mode | HSC3_MODE_STRMAP );
}


int CHsc3::CompileLabelOut( char *fname, int mode )
{
	return Compile( fname, "", mode | HSC3_MODE_LABOUT );
}


void CHsc3::SetCommonPath( char *path )
{
	if ( path == nullptr ) {
		common_path[0] = 0;
		return;
	}
	strcpy( common_path, path );
}


int CHsc3::GetCmdList( int option, char *match )
{
	int res;
	auto opts = std::make_shared<CompileOptions>();
	auto logger = std::make_shared<CLogger>( errbuf );
	CPreProcessor lexer( opts, logger );
	CMemBuf outbuf;

	opts->SetCommonPath( common_path );
	lexer.symtab.LabelRegist3( hsp_prestr ); // 標準キーワード
	lexer.symtab.LabelRegist3( hsp_prepp );	 // プリプロセッサキーワード

	res = lexer.ExpandFile( &outbuf, "hspdef.as", "hspdef.as" );
	lexer.LabelDump( *errbuf, DUMPMODE_ALL, match );

	return 0;
}


int CHsc3::OpenPackfile()
{
	pfbuf = new CMemBuf( 0x1000 );
	if ( pfbuf->PutFile( "packfile" ) < 0 ) {
		delete pfbuf;
		return -1;
	}
	return 0;
}


void CHsc3::GetPackfileOption( char *out, char *keyword, char *defval )
{
	int max;
	int i;
	char tmp[512];
	char *s;
	char a1;
	CStrNote note;
	note.Select( pfbuf->GetBuffer() );
	max = note.GetMaxLine();
	strcpy( out, defval );
	for ( i = 0; i < max; i++ ) {
		note.GetLine( tmp, i );
		if ( ( tmp[0] == ';' ) && ( tmp[1] == '!' ) ) {
			s = tmp + 2;
			while ( true ) {
				a1 = *s;
				if ( ( a1 == 0 ) || ( a1 == '=' ) ) {
					break;
				}
				s++;
			}
			if ( a1 != 0 ) {
				s[0] = 0;
				if ( strcmp( tmp + 2, keyword ) == 0 ) {
					strcpy( out, s + 1 );
				}
			}
		}
	}
}


int CHsc3::GetPackfileOptionInt( char *keyword, int defval )
{
	char tmp[512];
	char deftmp[32];
	sprintf( deftmp, "%d", defval );
	GetPackfileOption( tmp, keyword, deftmp );
	if ( ( tmp[0] >= '0' ) && ( tmp[0] <= '9' ) ) {
		return atoi( tmp );
	}
	return defval;
}


void CHsc3::ClosePackfile()
{
	delete pfbuf;
}


int CHsc3::GetRuntimeFromHeader( char *fname, char *res )
{
	FILE *fp;
	HSPHED hsphed;
	int hedsize;
	int exsize;
	int ires;
	char *data;

	fp = fopen( fname, "rb" );
	if ( fp == nullptr ) {
		return -1;
	}
	hedsize = sizeof( hsphed );
	fread( &hsphed, 1, hedsize, fp );
	exsize = hsphed.pt_cs - hedsize;

	if ( exsize == 0 ) {
		fclose( fp );
		return 0;
	}

	data = (char *)malloc( exsize );
	fread( data, 1, exsize, fp );
	fclose( fp );
	ires = 0;
	if ( ( hsphed.bootoption & HSPHED_BOOTOPT_RUNTIME ) != 0 ) {
		char runtime[HSP_MAX_PATH];
		strcpy( runtime, data + ( hsphed.runtime - hedsize ) );
		cutext( runtime );
		addext( runtime, "exe" );
		strcpy( res, runtime );
		ires = 1;
	}
	free( data );
	return ires;
}


int CHsc3::SaveOutbuf( char *fname )
{
	int res;
	res = outbuf->SaveFile( fname );
	if ( res < 0 ) {
		return -1;
	}
	return 0;
}


int CHsc3::SaveAHTOutbuf( char *fname )
{
	int res;
	res = ahtbuf->SaveFile( fname );
	if ( res < 0 ) {
		return -1;
	}
	return 0;
}


void CHsc3::Print( char *mes )
{
	errbuf->PutStr( mes );
	errbuf->PutStr( "\r\n" );
}
