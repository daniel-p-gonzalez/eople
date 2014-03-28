#pragma once
#include <vector>
#include <string>
#include <memory>

namespace Eople
{

class TextBuffer;

namespace Log
{
  void Print( const char* format, ... );
  void Error( const char* format, ... );
  void SilenceErrors( bool silence );
  void PushContext( const char* context_string );
  void PopContext();
  void ClearContext();
  bool PushFile( std::string file_name );
  void PopFile();
  void SetTextBuffer( std::weak_ptr<TextBuffer> text_buffer );

  struct AutoContext
  {
    AutoContext( const char* context_string )
    {
      PushContext(context_string);
    }
    ~AutoContext()
    {
      PopContext();
    }
  };

  struct AutoFile
  {
    bool file_opened;
    AutoFile( std::string file_name )
    {
      file_opened = PushFile(file_name);
    }
    ~AutoFile()
    {
      if( file_opened )
      {
        PopFile();
      }
    }
  };
}

} // namespace Eople
