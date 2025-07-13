//
//	token.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lexer_util.h"
#include "logger.h"
#include "token_def.h"

#define CG_FLAG_ENABLE 0
#define CG_FLAG_DISABLE 1

#define CG_LASTCMD_NONE 0
#define CG_LASTCMD_LET 1
#define CG_LASTCMD_CMD 2
#define CG_LASTCMD_CMDIF 3
#define CG_LASTCMD_CMDMIF 4
#define CG_LASTCMD_CMDELSE 5
#define CG_LASTCMD_CMDMELSE 6

#define CG_IFLEV_MAX 128
#define CG_REPLEV_MAX 128

// option for 'GetTokenCG'
#define GETTOKEN_DEFAULT 0
#define GETTOKEN_NOFLOAT 1 // '.'を小数点と見なさない(整数のみ取得)
#define GETTOKEN_LABEL 2   // '*'に続く名前をラベルとして取得
#define GETTOKEN_EXPRBEG 4 // 式の先頭

#define CG_LOCALSTRUCT_MAX 256

#define CG_IFCHECK_SCOPE 0
#define CG_IFCHECK_LINE 1

#define CG_LIBMODE_NONE ( -1 )
#define CG_LIBMODE_DLL 0
#define CG_LIBMODE_DLLNEW 1
#define CG_LIBMODE_COM 2
#define CG_LIBMODE_COMNEW 3


struct CCgToken
{
	int line;
	int val;
	int ttype; // last token type
	// char *lasttoken; // last token point
	// float val_f;
	double val_d;
	// double fpbit;
	// unsigned char *s3;

	char *cg_str;
};

class CCgLexer : public CCompilerUtil
{
public:
	explicit CCgLexer( const std::shared_ptr<CompileOptions> &compopt, std::shared_ptr<CLogger> log );
	~CCgLexer() override;


	CCgToken token;


	int GetParameterTypeCG( const char *name ) const;
	int GetParameterStructTypeCG( const char *name ) const;
	int GetParameterFuncTypeCG( const char *name ) const;
	int GetParameterResTypeCG( const char *name ) const;

	char *GetTokenCG( const char *str, int option );
	char *GetTokenCG( int option );
	char *GetSymbolCG( char *str );
	char *GetLineCG( void );
	int PickNextCodeCG( void );

private:
	char *PickStringCG( char *str, int sep );
	char *PickStringCG2( char *str, char **strsrc );
	char *PickLongStringCG( char *str );

	//		Data
	//
	std::shared_ptr<CLogger> logger;
	unsigned char s2[4096];

public:
	char *cg_ptr;
	char *cg_ptr_bak;
	unsigned char *cg_wp;


	//		for Error
	//
	int cg_orgline;
	char cg_orgfile[HSP_MAX_PATH];
	char cg_orgfilefull[HSP_MAX_PATH];
};
