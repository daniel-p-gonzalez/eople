#pragma once
#include "core.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>

namespace Eople
{

struct Object;
struct Function;
struct Process;
struct Promise;
struct Type;

typedef i64                  int_t;
typedef f64                  float_t;
typedef u32                  bool_t;
typedef Process*             process_t;
typedef Promise*             promise_t;
typedef Function*            function_t;
typedef Type*                type_t;
typedef std::string*         string_t;
typedef std::vector<Object>  array_t;
typedef std::vector<Object>* array_ptr_t;
typedef std::unordered_map<std::string, Object>* dict_t;


// order matters: eople_parse.cpp:ParseType and eople_static.cpp:BuildPrimitiveTypes
enum class ValueType
{
  NIL = 0,
  FLOAT,
  INT,
  BOOL,
  STRING,
  DICT,
  STRUCT,
  PROCESS,
  FUNCTION,
  PROMISE,
  ARRAY,
  TYPE,
  ANY,
};

struct StructType;
struct ProcessType;
struct FunctionType;
struct PromiseType;
struct ArrayType;
struct DictType;
struct KindType;
struct AnyType;
typedef std::unique_ptr<Type> TypePtr;

struct TypeBuilder
{
  static type_t GetPrimitiveType( ValueType type )
  {
    assert((size_t)type <= (size_t)ValueType::DICT);

    return primitive_types[(size_t)type].get();
  }

  static type_t GetNilType()
  {
    return GetPrimitiveType(ValueType::NIL);
  }

  static type_t GetBoolType()
  {
    return GetPrimitiveType(ValueType::BOOL);
  }

  static type_t GetIntType()
  {
    return GetPrimitiveType(ValueType::INT);
  }

  static type_t GetFloatType()
  {
    return GetPrimitiveType(ValueType::FLOAT);
  }

  static type_t GetDictType()
  {
    return GetPrimitiveType(ValueType::DICT);
  }

  static type_t GetProcessType( std::string class_name )
  {
    return GetType<ProcessType>(class_name, process_types);
  }

  static type_t GetFunctionType( std::string function_name )
  {
    return GetType<FunctionType>(function_name, function_types);
  }

  static type_t GetPromiseType( type_t inner_type )
  {
    return GetType<PromiseType>(inner_type, promise_types);
  }

  static type_t GetArrayType( type_t inner_type = GetPrimitiveType(ValueType::NIL) )
  {
    return GetType<ArrayType>(inner_type, array_types);
  }

  static type_t GetKindType( type_t inner_type = GetPrimitiveType(ValueType::NIL) )
  {
    return GetType<KindType>(inner_type, kind_types);
  }

  static type_t GetAnyType()
  {
    return any_type.get();
  }

  typedef std::vector<TypePtr>           PrimitiveTypeVector;
  typedef std::map<std::string, TypePtr> ProcessTypeMap;
  typedef std::map<std::string, TypePtr> FunctionTypeMap;
  typedef std::map<type_t, TypePtr>      PromiseTypeMap;
  typedef std::map<type_t, TypePtr>      ArrayTypeMap;
  typedef std::map<type_t, TypePtr>      KindTypeMap;

private:
  template <class T, class KT, class MT>
  static type_t GetType( KT key, MT &the_map )
  {
    auto it = the_map.find(key);
    if( it != the_map.end() )
    {
      return it->second.get();
    }
    TypePtr new_type( new T(key) );
    type_t result = new_type.get();
    the_map[key] = std::move(new_type);
    return result;
  }

  static PrimitiveTypeVector primitive_types;
  static ProcessTypeMap       process_types;
  static FunctionTypeMap     function_types;
  static PromiseTypeMap      promise_types;
  static ArrayTypeMap        array_types;
  static KindTypeMap         kind_types;
  static TypePtr             any_type;
};

struct Type
{
  Type( ValueType in_type ) : type(in_type) {}

  virtual ~Type() {}

  virtual bool IsIncompleteType()
  {
    return type == ValueType::NIL;
  }

  virtual type_t GetVaryingType()
  {
    if(type == ValueType::DICT)
    {
      return TypeBuilder::GetAnyType();
    }
    return TypeBuilder::GetNilType();
  }

  virtual void SetVaryingType( type_t )
  {
  }

  ValueType type;
};

struct StructType : public Type
{
  StructType( std::string name ) : struct_name(name), Type(ValueType::STRUCT)
  {
  }

  std::string struct_name;
  std::vector<type_t> member_names;
};

struct ProcessType : public Type
{
  ProcessType( std::string name ) : class_name(name), Type(ValueType::PROCESS) {}
  std::string class_name;
};

struct FunctionType : public Type
{
  FunctionType( std::string name ) : function_name(name), Type(ValueType::FUNCTION) {}
  std::string         function_name;
  std::vector<type_t> arg_types;
  type_t              return_type;
};

struct PromiseType : public Type
{
  PromiseType( type_t inner_type ) : result_type(inner_type), Type(ValueType::PROMISE) {}
  virtual bool IsIncompleteType()
  {
    return result_type->IsIncompleteType();
  }

  virtual type_t GetVaryingType()
  {
    return result_type;
  }

  virtual void SetVaryingType( type_t varying_type )
  {
    if(!result_type->IsIncompleteType() && result_type != varying_type)
    {
      throw "Type Mismatch";
    }
    result_type = varying_type;
  }

  type_t result_type;
};

struct ArrayType : public Type
{
  ArrayType( type_t inner_type ) : element_type(inner_type), Type(ValueType::ARRAY) {}
  virtual bool IsIncompleteType()
  {
    return element_type->IsIncompleteType();
  }

  virtual type_t GetVaryingType()
  {
    return element_type;
  }

  virtual void SetVaryingType( type_t varying_type )
  {
    if(!element_type->IsIncompleteType() && element_type != varying_type)
    {
      throw "Type Mismatch";
    }
    element_type = varying_type;
  }

  type_t element_type;
};

// type whose value is a type.
// eg. in x = <int>, the type of x is KindType, and KindType::kind_type->type == INT
// slightly less confusing than TypeType... TypeType::type_type->type == type
// typetypetypetypetypetypetypetype
struct KindType : public Type
{
  KindType( type_t inner_type ) : kind_type(inner_type), Type(ValueType::TYPE) {}
  virtual bool IsIncompleteType()
  {
    return kind_type->IsIncompleteType();
  }

  virtual type_t GetVaryingType()
  {
    return kind_type;
  }

  virtual void SetVaryingType( type_t varying_type )
  {
    if(!kind_type->IsIncompleteType() && kind_type != varying_type)
    {
      throw "Type Mismatch";
    }
    kind_type = varying_type;
  }

  type_t kind_type;
};

struct AnyType : public Type
{
  AnyType() : Type(ValueType::ANY) {}
};

} // namespace Eople
