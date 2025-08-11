#pragma once

#include "ast.h" // Assuming ast.h defines AstNode and its derivatives
#include <memory>
#include <string>

// Forward declaration for AstNode if not included
// class AstNode;

class ICodeGenerator
{
public:
	virtual ~ICodeGenerator() = default;

	// Code generation lifecycle methods
	virtual void initialize() = 0;
	virtual void finalize() = 0;
	virtual std::string getGeneratedCode() = 0; // For C++ code, or path to .ax file

	// AST node visit methods (pure virtual)
	virtual void visit( const AstNode &node ) = 0;
	// Add more specific visit methods as needed, e.g.:
	// virtual void visit(const LiteralIntNode& node) = 0;
	// virtual void visit(const BinaryOpNode& node) = 0;
	// virtual void visit(const AssignmentNode& node) = 0;
	// ... and so on for all relevant AST node types
};
