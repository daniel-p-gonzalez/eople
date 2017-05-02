#pragma once
#include "core.h"
#include "eople_symbol_table.h"
#include "eople_ast.h"
#include <vector>

namespace Eople
{

struct StatementCheck
{
  Node::Function*  function;
  Node::Statement* statement;
  size_t           specialization;

  StatementCheck( Node::Function* in_function, Node::Statement* in_statement, size_t in_specialization )
    : function(in_function), statement(in_statement), specialization(in_specialization)
  {
  }
};

struct FunctionCheck
{
  Node::Function*  function;
  size_t           specialization;

  FunctionCheck( Node::Function* in_function, size_t in_specialization )
    : function(in_function), specialization(in_specialization)
  {
  }
};

typedef std::vector<StatementCheck> StatementVector;
typedef std::vector<FunctionCheck> FunctionVector;

// for propagating state during tree walk
struct InferenceState
{
  InferenceState() : type(TypeBuilder::GetNilType()), clear(false) {}
  type_t type;
  // node and all children typed?
  bool      clear;
};

class PropagateTypeDispatcher;
class InferStatementDispatcher;

class TypeInfer
{
public:
  TypeInfer();
  void InferTypes( Node::Module* module, std::string function_name );
  void IncrementalInfer( Node::Module* module );

  u32 GetErrorCount() { return m_error_count; }

private:
  TypeInfer& operator=(const TypeInfer &){}

//  FunctionVector FunctionFromCall( Node::FunctionCall* node );
  Node::Function* FunctionFromCall( Node::FunctionCall* node );
  Node::Function* FunctionFromProcessMessage( std::string process_name, Node::FunctionCall* node );

  InferenceState PropagateType( Node::NodeCommon*, type_t )
  {
    // Unhandled expression type
    assert(false);
    return InferenceState();
  }
  InferenceState PropagateType( Node::Expression* identifier, type_t type );
  InferenceState PropagateType( Node::Identifier* identifier, type_t type );
  InferenceState PropagateType( Node::Literal* literal, type_t type );
  InferenceState PropagateType( Node::ProcessMessage* process_call, type_t type );
  InferenceState PropagateType( Node::FunctionCall* function_call, type_t type );
  InferenceState PropagateType( Node::ArrayLiteral* array_literal, type_t type );
  InferenceState PropagateType( Node::ArraySubscript* array_subscript, type_t type );
  InferenceState PropagateType( Node::DictLiteral* dict_literal, type_t type );
  InferenceState PropagateType( Node::BinaryOp* binary_op, type_t type );

  void InferStatement( Node::NodeCommon* )
  {
    // Unhandled statement type
    assert(false);
  }
  void InferStatement( Node::Statement* node );
  void InferStatement( Node::Assignment* assignment );
  void InferStatement( Node::If* if_statement );
  void InferStatement( Node::For* for_statement );
  void InferStatement( Node::FunctionCall* function_call );
  void InferStatement( Node::ProcessMessage* process_message );
  void InferStatement( Node::Return* return_statement );
  void InferStatement( Node::When* when );
  void InferStatement( Node::Whenever* whenever );
  void InferStatement( Node::While* while_statement );

  void InferFunctionCall( Node::Statement* node, Node::Function* func );
  void InferProcessMessage( Node::Statement* node, Node::Function* func );
  void InferFunction( Node::Function* node );
  void InferClass( Node::Function* node );
  void DelayedCheck( Node::Function* func, size_t specialization );
  void DelayedCheck( Node::Statement* statement );

  void BumpError()
  {
    ++m_error_count;
  }

  Node::Module*    m_current_module;
  FunctionVector   m_check_functions;
  StatementVector  m_check_statements;

  // current function being processed
  Node::Function* m_function;
  u32             m_error_count;

  friend PropagateTypeDispatcher;
  friend InferStatementDispatcher;
};

class PropagateTypeDispatcher : public ASTDispatcher<PropagateTypeDispatcher>
{
public:
  PropagateTypeDispatcher( TypeInfer* type_infer, Node::NodeCommon* node, type_t type )
    : m_type_infer(type_infer), m_type(type)
  {
    Dispatch(node);
  }

  template <class NODE_T>
  void Process( NODE_T node )
  {
    m_return_val = m_type_infer->PropagateType(node, m_type);
  }

  InferenceState ReturnValue() { return m_return_val; }

private:
  TypeInfer*     m_type_infer;
  InferenceState m_return_val;
  type_t         m_type;
};

class InferStatementDispatcher : public ASTDispatcher<InferStatementDispatcher>
{
public:
  InferStatementDispatcher( TypeInfer* type_infer, Node::NodeCommon* node )
    : m_type_infer(type_infer)
  {
    Dispatch(node);
  }

  template <class NODE_T>
  void Process( NODE_T node )
  {
    m_type_infer->InferStatement(node);
  }

private:
  TypeInfer*      m_type_infer;
};

} // namespace Eople
