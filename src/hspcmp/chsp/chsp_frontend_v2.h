#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../membuf.h"

class CMemBuf;

enum class ChspNativeTarget
{
	C,
	Plugin,
};

struct ChspNativeArtifact
{
	std::string module_name;
	std::string file_stem;
	ChspNativeTarget target = ChspNativeTarget::Plugin;
	std::unique_ptr<CMemBuf> output;
};

class CChspFrontendV2
{
public:
	explicit CChspFrontendV2( const std::shared_ptr<CMemBuf> &errbuf );
	int GenerateFromBuffer( const char *source_name, const char *input_text, CMemBuf *hsp_output,
							std::vector<ChspNativeArtifact> *native_outputs );

private:
	std::shared_ptr<CMemBuf> errbuf;
};
