#include "eople_core.h"
#include "eople_opcodes.h"
#include "eople_binop.h"
#include "eople_stdlib.h"
#include "eople_vm.h"

#include <iostream>

namespace Eople
{
namespace Instruction
{

bool WhenRegister( process_t process_ref )
{
  Function* when = process_ref->OperandA()->function;

  auto when_block = WhenBlock(when);
  size_t copy_count = when->locals_count() + when->parameter_count() + when->constant_count();
  if( !when->is_repl && !when->is_constructor && copy_count )
  {
    Object* context = new Object[copy_count];
    when_block.context = SafeRange<Object>( context, context + copy_count );
    when_block.base_offset = process_ref->stack_base - process_ref->stack;
    assert( (process_ref->stack_base + when->parameters_start + when_block.context.size()) <= process_ref->stack_end );
    memcpy( when_block.context.base, process_ref->stack_base + when->parameters_start, when_block.context.size() * sizeof(Object) );
  }

  process_ref->when_blocks.push_back(when_block);
  return true;
}

bool WheneverRegister( process_t process_ref )
{
  Function* whenever = process_ref->OperandA()->function;

  auto when_block = WhenBlock(whenever);
  size_t copy_count = whenever->locals_count() + whenever->parameter_count() + whenever->constant_count();
  if( !whenever->is_repl && !whenever->is_constructor && copy_count )
  {
    Object* context = new Object[copy_count];
    when_block.context = SafeRange<Object>( context, context + copy_count );
    when_block.base_offset = process_ref->stack_base - process_ref->stack;
    assert( (process_ref->stack_base + whenever->parameters_start + when_block.context.size()) <= process_ref->stack_end );
    memcpy( when_block.context.base, process_ref->stack_base + whenever->parameters_start, when_block.context.size() * sizeof(Object) );
  }

  process_ref->whenever_blocks.push_back(when_block);
  return true;
}

bool Ready( process_t process_ref )
{
  promise_t promise = process_ref->OperandA()->promise;

  assert( process_ref->ccall_return_val < process_ref->stack_end );
  bool is_ready = promise->is_ready;

  promise_t child_promise = promise;
  if( is_ready )
  {
    while( child_promise->value.object_type == (u8)ValueType::NIL || child_promise->value.object_type == (u8)ValueType::PROMISE )
    {
      // TODO: promise types are not always propagating correctly
      if( child_promise->value.object_type == (u8)ValueType::NIL )
      {
        // is_ready = false;
        break;
      }
      child_promise = child_promise->value.promise;
      is_ready = is_ready && child_promise->is_ready;
    }
  }

  process_ref->ccall_return_val->bool_val = is_ready;

  return true;
}

bool GetValue( process_t process_ref )
{
  promise_t promise = process_ref->OperandA()->promise;

  assert( process_ref->ccall_return_val < process_ref->stack_end );
  // TODO: this is clearly a hack, and is the only reason Object::object_type exists.
  if( promise->value.object_type == (u8)ValueType::STRING )
  {
    string_t string_value = new std::string(*promise->value.string_ref);
    *process_ref->ccall_return_val = Object::GetString(string_value);
  }
  else if( promise->value.object_type == (u8)ValueType::ARRAY )
  {
    array_t array_value = new std::vector<Object>(*promise->value.array_ref);
    *process_ref->ccall_return_val = Object::GetArray(array_value);
  }
  else if( promise->value.object_type == (u8)ValueType::PROMISE )
  {
    promise_t child_promise = promise->value.promise;
    // recurse, to support chained promises
    while( child_promise->value.object_type == (u8)ValueType::PROMISE )
    {
      child_promise = child_promise->value.promise;
    }
    *process_ref->ccall_return_val = child_promise->value;
  }
  else
  {
    *process_ref->ccall_return_val = promise->value;
  }

  return true;
}

// unlike other instructions, when returns true when it has been executed, and false otherwise
bool When( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  Object* condition        = process_ref->OperandA();
  size_t condition_i_count = (size_t)process_ref->ip->b;
  size_t body_i_count      = (size_t)process_ref->ip->c;

  // skip when
  ++ip;

  const ByteCode* condition_start = ip;
  const ByteCode* condition_end   = ip + condition_i_count;
  const ByteCode* body_start      = condition_end;

  for( size_t i = 0; i < condition_i_count; ++i, ++ip )
  {
    condition_start[i].instruction(process_ref);
  }

  if( condition->bool_val )
  {
    // when body
    ip = body_start;
    for( size_t i = 0; i < body_i_count; ++i, ++ip )
    {
      body_start[i].instruction(process_ref);
    }

    ip = body_start + body_i_count;
    return true;
  }

  ip = body_start + body_i_count;
  return false;
}

bool Whenever( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  Object* condition        = process_ref->OperandA();
  size_t condition_i_count = (size_t)process_ref->ip->b;
  size_t body_i_count      = (size_t)process_ref->ip->c;

  // skip when
  ++ip;

  const ByteCode* condition_start = ip;
  const ByteCode* condition_end   = ip + condition_i_count;
  const ByteCode* body_start      = condition_end;

  for( size_t i = 0; i < condition_i_count; ++i, ++ip )
  {
    condition_start[i].instruction(process_ref);
  }

  if( condition->bool_val )
  {
    // when body
    ip = body_start;
    for( size_t i = 0; i < body_i_count; ++i, ++ip )
    {
      if( !body_start[i].instruction(process_ref) )
      {
        // 'return'ing from a whenever stops the loop
        process_ref->ccall_return_val->bool_val = false;
        break;
      }
    }

    ip = body_start + body_i_count;
    process_ref->ccall_return_val->bool_val = true;
    return true;
  }

  ip = body_start + body_i_count;
  return false;
}

bool While( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  Object* condition        = process_ref->OperandA();
  size_t condition_i_count = (size_t)process_ref->ip->b;
  size_t body_i_count      = (size_t)process_ref->ip->c;

  // skip while
  ++ip;

  const ByteCode* condition_start = ip;
  const ByteCode* condition_end   = ip + condition_i_count;
  const ByteCode* body_start      = condition_end;

  for( size_t i = 0; i < condition_i_count; ++i, ++ip )
  {
    condition_start[i].instruction(process_ref);
  }

  while( condition->bool_val )
  {
    // while body
    ip = body_start;
    for( size_t i = 0; i < body_i_count; ++i, ++ip )
    {
      body_start[i].instruction(process_ref);
    }

    // re-evaluate condition
    ip = condition_start;
    for( size_t i = 0; i < condition_i_count; ++i, ++ip )
    {
      condition_start[i].instruction(process_ref);
    }
  }

  ip = body_start + body_i_count - 1;
  return true;
}

bool ForI( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  int_t        start     = process_ref->OperandA()->int_val;
  size_t       counter_offset = process_ref->ip->a;
  const int_t  stop_value = process_ref->OperandB()->int_val;
  const int_t  step       = process_ref->OperandC()->int_val;
  const size_t i_count    = (size_t)process_ref->ip->d;

  // skip for
  ++ip;
  const ByteCode* const start_ip = process_ref->ip;
  const ByteCode* const end_ip   = start_ip + i_count;

  if( step >= 0 )
  {
    for( int_t i = start; i < stop_value; i += step )
    {
      assert( (process_ref->stack_base+counter_offset) < process_ref->stack_end );
      process_ref->stack_base[counter_offset].int_val = i;
      for( ip = start_ip; ip != end_ip; ++ip )
      {
        ip->instruction(process_ref);
      }
    }
  }
  else
  {
    for( int_t i = start; i > stop_value; i += step )
    {
      assert( (process_ref->stack_base+counter_offset) < process_ref->stack_end );
      process_ref->stack_base[counter_offset].int_val = i;
      for( ip = start_ip; ip != end_ip; ++ip )
      {
        ip->instruction(process_ref);
      }
    }
  }
  ip = end_ip - 1;

  return true;
}

bool ForF( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  float_t       start     = process_ref->OperandA()->float_val;
  size_t        counter_offset = process_ref->ip->a;
  const float_t stop_value = process_ref->OperandB()->float_val;
  const float_t step       = process_ref->OperandC()->float_val;
  const size_t  i_count    = (size_t)process_ref->ip->d;

  // skip for
  ++ip;
  const ByteCode* const start_ip = process_ref->ip;
  const ByteCode* const end_ip   = start_ip + i_count;

  if( step >= 0 )
  {
    for( float_t i = start; i < stop_value; i += step )
    {
      assert( (process_ref->stack_base+counter_offset) < process_ref->stack_end );
      process_ref->stack_base[counter_offset].float_val = i;
      for( ip = start_ip; ip != end_ip; ++ip )
      {
        ip->instruction(process_ref);
      }
    }
  }
  else
  {
    for( float_t i = start; i > stop_value; i += step )
    {
      assert( (process_ref->stack_base+counter_offset) < process_ref->stack_end );
      process_ref->stack_base[counter_offset].float_val = i;
      for( ip = start_ip; ip != end_ip; ++ip )
      {
        ip->instruction(process_ref);
      }
    }
  }
  ip = end_ip - 1;

  return true;
}

bool ForA( process_t process_ref )
{
  const ByteCode* &ip = process_ref->ip;

  array_t array   = process_ref->OperandB()->array_ref;
  size_t  element_offset = process_ref->ip->a;
  const size_t  i_count  = (size_t)process_ref->ip->d;

  // skip for
  ++ip;
  const ByteCode* const start_ip = process_ref->ip;
  const ByteCode* const end_ip   = start_ip + i_count;

  for( auto thing : *array )
  {
    assert( (process_ref->stack_base+element_offset) < process_ref->stack_end );
    process_ref->stack_base[element_offset] = thing;
    for( ip = start_ip; ip != end_ip; ++ip )
    {
      ip->instruction(process_ref);
    }
  }
  ip = end_ip - 1;

  return true;
}

bool Jump( process_t process_ref )
{
  process_ref->ip += process_ref->ip->a;

  return true;
}

bool JumpIf( process_t process_ref )
{
  // jump offsets for instruction pointer
  const i32 i_count = process_ref->ip->a;

  if( !process_ref->OperandB()->bool_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpGT( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val > process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpLT( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val < process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpEQ( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val == process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpNEQ( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val != process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpLEQ( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val <= process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool JumpGEQ( process_t process_ref )
{
  // jump offsets for instruction pointer and operand pointer
  const i32 i_count = process_ref->ip->a;

  if( process_ref->OperandB()->int_val >= process_ref->OperandC()->int_val )
  {
    process_ref->ip += i_count;
  }

  return true;
}

bool Return( process_t /*process_ref*/ )
{
  return false;
}

bool ReturnValue( process_t process_ref )
{
  assert( process_ref->stack_base < process_ref->stack_end );
  *process_ref->stack_base = *process_ref->OperandA();
  return false;
}

void CopyMessageArgs( const Function* function, process_t process_ref, Object* dest )
{
  size_t parameter_count = function->parameter_count();
  if( parameter_count )
  {
    if( parameter_count <= 2 )
    {
      if( parameter_count == 2 )
      {
        dest[0] = *process_ref->OperandC();
        dest[1] = *process_ref->OperandD();
      }
      else
      {
        dest[0] = *process_ref->OperandC();
      }
    }
    else // TODO: make sure this all makes sense
    {
      // copy args to stack
      for( size_t i = 0; (i+2) < parameter_count; )
      {
        // first operand is function object in first instruction, so skip that
        if( i == 0 )
        {
          dest[i]   = *process_ref->OperandC();
          dest[i+1] = *process_ref->OperandD();
          i+=2;
        }
        else
        {
          dest[i]   = *process_ref->OperandA();
          dest[i+1] = *process_ref->OperandB();
          dest[i+2] = *process_ref->OperandC();
          dest[i+3] = *process_ref->OperandD();
          i+=4;
        }
      }

      const size_t remainder = parameter_count % 4;
      const size_t last      = parameter_count - remainder;

      if( remainder == 3 )
      {
        dest[last]   = *process_ref->OperandA();
        dest[last+1] = *process_ref->OperandB();
        dest[last+2] = *process_ref->OperandC();
      }
      else if( remainder == 2 )
      {
        dest[last]   = *process_ref->OperandA();
        dest[last+1] = *process_ref->OperandB();
      }
      else if( remainder == 1 )
      {
        dest[last] = *process_ref->OperandA();
      }
    }
  }
}

bool ProcessMessage( process_t process_ref )
{
  process_t  other_process = process_ref->OperandA()->process_ref;
  Function* function     = process_ref->OperandB()->function;

  Object* args = new Object[function->parameter_count()];
  CopyMessageArgs( function, process_ref, args );

  promise_t promise = (function->return_type == TypeBuilder::GetNilType()) ? nullptr : new Promise(process_ref);
  process_ref->vm->SendMessage( CallData(function, other_process, args, promise) );
  process_ref->ccall_return_val->promise = promise;
  return true;
}

bool FunctionCall( process_t process_ref )
{
  const Function* const function = process_ref->OperandA()->function;

  process_ref->vm->ExecuteFunction( CallData(function, process_ref) );
  return true;
}

} // namespace Instruction
} // namespace Eople
