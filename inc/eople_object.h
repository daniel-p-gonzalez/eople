#pragma once
#include "core.h"
#include "eople_types.h"

namespace Eople
{

// An Eople::Object is a lightweight runtime instance of a type
struct Object
{
  union
  {
    // These typedefs defined in eople_types.h
    function_t    function;
    process_t     process_ref;
    promise_t     promise;
    string_t      string_ref;
    array_ptr_t       array_ref;
    dict_t        dict_ref;
    type_t        type;
    // concrete (non-pointer) primitive types
    float_t       float_val;
    int_t         int_val;
    i32           jump_offset;
    bool_t        bool_val;
  };

  u8 object_type;

  Object() : object_type((u8)ValueType::NIL)
  {
  }

  void SetInt( int_t val )
  {
    object_type = (u8)ValueType::INT;
    int_val = val;
  }

  void SetFloat( float_t val )
  {
    object_type = (u8)ValueType::FLOAT;
    float_val = val;
  }

  void SetBool( bool_t val )
  {
    object_type = (u8)ValueType::BOOL;
    bool_val = val;
  }

  void SetString( string_t val )
  {
    object_type = (u8)ValueType::STRING;
    string_ref = val;
  }

  void SetType( type_t val )
  {
    type = val;
  }

  void SetFunction( Function* in_function )
  {
    object_type = (u8)ValueType::FUNCTION;
    function = in_function;
  }

  void SetProcess( process_t in_process )
  {
    object_type = (u8)ValueType::PROCESS;
    process_ref = in_process;
  }

  void SetPromise( promise_t in_promise )
  {
    object_type = (u8)ValueType::PROMISE;
    promise = in_promise;
  }

  void SetArray( array_ptr_t val )
  {
    object_type = (u8)ValueType::ARRAY;
    array_ref = val;
  }

  void SetDict( dict_t val )
  {
    object_type = (u8)ValueType::DICT;
    dict_ref = val;
  }

  static Object BuildArray( array_ptr_t val_ptr )
  {
    Object array_object;
    array_object.SetArray(val_ptr);

    return array_object;
  }

  static Object BuildArray( const array_t &val )
  {
    Object array_object;
    array_object.array_ref = new std::vector<Object>();
    *array_object.array_ref = val;
    array_object.object_type = (u8)ValueType::ARRAY;

    return array_object;
  }

  static Object BuildArray()
  {
    Object array_object;
    array_object.array_ref = new std::vector<Object>();
    array_object.object_type = (u8)ValueType::ARRAY;

    return array_object;
  }

  static Object BuildDict( dict_t val )
  {
    Object dict_object;
    dict_object.SetDict(val);

    return dict_object;
  }

  static Object BuildDict()
  {
    Object dict_object;
    dict_object.dict_ref = new std::unordered_map<std::string, Object>();
    dict_object.object_type = (u8)ValueType::DICT;

    return dict_object;
  }

  static Object BuildInt( int_t val )
  {
    Object int_object;
    int_object.SetInt(val);

    return int_object;
  }

  static Object BuildFloat( float_t val )
  {
    Object float_object;
    float_object.SetFloat(val);

    return float_object;
  }

  static Object BuildBool( bool_t val )
  {
    Object bool_object;
    bool_object.SetBool(val);

    return bool_object;
  }

  static Object BuildString( string_t val )
  {
    Object string_object;
    string_object.SetString(val);

    return string_object;
  }

  static Object BuildType( type_t val )
  {
    Object type_object;
    type_object.SetType(val);

    return type_object;
  }

  static Object BuildOffset( i32 offset )
  {
    Object offset_object;
    offset_object.jump_offset = offset;

    return offset_object;
  }

  static Object BuildFunction( Function* function )
  {
    Object function_object;
    function_object.SetFunction( function );

    return function_object;
  }

  static Object BuildProcess( process_t process )
  {
    Object process_object;
    process_object.SetProcess( process );

    return process_object;
  }

  static Object BuildPromise( promise_t promise )
  {
    Object promise_object;
    promise_object.SetPromise( promise );

    return promise_object;
  }
};

} // namespace Eople
