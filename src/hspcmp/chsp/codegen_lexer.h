//
//	token.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../token_def.h"
#include "lexer_util.h"
#include "logger.h"

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
	int line{};
	int val{};
	int ttype{}; // last token type
	// char *lasttoken  {}; // last token point
	// float val_f  {};
	double val_d{};
	// double fpbit  {};
	// unsigned char *s3  {};

	std::string cg_str{};
};

class CCgLexer : public CSourceTextUtil
{
public:
	explicit CCgLexer( const std::shared_ptr<CompileOptions> &compopt, std::shared_ptr<CLogger> log );
	~CCgLexer() override;

	int GetParameterTypeCG( const std::string &name ) const;
	int GetParameterStructTypeCG( const std::string &name ) const;
	int GetParameterFuncTypeCG( const std::string &name ) const;
	int GetParameterResTypeCG( const std::string &name ) const;

	std::pair<const char *, CCgToken> GetTokenCG( const char *str, int option );
	CCgToken GetTokenCG( int option );
	std::string GetSymbolCG( const char *str );
	int PickNextCodeCG( void );

	const char *NextLine();

private:
	const char *GetLineCG( void );
	const char *PickStringCG( const char *str, int sep, std::string &out );
	const char *PickStringCG2( std::string &out, const char **strsrc );
	const char *PickLongStringCG( const char *str, std::string &out );

	//		Data
	//
	std::shared_ptr<CLogger> logger;
	std::string current_line_buffer;

public:
	int line;
	const char *cg_ptr;
	const char *cg_ptr_bak;
	const char *cg_wp;


	//		for Error
	//
	int cg_orgline;
	std::string cg_orgfile;
	std::string cg_orgfilefull;
};
