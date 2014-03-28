#include "eople_ast.h"

namespace Eople
{
namespace Node
{

/////////////////////
//
// FunctionCall::
//
/////////////////////
  std::string FunctionCall::GetMangledName( size_t parent_specialization )
  {
    std::string name = ident->GetAsIdentifier()->name + " ";
    name += '0' + (char)parent_specialization;
    name += " ";
    name += '0' + (char)GetSpecialization( parent_specialization );
    return name;
  }

  type_t FunctionCall::GetReturnType( size_t parent_specialization )
  {
    if( parent_specialization >= return_specializations.size() )
    {
      return_specializations.resize(parent_specialization+1, TypeBuilder::GetNilType());
    }

    return return_specializations[parent_specialization];
  }

  bool FunctionCall::TrySetReturnType( size_t parent_specialization, type_t type )
  {
    if( parent_specialization >= return_specializations.size() )
    {
      return_specializations.resize(parent_specialization+1, TypeBuilder::GetNilType());
    }

    bool mismatch = (return_specializations[parent_specialization] != TypeBuilder::GetNilType())
                          && (return_specializations[parent_specialization] != type);
    return_specializations[parent_specialization] = type;

    if( mismatch )
    {
      throw "Type Mismatch";
    }

    return mismatch;
  }

  size_t FunctionCall::GetSpecialization( size_t parent_specialization )
  {
    if( parent_specialization >= call_specializations.size() )
    {
      call_specializations.resize(parent_specialization+1, 0);
    }

    return call_specializations[parent_specialization];
  }

  void FunctionCall::SetSpecialization( size_t parent_specialization, size_t specialization )
  {
    if( parent_specialization >= call_specializations.size() )
    {
      call_specializations.resize(parent_specialization+1, 0);
    }

    call_specializations[parent_specialization] = specialization;
  }

/////////////////////
//
// ProcessMessage::
//
/////////////////////
  std::string ProcessMessage::GetMangledName( size_t parent_specialization )
  {
    auto process_ident = process->GetAsIdentifier();
    auto function_call = message->GetAsFunctionCall();
    std::string name = process_ident->name + "::" + function_call->GetName() + " ";
    name += '0' + (char)parent_specialization;
    name += " ";
    name += '0' + (char)message->GetAsFunctionCall()->GetSpecialization( parent_specialization );
    return name;
  }

/////////////////////
//
// Function::
//
/////////////////////
  TableEntry* Function::GetTableEntryForName( std::string key )
  {
    SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
    size_t symbol_id = sym.GetTableEntryIndex(scope_name + key, false, true);
    if( symbol_id != symbols.NOT_FOUND )
    {
      return &sym.TableEntryFromId(symbol_id);
    }
    if( context )
    {
      return context->GetTableEntryForName(key);
    }

    assert(false);
    return nullptr;
  }

  TableEntry* Function::GetTableEntry( Expression* expr )
  {
    auto identifier    = expr->GetAsIdentifier();
    auto literal       = expr->GetAsLiteral();
    auto array_literal = expr->GetAsArrayLiteral();

    std::string key = identifier ? identifier->name : (array_literal ? array_literal->ident->GetAsIdentifier()->name : (literal ? literal->value_string : ""));
    return GetTableEntryForName(key);
  }

  EntryId Function::GetEntryIdForName( std::string key, u32& depth )
  {
    SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
    size_t symbol_id = sym.GetTableEntryIndex(scope_name + key, false, true);
    //
    // TODO: Make sure this works for class specializations
    //
    if( symbol_id != symbols.NOT_FOUND )
    {
      return symbol_id;
    }
    if( context )
    {
      // nesting depth (needed for calculating stack index)
      ++depth;
      return context->GetEntryIdForName(key, depth);
    }

    assert(false);
    return symbols.NOT_FOUND;
  }

  EntryId Function::GetEntryId( Expression* expr, u32& depth )
  {
    auto identifier    = expr->GetAsIdentifier();
    auto literal       = expr->GetAsLiteral();
    auto array_literal = expr->GetAsArrayLiteral();

    depth = 0;

    std::string key = identifier ? identifier->name : (array_literal ? array_literal->ident->GetAsIdentifier()->name : (literal ? literal->value_string : ""));
    return GetEntryIdForName(key, depth);
  }

  type_t Function::GetValueType()
  {
    return (current_specialization == 0) ? return_type : return_specializations[current_specialization-1];
  }

  // throws on type mismatch
  bool Function::TrySetValueType( type_t type )
  {
    type_t &r_type = (current_specialization == 0) ? return_type : return_specializations[current_specialization-1];
    bool mismatch = (r_type != TypeBuilder::GetNilType()) && (r_type != type);

    if( mismatch )
    {
      throw "Type Mismatch";
    }

    r_type = type;
    return mismatch;
  }

  // throws on type mismatch
  bool Function::TrySetExpressionType( Expression* expr, type_t type )
  {
    auto identifier = expr->GetAsIdentifier();

    if( identifier )
    {
      TableEntry* entry = GetTableEntry(expr);
      if( !entry )
      {
        // entry should always exist at this point
        assert(false);
        return true;
      }
      type_t ident_type = entry->type;
      bool mismatch = (ident_type != TypeBuilder::GetNilType()) && (ident_type != type);

      if( mismatch )
      {
        throw "Type Mismatch";
      }

      entry->type = type;
      return mismatch;
    }

    return expr->TrySetValueType( type );
  }

  // throws on type mismatch
  void Function::TrySetExpressionVaryingType( Expression* expr, type_t type )
  {
    TableEntry* entry = GetTableEntry(expr);
    if( !entry )
    {
      // entry should always exist at this point
      assert(false);
    }

    // TODO: BUG - this actually needs to replace the original type, not modify the type in place
    entry->type->SetVaryingType(type);
  }

  // doesn't check for identifier type mismatch, but can still throw if literal type doesn't match
  void Function::SetExpressionValueType( Expression* expr, type_t type )
  {
    auto identifier = expr->GetAsIdentifier();

    if( identifier )
    {
      TableEntry* entry = GetTableEntry(expr);
      if( !entry )
      {
        // entry should always exist at this point
        assert(false);
        return;
      }
      entry->type = type;
      return;
    }

    // mismatch with literals can still throw
    expr->TrySetValueType( type );
  }

  type_t Function::GetExpressionType( Expression* expr )
  {
    auto identifier = expr->GetAsIdentifier();

    if( identifier )
    {
      // TODO: this is a kludge to get static guards off the ground
      if( identifier->is_guard )
      {
        return TypeBuilder::GetBoolType();
      }

      TableEntry* entry = GetTableEntry(expr);
      if( !entry )
      {
        // entry should always exist at this point
        assert(false);
        return TypeBuilder::GetNilType();
      }

      return entry->type;
    }

    auto function_call = expr->GetAsFunctionCall();
    if( function_call )
    {
      return function_call->GetReturnType( current_specialization );
    }

    return expr->GetValueType();
  }

  type_t Function::GetExpressionVaryingType( Expression* expr )
  {
    TableEntry* entry = GetTableEntry(expr);
    if( !entry )
    {
      // entry should always exist at this point
      assert(false);
      return TypeBuilder::GetNilType();
    }

    return entry->type->GetVaryingType();
  }

  bool Function::TrySetCallReturnType( FunctionCall* function_call, type_t type )
  {
    return function_call->TrySetReturnType( current_specialization, type );
  }

  type_t Function::GetCallReturnType( FunctionCall* function_call )
  {
    return function_call->GetReturnType( current_specialization );
  }

  size_t Function::GetStackIndex( Expression* expr )
  {
    auto identifier    = expr->GetAsIdentifier();
    auto literal       = expr->GetAsLiteral();
    auto array_literal = expr->GetAsArrayLiteral();
    auto function_call = expr->GetAsFunctionCall();
    auto process_call   = expr->GetAsProcessMessage();

    if( identifier || literal || array_literal )
    {
      u32 match_depth;
      EntryId entry_id = GetEntryId(expr, match_depth);
      if( entry_id == symbols.NOT_FOUND )
      {
        // entry should always exist at this point
        assert(false);
        return true;
      }

      SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
      size_t stack_index = 0;
      // if symbol is in this function scope, grab register index
      if( match_depth == 0 )
      {
        stack_index = sym.GetRegisterIndex(entry_id);
      }
      size_t offset = 0;
      u32    depth  = 1;
      Function* parent = context;
      while( parent )
      {
        // If there is a parent of the containing scope, add its size to stack offset if the scope is 'outer' in relation to the thing we're searching for.
        // eg. if we were searching for a local variable in a member function, the member function will sit on top of the class object
        //       on the stack, so add the size of the class object to the offset
        if( depth > match_depth )
        {
          SymbolTable &parent_sym = parent->current_specialization ? parent->specializations[parent->current_specialization-1] : parent->symbols;
          offset += parent_sym.symbol_count();
        }
        else if( depth == match_depth )
        {
          // if symbol is in this function scope, grab register index
          SymbolTable &parent_sym = parent->current_specialization ? parent->specializations[parent->current_specialization-1] : parent->symbols;
          stack_index = parent_sym.GetRegisterIndex(entry_id);
        }
        parent = parent->context;
      }
      return stack_index + offset;
    }
    else if( function_call )
    {
      std::string name = function_call->GetMangledName(current_specialization);
      u32 match_depth = 0;
      EntryId entry_id = GetEntryIdForName(name, match_depth);
      if( entry_id == symbols.NOT_FOUND )
      {
        // entry should always exist at this point
        assert(false);
        return true;
      }

      size_t offset = 0;
      Function* parent = context;
      // a function call's reference on the stack is always local to the containing function, so add all outer offsets
      assert(match_depth == 0);
      SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
      size_t stack_index = sym.GetRegisterIndex(entry_id);
      while( parent )
      {
        SymbolTable &parent_sym = parent->current_specialization ? parent->specializations[parent->current_specialization-1] : parent->symbols;
        offset += parent_sym.symbol_count();
        parent = parent->context;
      }
      return stack_index + offset;
    }
    else if( process_call )
    {
      std::string name = process_call->GetMangledName(current_specialization);
      u32 match_depth = 0;
      EntryId entry_id = GetEntryIdForName(name, match_depth);
      if( entry_id == symbols.NOT_FOUND )
      {
        // entry should always exist at this point
        assert(false);
        return true;
      }

      size_t offset = 0;
      Function* parent = context;
      // a function call's reference on the stack is always local to the containing function, so add all outer offsets
      assert(match_depth == 0);
      SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
      size_t stack_index = sym.GetRegisterIndex(entry_id);
      while( parent )
      {
        SymbolTable &parent_sym = parent->current_specialization ? parent->specializations[parent->current_specialization-1] : parent->symbols;
        offset += parent_sym.symbol_count();
        parent = parent->context;
      }
      return stack_index + offset;
    }

    assert(false);
    return 0;
  }

  void Function::RegisterFunctionCall( FunctionCall* function_call )
  {
    std::string name = function_call->GetMangledName(current_specialization);
    SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
    sym.GetTableEntryIndex(name, true);
  }

  void Function::RegisterProcessMessage( ProcessMessage* process_call )
  {
    std::string name = process_call->GetMangledName(current_specialization);
    SymbolTable &sym = current_specialization ? specializations[current_specialization-1] : symbols;
    sym.GetTableEntryIndex(name, true);
  }

  void Function::SetCurrentSpecialization( size_t index )
  {
    current_specialization = index;
  }

  // Make the class names of any process reference parameters match those in the function call arguments
  void Function::SetParameterClassNames( FunctionCall* function_call, Function* caller_scope, std::vector<type_t> &arg_types, size_t specialization )
  {
    assert( parameters.size() == function_call->arguments.size() );

    size_t old_specialization = current_specialization;
    current_specialization = specialization;

    for( size_t i = 0; i < parameters.size(); ++i )
    {
      if( arg_types[i]->type != ValueType::PROCESS )
      {
        continue;
      }
      TableEntry* parameter_entry = GetTableEntry(parameters[i].get());
      TableEntry* arg_entry       = caller_scope->GetTableEntry(function_call->arguments[i].get());
      if( parameter_entry && arg_entry )
      {
        parameter_entry->target_name = arg_entry->target_name;
      }
    }

    current_specialization = old_specialization;
  }

  size_t Function::FindSpecializationForArgs( std::vector<type_t> &arg_types )
  {
    assert( arg_types.size() == parameters.size() );

    size_t temp_specialization = current_specialization;

    for( size_t s = 0; s < specializations.size()+1; ++s )
    {
      bool match = true;

      current_specialization = s;
      for( size_t arg = 0; arg < arg_types.size(); ++arg )
      {
        type_t parameter_type = GetExpressionType( parameters[arg].get() );
        // TODO: surely there's a simpler way to express this
        // check for match, with parameter type 'ANY' matching any given arg type
        if( parameter_type != TypeBuilder::GetAnyType() && arg_types[arg] != parameter_type
          && ((parameter_type->GetVaryingType() != TypeBuilder::GetAnyType()) || (arg_types[arg]->type != parameter_type->type)) )
        {
          match = false;
          break;
        }
      }

      size_t index = current_specialization;
      current_specialization = temp_specialization;

      if( match )
      {
        return index;
      }
    }

    specializations.push_back(symbols);
    is_template.push_back(true);
    size_t new_index = specializations.size();
    return_specializations.push_back(TypeBuilder::GetNilType());

    // set parameter types to match arg types
    current_specialization = new_index;
    for( size_t i = 0; i < arg_types.size(); ++i )
    {
      SetExpressionValueType( parameters[i].get(), arg_types[i] );
    }

    // reset specialization
    current_specialization = temp_specialization;
    return new_index;
  }

//
// ASTDispatcher
//
//void WalkNode( ast_walker walker, Node::NodeCommon* node, Node::Function* scope )
//{
//  auto module         = node->GetAsModule();
//  auto function       = node->GetAsFunction();
//  auto identifier     = node->GetAsIdentifier();
//  auto int_literal    = node->GetAsIntLiteral();
//  auto float_literal  = node->GetAsFloatLiteral();
//  auto bool_literal   = node->GetAsBoolLiteral();
//  auto string_literal = node->GetAsStringLiteral();
//  auto array_literal  = node->GetAsArrayLiteral();
//  auto function_call  = node->GetAsFunctionCall();
//  auto process_call   = node->GetAsProcessMessage();
//
//  if( module )
//  {
//    ast_walker module_walker = walker->Process(module);
//    for( auto &func : module->classes )
//    {
//      WalkNode( std::move(module_walker), func.get(), scope );
//    }
//
//    for( auto &func : module->functions )
//    {
//      WalkNode( std::move(module_walker), func.get(), scope );
//    }
//  }
//  else if( function )
//  {
//    ast_walker function_walker = walker->Process(function);
//    for( auto &func : function->functions )
//    {
//      WalkNode( std::move(function_walker), func.get(), function );
//    }
//    for( auto &statement : function->body )
//    {
//      WalkNode( std::move(function_walker), statement.get(), function );
//    }
//  }
//  else if( identifier )
//  {
//    walker->Process( identifier, scope->GetTableEntry(identifier) );
//  }
//  else if( int_literal )
//  {
//    walker->Process( int_literal, scope->GetTableEntry(int_literal) );
//  }
//  else if( float_literal )
//  {
//    walker->Process( float_literal, scope->GetTableEntry(float_literal) );
//  }
//  else if( bool_literal )
//  {
//    walker->Process( bool_literal, scope->GetTableEntry(bool_literal) );
//  }
//  else if( string_literal )
//  {
//    walker->Process( string_literal, scope->GetTableEntry(string_literal) );
//  }
//  else if( array_literal )
//  {
//    ast_walker array_literal_walker = walker->Process( array_literal, scope->GetTableEntry(array_literal) );
//    for( auto &element : array_literal->elements )
//    {
//      WalkNode( std::move(array_literal_walker), element.get(), scope );
//    }
//  }
//  else if( function_call )
//  {
//    ast_walker function_call_walker = walker->Process( function_call );
//    for( auto &arg : function_call->arguments )
//    {
//      WalkNode( std::move(function_call_walker), arg.get(), scope );
//    }
//  }
//  else
//  {
//    throw "Unhandled node type in ASTDispatcher::WalkNode.";
//  }
//}
//
//void WalkTree( ast_walker walker, ASTree* astree )
//{
//  for( auto &node : astree->modules )
//  {
//    WalkNode( std::move(walker), node.get(), nullptr );
//  }
//}

} // namespace Node
} // namespace eople
