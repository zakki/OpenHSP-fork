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
#include "chsp_frontend_v3_ast.h"
#include "../token_def.h"

#include "codegen_lexer.h"
//CG: #include "codegen_writer.h"


class CMemBuf;


//  token analysis class
class CChspParser : public CSourceTextUtil
{
public:
	explicit CChspParser( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log );
	~CChspParser() override;

	void ResetCompiler( void );
	void SetLabelListBuffer( CMemBuf *buf, int mode, char *match, int line = 0, char *filename = nullptr );

	void SetLabelInfo( CLabel* lbinfo )
	{
		symtab->SetLabelInfo( lbinfo );
	}
	int LabelRegist( char **list, int mode )
	{
		return symtab->LabelRegist( list, mode );
	}

	//		For Code Generate
	//
	int GenerateCode( const std::string &fname, const std::string &oname, int mode );
	int GenerateCode( CMemBuf *srcbuf, const std::string &oname, int mode );
	const chspv3::ChspV3AstProgram &GetAstProgram( void ) const
	{
		return ast_program;
	}

private:
	void CalcCG( int ex );

	std::shared_ptr<CLogger> logger;
	CCgLexer lexer;
	CCgToken token;
	//CG: std::unique_ptr<CCodeWriter> writer;
	std::shared_ptr<CSymbolTable> symtab;

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
	void GenerateCodeVAR( int id, int ex, std::unique_ptr<chspv3::ChspV3AstExpr> *ast_target = nullptr );
	void GenerateCodePRM( void );
	void GenerateCodePRMN( void );
	int GenerateCodePRMF( std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> *ast_args = nullptr );
	void GenerateCodePRMF2( std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> *ast_args = nullptr );
	void GenerateCodePRMF3( void );
	int GenerateCodePRMF4( int t, std::unique_ptr<chspv3::ChspV3AstExpr> *ast_target = nullptr );
	void GenerateCodeMethod( void );
	void GenerateCodeLabel( const std::string &name, int ex );

	void GenerateCodePP_regcmd( void );
	void GenerateCodePP_cmd( void );
	void GenerateCodePP_deffunc0( int is_command );
	void GenerateCodePP_deffunc( void );
	void GenerateCodePP_defcfunc( void );
	void GenerateCodePP_chsp_deffunc0( int is_command );
	void GenerateCodePP_chsp_deffunc( void );
	void GenerateCodePP_chsp_defcfunc( void );
	void GenerateCodePP_chsp_module( void );
	void GenerateCodePP_chsp_module_end( void );
	void GenerateCodePP_uselib( void );
	void GenerateCodePP_module( void );
	void GenerateCodePP_struct( void );
	void GenerateCodePP_func( int deftype );
	void GenerateCodePP_usecom( void );
	void GenerateCodePP_comfunc( void );
	void GenerateCodePP_defvars( int fixedvalue );
	void ClearLocalStructAliases( void );
	void RegisterLocalStructAlias( const std::string &name );
	void ParseChspSignatureType( bool allow_extended_type, chspv3::ChspV3AstParam *param_ast = nullptr );

	void CheckInternalListenerCMD( int opt );
	int CheckInternalProgCMD( int opt, int orgcs );
	void CheckInternalIF( int opt );
	void CheckCMDIF_Set( int mode );
	void CheckCMDIF_Fin( int mode );

	int SetVarsFixed( const std::string &varname, int fixedvalue );

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

	void CG_MesLabelDefinition( int label_id );
	void ResetAstBuilder( void );
	void RecordSourceLine( const char *text );
	void BeginAstStatement( int statement_kind, int token_kind, const std::string &text );
	void CaptureAstExpr( std::unique_ptr<chspv3::ChspV3AstExpr> expr );
	std::unique_ptr<chspv3::ChspV3AstExpr> TakeCapturedExpression( void );

	//		Data
	//
	int texflag;

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

	chspv3::ChspV3AstProgram ast_program;
	chspv3::ChspV3AstModule *current_module;
	chspv3::ChspV3AstFunction *current_function;
	chspv3::ChspV3AstStmt *current_stmt;
	std::vector<std::unique_ptr<chspv3::ChspV3AstExpr>> expression_stack;

	//		for Error
	//
	int cg_errline;
};
