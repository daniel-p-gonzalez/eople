#pragma once
//
// Memory range primitive to make simple memory write operations a bit safer
//
#include <string.h>
#include <assert.h>

namespace Eople
{

template <class T>
struct SafeRange
{
  SafeRange() : base(nullptr), end(nullptr) {}

  SafeRange( T* base, T* end )
    : base(base), end(end)
  {
    assert( end >= base );
  }

  SafeRange operator+( size_t offset )
  {
    T* new_base = base + offset;
    // the new base must not go beyond the end of the range
    assert( new_base <= end );
    new_base = (new_base > end) ? end : new_base;
    return SafeRange( new_base, end );
  }

  size_t operator-( const SafeRange& rhs )
  {
    return base - rhs.base;
  }

  void CopyFrom( const SafeRange& src )
  {
    size_t count = (src.end - src.base);
    size_t capacity = (end-base);
    assert( capacity >= count );
    count = count > capacity ? capacity : count;
    memcpy( base, src.base, (src.end - src.base) * sizeof(T) );
  }

  void CopyFrom( const SafeRange& src, size_t count )
  {
    // count must not be greater than the smallest capacity
    size_t capacity = (end-base) > (src.end-src.base) ? (src.end-src.base) : (end-base);
    assert( capacity >= count );
    count = count > capacity ? capacity : count;
    memcpy( base, src.base, count * sizeof(T) );
  }

  void MemSet( int value, size_t count )
  {
    // count must not be greater than the capacity
    size_t capacity = (end-base);
    assert( capacity >= count );
    count = count > capacity ? capacity : count;
    memset( base, value, count * sizeof(T) );
  }

  size_t size() { return end-base; }

  T* base;
  T* end;
};

} // namespace Eople
