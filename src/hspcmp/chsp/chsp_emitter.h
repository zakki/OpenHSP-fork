//
//      cHSP AST emitter surface
//
#pragma once

#include <vector>

#include "chsp_builtin_map.h"
#include "chsp_util.h"
#include "chsp_ast.h"

class CLogger;
class CMemBuf;

namespace chsp
{

int GenerateProgramOutput( const ChspAstProgram &ast_program, CLogger &logger, CMemBuf &hsp_out,
						   std::vector<ChspNativeArtifact> &native_outputs, const char *source_name,
						   const ChspBuiltinMap &builtin_map, bool for_preprocessor = false );

} // namespace chsp
