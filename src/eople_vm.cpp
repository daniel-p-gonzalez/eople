#include "eople_exec_env.h"
#include "eople_core.h"
#include "eople_opcodes.h"
#include "eople_binop.h"
#include "eople_control_flow.h"
#include "eople_stdlib.h"
#include "eople_vm.h"

#include <iostream>
#include <stdio.h>
#include <vector>
#include <map>
#include <thread>
#include <string>
#include <chrono>

namespace Eople
{

#define OPCODE_TO_INSTRUCTION(opcode) case Opcode::opcode: return Instruction::opcode

#define INSTRUCTION_TO_STRING(opcode) else if(instruction == Instruction::opcode){return #opcode;}

std::string InstructionToString( InstructionImpl instruction )
{
  if(instruction == Instruction::AddI){return "AddI";}
  INSTRUCTION_TO_STRING(SubI)
  INSTRUCTION_TO_STRING(MulI)
  INSTRUCTION_TO_STRING(DivI)
  INSTRUCTION_TO_STRING(ModI)
  INSTRUCTION_TO_STRING(AddF)
  INSTRUCTION_TO_STRING(SubF)
  INSTRUCTION_TO_STRING(MulF)
  INSTRUCTION_TO_STRING(DivF)
  INSTRUCTION_TO_STRING(ShiftLeft)
  INSTRUCTION_TO_STRING(ShiftRight)
  INSTRUCTION_TO_STRING(BAnd)
  INSTRUCTION_TO_STRING(BXor)
  INSTRUCTION_TO_STRING(BOr)
  INSTRUCTION_TO_STRING(ConcatS)
  INSTRUCTION_TO_STRING(EqualS)
  INSTRUCTION_TO_STRING(ForI)
  INSTRUCTION_TO_STRING(ForF)
  INSTRUCTION_TO_STRING(ForA)
  INSTRUCTION_TO_STRING(While)
  INSTRUCTION_TO_STRING(Return)
  INSTRUCTION_TO_STRING(ReturnValue)
  INSTRUCTION_TO_STRING(PrintI)
  INSTRUCTION_TO_STRING(PrintF)
  INSTRUCTION_TO_STRING(FunctionCall)
  INSTRUCTION_TO_STRING(ProcessMessage)
  INSTRUCTION_TO_STRING(GreaterThanI)
  INSTRUCTION_TO_STRING(LessThanI)
  INSTRUCTION_TO_STRING(EqualI)
  INSTRUCTION_TO_STRING(NotEqualI)
  INSTRUCTION_TO_STRING(LessEqualI)
  INSTRUCTION_TO_STRING(GreaterEqualI)
  INSTRUCTION_TO_STRING(GreaterThanF)
  INSTRUCTION_TO_STRING(LessThanF)
  INSTRUCTION_TO_STRING(EqualF)
  INSTRUCTION_TO_STRING(NotEqualF)
  INSTRUCTION_TO_STRING(LessEqualF)
  INSTRUCTION_TO_STRING(GreaterEqualF)
  INSTRUCTION_TO_STRING(And)
  INSTRUCTION_TO_STRING(Or)
  INSTRUCTION_TO_STRING(Store)
  INSTRUCTION_TO_STRING(StringCopy)
  INSTRUCTION_TO_STRING(SpawnProcess)
  INSTRUCTION_TO_STRING(WhenRegister)
  INSTRUCTION_TO_STRING(WheneverRegister)
  INSTRUCTION_TO_STRING(When)
  INSTRUCTION_TO_STRING(Whenever)
  INSTRUCTION_TO_STRING(Jump)
  INSTRUCTION_TO_STRING(JumpIf)
  INSTRUCTION_TO_STRING(JumpGT)
  INSTRUCTION_TO_STRING(JumpLT)
  INSTRUCTION_TO_STRING(JumpEQ)
  INSTRUCTION_TO_STRING(JumpNEQ)
  INSTRUCTION_TO_STRING(JumpLEQ)
  INSTRUCTION_TO_STRING(JumpGEQ)

  return "ERR";
}

InstructionImpl OpcodeToInstruction( Opcode opcode )
{
  switch(opcode)
  {
    OPCODE_TO_INSTRUCTION(AddI);
    OPCODE_TO_INSTRUCTION(SubI);
    OPCODE_TO_INSTRUCTION(MulI);
    OPCODE_TO_INSTRUCTION(DivI);
    OPCODE_TO_INSTRUCTION(ModI);
    OPCODE_TO_INSTRUCTION(AddF);
    OPCODE_TO_INSTRUCTION(SubF);
    OPCODE_TO_INSTRUCTION(MulF);
    OPCODE_TO_INSTRUCTION(DivF);
    OPCODE_TO_INSTRUCTION(ShiftLeft);
    OPCODE_TO_INSTRUCTION(ShiftRight);
    OPCODE_TO_INSTRUCTION(BAnd);
    OPCODE_TO_INSTRUCTION(BXor);
    OPCODE_TO_INSTRUCTION(BOr);
    OPCODE_TO_INSTRUCTION(ConcatS);
    OPCODE_TO_INSTRUCTION(EqualS);
    OPCODE_TO_INSTRUCTION(ForI);
    OPCODE_TO_INSTRUCTION(ForF);
    OPCODE_TO_INSTRUCTION(ForA);
    OPCODE_TO_INSTRUCTION(While);
    OPCODE_TO_INSTRUCTION(Return);
    OPCODE_TO_INSTRUCTION(ReturnValue);
    OPCODE_TO_INSTRUCTION(PrintI);
    OPCODE_TO_INSTRUCTION(PrintF);
    OPCODE_TO_INSTRUCTION(FunctionCall);
    OPCODE_TO_INSTRUCTION(ProcessMessage);
    OPCODE_TO_INSTRUCTION(GreaterThanI);
    OPCODE_TO_INSTRUCTION(LessThanI);
    OPCODE_TO_INSTRUCTION(EqualI);
    OPCODE_TO_INSTRUCTION(NotEqualI);
    OPCODE_TO_INSTRUCTION(LessEqualI);
    OPCODE_TO_INSTRUCTION(GreaterEqualI);
    OPCODE_TO_INSTRUCTION(GreaterThanF);
    OPCODE_TO_INSTRUCTION(LessThanF);
    OPCODE_TO_INSTRUCTION(EqualF);
    OPCODE_TO_INSTRUCTION(NotEqualF);
    OPCODE_TO_INSTRUCTION(LessEqualF);
    OPCODE_TO_INSTRUCTION(GreaterEqualF);
    OPCODE_TO_INSTRUCTION(And);
    OPCODE_TO_INSTRUCTION(Or);
    OPCODE_TO_INSTRUCTION(Store);
    OPCODE_TO_INSTRUCTION(StringCopy);
    OPCODE_TO_INSTRUCTION(SpawnProcess);
    OPCODE_TO_INSTRUCTION(WhenRegister);
    OPCODE_TO_INSTRUCTION(WheneverRegister);
    OPCODE_TO_INSTRUCTION(When);
    OPCODE_TO_INSTRUCTION(Whenever);
    OPCODE_TO_INSTRUCTION(Jump);
    OPCODE_TO_INSTRUCTION(JumpIf);
    OPCODE_TO_INSTRUCTION(JumpGT);
    OPCODE_TO_INSTRUCTION(JumpLT);
    OPCODE_TO_INSTRUCTION(JumpEQ);
    OPCODE_TO_INSTRUCTION(JumpNEQ);
    OPCODE_TO_INSTRUCTION(JumpLEQ);
    OPCODE_TO_INSTRUCTION(JumpGEQ);
  }

  return nullptr;
}

void CoreMain( VirtualMachine* vm, u32 id )
{
  size_t core_count  = vm->core_count;
  size_t queue_count = vm->queues.size();

  const size_t buffer_size = 16;
  CallData messages[buffer_size];
  std::vector<CallData> future_messages;
  std::atomic<int>* queue_locks = vm->queue_locks.get();

  u32 tries = 0;
  const u32 max_retries = 10;

  for(;;)
  {
    ++tries;
    for( size_t j = 0; j < queue_count; ++j )
    {
      size_t i = (j+id) % queue_count;

      AutoTryLock lock(queue_locks[i]);
      if( lock.got_lock && !vm->queues[i].empty() )
      {
        auto current_time = HighResClock::now();
        CallData message = vm->queues[i].front();
        // attempt to get process lock. first come first served.
        AutoTryLock process_lock(message.process_ref->lock);
        if( process_lock.got_lock )
        {
          process_t locked_process = message.process_ref;

          int pop_count = 0;
          bool have_message = true;
          while( have_message && message.process_ref == locked_process && pop_count < buffer_size )
          {
            vm->queues[i].pop();
            // Is this message ready to be processed?
            if( message.time > current_time )
            {
              future_messages.push_back(message);
            }
            else
            {
              // If this message was a timer, mark it as ready.
              if( message.promise && message.promise->is_timer )
              {
                message.promise->is_ready = true;
              }
              messages[pop_count++] = message;
            }
            have_message = false;
            if( !vm->queues[i].empty() )
            {
              message = vm->queues[i].front();
              have_message = true;
            }
          }

          if( pop_count )
          {
            // Save signaling one of the messages until done processing.
            // This way, the idle_count check below is more accurate.
            vm->message_count -= pop_count-1;
          }

          // These messages weren't ready to be processed, so push them to the end of the queue.
          for( auto future_message : future_messages )
          {
            vm->queues[i].push(future_message);
          }
          future_messages.clear();

          // Done with the queue.
          lock.unlock();

          // Process any messages we grabbed.
          for( int f = 0; f < pop_count; ++f )
          {
            if( messages[f].function && messages[f].function->updated_function.load() != nullptr )
            {
              messages[f].function = messages[f].function->updated_function.load();
            }
            vm->ExecuteProcessMessage( messages[f] );
          }

          // Done with the process.
          process_lock.unlock();

          // Adjust message count once we're done processing.
          if( pop_count )
          {
            --vm->message_count;
          }

          // Reset try count if we successfully executed a job.
          tries = pop_count ? 0 : tries;
        }
      }
    }

    if( (tries > max_retries) )
    {
      tries = 0;
      // Take a mini-nap in case we're spinning, so we don't needlessly burn resources.
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      if( vm->message_count < (core_count - vm->idle_count) )
      {
        std::unique_lock<std::mutex> lock(vm->idle_lock);
        // Make sure we still need to sleep before we commit to it.
        if( vm->message_count < (core_count - vm->idle_count) )
        {
          ++vm->idle_count;
          // If we're the last core to sleep and we're exiting, tell everyone to go home
          if( vm->idle_count == core_count && vm->ready_to_exit.load() != 0 )
          {
            vm->message_waiting_event.notify_all();
            return;
          }
          vm->message_waiting_event.wait(lock);
          // If we get woken by the notify_all() above, it's time to go.
          if( vm->message_count < (core_count - vm->idle_count) &&
              vm->idle_count == core_count  && vm->ready_to_exit.load() != 0 )
          {
            return;
          }
          --vm->idle_count;
        }
      }
    }
  }
}

void VirtualMachine::Run()
{
  for( u32 i = 0; i < core_count; ++i )
  {
    cores.push_back(std::thread(CoreMain, this, i+1));
  }
}

VirtualMachine::VirtualMachine()
  : idle_count(0), message_count(0), process_count(0), process_list(nullptr), ready_to_exit(0)
{
  core_count = Max<u32>( 2, std::thread::hardware_concurrency() );
  Eople::Log::Debug("vm> Initializing with %d cores.\n", core_count);
  queue_locks = std::unique_ptr<std::atomic<int>[]>(new std::atomic<int>[core_count]);

  for( u32 i = 0; i < core_count; ++i )
  {
    queue_locks[i].store(0);
    queues.push_back(MessageQueue());
  }
}

void VirtualMachine::Shutdown()
{
  ready_to_exit.store(1);
  message_waiting_event.notify_all();

  int mc = message_count;
  Eople::Log::Debug("vm> Recieved shutdown signal. Waiting to deliver %d messages...\n", mc);
  for( u32 i = 0; i < core_count; ++i )
  {
    cores[i].join();
  }

  mc = message_count;
  if( mc > 0 )
  {
    Eople::Log::Error("vm> Left %d message undelivered.\n", mc);
  }
  Eople::Log::Debug("vm> Shutdown complete.\n");
}

VirtualMachine::~VirtualMachine()
{
  FreeProcesses();
}

void VirtualMachine::FreeProcesses()
{
  auto next = process_list;
  while(next)
  {
    auto to_delete = next;
    next = next->next;
    delete to_delete;
  }
  process_list = nullptr;
}

process_t VirtualMachine::GenerateUniqueProcess()
{
  std::unique_lock<std::mutex> lock(list_lock);
  auto new_process = new Process( process_count++, this, process_list );
  process_list = new_process;
  return new_process;
}

void VirtualMachine::SendMessage( CallData call_data )
{
  u32 index = call_data.process_ref->process_id % queues.size();
  AutoTryLock queue_lock(queue_locks[index]);
  int retry_count = 0;
  while( !queue_lock.got_lock )
  {
    if( ++retry_count > 50 )
    {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      retry_count = 0;
    }
    queue_lock.try_again();
  }
  queues[index].push(call_data);
  queue_lock.unlock();

  ++message_count;

  std::unique_lock<std::mutex> lock(idle_lock);
  if( message_count > (core_count - idle_count) )
  {
    message_waiting_event.notify_one();
  }
}

void CopyArgs( const Function* function, process_t process_ref, Object* dest )
{
  size_t parameter_count = function->parameter_count();
  if( !parameter_count )
  {
    return;
  }

  if( parameter_count <= 3 )
  {
    if( parameter_count == 3 )
    {
      dest[0] = *process_ref->OperandB();
      dest[1] = *process_ref->OperandC();
      dest[2] = *process_ref->OperandD();
    }
    else if( parameter_count == 2 )
    {
      dest[0] = *process_ref->OperandB();
      dest[1] = *process_ref->OperandC();
    }
    else
    {
      dest[0] = *process_ref->OperandB();
    }
  }
  else // TODO: make sure this all makes sense
  {
    // copy args to stack
    for( size_t i = 0; (i+3) < parameter_count; )
    {
      // first operand is function object in first instruction, so skip that
      if( i == 0 )
      {
        dest[i]   = *process_ref->OperandB();
        dest[i+1] = *process_ref->OperandC();
        dest[i+2] = *process_ref->OperandD();
        i+=3;
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

// TODO: refactor with above
void CopyConstructorArgs( const Function* function, process_t process_ref, Object* dest )
{
  size_t parameter_count = function->parameter_count();
  if( !parameter_count )
  {
    return;
  }

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
    size_t i = 0;
    for( ; (i+3) < parameter_count; ++process_ref->ip )
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

    const size_t remainder = parameter_count - i;
    const size_t last      = i;

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

void VirtualMachine::ExecuteConstructor( CallData call_data, process_t caller )
{
  const Function* function    = call_data.function;
        process_t process_ref = call_data.process_ref;

  process_ref->SetupStackFrame( function );

  // copy constants to stack
  assert( (process_ref->stack + function->constants_start + function->constant_count()) <= process_ref->stack_end );
  memcpy( process_ref->stack + function->constants_start, function->constants, function->constant_count() * sizeof(Object) );

  // copy function call args to stack, after 'this' reference
  CopyConstructorArgs( function, caller, process_ref->stack+1 );

  // set 'this' process reference
  assert( process_ref->stack < process_ref->stack_end );
  call_data.process_ref->stack[0] = Object::GetProcess(call_data.process_ref);

  ExecutionLoop(call_data);
}

void VirtualMachine::ExecuteProcessMessage( CallData call_data )
{
  const Function* function    = call_data.function;
        process_t process_ref = call_data.process_ref;
  const Object*   args        = call_data.args;

  if( function )
  {
    process_ref->SetupStackFrame( function );

    // copy args to stack
    assert( (process_ref->stack + function->parameters_start + function->parameter_count()) <= process_ref->stack_end );
    memcpy( process_ref->stack + function->parameters_start, args, sizeof(Object) * function->parameter_count() );
    delete[] args;

    // copy constants to stack
    assert( (process_ref->stack + function->constants_start + function->constant_count()) <= process_ref->stack_end );
    memcpy( process_ref->stack + function->constants_start, function->constants, function->constant_count() * sizeof(Object) );
    // TODO: only necessary because of how string/array memory management is currently implemented
    assert( (process_ref->stack + function->locals_start + function->locals_count()) <= process_ref->stack_end );
    memset( process_ref->stack + function->locals_start, 0, function->locals_count() * sizeof(Object) );

    ExecutionLoop(call_data);

    // notify caller that the promise is ready
    if( call_data.promise )
    {
      call_data.promise->value = *process_ref->stack_base;
      call_data.promise->is_ready = true;
      SendMessage( CallData( nullptr, call_data.promise->owner, nullptr, call_data.promise ) );
      call_data.promise = nullptr;
    }

    process_ref->PopStackFrame();
  }

  auto &when_blocks = process_ref->when_blocks;
  for( size_t i = 0; i < when_blocks.size(); process_ref->PopStackFrame() )
  {
    auto &when = when_blocks[i];
    if( when.eval->updated_function.load() != nullptr )
    {
      when.eval = when.eval->updated_function.load();
    }
    process_ref->SetupStackFrame( when.eval );
    auto old_base = process_ref->stack_base;
    process_ref->stack_base = process_ref->stack + when.base_offset;
    process_ref->stack_top += process_ref->stack_base - old_base;
    process_ref->temporaries += process_ref->stack_base - old_base;
    // TODO: this is a hack to determine whether this when statement is within constructor
    //if( when.eval->parameters_start > 0 )
    //{
    //  // copy constants to stack
    //  assert( (process_ref->stack + when.eval->locals_start + when.eval->locals_count() + when.base_offset) <= process_ref->stack_end );
    //  memcpy( process_ref->stack + when.eval->locals_start + when.base_offset, when.eval->constants, when.eval->locals_count() * sizeof(Object) );
    //}

    if( when.context.base )
    {
      // copy closure state to stack
      assert( (process_ref->stack_base + when.eval->parameters_start + when.context.size()) <= process_ref->stack_end );
      memcpy( process_ref->stack_base + when.eval->parameters_start, when.context.base, when.context.size() * sizeof(Object) );
    }

    const ByteCode* &ip = process_ref->ip;
    if( ip->instruction(process_ref) )
    {
      // when branch taken. swap with last element and pop to remove.
      when_blocks[i] = when_blocks.back();
      when_blocks.pop_back();
      // process i again
      continue;
    }
    ++i;
  }

  auto &whenever_blocks = process_ref->whenever_blocks;
  for( size_t i = 0; i < whenever_blocks.size(); process_ref->PopStackFrame() )
  {
    auto &whenever = whenever_blocks[i];
    if( whenever.eval->updated_function.load() != nullptr )
    {
      whenever.eval = whenever.eval->updated_function.load();
    }
    process_ref->SetupStackFrame( whenever.eval );
    auto old_base = process_ref->stack_base;
    process_ref->stack_base = process_ref->stack + whenever.base_offset;
    process_ref->stack_top += process_ref->stack_base - old_base;
    process_ref->temporaries += process_ref->stack_base - old_base;
    //if( whenever.eval->parameters_start > 0 )
    //{
    //  // copy constants to stack
    //  assert( (process_ref->stack + whenever.eval->locals_start + whenever.eval->locals_count() + whenever.base_offset) <= process_ref->stack_end );
    //  memcpy( process_ref->stack + whenever.eval->locals_start + whenever.base_offset, whenever.eval->constants, whenever.eval->locals_count() * sizeof(Object) );
    //}

    if( whenever.context.base )
    {
      // copy closure state to stack
      assert( (process_ref->stack_base + whenever.eval->parameters_start + whenever.context.size()) <= process_ref->stack_end );
      memcpy( process_ref->stack_base + whenever.eval->parameters_start, whenever.context.base, whenever.context.size() * sizeof(Object) );
    }

    const ByteCode* &ip = process_ref->ip;
    if( ip->instruction(process_ref) )
    {
      if( !process_ref->ccall_return_val->bool_val )
      {
        // broke out of loop. swap with last element and pop to remove.
        when_blocks[i] = when_blocks.back();
        when_blocks.pop_back();
        // process i again
        continue;
      }
      else if( whenever.context.base )
      {
        // update closure state
        assert( (process_ref->stack_base + whenever.eval->parameters_start + whenever.context.size()) <= process_ref->stack_end );
        memcpy( whenever.context.base, process_ref->stack_base + whenever.eval->parameters_start, whenever.context.size() * sizeof(Object) );
      }
    }
    ++i;
  }

  //if( call_data.promise )
  //{
  //  delete call_data.promise;
  //}
}

void VirtualMachine::ExecuteFunction( CallData call_data )
{
  const Function* function    = call_data.function;
  process_t       process_ref = call_data.process_ref;

  // grab old stack base offset so args can be copied from the right offset.
  // using offset instead of pointer since stack may grow
  size_t stack_base_offset = process_ref->stack_base - process_ref->stack;
  // grab ip as well for current instruction
  const ByteCode* old_ip = process_ref->ip;
  // setup new stack frame, potentially growing the stack
  process_ref->SetupStackFrame( function );

  // stash new stack base to restore after arg copy
  Object* new_stack_base = process_ref->stack_base;
  const ByteCode* new_ip = process_ref->ip;

  // temporarily set stack base and ip to caller version to simplify arg copy
  process_ref->stack_base = process_ref->stack + stack_base_offset;
  process_ref->ip = old_ip;
  assert( (process_ref->stack_end - new_stack_base) > function->parameter_count() );
  CopyArgs( function, process_ref, new_stack_base );
  process_ref->ip = new_ip;

  // restore new stack base
  process_ref->stack_base = new_stack_base;

  // TODO: replace ALL memcpy / memset calls with SafeRange operations
  // copy constants to stack
  assert( (process_ref->stack_base + function->constants_start + function->constant_count()) <= process_ref->stack_end );
  memcpy( process_ref->stack_base + function->constants_start, function->constants, function->constant_count() * sizeof(Object) );
  // TODO: only necessary because of how string/array memory management is currently implemented
  assert( (process_ref->stack_base + function->locals_start + function->locals_count()) <= process_ref->stack_end );
  memset( process_ref->stack_base + function->locals_start, 0, function->locals_count() * sizeof(Object) );

  ExecutionLoop(call_data);
  process_ref->PopStackFrame();
}

void VirtualMachine::ExecuteFunctionIncremental( CallData call_data )
{
  const Function* function = call_data.function;
  process_t    process_ref = call_data.process_ref;

  AutoTryLock queue_lock(process_ref->lock);
  int retry_count = 0;
  while( !queue_lock.got_lock )
  {
    if( ++retry_count > 50 )
    {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      retry_count = 0;
    }
    queue_lock.try_again();
  }

  // setup new stack frame, potentially growing the stack
  if( !process_ref->stack_frame.empty() )
  {
    process_ref->PopStackFrame();
  }
  process_ref->SetupStackFrame( function );

  // TODO: replace ALL memcpy / memset calls with SafeRange operations

  size_t old_constants_count = process_ref->incremental_constants_offset;
  size_t old_locals_count = process_ref->incremental_locals_offset;

  size_t constants_copy_count = function->constant_count() - old_constants_count;

  Object* new_locals = process_ref->stack_base + function->locals_start;
  Object* old_locals = process_ref->stack_base + function->constants_start + old_constants_count;

  // copy old locals to new stack location after constants
  if( new_locals != old_locals )
  {
    size_t locals_copy_count = old_locals_count;
    assert( (new_locals + locals_copy_count) <= process_ref->stack_end );
    memmove( new_locals, old_locals, sizeof(Object) * locals_copy_count );

    // TODO: make sure this work in any context.
    // Adjust when and whenever captured closures
    auto adjust_block = 
          [old_constants_count, locals_copy_count](std::vector<WhenBlock> &blocks)
          {
            for( auto &block : blocks )
            {
              if( !block.context.base )
              {
                continue;
              }
              // hot swap when/whenever eval function
              if( block.eval->updated_function.load() != nullptr )
              {
                block.eval = block.eval->updated_function.load();
              }
              // make room if capture size grew
              size_t new_context_size = block.eval->locals_count() + block.eval->parameter_count() + block.eval->constant_count();
              if( new_context_size != block.context.size() )
              {
                Object* new_context = new Object[new_context_size];
                memcpy( new_context, block.context.base, block.context.size() * sizeof(Object) );
                delete[] block.context.base;
                block.context = SafeRange<Object>( new_context, new_context + new_context_size );
              }

              Object* base = block.context.base - block.eval->parameters_start;
              Object* new_block_locals = base + block.eval->locals_start;
              Object* old_block_locals = base + block.eval->constants_start + old_constants_count;
              assert( (new_block_locals + locals_copy_count) <= base + block.context.size() );
              memmove( new_block_locals, old_block_locals, sizeof(Object) * locals_copy_count );
            }
          };

    adjust_block(process_ref->when_blocks);
    adjust_block(process_ref->whenever_blocks);
  }

  if( old_locals_count != function->locals_count() )
  {
    size_t locals_init_count = function->locals_count() - old_locals_count;
    // TODO: only necessary because of how string/array memory management is currently implemented
    memset( process_ref->stack_base + function->locals_start + old_locals_count, 0, locals_init_count * sizeof(Object) );
    // TODO: make sure this work in any context.
    // Adjust when and whenever captured closures
    auto adjust_block = 
          [old_locals_count, locals_init_count](std::vector<WhenBlock> &blocks)
          {
            for( auto &block : blocks )
            {
              if( !block.context.base )
              {
                continue;
              }
              Object* base = block.context.base - block.eval->parameters_start;
              memset( base + block.eval->locals_start + old_locals_count, 0, locals_init_count * sizeof(Object) );
            }
          };

    adjust_block(process_ref->when_blocks);
    adjust_block(process_ref->whenever_blocks);
  }

  // copy new constants to stack after old constants
  if( constants_copy_count )
  {
    Object* new_constants = old_locals;
    assert( (process_ref->stack_base + function->constants_start + function->constant_count()) <= process_ref->stack_end );
    memcpy( new_constants, function->constants + old_constants_count, constants_copy_count * sizeof(Object) );
    // TODO: make sure this work in any context.
    // Adjust when and whenever captured closures
    auto adjust_block = 
          [function, old_constants_count, constants_copy_count](std::vector<WhenBlock> &blocks)
          {
            for( auto &block : blocks )
            {
              if( !block.context.base )
              {
                continue;
              }
              Object* base = block.context.base - block.eval->parameters_start;
              Object* new_block_constants = base + block.eval->constants_start + old_constants_count;
              memcpy( new_block_constants, function->constants + old_constants_count, constants_copy_count * sizeof(Object) );
            }
          };

    adjust_block(process_ref->when_blocks);
    adjust_block(process_ref->whenever_blocks);
  }
  process_ref->incremental_constants_offset = function->constant_count();
  process_ref->incremental_locals_offset = function->locals_count();

  ExecutionLoopIncremental(call_data);
//  process_ref->PopStackFrame();

  auto &when_blocks = process_ref->when_blocks;
  for( size_t i = 0; i < when_blocks.size(); process_ref->PopStackFrame() )
  {
    auto &when = when_blocks[i];
    if( when.eval->updated_function.load() != nullptr )
    {
      when.eval = when.eval->updated_function.load();
    }
    process_ref->SetupStackFrame( when.eval );
    auto old_base = process_ref->stack_base;
    process_ref->stack_base = process_ref->stack + when.base_offset;
    process_ref->stack_top += process_ref->stack_base - old_base;
    process_ref->temporaries += process_ref->stack_base - old_base;
    // TODO: this is a hack to determine whether this when statement is within constructor
    //if( when.eval->parameters_start > 0 )
    //{
    //  // copy constants to stack
    //  assert( (process_ref->stack + when.eval->locals_start + when.eval->locals_count() + when.base_offset) <= process_ref->stack_end );
    //  memcpy( process_ref->stack + when.eval->locals_start + when.base_offset, when.eval->constants, when.eval->locals_count() * sizeof(Object) );
    //}

    if( when.context.base )
    {
      // copy closure state to stack
      assert( (process_ref->stack_base + when.eval->parameters_start + when.context.size()) <= process_ref->stack_end );
      memcpy( process_ref->stack_base + when.eval->parameters_start, when.context.base, when.context.size() * sizeof(Object) );
    }

    const ByteCode* &ip = process_ref->ip;
    if( ip->instruction(process_ref) )
    {
      // when branch taken. swap with last element and pop to remove.
      when_blocks[i] = when_blocks.back();
      when_blocks.pop_back();
      // process i again
      continue;
    }
    ++i;
  }

  auto &whenever_blocks = process_ref->whenever_blocks;
  for( size_t i = 0; i < whenever_blocks.size(); process_ref->PopStackFrame() )
  {
    auto &whenever = whenever_blocks[i];
    if( whenever.eval->updated_function.load() != nullptr )
    {
      whenever.eval = whenever.eval->updated_function.load();
    }
    process_ref->SetupStackFrame( whenever.eval );
    auto old_base = process_ref->stack_base;
    process_ref->stack_base = process_ref->stack + whenever.base_offset;
    process_ref->stack_top += process_ref->stack_base - old_base;
    process_ref->temporaries += process_ref->stack_base - old_base;
    //if( whenever.eval->parameters_start > 0 )
    //{
    //  // copy constants to stack
    //  assert( (process_ref->stack + whenever.eval->locals_start + whenever.eval->locals_count() + whenever.base_offset) <= process_ref->stack_end );
    //  memcpy( process_ref->stack + whenever.eval->locals_start + whenever.base_offset, whenever.eval->constants, whenever.eval->locals_count() * sizeof(Object) );
    //}

    if( whenever.context.base )
    {
      // copy closure state to stack
      assert( (process_ref->stack_base + whenever.eval->parameters_start + whenever.context.size()) <= process_ref->stack_end );
      memcpy( process_ref->stack_base + whenever.eval->parameters_start, whenever.context.base, whenever.context.size() * sizeof(Object) );
    }

    const ByteCode* &ip = process_ref->ip;
    if( ip->instruction(process_ref) )
    {
      if( !process_ref->ccall_return_val->bool_val )
      {
        // broke out of loop. swap with last element and pop to remove.
        when_blocks[i] = when_blocks.back();
        when_blocks.pop_back();
        // process i again
        continue;
      }
      else if( whenever.context.base )
      {
        // update closure state
        assert( (process_ref->stack_base + whenever.eval->parameters_start + whenever.context.size()) <= process_ref->stack_end );
        memcpy( whenever.context.base, process_ref->stack_base + whenever.eval->parameters_start, whenever.context.size() * sizeof(Object) );
      }
    }
    ++i;
  }

  queue_lock.unlock();
}

} // namespace Eople
