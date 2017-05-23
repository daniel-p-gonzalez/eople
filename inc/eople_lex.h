#pragma once
#include "core.h"
#include <string>
#include <functional>

namespace Eople
{

#define TOKEN_STRING(token) #token

enum Token
{
  // 256 so we start after ascii codes
  TOK_NUMBER_INT = 256,   // eg. 22149
  TOK_NUMBER_FLOAT,       // eg. 8.125
  TOK_BOOL,
  TOK_IDENTIFIER,  // eg. ComputeBanana in the function call ComputeBanana()
  TOK_STRING,      // eg. "this is a string"
  // keywords (order matters)
  TOK_NAMESPACE,
  TOK_IMPORT,
  TOK_FROM,
  TOK_STRUCT,
  TOK_FUNCTION,    // function (function declaration)
  TOK_CLASS,       // class (class declaration)
  TOK_END,         // end (scoping)
  TOK_IF,          // if (control flow)
  TOK_ELSE,        // else (control flow)
  TOK_ELSEIF,      // elif (control flow)
  TOK_FOR,         // for (control flow)
  TOK_IN,          // for x in [] (control flow)
  TOK_TO,          // for x in 0 to 100 (control flow)
  TOK_BY,          // for x in 100 to 0 by -1 (control flow)
  TOK_WHILE,       // while (control flow)
  TOK_WHEN,        // when (temporal control flow)
  TOK_WHENEVER,    // whenever (temporal control flow)
  TOK_RETURN,      // return (control flow)
  TOK_BREAK,       // break (control flow)
  TOK_PRIVATE,     // private (access control)
  TOK_CONST,       // const qualifier
  TOK_TRUE,        // true (boolean value)
  TOK_FALSE,       // false (boolean value)
  TOK_AND,         // and (boolean logic)
  TOK_OR,          // or  (boolean logic)
  TOK_NOT,         // not / ! (boolean logic)
  // end keywords
  TOK_EQ,          // == (boolean logic)
  TOK_NEQ,         // != (boolean logic)
  TOK_LEQ,         // <= (boolean logic)
  TOK_GEQ,         // >= (boolean logic)
  TOK_ADD_ASSIGN,  // +=
  TOK_SUB_ASSIGN,  // -=
  TOK_MUL_ASSIGN,  // *=
  TOK_DIV_ASSIGN,  // /=
  TOK_MOD_ASSIGN,  // %=
  TOK_SHIFT_LEFT,  // << (bitwise operator)
  TOK_SHIFT_RIGHT, // >> (bitwise operator)
  TOK_PROCESS_MESSAGE, // ->
  TOK_STRUCT_CALL, // .
  TOK_SCOPE,       // ::
  TOK_NEWLINE,
  TOK_EOF,
};

class Lexer
{
public:
  Lexer( const char* buffer, const char* end );
  Token NextToken();
  u64 GetU64() { return m_u64; }
  f64 GetF64() { return m_f64; }
  std::string GetString() { return m_string; }
  u32 GetLine() { return m_line; }
  std::string TokenToString();

private:
  // safe lookahead tests
  bool Peek( char test );
  bool PeekTest( std::function<int (int)> test_func );
  const char* m_char;
  const char* m_end;

  Token m_token;
  u32 m_line;
  u64 m_u64;
  f64 m_f64;
  std::string m_string;
};

} // namespace Eople
