#include "eople_lex.h"
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>

namespace Eople
{

#define KEYWORD_LIST "namespace", "import", "from", "struct", "def", "class", "end", \
                     "if", "else", "elif", "for", "in", "to", "by", "while", "when", "whenever", \
                     "return", "break", "private", "const", "true", "false", "and", "or", \
                     "not"

  Lexer::Lexer( const char* buffer, const char* end )
  : m_char(buffer), m_end(end), m_line(1)
{
  if( utf8::starts_with_bom( buffer, end ) )
  {
    m_char += 3;
  }
}

Token Lexer::NextToken()
{
  static const char* keyword_list[] = { KEYWORD_LIST };
  static const size_t keyword_count = _countof(keyword_list);

  if( m_char >= m_end )
  {
    m_string = "EOF";
    return(m_token = TOK_EOF);
  }

  if( isspace(*m_char) )
  {
    do
    {
      if( *m_char == '\n' || *m_char == '\r' )
      {
        ++m_line;
        ++m_char;
        return(m_token = TOK_NEWLINE);
      }
      ++m_char;
    }
    while( m_char < m_end && isspace(*m_char) );
    return NextToken();
  }

  char delim = *m_char;
  if( delim == '\'' || delim == '\"' )
  {
    m_string = "";
    ++m_char;
    while( m_char < m_end && ( *(m_char-1) == '\\' || *m_char != delim ) )
    {
      m_string += *m_char++;
    }
    ++m_char;

    convert_raw_string( m_string );
    return(m_token = TOK_STRING);
  }

  if( isalpha(*m_char) )
  {
    m_string = *m_char++;
    while( m_char < m_end && ( isalnum(*(m_char)) || *m_char == '_' || (*m_char == ':' && *(m_char+1) == ':') ) )
    {
      if( *m_char == ':' )
      {
        m_string += *m_char++;
      }
      m_string += *m_char++;
    }

    // unordered hash doesn't do much to speed up overall lexing, so... sticking to linear search
    for( size_t i = 0; i < keyword_count; ++i )
    {
      if( keyword_list[i] == m_string )
      {
        return( m_token = (Token)(TOK_NAMESPACE + i) );
      }
    }

    return(m_token = TOK_IDENTIFIER);
  }

  if( isdigit(*m_char) )
  {
    m_string = *m_char++;

    while( m_char < m_end && isdigit(*m_char) )
    {
      m_string += *m_char++;
    }

    if( *m_char == '.' )
    {
      m_string += *m_char++;
      while( m_char < m_end && isdigit(*m_char) )
      {
        m_string += *m_char++;
      }
      m_f64 = strtod(m_string.c_str(), 0);
      return(m_token = TOK_NUMBER_FLOAT);
    }
    else
    {
      m_u64 = std::stoull(m_string);
      return(m_token = TOK_NUMBER_INT);
    }
  }
  else if( *m_char == '.' && PeekTest(isdigit) )
  {
    m_string = *m_char++;

    while( m_char < m_end && isdigit(*m_char) )
    {
      m_string += *m_char++;
    }
    m_f64 = strtod(m_string.c_str(), 0);
    return(m_token = TOK_NUMBER_FLOAT);
  }

  if( *m_char == '=' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_EQ);
  }
  if( *m_char == '!' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_NEQ);
  }
  if( *m_char == '<' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_LEQ);
  }
  if( *m_char == '>' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_GEQ);
  }
  if( *m_char == '+' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_ADD_ASSIGN);
  }
  if( *m_char == '-' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_SUB_ASSIGN);
  }
  if( *m_char == '*' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_MUL_ASSIGN);
  }
  if( *m_char == '/' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_DIV_ASSIGN);
  }
  if( *m_char == '%' && Peek('=') )
  {
    m_char += 2;
    return(m_token = TOK_MOD_ASSIGN);
  }
  if( *m_char == '>' && Peek('>') )
  {
    m_char += 2;
    return(m_token = TOK_SHIFT_RIGHT);
  }
  if( *m_char == '<' && Peek('<') )
  {
    m_char += 2;
    return(m_token = TOK_SHIFT_LEFT);
  }
  if( *m_char == '-' && Peek('>') )
  {
    m_char += 2;
    return(m_token = TOK_PROCESS_MESSAGE);
  }

  if( *m_char == '.' && PeekTest(isalpha) )
  {
    m_string = *m_char++;
    return(m_token = TOK_STRUCT_CALL);
  }

  if( *m_char == '"' )
  {
    ++m_char;
    if( *m_char != '"' )
    {
      m_string = *m_char++;
    }
    else
    {
      ++m_char;
      m_string = "";
      return(m_token = TOK_STRING);
    }
    while( *m_char != '"' && m_char < m_end )
    {
      m_string += *m_char++;
    }
    ++m_char;
    return(m_token = TOK_STRING);
  }

  if( *m_char == '#' )
  {
    while( m_char < m_end && *m_char != '\n' && *m_char != '\r' )
    {
      ++m_char;
    }

    return NextToken();
  }

  return(m_token = (Token)*m_char++);
}

bool Lexer::Peek( char test )
{
  const char* next = m_char+1;
  if( next < m_end && *next == test )
  {
    return true;
  }

  return false;
}

bool Lexer::PeekTest( std::function<int (int)> test_func )
{
  const char* next = m_char+1;
  if( next < m_end )
  {
    return test_func(*next) != 0;
  }

  return false;
}

#define TOKEN_TO_STRING(token) case token: { return GetString(); }

std::string Lexer::TokenToString()
{
  switch(m_token)
  {
    case TOK_NUMBER_INT:
    case TOK_NUMBER_FLOAT:
    case TOK_BOOL:
    case TOK_STRING:
    case TOK_IDENTIFIER:
    case TOK_NAMESPACE:
    case TOK_IMPORT:
    case TOK_FROM:
    case TOK_FUNCTION:
    case TOK_CLASS:
    case TOK_STRUCT:
    case TOK_END:
    case TOK_IF:
    case TOK_ELSE:
    case TOK_FOR:
    case TOK_IN:
    case TOK_TO:
    case TOK_BY:
    case TOK_WHILE:
    case TOK_WHEN:
    case TOK_WHENEVER:
    case TOK_RETURN:
    case TOK_BREAK:
    case TOK_PRIVATE:
    case TOK_CONST:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_AND:
    case TOK_OR:
    case TOK_NOT:
    case TOK_EQ:
    case TOK_NEQ:
    case TOK_LEQ:
    case TOK_GEQ:
    case TOK_ADD_ASSIGN:
    case TOK_SUB_ASSIGN:
    case TOK_MUL_ASSIGN:
    case TOK_DIV_ASSIGN:
    case TOK_MOD_ASSIGN:
    case TOK_SHIFT_LEFT:
    case TOK_SHIFT_RIGHT:
    case TOK_PROCESS_MESSAGE:
    case TOK_STRUCT_CALL:
    case TOK_SCOPE:
    case TOK_EOF:
    {
      return GetString();
    }
    case TOK_NEWLINE:
    {
      return "new line";
    }
    default:
    {
      std::string out_token;
      out_token = (char)m_token;
      return out_token;
    }
  }
}

} // namespace Eople
