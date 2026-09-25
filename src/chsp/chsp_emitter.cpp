//
//      cHSP AST emitter surface
//
#include "chsp_emitter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../hspcmp/membuf.h"
#include "../hspcmp/membuf.h"
#include "chsp_c_emitter.h"
#include "chsp_hsp_emitter.h"
#include "chsp_types.h"
#include "logger.h"

namespace chsp
{

int GenerateProgramOutput( const ChspAstProgram &ast_program, CLogger &logger, CMemBuf &hsp_out,
						   std::vector<ChspNativeArtifact> &native_outputs, const char *source_name,
						   const ChspBuiltinMap &builtin_map, bool for_preprocessor )
{
	native_outputs.clear();
	native_outputs.reserve( ast_program.modules.size() );
	for ( size_t i = 0; i < ast_program.modules.size(); ++i ) {
		const auto &module = ast_program.modules[i];
		ChspNativeArtifact artifact;
		artifact.module_name = module.name;
		artifact.file_stem = ModuleFileStem( module, source_name, i );
		artifact.target = module.target;
		artifact.linked_libraries = module.linked_libraries;
		artifact.output = std::make_unique<CMemBuf>();
		WriteNativePreamble( *artifact.output, artifact.target );
		WriteNativeSourceBlocks( *artifact.output, module );
		native_outputs.push_back( std::move( artifact ) );
	}

	const auto function_cpp_names = BuildFunctionCppNames( ast_program );
	const auto function_defs = BuildFunctionDefs( ast_program );
	const auto function_array_metadata_needs = BuildFunctionArrayMetadataNeeds( ast_program, function_defs );

	for ( size_t i = 0; i < ast_program.modules.size(); ++i ) {
		auto &artifact = native_outputs[i];
		const auto &module = ast_program.modules[i];
		if ( artifact.target != ChspNativeTarget::C ) {
			continue;
		}
		for ( const auto &func : module.functions ) {
			WriteFunctionPrototypeToNative( *artifact.output, func, module, function_cpp_names, function_defs,
											function_array_metadata_needs, ChspNativeTarget::C, builtin_map );
		}
		if ( !module.functions.empty() ) {
			artifact.output->PutCR();
		}
	}

	size_t module_index = 0;
	size_t function_index = 0;
	bool in_function = false;
	std::string current_module_tag;
	int module_start_line = 0;
	int module_output_line_start = 0;

	auto count_newlines = []( CMemBuf &buf ) -> int {
		const char *p = buf.GetBuffer();
		int sz = buf.GetSize();
		int cnt = 0;
		for ( int j = 0; j < sz; ++j ) {
			if ( p[j] == '\n' ) cnt++;
		}
		return cnt;
	};

	for ( const auto &line : ast_program.source_lines ) {
		const auto trimmed = chsputil::Trim( line.text );
		switch ( line.directive ) {
		case ChspSourceDirectiveKind::None:
			if ( in_function ) {
				continue;
			}
			if ( chsputil::StartsWith( trimmed, "##chsp_" ) ) {
				continue;
			}
			if ( chsputil::StartsWith( trimmed, "#define " ) ) {
				for ( auto &artifact : native_outputs ) {
					artifact.output->PutStr( line.text.c_str() );
					artifact.output->PutCR();
				}
			}
			hsp_out.PutStr( line.text.c_str() );
			hsp_out.PutCR();
			break;
		case ChspSourceDirectiveKind::Module:
			if ( module_index >= ast_program.modules.size() ) {
				logger.Mesf( "#Error:Internal cHSP module index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			current_module_tag = "chsp_mod_" + std::to_string( module_index );
			module_start_line = line.line;
			module_output_line_start = count_newlines( hsp_out );
			if ( !for_preprocessor ) {
				WriteOriginalSourceMarker( hsp_out, line.line, source_name );
			}
			WriteModuleHeaderToHsp( hsp_out, current_module_tag, native_outputs[module_index].file_stem,
									native_outputs[module_index].target );
			in_function = false;
			function_index = 0;
			break;
		case ChspSourceDirectiveKind::ModuleEnd:
			if ( module_index >= native_outputs.size() ) {
				logger.Mesf( "#Error:Internal cHSP module output mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target == ChspNativeTarget::Plugin ) {
				if ( !WritePluginNativeDispatch( *native_outputs[module_index].output,
												 ast_program.modules[module_index], function_cpp_names, function_defs,
												 function_array_metadata_needs, logger, builtin_map ) ) {
					return -1;
				}
			}
			WriteModuleFooterToHsp( hsp_out, current_module_tag );
			if ( !for_preprocessor ) {
				WriteOriginalSourceMarker( hsp_out, line.line + 1, source_name );
			} else {
				int module_end_line = line.line;
				int module_output_line_end = count_newlines( hsp_out );
				int emitted = module_output_line_end - module_output_line_start;
				int expected = module_end_line - module_start_line + 1;
				for ( int pad = emitted; pad < expected; ++pad ) {
					hsp_out.PutCR();
				}
			}
			++module_index;
			in_function = false;
			break;
		case ChspSourceDirectiveKind::ChspC:
		case ChspSourceDirectiveKind::ChspCDecl:
		case ChspSourceDirectiveKind::ChspCLink:
			break;
		case ChspSourceDirectiveKind::DefFunc:
		case ChspSourceDirectiveKind::DefCFunc:
			in_function = true;
			if ( module_index >= ast_program.modules.size() ||
				 function_index >= ast_program.modules[module_index].functions.size() ) {
				logger.Mesf( "#Error:Internal cHSP function index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target == ChspNativeTarget::Plugin ) {
				WritePluginFunctionDeclToHsp( hsp_out, ast_program.modules[module_index].functions[function_index],
											  static_cast<int>( function_index ) );
			} else {
				const auto &func = ast_program.modules[module_index].functions[function_index];
				const auto cpp_name_it = function_cpp_names.find( func.name );
				const std::string cpp_name = cpp_name_it != function_cpp_names.end() ? cpp_name_it->second : func.name;
				if ( FunctionNeedsArrayMetadata( function_array_metadata_needs, func ) ) {
					const auto internal_name = HspInternalFunctionDeclName( func );
					WriteFunctionDeclToHspInternal( hsp_out, func, internal_name, cpp_name,
													function_array_metadata_needs );
					WriteFunctionWrapperToHsp( hsp_out, func, internal_name, function_array_metadata_needs );
				} else {
					WriteFunctionDeclToHsp( hsp_out, func, cpp_name );
				}
			}
			break;
		case ChspSourceDirectiveKind::End:
			if ( module_index >= ast_program.modules.size() ||
				 function_index >= ast_program.modules[module_index].functions.size() ) {
				logger.Mesf( "#Error:Internal cHSP end index mismatch [%s]",
							 source_name != nullptr ? source_name : "<buffer>" );
				return -1;
			}
			if ( native_outputs[module_index].target != ChspNativeTarget::Plugin ) {
				if ( !WriteFunctionToNative( *native_outputs[module_index].output, ast_program.modules[module_index],
											 ast_program.modules[module_index].functions[function_index],
											 function_cpp_names, function_defs, function_array_metadata_needs,
											 native_outputs[module_index].target, logger, builtin_map ) ) {
					return -1;
				}
			}
			++function_index;
			in_function = false;
			break;
		}
	}

	return 0;
}

} // namespace chsp
