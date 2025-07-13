//
//	codegen_writer.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "codegen_lexer.h"
#include "lexer_util.h"
#include "logger.h"
#include "token_def.h"


class CMemBuf;


class CCodeWriter : public CCompilerUtil
{
public:
	CCodeWriter( std::shared_ptr<CompileOptions> compopt, std::shared_ptr<CLogger> log,
				 std::shared_ptr<CSymbolTable> symtab );
	~CCodeWriter() override;

	int Write( const char *oname, int mode, int cg_valcnt, int cg_varhpi, int cg_putvars );
	int SaveStringMap( const char *fname );

	void PutCS( int type, int value, int exflg );
	void PutCSSymbol( int label_id, int exflag );
	int GetCS( void );
	void PutCS( int type, double value, int exflg );
	int PutOT( int value );
	int PutDS( double value );
	int PutDS( char *str );
	int PutDSStr( char *str, bool converts_to_utf8 );
	int PutDSBuf( char *str );
	int PutDSBuf( char *str, int size );
	char *GetDS( int ptr );
	void SetOT( int id, int value );
	void PutDI( void );
	void PutDI( int dbg_code, int a, int subid );
	void PutDIVars( void );
	void PutDILabels( void );
	void PutDIParams( void );
	void PutHPI( short flag, short option, char *libname, char *funcname );
	int PutLIB( int flag, char *name );
	void SetLIBIID( int id, char *clsid );
	int PutStructParam( short mptype, int extype );
	int PutStructParamTag( void );
	void PutStructStart( void );
	int PutStructEnd( char *name, int libindex, int otindex, int funcflag );
	int PutStructEnd( int i, char *name, int libindex, int otindex, int funcflag );
	int PutStructEndDll( char *name, int libindex, int subid, int otindex );

private:
	bool CG_optCode() const
	{
		return ( compopt->hed_cmpmode & CMPMODE_OPTCODE ) != 0;
	}
	bool CG_optInfo() const
	{
		return ( compopt->hed_cmpmode & CMPMODE_OPTINFO ) != 0;
	}

	std::shared_ptr<CLogger> logger;
	std::shared_ptr<CSymbolTable> symtab;

public:
	std::unique_ptr<CMemBuf> cs_buf;
	std::unique_ptr<CMemBuf> ds_buf;
	std::unique_ptr<CMemBuf> ot_buf;
	std::unique_ptr<CMemBuf> di_buf;

	std::unique_ptr<CMemBuf> li_buf;
	std::unique_ptr<CMemBuf> fi_buf;
	std::unique_ptr<CMemBuf> mi_buf;
	std::unique_ptr<CMemBuf> fi2_buf;
	std::unique_ptr<CMemBuf> hpi_buf;

	int cg_lastcs;

	//		for Struct
	int cg_stnum;
	int cg_stsize;
	int cg_stptr;
};
