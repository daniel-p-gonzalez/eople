#include "eople_core.h"
#include "eople_binop.h"
#include "eople_vm.h"
#include <stdio.h>

namespace Eople
{
namespace Instruction
{

//
// Integer arithmatic.
//
bool AddI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val + op_b->int_val;
  return true;
}

bool SubI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val - op_b->int_val;
  return true;
}

bool MulI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val * op_b->int_val;
  return true;
}

bool DivI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val / op_b->int_val;
  return true;
}

bool ModI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val % op_b->int_val;
  return true;
}

//
// Floating point arithmatic.
//
bool AddF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->float_val = op_a->float_val + op_b->float_val;
  return true;
}

bool SubF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->float_val = op_a->float_val - op_b->float_val;
  return true;
}

bool MulF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->float_val = op_a->float_val * op_b->float_val;
  return true;
}

bool DivF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->float_val = op_a->float_val / op_b->float_val;
  return true;
}

//
// Bitwise operators.
//
bool ShiftLeft( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val << op_b->int_val;
  return true;
}

bool ShiftRight( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val >> op_b->int_val;
  return true;
}

bool BAnd( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val & op_b->int_val;
  return true;
}

bool BXor( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val ^ op_b->int_val;
  return true;
}

bool BOr( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->int_val = op_a->int_val | op_b->int_val;
  return true;
}

//
// String manipulation
//
bool ConcatS( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();
  Object* dest = process_ref->OperandC();

  bool aliasing_a = dest == op_a;
  bool aliasing_b = dest == op_b;
  bool aliasing = aliasing_a || aliasing_b;

  // If the destination is not the same as one of the source operands, initialize (if it's a temp object)
  if( !aliasing )
  {
    process_ref->TryInitTempString(dest);
  }

  // use much faster '+=' if destination is the same as the lhs
  if( dest->string_ref == op_a->string_ref )
  {
    *dest->string_ref += *op_b->string_ref;
  }
  else
  {
    *dest->string_ref = *op_a->string_ref + *op_b->string_ref;
  }

//  dest->string_ref.concatenate( op_a->string_ref, op_b->string_ref );

  // if operands were temporary objects, free them right away
  if( !aliasing_a )
  {
    process_ref->TryCollectTempString(op_a);
  }

  if( !aliasing_b )
  {
    process_ref->TryCollectTempString(op_b);
  }

  return true;
}

bool EqualS( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = ( op_a->string_ref == op_b->string_ref || *op_a->string_ref == *op_b->string_ref );

  // if operands were temporary objects, free them right away
  process_ref->TryCollectTempString(op_a);
  process_ref->TryCollectTempString(op_b);

  return true;
}

//
// Boolean logic
//
bool GreaterThanI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val > op_b->int_val;
  return true;
}

bool LessThanI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val < op_b->int_val;
  return true;
}

bool EqualI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val == op_b->int_val;
  return true;
}

bool NotEqualI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val != op_b->int_val;
  return true;
}

bool LessEqualI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val <= op_b->int_val;
  return true;
}

bool GreaterEqualI( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->int_val >= op_b->int_val;
  return true;
}

bool GreaterThanF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val > op_b->float_val;
  return true;
}

bool LessThanF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val < op_b->float_val;
  return true;
}

bool EqualF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val == op_b->float_val;
  return true;
}

bool NotEqualF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val != op_b->float_val;
  return true;
}

bool LessEqualF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val <= op_b->float_val;
  return true;
}

bool GreaterEqualF( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->float_val >= op_b->float_val;
  return true;
}

bool And( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->bool_val && op_b->bool_val;
  return true;
}

bool Or( process_t process_ref )
{
  Object* op_a = process_ref->OperandA();
  Object* op_b = process_ref->OperandB();

  process_ref->OperandC()->bool_val = op_a->bool_val || op_b->bool_val;
  return true;
}

bool Store( process_t process_ref )
{
  Object* dest   = process_ref->OperandA();
  Object* source = process_ref->OperandB();

  *dest = *source;

  return true;
}

bool StoreArrayElement( process_t process_ref )
{
  auto& array_ref = *process_ref->OperandA()->array_ref;
  int_t index = process_ref->OperandB()->int_val;
  Object* source = process_ref->OperandC();

  assert(index < array_ref.size());
  array_ref[index] = *source;

  return true;
}

bool StoreArrayStringElement( process_t process_ref )
{
  auto& array_ref = *process_ref->OperandA()->array_ref;
  int_t index = process_ref->OperandB()->int_val;
  Object* source = process_ref->OperandC();

  assert(index < array_ref.size());

  // if we're storing from a temp object, just steal its pointers instead of copying
  if( process_ref->IsTemporary(source) )
  {
    if( array_ref[index].string_ref )
    {
      delete array_ref[index].string_ref;
    }
    array_ref[index].string_ref = source->string_ref;
  }
  else
  {
    array_ref[index] = *source;
  }

  return true;
}

bool SpawnProcess( process_t process_ref )
{
  Object*   dest        = process_ref->OperandA();
  Function* constructor = process_ref->OperandB()->function;

  process_t new_process = process_ref->vm->GenerateUniqueProcess();
  process_ref->vm->ExecuteConstructor( CallData(constructor, new_process), process_ref );
  dest->process_ref = new_process;

  return true;
}

bool StringCopy( process_t process_ref )
{
  Object* dest   = process_ref->OperandA();
  Object* source = process_ref->OperandB();

  // if we're storing from a temp object, just steal its pointers instead of copying
  if( process_ref->IsTemporary(source) )
  {
    if( !process_ref->IsTemporary(dest) && dest->string_ref )
    {
      delete dest->string_ref;
    }
    dest->string_ref = source->string_ref;
//    dest->string_ref.move( source->string_ref );
  }
  else
  {
    process_ref->TryInitTempString(dest);
    *dest = *source;
//    dest->string_ref.set( source->string_ref );
  }

  return true;
}

} // namespace Instruction
} // namespace eople
