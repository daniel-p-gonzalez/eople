#pragma once
#include "core.h"

#include "eople_types.h"
#include "eople_object.h"
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

typedef u16 Operand;

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

// represents a single virtual machine instruction with its operands
struct VMCode
{
  VMCode( Opcode in_func )
    : instruction(OpcodeToInstruction(in_func)), a(0), b(0), c(0), d(0)
  {
  }

  VMCode( InstructionImpl cfunction )
    : instruction(cfunction), a(0), b(0), c(0), d(0)
  {
  }

  VMCode() {}

  // function pointer to instruction implementation
  InstructionImpl instruction;
  // operands are indices into the process stack
  Operand a;
  Operand b;
  Operand c;
  Operand d;
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

  std::vector<VMCode> code;
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
    StackFrame( size_t base, size_t top, size_t temp, const VMCode* code )
      : bp(base), sp(top), tp(temp), ip(code)
    {
    }

    // must be offsets since the stack may be reallocated
    size_t bp;
    size_t sp;
    size_t tp;
    const VMCode* ip;
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
  const VMCode* ip;
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
