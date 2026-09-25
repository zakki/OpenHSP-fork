#pragma once

#include <string>
#include <vector>
#include <memory>

enum class ChspPipelineCompileMode {
	None,
	Libtcc,
};

struct ChspPipelineOptions {
	std::string source_path;
	std::string output_ax_path;
	std::string common_path;
	ChspPipelineCompileMode compile_mode = ChspPipelineCompileMode::Libtcc;
	bool debug_mode = false;
	bool keep_intermediate = false;
};

struct ChspPipelineResult {
	bool has_chsp = false;
	bool success = false;
	std::string intermediate_hsp_path;
	std::vector<std::string> generated_c_files;
	std::vector<std::string> generated_so_files;
	std::string error_message;
};

class ChspPipeline {
public:
	static bool ContainsChspDirective( const char *text );
	static bool ContainsChspInFile( const std::string &filepath );

	// ソースをパースし、Cコード・共有ライブラリ・HSP中間コードを生成する
	static ChspPipelineResult Process( const ChspPipelineOptions &options );

	// 生成された一時ファイルを削除する
	static void CleanupIntermediate( const ChspPipelineResult &result );
};
