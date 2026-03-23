#include "chsp_frontend_v2.h"

#include <memory>
#include <vector>

#include "../membuf.h"
#include "chsp_frontend_v2_internal.h"
#include "logger.h"

CChspFrontendV2::CChspFrontendV2( const std::shared_ptr<CMemBuf> &errbuf_ ) : errbuf( errbuf_ )
{
}

int CChspFrontendV2::GenerateFromBuffer( const char *source_name, const char *input_text, CMemBuf *hsp_output,
										 CMemBuf *cpp_output, ChspNativeTarget target )
{
	auto logger = std::make_shared<CLogger>( errbuf );
	CLogger local_logger( errbuf );
	std::vector<chspv2::ChspSourceLine> indexed_lines;
	try {
		indexed_lines = chspv2::BuildSourceIndex( input_text, logger );
	} catch ( ... ) {
		local_logger.Mesf( "#Error:cHSP lexer/parser frontend v2 aborted during tokenization [%s]",
						   source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}

	chspv2::ChspProgram program;
	if ( !chspv2::ParseProgram( indexed_lines, program, local_logger, source_name ) ) {
		return -1;
	}
	if ( hsp_output == nullptr || cpp_output == nullptr ) {
		local_logger.Mesf( "#Error:Invalid cHSP output buffer [%s]",
						   source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}
	try {
		return chspv2::GenerateProgramOutput( indexed_lines, program, local_logger, *hsp_output, *cpp_output,
											  source_name, target );
	} catch ( ... ) {
		local_logger.Mesf( "#Error:cHSP lexer/parser frontend v2 aborted during output generation [%s]",
						   source_name != nullptr ? source_name : "<buffer>" );
		return -1;
	}
}
