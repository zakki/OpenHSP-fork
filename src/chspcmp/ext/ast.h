#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class ExpressionNode;
class StatementNode;

// Enum for AST Node Types
enum class NodeType
{
	Program,
	// Expressions
	LiteralInt,
	LiteralDouble,
	LiteralString,
	LiteralLabel,
	LiteralSymbol,
	LiteralMark,
	Variable,
	BinOp,
	UnaryOp,
	Command, // Can be an expression (e.g., a function returning a value)
	ArrayAccess,

	// Statements
	Assignment,
	IfStatement,
	RepeatStatement,
	Label,
	ExpressionStatement,
	// ... other statement types
};

// Base class for all AST nodes
class AstNode
{
public:
	virtual ~AstNode() = default;
	NodeType type;
};

// Root node for the entire program
class ProgramNode : public AstNode
{
public:
	ProgramNode()
	{
		type = NodeType::Program;
	}
	std::vector<std::unique_ptr<StatementNode>> statements;
};

// Base class for all expression nodes
class ExpressionNode : public AstNode
{
public:
	// Virtual destructor
	~ExpressionNode() override = default;
};

// Base class for all statement nodes
class StatementNode : public AstNode
{
public:
	// Virtual destructor
	~StatementNode() override = default;
};

// --- Expression Nodes ---

class LiteralIntNode : public ExpressionNode
{
public:
	explicit LiteralIntNode( int val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralInt;
	}
	int value;
	int ex_flag;
};

class LiteralDoubleNode : public ExpressionNode
{
public:
	explicit LiteralDoubleNode( double val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralDouble;
	}
	double value;
	int ex_flag;
};

class LiteralStringNode : public ExpressionNode
{
public:
	explicit LiteralStringNode( const std::string &val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralString;
	}
	std::string value;
	int ex_flag;
};

class LiteralLabelNode : public ExpressionNode
{
public:
	explicit LiteralLabelNode( int val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralLabel;
	}
	int value;
	int ex_flag;
};

class LiteralSymbolNode : public ExpressionNode
{
public:
	explicit LiteralSymbolNode( int val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralSymbol;
	}
	int value;
	int ex_flag;
};

class LiteralMarkNode : public ExpressionNode
{
public:
	explicit LiteralMarkNode( char val, int ex_flag ) : value( val ), ex_flag( ex_flag )
	{
		type = NodeType::LiteralMark;
	}
	char value;
	int ex_flag;
};

class VariableNode : public ExpressionNode
{
public:
	explicit VariableNode( std::unique_ptr<LiteralSymbolNode> n ) : node( std::move( n ) )
	{
		type = NodeType::Variable;
	}
	std::unique_ptr<LiteralSymbolNode> node;
	std::vector<std::unique_ptr<ExpressionNode>> array_indices;
};

class OperatorType
{
public:
	OperatorType( int op, int ex_flag ) : op( op ), ex_flag( ex_flag )
	{
	}
	int op;
	int ex_flag;
};

class BinaryOpNode : public ExpressionNode
{
public:
	BinaryOpNode( OperatorType op, std::unique_ptr<ExpressionNode> l, std::unique_ptr<ExpressionNode> r )
		: op( op ), left( std::move( l ) ), right( std::move( r ) )
	{
		type = NodeType::BinOp;
	}
	OperatorType op;
	std::unique_ptr<ExpressionNode> left;
	std::unique_ptr<ExpressionNode> right;
};

class UnaryOpNode : public ExpressionNode
{
public:
	UnaryOpNode( int op, std::unique_ptr<ExpressionNode> r ) : op( op ), right( std::move( r ) )
	{
		type = NodeType::UnaryOp;
	}
	int op;
	std::unique_ptr<ExpressionNode> right;
};

class CommandNode : public ExpressionNode
{ // Also a statement
public:
	explicit CommandNode( const std::string &name ) : command_name( name )
	{
		type = NodeType::Command;
	}
	std::string command_name;
	std::vector<std::unique_ptr<ExpressionNode>> params;
};

class ParameterList
{
public:
	ParameterList( char t, std::vector<std::unique_ptr<ExpressionNode>> p ) : access_type( t ), params( std::move( p ) )
	{
	}
	char access_type; // '.' or '(' or '['
	std::vector<std::unique_ptr<ExpressionNode>> params;
};

class ArrayAccessNode : public ExpressionNode
{
public:
	ArrayAccessNode( std::unique_ptr<ExpressionNode> n, std::unique_ptr<ParameterList> i )
		: node( std::move( n ) ), indices( std::move( i ) )
	{
		type = NodeType::ArrayAccess;
	}
	std::unique_ptr<ExpressionNode> node;
	std::unique_ptr<ParameterList> indices;
};


// --- Statement Nodes ---

class AssignmentNode : public StatementNode
{
public:
	AssignmentNode( OperatorType op, std::unique_ptr<VariableNode> lval,
					std::vector<std::unique_ptr<ExpressionNode>> rval = {} )
		: op( op ), lvalue( std::move( lval ) ), rvalue( std::move( rval ) )
	{
		type = NodeType::Assignment;
	}
	OperatorType op;
	std::unique_ptr<VariableNode> lvalue;
	std::vector<std::unique_ptr<ExpressionNode>> rvalue;
};

class IfStatementNode : public StatementNode
{
public:
	explicit IfStatementNode( std::unique_ptr<ExpressionNode> cond ) : condition( std::move( cond ) )
	{
		type = NodeType::IfStatement;
	}
	std::unique_ptr<ExpressionNode> condition;
	std::vector<std::unique_ptr<StatementNode>> if_branch;
	std::vector<std::unique_ptr<StatementNode>> else_branch; // Optional
};

class RepeatStatementNode : public StatementNode
{
public:
	RepeatStatementNode()
	{
		type = NodeType::RepeatStatement;
	}
	std::unique_ptr<ExpressionNode> count; // For 'repeat N'
	std::vector<std::unique_ptr<StatementNode>> loop_body;
	// We can add fields for 'break' and 'continue' handling later
};

class LabelNode : public StatementNode
{
public:
	explicit LabelNode( const std::string &name ) : label_name( name )
	{
		type = NodeType::Label;
	}
	std::string label_name;
};

class ExpressionStatementNode : public StatementNode
{
public:
	explicit ExpressionStatementNode( std::unique_ptr<ExpressionNode> expr ) : expression( std::move( expr ) )
	{
		type = NodeType::ExpressionStatement;
	}
	std::unique_ptr<ExpressionNode> expression;
};
