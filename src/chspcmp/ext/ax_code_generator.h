#pragma once

#include "code_generator.h"
#include "membuf.h" // For generating .ax code

class AxCodeGenerator : public ICodeGenerator
{
public:
	AxCodeGenerator();
	~AxCodeGenerator() override = default;

	void initialize() override;
	void finalize() override;
	std::string getGeneratedCode() override; // Returns the generated .ax data as a string (or path)

	void visit( const AstNode &node ) override;
	// Add specific visit methods for different AST node types as needed
	// For example:
	// void visit(const LiteralIntNode& node) override;
	// void visit(const BinaryOpNode& node) override;

private:
	CMemBuf ax_buffer_;
};
