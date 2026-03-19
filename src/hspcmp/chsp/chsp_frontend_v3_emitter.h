//
//      cHSP v3 AST emitter surface
//
#pragma once

#include <vector>

#include "chsp_builtin_map.h"
#include "chsp_util.h"
#include "chsp_frontend_v3_ast.h"

class CLogger;
class CMemBuf;

namespace chspv3
{

int GenerateProgramOutput( const ChspV3AstProgram &ast_program, CLogger &logger, CMemBuf &hsp_out,
						   std::vector<ChspNativeArtifact> &native_outputs, const char *source_name,
						   const ChspBuiltinMap &builtin_map );

} // namespace chspv3
