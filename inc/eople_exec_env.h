#pragma once
#include "eople_ast.h"
#include "eople_parse.h"
#include "eople_type_infer.h"
#include "eople_vmcode_gen.h"
#include "eople_vm.h"
#include "eople_cmodule_builder.h"

#include <string>

namespace Eople
{

class ExecutionEnvironment
{
public:
  ExecutionEnvironment();
  ~ExecutionEnvironment();
  void Shutdown();
  bool ImportModuleFromFile( std::string file_name );
  bool ExecuteBuffer( std::string buffer );
  bool ExecuteFunction( std::string entry_function, bool spawn_in_new_process );
  void ListImportedFunctions();

private:
  void ImportBuiltins();

  VirtualMachine m_vm;
  CModuleBuilder m_builtins;
  typedef std::unique_ptr<ExecutableModule> ExecutableModulePtr;
  ExecutableModulePtr m_builtin_module;
  Node::Module   m_repl_module;
  std::vector<ExecutableModulePtr> m_loaded_modules;
  ASTree         m_ast;
  TypeInfer      m_type_infer;
  VMCodeGen    m_code_gen;
  Parser         m_parser;
  process_t       m_main_process;
};

} // namespace Eople
