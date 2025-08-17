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

#include "preprocessor_lexer.h"
#include "token_def.h"

#define CALCVAR double
#define LINEBUF_MAX 0x10000

class CLabel;
class CMemBuf;
class CTagStack;
class CStrNote;
class AHTMODEL;

//  token analysis class
class CPreProcessor : public CCompilerUtil
{
public:
	// CPreProcessor();
	explicit CPreProcessor( const std::shared_ptr<CompileOptions> &compopt, const std::shared_ptr<CLogger> &log );
	~CPreProcessor() override;

	void SetAHT( AHTMODEL *aht );
	void SetAHTBuffer( CMemBuf *aht );
	void SetLabelListBuffer( CMemBuf *buf, int mode, char *match, int line = 0, char *filename = nullptr );
	char *GetLabelListHeader( int flag );

	void ResetCompiler( void );
	int Calc( CALCVAR &val );
	char *CheckValidWord( void );


	//		For preprocess
	//
	ppresult_t Preprocess( char *str );
	ppresult_t PreprocessNM( char *str );
	void PreprocessCommentCheck( char *str );

	int ExpandLine( CMemBuf *buf, CMemBuf *src, char *refname );
	int ExpandFile( CMemBuf *buf, char *fname, char *refname );
	void FinishPreprocess( CMemBuf *buf );
	int SetAdditionMode( int mode );

	int LabelDump( CMemBuf &out, int option, char *match = nullptr );
	int RegistExtMacroPath( char *name, char *str );
	int RegistExtMacro( char *name, char *str );
	int RegistExtMacro( char *keyword, int val );
	void SetPackfileOut( CMemBuf *pack );
	int AddPackfile( char *name, int mode );
	int AddPackfileOrig( char *name, int mode );

	// void GenerateLabelList(int mode, char* match);
	char *GetLabelListLineModule( void );
	int GetLabelListLineCaseFlag( void );

protected:
	//		For preprocess
	//
	void Calc_token( void );
	void Calc_factor( CALCVAR &v );
	void Calc_unary( CALCVAR &v );
	void Calc_muldiv( CALCVAR &v );
	void Calc_addsub( CALCVAR &v );
	void Calc_bool( CALCVAR &v );
	void Calc_bool2( CALCVAR &v );
	void Calc_compare( CALCVAR &v );
	void Calc_start( CALCVAR &v );

	ppresult_t PP_IncludeSub( char *name, int is_addition );

	ppresult_t PP_Define( void );
	ppresult_t PP_Const( void );
	ppresult_t PP_Enum( void );
	ppresult_t PP_SwitchStart( int sw );
	ppresult_t PP_SwitchEnd( void );
	ppresult_t PP_SwitchReverse( void );
	ppresult_t PP_use( void );
	ppresult_t PP_Include( int is_addition );
	ppresult_t PP_Module( void );
	ppresult_t PP_Global( void );
	ppresult_t PP_Deffunc( int mode );
	ppresult_t PP_Defcfunc( int mode );
	ppresult_t PP_Struct( void );
	ppresult_t PP_Func( char *name );
	ppresult_t PP_Cmd( char *name );
	ppresult_t PP_Pack( int mode );
	ppresult_t PP_PackOpt( void );
	ppresult_t PP_RuntimeOpt( void );
	ppresult_t PP_CmpOpt( void );
	ppresult_t PP_Usecom( void );
	ppresult_t PP_Aht( void );
	ppresult_t PP_Ahtout( void );
	ppresult_t PP_Ahtmes( void );
	ppresult_t PP_BootOpt( void );
	ppresult_t PP_VarFix( char *word );

	void SetModuleName( char *name );
	char *GetModuleName( void );
	void AddModuleName( char *str );
	void FixModuleName( char *str );
	int IsGlobalMode( void );
	int CheckModuleName( char *name );

	char *SkipLine( char *str, int *pline );
	char *ExpandStr( char *str, int opt );
	char *ExpandStrEx( char *str );
	char *ExpandStrComment( char *str, int opt );
	char *ExpandStrComment2( char *str );
	char *ExpandAhtStr( char *str );
	char *ExpandBin( char *str, int *val );
	char *ExpandHex( char *str, int *val );
	char *ExpandToken( char *str, int *type, int ppmode );
	int ExpandTokens( char *vp, CMemBuf *buf, int *lineext, int is_preprocess_line );
	char *SendLineBuf( char *str );
	char *SendLineBufPP( char *str, int *lines );
	int ReplaceLineBuf( char *str1, char *str2, char *repl, int macopt, MACDEF *macdef );

	void SetErrorSymbolOverdefined( char *keyword, int label_id );

public:
	void GenerateLabelTag( char *name, int flag, int type, char *fname, int line );
	void GenerateLabelListAndTagPP( char *name, int flag = 0 );
	void GenerateLabelListAndTagRefPP( char *name, int flag = 0 );

private:
	//		Data
	//
	std::shared_ptr<CLogger> logger;

public:
	CPpLexer lexer;
	const CPpToken *token;
	CSymbolTable symtab;
	int ttype;		 // last token type
	char *lasttoken; // last token point

private:
	unsigned char s2[4096];
	std::unique_ptr<CTagStack> tstack; // tag stack object
	CMemBuf *wrtbuf;
	CMemBuf *packbuf;
	CMemBuf *ahtbuf;
	CMemBuf *labbuf;
	CStrNote *note;
	AHTMODEL *ahtmodel; // AHT process data
	// char common_path[HSP_MAX_PATH];	// common path
	char search_path[HSP_MAX_PATH]; // search path

public:
private:
	char linebuf[LINEBUF_MAX];	 // Line expand buffer
	char linetmp[LINEBUF_MAX];	 // Line expand temp
	char mestmp[128];			 // meseage temp
	int incinf;					 // include level
	int mulstr;					 // multiline string flag
	short swstack[SWSTACK_MAX];	 // generator sw stack (flag)
	short swstack2[SWSTACK_MAX]; // generator sw stack (mode)
	short swstack3[SWSTACK_MAX]; // generator sw stack (sw)
	int swsp;					 // generator sw stack pointer
	int swmode;					 // generator sw mode (0=if/1=else)
	int swlevel;				 // first stack level ( when off )
	int fileadd;				 // File Addition Mode (1=on)
	int swflag;					 // generator sw enable flag
	char *ahtkeyword;			 // keyword for AHT

	char modname[MODNAME_MAX + 2]; // Module Name Prefix
	int modgc;					   // Global counter for Module
	int enumgc;					   // Global counter for Enum
	using undefined_symbol_t = struct undefined_symbol_t
	{
		int pos;
		int len_include_modname;
		int len;
	};
	std::vector<undefined_symbol_t> undefined_symbols;

	//		for CodeGenerator
	//
	int cg_labout_mode;
	int cg_labout_line;
	char *cg_labout_match;
	char **cg_labout_header;
	char cg_labout_modname[MODNAME_MAX + 2]; // Module Name Prefix
	int cg_labout_caseflag;
	char cg_labout_orgfile[HSP_MAX_PATH];

public:
	//		for Error
	//
	int pp_orgline;
	char pp_orgfile[HSP_MAX_PATH];
	char pp_orgfilefull[HSP_MAX_PATH];
};
