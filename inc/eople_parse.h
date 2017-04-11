#pragma once
#include "eople_core.h"
#include "eople_lex.h"
#include "eople_symbol_table.h"
#include "eople_ast.h"

#include <functional>

namespace Eople
{

struct Function;

class Parser
{
public:
  Parser();

  void ParseModule( Lexer &lexer, Node::Module* module );
  void IncrementalParse( Lexer &lexer, Node::Module* module );
  u32 GetErrorCount() { return m_error_count; }

private:
  Parser& operator=(const Parser&){}

  void ConsumeToken()
  {
    m_last_line = m_lex->GetLine();
    m_current_token = m_lex->NextToken();
  }

  bool ConsumeExpected( u32 token )
  {
    if( token == m_current_token )
    {
      m_last_line = m_lex->GetLine();
      ConsumeToken();
      return true;
    }

    return false;
  }

  void ConsumeNewlines();

  ExpressionNode   ParseIdentifier();
  ExpressionNode   ParseUniqueIdentifier();
  ExpressionNode   ParseValidIdentifier( std::string ident );
  ExpressionNode   ParseType();
  ExpressionNode   ParseArrayLiteral();
  ExpressionNode   ParseArrayDereference( std::string ident );
  ExpressionNode   ParseFloat(bool negative);
  ExpressionNode   ParseInt(bool negative);
  ExpressionNode   ParseBool();
  ExpressionNode   ParseString();
  ExpressionNode   ParseFactor();
  ExpressionNode   ParseOp( std::function<bool(i32)> predicate, std::function<ExpressionNode(Parser* const)> arg_parser );
  ExpressionNode   ParseTerm();
  ExpressionNode   ParseArithExpression();
  ExpressionNode   ParseExpression();
  ExpressionNode   ParseAndExpression();
  ExpressionNode   ParseBitshiftExpression();
  ExpressionNode   ParseBitwiseAndExpression();
  ExpressionNode   ParseBitwiseXorExpression();
  ExpressionNode   ParseBitwiseOrExpression();
  ExpressionNode   ParseCompExpression();
  ExpressionNode   ParseParExpression();
  ExpressionNode   ParseFunctionCallExpression( std::string ident );
  ExpressionNode   ParseStructMember();
  StatementNode    ParseProcessMessage( ExpressionNode ident_node );
  StatementNode    ParseStatement();
  StatementNode    ParseIf();
  StatementNode    ParseAssignment( ExpressionNode ident_node );
  StatementNode    ParseFunctionCall( ExpressionNode ident_node, bool need_terminator );
  StatementNode    ParseReturn();
  StatementNode    ParseFor();
  StatementNode    ParseForInit();
  ExpressionNode   ParseCondition();
  void             ParseStatementList( std::vector<StatementNode> &statement_list );
  StatementNode    ParseWhile();
  StatementNode    ParseWhen();
  StatementNode    ParseWhenever();
  FunctionNode     ParseClass();
  FunctionNode     ParseFunction( bool is_constructor );
  StructNode       ParseStruct();
  bool             ParseNamespace();

  void PushNamespace( std::string name )
  {
    std::string parent_scope = m_namespace_stack.size() > 0 ? m_namespace_stack.back() : "";
    m_namespace_stack.push_back(parent_scope + name + "::");
  }

  void PopNamespace()
  {
    m_namespace_stack.pop_back();
  }

  void BumpError()
  {
    ++m_error_count;
    m_last_error_line = m_last_line;
  }

  Lexer*        m_lex;
  Node::Module* m_current_module;
  SymbolTable*  m_symbols;

  std::vector<std::string>     m_namespace_stack;
  std::vector<Node::Function*> m_context_stack;

  // current function being parsed
  Node::Function* m_function;
  u32             m_current_token;
  u32             m_error_count;
  u32             m_last_line;
  u32             m_last_error_line;
  u32             m_temp_count;
  u32             m_temp_max;
  u32             m_when_count;
  u32             m_whenever_count;
};

} // namespace Eople
