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
  NotEqualS,
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
    : reuse_context(false), constants(nullptr), updated_function(nullptr),
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
  bool   reuse_context;
  bool   is_constructor;
  bool   is_repl;
  // this function is the evaluation function of a when block
  bool   is_when_eval;
private:
  Function& operator=( const Function & );
};

class VirtualMachine;

struct ClosureState
{
  ClosureState() : state(nullptr), base_offset(0), object_count(0) {}

  ClosureState(size_t offset, size_t count)
  : object_count(count), base_offset(offset), state(new Object[count])
  {
  }

  ClosureState(ClosureState&& other)
  {
    state = other.state;
    object_count = other.object_count;
    base_offset = other.base_offset;
    other.state = nullptr;
    other.object_count = 0;
  }

  ClosureState& operator=(ClosureState&& other)
  {
    if( this != &other )
    {
      if( state )
      {
        delete[] state;
      }
      state = other.state;
      object_count = other.object_count;
      base_offset = other.base_offset;
      other.state = nullptr;
      other.object_count = 0;

      return *this;
    }
  }

  ~ClosureState() { if(state) delete[] state; }

  Object* state;
  size_t base_offset;
  size_t object_count;
};

struct WhenBlock
{
  WhenBlock( Function* in_eval, ClosureState &&state )
    : eval(in_eval), closure_state(std::move(state))
  {
  }

  // when block to evaluate
  Function* eval;
  ClosureState closure_state;
};

// Stack layout for a single process looks like:
//  [[this][class_args][constants][locals][temporaries]]
struct ProcessStack
{
  ProcessStack() : stack(nullptr), stack_end(nullptr), stack_base(nullptr),
                   stack_top(nullptr), temporaries(nullptr)
  {
  }

  ~ProcessStack()
  {
    if( stack )
    {
      aligned_free(stack);
      stack = nullptr;
    }
  }

  struct StackFrame
  {
    StackFrame( size_t base, size_t top, size_t temp )
      : bp(base), sp(top), tp(temp)
    {
    }

    // must be offsets since the stack may be reallocated
    size_t bp;
    size_t sp;
    size_t tp;
  };

  void PushStackFrame()
  {
    stack_frame.push_back(StackFrame(stack_base-stack, stack_top-stack, temporaries-stack));
  }

  void PopStackFrame()
  {
    if( !stack_frame.empty() )
    {
      StackFrame frame = stack_frame.back(); stack_frame.pop_back();
      stack_base  = stack + frame.bp;
      stack_top   = stack + frame.sp;
      temporaries = stack + frame.tp;
    }
  }

  void SetupStackFrame( const Function* function )
  {
    // preserve caller's stack frame
    PushStackFrame();

    //
    // Allocate stack space for new active function.
    //
    // increment stack base if this is not a method.
    // this allow access to class member variables
    //  (which start at base of process stack)
    stack_base = function->reuse_context ? stack_base : stack_top;
    // TODO: refactor use of temp_end and storage_requirement.
    // "when" blocks reuse exact stack frame of outer function
    //   e.g. its closure, which is restored + space for additional temporaries
    stack_top = function->is_when_eval ? stack_base + function->temp_end : stack_top + function->storage_requirement;
    temporaries = function->temp_start + (function->reuse_context ? stack : stack_base);

    // if there's not enough room, grow the stack
    //   + 1 so there is space for ccalls to store return values.
    // TODO: refactor to remove need for special handling of ccalls
    if( (stack_top+1) > stack_end )
    {
      ReallocStack( stack_top - stack );
    }
  }

  void InitializeLocals( const Function* function )
  {
    // TODO: only necessary because of how string/array memory management is currently implemented
    assert( (stack_base + function->locals_start + function->locals_count()) <= stack_end );
    memset( stack_base + function->locals_start, 0, function->locals_count() * sizeof(Object) );
  }

  void PushConstants( const Function* function )
  {
    assert( (stack_base + function->constants_start + function->constant_count()) <= stack_end );
    memcpy( stack_base + function->constants_start, function->constants, function->constant_count() * sizeof(Object) );
  }

  void PushArgs( const Function* function, const Object* args )
  {
    assert( (stack + function->parameters_start + function->parameter_count()) <= stack_end );
    memcpy( stack + function->parameters_start, args, sizeof(Object) * function->parameter_count() );
  }

  // TODO: this makes a lot of assumptions about safety
  void PushArgs( const Function* function, const VMCode* caller_ip, Object* src )
  {
    Object* dest;
    if( function->is_constructor )
    {
      dest = stack + 1;
    }
    else
    {
      dest = stack_base;
    }

    size_t parameter_count = function->parameter_count();
    if( !parameter_count )
    {
      return;
    }

    if( !function->is_constructor )
    {
      *(dest++) = src[caller_ip->b];
      --parameter_count;
    }
    if( parameter_count )
    {
      *(dest++) = src[caller_ip->c];
      --parameter_count;
    }
    if( parameter_count )
    {
      *(dest++) = src[caller_ip->d];
      --parameter_count;
    }

    while( parameter_count )
    {
      ++caller_ip;
      if( parameter_count )
      {
        *(dest++) = src[caller_ip->a];
        --parameter_count;
      }
      if( parameter_count )
      {
        *(dest++) = src[caller_ip->b];
        --parameter_count;
      }
      if( parameter_count )
      {
        *(dest++) = src[caller_ip->c];
        --parameter_count;
      }
      if( parameter_count )
      {
        *(dest++) = src[caller_ip->d];
        --parameter_count;
      }
    }
  }

  ClosureState CaptureClosure( const Function* function )
  {
    size_t copy_count = function->locals_count() + function->parameter_count() + function->constant_count();
    if( copy_count )
    {
      size_t base_offset = stack_base - stack;
      ClosureState closure_state = ClosureState(base_offset, copy_count);
      assert( (stack_base + function->parameters_start + copy_count) <= stack_end );
      memcpy( closure_state.state, stack_base + function->parameters_start, copy_count * sizeof(Object) );
      return closure_state;
    }
    else
    {
      return ClosureState();
    }
  }

  void ApplyClosureState( const Function* function, const ClosureState &closure_state )
  {
    auto old_base = stack_base;
    stack_base = stack + closure_state.base_offset;
    stack_top += stack_base - old_base;
    temporaries += stack_base - old_base;

    if( closure_state.state )
    {
      // copy closure state to stack
      assert( (stack_base + function->parameters_start + closure_state.object_count) <= stack_end );
      memcpy( stack_base + function->parameters_start, closure_state.state, closure_state.object_count * sizeof(Object) );
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

  Object* GetObjectAtOffset( u16 offset )
  {
    assert( (stack_base+offset) < stack_end );
    return &stack_base[offset];
  }

  void PushObjectAtOffset( const Object &object, u16 offset )
  {
    assert( (stack_base+offset) < stack_end );
    stack_base[offset] = object;
  }

  void ShiftSegment(size_t old_offset, size_t new_offset, size_t segment_size)
  {
    Object* old_segment = stack_base + old_offset;
    Object* new_segment = stack_base + new_offset;

    assert( (new_segment + segment_size) <= stack_end );
    memmove( new_segment, old_segment, sizeof(Object) * segment_size );
  }

  bool IsTemporary( Object* object )
  {
    return object >= temporaries;
  }

  std::vector<StackFrame> stack_frame;

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
};

// Runtime instance for lightweight asynchronous process.
// In Eople, class constructors build a Process, not an object.
struct Process
{
  Process( u32 in_process, VirtualMachine* in_vm, Process* old_list_head )
    : process_id(in_process), vm(in_vm), next(old_list_head), incremental_ip_offset(0), incremental_locals_offset(0),
      incremental_constants_offset(0), ip(nullptr)
  {
    lock.store(0);
  }

  Object* OperandA()
  {
    return stack.GetObjectAtOffset(ip->a);
  }

  Object* OperandB()
  {
    return stack.GetObjectAtOffset(ip->b);
  }

  Object* OperandC()
  {
    return stack.GetObjectAtOffset(ip->c);
  }

  Object* OperandD()
  {
    return stack.GetObjectAtOffset(ip->d);
  }

  Object* CCallReturnVal()
  {
    return stack.ccall_return_val;
  }

  bool IsTemporary( Object* object )
  {
    return stack.IsTemporary( object );
  }

  // Free memory from temp string (if it's actually a temp)
  void TryCollectTempString( Object* temp )
  {
    if( stack.IsTemporary(temp) )
    {
      delete temp->string_ref;
    }
  }

  // If destination is a temporary, this makes sure it's initialized so that values left over
  //     from other (non-string) temporaries are not misinterpreted as pointers.
  void TryInitTempString( Object* temp )
  {
    if( stack.IsTemporary(temp) )
    {
      temp->string_ref = new std::string();
    }
  }

  void PopStackFrame()
  {
    stack.PopStackFrame();
    if( !callstack.empty() )
    {
      ip = callstack.back(); callstack.pop_back();
    }
  }

  void SetupStackFrame( const Function* function )
  {
    stack.SetupStackFrame(function);
    callstack.push_back(ip);
    ip = function->code.data();
  }

  void InitializeLocalsOnStack( const Function* function )
  {
    stack.InitializeLocals( function );
  }

  void PushConstantsToStack( const Function* function )
  {
    stack.PushConstants(function);
  }

  void PushArgsToStack( const Function* function, const VMCode* caller_ip, Object* src )
  {
    stack.PushArgs(function, caller_ip, src);
  }

  void PushArgsToStack( const Function* function, const Object* args )
  {
    stack.PushArgs(function, args);
  }

  void PushThisPointer( const Object &process_object )
  {
    stack.PushObjectAtOffset( process_object, 0 );
  }

  bool IncrementalStackShift( const Function* function )
  {
    size_t old_constants_count = incremental_constants_offset;
    size_t old_locals_count = incremental_locals_offset;

    size_t constants_copy_count = function->constant_count() - old_constants_count;
    size_t old_offset = function->constants_start + old_constants_count;
    size_t new_offset = function->locals_start;
    if( new_offset != old_offset )
    {
      // move locals to new location
      stack.ShiftSegment(old_offset, new_offset, old_locals_count);

      if( function->locals_count() > old_locals_count )
      {
        size_t locals_init_count = function->locals_count() - old_locals_count;
        // clear memory to simplify garbage collection
        assert( stack.stack_base + function->locals_start + old_locals_count + locals_init_count <= stack.stack_end );
        memset( stack.stack_base + function->locals_start + old_locals_count, 0, locals_init_count * sizeof(Object) );
      }

      if( constants_copy_count )
      {
        Object* new_constants = stack.stack_base + old_offset;
        assert( (stack.stack_base + function->constants_start + function->constant_count()) <= stack.stack_end );
        memcpy( new_constants, function->constants + old_constants_count, constants_copy_count * sizeof(Object) );
      }

      return true;
    }

    return false;
  }

  ProcessStack stack;
  std::vector<const VMCode*> callstack;
  std::atomic<int> lock;
  const int  process_id;
  Process*    next;

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
