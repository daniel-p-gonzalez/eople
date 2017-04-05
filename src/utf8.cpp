#include <string>
#include "core.h"

namespace Eople
{

std::string utf8_error_to_string( utf8::utf_error error )
{
  switch(error)
  {
    case utf8::UTF8_OK:
    {
      return "OK";
    }
    case utf8::NOT_ENOUGH_ROOM:
    {
      return "Not enough room.";
    }
    case utf8::INVALID_LEAD:
    {
      return "Invalid lead.";
    }
    case utf8::INCOMPLETE_SEQUENCE:
    {
      return "Incomplete sequence.";
    }
    case utf8::OVERLONG_SEQUENCE:
    {
      return "Overlong sequence.";
    }
    case utf8::INVALID_CODE_POINT:
    {
      return "Invalid code point.";
    }
  }

  return "Unknown error.";
}

std::string convert_raw_string( std::string &formatted_string )
{
  // convert white space characters to escape sequences
  size_t pos = 0;
  while( (pos = formatted_string.find( "\\n", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\n", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\t", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\t", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\r", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\r", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\b", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\b", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\f", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\f", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\\\", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\\", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\'", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\'", 1 );
  }
  pos = 0;
  while( (pos = formatted_string.find( "\\\"", pos )) != std::string::npos )
  {
    formatted_string.replace( pos, 2, "\"", 1 );
  }

  return formatted_string;
}

} // namespace Eople
