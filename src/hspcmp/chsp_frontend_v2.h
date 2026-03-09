#pragma once

#include <memory>

class CMemBuf;

enum class ChspNativeTarget
{
	Cpp,
	C,
};

class CChspFrontendV2
{
public:
	explicit CChspFrontendV2( const std::shared_ptr<CMemBuf> &errbuf );
	int GenerateFromBuffer( const char *source_name, const char *input_text, CMemBuf *hsp_output, CMemBuf *native_output,
						   ChspNativeTarget target = ChspNativeTarget::Cpp );

private:
	std::shared_ptr<CMemBuf> errbuf;
};
