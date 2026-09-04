#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "../hspcmp/chsp/chsp_pipeline.h"

static void usage()
{
	printf( "OpenHSP cHSP Compiler Frontend ver 1.0\n" );
	printf( "Usage: chsp [options] <source.chsp|source.hsp>\n" );
	printf( "Options:\n" );
	printf( "  -o<file>           Set output file (.ax)\n" );
	printf( "  -d                 Add debug information\n" );
	printf( "  -c                 HSP2.55 compatible mode\n" );
	printf( "  -i                 Input UTF-8 source code\n" );
	printf( "  -u                 Output UTF-8 strings\n" );
	printf( "  --compath=<path>   Set common directory path\n" );
	printf( "  --chsp-compile=libtcc|none (default: libtcc)\n" );
	printf( "  --keep-tmp         Keep intermediate .tmp.hsp file\n" );
	printf( "  --hspcmp=<path>    Path to upstream hspcmp binary\n" );
}

static std::string find_hspcmp_executable( const char *specified_path, const char *argv0 )
{
	if ( specified_path != nullptr && specified_path[0] != '\0' ) {
		return std::filesystem::absolute( specified_path ).string();
	}
	const char *env_orig = getenv( "HSPCMP_ORIGINAL" );
	if ( env_orig != nullptr && env_orig[0] != '\0' ) {
		return std::filesystem::absolute( env_orig ).string();
	}
	const char *env_upstream = getenv( "HSPCMP_UPSTREAM" );
	if ( env_upstream != nullptr && env_upstream[0] != '\0' ) {
		return std::filesystem::absolute( env_upstream ).string();
	}

	// argv[0] のあるディレクトリを探索
	if ( argv0 != nullptr && argv0[0] != '\0' ) {
		std::filesystem::path bin_dir = std::filesystem::absolute( argv0 ).parent_path();
		std::filesystem::path candidate = bin_dir / "hspcmp";
		if ( std::filesystem::exists( candidate ) ) {
			return candidate.string();
		}
#ifdef _WIN32
		candidate = bin_dir / "hspcmp.exe";
		if ( std::filesystem::exists( candidate ) ) {
			return candidate.string();
		}
#endif
	}

	if ( std::filesystem::exists( "./hspcmp" ) ) {
		return std::filesystem::absolute( "./hspcmp" ).string();
	}
	return "hspcmp";
}

int main( int argc, char *argv[] )
{
	if ( argc < 2 ) {
		usage();
		return -1;
	}

	std::string source_file;
	std::string output_ax;
	std::string compath = "common/";
	std::string hspcmp_bin;
	ChspPipelineCompileMode compile_mode = ChspPipelineCompileMode::Libtcc;
	bool debug_mode = false;
	bool keep_tmp = false;
	std::vector<std::string> passthrough_args;

	for ( int i = 1; i < argc; ++i ) {
		const char *arg = argv[i];
		if ( arg[0] == '-' ) {
			if ( strncmp( arg, "--compath=", 10 ) == 0 ) {
				compath = arg + 10;
				passthrough_args.push_back( arg );
				continue;
			}
			if ( strncmp( arg, "--chsp-compile=", 15 ) == 0 ) {
				const char *val = arg + 15;
				if ( strcmp( val, "none" ) == 0 ) {
					compile_mode = ChspPipelineCompileMode::None;
				} else if ( strcmp( val, "libtcc" ) == 0 ) {
					compile_mode = ChspPipelineCompileMode::Libtcc;
				}
				continue;
			}
			if ( strncmp( arg, "--hspcmp=", 9 ) == 0 ) {
				hspcmp_bin = arg + 9;
				continue;
			}
			if ( strcmp( arg, "--keep-tmp" ) == 0 ) {
				keep_tmp = true;
				continue;
			}
			if ( arg[1] == 'o' ) {
				output_ax = arg + 2;
				continue;
			}
			if ( arg[1] == 'd' ) {
				debug_mode = true;
			}
			passthrough_args.push_back( arg );
		} else {
			if ( source_file.empty() ) {
				source_file = arg;
			} else {
				passthrough_args.push_back( arg );
			}
		}
	}

	if ( source_file.empty() ) {
		std::cerr << "No input file specified." << std::endl;
		return 1;
	}

	// 拡張子補完
	if ( !std::filesystem::exists( source_file ) ) {
		if ( std::filesystem::exists( source_file + ".chsp" ) ) {
			source_file += ".chsp";
		} else if ( std::filesystem::exists( source_file + ".hsp" ) ) {
			source_file += ".hsp";
		}
	}

	if ( output_ax.empty() ) {
		std::filesystem::path sp( source_file );
		output_ax = ( sp.parent_path() / ( sp.stem().string() + ".ax" ) ).string();
	}

	std::string resolved_hspcmp = find_hspcmp_executable( hspcmp_bin.c_str(), argv[0] );

	ChspPipelineOptions opts;
	opts.source_path = source_file;
	opts.output_ax_path = output_ax;
	opts.common_path = compath;
	opts.compile_mode = compile_mode;
	opts.debug_mode = debug_mode;
	opts.keep_intermediate = keep_tmp;

	ChspPipelineResult pipe_res = ChspPipeline::Process( opts );
	if ( !pipe_res.success ) {
		std::cerr << pipe_res.error_message << std::endl;
		return 1;
	}

	std::string input_for_hspcmp = source_file;
	if ( pipe_res.has_chsp ) {
		input_for_hspcmp = pipe_res.intermediate_hsp_path;
	}

	// hspcmp 実行コマンドの構築
	std::vector<std::string> cmd_args;
	cmd_args.push_back( resolved_hspcmp );
	cmd_args.push_back( "-o" + output_ax );
	for ( const auto &opt : passthrough_args ) {
		cmd_args.push_back( opt );
	}
	cmd_args.push_back( input_for_hspcmp );

	std::vector<char *> exec_argv;
	for ( auto &s : cmd_args ) {
		exec_argv.push_back( const_cast<char *>( s.c_str() ) );
	}
	exec_argv.push_back( nullptr );

#if defined(__unix__) || defined(__APPLE__)
	pid_t pid = fork();
	if ( pid < 0 ) {
		std::cerr << "fork() failed to execute " << resolved_hspcmp << std::endl;
		if ( pipe_res.has_chsp && !keep_tmp ) ChspPipeline::CleanupIntermediate( pipe_res );
		return 1;
	}
	if ( pid == 0 ) {
		execvp( resolved_hspcmp.c_str(), exec_argv.data() );
		std::cerr << "Failed to exec " << resolved_hspcmp << std::endl;
		exit( 127 );
	}
	int status = 0;
	waitpid( pid, &status, 0 );
	int exit_code = WIFEXITED( status ) ? WEXITSTATUS( status ) : 1;
#else
	// Windows fallback (system / spawn)
	std::string cmdline;
	for ( const auto &s : cmd_args ) {
		cmdline += "\"" + s + "\" ";
	}
	int exit_code = system( cmdline.c_str() );
#endif

	if ( pipe_res.has_chsp && !keep_tmp ) {
		ChspPipeline::CleanupIntermediate( pipe_res );
	}

	return exit_code;
}
