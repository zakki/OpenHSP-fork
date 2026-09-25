#include "chsp_hsp_emitter.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "../hspcmp/membuf.h"
#include "chsp_types.h"

namespace chsp
{

std::string ModuleLibraryName( const std::string &file_stem )
{
#if defined( HSPWIN )
	return file_stem + ".dll";
#elif defined( HSPMAC )
	return file_stem + ".dylib";
#else
	return file_stem + ".so";
#endif
}

std::string DefaultModuleFileStem( const char *source_name, size_t module_index )
{
	std::string base = "chsp_module";
	if ( source_name != nullptr && source_name[0] != '\0' ) {
		std::filesystem::path source_path( source_name );
		if ( source_path.has_stem() ) {
			base = source_path.stem().string();
		}
	}
	return base + "_" + std::to_string( module_index + 1 );
}

std::string ModuleFileStem( const ChspAstModule &module, const char *source_name, size_t module_index )
{
	if ( !module.name.empty() ) {
		return module.name;
	}
	return DefaultModuleFileStem( source_name, module_index );
}

void WriteModuleHeaderToHsp( CMemBuf &buf, const std::string &module_tag, const std::string &file_stem,
							 ChspNativeTarget target )
{
	buf.PutStr( "goto@hsp *_" );
	buf.PutStr( module_tag.c_str() );
	buf.PutStr( "_exit" );
	buf.PutCR();
	buf.PutCR();

	if ( target == ChspNativeTarget::Plugin ) {
#if defined( HSPWIN )
		buf.PutStr( "#regcmd \"_hsp3cmdinit@4\", \"" );
#else
		buf.PutStr( "#regcmd \"hsp3cmdinit\", \"" );
#endif
		buf.PutStr( ModuleLibraryName( file_stem ).c_str() );
		buf.PutStr( "\"\n\n" );
	} else {
		buf.PutStr( "#uselib \"" );
		buf.PutStr( ModuleLibraryName( file_stem ).c_str() );
		buf.PutStr( "\"\n\n" );
	}
}

void WriteModuleFooterToHsp( CMemBuf &buf, const std::string &module_tag )
{
	buf.PutCR();
	buf.PutStr( "*_" );
	buf.PutStr( module_tag.c_str() );
	buf.PutStr( "_exit" );
	buf.PutCR();
}

void WriteOriginalSourceMarker( CMemBuf &buf, int line_number, const char *source_name )
{
	buf.PutStr( "##" );
	buf.PutStr( std::to_string( std::max( line_number, 0 ) ).c_str() );
	if ( source_name != nullptr && source_name[0] != '\0' ) {
		buf.PutStr( " \"" );
		buf.PutStr( source_name );
		buf.PutStr( "\"" );
	}
	buf.PutCR();
}

void WriteFunctionDeclToHsp( CMemBuf &buf, const ChspAstFunction &func, const std::string &cpp_name )
{
	buf.PutStr( IsDefCFunc( func ) ? "#cfunc " : "#func " );
	buf.PutStr( NormalizeScopedName( func.name ).c_str() );
	buf.PutStr( " \"" );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "\"" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToHspParamType( param ).c_str() );
	}
	buf.PutCR();
}

std::string HspInternalFunctionDeclName( const ChspAstFunction &func )
{
	return "chsp_native_wrap_" + SanitizeForCppIdentifier( NormalizeScopedName( func.name ) );
}

void WriteFunctionDeclToHspInternal( CMemBuf &buf, const ChspAstFunction &func, const std::string &name,
									 const std::string &cpp_name,
									 const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	buf.PutStr( IsDefCFunc( func ) ? "#cfunc " : "#func " );
	buf.PutStr( name.c_str() );
	buf.PutStr( " \"" );
	buf.PutStr( cpp_name.c_str() );
	buf.PutStr( "\"" );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToHspParamType( param ).c_str() );
		if ( needs_array_metadata && param.is_array ) {
			for ( int dim = 0; dim < 4; ++dim ) {
				buf.PutStr( ", int" );
			}
		}
	}
	buf.PutCR();
}

void WriteFunctionWrapperToHsp( CMemBuf &buf, const ChspAstFunction &func, const std::string &internal_name,
								const std::unordered_map<std::string, bool> &function_array_metadata_needs )
{
	const bool needs_array_metadata = FunctionNeedsArrayMetadata( function_array_metadata_needs, func );
	buf.PutStr( IsDefCFunc( func ) ? "#defcfunc " : "#deffunc " );
	buf.PutStr( NormalizeScopedName( func.name ).c_str() );
	bool first = true;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		buf.PutStr( first ? " " : ", " );
		first = false;
		buf.PutStr( ToPublicHspParamType( param ).c_str() );
		buf.PutStr( " " );
		buf.PutStr( NormalizeScopedName( param.name ).c_str() );
	}
	buf.PutCR();
	buf.PutStr( "    " );
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( "return@hsp " );
	}
	buf.PutStr( internal_name.c_str() );
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( "(" );
	}
	bool has_args = false;
	for ( const auto &param : func.params ) {
		if ( param.is_local ) {
			continue;
		}
		if ( IsDefCFunc( func ) ) {
			buf.PutStr( has_args ? ", " : "" );
		} else {
			buf.PutStr( has_args ? ", " : " " );
		}
		has_args = true;
		const auto public_name = NormalizeScopedName( param.name );
		buf.PutStr( public_name.c_str() );
		if ( needs_array_metadata && param.is_array ) {
			buf.PutStr( ", length@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length2@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length3@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( "), length4@hsp(" );
			buf.PutStr( public_name.c_str() );
			buf.PutStr( ")" );
		}
	}
	if ( IsDefCFunc( func ) ) {
		buf.PutStr( ")" );
	}
	buf.PutCR();
	if ( !IsDefCFunc( func ) ) {
		buf.PutStr( "    return@hsp" );
		buf.PutCR();
	}
}

void WritePluginFunctionDeclToHsp( CMemBuf &buf, const ChspAstFunction &func, int command_id )
{
	const std::string command_name = PluginCommandName( func );
	char idbuf[16];
	std::snprintf( idbuf, sizeof( idbuf ), "$%02x", command_id );
	buf.PutStr( "#cmd " );
	buf.PutStr( command_name.c_str() );
	buf.PutStr( " " );
	buf.PutStr( idbuf );
	buf.PutCR();
}

} // namespace chsp
