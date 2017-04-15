#pragma once
#include "eople_core.h"
#include "eople_symbol_table.h"
#include "eople_ast.h"

namespace Eople
{

struct ExecutableModule
{
  Node::Module* module;
  std::vector<std::unique_ptr<Function>> functions;
  std::vector<std::unique_ptr<Function>> constructors;
  std::vector<std::unique_ptr<Function>> member_functions;
  std::vector<InstructionImpl>           cfunctions;
  std::vector<ExecutableModule*>         imported;

  ExecutableModule(Node::Module* in_module) : module(in_module) {}

  void AddImport( ExecutableModule* imported_module )
  {
    imported.push_back(imported_module);
  }
};

class GenExpressionDispatcher;
class GenStatementDispatcher;
class GetTypeDispatcher;

class ByteCodeGen
{
public:
  ByteCodeGen();
  ~ByteCodeGen()
  {
  }

  std::unique_ptr<ExecutableModule> InitModule( Node::Module* module );
  void GenModule( ExecutableModule* module );
  void ImportCFunctions( ExecutableModule* module, std::vector<std::vector<InstructionImpl>> &cfunctions );

  u32 GetErrorCount() { return m_error_count; }
private:
  ByteCodeGen& operator=(const ByteCodeGen &){}

  size_t GenExpressionTerm( Node::NodeCommon*, bool )
  {
    // Unhandled expression type
    assert(false);
    return (size_t)-1;
  }
  size_t GenExpressionTerm( Node::Expression* node, bool is_root );
  size_t GenExpressionTerm( Node::Identifier* identifier, bool is_root );
  size_t GenExpressionTerm( Node::Literal* literal, bool is_root );
  size_t GenExpressionTerm( Node::BinaryOp* binary_op, bool is_root );
  size_t GenExpressionTerm( Node::FunctionCall* function_call, bool is_root );
  size_t GenExpressionTerm( Node::ProcessMessage* process_message, bool is_root );
  size_t GenExpressionTerm( Node::ArrayLiteral* array_literal, bool is_root );
  size_t GenExpressionTerm( Node::ArrayDereference* array_dereference, bool is_root );
  size_t GenExpressionTerm( Node::DictLiteral* dict_literal, bool is_root );

  size_t GenForInit( Node::ForInit* node );

  void GenStatements( std::vector<StatementNode> &nodes );

  void GenStatement( Node::NodeCommon* )
  {
    // Unhandled statement type
    assert(false);
  }
  void GenStatement( Node::Statement* statement );
  void GenStatement( Node::Return* return_statement );
  void GenStatement( Node::Assignment* assignment );
  void GenStatement( Node::For* for_loop );
  void GenStatement( Node::While* while_loop );
  void GenStatement( Node::When* when );
  void GenStatement( Node::Whenever* whenever );
  void GenStatement( Node::If* if_statement );
  void GenStatement( Node::FunctionCall* function_call );
  void GenStatement( Node::ProcessMessage* process_call );

  void GenFunction( Node::Function* node );
  void GenFunctionCall( Node::FunctionCall* node );
  void GenProcessMessage( Node::ProcessMessage* node );

  template <class T>
  Function* GenWhen( T when_node, Opcode opcode );

  void InitFunction( Node::Function* node );
  size_t PushOpcode( Opcode opcode );
  size_t PushCCall( InstructionImpl cfunction );
  void PushOperand( size_t operand, bool allow_new_op = false );
  Object* StackObject( size_t index );

  type_t GetType( Node::NodeCommon* )
  {
    // Unhandled type
    assert(false);
    return TypeBuilder::GetNilType();
  }
  type_t GetType( Node::Expression* node );
  type_t GetType( Node::Identifier* identifier );
  type_t GetType( Node::Literal* literal );
  type_t GetType( Node::BinaryOp* binary_op );
  type_t GetType( Node::FunctionCall* function_call );
  type_t GetType( Node::ProcessMessage* process_message );
  type_t GetType( Node::ArrayDereference* array_deref );

  Function*        GetConstructor( Node::Function* node, size_t specialization );
  Function*        GetFunction( Node::FunctionCall* node, size_t specialization );
  InstructionImpl  GetCFunction( std::string name, size_t specialization );
  Function*        GetMemberFunction( Node::ProcessMessage* node, size_t specialization );

  void BumpError()
  {
    ++m_error_count;
  }

  u32             m_error_count;

  ExecutableModule* m_current_module;

  // current function being built
  Function*        m_function;
  // ast node for this function
  Node::Function*  m_function_node;
  // Temp info for current function being built:
  // index of first temporary in stack
  size_t           m_first_temp;
  // index of next available temporary
  size_t           m_current_temp;
  // stack index to place result for current op
  size_t           m_result_index;
  // for member functions and nested functions
  size_t           m_base_stack_offset;
  // for determining size of loop/jump
  size_t           m_opcode_count;
  size_t           m_current_operand;

  friend GenExpressionDispatcher;
  friend GenStatementDispatcher;
  friend GetTypeDispatcher;
};

// allows walking of ast nodes with concrete types when all we have are base class pointers
class GenExpressionDispatcher : public ASTDispatcher<GenExpressionDispatcher>
{
public:
  GenExpressionDispatcher( ByteCodeGen* code_gen, Node::NodeCommon* node, bool is_root )
    : m_code_gen(code_gen), m_is_root(is_root)
  {
    Dispatch(node);
  }

  template <class NODE_T>
  void Process( NODE_T node )
  {
    m_return_val = m_code_gen->GenExpressionTerm(node, m_is_root);
  }

  size_t ReturnValue() { return m_return_val; }

private:
  ByteCodeGen* m_code_gen;
  size_t       m_return_val;
  bool         m_is_root;
};

class GenStatementDispatcher : public ASTDispatcher<GenStatementDispatcher>
{
public:
  GenStatementDispatcher( ByteCodeGen* code_gen, Node::NodeCommon* node )
    : m_code_gen(code_gen)
  {
    Dispatch(node);
  }

  template <class NODE_T>
  void Process( NODE_T node )
  {
    m_code_gen->GenStatement(node);
  }

private:
  ByteCodeGen*      m_code_gen;
};

class GetTypeDispatcher : public ASTDispatcher<GetTypeDispatcher>
{
public:
  GetTypeDispatcher( ByteCodeGen* code_gen, Node::NodeCommon* node )
    : m_code_gen(code_gen)
  {
    Dispatch(node);
  }

  template <class NODE_T>
  void Process( NODE_T node )
  {
    m_return_val = m_code_gen->GetType(node);
  }

  type_t ReturnValue() { return m_return_val; }

private:
  ByteCodeGen* m_code_gen;
  type_t       m_return_val;
};

} // namespace Eople
