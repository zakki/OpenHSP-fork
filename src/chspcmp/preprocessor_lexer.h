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

class CMemBuf;
class CTagStack;
class CStrNote;

struct CPpToken
{
	int val;
	float val_f;
	double val_d;
	double fpbit;
	unsigned char *s3;
};

//  token analysis class
class CPpLexer : public CCompilerUtil
{
public:
	explicit CPpLexer( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log );
	virtual ~CPpLexer();

	void ResetCompiler( void );

	int GetToken( void );
	int PeekToken( void );

	void SetLook( char *buf );
	char *GetLook( void );
	char *GetLookResult( void );
	int GetLookResultInt( void );

private:
	void Pickstr( void );
	char *Pickstr2( char *str );

	//		Data
	//
	std::shared_ptr<CLogger> logger;

public:
	unsigned char *wp;
	int line;

	CPpToken token; // token data
};
