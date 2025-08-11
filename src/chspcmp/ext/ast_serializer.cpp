#include <sstream>
#include <stdexcept>

#include "../label.h"
#include "ast_serializer.h"

// Helper function to dump a vector of nodes
template <typename T>
static void dump_vector( const std::vector<std::unique_ptr<T>> &nodes, std::stringstream &ss,
						 std::shared_ptr<CSymbolTable> symtab )
{
	for ( const auto &node : nodes ) {
		ss << " ";
		dump_node( *node, ss, symtab );
	}
}

static void dump_node( const AstNode &node, std::stringstream &ss, std::shared_ptr<CSymbolTable> symtab )
{
	ss << "(";

	switch ( node.type ) {
	case NodeType::Program: {
		const auto &n = static_cast<const ProgramNode &>( node );
		ss << "program";
		dump_vector( n.statements, ss, symtab );
		break;
	}
	case NodeType::LiteralInt: {
		const auto &n = static_cast<const LiteralIntNode &>( node );
		ss << "int " << n.value;
		break;
	}
	case NodeType::LiteralString: {
		const auto &n = static_cast<const LiteralStringNode &>( node );
		ss << "string \"" << n.value << "\"";
		break;
	}
	case NodeType::Variable: {
		const auto &n = static_cast<const VariableNode &>( node );
		ss << "var ";
		dump_node( *n.node, ss, symtab );
		if ( !n.array_indices.empty() ) {
			ss << " (indices";
			dump_vector( n.array_indices, ss, symtab );
			ss << ")";
		}
		break;
	}
	case NodeType::Assignment: {
		const auto &n = static_cast<const AssignmentNode &>( node );
		ss << "assign ";
		dump_node( *n.lvalue, ss, symtab );
		ss << " ";
		ss << n.op.op;
		dump_vector( n.rvalue, ss, symtab );
		break;
	}
	case NodeType::ExpressionStatement: {
		const auto &n = static_cast<const ExpressionStatementNode &>( node );
		ss << "expr_stmt ";
		dump_node( *n.expression, ss, symtab );
		break;
	}
	case NodeType::LiteralDouble: {
		const auto &n = static_cast<const LiteralDoubleNode &>( node );
		ss << "double " << n.value;
		break;
	}
	case NodeType::LiteralLabel: {
		const auto &n = static_cast<const LiteralLabelNode &>( node );
		ss << "label " << n.value;
		break;
	}
	case NodeType::LiteralSymbol: {
		const auto &n = static_cast<const LiteralSymbolNode &>( node );
		ss << "symbol " << n.value;
		ss << " \"" << symtab->lb->GetName( n.value ) << "\"";
		break;
	}
	case NodeType::LiteralMark: {
		const auto &n = static_cast<const LiteralMarkNode &>( node );
		ss << "mark " << n.value;
		break;
	}
	case NodeType::BinOp: {
		const auto &n = static_cast<const BinaryOpNode &>( node );
		ss << "binop " << n.op.op << " ";
		ss << "'" << (char)n.op.op << "' ";
		dump_node( *n.left, ss, symtab );
		ss << " ";
		dump_node( *n.right, ss, symtab );
		break;
	}
	case NodeType::UnaryOp: {
		const auto &n = static_cast<const UnaryOpNode &>( node );
		ss << "unop " << n.op << " ";
		ss << "'" << (char)n.op << "' ";
		dump_node( *n.right, ss, symtab );
		break;
	}
	case NodeType::Command: {
		const auto &n = static_cast<const CommandNode &>( node );
		ss << "command " << n.command_name;
		dump_vector( n.params, ss, symtab );
		break;
	}
	case NodeType::ArrayAccess: {
		const auto &n = static_cast<const ArrayAccessNode &>( node );
		ss << "array_access ";
		dump_node( *n.node, ss, symtab );
		ss << " (access_type \"" << n.indices->access_type << "\"";
		dump_vector( n.indices->params, ss, symtab );
		ss << ")";
		break;
	}
	case NodeType::IfStatement: {
		const auto &n = static_cast<const IfStatementNode &>( node );
		ss << "if ";
		dump_node( *n.condition, ss, symtab );
		ss << " (then";
		dump_vector( n.if_branch, ss, symtab );
		ss << ")";
		if ( !n.else_branch.empty() ) {
			ss << " (else";
			dump_vector( n.else_branch, ss, symtab );
			ss << ")";
		}
		break;
	}
	case NodeType::RepeatStatement: {
		const auto &n = static_cast<const RepeatStatementNode &>( node );
		ss << "repeat";
		if ( n.count ) {
			ss << " (count ";
			dump_node( *n.count, ss, symtab );
			ss << ")";
		}
		ss << " (body";
		dump_vector( n.loop_body, ss, symtab );
		ss << ")";
		break;
	}
	case NodeType::Label: {
		const auto &n = static_cast<const LabelNode &>( node );
		ss << "label_stmt " << n.label_name;
		break;
	}
	default:
		ss << "unknown_node_type_" << static_cast<int>( node.type );
		break;
	}

	ss << ")";
}

void AstSerializer::SetSymbolTable( std::shared_ptr<CSymbolTable> symtab )
{
	this->symtab = symtab;
}

std::string AstSerializer::Dump( const AstNode &node )
{
	std::stringstream ss;
	dump_node( node, ss, symtab );
	return ss.str();
}

std::unique_ptr<AstNode> AstSerializer::Parse( const std::string &sexpr )
{
	// TODO: Implement the parser
	throw std::runtime_error( "Parsing not yet implemented" );
}
