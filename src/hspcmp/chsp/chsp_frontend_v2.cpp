//
//		cHSP frontend
//
#include <cstdlib>
#include <string.h>

#include "../../hsp3/hsp3config.h"
#include "chsp_frontend_v2.h"

#include <memory>
#include <vector>

#include "../membuf.h"
#include "chsp_builtin_map.h"
#include "chsp_util.h"
#include "chsp_frontend_v3_emitter.h"
#include "chsp_frontend_v3_parser.h"
#include "logger.h"


extern char *hsp_prestr[];
extern char *hsp_prepp[];

CChspFrontendV2::CChspFrontendV2( const std::shared_ptr<CMemBuf> &errbuf_ ) : errbuf( errbuf_ )
{
}

int CChspFrontendV2::GenerateFromBuffer( const char *source_name, const char *input_text, CLabel *lb_info,
										 CMemBuf *hsp_output, std::vector<ChspNativeArtifact> *native_outputs,
										 const char *compath )
{
	auto logger = std::make_shared<CLogger>( errbuf );
	CLogger local_logger( errbuf );

	if ( source_name == nullptr ) {
		local_logger.Mes( "#Error:Invalid cHSP source name." );
		return -1;
	}
	if ( hsp_output == nullptr || native_outputs == nullptr ) {
		local_logger.Mesf( "#Error:Invalid cHSP output buffer." );
		return -1;
	}

	int genmode = 0;
	char oname_debug[HSP_MAX_PATH] = {};
	strcpy( oname_debug, source_name );
	cutext( oname_debug );
	addext( oname_debug, "log" );
	auto opts = std::make_shared<CompileOptions>();
	CChspParser parser( opts, logger );
	if ( lb_info != NULL )
		parser.SetLabelInfo( lb_info );
	parser.LabelRegist( hsp_prestr, 1 );

	const bool chsp_dev_debug = ( getenv( "CHSP_DEBUG" ) != nullptr );
	CMemBuf srcbuf;
	srcbuf.PutStr( input_text != NULL ? input_text : "" );
	if ( chsp_dev_debug ) {
		printf( "#cHSP input:\n\n%s", srcbuf.GetBuffer() );
		printf( "#cHSP parser %s\n", oname_debug );
	}
	int res = parser.GenerateCode( &srcbuf, oname_debug, genmode );
	if ( chsp_dev_debug ) {
		printf( "#cHSP AST JSON:\n%s\n", chspv3::SerializeAstProgramJson( parser.GetAstProgram() ).c_str() );
		printf( "#cHSP parser log:\n%s", errbuf->GetBuffer() );
		printf( "#cHSP parser end\n\n" );
	}

	if ( res != 0 ) {
		return res;
	}

	ChspBuiltinMap builtin_map;
	const std::string load_err = LoadChspBuiltinMap( compath, builtin_map );
	if ( !load_err.empty() ) {
		local_logger.Mes( load_err.c_str() );
		return -1;
	}

	try {
		return chspv3::GenerateProgramOutput( parser.GetAstProgram(), local_logger, *hsp_output, *native_outputs,
											  source_name, builtin_map );
	} catch ( ... ) {
		local_logger.Mesf( "#Error:cHSP lexer/parser frontend v2 aborted during output generation [%s]",
						   source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}
}
