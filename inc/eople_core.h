#pragma once
#include "core.h"

#include "eople_log.h"

#include <new>
#include <atomic>
#include <map>
#include <unordered_map>
#include <memory>

#include <cassert>
#include <cstring>

namespace Eople
{

struct Object;
struct Function;
struct Process;
struct Promise;

typedef i64                  int_t;
typedef f64                  float_t;
typedef u32                  bool_t;
typedef Process*             process_t;
typedef Promise*             promise_t;
typedef Function*            function_t;
typedef std::string*         string_t;
typedef std::vector<Object>* array_t;
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

struct Type;
struct StructType;
struct ProcessType;
struct FunctionType;
struct PromiseType;
struct ArrayType;
struct DictType;
struct KindType;
struct AnyType;
typedef Type* type_t;
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

typedef u16 Operand;

//#define INSTR_CCONV _fastcall

// opcodes must be 1 to 1 with Instruction::opcode
// mapping defined in eople_vm.cpp::OpcodeToInstruction
enum class Opcode
{
  AddI,
  SubI,
  MulI,
  DivI,
  ModI,
  AddF,
  SubF,
  MulF,
  DivF,
  ShiftLeft,
  ShiftRight,
  BAnd,
  BXor,
  BOr,
  ConcatS,
  EqualS,
  ForI,
  ForF,
  ForA,
  While,
  Return,
  ReturnValue,
  PrintI,
  PrintF,
  PrintIArr,
  PrintFArr,
  PrintSArr,
  PrintSPromise,
  PrintDict,
  FunctionCall,
  ArrayDeref,
  ProcessMessage,
  GreaterThanI,
  LessThanI,
  EqualI,
  NotEqualI,
  LessEqualI,
  GreaterEqualI,
  GreaterThanF,
  LessThanF,
  EqualF,
  NotEqualF,
  LessEqualF,
  GreaterEqualF,
  And,
  Or,
  Store,
  StoreArrayElement,
  StoreArrayStringElement,
  StringCopy,
  SpawnProcess,
  WhenRegister,
  WheneverRegister,
  When,
  Whenever,
  Jump,
  JumpIf,
  JumpGT,
  JumpLT,
  JumpEQ,
  JumpNEQ,
  JumpLEQ,
  JumpGEQ,
  NOP,
};

typedef bool (*InstructionImpl) ( process_t process_ref );
InstructionImpl OpcodeToInstruction( Opcode opcode );
std::string InstructionToString( InstructionImpl instruction );

struct ByteCode
{
  ByteCode( Opcode in_func )
    : instruction(OpcodeToInstruction(in_func)), a(0), b(0), c(0), d(0)
  {
  }

  ByteCode( InstructionImpl cfunction )
    : instruction(cfunction), a(0), b(0), c(0), d(0)
  {
  }

  ByteCode() {}

  InstructionImpl instruction;
  // operands encoded in instruction
  Operand a;
  Operand b;
  Operand c;
  Operand d;
};

struct Object
{
  union
  {
    function_t    function;
    process_t      process_ref;
    promise_t     promise;
    string_t      string_ref;
    array_t       array_ref;
    dict_t        dict_ref;
    type_t        type;
    float_t       float_val;
    int_t         int_val;
    i32           jump_offset;
    bool_t        bool_val;
  };
  // TODO: this is a hack related to garbage collection and shouldn't be necessary.
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

  void SetArray( array_t val )
  {
    object_type = (u8)ValueType::ARRAY;
    array_ref = val;
  }

  static Object GetArray( array_t val )
  {
    Object array_object;
    array_object.SetArray(val);

    return array_object;
  }

  static Object GetArray()
  {
    Object array_object;
    array_object.array_ref = new std::vector<Object>();
    array_object.object_type = (u8)ValueType::ARRAY;

    return array_object;
  }

  void SetDict( dict_t val )
  {
    object_type = (u8)ValueType::DICT;
    dict_ref = val;
  }

  static Object GetDict( dict_t val )
  {
    Object dict_object;
    dict_object.SetDict(val);

    return dict_object;
  }

  static Object GetDict()
  {
    Object dict_object;
    dict_object.dict_ref = new std::unordered_map<std::string, Object>();
    dict_object.object_type = (u8)ValueType::DICT;

    return dict_object;
  }

  static Object GetInt( int_t val )
  {
    Object int_object;
    int_object.SetInt(val);

    return int_object;
  }

  static Object GetFloat( float_t val )
  {
    Object float_object;
    float_object.SetFloat(val);

    return float_object;
  }

  static Object GetBool( bool_t val )
  {
    Object bool_object;
    bool_object.SetBool(val);

    return bool_object;
  }

  static Object GetString( string_t val )
  {
    Object string_object;
    string_object.SetString(val);

    return string_object;
  }

  static Object GetType( type_t val )
  {
    Object type_object;
    type_object.SetType(val);

    return type_object;
  }

  static Object GetOffset( i32 offset )
  {
    Object offset_object;
    offset_object.jump_offset = offset;

    return offset_object;
  }

  static Object GetFunction( Function* function )
  {
    Object function_object;
    function_object.SetFunction( function );

    return function_object;
  }

  static Object GetProcess( process_t process )
  {
    Object process_object;
    process_object.SetProcess( process );

    return process_object;
  }

  static Object GetPromise( promise_t promise )
  {
    Object promise_object;
    promise_object.SetPromise( promise );

    return promise_object;
  }
};

struct Promise
{
  Promise( process_t in_owner ) : owner(in_owner), value(), is_ready(false), is_timer(false) {}

  process_t owner;
  Object   value;
  bool     is_ready;
  bool     is_timer;
};

struct Function
{
  Function()
    : is_method(false), constants(nullptr), updated_function(nullptr),
      temp_end(0), return_type(TypeBuilder::GetNilType()), is_constructor(false), is_when_eval(false)
  {
  }

  ~Function()
  {
    // when eval function shares outer function constants
    if( !is_when_eval && constants )
    {
      delete[] constants;
      constants = nullptr;
    }
  }

  std::vector<ByteCode> code;
  // function to switch to when hot swapping
  std::atomic<Function*> updated_function;
  std::string          name;
  Object*              constants;

  u32 parameters_start;
  u32 parameter_count() const { return (constants_start - parameters_start); }

  u32 constants_start;
  u32 constant_count() const { return (locals_start - constants_start); }

  u32 locals_start;
  u32 locals_count() const { return (temp_start - locals_start); }

  u32 temp_start;
  u32 temp_end;

  u32 storage_requirement;

  type_t return_type;
  bool   is_method;
  bool   is_constructor;
  bool   is_repl;
  // this function is the evaluation function of a when block
  bool   is_when_eval;
private:
  Function& operator=( const Function & );
};

class VirtualMachine;

struct WhenBlock
{
  WhenBlock( Function* in_eval )
    : eval(in_eval), base_offset(0)
  {
  }

  // when block to evaluate
  Function* eval;
  // state local to when block closure
  // TODO: make proper use of SafeRange
  SafeRange<Object> context;
  // where the closure lives on the stack
  size_t  base_offset;
};

struct Process
{
  Process( u32 in_process, VirtualMachine* in_vm, Process* old_list_head )
    : process_id(in_process), vm(in_vm), next(old_list_head), incremental_ip_offset(0), incremental_locals_offset(0),
      incremental_constants_offset(0), stack(nullptr), stack_end(nullptr), stack_base(nullptr), stack_top(nullptr),
      temporaries(nullptr), ip(nullptr)
  {
    lock.store(0);
  }

  ~Process()
  {
    if( stack )
    {
      aligned_free(stack);
      stack = nullptr;
    }
  }

  Object* OperandA()
  {
    assert( (stack_base+ip->a) < stack_end );
    return &stack_base[ip->a];
  }

  Object* OperandB()
  {
    assert( (stack_base+ip->b) < stack_end );
    return &stack_base[ip->b];
  }

  Object* OperandC()
  {
    assert( (stack_base+ip->c) < stack_end );
    return &stack_base[ip->c];
  }

  Object* OperandD()
  {
    assert( (stack_base+ip->d) < stack_end );
    return &stack_base[ip->d];
  }

  bool IsTemporary( Object* object )
  {
    return object >= temporaries;
  }

  // Free memory from temp string (if it's actually a temp)
  void TryCollectTempString( Object* temp )
  {
    if( temp >= temporaries )
    {
      delete temp->string_ref;
    }
  }

  // If destination is a temporary, this makes sure it's initialized so that values left over
  //     from other (non-string) temporaries are not misinterpreted as pointers.
  void TryInitTempString( Object* temp )
  {
    if( temp >= temporaries )
    {
      temp->string_ref = new std::string();
    }
  }

  void ReallocStack( size_t size )
  {
    // This is to leave c function calls room to return values. There's surely a better way to handle this.
    ++size;

    // to figure out size delta
    size_t old_size = stack_end - stack;

    size_t base_offset = stack_base  - stack;
    size_t top_offset  = stack_top   - stack;
    size_t temp_offset = temporaries - stack;
    Object* old_data = stack;
    stack = (Object*)aligned_realloc( stack, size * sizeof(Object), 64 );
    if( !stack )
    {
      aligned_free(old_data);
      throw std::bad_alloc();
    }
    stack_base  = stack + base_offset;
    stack_top   = stack + top_offset;
    temporaries = stack + temp_offset;
    stack_end   = stack + size;

    // zero initialize new portion of stack
    assert( (stack + old_size + (size - old_size)) == stack_end );
    memset( stack + old_size, 0, sizeof(Object) * (size - old_size) );
  }

  struct StackFrame
  {
    StackFrame( size_t base, size_t top, size_t temp, const ByteCode* code )
      : bp(base), sp(top), tp(temp), ip(code)
    {
    }

    // must be offsets since the stack may be reallocated
    size_t bp;
    size_t sp;
    size_t tp;
    const ByteCode* ip;
  };

  void PushStackFrame()
  {
    stack_frame.push_back(StackFrame(stack_base-stack, stack_top-stack, temporaries-stack, ip));
  }

  void PopStackFrame()
  {
    StackFrame frame = stack_frame.back(); stack_frame.pop_back();
    stack_base  = stack + frame.bp;
    stack_top   = stack + frame.sp;
    temporaries = stack + frame.tp;
    ip          = frame.ip;
  }

  void SetupStackFrame( const Function* function )
  {
    // preserve caller's stack frame
    PushStackFrame();

    // allocate stack space for new active function
    // increment stack base if this is not a method
    stack_base = function->is_method ? stack_base : stack_top;
    stack_top = function->is_when_eval ? stack_base + function->temp_end : stack_top + function->storage_requirement;
    temporaries = function->temp_start + (function->is_method ? stack : stack_base);

    ip = function->code.data();

    // if there's not enough room, grow the stack
    //   + 1 so there is space for ccalls to store return values. TODO: need to clean this up
    if( (stack_top+1) > stack_end )
    {
      ReallocStack( stack_top - stack );
    }
  }

  std::vector<StackFrame> stack_frame;
  std::atomic<int> lock;
  const int  process_id;
  Process*    next;

  // TODO: clean this all up using SafeRange
  Object*    stack;
  Object*    stack_end;

  // stack frame for active function
  Object*    stack_base;
  union
  {
    Object*    stack_top;
    // c function calls return values on top of stack
    Object*    ccall_return_val;
  };
  Object*    temporaries;

  // instruction pointer
  const ByteCode* ip;
  // when running incremental execution (repl), this is the last instruction executed
  size_t incremental_ip_offset;
  // when running incremental execution (repl), this is the last constant/local initialized
  size_t incremental_constants_offset;
  size_t incremental_locals_offset;

  VirtualMachine* vm;

  std::vector<WhenBlock> when_blocks;
  std::vector<WhenBlock> whenever_blocks;
};

} // namespace Eople
