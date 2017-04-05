#pragma once
#include "eople_core.h"
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <memory>

namespace Eople
{

struct CallData
{
  CallData(const Function* in_function, process_t in_process_ref, const Object* in_args=nullptr,
    promise_t return_value=nullptr, HighResClock::time_point when=HighResClock::now())
    : function(in_function), process_ref(in_process_ref), args(in_args), promise(return_value), time(when)
  {
  }

  CallData() {}

  const Function* function;
  const Object*   args;
  process_t        process_ref;
  // to store return value
  promise_t       promise;
  HighResClock::time_point time;
};

typedef std::queue<CallData> MessageQueue;

struct AutoTryLock
{
  AutoTryLock( std::atomic<int> &lock ) : try_lock(lock)
  {
    int old = try_lock.fetch_add(1);
    got_lock = old == 0;

    if( !got_lock )
    {
      --try_lock;
    }
  }

  ~AutoTryLock()
  {
    unlock();
  }

  void try_again()
  {
    int old = try_lock.fetch_add(1);
    got_lock = old == 0;

    if( !got_lock )
    {
      --try_lock;
    }
  }

  void unlock()
  {
    if( got_lock )
    {
      --try_lock;
      got_lock = false;
    }
  }

  bool got_lock;
private:
  void operator=( AutoTryLock &other );
  std::atomic<int> &try_lock;
};

inline void ExecutionLoop( CallData& call_data )
{
  process_t process_ref = call_data.process_ref;
  const ByteCode* &ip = process_ref->ip;

  while( ip->instruction(process_ref) )
  {
    ++ip;
  }
}

inline void ExecutionLoopIncremental( CallData& call_data )
{
  process_t process_ref = call_data.process_ref;
  const ByteCode* start_ip = process_ref->ip;
  process_ref->ip += process_ref->incremental_ip_offset;
  const ByteCode* &ip = process_ref->ip;

  while( ip->instruction(process_ref) )
  {
    ++ip;
  }

  process_ref->incremental_ip_offset = ip - start_ip;
}

class VirtualMachine
{
public:
  VirtualMachine();
  ~VirtualMachine();

  void Run();
  void Shutdown();
  void FreeProcesses();

  process_t GenerateUniqueProcess();
  void SendMessage( CallData call_data );

  void ExecuteFunction( CallData call_data );
  void ExecuteFunctionIncremental( CallData call_data );
  void ExecuteConstructor( CallData call_data, process_t caller );

private:
  void ExecuteProcessMessage( CallData call_data );

  // Core job loop
  friend void CoreMain( VirtualMachine* vm, u32 id );

  std::vector<std::thread>            cores;
  std::vector<MessageQueue>           queues;
  std::unique_ptr<std::atomic<int>[]> queue_locks;
  std::condition_variable             message_waiting_event;
  std::mutex                          idle_lock;
  std::mutex                          list_lock;
  std::atomic<u32>                    idle_count;
  std::atomic<u32>                    message_count;
  std::atomic<u32>                    ready_to_exit;
  process_t                           process_list;
  u32                                 process_count;
  u32                                 core_count;

  VirtualMachine(const VirtualMachine&);
  VirtualMachine& operator=(const VirtualMachine&);  

  // this is all very questionable
  std::weak_ptr<TextBuffer> context_buffer;
public:
  void SetCurrentTextBuffer(std::weak_ptr<TextBuffer> text_buffer)
  {
    context_buffer = text_buffer;
  }
  std::weak_ptr<TextBuffer> GetCurrentTextBuffer()
  {
    return context_buffer;
  }
};

}
