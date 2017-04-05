#include "eople_core.h"
#include "eople_stdlib.h"
#include "eople_vm.h"

#include <iostream>
#include <cstdio>
#include <thread>
#include <sstream>

namespace Eople
{
namespace Instruction
{

bool PrintI( process_t process_ref )
{
  Object* result = process_ref->OperandA();
  Log::Print("%d\n", result->int_val);

  return true;
}

bool PrintF( process_t process_ref )
{
  Object* result = process_ref->OperandA();
  Log::Print("%f\n", result->float_val);

  return true;
}

bool PrintS( process_t process_ref )
{
  Object* text = process_ref->OperandA();
  Log::Print("%s\n", text->string_ref->c_str());

  process_ref->TryCollectTempString(text);

  return true;
}

//bool GetLine( process_t process_ref )
//{
//  promise_t &promise = process_ref->ccall_return_val->promise;
//  promise = new Promise(process_ref);
//  promise->value.string_ref = new std::string();
//  string_t &new_string = promise->value.string_ref;
//  ...
//
//  return true;
//}

bool GetLine( process_t process_ref )
{
  string_t &new_string = process_ref->ccall_return_val->string_ref;
  new_string = new std::string();

  std::cin >> *new_string;
  //u8 next_char = (u8)getchar();
  //while( next_char != '\n' )
  //{
  //  *new_string += next_char;
  //  next_char = (u8)getchar();
  //}

  return true;
}

bool ArrayConstructor( process_t process_ref )
{
  process_ref->ccall_return_val->array_ref = new std::vector<Object>();

  return true;
}

bool ArrayPush( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  array_ref->push_back(*object);

  return true;
}

bool ArrayPushArray( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  // push a copy
  array_t array_value = new std::vector<Object>(*object->array_ref);
  array_ref->push_back(Object::GetArray(array_value));

  return true;
}

bool ArrayPushString( process_t process_ref )
{
  auto array_ref = process_ref->OperandA()->array_ref;
  Object* object = process_ref->OperandB();

  // push a copy
  string_t string_value = new std::string(*object->string_ref);
  array_ref->push_back(Object::GetString(string_value));

  return true;
}

bool ArraySize( process_t process_ref )
{
  process_ref->ccall_return_val->int_val = process_ref->OperandA()->array_ref->size();

  return true;
}

bool ArrayTop( process_t process_ref )
{
  *process_ref->ccall_return_val = process_ref->OperandA()->array_ref->back();

  return true;
}

bool ArrayTopArray( process_t process_ref )
{
  Object &object = process_ref->OperandA()->array_ref->back();

  // return a copy
  array_t array_value = new std::vector<Object>(*object.array_ref);
  *process_ref->ccall_return_val = Object::GetArray(array_value);

  return true;
}

bool ArrayTopString( process_t process_ref )
{
  Object &object = process_ref->OperandA()->array_ref->back();

  // return a copy
  string_t string_value = new std::string(*object.string_ref);
  *process_ref->ccall_return_val = Object::GetString(string_value);

  return true;
}

bool ArrayPop( process_t process_ref )
{
  process_ref->OperandA()->array_ref->pop_back();

  return true;
}

bool ArrayClear( process_t process_ref )
{
  process_ref->OperandA()->array_ref->clear();

  return true;
}

bool Timer( process_t process_ref )
{
  Object* duration = process_ref->OperandA();

  auto future_time = HighResClock::now() + std::chrono::milliseconds(duration->int_val);
  promise_t promise = new Promise(process_ref);
  promise->is_timer = true;
  process_ref->vm->SendMessage( CallData(nullptr, process_ref, nullptr, promise, future_time) );
  process_ref->ccall_return_val->promise = promise;
  return true;
}

bool GetTime( process_t process_ref )
{
  auto now = HighResClock::now().time_since_epoch();
  process_ref->ccall_return_val->float_val = (float_t)(std::chrono::duration_cast<std::chrono::microseconds>(now).count())/1000000.0;

  return true;
}

bool SleepMilliseconds( process_t process_ref )
{
  Object* duration = process_ref->OperandA();

  std::chrono::milliseconds duration_ms( duration->int_val );
  std::this_thread::sleep_for( duration_ms );

  return true;
}

bool IntToFloat( process_t process_ref )
{
  process_ref->ccall_return_val->float_val = (float_t)process_ref->OperandA()->int_val;
  return true;
}

bool FloatToInt( process_t process_ref )
{
  process_ref->ccall_return_val->int_val = (int_t)process_ref->OperandA()->float_val;
  return true;
}

bool IntToString( process_t process_ref )
{
  std::ostringstream string_stream;
  string_stream << process_ref->OperandA()->int_val;
  process_ref->ccall_return_val->string_ref = new std::string();
  *process_ref->ccall_return_val->string_ref = string_stream.str();

  return true;
}

bool FloatToString( process_t process_ref )
{
  std::ostringstream string_stream;
  string_stream << process_ref->OperandA()->float_val;
  process_ref->ccall_return_val->string_ref = new std::string();
  *process_ref->ccall_return_val->string_ref = string_stream.str();
  return true;
}

} // namespace Instruction
} // namespace Eople
