#pragma once

#include <string>

#include "../lexer_util.h"
#include "ast.h"

class AstSerializer
{
public:
	std::string Dump( const AstNode &node );
	std::unique_ptr<AstNode> Parse( const std::string &sexpr );

	void SetSymbolTable( std::shared_ptr<CSymbolTable> symtab );

private:
	std::shared_ptr<CSymbolTable> symtab;
};
