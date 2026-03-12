#ifdef HSPWIN
#include <windows.h>
#endif

#include <filesystem>
#include <stdlib.h>
#include <string>
#include <vector>

#include "../hsc3.h"
#include "chsp_libtcc_shared.h"

#ifdef CHSP_HAS_LIBTCC
#include <libtcc.h>

static void chsp_tcc_error_handler( void *opaque, const char *msg )
{
	auto *hsc3 = reinterpret_cast<CHsc3 *>( opaque );
	if ( hsc3 != nullptr && msg != nullptr ) {
		hsc3->Print( (char *)msg );
	}
}

static bool derive_repo_root_from_compath( const char *compath, std::filesystem::path &out_root )
{
	if ( compath == nullptr ) {
		return false;
	}
	std::string compath_str( compath );
	if ( compath_str == "common\\" || compath_str == "common/" || compath_str == "common" ) {
		out_root = ".";
		return true;
	}
	while ( !compath_str.empty() && ( compath_str.back() == '/' || compath_str.back() == '\\' ) ) {
		compath_str.pop_back();
	}
	if ( compath_str.empty() ) {
		out_root = ".";
		return true;
	}
	std::filesystem::path compath_path( compath_str );
	if ( !compath_path.has_parent_path() ) {
		out_root = ".";
		return true;
	}
	out_root = compath_path.parent_path();
	if ( out_root.empty() ) {
		out_root = ".";
	}
	return true;
}

static bool resolve_libtcc_runtime_dir( std::filesystem::path &out_dir )
{
	const char *env_dir = getenv( "LIBTCC_DIR" );
	if ( env_dir != nullptr && env_dir[0] != 0 ) {
		out_dir = env_dir;
		return true;
	}
#ifdef HSPWIN
	char exe_path[HSP_MAX_PATH];
	const DWORD got = GetModuleFileNameA( NULL, exe_path, HSP_MAX_PATH );
	if ( got == 0 || got >= HSP_MAX_PATH )
		return false;
	out_dir = std::filesystem::path( exe_path ).parent_path() / "tcc";
	return true;
#else
	return false;
#endif
}

static std::string shared_library_name_for_artifact( const ChspNativeArtifact &artifact )
{
#if defined( HSPWIN )
	return artifact.file_stem + ".dll";
#elif defined( HSPMAC )
	return artifact.file_stem + ".dylib";
#else
	return artifact.file_stem + ".so";
#endif
}

static int compile_one_library_with_libtcc( CHsc3 *hsc3, const std::filesystem::path &native_path,
											const std::filesystem::path &repo_root,
											const std::filesystem::path &output_path )
{
	TCCState *tcc = tcc_new();
	if ( tcc == nullptr ) {
		hsc3->Print( (char *)"#libtcc initialization failed." );
		return -1;
	}

	tcc_set_error_func( tcc, hsc3, chsp_tcc_error_handler );
	std::filesystem::path tcc_runtime_dir;
	if ( resolve_libtcc_runtime_dir( tcc_runtime_dir ) ) {
		const std::string tcc_runtime_dir_str = tcc_runtime_dir.string();
		tcc_set_lib_path( tcc, tcc_runtime_dir_str.c_str() );
		tcc_add_library_path( tcc, tcc_runtime_dir_str.c_str() );
	}
	if ( tcc_set_output_type( tcc, TCC_OUTPUT_DLL ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to configure DLL output." );
		return -1;
	}
	const std::string repo_root_str = repo_root.string();
	if ( tcc_add_include_path( tcc, repo_root_str.c_str() ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to add repository include path." );
		return -1;
	}
#ifdef HSPLINUX
	if ( tcc_add_library( tcc, "m" ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to link libm." );
		return -1;
	}
#endif
	const std::string native_path_str = native_path.string();
	if ( tcc_add_file( tcc, native_path_str.c_str() ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to compile generated cHSP C source." );
		return -1;
	}
	const std::string output_path_str = output_path.string();
	if ( tcc_output_file( tcc, output_path_str.c_str() ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to write cHSP shared library." );
		return -1;
	}
	tcc_delete( tcc );
	return 0;
}

int chsp_compile_library_with_libtcc( CHsc3 *hsc3, const std::vector<std::string> &native_files, const char *compath,
									  const std::vector<ChspNativeArtifact> &native_artifacts )
{
	if ( native_files.empty() || native_files.size() != native_artifacts.size() ) {
		hsc3->Print( (char *)"#Invalid cHSP native output list for libtcc compilation." );
		return -1;
	}

	std::filesystem::path repo_root;
	if ( !derive_repo_root_from_compath( compath, repo_root ) ) {
		hsc3->Print( (char *)"#Failed to resolve repository root for libtcc include path." );
		return -1;
	}

	for ( size_t i = 0; i < native_files.size(); ++i ) {
		const std::filesystem::path native_path( native_files[i] );
		const std::filesystem::path native_dir =
			native_path.has_parent_path() ? native_path.parent_path() : std::filesystem::path( "." );
		const std::filesystem::path output_path = native_dir / shared_library_name_for_artifact( native_artifacts[i] );
		if ( compile_one_library_with_libtcc( hsc3, native_path, repo_root, output_path ) != 0 ) {
			return -1;
		}
	}
	return 0;
}

#endif
