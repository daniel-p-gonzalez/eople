#include "eople_bytecode_gen.h"
#include "eople_control_flow.h"
#include "eople_binop.h"

#include <iostream>
#include <queue>

#define LOG_DEBUG 0
#define DUMP_CODE 0

namespace Eople
{

ByteCodeGen::ByteCodeGen()
  : m_current_module(nullptr), m_result_index((size_t)-1), m_function(nullptr),
    m_current_operand(0), m_current_temp(0), m_error_count(0),
    m_base_stack_offset(0), m_opcode_count(0)
{
}

std::unique_ptr<ExecutableModule> ByteCodeGen::InitModule( Node::Module* module )
{
  auto executable_module = std::unique_ptr<ExecutableModule>( new ExecutableModule(module) );

  size_t function_count = 0;
  size_t constructor_count = 0;
  size_t member_function_count = 0;
  size_t highest_cfunction = 0;

  for( auto &function : executable_module->module->functions )
  {
    if( function->is_c_call )
    {
      highest_cfunction = function_count + function->specializations.size()+1;
    }
    function_count += function->specializations.size()+1;
  }

  executable_module->functions.resize(function_count);

  // TODO: support native classes in some fashion
  for( auto &function : executable_module->module->classes )
  {
    constructor_count += function->specializations.size()+1;
    for( auto &member : function->functions )
    {
      member_function_count += member->specializations.size()+1;
    }
  }

  executable_module->constructors.resize(constructor_count);
  executable_module->member_functions.resize(member_function_count);
  executable_module->cfunctions.resize(highest_cfunction);

  return executable_module;
}

#define CHOOSE_OP(op) opcode = ((GetType(binary_op->left.get()) == TypeBuilder::GetPrimitiveType(ValueType::FLOAT)) ? Opcode:: op##F : Opcode:: op##I)
#define CHOOSE_OP_IF_UNSET(op) opcode = ((opcode == Opcode::NOP) ? ((GetType(binary_op->left.get()) == TypeBuilder::GetPrimitiveType(ValueType::FLOAT)) ? Opcode:: op##F : Opcode:: op##I) : opcode)
#define CHOOSE_BOOL_OP_IF_UNSET(op) opcode = ((opcode == Opcode::NOP) ? Opcode::op : opcode)

#define SYMBOL_TO_STACK(node) (m_function_node->GetStackIndex(node))

// double dispatch support
size_t ByteCodeGen::GenExpressionTerm( Node::Expression* node, bool is_root )
{
  // dispatch call to concrete type
  GenExpressionDispatcher dispatcher(this, node, is_root);
  return dispatcher.ReturnValue();
}

size_t ByteCodeGen::GenExpressionTerm( Node::Identifier* identifier, bool )
{
  // TODO: this is a kludge to get static guards off the ground
  if( identifier->is_guard )
  {
    InstructionImpl target_cfunction = GetCFunction( "is_ready", 0 );
    if( target_cfunction )
    {
      // push c function call
      PushCCall( target_cfunction );
      PushOperand(SYMBOL_TO_STACK(identifier), true);
    }
    else
    {
      Log::Error("(%d): CodeGen Error: Function not found: (Promise::is_ready)\n", identifier->line);
      BumpError();
    }
    // return values are left at the bottom of the callee stack frame / just above caller stack frame.
    return (m_function->storage_requirement+m_base_stack_offset);
  }

  return SYMBOL_TO_STACK(identifier);
}

size_t ByteCodeGen::GenExpressionTerm( Node::Literal* literal, bool )
{
  switch( literal->GetValueType()->type )
  {
    case ValueType::FLOAT:
    {
      size_t stack_index = SYMBOL_TO_STACK(literal);
      *StackObject(stack_index) = Object::GetFloat(literal->GetAsFloatLiteral()->value);
      return stack_index;
    }
    case ValueType::INT:
    {
      size_t stack_index = SYMBOL_TO_STACK(literal);
      *StackObject(stack_index) = Object::GetInt(literal->GetAsIntLiteral()->value);
      return stack_index;
    }
    case ValueType::BOOL:
    {
      size_t stack_index = SYMBOL_TO_STACK(literal);
      *StackObject(stack_index) = Object::GetBool(literal->GetAsBoolLiteral()->value);
      return stack_index;
    }
    case ValueType::STRING:
    {
      size_t stack_index = SYMBOL_TO_STACK(literal);
      *StackObject(stack_index) = Object::GetString(literal->GetAsStringLiteral()->value);
      return stack_index;
    }
    case ValueType::TYPE:
    {
      size_t stack_index = SYMBOL_TO_STACK(literal);
      *StackObject(stack_index) = Object::GetType(literal->GetAsTypeLiteral()->type);
      return stack_index;
    }
  }

  // if adding a new literal type, must handle it above
  assert(false);
  return (size_t)-1;
}

size_t ByteCodeGen::GenExpressionTerm( Node::BinaryOp* binary_op, bool is_root )
{
  Opcode opcode = Opcode::NOP;

  switch( binary_op->op )
  {
    case OpType::GT:
    {
      CHOOSE_OP(GreaterThan);
      break;
    }
    case OpType::LT:
    {
      CHOOSE_OP_IF_UNSET(LessThan);
      break;
    }
    case OpType::EQ:
    {
      if( opcode == Opcode::NOP )
      {
        switch(GetType(binary_op->left.get())->type)
        {
          case ValueType::FLOAT:  { opcode = Opcode::EqualF;    break; }
          case ValueType::INT:    { opcode = Opcode::EqualI;    break; }
          case ValueType::BOOL:   { opcode = Opcode::EqualI;    break; }
          case ValueType::STRING: { opcode = Opcode::EqualS; break; }
          default:                { opcode = Opcode::NOP;     break; }
        }
      }
      break;
    }
    case OpType::NEQ:
    {
      CHOOSE_OP_IF_UNSET(NotEqual);
      break;
    }
    case OpType::LEQ:
    {
      CHOOSE_OP_IF_UNSET(LessEqual);
      break;
    }
    case OpType::GEQ:
    {
      CHOOSE_OP_IF_UNSET(GreaterEqual);
      break;
    } // TODO: and / or need short circuit logic
    case OpType::AND:
    {
      CHOOSE_BOOL_OP_IF_UNSET(And);
      break;
    }
    case OpType::OR:
    {
      CHOOSE_BOOL_OP_IF_UNSET(Or);
      break;
    }
    case OpType::SUB:
    {
      CHOOSE_OP_IF_UNSET(Sub);
      break;
    }
    case OpType::MUL:
    {
      CHOOSE_OP_IF_UNSET(Mul);
      break;
    }
    case OpType::DIV:
    {
      CHOOSE_OP_IF_UNSET(Div);
      break;
    }
    case OpType::SHL:
    {
      opcode = (opcode == Opcode::NOP) ? Opcode::ShiftLeft : opcode;
      break;
    }
    case OpType::SHR:
    {
      opcode = (opcode == Opcode::NOP) ? Opcode::ShiftRight : opcode;
      break;
    }
    case OpType::BAND:
    {
      opcode = (opcode == Opcode::NOP) ? Opcode::BAnd : opcode;
      break;
    }
    case OpType::BXOR:
    {
      opcode = (opcode == Opcode::NOP) ? Opcode::BXor : opcode;
      break;
    }
    case OpType::BOR:
    {
      opcode = (opcode == Opcode::NOP) ? Opcode::BOr : opcode;
      break;
    }
    case OpType::ADD:
    {
      if( opcode == Opcode::NOP )
      {
        switch(GetType(binary_op->left.get())->type)
        {
          case ValueType::FLOAT:  { opcode = Opcode::AddF;    break; }
          case ValueType::INT:    { opcode = Opcode::AddI;    break; }
          case ValueType::STRING: { opcode = Opcode::ConcatS; break; }
          default:                { opcode = Opcode::NOP;     break; }
        }
      }
      break;
    }
    case OpType::MOD:
    {
      if( opcode == Opcode::NOP )
      {
        if( GetType(binary_op->left.get()) != TypeBuilder::GetPrimitiveType(ValueType::INT) )
        {
          Log::Error("(%d): Type Error: MOD operator only defined for integers.\n", binary_op->line);
          BumpError();
        }
        else
        {
          opcode = Opcode::ModI;
        }
      }
      break;
    }
    default:
    {
      Log::Error("(%d): Code Gen: Unhandled expression type. %d\n", binary_op->line, binary_op->op);
      BumpError();
      break;
    }
  }

  size_t old_current_temp = m_current_temp;
  size_t lhs_stack_index = GenExpressionTerm( binary_op->left.get(), false );
  size_t rhs_stack_index = GenExpressionTerm( binary_op->right.get(), false );
  m_current_temp = old_current_temp;
  PushOpcode( opcode );
  PushOperand(lhs_stack_index);
  PushOperand(rhs_stack_index);
  if( is_root )
  {
    PushOperand(m_result_index);
    return m_result_index;
  }

  PushOperand(m_current_temp);
  return m_current_temp++;
}

size_t ByteCodeGen::GenExpressionTerm( Node::FunctionCall* function_call, bool is_root )
{
  GenFunctionCall(function_call);
  size_t dest = is_root ? m_result_index : m_current_temp++;
  type_t op_value_type = GetType(function_call);
  if( op_value_type->type != ValueType::PROCESS )
  {
    PushOpcode( (op_value_type == TypeBuilder::GetPrimitiveType(ValueType::STRING)) ? Opcode::StringCopy : Opcode::Store );
    PushOperand(dest);
    // return values are left at the bottom of the callee stack frame / just above caller stack frame.
    PushOperand(m_function->storage_requirement+m_base_stack_offset);
  }

  return dest;
}

size_t ByteCodeGen::GenExpressionTerm( Node::ProcessMessage* process_message, bool is_root )
{
  GenProcessMessage(process_message);
  size_t dest = is_root ? m_result_index : m_current_temp++;
//    ValueType value_type = GetType(process_call);
  PushOpcode( Opcode::Store );
  PushOperand(dest);
  // return values are left at the bottom of the callee stack frame / just above caller stack frame.
  PushOperand(m_function->storage_requirement+m_base_stack_offset);
  return dest;
}

size_t ByteCodeGen::GenExpressionTerm( Node::ArrayLiteral* array_literal, bool )
{
  size_t stack_index = SYMBOL_TO_STACK(array_literal);
  Object* array_object = StackObject(stack_index);
  *array_object = Object::GetArray();
  switch( array_literal->array_type->type )
  {
    case ValueType::FLOAT:
    {
      for( auto &element : array_literal->elements )
      {
        // TODO: currently, only literals supported as elements in array literals
        assert(element->GetAsLiteral());
        array_object->array_ref->push_back(Object::GetFloat(element->GetAsFloatLiteral()->value));
      }
      break;
    }
    case ValueType::INT:
    {
      for( auto &element : array_literal->elements )
      {
        assert(element->GetAsLiteral());
        array_object->array_ref->push_back(Object::GetInt(element->GetAsIntLiteral()->value));
      }
      break;
    }
    case ValueType::BOOL:
    {
      for( auto &element : array_literal->elements )
      {
        assert(element->GetAsLiteral());
        array_object->array_ref->push_back(Object::GetBool(element->GetAsBoolLiteral()->value));
      }
      break;
    }
    case ValueType::STRING:
    {
      for( auto &element : array_literal->elements )
      {
        assert(element->GetAsLiteral());
        array_object->array_ref->push_back(Object::GetString(element->GetAsStringLiteral()->value));
      }
      break;
    }
    case ValueType::TYPE:
    {
      for( auto &element : array_literal->elements )
      {
        assert(element->GetAsLiteral());
        array_object->array_ref->push_back(Object::GetType(element->GetAsTypeLiteral()->type));
      }
      break;
    }
    default:
    {
      Log::Error("(%d): Type ERROR: Type not yet supported in array literal.\n", array_literal->line);
      BumpError();
      break;
    }
  }
  return stack_index;
}

// double dispatch support
type_t ByteCodeGen::GetType( Node::Expression* node )
{
  // dispatch call to concrete type
  GetTypeDispatcher dispatcher(this, node);
  return dispatcher.ReturnValue();
}

type_t ByteCodeGen::GetType( Node::Identifier* identifier )
{
  return m_function_node->GetExpressionType( identifier );
}

type_t ByteCodeGen::GetType( Node::Literal* literal )
{
  return literal->GetValueType();
}

type_t ByteCodeGen::GetType( Node::BinaryOp* binary_op )
{
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
      return TypeBuilder::GetBoolType();
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
      return GetType( binary_op->left.get() );
    }
  }

  // Unhandled binary op type
  assert(false);
  return TypeBuilder::GetNilType();
}

type_t ByteCodeGen::GetType( Node::FunctionCall* function_call )
{
  Function* function = GetFunction( function_call, function_call->GetSpecialization(m_function_node->current_specialization) );
  if( function )
  {
    return function->return_type;
  }
  return function_call->GetReturnType(m_function_node->current_specialization);
}

type_t ByteCodeGen::GetType( Node::ProcessMessage* process_message )
{
  auto function_call = process_message->message->GetAsFunctionCall();
  Function* function = GetFunction( function_call, function_call->GetSpecialization(m_function_node->current_specialization) );
  if( function )
  {
    return function->return_type;
  }
  return function_call->GetReturnType(m_function_node->current_specialization);
}

size_t ByteCodeGen::GenForInit( Node::ForInit* node )
{
  size_t counter = SYMBOL_TO_STACK(node->counter->GetAsIdentifier());
  size_t start = GenExpressionTerm( node->start.get(), false );
  size_t end = node->end ? GenExpressionTerm( node->end.get(), false ) : 0;
  size_t step = node->step ? GenExpressionTerm( node->step.get(), false ) : 0;

  type_t start_type = GetType(node->start.get());
  if( start_type->type == ValueType::ARRAY )
  {
    size_t for_index = PushOpcode( Opcode::ForA );
    PushOperand(counter); // element
    PushOperand(start);   // array

    return for_index;
  }

  type_t counter_type = GetType(node->counter.get());
  PushOpcode( Opcode::Store );
  PushOperand(counter);
  PushOperand(start);

  size_t for_index = PushOpcode( counter_type == TypeBuilder::GetPrimitiveType(ValueType::FLOAT) ? Opcode::ForF : Opcode::ForI );
  PushOperand(counter);
  PushOperand(end);
  PushOperand(step);

  return for_index;
}

#define OPCODE_CASE(op)     case op: \
                            { \
                              m_function->code.push_back(ByteCode(op)); \
                              break; \
                            }

size_t ByteCodeGen::PushOpcode( Opcode opcode )
{
  switch(opcode)
  {
    OPCODE_CASE(Opcode::ForI)
    OPCODE_CASE(Opcode::ForF)
    OPCODE_CASE(Opcode::ForA)
    OPCODE_CASE(Opcode::While)
    OPCODE_CASE(Opcode::WhenRegister)
    OPCODE_CASE(Opcode::WheneverRegister)
    OPCODE_CASE(Opcode::When)
    OPCODE_CASE(Opcode::Whenever)
    OPCODE_CASE(Opcode::Jump)
    OPCODE_CASE(Opcode::JumpIf)
    OPCODE_CASE(Opcode::JumpGT)
    OPCODE_CASE(Opcode::JumpLT)
    OPCODE_CASE(Opcode::JumpEQ)
    OPCODE_CASE(Opcode::JumpNEQ)
    OPCODE_CASE(Opcode::JumpLEQ)
    OPCODE_CASE(Opcode::JumpGEQ)
    OPCODE_CASE(Opcode::AddI)
    OPCODE_CASE(Opcode::SubI)
    OPCODE_CASE(Opcode::DivI)
    OPCODE_CASE(Opcode::MulI)
    OPCODE_CASE(Opcode::ModI)
    OPCODE_CASE(Opcode::AddF)
    OPCODE_CASE(Opcode::SubF)
    OPCODE_CASE(Opcode::DivF)
    OPCODE_CASE(Opcode::MulF)
    OPCODE_CASE(Opcode::ShiftLeft)
    OPCODE_CASE(Opcode::ShiftRight)
    OPCODE_CASE(Opcode::BAnd)
    OPCODE_CASE(Opcode::BXor)
    OPCODE_CASE(Opcode::BOr)
    OPCODE_CASE(Opcode::ConcatS)
    OPCODE_CASE(Opcode::EqualS)
    OPCODE_CASE(Opcode::GreaterThanI)
    OPCODE_CASE(Opcode::LessThanI)
    OPCODE_CASE(Opcode::EqualI)
    OPCODE_CASE(Opcode::NotEqualI)
    OPCODE_CASE(Opcode::LessEqualI)
    OPCODE_CASE(Opcode::GreaterEqualI)
    OPCODE_CASE(Opcode::GreaterThanF)
    OPCODE_CASE(Opcode::LessThanF)
    OPCODE_CASE(Opcode::EqualF)
    OPCODE_CASE(Opcode::NotEqualF)
    OPCODE_CASE(Opcode::LessEqualF)
    OPCODE_CASE(Opcode::GreaterEqualF)
    OPCODE_CASE(Opcode::And)
    OPCODE_CASE(Opcode::Or)
    OPCODE_CASE(Opcode::Store)
    OPCODE_CASE(Opcode::StringCopy)
    OPCODE_CASE(Opcode::SpawnProcess)
    OPCODE_CASE(Opcode::PrintI)
    OPCODE_CASE(Opcode::PrintF)
    OPCODE_CASE(Opcode::FunctionCall)
    OPCODE_CASE(Opcode::ProcessMessage)
    OPCODE_CASE(Opcode::Return)
    OPCODE_CASE(Opcode::ReturnValue)
    OPCODE_CASE(Opcode::NOP)
    default:
    {
      Log::Error("Unhandled opcode. Get to work! (%d)\n", opcode);
      BumpError();
      break;
    }
  }

  m_current_operand = 0;
  ++m_opcode_count;
  return m_function->code.size() - 1;
}

size_t ByteCodeGen::PushCCall( InstructionImpl cfunction )
{
  // TODO: bubble cfunction name up to here
  m_function->code.push_back(ByteCode(cfunction));
  m_current_operand = 0;
  ++m_opcode_count;
  return m_function->code.size() - 1;
}

// allow_new_op will allow creation of empty ops to carry arguments that won't fit into a single instruction
void ByteCodeGen::PushOperand( size_t operand, bool allow_new_op )
{
  size_t last_op = m_function->code.size() - 1;

  if( m_current_operand > 3 && allow_new_op )
  {
    PushOpcode( Opcode::NOP );
    last_op = m_function->code.size() - 1;
  }

  switch( m_current_operand )
  {
    case 0:
    {
      m_function->code[last_op].a = (Operand)operand;
      break;
    }
    case 1:
    {
      m_function->code[last_op].b = (Operand)operand;
      break;
    }
    case 2:
    {
      m_function->code[last_op].c = (Operand)operand;
      break;
    }
    case 3:
    {
      m_function->code[last_op].d = (Operand)operand;
      break;
    }
    default:
    {
      // no single instruction can have more than 4 operands
      assert(false);
      break;
    }
  }
  ++m_current_operand;
}

Object* ByteCodeGen::StackObject( size_t index )
{
  // convert index into local stack offset
  index -= m_base_stack_offset;

  // convert local stack offset to constants offset (taking into account parameters and 'this')
  size_t offset = index - m_function->parameter_count() - (m_function->is_constructor ? 1 : 0);

  assert(offset < m_function->constant_count());
  return &m_function->constants[offset];
}

void ByteCodeGen::GenFunctionCall( Node::FunctionCall* node )
{
  // evaluate args
  size_t arg_count = node->arguments.size();
  std::vector<size_t> arg_indices;
  for( size_t i = 0; i < arg_count; ++i )
  {
    auto arg_type = GetType(node->arguments[i].get());
    if( arg_type == TypeBuilder::GetNilType() )
    {
      Log::Error("(%d): CodeGen Error: Function call argument has nil type.\n", node->line);
      BumpError();
      return;
    }
    size_t arg_index = GenExpressionTerm( node->arguments[i].get(), false );
    arg_indices.push_back(arg_index);
  }

  // get stack index for call
  size_t call_index = SYMBOL_TO_STACK(node);

  // find function
  std::string func_name = node->GetName();
  Function* target_function = GetFunction( node, node->GetSpecialization(m_function_node->current_specialization) );
  if( target_function )
  {
    StackObject(call_index)->function = target_function;
    // is this a constructor?
    if( target_function->return_type->type == ValueType::PROCESS )
    {
      PushOpcode( Opcode::SpawnProcess );
      PushOperand(m_result_index);
      PushOperand(call_index);
      // now push args
      for( auto arg : arg_indices )
      {
        PushOperand(arg, true);
      }
    }
    else
    {
      PushOpcode( Opcode::FunctionCall );
      PushOperand(call_index);
      // now push args
      for( auto arg : arg_indices )
      {
        PushOperand(arg, true);
      }
    }
  }
  else
  {
    InstructionImpl target_cfunction = GetCFunction( func_name, node->GetSpecialization(m_function_node->current_specialization) );
    if( target_cfunction )
    {
      // push c function call
      PushCCall( target_cfunction );
      // now push args
      for( auto arg : arg_indices )
      {
        PushOperand(arg, true);
      }
    }
    else
    {
      Log::Error("(%d): CodeGen Error: Function not found: (%s)\n", node->line, func_name.c_str());
      BumpError();
    }
  }
}

void ByteCodeGen::GenProcessMessage( Node::ProcessMessage* call_node )
{
  auto node = call_node->message->GetAsFunctionCall();

  // get stack index for call
  size_t call_index = SYMBOL_TO_STACK(call_node);
  size_t process_index = SYMBOL_TO_STACK(call_node->process->GetAsIdentifier());

  // evaluate args
  size_t arg_count = node->arguments.size();
  std::vector<size_t> arg_indices;
  for( size_t i = 0; i < arg_count; ++i )
  {
    size_t arg_index = GenExpressionTerm( node->arguments[i].get(), false );
    arg_indices.push_back(arg_index);
  }

  // find member function
  std::string func_name = node->GetName();
  Function* target_function = GetMemberFunction( call_node, node->GetSpecialization(m_function_node->current_specialization) );
  if( target_function )
  {
    StackObject(call_index)->function = target_function;
    // push call
    PushOpcode( Opcode::ProcessMessage );
    PushOperand(process_index);
    PushOperand(call_index);
    // now push args
    for( auto arg : arg_indices )
    {
      PushOperand(arg, true);
    }
    //auto ident = call_node->process->GetAsIdentifier();
    //TableEntry* entry = m_function_node->GetTableEntry(ident);
    //std::string &process_name = entry->target_name;
  }
  else
  {
    auto ident = call_node->process->GetAsIdentifier();
    TableEntry* entry = m_function_node->GetTableEntry(ident);
    if( entry )
    {
      std::string &process_name = entry->target_name;
      Log::Error("(%d): CodeGen Error: Member function not found: %s::%s\n", node->line, process_name.c_str(), func_name.c_str());
      BumpError();
    }
    else
    {
      Log::Error("(%d): CodeGen Error: Member function not found: (symbol missing)::%s\n", node->line, func_name.c_str());
      BumpError();
    }
  }
}

template <class T>
Function* ByteCodeGen::GenWhen( T when_node, Opcode opcode )
{
  Function* when_eval = new Function();

  Function* temp          = m_function;
  size_t temp_base        = m_base_stack_offset;
  size_t old_first_temp   = m_first_temp;
  size_t old_current_temp = m_current_temp;
  size_t old_opcount      = m_opcode_count;

//  m_base_stack_offset = 0;

  Function* outer = m_function;
  m_function = when_eval;

  auto ident = when_node->ident->GetAsIdentifier();
  m_function->name                = ident->name;
  m_function->parameters_start    = outer->parameters_start;
  m_function->constants_start     = outer->constants_start;
  m_function->locals_start        = outer->locals_start;
  m_function->temp_start          = outer->temp_start;
  m_function->temp_end            = outer->temp_end;
  m_function->storage_requirement = outer->storage_requirement;
  m_function->is_constructor      = outer->is_constructor;
  m_function->is_repl             = outer->is_repl;
  m_function->constants           = outer->constants;
  m_function->is_when_eval        = true;

  // "when" statement runs within outer context, like methods
  m_function->is_method           = true;

  m_first_temp = m_current_temp = m_function->temp_start;

  size_t when_id = PushOpcode( opcode );
  size_t pre_opcount = m_opcode_count;
  size_t expr_id = GenExpressionTerm(when_node->condition.get(), false);
  size_t condition_opcount = m_opcode_count - pre_opcount;

  pre_opcount = m_opcode_count;

  // TODO: keep track of whether we branch, to allow for some loop optimizations
  GenStatements( when_node->body );

  // TODO: need to clean this up. want execution of this block to overlay the function it exists in.
  if( m_function_node->is_constructor || outer->is_repl )
  {
    m_function->storage_requirement = 0;
  }

  size_t body_opcount = m_opcode_count - pre_opcount;
  // TODO: handle overflow
  m_function->code[when_id].a = (Operand)expr_id;
  m_function->code[when_id].b = (Operand)condition_opcount;
  m_function->code[when_id].c = (Operand)body_opcount;

  m_first_temp = old_first_temp;
  m_current_temp = old_current_temp;
  m_opcode_count = old_opcount;
  m_base_stack_offset = temp_base;
  m_function = temp;

  m_current_module->functions.push_back(std::unique_ptr<Function>(when_eval));
  return when_eval;
}

// double dispatch support
void ByteCodeGen::GenStatement( Node::Statement* statement )
{
  // dispatch call to concrete type
  GenStatementDispatcher dispatcher(this, statement);
}

void ByteCodeGen::GenStatements( std::vector<StatementNode> &nodes )
{
  for( auto &node : nodes )
  {
    GenStatement(node.get());
  }
}

void ByteCodeGen::GenStatement( Node::Return* return_statement )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  if( return_statement->return_value )
  {
    size_t expr_index = GenExpressionTerm( return_statement->return_value.get(), false );
    PushOpcode( Opcode::ReturnValue );
    PushOperand(expr_index);
  }
  else
  {
    PushOpcode( Opcode::Return );
  }
}

void ByteCodeGen::GenStatement( Node::Assignment* assignment )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  auto left_ident = assignment->left->GetAsIdentifier();
  m_result_index = SYMBOL_TO_STACK(left_ident);
  size_t rhs = GenExpressionTerm( assignment->right.get(), true );
  bool rhs_is_func_call = rhs == (m_function->storage_requirement+m_base_stack_offset);
  // check for simple assignment
  if( rhs != m_result_index && (rhs < m_first_temp || rhs_is_func_call) )
  {
    type_t op_value_type = GetType(left_ident);
    PushOpcode( (op_value_type == TypeBuilder::GetPrimitiveType(ValueType::STRING)) ? Opcode::StringCopy : Opcode::Store );
    PushOperand(m_result_index);
    PushOperand(rhs);
  }
}

void ByteCodeGen::GenStatement( Node::For* for_loop )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  auto for_init = for_loop->init->GetAsForInit();
  size_t for_index = GenForInit(for_init);
  // in case this is a nested loop, start opcount from current
  size_t pre_opcount = m_opcode_count;

  // TODO: keep track of whether we branch, to allow for some loop optimizations
  GenStatements( for_loop->body );

  size_t opcount = m_opcode_count - pre_opcount;
  // TODO: handle overflow
  m_function->code[for_index].d = (u16)opcount;
}

void ByteCodeGen::GenStatement( Node::While* while_loop )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  size_t while_id = PushOpcode( Opcode::While );
  size_t pre_opcount = m_opcode_count;
  size_t expr_id = GenExpressionTerm(while_loop->condition.get(), false);
  size_t condition_opcount = m_opcode_count - pre_opcount;

  pre_opcount = m_opcode_count;

  // TODO: keep track of whether we branch, to allow for some loop optimizations
  GenStatements( while_loop->body );

  size_t body_opcount = m_opcode_count - pre_opcount;
  // TODO: handle overflow
  m_function->code[while_id].a = (Operand)expr_id;
  m_function->code[while_id].b = (Operand)condition_opcount;
  m_function->code[while_id].c = (Operand)body_opcount;
}

void ByteCodeGen::GenStatement( Node::When* when )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  Function* when_eval = GenWhen(when, Opcode::When);
  PushOpcode( Opcode::WhenRegister );

  size_t stack_index = SYMBOL_TO_STACK(when->ident.get());
  *StackObject( stack_index ) = Object::GetFunction(when_eval);
  PushOperand( stack_index );
}

void ByteCodeGen::GenStatement( Node::Whenever* whenever )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  Function* whenever_eval = GenWhen(whenever, Opcode::Whenever);
  PushOpcode( Opcode::WheneverRegister );

  size_t stack_index = SYMBOL_TO_STACK(whenever->ident.get());
  *StackObject( stack_index ) = Object::GetFunction(whenever_eval);
  PushOperand( stack_index );
}

void ByteCodeGen::GenStatement( Node::If* if_statement )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  size_t expr_id = GenExpressionTerm(if_statement->condition.get(), false);
  size_t last_op = m_function->code.size()-1;
  bool less_than = m_function->code[last_op].instruction == Instruction::LessThanI;
  size_t last_jump_id = less_than ? last_op : PushOpcode( Opcode::JumpIf );
  if( !less_than )
  {
    size_t jump_if_op = m_function->code.size() - 1;
    m_function->code[jump_if_op].b = (Operand)expr_id;
  }
  else
  {
    // shift over operands (op a is jump offset in Jump* instructions)
    m_function->code[last_op].c = m_function->code[last_op].b;
    m_function->code[last_op].b = m_function->code[last_op].a;
    m_function->code[last_op].instruction = Instruction::JumpGEQ;
  }
  // in case this is a nested if statement, start opcount from current
  size_t pre_op_count = m_opcode_count;

  GenStatements( if_statement->body );

  size_t op_count = m_opcode_count - pre_op_count;

  m_function->code[last_jump_id].a = (Operand)op_count;

  // keep track of all unconditional jumps to end of if statement, so the jump dest can be updated
  std::vector<size_t> jumps_to_end;

  // TODO: seriously in need of refactor

  // elseif blocks?
  for( auto &elseif_node : if_statement->elseifs )
  {
    auto elseif = elseif_node->GetAsElseIf();
    // for determining distance to jump-to-end for previous blocks
    size_t end_pre_op_count = m_opcode_count;

    // tack an unconditional jump-to-end onto previous (successful) block, so we jump over remaining blocks
    size_t end_jump_id = PushOpcode( Opcode::Jump );
    m_function->code[end_jump_id].a   = 0;
    m_function->code[end_jump_id].b = 0;

    jumps_to_end.push_back( end_jump_id );
    // want last condition failing jump to go over unconditional jump-to-end, so we end up on the next condition op
    ++m_function->code[last_jump_id].a;

    size_t local_pre_op_count = m_opcode_count;

    size_t expr_id = GenExpressionTerm(elseif->condition.get(), false);
    size_t last_op = m_function->code.size()-1;
    bool less_than = m_function->code[last_op].instruction == Instruction::LessThanI;
    last_jump_id = less_than ? last_op : PushOpcode( Opcode::JumpIf );
    if( !less_than )
    {
      m_function->code[last_jump_id].b = (Operand)expr_id;
    }
    else
    {
      // shift over operands (op a is jump offset in Jump* instructions)
      m_function->code[last_op].c = m_function->code[last_op].b;
      m_function->code[last_op].b = m_function->code[last_op].a;
      m_function->code[last_op].instruction = Instruction::JumpGEQ;
    }

    size_t pre_op_count = m_opcode_count;

    GenStatements( elseif->body );

    size_t op_count       = m_opcode_count - pre_op_count;
    size_t end_op_count   = m_opcode_count - end_pre_op_count;
    size_t local_op_count = m_opcode_count - local_pre_op_count;

    m_function->code[last_jump_id].a = (Operand)op_count;

    // update jump-to-end jumps to point to the (current) end
    for( auto jump_id : jumps_to_end )
    {
      if( end_jump_id == jump_id )
      {
        m_function->code[jump_id].a += (Operand)local_op_count;
      }
      else
      {
        m_function->code[jump_id].a += (Operand)end_op_count;
      }
    }
  }

  // else block?
  if( !if_statement->else_body.empty() )
  {
    // for determining distance to jump-to-end for previous blocks
    size_t end_pre_op_count = m_opcode_count;

    // tack an unconditional jump-to-end onto previous (successful) block, so we jump over remaining blocks
    size_t end_jump_id = PushOpcode( Opcode::Jump );
    m_function->code[end_jump_id].a = 0;

    jumps_to_end.push_back( end_jump_id );
    // want last condition failing jump to go over unconditional jump-to-end, so we end up on the next condition op
    ++m_function->code[last_jump_id].a;

    size_t pre_op_count = m_opcode_count;

    GenStatements( if_statement->else_body );

    size_t op_count     = m_opcode_count - pre_op_count;
    size_t end_op_count = m_opcode_count - end_pre_op_count;

    // update jump-to-end jumps to point to the end
    for( auto jump_id : jumps_to_end )
    {
      if( end_jump_id == jump_id )
      {
        m_function->code[jump_id].a += (Operand)op_count;
      }
      else
      {
        m_function->code[jump_id].a += (Operand)end_op_count;
      }
    }
  }
}

void ByteCodeGen::GenStatement( Node::FunctionCall* function_call )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  // throwing away any return value
  m_result_index = m_first_temp;
  GenFunctionCall(function_call);
}

void ByteCodeGen::GenStatement( Node::ProcessMessage* process_message )
{
  m_result_index = (size_t)-1;
  m_current_temp = m_first_temp;

  GenProcessMessage(process_message);
}

void ByteCodeGen::ImportCFunctions( ExecutableModule* executable_module, std::vector<std::vector<InstructionImpl>> &cfunctions )
{
  size_t function_count = 0;
  for( size_t i = 0; i < executable_module->module->functions.size(); ++i )
  {
    auto &function = executable_module->module->functions[i];
    // TODO: is support for module mixing c calls and eople code within the same module needed?
    //       mixing will only work under the assumption that c functions are added before eople functions...
    if( !function->is_c_call )
    {
      break;
    }
    for( auto cfunc : cfunctions[i] )
    {
      executable_module->cfunctions[function_count++] = cfunc;
    }
  }
}

void ByteCodeGen::GenFunction( Node::Function* node )
{
  // skip function templates
  // TODO: do proper check for function templates
  if( !node->is_c_call && m_function_node->current_specialization == 0 && m_function_node->specializations.size() )
  {
    return;
  }

  m_first_temp   = m_function->temp_start;
  m_current_temp = m_first_temp;

  GenStatements( node->body );

  // if there isn't an explicit return, add one
  if( !m_function->code.size() || (m_function->code[m_function->code.size()-1].instruction != Instruction::Return
                             &&  m_function->code[m_function->code.size()-1].instruction != Instruction::ReturnValue))
  {
    m_function->code.push_back(ByteCode(Opcode::Return));
  }

  #if DUMP_CODE == 1
    Eople::Log::Print("def %s\n", m_function->name.c_str());
    for( auto instr : m_function->code )
    {
      Eople::Log::Print("  %s, %d, %d, %d, %d\n", InstructionToString(instr.instruction).c_str(),
                                              instr.a, instr.b, instr.c, instr.d);
    }
    Eople::Log::Print("end\n");
  #endif
}

void ByteCodeGen::InitFunction( Node::Function* node )
{
  // update symbol count in case we generated multiple specializations of a function call within the same function
  node->symbol_count = node->current_specialization ? node->specializations[node->current_specialization-1].symbol_count() :
                                                      node->symbols.symbol_count();
  size_t constant_count = node->current_specialization ? node->specializations[node->current_specialization-1].constant_count() :
                                                      node->symbols.constant_count();

  m_function->name = node->name;

  // [this][parameters][constants][locals][temp]
  u32 this_ref = (node->is_constructor ? 1 : 0);

  m_function->parameters_start = (u32)m_base_stack_offset + (u32)this_ref;
  u32 parameter_count = (u32)node->parameters.size();

  m_function->constants_start = m_function->parameters_start + parameter_count;
  // 'this' and parameters are treated as constants during parse, but they are differentiated here
  constant_count -= parameter_count + this_ref;

  m_function->locals_start = m_function->constants_start + (u32)constant_count;
  u32 locals_count = ((u32)node->symbol_count - parameter_count - (u32)constant_count - this_ref);

  m_function->temp_start = m_function->locals_start + locals_count;
  m_function->temp_end = m_function->temp_start + (u32)node->temp_count;

  m_function->storage_requirement = (u32)node->symbol_count + (u32)node->temp_count;
  m_function->constants = new Object[constant_count];
  memset( m_function->constants, 0, sizeof(Object) * constant_count );
  m_function->return_type = m_function_node->GetValueType();
}

Function* ByteCodeGen::GetConstructor( Node::Function* node, size_t specialization )
{
  std::queue<ExecutableModule*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto executable_module = modules.front(); modules.pop();

    size_t function_count = 0;
    for( auto &function : executable_module->module->classes )
    {
      if( !function->is_c_call )
      {
        if( function->name == node->name )
        {
          return executable_module->constructors[function_count + specialization].get();
        }
        function_count += function->specializations.size()+1;
      }
    }
    // couldn't find constructor - search imported modules
    for( auto imported_module : executable_module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

Function* ByteCodeGen::GetFunction( Node::FunctionCall* node, size_t specialization )
{
  std::queue<ExecutableModule*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto executable_module = modules.front(); modules.pop();

    // TODO: not really properly checking namespaces here...
    bool is_local_module = true; //m_current_module == executable_module;

    Function* best_match = nullptr;
    size_t best_match_depth = 0;
    size_t function_count = 0;
    for( auto &function : executable_module->module->functions )
    {
      if( !function->is_c_call )
      {
        for( size_t i = best_match_depth; i < node->namespace_stack.size(); ++i )
        {
          if( function->name == node->namespace_stack[i] + node->GetName() )
          {
            if( i == (node->namespace_stack.size()-1) )
            {
              // closest possible match
              return executable_module->functions[function_count + specialization].get();
            }
            best_match = executable_module->functions[function_count + specialization].get();
            best_match_depth = i;
          }
        }
        // is it in local module scope?
        if( !best_match_depth && !best_match && is_local_module && function->name == node->GetName() )
        {
          best_match = executable_module->functions[function_count + specialization].get();
          best_match_depth = 0;
        }
        function_count += function->specializations.size()+1;
      }
    }

    function_count = 0;
    // maybe it's a constructor?
    if( !best_match )
    {
      for( auto &function : executable_module->module->classes )
      {
        if( !function->is_c_call )
        {
          for( size_t i = best_match_depth; i < node->namespace_stack.size(); ++i )
          {
            if( function->name == node->namespace_stack[i] + node->GetName() )
            {
              if( i == (node->namespace_stack.size()-1) )
              {
                // closest possible match
                return executable_module->constructors[function_count + specialization].get();
              }
              best_match = executable_module->constructors[function_count + specialization].get();
              best_match_depth = i;
            }
          }
          // is it in local module scope?
          if( !best_match_depth && !best_match && is_local_module && function->name == node->GetName() )
          {
            best_match = executable_module->constructors[function_count + specialization].get();
            best_match_depth = 0;
          }
          function_count += function->specializations.size()+1;
        }
      }
    }
    if( best_match )
    {
      return best_match;
    }
    // couldn't find function - search imported modules
    for( auto imported_module : executable_module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

Function* ByteCodeGen::GetMemberFunction( Node::ProcessMessage* call_node, size_t specialization )
{
  auto node = call_node->message->GetAsFunctionCall();
  auto ident = call_node->process->GetAsIdentifier();
  TableEntry* entry = m_function_node->GetTableEntry(ident);
  if( !entry )
  {
    Log::Error("(%d): CodeGen Error: Class type missing for: %s in call: %s->%s\n", node->line, ident->name.c_str(),
                                        ident->name.c_str(), node->GetName().c_str());
    BumpError();
    return nullptr;
  }

  std::string &process_name = entry->target_name;

  std::string full_name = process_name + "::" + node->GetName();

  std::queue<ExecutableModule*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto executable_module = modules.front(); modules.pop();

    for( size_t i = 0; i < executable_module->member_functions.size(); ++i )
    {
      if( executable_module->member_functions[i]->name == full_name )
      {
        return executable_module->member_functions[i + specialization].get();
      }
    }
    // couldn't find member function - search imported modules
    for( auto imported_module : executable_module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

InstructionImpl ByteCodeGen::GetCFunction( std::string name, size_t specialization )
{
  std::queue<ExecutableModule*> modules;
  modules.push(m_current_module);

  while( !modules.empty() )
  {
    auto executable_module = modules.front(); modules.pop();

    size_t function_count = 0;
    for( size_t i = 0; i < executable_module->module->functions.size(); ++i )
    {
      auto &function = executable_module->module->functions[i];
      if( function->is_c_call )
      {
        if( name == function->name )
        {
          return executable_module->cfunctions[function_count + specialization];
        }
        function_count += function->specializations.size()+1;
      }
    }
    // couldn't find function - search imported modules
    for( auto imported_module : executable_module->imported )
    {
      modules.push(imported_module);
    }
  }

  return nullptr;
}

void ByteCodeGen::GenModule( ExecutableModule* module )
{
#if LOG_DEBUG == 1
  Log::PopContext();
  Log::AutoFile ir_logger(module->module->name + ".eir");
#endif
  m_base_stack_offset = 0;
  m_current_module = module;
  m_function      = nullptr;
  m_function_node = nullptr;

  size_t function_count = 0;
  size_t member_count = 0;
  // pre-initialize generated functions for lazy function call binding
  for( auto &function : m_current_module->module->functions )
  {
    m_function_node = function.get();
    if( !function->is_c_call )
    {
      for( size_t s = 0; s < function->specializations.size()+1; ++s )
      {
        m_current_module->functions[function_count] = std::unique_ptr<Function>(new Function());
        m_function = m_current_module->functions[function_count++].get();
        function->current_specialization = s;
        if( function->is_template[s] )
        {
          continue;
        }
        InitFunction( function.get() );
        // hack to get when/whenever statements working with persistant vars in repl
        m_function->is_repl = m_function_node->is_repl;
      }
    }
  }

  function_count = 0;
  member_count = 0;
  // pre-initialize generated constructors and member functions
  for( auto &function : m_current_module->module->classes )
  {
    m_base_stack_offset = 0;
    m_function_node = function.get();
    if( !function->is_c_call )
    {
      for( size_t s = 0; s < function->specializations.size()+1; ++s )
      {
        m_current_module->constructors[function_count] = std::unique_ptr<Function>(new Function());
        m_function = m_current_module->constructors[function_count++].get();
        function->current_specialization = s;
        if( function->is_template[s] )
        {
          continue;
        }
        InitFunction( function.get() );
        m_function->is_constructor = true;
      }
      m_base_stack_offset = m_function->storage_requirement - m_function_node->temp_count;
      for( auto &member : function->functions )
      {
        m_function_node = member.get();
        for( size_t s = 0; s < member->specializations.size()+1; ++s )
        {
          m_current_module->member_functions[member_count] = std::unique_ptr<Function>(new Function());
          m_function = m_current_module->member_functions[member_count++].get();
          member->current_specialization = s;
          if( member->is_template[s] )
          {
            continue;
          }
          InitFunction( member.get() );
          m_function->is_method = true;
        }
      }
    }
  }

  function_count = 0;
  m_base_stack_offset = 0;
  for( auto &function : m_current_module->module->functions )
  {
    m_function_node = function.get();
    if( !function->is_c_call )
    {
      for( size_t s = 0; s < function->specializations.size()+1; ++s )
      {
        m_function = m_current_module->functions[function_count++].get();
        function->current_specialization = s;
        if( function->is_template[s] )
        {
          continue;
        }
        GenFunction( function.get() );
      }
    }
  }

  function_count = 0;
  member_count = 0;
  for( auto &function : m_current_module->module->classes )
  {
    m_base_stack_offset = 0;
    m_function_node = function.get();
    if( !function->is_c_call )
    {
      for( size_t s = 0; s < function->specializations.size()+1; ++s )
      {
        m_function = m_current_module->constructors[function_count++].get();
        function->current_specialization = s;
        if( function->is_template[s] )
        {
          continue;
        }
        GenFunction( function.get() );
      }
      m_base_stack_offset = m_function->storage_requirement - m_function_node->temp_count;
      for( auto &member : function->functions )
      {
        m_function_node = member.get();
        for( size_t s = 0; s < member->specializations.size()+1; ++s )
        {
          m_function = m_current_module->member_functions[member_count++].get();
          member->current_specialization = s;
          if( member->is_template[s] )
          {
            continue;
          }
          GenFunction( member.get() );
        }
      }
    }
  }
}

} // namespace Eople
