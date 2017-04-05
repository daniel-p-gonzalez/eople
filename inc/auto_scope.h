#pragma once
//
// Simple unobtrusive mechanism for handling automatic object cleanup
//
#include <functional>

namespace Eople
{

struct AutoScope
{
  AutoScope( std::function<void(void)> func )
    : func(func), m_aborted(false)
  {
  }

  ~AutoScope()
  {
    if(!m_aborted)
    {
      func();
    }
  }

  void Trigger()
  {
    if(!m_aborted)
    {
      func();
    }
    m_aborted = true;
  }

  void Abort()
  {
    m_aborted = true;
  }

  std::function<void(void)> func;

private:
  void operator=(const AutoScope&);
  AutoScope();
  bool m_aborted;
};

} // namespace Eople
