#include "eople_log.h"
#include "eople_type_infer.h"

#include <queue>

namespace Eople
{

TypeInfer::TypeInfer()
  : m_current_module(nullptr), m_error_count(0)
{
}

#define TOKEN_STRING_CASE(token, string) case token: return string

const char* StringFromValueType( ValueType val_type )
{
  switch(val_type)
  {
    TOKEN_STRING_CASE(ValueType::INT,      "int");
    TOKEN_STRING_CASE(ValueType::FLOAT,    "float");
    TOKEN_STRING_CASE(ValueType::BOOL,     "bool");
    TOKEN_STRING_CASE(ValueType::NIL,      "nil");
    TOKEN_STRING_CASE(ValueType::STRING,   "string");
    TOKEN_STRING_CASE(ValueType::PROCESS,  "process");
    TOKEN_STRING_CASE(ValueType::PROMISE,  "promise");
    TOKEN_STRING_CASE(ValueType::ARRAY,    "array");
    TOKEN_STRING_CASE(ValueType::FUNCTION, "function");
    TOKEN_STRING_CASE(ValueType::TYPE,     "kind");
    default:
      return "<Unknown Type>";
  }
}

void TypeInfer::IncrementalInfer( Node::Module* module )
{
  m_error_count = 0;
  m_current_module = module;

  for( auto &function : m_current_module->functions )
  {
    m_function = function.get();
    for( size_t i = m_function->next_incremental_statement; i < m_function->body.size(); ++i )
    {
      auto &statement = m_function->body[i];
      InferStatement( statement.get() );
    }
    if( m_error_count )
    {
      m_function->symbols.Rollback();
      m_function->body.erase( m_function->body.begin() + m_function->next_incremental_statement, m_function->body.end() );
    }
    m_function->next_incremental_statement = m_function->body.size();
  }

  for( auto &constructor : m_current_module->classes )
  {
    InferClass( constructor.get() );
  }

  if( m_error_count )
  {
    return;
  }

  if( !m_check_statements.empty() )
  {
    for( auto statement : m_check_statements )
    {
      m_function = statement.function;

      InferStatement(statement.statement);
    }
  }

  if( !m_check_functions.empty() )
  {
    FunctionVector copy = m_check_functions;
    for( auto function : copy )
    {
      function.function->SetCurrentSpecialization(function.specialization);
      InferFunction(function.function);
    }
  }
}

void TypeInfer::InferTypes( Node::Module* module, std::string function_name )
{
  m_error_count = 0;
  m_current_module = module;

  for( auto &function : m_current_module->functions )
  {
    if( function->name == function_name )
    {
      InferFunction( function.get() );
      break;
    }
  }

  if( m_error_count )
  {
    return;
  }

  if( !m_check_statements.empty() )
  {
    for( auto statement : m_check_statements )
    {
      m_function = statement.function;

      InferStatement(statement.statement);
    }
  }

  if( !m_check_functions.empty() )
  {
    FunctionVector copy = m_check_functions;
    for( auto function : copy )
    {
      function.function->SetCurrentSpecialization(function.specialization);
      InferFunction(function.function);
    }
  }
}

void TypeInfer::InferFunction( Node::Function* node )
{
  if( node->is_c_call || !node->is_template[node->current_specialization] )
  {
    return;
  }

  node->is_template[node->current_specialization] = false;

  Node::Function* outer = m_function;
  m_function = node;

  for( auto &statement : node->body )
  {
    InferStatement( statement.get() );
  }

  // nested functions
  for( auto &function : node->functions )
  {
    InferFunction( function.get() );
  }

  m_function = outer;
}

void TypeInfer::InferClass( Node::Function* node )
{
  if( node->is_c_call || !node->is_template[node->current_specialization] )
  {
    return;
  }

  node->is_template[node->current_specialization] = false;

  Node::Function* outer = m_function;
  m_function = node;

  // constructor
  for( auto &statement : node->body )
  {
    InferStatement( statement.get() );
  }

  // member functions
  for( auto &function : node->functions )
  {
    InferFunction( function.get() );
  }

  m_function = outer;
}

// double dispatch support
InferenceState TypeInfer::PropagateType( Node::Expression* expression, type_t type )
{
  // dispatch call to concrete type
  PropagateTypeDispatcher dispatcher(this, expression, type);
  return dispatcher.ReturnValue();
}

InferenceState TypeInfer::PropagateType( Node::Identifier* identifier, type_t type )
{
  InferenceState state;
  type_t my_type = m_function->GetExpressionType( identifier );

  if( type == TypeBuilder::GetNilType() )
  {
    state.type = my_type;
    if( my_type != TypeBuilder::GetNilType() )
    {
      state.clear = true;
    }
  }
  else if( my_type->type == ValueType::PROMISE && type == TypeBuilder::GetBoolType() )
  {
    // TODO: this is a kludge to get static guards off the ground
    identifier->is_guard = true;
    state.type = type;
    state.clear = true;
  }
  else if( (my_type != TypeBuilder::GetNilType()) && (my_type != type) )
  {
    Log::Error("(%d): Type ERROR: Type of '%s' is %s, expected: %s.\n", identifier->line, identifier->name.c_str(),
                                                  StringFromValueType(my_type->type), StringFromValueType(type->type));
    BumpError();
  }
  else
  {
    state.type = type;
    state.clear = true;
    // TODO: this is a kludge to get static guards off the ground
    if( !identifier->is_guard )
    {
      m_function->TrySetExpressionType( identifier, type );
    }
  }

  return state;
}

InferenceState TypeInfer::PropagateType( Node::ArrayDereference* array_deref, type_t type )
{
  type_t element_type = m_function->GetExpressionType( array_deref->ident->GetAsIdentifier() )->GetVaryingType();
  InferenceState state;
  state.type = element_type;
  state.clear = true;
  return state;
}

InferenceState TypeInfer::PropagateType( Node::Literal* literal, type_t type )
{
  InferenceState state;
  if( (type->type != ValueType::NIL) && (literal->GetValueType() != type) )
  {
      Log::Error("(%d): Type ERROR: Type of %f is %s, was expecting: %s.\n", literal->line,
              literal->value_string.c_str(), StringFromValueType(literal->GetValueType()->type), StringFromValueType(type->type));
      BumpError();
  }
  state.type = literal->GetValueType();
  state.clear = true;
  return state;
}

InferenceState TypeInfer::PropagateType( Node::ProcessMessage* process_call, type_t type )
{
  InferenceState state;
  state.type = TypeBuilder::GetPromiseType(TypeBuilder::GetNilType());
  Node::FunctionCall* message = process_call->message->GetAsFunctionCall();
  TableEntry* entry = m_function->GetTableEntry(process_call->process.get());
  if( !entry )
  {
    Log::Error("(%d): Type ERROR: Identifer missing symbol table entry: %s.\n", process_call->line, process_call->process->GetAsIdentifier()->name.c_str());
    BumpError();
  }
  Node::Function* func = FunctionFromProcessMessage( entry->target_name, message );
  if( !func )
  {
    state.clear = false;
    return state;
  }
  if( func->parameters.size() != message->arguments.size() )
  {
    Log::Error("(%d): Type ERROR: Expected %d arguments to '%s' function call, got %d\n", process_call->line, 
                          func->parameters.size(), func->name.c_str(), message->arguments.size());
    BumpError();
    state.clear = false;
    return state;
  }
  InferProcessMessage( process_call, func );

  // check if ready to evaluate this yet
  std::vector<type_t> arg_types;
  for( auto &argument : message->arguments )
  {
    InferenceState arg_state = PropagateType( argument.get(), TypeBuilder::GetNilType() );
    if( arg_state.type == TypeBuilder::GetNilType() )
    {
      state.clear = false;
      state.type = TypeBuilder::GetNilType();
      return state;
    }
    arg_types.push_back(arg_state.type);
  }

  if( func->is_constructor )
  {
    func->TrySetValueType(TypeBuilder::GetProcessType(func->name));
  }

  size_t specialization = func->FindSpecializationForArgs( arg_types );
  func->SetParameterClassNames( message, m_function, arg_types, specialization );
  func->current_specialization = specialization;
  message->SetSpecialization( m_function->current_specialization, specialization );
  m_function->RegisterProcessMessage( process_call );

  if( func->is_constructor )
  {
    InferClass(func);
  }
  else
  {
    InferFunction(func);
  }

  type = func->GetValueType();
  //if( type->type == ValueType::NIL )
  //{
  //  DelayedCheck( func, specialization );
  //  DelayedCheck( m_function, m_function->current_specialization );
  //}
  //else
  if( type->type != ValueType::NIL )
  {
    message->TrySetReturnType(m_function->current_specialization, type);
    state.type = type;
    state.clear = true;
  }

  state.type = TypeBuilder::GetPromiseType(func->GetValueType());
  state.clear = true;
  return state;
}

InferenceState TypeInfer::PropagateType( Node::FunctionCall* function_call, type_t type )
{
  InferenceState state;
  Node::Function* func = FunctionFromCall(function_call);
  if( !func )
  {
    Log::Error("(%d): Type ERROR: Couldn't find function definition for: '%s'.\n", function_call->line, function_call->GetName().c_str());
    BumpError();
    return state;
  }
  // TODO: get rid of this mess
  if( function_call->GetName() == "get_value" || function_call->GetName() == "top" )
  {
    assert( function_call->arguments.size() == 1 );
    type_t promise_type = m_function->GetExpressionVaryingType( function_call->arguments[0].get() );
    assert( type == TypeBuilder::GetNilType() || promise_type == type );
    type = promise_type;
  }
  else if( function_call->GetName() == "array" )
  {
    // TODO: is this enforced before this point?
    assert( function_call->arguments.size() == 1 && function_call->arguments[0]->GetAsTypeLiteral() );
    type_t array_type = TypeBuilder::GetArrayType(function_call->arguments[0]->GetAsTypeLiteral()->type);
    assert( type == TypeBuilder::GetNilType() || array_type == type );
    type = array_type;
  }
  else if( function_call->GetName() == "push" )
  {
    // TODO: is this enforced before this point?
    assert( function_call->arguments.size() == 2 );

    type_t array_type   = m_function->GetExpressionType( function_call->arguments[0].get() );
    m_function->TrySetExpressionType( function_call->arguments[1].get(), array_type->GetVaryingType() );
    assert( array_type->GetVaryingType() == m_function->GetExpressionType(function_call->arguments[1].get()) );
  }

  // check if ready to evaluate this function call yet
  std::vector<type_t> arg_types;
  for( auto &argument : function_call->arguments )
  {
    InferenceState arg_state = PropagateType( argument.get(), TypeBuilder::GetNilType() );
    if( arg_state.type == TypeBuilder::GetNilType() )
    {
      state.clear = false;
      state.type = arg_state.type;
      return state;
    }
    arg_types.push_back(arg_state.type);
  }

  if( func->parameters.size() != function_call->arguments.size() )
  {
    Log::Error("(%d): Type ERROR: Expected %d arguments to '%s' function call, got %d\n", function_call->line, 
                          func->parameters.size(), func->name.c_str(), function_call->arguments.size());
    BumpError();
    return state;
  }

  if( func->is_constructor )
  {
    func->TrySetValueType(TypeBuilder::GetProcessType(func->name));
  }

  size_t specialization = func->FindSpecializationForArgs( arg_types );
  func->SetParameterClassNames( function_call, m_function, arg_types, specialization );
  func->current_specialization = specialization;
  function_call->SetSpecialization( m_function->current_specialization, specialization );
  m_function->RegisterFunctionCall( function_call );

  if( func->is_constructor )
  {
    InferClass(func);
  }
  else
  {
    InferFunction(func);
  }

  if( type->type != ValueType::NIL )
  {
    type_t return_type = func->GetValueType();
    if( return_type->type != ValueType::NIL && return_type->type != ValueType::ANY && return_type != type
      && return_type->GetVaryingType()->type != ValueType::ANY && return_type->GetVaryingType() != type->GetVaryingType()
      && !(func->is_latent_c_call && (type->type == ValueType::PROMISE)))
    {
      Log::Error("(%d): Type ERROR: Return value for '%s' is %s, expected: %s.\n", function_call->line, function_call->GetName().c_str(),
                                                  StringFromValueType(func->GetValueType()->type), StringFromValueType(type->type));
      BumpError();
    }
    else
    {
      function_call->TrySetReturnType(m_function->current_specialization, type);
      state.type = type;
      state.clear = true;
    }
  }
  else
  {
    type = func->GetValueType();
    //if( type->type == ValueType::NIL )
    //{
    //  DelayedCheck( func, specialization );
    //  DelayedCheck( m_function, m_function->current_specialization );
    //}
    //else
    if( type->type != ValueType::NIL )
    {
      function_call->TrySetReturnType(m_function->current_specialization, type);
      state.type = type;
      state.clear = true;
    }
    else
    {
      state.clear = false;
    }
  }
  return state;
}

InferenceState TypeInfer::PropagateType( Node::ArrayLiteral* array_literal, type_t )
{
  InferenceState array_type;
  InferenceState element_state;
  // find a valid type
  for( auto &element : array_literal->elements )
  {
    element_state = PropagateType( element.get(), TypeBuilder::GetNilType() );
    if( element_state.clear && !element_state.type->IsIncompleteType() )
    {
      array_type = element_state;
      break;
    }
  }
  if( array_type.clear )
  {
    // try to set all element types to the found type
    for( auto &element : array_literal->elements )
    {
      element_state = PropagateType( element.get(), array_type.type );
      if( !element_state.clear )
      {
        array_type.clear = false;
        return array_type;
      }
    }
  }
  array_literal->array_type = array_type.type;
  array_type.type = TypeBuilder::GetArrayType(array_type.type);
  return array_type;
}

InferenceState TypeInfer::PropagateType( Node::DictLiteral* dict_literal, type_t )
{
  InferenceState dict_type;
  dict_type.type = TypeBuilder::GetDictType();
  dict_type.clear = true;
  return dict_type;
}

InferenceState TypeInfer::PropagateType( Node::BinaryOp* binary_op, type_t type )
{
  InferenceState state;
  bool is_boolean = false;
  switch( binary_op->op )
  {
    case OpType::GT:
    case OpType::LT:
    case OpType::EQ:
    case OpType::NEQ:
    case OpType::LEQ:
    case OpType::GEQ:
    case OpType::AND:
    case OpType::OR:
    {
      if( (type->type != ValueType::NIL) && (ValueType::BOOL != type->type) )
      {
        Log::Error("(%d): Type ERROR: Expression type should be %s, got: %s.\n", binary_op->line,
                                      TOKEN_STRING(ValueType::BOOL), StringFromValueType(type->type));
        BumpError();
      }
      is_boolean = true;
      // shouldn't propagate boolean type to children if it's a comparison
      if( binary_op->op != OpType::AND && binary_op->op != OpType::OR )
      {
        type = TypeBuilder::GetNilType();
      }
      // intentional fall-through -
    }
    case OpType::ADD:
    case OpType::SUB:
    case OpType::MUL:
    case OpType::DIV:
    case OpType::MOD:
    case OpType::SHL:
    case OpType::SHR:
    case OpType::BAND:
    case OpType::BXOR:
    case OpType::BOR:
    {
      if( binary_op->op == OpType::SHL || binary_op->op == OpType::SHR || binary_op->op == OpType::BAND || 
          binary_op->op == OpType::BXOR || binary_op->op == OpType::BOR )
      {
        if( type->type != ValueType::NIL && type->type != ValueType::INT )
        {
          Log::Error("(%d): Type ERROR: Can't use bitwise operator on type: %s.\n", binary_op->line, StringFromValueType(type->type));
          BumpError();
        }

        type = TypeBuilder::GetPrimitiveType(ValueType::INT);
      }

      InferenceState left_state = PropagateType( binary_op->left.get(), type );
      InferenceState right_state = PropagateType( binary_op->right.get(), type );

      bool left_has_type = (left_state.type->type != ValueType::NIL);
      bool promise_matches = is_boolean &&
                             (((left_state.type->type == ValueType::PROMISE) || (left_state.type->type == ValueType::BOOL))) &&
                             (((right_state.type->type == ValueType::PROMISE) || (right_state.type->type == ValueType::BOOL)));
      bool right_has_type = (right_state.type->type != ValueType::NIL);

      if( left_has_type )
      {
        if( right_has_type && (right_state.type != left_state.type) && !promise_matches )
        {
          Log::Error("(%d): Type ERROR: Mismatch between %s and %s.\n", binary_op->line,
                                        StringFromValueType(left_state.type->type), StringFromValueType(right_state.type->type));
          BumpError();
        }
        if( !right_has_type )
        {
          right_state = PropagateType( binary_op->right.get(), left_state.type );
        }
      }
      else if( right_has_type )
      {
        if( left_has_type && (left_state.type != right_state.type) && !promise_matches )
        {
          Log::Error("(%d): Type ERROR: Mismatch between %s and %s.\n", binary_op->line,
                                        StringFromValueType(left_state.type->type), StringFromValueType(right_state.type->type));
          BumpError();
        }
        if( !left_has_type )
        {
          left_state = PropagateType( binary_op->left.get(), right_state.type );
        }
      }

      state.type = is_boolean ? TypeBuilder::GetBoolType() : left_state.type;
      state.clear = left_state.clear && right_state.clear;

      return state;
    }
    default:
    {
      Log::Error("(%d): Type ERROR: Unhandled binary op type: %d\n", binary_op->line, binary_op->op);
      break;
    }
  }

  return state;
}

// double dispatch support
void TypeInfer::InferStatement( Node::Statement* statement )
{
  // dispatch call to concrete type
  InferStatementDispatcher dispatcher(this, statement);
}

void TypeInfer::InferStatement( Node::Assignment* assignment )
{
  InferenceState left_state = PropagateType(assignment->left.get(), TypeBuilder::GetNilType());
  InferenceState right_state = PropagateType(assignment->right.get(), TypeBuilder::GetNilType());

  bool left_has_type = (left_state.type->type != ValueType::NIL)
    && (left_state.type->type != ValueType::PROMISE || ((PromiseType*)left_state.type)->result_type->type != ValueType::NIL);
  bool right_has_type = (right_state.type->type != ValueType::NIL)
    && (right_state.type->type != ValueType::PROMISE || ((PromiseType*)right_state.type)->result_type->type != ValueType::NIL);

  if( right_state.type->type == ValueType::PROCESS )
  {
    auto left  = assignment->left->GetAsIdentifier();
    auto right = assignment->right->GetAsFunctionCall();
    TableEntry* entry = m_function->GetTableEntry(left);
    if( entry )
    {
      if( right )
      {
        entry->target_name = right->GetName();
      }
      else
      {
        auto right_ident = assignment->right->GetAsIdentifier();
        TableEntry* right_entry = m_function->GetTableEntry(assignment->right.get());
        if( right_ident && right_entry )
        {
          entry->target_name = right_entry->target_name;
        }
        else
        {
          Log::Error("(%d): Type ERROR: Could not infer type for process reference: %s.\n", assignment->line, left->name.c_str());
          BumpError();
        }
      }
    }
    else
    {
      Log::Error("(%d): Type ERROR: Identifer missing symbol table entry: %s.\n", assignment->line, left->name.c_str());
      BumpError();
    }
  }

  if( left_has_type )
  {
    if( right_has_type && (right_state.type != left_state.type) )
    {
      Log::Error("(%d): Type ERROR: Mismatch between %s and %s.\n", assignment->line,
                                    StringFromValueType(left_state.type->type), StringFromValueType(right_state.type->type));
      BumpError();
    }
    if( !right_has_type )
    {
      right_state = PropagateType(assignment->right.get(), left_state.type);
    }
  }
  else if( right_has_type )
  {
    if( left_has_type && (left_state.type != right_state.type) )
    {
      Log::Error("(%d): Type ERROR: Mismatch between %s and %s.\n", assignment->line,
                                    StringFromValueType(left_state.type->type), StringFromValueType(right_state.type->type));
      BumpError();
    }
    if( !left_has_type )
    {
      left_state = PropagateType(assignment->left.get(), right_state.type);
    }
  }

  if( !(left_state.clear && right_state.clear) )
  {
    DelayedCheck( assignment );
  }
}

void TypeInfer::InferStatement( Node::If* if_statement )
{
  InferenceState state = PropagateType(if_statement->condition.get(), TypeBuilder::GetBoolType());

  if( !state.clear )
  {
    DelayedCheck( if_statement );
  }

  for( auto &statement : if_statement->body )
  {
    InferStatement( statement.get() );
  }

  for( auto &elseif_node : if_statement->elseifs )
  {
    auto elseif = elseif_node->GetAsElseIf();
    state = PropagateType(elseif->condition.get(), TypeBuilder::GetBoolType());
    if( !state.clear )
    {
      DelayedCheck( if_statement );
    }

    for( auto &statement : elseif->body )
    {
      InferStatement( statement.get() );
    }
  }

  for( auto &statement : if_statement->else_body )
  {
    InferStatement( statement.get() );
  }
}

void TypeInfer::InferStatement( Node::For* for_loop )
{
  auto for_init = for_loop->init->GetAsForInit();
  InferenceState start_state = PropagateType(for_init->start.get(), TypeBuilder::GetNilType());
  InferenceState end_state;
  InferenceState step_state;
  InferenceState counter_state;

  if( start_state.type->type == ValueType::ARRAY )
  {
    start_state.type = m_function->GetExpressionVaryingType( for_init->start.get() );
  }

  if( start_state.type->type != ValueType::NIL )
  {
    if( for_init->end )
    {
      end_state = PropagateType(for_init->end.get(), start_state.type);
    }
    else
    {
      end_state.clear = true;
    }

    if( for_init->step )
    {
      step_state = PropagateType(for_init->step.get(), start_state.type);
    }
    else
    {
      step_state.clear = true;
    }

    if( for_init->counter )
    {
      counter_state = PropagateType(for_init->counter.get(), start_state.type);
    }
    else
    {
      counter_state.clear = true;
    }
    // TODO: check for mismatch after implementing proper scoping
  }

  if( !(start_state.clear && end_state.clear && step_state.clear && counter_state.clear) )
  {
    DelayedCheck( for_loop );
  }

  for( auto &statement : for_loop->body )
  {
    InferStatement( statement.get() );
  }
}

void TypeInfer::InferStatement( Node::FunctionCall* function_call )
{
  Node::Function* func = FunctionFromCall(function_call);
  if( !func )
  {
    Log::Error("(%d): Type ERROR: Couldn't find function definition for: '%s'.\n", function_call->line, function_call->GetName().c_str());
    BumpError();
    return;
  }
  if( func->parameters.size() != function_call->arguments.size() )
  {
    Log::Error("(%d): Type Warning: Expected %d arguments to '%s' function call, got %d\n", function_call->line, 
                          func->parameters.size(), func->name.c_str(), function_call->arguments.size());
    BumpError();
    return;
  }
  InferFunctionCall( function_call, func );
}

void TypeInfer::InferStatement( Node::ProcessMessage* process_message )
{
  Node::FunctionCall* message = process_message->message->GetAsFunctionCall();
  TableEntry* entry = m_function->GetTableEntry(process_message->process.get());
  if( !entry )
  {
    Log::Error("(%d): Type ERROR: Identifer missing symbol table entry: %s.\n", process_message->line, process_message->process->GetAsIdentifier()->name.c_str());
    BumpError();
  }
  Node::Function* func = FunctionFromProcessMessage( entry->target_name, message );
  if( !func )
  {
    DelayedCheck( process_message );
    return;
  }
  if( func->parameters.size() != message->arguments.size() )
  {
    Log::Error("(%d): Type Warning: Expected %d arguments to '%s' function call, got %d\n", process_message->line, 
                          func->parameters.size(), func->name.c_str(), message->arguments.size());
    BumpError();
    return;
  }

  InferenceState state = PropagateType(process_message, TypeBuilder::GetNilType());

//  InferProcessMessage( process_message, func );

  InferenceState arg_state;
  // check if ready to evaluate this yet
  std::vector<type_t> arg_types;
  for( auto &argument : message->arguments )
  {
    arg_state = PropagateType( argument.get(), TypeBuilder::GetNilType() );
    if( arg_state.type == TypeBuilder::GetNilType() )
    {
      return;
    }
    arg_types.push_back(arg_state.type);
  }

  if( func->is_constructor )
  {
    func->TrySetValueType(TypeBuilder::GetProcessType(func->name));
  }

  size_t specialization = func->FindSpecializationForArgs( arg_types );
  func->SetParameterClassNames( message, m_function, arg_types, specialization );
  func->current_specialization = specialization;
  message->SetSpecialization( m_function->current_specialization, specialization );
  m_function->RegisterProcessMessage( process_message );

  if( func->is_constructor )
  {
    InferClass(func);
  }
  else
  {
    InferFunction(func);
  }

  type_t type = func->GetValueType();
  //if( type->type == ValueType::NIL )
  //{
  //  DelayedCheck( func, specialization );
  //  DelayedCheck( m_function, m_function->current_specialization );
  //}
  //else
  if( type->type != ValueType::NIL )
  {
    message->TrySetReturnType(m_function->current_specialization, type);
  }
}

void TypeInfer::InferStatement( Node::Return* return_statement )
{
  if( return_statement->return_value )
  {
    InferenceState state = PropagateType(return_statement->return_value.get(), TypeBuilder::GetNilType());

    if( !state.clear )
    {
      DelayedCheck( return_statement );
    }

    if( state.type != TypeBuilder::GetNilType() )
    {
      return_statement->TrySetValueType( state.type );
      m_function->TrySetValueType( state.type );
    }
  }
}

void TypeInfer::InferStatement( Node::When* when )
{
  InferenceState state = PropagateType(when->condition.get(), TypeBuilder::GetBoolType());
  if( !state.clear )
  {
    DelayedCheck( when );
  }

  for( auto &statement : when->body )
  {
    InferStatement( statement.get() );
  }
}

void TypeInfer::InferStatement( Node::Whenever* whenever )
{
  InferenceState state = PropagateType(whenever->condition.get(), TypeBuilder::GetBoolType());
  if( !state.clear )
  {
    DelayedCheck( whenever );
  }

  for( auto &statement : whenever->body )
  {
    InferStatement( statement.get() );
  }
}

void TypeInfer::InferStatement( Node::While* while_loop )
{
  InferenceState state = PropagateType(while_loop->condition.get(), TypeBuilder::GetBoolType());
  if( !state.clear )
  {
    DelayedCheck( while_loop );
  }

  if( state.type != TypeBuilder::GetNilType() )
  {
    while_loop->TrySetValueType( state.type );
  }

  for( auto &statement : while_loop->body )
  {
    InferStatement( statement.get() );
  }
}

void TypeInfer::DelayedCheck( Node::Statement* statement )
{
  bool found = false;
  if( !m_check_statements.empty() )
  {
    for( auto check_statement : m_check_statements )
    {
      if( check_statement.statement == statement )
      {
        found = true;
        break;
      }
    }
  }

  if( !found )
  {
    m_check_statements.push_back( StatementCheck(m_function, statement, m_function->current_specialization) );
  }
}

void TypeInfer::DelayedCheck( Node::Function* func, size_t specialization )
{
  if( func->is_c_call )
  {
    return;
  }

  bool found = false;
  if( !m_check_functions.empty() )
  {
    for( auto check_func : m_check_functions )
    {
      if( check_func.function == func )
      {
        if( check_func.specialization == specialization )
        {
          found = true;
          break;
        }
        else if( check_func.specialization == 0 )
        {
          check_func.specialization = specialization;
          return;
        }
      }
    }
  }

  if( !found )
  {
    m_check_functions.push_back( FunctionCheck(func, specialization) );
  }
}

Node::Function* TypeInfer::FunctionFromCall( Node::FunctionCall* node )
{
  std::queue<Node::Module*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto module = modules.front(); modules.pop();

    // TODO: not really properly checking namespaces here...
    bool is_local_module = true; //m_current_module == module;

    Node::Function* best_match = nullptr;
    size_t best_match_depth = 0;
    for( auto &func : module->functions )
    {
      for( size_t i = best_match_depth; i < node->namespace_stack.size(); ++i )
      {
        if( func->name == node->namespace_stack[i] + node->GetName() )
        {
          if( i == (node->namespace_stack.size()-1) )
          {
            // closest possible match
            return func.get();
          }
          best_match = func.get();
          best_match_depth = i;
        }
      }
      // is it in local module scope?
      if( !best_match_depth && !best_match && is_local_module && func->name == node->GetName() )
      {
        best_match = func.get();
        best_match_depth = 0;
      }
    }

    if( !best_match )
    {
      for( auto &func : module->classes )
      {
        for( size_t i = best_match_depth; i < node->namespace_stack.size(); ++i )
        {
          if( func->name == node->namespace_stack[i] + node->GetName() )
          {
            if( i == (node->namespace_stack.size()-1) )
            {
              // closest possible match
              return func.get();
            }
            best_match = func.get();
            best_match_depth = i;
          }
        }
        // is it in local module scope?
        if( !best_match_depth && !best_match && is_local_module && func->name == node->GetName() )
        {
          best_match = func.get();
          best_match_depth = 0;
        }
      }
    }
    if( best_match )
    {
      return best_match;
    }
    // couldn't find function - search imported modules
    for( auto imported_module : module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

Node::Function* TypeInfer::FunctionFromProcessMessage( std::string process_name, Node::FunctionCall* node )
{
  std::queue<Node::Module*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto module = modules.front(); modules.pop();

    // TODO: not really properly checking namespaces here...
    bool is_local_module = true; //m_current_module == module;

    Node::Function* best_process_match = nullptr;
    size_t best_match_depth = 0;
    for( auto &func : module->classes )
    {
      for( size_t i = best_match_depth; i < node->namespace_stack.size(); ++i )
      {
        if( func->name == node->namespace_stack[i] + process_name )
        {
          best_process_match = func.get();
          best_match_depth = i;
          if( i == (node->namespace_stack.size()-1) )
          {
            // closest possible match
            break;
          }
        }
      }
      // is it in local module scope?
      if( !best_match_depth && !best_process_match && is_local_module && func->name == process_name )
      {
        best_process_match = func.get();
        best_match_depth = 0;
      }
    }

    std::string full_name = process_name + "::" + node->GetName();
    if( best_process_match )
    {
      for( auto &func : best_process_match->functions )
      {
        if( func->name == full_name )
        {
          return func.get();
        }
      }
    }
    // couldn't find class - search imported modules
    for( auto imported_module : module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

void TypeInfer::InferFunctionCall( Node::Statement* node, Node::Function* func )
{
  auto function_call = node->GetAsFunctionCall();
  // check if ready to evaluate this function call yet
  for( auto &argument : function_call->arguments )
  {
    InferenceState arg_state = PropagateType(argument.get(), TypeBuilder::GetNilType());
    if( !arg_state.clear )
    {
      return;
    }
  }

  if( func->parameters.size() != function_call->arguments.size() )
  {
    Log::Error("(%d): Type ERROR: Expected %d arguments to '%s' function call, got %d\n", node->line, 
                          func->parameters.size(), func->name.c_str(), function_call->arguments.size());
    BumpError();
    return;
  }

  InferenceState state = PropagateType(function_call, TypeBuilder::GetNilType());
}

void TypeInfer::InferProcessMessage( Node::Statement* node, Node::Function* func )
{
  auto process_call = node->GetAsProcessMessage();
  auto function_call = process_call->message.get()->GetAsFunctionCall();
  // check if ready to evaluate this function call yet
  for( auto &argument : function_call->arguments )
  {
    InferenceState arg_state = PropagateType(argument.get(), TypeBuilder::GetNilType());
    if( !arg_state.clear )
    {
      return;
    }
  }

  if( func->parameters.size() != function_call->arguments.size() )
  {
    Log::Error("(%d): Type ERROR: Expected %d arguments to '%s' function call, got %d\n", node->line, 
                          func->parameters.size(), func->name.c_str(), function_call->arguments.size());
    BumpError();
    return;
  }
}

} // namespace Eople
