#include "core.h"
#include "eople_log.h"
#include "eople_text_buffer.h"

#include <mutex>

#include <cstdio>
#include <cstdarg>
#include <cassert>

namespace Eople
{
namespace Log
{

static std::vector<std::string> s_context_stack;
static std::vector<FILE*> s_file_stack;
static std::string s_context;
static std::weak_ptr<TextBuffer> s_text_buffer;
static bool s_dirty = true;
static bool s_print_errors = true;

void SetTextBuffer( std::weak_ptr<TextBuffer> text_buffer )
{
  s_text_buffer = text_buffer;
}

bool PushFile( std::string file_name )
{
  FILE* file = nullptr;

  if( !FOPEN( file, file_name.c_str(), "w" ) )
  {
    return false;
  }

  s_file_stack.push_back(file);
  return true;
}

void PopFile()
{
  if( !s_file_stack.empty() )
  {
    FILE* file = s_file_stack.back();
    fclose(file);
    s_file_stack.pop_back();
  }
}

void SilenceErrors( bool silence )
{
  s_print_errors = !silence;
}

void Print( const char* format, va_list args )
{
  // regenerate context string?
  if( s_dirty )
  {
    s_context = "";
    for( auto context_string : s_context_stack )
    {
      s_context += context_string;
    }
  }

  s_dirty = false;

  char buffer[1024];
  assert( s_context.size() < 1024 );
  // copy context string to output buffer before appending input string
  strcpy(buffer, s_context.c_str());

  int count = vsnprintf( buffer + s_context.size(), 1024 - s_context.size(), format, args );

  // write to log file if there is one
  if( !s_file_stack.empty() )
  {
    fwrite( buffer, 1, s_context.size() + count, s_file_stack.back() );
  }

  std::shared_ptr<TextBuffer> text_buffer = s_text_buffer.lock();
  // output to console
  if( text_buffer )
  {
    text_buffer->Append( buffer );
  }
}

void Print( const char* format, ... )
{
  va_list args;
  va_start(args, format);
  Print(format, args);
  va_end(args);
}

void Error( const char* format, ... )
{
  if( s_print_errors )
  {
    va_list args;
    va_start(args, format);
    Print(format, args);
    va_end(args);
  }
}

void PushContext( const char* context_string )
{
  s_dirty = true;
  s_context_stack.push_back(std::string(context_string));
}

void PopContext()
{
  s_dirty = true;
  if( !s_context_stack.empty() )
  {
    s_context_stack.pop_back();
  }
}

void ClearContext()
{
  s_context_stack.clear();
  s_context = "";
}

} // namespace Log
} // namespace Eople
