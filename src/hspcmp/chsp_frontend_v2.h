#pragma once

#include <memory>

class CMemBuf;

class CChspFrontendV2
{
public:
	explicit CChspFrontendV2( const std::shared_ptr<CMemBuf> &errbuf );
	int GenerateFromBuffer( const char *source_name, const char *input_text, CMemBuf *hsp_output, CMemBuf *cpp_output );

private:
	std::shared_ptr<CMemBuf> errbuf;
};
