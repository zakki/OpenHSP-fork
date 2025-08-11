//
//	codegen.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../lexer_util.h"
#include "../logger.h"
#include "../token_def.h"

#include "../codegen_lexer.h"
#include "../codegen_writer.h"

#include "ast.h"
#include "ast_serializer.h"

class CMemBuf;


//  token analysis class
class CHspParser : public CCompilerUtil
{
public:
	explicit CHspParser( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log );
	~CHspParser() override;

	void ResetCompiler( void );
	void SetLabelListBuffer( CMemBuf *buf, int mode, char *match, int line = 0, char *filename = nullptr );

	void SetLabelInfo( std::unique_ptr<CLabel> lbinfo )
	{
		symtab->SetLabelInfo( std::move( lbinfo ) );
	}
	int LabelRegist( char **list, int mode )
	{
		return symtab->LabelRegist( list, mode );
	}

	//		For Code Generate
	//
	int GenerateCode( const std::string &fname, const std::string &oname, int mode );
	int GenerateCode( CMemBuf *srcbuf, const std::string &oname, int mode );

private:
	std::unique_ptr<ExpressionNode> CalcCG( int ex );

	std::shared_ptr<CLogger> logger;
	CCgLexer lexer;
	CCgToken token;
	std::unique_ptr<CCodeWriter> writer;
	std::shared_ptr<CSymbolTable> symtab;
	AstSerializer serializer;

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
	std::unique_ptr<StatementNode> GenerateCodeLET( int id, bool first = false );
	[[nodiscard]] std::unique_ptr<ExpressionNode> GenerateCodeVAR( int id, int ex );
	[[nodiscard]] std::vector<std::unique_ptr<ExpressionNode>> GenerateCodePRM( void );
	[[nodiscard]] std::unique_ptr<ParameterList> GenerateCodePRMF( void );
	[[nodiscard]] std::unique_ptr<ParameterList> GenerateCodePRMF2( void );
	[[nodiscard]] std::unique_ptr<ParameterList> GenerateCodePRMF3( void );
	[[nodiscard]] std::unique_ptr<VariableNode> GenerateCodePRMF4( int t, std::unique_ptr<LiteralSymbolNode> node );
	[[nodiscard]] std::unique_ptr<ParameterList> GenerateCodeMethod( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> GenerateCodeLabel( const std::string &name, int ex );

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

	void GenerateLabelTag( const std::string &name, int flag, int type, const std::string &fname, int line );
	void GenerateLabelListAndTag( int labelid, int flag = 0 );
	void GenerateLabelListAndTag( const std::string &name, int flag = 0 );
	void GenerateLabelListAndTagRef( int labelid, int flag = 0 );

	void CheckInternalListenerCMD( int opt );
	int CheckInternalProgCMD( int opt, int orgcs );
	void CheckInternalIF( int opt );
	void CheckCMDIF_Set( int mode );
	void CheckCMDIF_Fin( int mode );

	int SetVarsFixed( const std::string &varname, int fixedvalue );

	void CalcCG_token( void );
	void CalcCG_token_exprbeg( void );
	void CalcCG_token_exprbeg_redo( void );
	[[nodiscard]] OperatorType CalcCG_regmark( int mark );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_factor( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_unary( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_muldiv( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_addsub( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_shift( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_bool( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_compare( void );
	[[nodiscard]] std::unique_ptr<ExpressionNode> CalcCG_start( void );

	void CG_MesLabelDefinition( int label_id );

	// int SaveStringMap( char *fname );

	char *GetLabelListHeader( int flag );

	//		Data
	//
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
};
