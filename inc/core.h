#pragma once
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <chrono>
#include <string>
#include "primitive_types.h"
#include "utf8.h"
#include "highres_clock.h"
#include "auto_scope.h"
#include "safe_range.h"

namespace Eople
{

#ifdef WIN32
  #define snprintf _snprintf_s
  #define strtoull _strtoui64
#endif

#ifdef WIN32
#define FOPEN(file, name, mode) ((fopen_s( &file, name, mode )) == 0)
#else
#define FOPEN(file, name, mode) ((file = fopen( name, mode )) != nullptr)
#endif

#ifndef _MSC_VER
  #define _countof(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

// TODO: find a proper home for this

#ifdef _MSC_VER
  #define aligned_malloc(size, alignment) _aligned_malloc((size), (alignment))
  #define aligned_realloc(ptr, size, alignment) _aligned_realloc((ptr), (size), (alignment))
  #define aligned_free(ptr) _aligned_free((ptr))
#else
  #define aligned_malloc(size, alignment) malloc((size))
  #define aligned_realloc(ptr, size, alignment) realloc((ptr), (size))
  #define aligned_free(ptr) free((ptr))
#endif

std::string convert_raw_string( std::string &formatted_string );
std::string utf8_error_to_string( utf8::utf_error error );

template<typename T> T Min( T x, T y )
{
  return x > y ? y : x;
}

template<typename T> T Max( T x, T y )
{
  return x > y ? x : y;
}

template<typename T> T Clamp( T v, T lo, T hi )
{
  return Max( lo, Min(v, hi) );
}

#define BIT(bit) (1 << (bit))

} // namespace Eople
