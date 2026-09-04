#include "chsp_pipeline.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../../hsp3/hsp3config.h"
#include "../membuf.h"
#include "chsp_frontend_v2.h"
#include "chsp_libtcc_shared.h"
#include "chsp_util.h"

bool ChspPipeline::ContainsChspDirective( const char *text )
{
	if ( text == nullptr ) return false;
	return strstr( text, "#chsp_" ) != nullptr;
}

bool ChspPipeline::ContainsChspInFile( const std::string &filepath )
{
	std::ifstream ifs( filepath );
	if ( !ifs.is_open() ) return false;
	std::string line;
	while ( std::getline( ifs, line ) ) {
		if ( ContainsChspDirective( line.c_str() ) ) {
			return true;
		}
	}
	return false;
}

ChspPipelineResult ChspPipeline::Process( const ChspPipelineOptions &options )
{
	ChspPipelineResult result;
	std::ifstream ifs( options.source_path );
	if ( !ifs.is_open() ) {
		result.error_message = "#Can't open source file: " + options.source_path;
		return result;
	}
	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string source_text = ss.str();

	if ( !ContainsChspDirective( source_text.c_str() ) ) {
		result.has_chsp = false;
		result.success = true;
		return result;
	}
	result.has_chsp = true;

	auto errbuf = std::make_shared<CMemBuf>();
	CChspFrontendV2 frontend( errbuf );
	CMemBuf transformed_hsp;
	std::vector<ChspNativeArtifact> native_outputs;

	int res = frontend.GenerateFromBuffer( options.source_path.c_str(), source_text.c_str(),
										   &transformed_hsp, &native_outputs, options.common_path.c_str(),
										   true /* for_preprocessor */ );
	if ( res != 0 ) {
		result.success = false;
		result.error_message = errbuf->GetBuffer() != nullptr ? errbuf->GetBuffer() : "#cHSP frontend failed.";
		return result;
	}

	std::filesystem::path source_path( options.source_path );
	std::filesystem::path native_dir =
		source_path.has_parent_path() ? source_path.parent_path() : std::filesystem::path( "." );

	std::vector<std::string> native_c_files;
	native_c_files.reserve( native_outputs.size() );
	for ( const auto &artifact : native_outputs ) {
		std::filesystem::path c_path = native_dir / ( artifact.file_stem + ".c" );
		std::string c_path_str = c_path.string();
		native_c_files.push_back( c_path_str );
		result.generated_c_files.push_back( c_path_str );

		if ( artifact.output == nullptr || artifact.output->SaveFile( const_cast<char *>( c_path_str.c_str() ) ) < 0 ) {
			result.success = false;
			result.error_message = "#Can't write generated cHSP native C file: " + c_path_str;
			return result;
		}

#if defined( HSPWIN )
		std::string so_name = artifact.file_stem + ".dll";
#elif defined( HSPMAC )
		std::string so_name = artifact.file_stem + ".dylib";
#else
		std::string so_name = artifact.file_stem + ".so";
#endif
		result.generated_so_files.push_back( ( native_dir / so_name ).string() );
	}

	if ( options.compile_mode == ChspPipelineCompileMode::Libtcc ) {
#ifdef CHSP_HAS_LIBTCC
		res = chsp_compile_library_with_libtcc( errbuf.get(), native_c_files, options.common_path.c_str(), native_outputs );
		if ( res != 0 ) {
			result.success = false;
			result.error_message = errbuf->GetBuffer() != nullptr ? errbuf->GetBuffer() : "#libtcc compile failed.";
			return result;
		}
#endif
	}

	std::string stem = source_path.stem().string();
	std::filesystem::path tmp_hsp_path = native_dir / ( stem + ".chsp.tmp.hsp" );
	result.intermediate_hsp_path = tmp_hsp_path.string();

	if ( transformed_hsp.SaveFile( const_cast<char *>( result.intermediate_hsp_path.c_str() ) ) < 0 ) {
		result.success = false;
		result.error_message = "#Can't write intermediate HSP file: " + result.intermediate_hsp_path;
		return result;
	}

	result.success = true;
	return result;
}

void ChspPipeline::CleanupIntermediate( const ChspPipelineResult &result )
{
	if ( !result.intermediate_hsp_path.empty() ) {
		std::filesystem::remove( result.intermediate_hsp_path );
	}
}
