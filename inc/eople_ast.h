#pragma once
#include "eople_core.h"
#include "eople_lex.h"
#include "eople_symbol_table.h"
#include <vector>
#include <string>
#include <memory>

namespace Eople
{

typedef size_t NodeId;

enum class OpType
{
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  GT,
  LT,
  EQ,
  NEQ,
  LEQ,
  GEQ,
  SHL,
  SHR,
  BAND,
  BXOR,
  BOR,
  AND,
  OR,
  NOT,
  CALL,
  UNKNOWN,
};

namespace Node
{
  struct Module;
  struct Struct;
  struct StructDereference;
  struct Function;
  struct Statement;
  struct Expression;
  struct If;
  struct ElseIf;
  struct Assignment;
  struct For;
  struct ForInit;
  struct While;
  struct When;
  struct Whenever;
  struct ProcessMessage;
  struct FunctionCall;
  struct Return;
  struct BinaryOp;
  struct ArraySubscript;
  struct Literal;
  struct TypeLiteral;
  struct ArrayLiteral;
  struct DictLiteral;
  struct IntLiteral;
  struct FloatLiteral;
  struct BoolLiteral;
  struct StringLiteral;
  struct Identifier;
}

typedef std::unique_ptr<Node::Module>            ModuleNode;
typedef std::unique_ptr<Node::Struct>            StructNode;
typedef std::unique_ptr<Node::StructDereference> StructDereferenceNode;
typedef std::unique_ptr<Node::Function>          FunctionNode;
typedef std::unique_ptr<Node::Statement>         StatementNode;
typedef std::unique_ptr<Node::Expression>        ExpressionNode;
typedef std::unique_ptr<Node::If>                IfNode;
typedef std::unique_ptr<Node::ElseIf>            ElseIfNode;
typedef std::unique_ptr<Node::Assignment>        AssignmentNode;
typedef std::unique_ptr<Node::For>               ForNode;
typedef std::unique_ptr<Node::ForInit>           ForInitNode;
typedef std::unique_ptr<Node::While>             WhileNode;
typedef std::unique_ptr<Node::When>              WhenNode;
typedef std::unique_ptr<Node::Whenever>          WheneverNode;
typedef std::unique_ptr<Node::FunctionCall>      FunctionCallNode;
typedef std::unique_ptr<Node::ProcessMessage>    ProcessMessageNode;
typedef std::unique_ptr<Node::Return>            ReturnNode;
typedef std::unique_ptr<Node::BinaryOp>          BinaryOpNode;
typedef std::unique_ptr<Node::ArraySubscript>  ArraySubscriptNode;
typedef std::unique_ptr<Node::TypeLiteral>       TypeLiteralNode;
typedef std::unique_ptr<Node::ArrayLiteral>      ArrayLiteralNode;
typedef std::unique_ptr<Node::DictLiteral>       DictLiteralNode;
typedef std::unique_ptr<Node::IntLiteral>        IntLiteralNode;
typedef std::unique_ptr<Node::FloatLiteral>      FloatLiteralNode;
typedef std::unique_ptr<Node::BoolLiteral>       BoolLiteralNode;
typedef std::unique_ptr<Node::StringLiteral>     StringLiteralNode;
typedef std::unique_ptr<Node::Identifier>        IdentifierNode;

namespace Node
{

  struct NodeCommon
  {
    NodeCommon( u32 in_line )
      : line(in_line)
    {
    }

    virtual ~NodeCommon() {}

    virtual type_t GetValueType()
    {
      return TypeBuilder::GetNilType();
    }

    // returns false on type mismatch
    virtual bool TrySetValueType( type_t /*value_type*/ )
    {
      return true;
    }

    virtual Module*         GetAsModule()         { return nullptr; }
    virtual Function*       GetAsFunction()       { return nullptr; }
    virtual Literal*        GetAsLiteral()        { return nullptr; }
    virtual TypeLiteral*    GetAsTypeLiteral()    { return nullptr; }
    virtual ArrayLiteral*   GetAsArrayLiteral()   { return nullptr; }
    virtual DictLiteral*    GetAsDictLiteral()    { return nullptr; }
    virtual IntLiteral*     GetAsIntLiteral()     { return nullptr; }
    virtual FloatLiteral*   GetAsFloatLiteral()   { return nullptr; }
    virtual BoolLiteral*    GetAsBoolLiteral()    { return nullptr; }
    virtual StringLiteral*  GetAsStringLiteral()  { return nullptr; }
    virtual Identifier*     GetAsIdentifier()     { return nullptr; }
    virtual BinaryOp*       GetAsBinaryOp()       { return nullptr; }
    virtual ForInit*        GetAsForInit()        { return nullptr; }
    virtual For*            GetAsFor()            { return nullptr; }
    virtual While*          GetAsWhile()          { return nullptr; }
    virtual When*           GetAsWhen()           { return nullptr; }
    virtual Whenever*       GetAsWhenever()       { return nullptr; }
    virtual If*             GetAsIf()             { return nullptr; }
    virtual ElseIf*         GetAsElseIf()         { return nullptr; }
    virtual Assignment*     GetAsAssignment()     { return nullptr; }
    virtual FunctionCall*   GetAsFunctionCall()   { return nullptr; }
    virtual ProcessMessage* GetAsProcessMessage() { return nullptr; }
    virtual Return*         GetAsReturn()         { return nullptr; }
    virtual ArraySubscript* GetAsArraySubscript() { return nullptr; }

    u32 line;
  };

  struct Expression : public NodeCommon
  {
    Expression( u32 in_line ) : NodeCommon(in_line)
    {
    }
    virtual ~Expression() {}
  };

  // statements are not always expressions... this is only to simplify FunctionCall
  struct Statement : public Expression
  {
    Statement( u32 in_line ) : Expression(in_line)
    {
    }
    virtual ~Statement() {}
  };

  struct Literal : public Expression
  {
    Literal( type_t type, size_t sym_index, std::string in_string, u32 line )
      : Expression(line), value_string(in_string), symbol_index(sym_index), value_type(type)
    {
    }
    virtual ~Literal() {}

    virtual type_t GetValueType()
    {
      return value_type;
    }

    // throws on type mismatch
    virtual bool TrySetValueType( type_t type )
    {
      if( value_type != type )
      {
        throw "Type Mismatch";
      }

      return value_type == type;
    }

    Literal* GetAsLiteral() { return this; }

    std::string value_string;
    size_t      symbol_index;
  private:
    type_t   value_type;
  };

  struct FloatLiteral : public Literal
  {
    FloatLiteral( float_t in_value, size_t sym_index, std::string value_string, u32 line )
      : Literal(TypeBuilder::GetPrimitiveType(ValueType::FLOAT), sym_index, value_string, line), value(in_value)
    {
    }

    FloatLiteral* GetAsFloatLiteral() { return this; }

    float_t value;
  };

  struct TypeLiteral : public Literal
  {
    TypeLiteral( type_t in_type, size_t sym_index, std::string type_string, u32 line )
      : Literal(TypeBuilder::GetKindType(), sym_index, type_string, line), type(in_type)
    {
    }

    TypeLiteral* GetAsTypeLiteral() { return this; }

    type_t type;
  };

  struct ArrayLiteral : public Expression
  {
    ArrayLiteral( ExpressionNode in_ident, u32 line )
      : Expression(line), array_type(TypeBuilder::GetArrayType()), ident(std::move(in_ident))
    {
    }

    ArrayLiteral* GetAsArrayLiteral() { return this; }

    virtual type_t GetValueType()
    {
      return array_type;
    }

    // throws on type mismatch
    virtual bool TrySetValueType( type_t type )
    {
      if( array_type != type )
      {
        throw "Type Mismatch";
      }

      return true;
    }

    ExpressionNode ident;
    std::vector<ExpressionNode> elements;
    type_t array_type;
  };

  struct ArraySubscript : public Expression
  {
    ArraySubscript( ExpressionNode in_ident, u32 line )
      : Expression(line), ident(std::move(in_ident))
    {
    }

    ArraySubscript* GetAsArraySubscript() { return this; }

    ExpressionNode ident;
    ExpressionNode index;
  };

  struct DictLiteral : public Expression
  {
    DictLiteral( ExpressionNode in_ident, u32 line )
      : Expression(line), ident(std::move(in_ident))
    {
    }

    DictLiteral* GetAsDictLiteral() { return this; }

    ExpressionNode ident;
    std::vector<ExpressionNode> keys;
    std::vector<ExpressionNode> values;
  };

  struct IntLiteral : public Literal
  {
    IntLiteral( int_t in_value, size_t sym_index, std::string value_string, u32 line )
      : Literal(TypeBuilder::GetPrimitiveType(ValueType::INT), sym_index, value_string, line), value(in_value)
    {
    }

    IntLiteral* GetAsIntLiteral() { return this; }

    int_t value;
  };

  struct BoolLiteral : public Literal
  {
    BoolLiteral( bool_t in_value, size_t sym_index, std::string value_string, u32 line )
      : Literal(TypeBuilder::GetBoolType(), sym_index, value_string, line), value(in_value)
    {
    }

    BoolLiteral* GetAsBoolLiteral() { return this; }

    bool_t value;
  };

  struct StringLiteral : public Literal
  {
    StringLiteral( string_t in_value, size_t sym_index, std::string value_string, u32 line )
      : Literal(TypeBuilder::GetPrimitiveType(ValueType::STRING), sym_index, value_string, line), value(in_value)
    {
    }

    StringLiteral* GetAsStringLiteral() { return this; }

    string_t value;
  };

  struct Identifier : public Expression
  {
    Identifier( std::string in_name, size_t sym_index, u32 in_line )
      : Expression(in_line), name(in_name), symbol_index(sym_index), is_guard(false), is_variant(false)
    {
    }

    Identifier* GetAsIdentifier() { return this; }

    std::string name;
    size_t      symbol_index;
    // TODO: this is a kludge to get static guards off the ground
    bool        is_guard;
    bool        is_variant;
  };

  struct BinaryOp : public Expression
  {
    BinaryOp( OpType op_type, u32 in_line ) : Expression(in_line), op(op_type)
    {
    }

    virtual BinaryOp* GetAsBinaryOp() { return this; }

    OpType         op;
    ExpressionNode left;
    ExpressionNode right;
  };

  struct FunctionCall : public Statement
  {
    FunctionCall( ExpressionNode in_ident, u32 in_line ) : Statement(in_line), ident(std::move(in_ident))
    {
    }

    std::string GetName() { return ident->GetAsIdentifier()->name; }

    // name mangling used to disambiguate function call symbols
    std::string GetMangledName( size_t parent_specialization );

    virtual FunctionCall* GetAsFunctionCall() { return this; }

    type_t GetReturnType( size_t parent_specialization );
    bool   TrySetReturnType( size_t parent_specialization, type_t type );
    size_t GetSpecialization( size_t parent_specialization );
    void   SetSpecialization( size_t parent_specialization, size_t specialization );

    std::vector<ExpressionNode> arguments;
    // return type, indexed by containing function specialization.
    // multiple specializations of the containing function may generate different function calls, so these are their return types.
    std::vector<type_t>         return_specializations;
    // which target function to call, indexed by containing function specialization.
    // multiple specializations of the containing function may generate different function calls, so these are their specializations.
    // yes this is very confusing and could use a refactor.
    std::vector<size_t>         call_specializations;

    // namespace stack for the scope containing this function call, to disambiguate
    std::vector<std::string>    namespace_stack;
  private:
    ExpressionNode ident;
  };

  struct ProcessMessage : public Statement
  {
    ProcessMessage( ExpressionNode process_node, StatementNode message_node, u32 in_line ) : Statement(in_line),
                                          process(std::move(process_node)), message(std::move(message_node))
    {
    }

    virtual ProcessMessage* GetAsProcessMessage() { return this; }

    // name mangling used to disambiguate function call symbols
    std::string GetMangledName( size_t parent_specialization );

    ExpressionNode process;
    StatementNode  message;
  };

  struct Struct : public Statement
  {
    Struct( std::string in_name, u32 in_line ) : Statement(in_line), name(in_name) {}

    std::string                 name;
    SymbolTable                 symbols;
    // each specialization gets its own symbol table
    std::vector<SymbolTable>    specializations;
    std::vector<ExpressionNode> members;
  };

  struct StructDereference : public Statement
  {
    StructDereference( std::string in_obj_name, std::string in_member_name, u32 in_line )
      : Statement(in_line), object_name(in_obj_name), member_name(in_member_name) {}

    std::string object_name;
    std::string member_name;
  };

  struct Function : public Statement
  {
    Function( std::string in_name, u32 in_line ) : Statement(in_line), name(in_name), is_c_call(false), temp_count(0),
                                                 is_latent_c_call(false), is_constructor(false), current_specialization(0), is_repl(false),
                                                 return_type(TypeBuilder::GetNilType()), context(nullptr), next_incremental_statement(0)
    {
      is_template.push_back(true);
      scope_name = name + "::";
    }

    Function* GetAsFunction() { return this; }

    TableEntry* GetTableEntryForName( std::string key );

    TableEntry* GetTableEntry( Expression* expr );

    EntryId GetEntryIdForName( std::string key, u32& depth );
    EntryId GetEntryId( Expression* expr, u32& depth );
    virtual type_t GetValueType();
    // throws on type mismatch
    virtual bool TrySetValueType( type_t type );
    // throws on type mismatch
    bool TrySetExpressionType( Expression* expr, type_t type );
    // throws on type mismatch
    void TrySetExpressionVaryingType( Expression* expr, type_t type );
    // doesn't check for identifier type mismatch, but can still throw if literal type doesn't match
    void SetExpressionValueType( Expression* expr, type_t type );
    type_t GetExpressionType( Expression* expr );
    type_t GetExpressionVaryingType( Expression* expr );
    bool TrySetCallReturnType( FunctionCall* function_call, type_t type );
    type_t GetCallReturnType( FunctionCall* function_call );
    size_t GetStackIndex( Expression* expr );
    void RegisterFunctionCall( FunctionCall* function_call );
    void RegisterProcessMessage( ProcessMessage* process_call );
    void SetCurrentSpecialization( size_t index );
    // Make the class names of any process reference parameters match those in the function call arguments
    void SetParameterClassNames( FunctionCall* function_call, Function* caller_scope, std::vector<type_t> &arg_types, size_t specialization );
    size_t FindSpecializationForArgs( std::vector<type_t> &arg_types );

    SymbolTable                 symbols;
    // each specialization gets its own symbol table
    std::vector<SymbolTable>    specializations;
    std::vector<bool>           is_template;
    size_t                      current_specialization;
    std::string                 name;
    std::string                 scope_name;
    std::vector<ExpressionNode> parameters;
    std::vector<StatementNode>  body;
    // only valid when this function is a class constructor
    std::vector<FunctionNode>   functions;
    // context is the containing environement if this is a member function or part of a closure
    Node::Function*             context;
    // count of symbols within the scope of this function
    size_t symbol_count;
    // max number of temporaries needed within the scope of this function
    size_t temp_count;

    // during incremental compile, next statement to process
    size_t next_incremental_statement;

    bool   is_c_call;
    // c call that returns a promise
    bool   is_latent_c_call;
    bool   is_constructor;
    bool   is_repl;
  private:
    type_t                   return_type;
    std::vector<type_t>      return_specializations;
  };

  struct Module : public NodeCommon
  {
    Module( std::string in_name ) : NodeCommon(0), name(in_name)
    {
    }

    Module* GetAsModule() { return this; }

    std::string               name;
    std::vector<StructNode>   structs;
    std::vector<FunctionNode> functions;
    std::vector<FunctionNode> classes;
    std::vector<FunctionNode> templates;
    std::vector<Module*>      imported;
  };

  struct ElseIf : public Statement
  {
    ElseIf( u32 in_line ) : Statement(in_line)
    {
    }

    virtual ElseIf* GetAsElseIf() { return this; }

    ExpressionNode             condition;
    std::vector<StatementNode> body;
  };

  struct If : public Statement
  {
    If( u32 in_line ) : Statement(in_line)
    {
    }

    virtual If* GetAsIf() { return this; }

    ExpressionNode             condition;
    std::vector<StatementNode> body;
    std::vector<StatementNode> elseifs;
    std::vector<StatementNode> else_body;
  };

  struct Assignment : public Statement
  {
    Assignment( u32 in_line ) : Statement(in_line)
    {
    }

    virtual Assignment* GetAsAssignment() { return this; }

    ExpressionNode left;
    ExpressionNode right;
  };

  struct ForInit : public Statement
  {
    ForInit( u32 in_line ) : Statement(in_line)
    {
    }

    virtual ForInit* GetAsForInit() { return this; }

    ExpressionNode counter;
    ExpressionNode start;
    ExpressionNode end;
    ExpressionNode step;
  };

  struct For : public Statement
  {
    For( u32 in_line ) : Statement(in_line)
    {
    }

    virtual For* GetAsFor() { return this; }

    StatementNode              init;
    std::vector<StatementNode> body;
  };

  struct While : public Statement
  {
    While( u32 in_line ) : Statement(in_line)
    {
    }

    virtual While* GetAsWhile() { return this; }

    ExpressionNode             condition;
    std::vector<StatementNode> body;
  };

  struct When : public Statement
  {
    When( ExpressionNode in_ident, u32 in_line ) : Statement(in_line), ident(std::move(in_ident))
    {
    }

    virtual When* GetAsWhen() { return this; }

    ExpressionNode             ident;
    ExpressionNode             condition;
    std::vector<StatementNode> body;
  };

  struct Whenever : public Statement
  {
    Whenever( ExpressionNode in_ident, u32 in_line ) : Statement(in_line), ident(std::move(in_ident))
    {
    }

    virtual Whenever* GetAsWhenever() { return this; }

    ExpressionNode             ident;
    ExpressionNode             condition;
    std::vector<StatementNode> body;
  };

  struct Return : public Statement
  {
    Return( u32 in_line ) : Statement(in_line)
    {
    }

    virtual Return* GetAsReturn() { return this; }

    ExpressionNode return_value;
  };

} // namespace Node

struct NodeBuilder
{
  static ModuleNode GetModuleNode( std::string name )
  {
    return ModuleNode( new Node::Module(name) );
  }

  static StructNode GetStructNode( std::string name, u32 line )
  {
    return StructNode( new Node::Struct( name, line ) );
  }

  static StatementNode GetStructDereferenceNode( std::string object_name, std::string member_name, u32 line )
  {
    return StructDereferenceNode( new Node::StructDereference( object_name, member_name, line ) );
  }

  static FunctionNode GetFunctionNode( std::string name, u32 line )
  {
    return FunctionNode( new Node::Function( name, line ) );
  }

  static StatementNode GetIfNode( u32 line )
  {
    return StatementNode( new Node::If( line ) );
  }

  static StatementNode GetElseIfNode( u32 line )
  {
    return StatementNode( new Node::ElseIf( line ) );
  }

  static StatementNode GetAssignmentNode( u32 line )
  {
    return StatementNode( new Node::Assignment( line ) );
  }

  static StatementNode GetForNode( u32 line )
  {
    return StatementNode( new Node::For( line ) );
  }

  static StatementNode GetForInitNode( u32 line )
  {
    return StatementNode( new Node::ForInit( line ) );
  }

  static StatementNode GetWhileNode( u32 line )
  {
    return StatementNode( new Node::While( line ) );
  }

  static StatementNode GetWhenNode( ExpressionNode ident, u32 line )
  {
    return StatementNode( new Node::When( std::move(ident), line ) );
  }

  static StatementNode GetWheneverNode( ExpressionNode ident, u32 line )
  {
    return StatementNode( new Node::Whenever( std::move(ident), line ) );
  }

  static StatementNode GetFunctionCallNode( ExpressionNode ident, u32 line )
  {
    return StatementNode( new Node::FunctionCall( std::move(ident), line ) );
  }

  static StatementNode GetProcessMessageNode( ExpressionNode process, StatementNode message, u32 line )
  {
    return StatementNode( new Node::ProcessMessage( std::move(process), std::move(message), line ) );
  }

  static StatementNode GetReturnNode( u32 line )
  {
    return StatementNode( new Node::Return( line ) );
  }

  static ExpressionNode GetBinaryOpNode( OpType op, u32 line )
  {
    return ExpressionNode( new Node::BinaryOp( op, line ) );
  }

  static ExpressionNode GetTypeNode( type_t type, size_t sym_index, std::string value_string, u32 line )
  {
    return ExpressionNode( new Node::TypeLiteral( type, sym_index, value_string, line ) );
  }

  static ExpressionNode GetArrayNode( ExpressionNode ident, u32 line )
  {
    return ExpressionNode( new Node::ArrayLiteral( std::move(ident), line ) );
  }

  static ExpressionNode GetArraySubscriptNode( ExpressionNode ident, u32 line )
  {
    return ExpressionNode( new Node::ArraySubscript( std::move(ident), line ) );
  }

  static ExpressionNode GetDictNode( ExpressionNode ident, u32 line )
  {
    return ExpressionNode( new Node::DictLiteral( std::move(ident), line ) );
  }

  static ExpressionNode GetIntNode( int_t int_val, size_t sym_index, std::string value_string, u32 line )
  {
    return ExpressionNode( new Node::IntLiteral( int_val, sym_index, value_string, line ) );
  }

  static ExpressionNode GetFloatNode( float_t float_val, size_t sym_index, std::string value_string, u32 line )
  {
    return ExpressionNode( new Node::FloatLiteral( float_val, sym_index, value_string, line ) );
  }

  static ExpressionNode GetBoolNode( bool_t bool_val, size_t sym_index, std::string value_string, u32 line )
  {
    return ExpressionNode( new Node::BoolLiteral( bool_val, sym_index, value_string, line ) );
  }

  static ExpressionNode GetStringNode( string_t string_val, size_t sym_index, std::string value_string, u32 line )
  {
    return StringLiteralNode( new Node::StringLiteral( string_val, sym_index, value_string, line ) );
  }

  static ExpressionNode GetIdentifierNode( std::string name, size_t symbol_index, u32 line )
  {
    return ExpressionNode( new Node::Identifier( name, symbol_index, line ) );
  }

}; // struct NodeBuilder

struct ASTree
{
  std::vector<ModuleNode> modules;

  // find module containing the given function
  Node::Module* GetModuleForFunction( std::string function_name )
  {
    for( auto &child : modules )
    {
      for( auto &func : child->functions )
      {
        if( func->name == function_name )
        {
          return child.get();
        }
      }
    }

    return nullptr;
  }

  Node::Module* GetModule( std::string module_name )
  {
    for( auto &child : modules )
    {
      if( child->name == module_name )
      {
        return child.get();
      }
    }

    return nullptr;
  }
};

template <class BASECLASS_T>
class ASTDispatcher
{
public:

  void Dispatch( Node::NodeCommon* node )
  {
    auto module             = node->GetAsModule();
    auto function           = node->GetAsFunction();
    auto identifier         = node->GetAsIdentifier();
    auto int_literal        = node->GetAsIntLiteral();
    auto float_literal      = node->GetAsFloatLiteral();
    auto bool_literal       = node->GetAsBoolLiteral();
    auto string_literal     = node->GetAsStringLiteral();
    auto type_literal       = node->GetAsTypeLiteral();
    auto array_literal      = node->GetAsArrayLiteral();
    auto dict_literal       = node->GetAsDictLiteral();
    auto binary_op          = node->GetAsBinaryOp();
    auto for_init           = node->GetAsForInit();
    auto for_statement      = node->GetAsFor();
    auto while_statement    = node->GetAsWhile();
    auto when_statement     = node->GetAsWhen();
    auto whenever_statement = node->GetAsWhenever();
    auto if_statement       = node->GetAsIf();
    auto elseif_statement   = node->GetAsElseIf();
    auto assignment         = node->GetAsAssignment();
    auto function_call      = node->GetAsFunctionCall();
    auto process_call       = node->GetAsProcessMessage();
    auto return_statement   = node->GetAsReturn();
    auto array_subscript  = node->GetAsArraySubscript();

    if( module )
    {
      static_cast<BASECLASS_T*>(this)->Process(module);
    }
    else if( function )
    {
      static_cast<BASECLASS_T*>(this)->Process(function);
    }
    else if( identifier )
    {
      static_cast<BASECLASS_T*>(this)->Process(identifier);
    }
    else if( int_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(int_literal);
    }
    else if( float_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(float_literal);
    }
    else if( bool_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(bool_literal);
    }
    else if( string_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(string_literal);
    }
    else if( type_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(type_literal);
    }
    else if( array_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(array_literal);
    }
    else if( dict_literal )
    {
      static_cast<BASECLASS_T*>(this)->Process(dict_literal);
    }
    else if( binary_op )
    {
      static_cast<BASECLASS_T*>(this)->Process(binary_op);
    }
    else if( for_init )
    {
      static_cast<BASECLASS_T*>(this)->Process(for_init);
    }
    else if( for_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(for_statement);
    }
    else if( while_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(while_statement);
    }
    else if( when_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(when_statement);
    }
    else if( whenever_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(whenever_statement);
    }
    else if( if_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(if_statement);
    }
    else if( elseif_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(elseif_statement);
    }
    else if( assignment )
    {
      static_cast<BASECLASS_T*>(this)->Process(assignment);
    }
    else if( function_call )
    {
      static_cast<BASECLASS_T*>(this)->Process(function_call);
    }
    else if( process_call )
    {
      static_cast<BASECLASS_T*>(this)->Process(process_call);
    }
    else if( return_statement )
    {
      static_cast<BASECLASS_T*>(this)->Process(return_statement);
    }
    else if( array_subscript )
    {
      static_cast<BASECLASS_T*>(this)->Process(array_subscript);
    }
  }
};

} // namespace Eople
