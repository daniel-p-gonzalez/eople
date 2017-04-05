#pragma once
#include "eople_core.h"
#include "eople_ast.h"

#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace Eople
{

// helper for creating c bindings
struct CModuleBuilder
{
  CModuleBuilder( std::string module_name )
    : module(NodeBuilder::GetModuleNode(module_name))
  {
    raw_module = module.get();
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t return_type )
  {
    std::vector<type_t> parameters;
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t p2, type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t p2, type_t p3,
                      type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t p2, type_t p3,
                      type_t p4, type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t p2, type_t p3,
                      type_t p4, type_t p5, type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    parameters.push_back(p5);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, type_t p1, type_t p2, type_t p3,
                      type_t p4, type_t p5, type_t p6, type_t return_type )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    parameters.push_back(p5);
    parameters.push_back(p6);
    return AddFunction( name, cfunc, parameters, return_type );
  }

  size_t AddFunction( std::string name, InstructionImpl cfunc, std::vector<type_t> &types, type_t return_type )
  {
    FunctionNode function = NodeBuilder::GetFunctionNode( name, 0 );
    function->is_c_call = true;

    for( size_t i = 0; i < types.size(); ++i )
    {
      std::ostringstream string_stream;
      string_stream << i;
      std::string parameter_name = "parameter_" + string_stream.str();
      // add parameter
      size_t parameter_id = function->symbols.GetTableEntryIndex(name + "::" + parameter_name, false );
      ExpressionNode parameter_node = NodeBuilder::GetIdentifierNode( parameter_name, parameter_id, 0 );

      // push parameter types
      function->TrySetExpressionType( parameter_node.get(), types[i] );
      function->parameters.push_back( std::move(parameter_node) );
    }

    // set return value
    function->TrySetValueType(return_type);

    size_t function_id = module->functions.size();

    module->functions.push_back(std::move(function));
    cfunctions.push_back(std::vector<InstructionImpl>());

    assert(module->functions.size() == cfunctions.size());
    cfunctions[function_id].push_back(cfunc);

    return function_id;
  }

  // TODO: support return value type specialization
  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc )
  {
    std::vector<type_t> parameters;
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1, type_t p2 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1, type_t p2,
                                  type_t p3 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1, type_t p2,
                                  type_t p3, type_t p4 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1, type_t p2,
                                  type_t p3, type_t p4, type_t p5 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    parameters.push_back(p5);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, type_t p1, type_t p2,
                                  type_t p3, type_t p4, type_t p5, type_t p6 )
  {
    std::vector<type_t> parameters;
    parameters.push_back(p1);
    parameters.push_back(p2);
    parameters.push_back(p3);
    parameters.push_back(p4);
    parameters.push_back(p5);
    parameters.push_back(p6);
    AddFunctionSpecialization( function_id, cfunc, parameters );
  }

  void AddFunctionSpecialization( size_t function_id, InstructionImpl cfunc, std::vector<type_t> &types )
  {
    assert(function_id < module->functions.size());
    // add specialization
    module->functions[function_id]->FindSpecializationForArgs( types );
    cfunctions[function_id].push_back(cfunc);
  }

  ModuleNode module;
  Node::Module* raw_module;
  std::vector<std::vector<InstructionImpl>> cfunctions;
};

} // namespace Eople
