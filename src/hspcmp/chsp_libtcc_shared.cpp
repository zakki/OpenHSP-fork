#ifdef HSPWIN
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chsp_libtcc_shared.h"
#include "hsc3.h"

#ifdef CHSP_HAS_LIBTCC
#include <libtcc.h>

static void chsp_tcc_error_handler( void *opaque, const char *msg )
{
	CHsc3 *hsc3 = reinterpret_cast<CHsc3 *>( opaque );
	if ( hsc3 != NULL && msg != NULL ) {
		hsc3->Print( (char *)msg );
	}
}

static int path_is_sep( char ch )
{
	return ch == '/' || ch == '\\';
}

static int path_dirname( const char *path, char *out_dir, int out_dir_size )
{
	const char *slash = NULL;
	for ( const char *p = path; *p != 0; ++p ) {
		if ( path_is_sep( *p ) ) slash = p;
	}
	if ( slash == NULL ) {
		if ( out_dir_size < 2 ) return -1;
		strcpy( out_dir, "." );
		return 0;
	}
	int len = static_cast<int>( slash - path );
	if ( len <= 0 ) len = 1;
	if ( len + 1 > out_dir_size ) return -1;
	memcpy( out_dir, path, len );
	out_dir[len] = 0;
	return 0;
}

static int path_join( const char *dir, const char *name, char *out_path, int out_path_size )
{
	if ( dir == NULL || name == NULL ) return -1;
	if ( strcmp( dir, "." ) == 0 ) {
		if ( static_cast<int>( strlen( name ) ) + 1 > out_path_size ) return -1;
		strcpy( out_path, name );
		return 0;
	}
	const int need_sep = dir[0] != 0 && !path_is_sep( dir[strlen( dir ) - 1] );
	const int len = static_cast<int>( strlen( dir ) + strlen( name ) + ( need_sep ? 2 : 1 ) );
	if ( len > out_path_size ) return -1;
	strcpy( out_path, dir );
	if ( need_sep ) {
#ifdef HSPWIN
		strcat( out_path, "\\" );
#else
		strcat( out_path, "/" );
#endif
	}
	strcat( out_path, name );
	return 0;
}

static int derive_repo_root_from_compath( const char *compath, char *out_root, int out_root_size )
{
	if ( compath == NULL ) return -1;
	if ( strcmp( compath, "common\\" ) == 0 || strcmp( compath, "common/" ) == 0 || strcmp( compath, "common" ) == 0 ) {
		if ( out_root_size < 2 ) return -1;
		strcpy( out_root, "." );
		return 0;
	}
	char tmp[HSP_MAX_PATH];
	strcpy( tmp, compath );
	size_t len = strlen( tmp );
	while ( len > 0 && path_is_sep( tmp[len - 1] ) ) {
		tmp[len - 1] = 0;
		--len;
	}
	char *slash = NULL;
	for ( char *p = tmp; *p != 0; ++p ) {
		if ( path_is_sep( *p ) ) slash = p;
	}
	if ( slash == NULL ) {
		if ( out_root_size < 2 ) return -1;
		strcpy( out_root, "." );
		return 0;
	}
	*slash = 0;
	if ( tmp[0] == 0 ) {
		if ( out_root_size < 2 ) return -1;
		strcpy( out_root, "." );
		return 0;
	}
	if ( static_cast<int>( strlen( tmp ) ) + 1 > out_root_size ) return -1;
	strcpy( out_root, tmp );
	return 0;
}

static int resolve_libtcc_runtime_dir( char *out_dir, int out_dir_size )
{
	const char *env_dir = getenv( "LIBTCC_DIR" );
	if ( env_dir != NULL && env_dir[0] != 0 ) {
		if ( static_cast<int>( strlen( env_dir ) ) + 1 > out_dir_size ) return -1;
		strcpy( out_dir, env_dir );
		return 0;
	}
#ifdef HSPWIN
	char exe_path[HSP_MAX_PATH];
	const DWORD got = GetModuleFileNameA( NULL, exe_path, HSP_MAX_PATH );
	if ( got == 0 || got >= HSP_MAX_PATH ) return -1;
	if ( path_dirname( exe_path, out_dir, out_dir_size ) != 0 ) return -1;
	char bundled_dir[HSP_MAX_PATH];
	if ( path_join( out_dir, "tcc", bundled_dir, HSP_MAX_PATH ) != 0 ) return -1;
	if ( static_cast<int>( strlen( bundled_dir ) ) + 1 > out_dir_size ) return -1;
	strcpy( out_dir, bundled_dir );
	return 0;
#else
	return -1;
#endif
}

static int copy_file_binary( const char *src, const char *dst )
{
	FILE *fp_src = fopen( src, "rb" );
	if ( fp_src == NULL ) return -1;
	FILE *fp_dst = fopen( dst, "wb" );
	if ( fp_dst == NULL ) {
		fclose( fp_src );
		return -1;
	}
	char buf[8192];
	int st = 0;
	while ( 1 ) {
		const size_t read_sz = fread( buf, 1, sizeof( buf ), fp_src );
		if ( read_sz > 0 ) {
			if ( fwrite( buf, 1, read_sz, fp_dst ) != read_sz ) {
				st = -1;
				break;
			}
		}
		if ( read_sz < sizeof( buf ) ) {
			if ( ferror( fp_src ) ) st = -1;
			break;
		}
	}
	fclose( fp_dst );
	fclose( fp_src );
	return st;
}

static int collect_chsp_library_names( CMemBuf &hsp_out, char names[][HSP_MAX_PATH], int max_names )
{
	if ( max_names <= 0 ) return 0;
	const char *buf = hsp_out.GetBuffer();
	if ( buf == NULL ) return 0;
	int count = 0;
	const char *cursor = buf;
	while ( ( cursor = strstr( cursor, "#uselib \"" ) ) != NULL ) {
		cursor += 9;
		const char *end = strchr( cursor, '"' );
		if ( end == NULL ) break;
		const int len = static_cast<int>( end - cursor );
		if ( len > 0 && len < HSP_MAX_PATH ) {
			char candidate[HSP_MAX_PATH];
			memcpy( candidate, cursor, len );
			candidate[len] = 0;
			int exists = 0;
			for ( int i = 0; i < count; ++i ) {
				if ( strcmp( names[i], candidate ) == 0 ) {
					exists = 1;
					break;
				}
			}
			if ( !exists && count < max_names ) {
				strcpy( names[count], candidate );
				++count;
			}
		}
		cursor = end + 1;
	}
	return count;
}

int chsp_compile_library_with_libtcc( CHsc3 *hsc3, const char *native_file, const char *compath, CMemBuf &hsp_out )
{
	char library_names[32][HSP_MAX_PATH];
	const int library_count = collect_chsp_library_names( hsp_out, library_names, 32 );
	if ( library_count <= 0 ) {
		hsc3->Print( (char *)"#No cHSP library names found for libtcc compilation." );
		return -1;
	}

	char native_dir[HSP_MAX_PATH];
	if ( path_dirname( native_file, native_dir, HSP_MAX_PATH ) != 0 ) {
		hsc3->Print( (char *)"#Failed to resolve cHSP native output directory." );
		return -1;
	}

	char repo_root[HSP_MAX_PATH];
	if ( derive_repo_root_from_compath( compath, repo_root, HSP_MAX_PATH ) != 0 ) {
		hsc3->Print( (char *)"#Failed to resolve repository root for libtcc include path." );
		return -1;
	}

	char primary_output[HSP_MAX_PATH];
	if ( path_join( native_dir, library_names[0], primary_output, HSP_MAX_PATH ) != 0 ) {
		hsc3->Print( (char *)"#Failed to resolve cHSP shared library output path." );
		return -1;
	}

	TCCState *tcc = tcc_new();
	if ( tcc == NULL ) {
		hsc3->Print( (char *)"#libtcc initialization failed." );
		return -1;
	}

	tcc_set_error_func( tcc, hsc3, chsp_tcc_error_handler );
	char tcc_runtime_dir[HSP_MAX_PATH];
	if ( resolve_libtcc_runtime_dir( tcc_runtime_dir, HSP_MAX_PATH ) == 0 ) {
		tcc_set_lib_path( tcc, tcc_runtime_dir );
		tcc_add_library_path( tcc, tcc_runtime_dir );
	}
	if ( tcc_set_output_type( tcc, TCC_OUTPUT_DLL ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to configure DLL output." );
		return -1;
	}
	if ( tcc_add_include_path( tcc, repo_root ) < 0 ) {
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
	if ( tcc_add_file( tcc, native_file ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to compile generated cHSP C source." );
		return -1;
	}
	if ( tcc_output_file( tcc, primary_output ) < 0 ) {
		tcc_delete( tcc );
		hsc3->Print( (char *)"#libtcc failed to write cHSP shared library." );
		return -1;
	}
	tcc_delete( tcc );

	for ( int i = 1; i < library_count; ++i ) {
		char out_path[HSP_MAX_PATH];
		if ( path_join( native_dir, library_names[i], out_path, HSP_MAX_PATH ) != 0 ) {
			hsc3->Print( (char *)"#Failed to resolve secondary cHSP shared library path." );
			return -1;
		}
		if ( copy_file_binary( primary_output, out_path ) != 0 ) {
			hsc3->Print( (char *)"#Failed to duplicate cHSP shared library for additional module." );
			return -1;
		}
	}
	return 0;
}

#endif
