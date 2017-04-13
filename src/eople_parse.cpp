#include "eople_parse.h"
#include "eople_vm.h"
#include <vector>
#include <unordered_map>
#include <cstdio>

namespace Eople
{

Parser::Parser()
  : m_current_module(nullptr), m_error_count(0), m_temp_max(0), m_temp_count(0), m_function(nullptr), m_symbols(nullptr),
    m_when_count(0), m_whenever_count(0)
{
}

static u32 s_temp_index = 0;
static u32 s_indent_level = 0;

void Parser::ParseModule( Lexer &lexer, Node::Module* module )
{
  m_error_count = 0;
  m_current_module = module;
  m_lex = &lexer;
  m_context_stack.clear();
  m_function = nullptr;
  m_namespace_stack.clear();
  m_symbols = nullptr;
  m_temp_count = m_temp_max = 0;
  m_whenever_count = m_when_count = 0;

  ConsumeToken();

  FunctionNode func;
  FunctionNode constructor;
  StructNode   struct_node;
  while( (struct_node = ParseStruct()) || (func = ParseFunction(false)) || (constructor = ParseClass()) || ParseNamespace() )
  {
    if( struct_node )
    {
      m_current_module->structs.push_back( std::move(struct_node) );
    }
    if( func )
    {
      m_current_module->functions.push_back( std::move(func) );
    }
    if( constructor )
    {
      m_current_module->classes.push_back( std::move(constructor) );
    }
  }

  if( m_current_token != TOK_EOF )
  {
    if( m_error_count == 0 )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected function, namespace, or EOF. Got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
      return;
    }
  }
}

void Parser::IncrementalParse( Lexer &lexer, Node::Module* module )
{
  m_error_count = 0;
  m_current_module = module;
  m_lex = &lexer;
  m_context_stack.clear();
  m_function = nullptr;
  m_namespace_stack.clear();
  m_symbols = nullptr;
  m_temp_count = m_temp_max = 0;
//  m_whenever_count = m_when_count = 0;

  ConsumeToken();

  if( module->functions.size() == 0 )
  {
    module->functions.push_back(NodeBuilder::GetFunctionNode( "__anonymous", 0 ));
  }

  m_function = module->functions[0].get();
  m_function->symbols.SetScopeName("__anonymous");
  m_function->symbols.PrepRollback();

  FunctionNode func = nullptr;
  FunctionNode constructor = nullptr;
  StatementNode statement = nullptr;
  while( (statement = ParseStatement()) || (func = ParseFunction(false)) || (constructor = ParseClass()) )
  {
    if( statement )
    {
      m_function->body.push_back( std::move(statement) );
    }
    if( func )
    {
      module->functions.push_back( std::move(func) );
    }
    if( constructor )
    {
      m_current_module->classes.push_back( std::move(constructor) );
    }

    // ParseFunction may modify this, so switch back to main repl function
    m_function = module->functions[0].get();

    // consume leading newlines between statements
    ConsumeNewlines();
  }

//  m_function->symbols.FinalizeRegisters();
  m_function->symbols.DumpTable();
  m_function->symbol_count = m_function->symbols.symbol_count();
  m_function->temp_count   += m_temp_max;

  if( m_current_token != TOK_EOF )
  {
    if( m_error_count == 0 )
    {
      BumpError();
//      Log::Error("(%d): Parse Error: Expected function, namespace, or EOF. Got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
      return;
    }
  }
}

bool Parser::ParseNamespace()
{
  ConsumeNewlines();
  if( !ConsumeExpected(TOK_NAMESPACE) )
  {
    return false;
  }

  // allow newline between 'namespace' and the namespace name
  ConsumeNewlines();

  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_IDENTIFIER) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected namespace name, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return false;
  }

  if( !ConsumeExpected(':') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Missing expected ':'.\n", m_last_error_line);
    return false;
  }

  // only need a temporary symbol table for namespace
  SymbolTable symbols;
  m_symbols = &symbols;
  Eople::AutoScope nuller( [&]{ m_symbols = nullptr; } );

  PushNamespace( ident );
  // parse any contained functions or namespaces
  FunctionNode func;
  while( (func = ParseFunction(false)) || (func = ParseClass()) || ParseNamespace() )
  {
    if( func )
    {
      m_current_module->functions.push_back( std::move(func) );
    }
  }
  PopNamespace();

  m_symbols = nullptr;

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected statement, function definition, or namespace 'end'. Got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return false;
  }

  return true;
}

StructNode Parser::ParseStruct()
{
  ConsumeNewlines();
  if( !ConsumeExpected(TOK_STRUCT) )
  {
    return nullptr;
  }

  // use namespace symbol table if it exists, otherwise use a temp table
  SymbolTable sym;
  m_symbols = m_symbols ? m_symbols : &sym;
  Eople::AutoScope nuller( [&]{ m_symbols = nullptr; } );

  auto ident_node = ParseUniqueIdentifier();
  if( !ident_node )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected struct name, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  // don't need struct identifier in struct's own symbol table
  m_symbols->ClearTable();

  std::string struct_name = ident_node->GetAsIdentifier()->name;

  // allow newline between the struct name and the parameters
  ConsumeNewlines();

  if( !ConsumeExpected('(') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Struct missing opening %c.\n", m_last_error_line, '(');
    return nullptr;
  }

  // allow newline between '(' and the function arguments
  ConsumeNewlines();

  // grab name from identifier, and begin struct scope
  std::string parent_scope = m_namespace_stack.size() > 0 ? m_namespace_stack.back() : "";
  std::string ident(parent_scope + struct_name);
  auto struct_node = NodeBuilder::GetStructNode( ident, m_last_line );

  struct_node->symbols = sym;

  struct_node->symbols.SetScopeName(ident);

  struct_node->symbols.ForceConstant(true);

  m_symbols = &struct_node->symbols;
  auto member = ParseIdentifier();
  while( member )
  {
    struct_node->members.push_back( std::move(member) );

    // consume leading newlines between args
    ConsumeNewlines();

    member = ConsumeExpected(',') ? ParseIdentifier() : nullptr;
  }

  // allow newline between the function arguments and ')'
  ConsumeNewlines();

  if( !ConsumeExpected(')') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Struct missing closing %c.\n", m_last_error_line, ')');
    return nullptr;
  }

  return struct_node;
}

FunctionNode Parser::ParseClass()
{
  return ParseFunction(true);
}

FunctionNode Parser::ParseFunction( bool is_constructor )
{
  if( is_constructor )
  {
    ConsumeNewlines();
    if( !ConsumeExpected(TOK_CLASS) )
    {
      return nullptr;
    }
  }
  else
  {
    ConsumeNewlines();
    if( !ConsumeExpected(TOK_FUNCTION) )
    {
      return nullptr;
    }
  }

  m_when_count     = 0;
  m_whenever_count = 0;

  // allow newline between 'function' and the function name
  ConsumeNewlines();

  // use namespace symbol table if it exists, otherwise use a temp table
  SymbolTable sym;// = m_symbols ? *m_symbols : SymbolTable();
  m_symbols = m_symbols ? m_symbols : &sym;
  Eople::AutoScope nuller( [&]{ m_symbols = nullptr; } );

  auto ident_node = ParseUniqueIdentifier();
  if( !ident_node )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected function name, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  // don't need function identifier in function's own symbol table
  m_symbols->ClearTable();

  std::string func_name = ident_node->GetAsIdentifier()->name;
  std::string this_label = "this";
//  m_symbols = &sym;

  // allow newline between the function name and the parameters
  ConsumeNewlines();

  if( !ConsumeExpected('(') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Function missing opening %c.\n", m_last_error_line, '(');
    return nullptr;
  }

  // allow newline between '(' and the function arguments
  ConsumeNewlines();

  // grab name from identifier, and begin function scope
  std::string parent_scope = m_namespace_stack.size() > 0 ? m_namespace_stack.back() : "";
  std::string ident(parent_scope + func_name);
  auto func = NodeBuilder::GetFunctionNode( ident, m_last_line );
  Node::Function* outer_function = m_function;
  m_function = func.get();

  func->symbols = sym;
  m_function->context = m_context_stack.empty() ? nullptr : m_context_stack.back();

  func->symbols.SetScopeName(ident);
  m_context_stack.push_back(m_function);

  func->symbols.ForceConstant(true);
  if( is_constructor )
  {
    size_t symbol_id = func->symbols.GetTableEntryIndex(this_label, false);
    auto &entry = func->symbols.TableEntryFromId(symbol_id);
    entry.type = TypeBuilder::GetProcessType(func_name);
    entry.target_name = func_name;
    // 'this' must be the first symbol
    assert(symbol_id == 0);
  }

  auto parameter = ParseIdentifier();
  while( parameter )
  {
    func->parameters.push_back( std::move(parameter) );

    // consume leading newlines between args
    ConsumeNewlines();

    parameter = ConsumeExpected(',') ? ParseIdentifier() : 0;
  }

  // allow newline between the function arguments and ')'
  ConsumeNewlines();

  if( !ConsumeExpected(')') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Function missing closing %c.\n", m_last_error_line, ')');
    m_function = outer_function;
    return nullptr;
  }

  if( !ConsumeExpected(':') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Missing expected ':'.\n", m_last_error_line);
    m_function = outer_function;
    return nullptr;
  }

  // consume leading newlines
  ConsumeNewlines();

  // partition between 'this' + parameters, and locals/constants
  func->symbols.ForceConstant(false);

  if( is_constructor )
  {
    func->is_constructor = is_constructor;
    PushNamespace( func_name );
  }

  auto statement = ParseStatement();

  // need to push/pop temp max since nested function will change it (TODO: this is sloppy)
  u32 temp_max = m_temp_max;
  m_temp_max = 0;
  auto nested_func  = ParseFunction(false);
  m_temp_max = temp_max;

  while( statement || nested_func )
  {
    if( statement )
    {
      func->body.push_back( std::move(statement) );
    }
    if( nested_func )
    {
      if( is_constructor )
      {
        func->functions.push_back( std::move(nested_func) );
      }
      else
      {
        BumpError();
        Log::Error("(%d): Parse Error: Nested function not allowed: %s.\n", m_last_error_line, nested_func->name.c_str());
      }
    }

    // consume leading newlines between statements
    ConsumeNewlines();
    statement = ParseStatement();

    temp_max = m_temp_max;
    m_temp_max = 0;
    nested_func  = ParseFunction(false);
    m_temp_max = temp_max;
  }

  if( is_constructor )
  {
    PopNamespace();
  }

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected statement or function 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    m_function = outer_function;
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_context_stack.pop_back();

//  func->symbols.FinalizeRegisters();
//  func->symbols.DumpTable();

  func->symbol_count = func->symbols.symbol_count();
  func->temp_count = m_temp_max;

  m_temp_max = 0;
  m_function = outer_function;
  return func;
}

ExpressionNode Parser::ParseUniqueIdentifier()
{
  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_IDENTIFIER) )
  {
    return nullptr;
  }

  SymbolTable* symbols = m_function ? &m_function->symbols : m_symbols;
  if( symbols->HasSymbol(ident) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Redefinition of symbol: '%s'.\n", m_last_error_line, ident.c_str());
    return nullptr;
  }

  auto ident_node = NodeBuilder::GetIdentifierNode( ident, symbols->GetTableEntryIndex(ident, false), m_last_line );
  return ident_node;
}

ExpressionNode Parser::ParseIdentifier()
{
  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_IDENTIFIER) )
  {
    return nullptr;
  }

  size_t symbol_id = (size_t)-1;

  if( m_function )
  {
    // try to find it if it already exists, including in outer scope
    size_t symbol_id = m_function->symbols.GetTableEntryIndex(ident, false, true);
    Node::Function* parent = m_function->context;
    while( parent && symbol_id == m_function->symbols.NOT_FOUND )
    {
      symbol_id = parent->symbols.GetTableEntryIndex(ident, false, true);
      parent = parent->context;
    }

    // if we didn't find it, create it
    symbol_id = (symbol_id == m_function->symbols.NOT_FOUND) ? m_function->symbols.GetTableEntryIndex(ident, false) : symbol_id;
  }
  else
  {
    // try to find in current symbol table
    size_t symbol_id = m_symbols->GetTableEntryIndex(ident, false, true);

    // if we didn't find it, create it
    symbol_id = (symbol_id == m_symbols->NOT_FOUND) ? m_symbols->GetTableEntryIndex(ident, false) : symbol_id;
  }

  auto ident_node = NodeBuilder::GetIdentifierNode( ident, symbol_id, m_last_line );
  return ident_node;
}

// identifier must have already been initialized
ExpressionNode Parser::ParseValidIdentifier( std::string ident )
{
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(ident, false, true);

  Node::Function* parent = m_function->context;
  while( parent && symbol_id == m_function->symbols.NOT_FOUND )
  {
    symbol_id = parent->symbols.GetTableEntryIndex(ident, false, true);
    parent = parent->context;
  }

  auto ident_node = symbol_id == m_function->symbols.NOT_FOUND ? nullptr : NodeBuilder::GetIdentifierNode( ident, symbol_id, m_last_line );

  if( !ident_node )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Attempt to use uninitialized identifier: '%s'.\n", m_last_error_line, ident.c_str() );
    return nullptr;
  }

  return ident_node;
}

ExpressionNode Parser::ParseArrayDereference( std::string ident )
{
  if( !ConsumeExpected('[') )
  {
    return nullptr;
  }

  size_t symbol_id = m_function->symbols.GetTableEntryIndex(ident, false, true);

  Node::Function* parent = m_function->context;
  while( parent && symbol_id == m_function->symbols.NOT_FOUND )
  {
    symbol_id = parent->symbols.GetTableEntryIndex(ident, false, true);
    parent = parent->context;
  }

  auto ident_node = symbol_id == m_function->symbols.NOT_FOUND ? nullptr : NodeBuilder::GetIdentifierNode( ident, symbol_id, m_last_line );

  if( !ident_node )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Attempt to dereference uninitialized array: '%s'.\n", m_last_error_line, ident.c_str() );
    return nullptr;
  }

  auto array_deref_node = NodeBuilder::GetArrayDereferenceNode(std::move(ident_node), m_last_line);
  auto array_deref = array_deref_node->GetAsArrayDereference();

  bool neg = ConsumeExpected('-');
  array_deref->index = ParseExpression();

  if( !array_deref->index )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Array dereference missing index.\n", m_last_error_line );
  }

  if( !ConsumeExpected(']') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Array dereference missing closing ']'.\n", m_last_error_line );
  }

  return array_deref_node;
}

ExpressionNode Parser::ParseFunctionCallExpression( std::string ident )
{
  bool struct_call = m_current_token == TOK_STRUCT_CALL;
  if( m_current_token != '(' && !struct_call )
  {
    return nullptr;
  }

  auto ident_node = struct_call ? ParseValidIdentifier(ident) :
                                  NodeBuilder::GetIdentifierNode(ident, m_function->symbols.GetTableEntryIndex(ident, false), m_last_line);

  ExpressionNode func_node = ParseFunctionCall( std::move(ident_node), false );
  if( func_node )
  {
    return func_node;
  }
  BumpError();
  Log::Error("(%d): Parse Error: Invalid function call syntax: '%s'.\n", m_last_error_line, ident.c_str() );
  return nullptr;
}

ExpressionNode Parser::ParseType()
{
  //"int", "string", "float", "array", "list", "table",
  if( !ConsumeExpected('<') )
  {
    return 0;
  }

  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_IDENTIFIER) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Missing expected type literal type name after '<'.\n", m_last_error_line );
    return nullptr;
  }

  static const char* type_list[] = { "float", "int", "bool", "string", "struct", "process", "function", "promise" };
  static const size_t type_count = _countof(type_list);

  ValueType type = ValueType::NIL;
  for( size_t i = 0; i < type_count; ++i )
  {
    if( ident == type_list[i] )
    {
      type = (ValueType)((size_t)ValueType::FLOAT + i);
    }
  }

  ident += " type";
  auto node = NodeBuilder::GetTypeNode( TypeBuilder::GetPrimitiveType(type), m_function->symbols.GetTableEntryIndex(ident, true), ident, m_last_line );

  if( !ConsumeExpected('>') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Type literal missing closing '>'.\n", m_last_error_line );
  }

  return node;
}

ExpressionNode Parser::ParseArrayLiteral()
{
  if( !ConsumeExpected('[') )
  {
    return nullptr;
  }

  // TODO: unsuck this
  static char count = 0;
  std::string array_literal_label = "array literal ";
  array_literal_label += '0' + count++;
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(array_literal_label, true);

  auto array_node = NodeBuilder::GetArrayNode(NodeBuilder::GetIdentifierNode( array_literal_label, symbol_id, m_last_line ), m_last_line);
  auto array = array_node->GetAsArrayLiteral();

  auto element = ParseExpression();
  while( element )
  {
    array->elements.push_back( std::move(element) );

    bool another = ConsumeExpected(',');

    // consume newlines between elements
    ConsumeNewlines();

    if( another )
    {
      element = ParseExpression();
    }
    else
    {
      element = nullptr;
    }
  }

  if( !ConsumeExpected(']') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Array literal missing closing ']'.\n", m_last_error_line );
  }

  return array_node;
}

ExpressionNode Parser::ParseDictLiteral()
{
  if( !ConsumeExpected('{') )
  {
    return nullptr;
  }

  static char count = 0;
  std::string dict_literal_label = "dict literal ";
  dict_literal_label += '0' + count++;
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(dict_literal_label, true);

  auto dict_node = NodeBuilder::GetDictNode(NodeBuilder::GetIdentifierNode( dict_literal_label, symbol_id, m_last_line ), m_last_line);
  auto dict = dict_node->GetAsDictLiteral();

  auto key = ParseString();
  while( key )
  {
    if( !ConsumeExpected(':') )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Missing expected ':'.\n", m_last_error_line );
    }
    auto val = ParseExpression();
    if( !val )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Missing expected value in key-value pair.\n", m_last_error_line );
    }

    dict->keys.push_back( std::move(key) );
    dict->values.push_back( std::move(val) );

    bool another = ConsumeExpected(',');

    // consume newlines between elements
    ConsumeNewlines();

    if( another )
    {
      key = ParseString();
    }
    else
    {
      key = nullptr;
    }
  }

  if( !ConsumeExpected('}') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Dict literal missing closing '}'.\n", m_last_error_line );
  }

  return dict_node;
}

ExpressionNode Parser::ParseFloat(bool neg)
{
  f64 number = m_lex->GetF64();
  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_NUMBER_FLOAT) )
  {
    return 0;
  }

  number = neg ? -number : number;
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%f", number);

  size_t symbol_id = m_function->symbols.GetTableEntryIndex(buffer, true);
  return NodeBuilder::GetFloatNode(number, symbol_id, buffer, m_last_line);
}

ExpressionNode Parser::ParseInt(bool neg)
{
  u64 num = m_lex->GetU64();

  int_t number = (int_t)num;
  std::string ident = m_lex->GetString();
  if( !ConsumeExpected(TOK_NUMBER_INT) )
  {
    return 0;
  }

  if( num > INT64_MAX )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Number too large to represent as an int: '%u'.\n", m_last_error_line, num );
  }

  number = neg ? -number : number;
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", (u32)number);

  size_t symbol_id = m_function->symbols.GetTableEntryIndex(buffer, true);
  return NodeBuilder::GetIntNode(number, symbol_id, buffer, m_last_line);
}

ExpressionNode Parser::ParseString()
{
  std::string text = m_lex->GetString();
  if( !ConsumeExpected(TOK_STRING) )
  {
    return 0;
  }

  size_t symbol_id = m_function->symbols.GetTableEntryIndex(text, true);
  string_t new_string;
  new_string = new std::string();
  *new_string = text;
  return NodeBuilder::GetStringNode(new_string, symbol_id, text, m_last_line);
}

ExpressionNode Parser::ParseBool()
{
  bool is_true = ConsumeExpected(TOK_TRUE);
  bool is_false = is_true ? false : ConsumeExpected(TOK_FALSE);
  if( !is_true && !is_false )
  {
    return 0;
  }

  // # is an illegal character for identifiers, using to avoid collision
  // TODO: this will actually still cause symbol table collision if
  //       someone uses "#true#" as a string constant
  std::string ident = is_true ? "#true#" : "#false#";
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(ident, true);
  return NodeBuilder::GetBoolNode(is_true, symbol_id, ident, m_last_line);
}

ExpressionNode Parser::ParseParExpression()
{
  if( !ConsumeExpected('(') )
  {
    return nullptr;
  }

  auto expr_node = ParseExpression();

  if( !ConsumeExpected(')') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected ')', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  return expr_node;
}

ExpressionNode Parser::ParseFactor()
{
  auto paren_expr_node = ParseParExpression();
  if( paren_expr_node )
  {
    return paren_expr_node;
  }

  ExpressionNode expr_node = nullptr;

  if( m_current_token == TOK_IDENTIFIER )
  {
    std::string ident = m_lex->GetString();
    ConsumeExpected(TOK_IDENTIFIER);

    // is this a process message?
    if( m_current_token == TOK_PROCESS_MESSAGE )
    {
      auto ident_node = NodeBuilder::GetIdentifierNode(ident, m_function->symbols.GetTableEntryIndex(ident, false), m_last_line);
      return ParseProcessMessage( std::move(ident_node) );
    }

    expr_node = ParseFunctionCallExpression(ident);
    if( expr_node )
    {
      return expr_node;
    }

    expr_node = ParseArrayDereference(ident);
    if( expr_node )
    {
      return expr_node;
    }

    expr_node = ParseValidIdentifier(ident);
    if( expr_node )
    {
      return expr_node;
    }

    return nullptr;
  }

  // '-' is a separate token in negative numbers
  bool neg = ConsumeExpected('-');
  bool pos = false;
  if( !neg )
  {
    // eat optional '+'
    pos = ConsumeExpected('+');
  }

  expr_node = ParseType();
  if( expr_node )
  {
    if( neg || pos )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Unexpected unary operator on type literal.\n", m_last_error_line);
      return nullptr;
    }
    return expr_node;
  }

  expr_node = ParseArrayLiteral();
  if( expr_node )
  {
    if( neg || pos )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Unexpected unary operator on array literal.\n", m_last_error_line);
      return nullptr;
    }
    return expr_node;
  }

  expr_node = ParseDictLiteral();
  if( expr_node )
  {
    if( neg || pos )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Unexpected unary operator on dict literal.\n", m_last_error_line);
      return nullptr;
    }
    return expr_node;
  }

  expr_node = ParseFloat(neg);
  if( expr_node )
  {
    return expr_node;
  }

  expr_node = ParseInt(neg);
  if( expr_node )
  {
    return expr_node;
  }

  expr_node = ParseBool();
  if( expr_node )
  {
    if( neg || pos )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Unexpected unary operator on bool literal.\n", m_last_error_line);
      return nullptr;
    }
    return expr_node;
  }

  expr_node = ParseString();
  if( expr_node )
  {
    if( neg || pos )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Unexpected unary operator on string literal.\n", m_last_error_line);
      return nullptr;
    }
    return expr_node;
  }

  return nullptr;
}

OpType TokenToOpType( u32 token )
{
  switch( token )
  {
    case '+':             return OpType::ADD;
    case '-':             return OpType::SUB;
    case '*':             return OpType::MUL;
    case '/':             return OpType::DIV;
    case '%':             return OpType::MOD;
    case '>':             return OpType::GT;
    case '<':             return OpType::LT;
    case TOK_EQ:          return OpType::EQ;
    case TOK_NEQ:         return OpType::NEQ;
    case TOK_LEQ:         return OpType::LEQ;
    case TOK_GEQ:         return OpType::GEQ;
    case TOK_SHIFT_LEFT:  return OpType::SHL;
    case TOK_SHIFT_RIGHT: return OpType::SHR;
    case '&':             return OpType::BAND;
    case '^':             return OpType::BXOR;
    case '|':             return OpType::BOR;
    case TOK_AND:         return OpType::AND;
    case TOK_OR:          return OpType::OR;
    case TOK_NOT:         return OpType::NOT;
    case TOK_PROCESS_MESSAGE: return OpType::CALL;
    default:              assert(false);
  }

  return OpType::UNKNOWN;
}

ExpressionNode Parser::ParseOp( std::function<bool(i32)> predicate, std::function<ExpressionNode(Parser* const)> arg_parser )
{
  ExpressionNode lhs_node = nullptr;
  if( (lhs_node = arg_parser(this)) == 0 )
  {
    return nullptr;
  }

  for(;;)
  {
    if( predicate((i32)m_current_token) )
    {
      auto expr_node = NodeBuilder::GetBinaryOpNode( TokenToOpType(m_current_token), m_last_line );
      auto binary_op_node = expr_node->GetAsBinaryOp();

      binary_op_node->left = std::move(lhs_node);

      ++m_temp_count;

      // consume binary op token
      ConsumeToken();

      // allow newline after binary op
      ConsumeNewlines();

      ExpressionNode rhs_node = nullptr;
      if( (rhs_node = arg_parser(this)) == nullptr )
      {
        if( m_last_line != m_last_error_line )
        {
          BumpError();
          Log::Error("(%d): Parse Error: Expected expression term, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
        }
        return nullptr;
      }
      binary_op_node->right = std::move(rhs_node);

      lhs_node = std::move(expr_node);
    }
    else
    {
      break;
    }
  }

  return lhs_node;
}

ExpressionNode Parser::ParseTerm()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '*' || token == '/' || token == '%' );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseFactor) );
}

ExpressionNode Parser::ParseArithExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '+' || token == '-' );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseTerm) );
}

ExpressionNode Parser::ParseBitshiftExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == TOK_SHIFT_LEFT || token == TOK_SHIFT_RIGHT );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseArithExpression) );
}

ExpressionNode Parser::ParseCompExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '>' || token == '<' || token == TOK_EQ || token == TOK_NEQ || token == TOK_LEQ || token == TOK_GEQ );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseBitshiftExpression) );
}

ExpressionNode Parser::ParseBitwiseAndExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '&' );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseCompExpression) );
}

ExpressionNode Parser::ParseBitwiseXorExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '^' );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseBitwiseAndExpression) );
}

ExpressionNode Parser::ParseBitwiseOrExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == '|' );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseBitwiseXorExpression) );
}

ExpressionNode Parser::ParseAndExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == TOK_AND );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseBitwiseOrExpression) );
}

ExpressionNode Parser::ParseExpression()
{
  auto predicate = [](i32 token) -> bool
                   {
                     return( token == TOK_OR );
                   };
  return ParseOp( predicate, std::mem_fn(&Parser::ParseAndExpression) );
}

StatementNode Parser::ParseForInit()
{
  ExpressionNode counter_node = nullptr;
  if( (counter_node = ParseIdentifier()) == 0 )
  {
    BumpError();
    Log::Error("(%d): Parse Error: For loop missing loop variable.\n", m_last_error_line);
    return 0;
  }

  if( !ConsumeExpected(TOK_IN) )
  {
    BumpError();
    Log::Error("(%d): Parse Error: For loop missing 'in'.\n", m_last_error_line);
    return 0;
  }

  ExpressionNode range_start_node = nullptr;
  if( (range_start_node = ParseFactor()) == 0 )
  {
    BumpError();
    Log::Error("(%d): Parse Error: For loop missing range or container.\n", m_last_error_line);
    return 0;
  }

  auto node = NodeBuilder::GetForInitNode( m_last_line );
  auto for_init = node->GetAsForInit();
  for_init->counter = std::move(counter_node);
  for_init->start = std::move(range_start_node);

  // range?
  if( ConsumeExpected(TOK_TO) )
  {
    ExpressionNode range_end_node = nullptr;
    if( (range_end_node = ParseFactor()) == 0 )
    {
      BumpError();
      Log::Error("(%d): Parse Error: For loop missing range limit.\n", m_last_error_line);
      return 0;
    }
    for_init->end = std::move(range_end_node);

    if( ConsumeExpected(TOK_BY) )
    {
      ExpressionNode step_node = nullptr;
      if( (step_node = ParseFactor()) == 0 )
      {
        BumpError();
        Log::Error("(%d): Parse Error: For loop missing range limit.\n", m_last_error_line);
        return 0;
      }
      for_init->step = std::move(step_node);
    }
    else
    {
      auto step_node = NodeBuilder::GetIntNode(1, m_function->symbols.GetTableEntryIndex("1", true), "1", m_last_line);
      for_init->step = std::move(step_node);
    }
  }

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseFor()
{
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_FOR) )
  {
    return nullptr;
  }

  auto node = NodeBuilder::GetForNode( m_last_line );
  auto for_node = node->GetAsFor();

  bool need_closing_paren = ConsumeExpected('(');

  if( need_closing_paren )
  {
    // allow newline between '(' and the for header
    ConsumeNewlines();
  }

  auto init_node = ParseForInit();
  if( !init_node )
  {
    return nullptr;
  }
  for_node->init = std::move(init_node);

  // allow newline between the for header and ')'
  ConsumeNewlines();

  if( need_closing_paren && !ConsumeExpected(')') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: For loop missing closing %c.\n", m_last_error_line, ')');
    return nullptr;
  }

  if( !ConsumeExpected(':') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Missing expected ':'.\n", m_last_error_line);
    return nullptr;
  }

  // consume leading newlines
  ConsumeNewlines();

  ParseStatementList(for_node->body);

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected statement or for loop 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    }
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

ExpressionNode Parser::ParseCondition()
{
  auto condition_node = ParseExpression();
  if( !condition_node )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected a condition expression, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  // allow newline between the condition and ')'
  ConsumeNewlines();

  if( !ConsumeExpected(':') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Missing ':' after condition.\n", m_last_error_line);
    return nullptr;
  }

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return condition_node;
}

void Parser::ParseStatementList( std::vector<StatementNode> &statement_list )
{
  auto statement = ParseStatement();
  while( statement )
  {
    statement_list.push_back( std::move(statement) );

    // consume leading newlines between statements
    ConsumeNewlines();
    statement = ParseStatement();
  }
}

StatementNode Parser::ParseWhile()
{
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_WHILE) )
  {
    return nullptr;
  }

  auto node = NodeBuilder::GetWhileNode(m_last_line);
  auto while_node = node->GetAsWhile();

  while_node->condition = ParseCondition();

  // consume leading newlines
  ConsumeNewlines();

  // parse true branch body
  ParseStatementList(while_node->body);

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected statement or 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    }
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseWhen()
{
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_WHEN) )
  {
    return nullptr;
  }

  // need extra storage for 'when' eval function pointer
  ++m_temp_max;

  // TODO: unsuck this
  std::string when_label = m_function->name + "::when ";
  when_label += '0' + (char)m_when_count++;
  m_function->symbols.ForceConstant(true);
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(when_label, false);
  m_function->symbols.ForceConstant(false);
  auto node = NodeBuilder::GetWhenNode(NodeBuilder::GetIdentifierNode( when_label, symbol_id, m_last_line ), m_last_line);
  auto when_node = node->GetAsWhen();

  when_node->condition = ParseCondition();

  // consume leading newlines
  ConsumeNewlines();

  // parse true branch body
  ParseStatementList(when_node->body);

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected statement or 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    }
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseWhenever()
{
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_WHENEVER) )
  {
    return nullptr;
  }

  // need extra storage for 'when' eval function pointer
  ++m_temp_max;

  // TODO: unsuck this
  std::string when_label = m_function->name + "::whenever ";
  when_label += '0' + (char)m_whenever_count++;
  m_function->symbols.ForceConstant(true);
  size_t symbol_id = m_function->symbols.GetTableEntryIndex(when_label, false);
  m_function->symbols.ForceConstant(false);
  auto node = NodeBuilder::GetWheneverNode(NodeBuilder::GetIdentifierNode( when_label, symbol_id, m_last_line ), m_last_line);
  auto whenever_node = node->GetAsWhenever();

  whenever_node->condition = ParseCondition();

  // consume leading newlines
  ConsumeNewlines();

  // parse true branch body
  ParseStatementList(whenever_node->body);

  // consume trailing newlines
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_END) )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected statement or 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    }
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseIf()
{
  ConsumeNewlines();

  if( !ConsumeExpected(TOK_IF) )
  {
    return nullptr;
  }

  auto node = NodeBuilder::GetIfNode(m_last_line);
  auto if_node = node->GetAsIf();

  if_node->condition = ParseCondition();

  // consume leading newlines
  ConsumeNewlines();

  // parse true branch body
  ParseStatementList(if_node->body);

  // consume trailing newlines
  ConsumeNewlines();

  // parse optional elseifs
  while( ConsumeExpected(TOK_ELSEIF) )
  {
    auto elseif_node = NodeBuilder::GetElseIfNode( m_last_line );
    auto elseif = elseif_node->GetAsElseIf();
    elseif->condition = ParseCondition();

    // consume leading newlines
    ConsumeNewlines();

    ParseStatementList(elseif->body);
    if_node->elseifs.push_back( std::move(elseif_node) );
  }

  // consume trailing newlines
  ConsumeNewlines();

  // parse optional else block
  if( ConsumeExpected(TOK_ELSE) )
  {
    if( !ConsumeExpected(':') )
    {
      BumpError();
      Log::Error("(%d): Parse Error: 'else' missing expected ':'.\n", m_last_error_line);
      return 0;
    }

    // consume leading newlines
    ConsumeNewlines();

    ParseStatementList(if_node->else_body);
  }

  if( !ConsumeExpected(TOK_END) )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected statement or 'end', got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    }
    return nullptr;
  }

  // consume trailing newlines
  ConsumeNewlines();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseAssignment( ExpressionNode ident_node )
{
  ConsumeToken();
  m_temp_count = 0;

  auto assignment_node = NodeBuilder::GetAssignmentNode( m_last_line );
  auto assignment = assignment_node->GetAsAssignment();
  assignment->left = std::move(ident_node);

  ExpressionNode expr_node = nullptr;
  if( (expr_node = ParseExpression()) == nullptr )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected rhs of assignment.\n", m_last_error_line);
    }
    return nullptr;
  }

  assignment->right = std::move(expr_node);

  if( m_current_token != TOK_NEWLINE && m_current_token != ';' )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected a statement separater, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  ConsumeToken();

  m_temp_count = (m_temp_count >= 1) ? (m_temp_count - 1) : 0;
  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;
  return assignment_node;
}

StatementNode Parser::ParseProcessMessage( ExpressionNode process_node )
{
  if( !ConsumeExpected(TOK_PROCESS_MESSAGE) )
  {
    return nullptr;
  }

  auto ident_node = ParseIdentifier();
  if( !ident_node )
  {
    BumpError();
    Log::Error( "(%d): Parse Error: Member function expected after '%s->'.\n", m_last_error_line, process_node->GetAsIdentifier()->name.c_str() );
    return nullptr;
  }

  StatementNode message_node = nullptr;
  if( (message_node = ParseFunctionCall( std::move(ident_node), false )) == nullptr )
  {
    if( m_last_line != m_last_error_line )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected rhs of assignment.\n", m_last_error_line);
    }
    return nullptr;
  }

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return NodeBuilder::GetProcessMessageNode( std::move(process_node), std::move(message_node), m_last_line );
}

StatementNode Parser::ParseFunctionCall( ExpressionNode ident_node, bool need_terminator )
{
  bool struct_call = false;
  ExpressionNode first_arg = nullptr;

  if( !ConsumeExpected('(') )
  {
    if( !ConsumeExpected(TOK_STRUCT_CALL) )
    {
      return nullptr;
    }
    struct_call = true;
    // ident_node is first parameter of call
    first_arg = std::move(ident_node);
    ident_node = ParseIdentifier();
    if( !ident_node )
    {
      BumpError();
      Log::Error("(%d): Parse Error: Expected struct method name after '%s.'.\n", m_last_error_line, first_arg->GetAsIdentifier()->name.c_str());
      return nullptr;
    }
    if( !ConsumeExpected('(') )
    {
      // must be a struct member variable dereference
      return NodeBuilder::GetStructDereferenceNode( first_arg->GetAsIdentifier()->name, ident_node->GetAsIdentifier()->name, m_last_line );
    }
  }

  ++m_temp_count;

  // find symbol for function call, and move to constant register
  {
    auto &func_name = ident_node->GetAsIdentifier()->name;
    auto function = m_function;
    size_t symbol_id = m_function->symbols.GetTableEntryIndex(func_name, false, true);
    Node::Function* parent = m_function->context;
    // TODO: this is necessary because of how ParseIdentifier() recursively searches if symbol already exists.
    //       symbol may be found if this is eg. a recursive function call in a nested function.
    while( parent && symbol_id == m_function->symbols.NOT_FOUND )
    {
      function = parent;
      symbol_id = parent->symbols.GetTableEntryIndex(func_name, false, true);
      parent = parent->context;
    }
    assert( function != nullptr && symbol_id != m_function->symbols.NOT_FOUND );
    function->symbols.ConvertToConstant(symbol_id);
  }

  auto function_call_node = NodeBuilder::GetFunctionCallNode( std::move(ident_node), m_last_line );
  auto function_call = function_call_node->GetAsFunctionCall();

  // capture the enclosing namespace
  function_call->namespace_stack = m_namespace_stack;

  // if this is using method-style syntactic sugar, add object as first parameter
  if( first_arg )
  {
    function_call->arguments.push_back( std::move(first_arg) );
  }

  // allow newline between '(' and the function arguments
  ConsumeNewlines();
  auto arg = ParseExpression();
  while( arg )
  {
    function_call->arguments.push_back( std::move(arg) );

    // consume leading newlines between args
    ConsumeNewlines();

    arg = ConsumeExpected(',') ? ParseExpression() : 0;
  }
  // allow newline between the function arguments and ')'
  ConsumeNewlines();

  if( !ConsumeExpected(')') )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Function call missing closing %c.\n", m_last_error_line, ')');
    return nullptr;
  }

  if( need_terminator && m_current_token != TOK_NEWLINE && m_current_token != TOK_EOF && m_current_token != ';' )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected a statement separater, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return nullptr;
  }

  if( need_terminator )
  {
    ConsumeToken();
  }

  if( need_terminator )
  {
    m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
    m_temp_count = 0;
  }
  return function_call_node;
}

StatementNode Parser::ParseReturn()
{
  if( !ConsumeExpected(TOK_RETURN) )
  {
    return nullptr;
  }

  m_temp_count = 0;

  auto node = NodeBuilder::GetReturnNode( m_last_line );
  auto return_node = node->GetAsReturn();

  auto expr_node = ParseExpression();

  if( expr_node )
  {
    return_node->return_value = std::move(expr_node);
  }

  if( m_current_token != TOK_NEWLINE && m_current_token != ';' )
  {
    BumpError();
    Log::Error("(%d): Parse Error: Expected a statement separater, got '%s' instead.\n", m_last_error_line, m_lex->TokenToString().c_str());
    return 0;
  }

  ConsumeToken();

  m_temp_max = m_temp_count > m_temp_max ? m_temp_count : m_temp_max;
  m_temp_count = 0;

  return node;
}

StatementNode Parser::ParseStatement()
{
  StatementNode statement = nullptr;

  m_temp_count = 0;

  auto ident_node = ParseIdentifier();
  if( ident_node )
  {
    auto array_deref_node = ParseArrayDereference(ident_node->GetAsIdentifier()->name);
    // is this an assignment?
    if( m_current_token == '=' || (m_current_token >= TOK_ADD_ASSIGN && m_current_token <= TOK_MOD_ASSIGN) )
    {
      if( array_deref_node )
      {
        statement = ParseAssignment( std::move(array_deref_node) );
      }
      else
      {
        statement = ParseAssignment( std::move(ident_node) );
      }
      if( statement )
      {
        return statement;
      }
    }
    else if( array_deref_node )
    {
      BumpError();
      if( m_current_token != TOK_NEWLINE )
      {
        Log::Error( "(%d): Parse Error: Assignment expected.\n", m_last_error_line );
      }
    }

    // is this a process message?
    if( m_current_token == TOK_PROCESS_MESSAGE )
    {
      statement = ParseProcessMessage( std::move(ident_node) );
      if( statement )
      {
        return statement;
      }
    }

    // is it a function call?
    if( m_current_token == '(' || m_current_token == TOK_STRUCT_CALL )
    {
      statement = ParseFunctionCall( std::move(ident_node), true);
      if( statement )
      {
        return statement;
      }
    }

    BumpError();
    if( m_current_token != TOK_NEWLINE )
    {
      Log::Error( "(%d): Parse Error: Assignment or function call expected after identifier.\n", m_last_error_line );
    }
    return nullptr;
  }

  statement = ParseIf();
  if( statement )
  {
    return statement;
  }

  statement = ParseFor();
  if( statement )
  {
    return statement;
  }

  statement = ParseWhile();
  if( statement )
  {
    return statement;
  }

  statement = ParseWhen();
  if( statement )
  {
    return statement;
  }

  statement = ParseWhenever();
  if( statement )
  {
    return statement;
  }

  statement = ParseReturn();
  if( statement )
  {
    return statement;
  }

  return nullptr;
}

void Parser::ConsumeNewlines()
{
  while( m_current_token == TOK_NEWLINE )
  {
    ConsumeToken();
  }
  m_last_line = m_lex->GetLine();
}

} // namespace Eople
