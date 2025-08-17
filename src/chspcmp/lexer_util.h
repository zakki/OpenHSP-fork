//
//	token.cpp structures
//
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "token_def.h"

#define SCNVBUF_DEFAULTSIZE 0x8000
#define SCNV_OPT_NONE 0
#define SCNV_OPT_SJISUTF8 1
#define SCNV_OPT_UTF8SJIS 2

struct CompileOptions
{
	int pp_utf8;					// ソースコードをUTF-8として処理する(0=無効)
	char common_path[HSP_MAX_PATH]; // common path
	int mode;

	bool cg_debug() const
	{
		return ( mode & COMP_MODE_DEBUG ) != 0;
	}
	bool cg_utf8out() const
	{
		return ( ( mode & COMP_MODE_UTF8 ) != 0 ) && ( pp_utf8 == 0 );
	}
	bool cg_strmap() const
	{
		return ( mode & COMP_MODE_STRMAP ) != 0;
	}
	bool cg_skiperror() const
	{
		return ( mode & COMP_MODE_SKIPERROR ) != 0;
	}

	//		for Header info
	int hed_option;
	char hed_runtime[64];
	int hed_cmpmode;
	int hed_autoopt_timer;
	int hed_autoopt_strexchange;

	void Reset()
	{
		hed_option = 0;
		hed_runtime[0] = 0;
		hed_autoopt_timer = 0;
		hed_autoopt_strexchange = 0;
		pp_utf8 = 0;
	}

	void SetMode( int m )
	{
		mode = m;
	}

	void SetUTF8Input( int utf8mode )
	{
		pp_utf8 = utf8mode;
	}
	void SetCommonPath( const char *path )
	{
		if ( path == nullptr ) {
			common_path[0] = 0;
			return;
		}
		strcpy( common_path, path );
	}

	int GetHeaderOption( void )
	{
		return hed_option;
	}
	char *GetHeaderRuntimeName( void )
	{
		return hed_runtime;
	}
	void SetHeaderOption( int opt, const char *name )
	{
		hed_option = opt;
		strcpy( hed_runtime, name );
	}
	int GetCmpOption( void )
	{
		return hed_cmpmode;
	}
	void SetCmpOption( int cmpmode )
	{
		hed_cmpmode = cmpmode;
	}


	void addCmpMode( int mode )
	{
		hed_cmpmode |= mode;
	}
	void delCmpMode( int mode )
	{
		hed_cmpmode &= ~mode;
	}
};

//  util class
class CCompilerUtil
{
public:
	explicit CCompilerUtil( std::shared_ptr<CompileOptions> opt );
	virtual ~CCompilerUtil()
	{
	}

protected:
	char *ExecSCNV( const char *srcbuf, int opt );
	int SkipMultiByte( unsigned char byte );

	//	UTF-8 service
	char *to_hsp_string_literal( const char *src, bool filename = false );
	int atoi_allow_overflow( const char *s );
	int ConvSJis2Utf8( const char *pSource, const char *pDist, int buffersize );
	int ConvUtf82SJis( const char *pSource, char *pDist, int buffersize );

	//		Data
	//
	std::shared_ptr<CompileOptions> compopt;

private:
	void InitSCNV( size_t size );
	int CheckByteSJIS( unsigned char byte );
	int CheckByteUTF8( unsigned char byte );

	//		for SCNV
	//
	std::vector<char> scnvbuf; // SCNV変換バッファ
};

//	String Service
void strcase2( const char *str, char *str2 );
void strcpy2( char *dest, const char *src, size_t size );
void addext( char *st, const char *exstr );
void cutext( char *st );

class CLabel;

class CSymbolTable
{
public:
	CSymbolTable();
	~CSymbolTable();

	std::unique_ptr<CLabel> GetLabelInfo( void );
	void SetLabelInfo( std::unique_ptr<CLabel> lbinfo );

	int LabelRegist( char **list, int mode );
	int LabelRegist2( char **list );
	int LabelRegist3( char **list );

	std::unique_ptr<CLabel> lb;		// label object
	std::unique_ptr<CLabel> tmp_lb; // label object (preprocessor reference)
};
