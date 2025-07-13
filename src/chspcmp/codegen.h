//
//	codegen.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lexer_util.h"
#include "logger.h"
#include "token_def.h"

#include "codegen_lexer.h"
#include "codegen_writer.h"


class CMemBuf;


//  token analysis class
class CCodeGenerator : public CCompilerUtil
{
public:
	explicit CCodeGenerator( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log );
	~CCodeGenerator() override;

	void ResetCompiler( void );
	void SetLabelListBuffer( CMemBuf *buf, int mode, char *match, int line = 0, char *filename = nullptr );

	//		For Code Generate
	//
	int GenerateCode( char *fname, char *oname, int mode );
	int GenerateCode( CMemBuf *srcbuf, char *oname, int mode );

private:
	void CalcCG( int ex );

	std::shared_ptr<CLogger> logger;
	CCgLexer lexer;
	CCgToken *token;
	std::unique_ptr<CCodeWriter> writer;

public:
	std::shared_ptr<CSymbolTable> symtab;

private:
	//		For Code Generate
	//
	void ResetGenerator( unsigned char *ptr );
	int GenerateCodeMain( CMemBuf *src );
	int GenerateCodeMainSkipError( CMemBuf *src );
	void RegisterFuncLabels( void );
	int GenerateCodeBlock( void );
	int GenerateCodeSub( void );
	void GenerateCodePP( char *buf );
	void GenerateCodeCMD( int id );
	void GenerateCodeLET( int id, bool first = false );
	void GenerateCodeVAR( int id, int ex );
	void GenerateCodePRM( void );
	void GenerateCodePRMN( void );
	int GenerateCodePRMF( void );
	void GenerateCodePRMF2( void );
	void GenerateCodePRMF3( void );
	int GenerateCodePRMF4( int t );
	void GenerateCodeMethod( void );
	void GenerateCodeLabel( char *name, int ex );

	void GenerateCodePP_regcmd( void );
	void GenerateCodePP_cmd( void );
	void GenerateCodePP_deffunc0( int is_command );
	void GenerateCodePP_deffunc( void );
	void GenerateCodePP_defcfunc( void );
	void GenerateCodePP_uselib( void );
	void GenerateCodePP_module( void );
	void GenerateCodePP_struct( void );
	void GenerateCodePP_func( int deftype );
	void GenerateCodePP_usecom( void );
	void GenerateCodePP_comfunc( void );
	void GenerateCodePP_defvars( int fixedvalue );

	void GenerateLabelTag( char *name, int flag, int type, char *fname, int line );
	void GenerateLabelListAndTag( int labelid, int flag = 0 );
	void GenerateLabelListAndTag( char *name, int flag = 0 );
	void GenerateLabelListAndTagRef( int labelid, int flag = 0 );

	void CheckInternalListenerCMD( int opt );
	int CheckInternalProgCMD( int opt, int orgcs );
	void CheckInternalIF( int opt );
	void CheckCMDIF_Set( int mode );
	void CheckCMDIF_Fin( int mode );

	int SetVarsFixed( char *varname, int fixedvalue );

	void CalcCG_token( void );
	void CalcCG_token_exprbeg( void );
	void CalcCG_token_exprbeg_redo( void );
	void CalcCG_regmark( int mark );
	void CalcCG_factor( void );
	void CalcCG_unary( void );
	void CalcCG_muldiv( void );
	void CalcCG_addsub( void );
	void CalcCG_shift( void );
	void CalcCG_bool( void );
	void CalcCG_compare( void );
	void CalcCG_start( void );

	bool CG_optCode() const
	{
		return ( compopt->hed_cmpmode & CMPMODE_OPTCODE ) != 0;
	}
	bool CG_optInfo() const
	{
		return ( compopt->hed_cmpmode & CMPMODE_OPTINFO ) != 0;
	}
	void CG_MesLabelDefinition( int label_id );

	// int SaveStringMap( char *fname );

	char *GetLabelListHeader( int flag );

	//		Data
	//
	// unsigned char s2[4096];
	int texflag;
	CMemBuf *labbuf;

	int cs_lastptr;	 // パラメーターの初期CS位置
	int cs_lasttype; // パラメーターのタイプ(単一時)
	int calccount;	 // パラメーター個数

	//		for CodeGenerator
	//
	int cg_flag;
	int cg_iflev;
	int cg_valcnt;
	int cg_typecnt;
	int cg_pptype;
	int cg_locallabel;
	int cg_varhpi;
	int cg_putvars;
	int cg_defvarfix;

	char cg_libname[1024];
	int cg_labout_mode;
	int cg_labout_line;
	char *cg_labout_match;
	char **cg_labout_header;
	char cg_labout_orgfile[HSP_MAX_PATH];

	int replev;
	int repend[CG_REPLEV_MAX];
	int iflev;
	int iftype[CG_IFLEV_MAX];
	int ifmode[CG_IFLEV_MAX];
	int ifscope[CG_IFLEV_MAX];
	int ifptr[CG_IFLEV_MAX];
	int ifterm[CG_IFLEV_MAX];

	int cg_lastcmd;
	int cg_lasttype;
	int cg_lastval;

	//		for Struct
	int cg_libindex;
	int cg_libmode;
	int cg_localstruct[CG_LOCALSTRUCT_MAX];
	int cg_localcur;

	//		for Error
	//
	int cg_errline;
	// int cg_orgline;
	// char cg_orgfile[HSP_MAX_PATH];
	// char cg_orgfilefull[HSP_MAX_PATH];
};
