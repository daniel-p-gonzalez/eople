#include "eople_exec_env.h"
#include "eople_core.h"
#include "eople_binop.h"
#include "eople_control_flow.h"
#include "eople_stdlib.h"
#include "eople_lex.h"
#include "eople_parse.h"
#include "eople_type_infer.h"
#include "eople_bytecode_gen.h"
#include "eople_vm.h"
#include "utf8.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstring>

#include <iostream>
#include <vector>
#include <iterator>
#include <map>
#include <thread>
#include <string>
#include <sstream>
#include <ctime>
#include <clocale>

namespace Eople
{

// static const char* s_calming_quotes[] = { "When you rest, you are a king surveying your estate. Look at the woodland, the peacocks on the lawn.\n"
//                                           "Be the king of your own calm kingdom.",
//                                           "When you're feeling under pressure, do something different. Roll up your sleeves, or eat an orange.",
//                                           "Be on the look out for things that make you laugh. If you see nothing worth laughing at,\npretend you see it, then laugh.",
//                                           "Whenever you're in a tight spot, try to imagine yourself marooned on a beautiful desert island.",
//                                           "Add a drop of lavender to your bath, and soon, you'll soak yourself calm!" };

bool loadFile( std::string file_name, const char* &buffer, const char* &buffer_end )
{
  FILE* file = nullptr;

  if( !FOPEN( file, file_name.c_str(), "r" ) )
  {
    buffer = NULL;
    return false;
  }

  fseek( file, -1, SEEK_END );
  size_t buffer_size = ftell(file);
  rewind(file);

  buffer = new char[buffer_size];
  buffer_size = fread( (void*)buffer, 1, buffer_size, file );
  fclose(file);
  buffer_end = buffer + buffer_size;

  const char* error_location = nullptr;
  utf8::utf_error error = utf8::validate(buffer, buffer_end, error_location);
  if(error != utf8::utf_error::UTF8_OK)
  {
    u32 line = 1;
    while( buffer != error_location )
    {
      if( *buffer++ == '\n' )
      {
        ++line;
      }
    }
    Log::Print("UTF-8 error: '%s' At line: %lu\n", utf8_error_to_string(error).c_str(), line);
    return false;
  }

  return true;
}

ExecutionEnvironment::ExecutionEnvironment()
:
  m_type_infer(), m_code_gen(), m_parser(), m_builtins("builtins"), m_repl_module("repl")
{
  curl_global_init(CURL_GLOBAL_ALL);

  m_vm.Run();
  m_main_process = m_vm.GenerateUniqueProcess();

  ImportBuiltins();
  m_repl_module.imported.push_back(m_builtins.raw_module);
}

ExecutionEnvironment::~ExecutionEnvironment()
{
}

void ExecutionEnvironment::Shutdown()
{
  m_vm.Shutdown();
}

void ExecutionEnvironment::ImportBuiltins()
{
  auto string_type  = TypeBuilder::GetPrimitiveType(ValueType::STRING);
  auto int_type     = TypeBuilder::GetPrimitiveType(ValueType::INT);
  auto float_type   = TypeBuilder::GetPrimitiveType(ValueType::FLOAT);
  auto bool_type    = TypeBuilder::GetBoolType();
  auto any_type     = TypeBuilder::GetAnyType();
  auto array_type   = TypeBuilder::GetArrayType(any_type);
  auto kind_type    = TypeBuilder::GetKindType(any_type);
  auto promise_type = TypeBuilder::GetPromiseType(any_type);
  auto promise_string_type = TypeBuilder::GetPromiseType(string_type);

  auto array_string_type   = TypeBuilder::GetArrayType(string_type);
  auto array_int_type      = TypeBuilder::GetArrayType(int_type);
  auto array_float_type    = TypeBuilder::GetArrayType(float_type);
  //auto array_bool_type     = TypeBuilder::GetArrayType(bool_type);
  auto array_array_type    = TypeBuilder::GetArrayType(array_type);

  auto print_func = m_builtins.AddFunction( "print", Instruction::PrintF, TypeBuilder::GetPrimitiveType(ValueType::FLOAT),
                                            TypeBuilder::GetNilType() );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintI, TypeBuilder::GetPrimitiveType(ValueType::INT) );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintS, string_type );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintSArr, array_string_type );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintIArr, array_int_type );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintFArr, array_float_type );
  m_builtins.AddFunctionSpecialization( print_func, Instruction::PrintSPromise, promise_string_type );

  m_builtins.AddFunction( "get_line", Instruction::GetLine, string_type );
  m_builtins.AddFunction( "array", Instruction::ArrayConstructor, kind_type, array_type );

  auto top_func = m_builtins.AddFunction( "top", Instruction::ArrayTopString, array_string_type, string_type );
  m_builtins.AddFunctionSpecialization( top_func, Instruction::ArrayTopArray, array_array_type );
  m_builtins.AddFunctionSpecialization( top_func, Instruction::ArrayTop, array_type );

  auto push_func = m_builtins.AddFunction( "push", Instruction::ArrayPushString, array_string_type, string_type,
                                                          TypeBuilder::GetNilType() );
  m_builtins.AddFunctionSpecialization( push_func, Instruction::ArrayPushArray, array_array_type, array_type );
  m_builtins.AddFunctionSpecialization( push_func, Instruction::ArrayPush, array_type, any_type );

  m_builtins.AddFunction( "clear", Instruction::ArrayClear, array_type, TypeBuilder::GetNilType() );
  m_builtins.AddFunction( "pop", Instruction::ArrayPop, array_type, TypeBuilder::GetNilType() );
  m_builtins.AddFunction( "size", Instruction::ArraySize, array_type, TypeBuilder::GetPrimitiveType(ValueType::INT) );
  m_builtins.AddFunction( "get_time", Instruction::GetTime, TypeBuilder::GetPrimitiveType(ValueType::FLOAT) );
  m_builtins.AddFunction( "sleep", Instruction::SleepMilliseconds, TypeBuilder::GetPrimitiveType(ValueType::INT), TypeBuilder::GetNilType() );
  m_builtins.AddFunction( "after", Instruction::Timer, TypeBuilder::GetPrimitiveType(ValueType::INT), promise_type );
  m_builtins.AddFunction( "is_ready", Instruction::Ready, promise_type, TypeBuilder::GetBoolType() );
  m_builtins.AddFunction( "get_value", Instruction::GetValue, promise_type, TypeBuilder::GetNilType() );
  m_builtins.AddFunction( "to_float", Instruction::IntToFloat, TypeBuilder::GetPrimitiveType(ValueType::INT), TypeBuilder::GetPrimitiveType(ValueType::FLOAT) );
  m_builtins.AddFunction( "to_int", Instruction::FloatToInt, TypeBuilder::GetPrimitiveType(ValueType::FLOAT), TypeBuilder::GetPrimitiveType(ValueType::INT) );
  auto to_string_func = m_builtins.AddFunction( "to_string", Instruction::IntToString, TypeBuilder::GetPrimitiveType(ValueType::INT),
                                                              string_type );
  m_builtins.AddFunctionSpecialization( to_string_func, Instruction::FloatToString, TypeBuilder::GetPrimitiveType(ValueType::FLOAT) );
  m_builtins.AddFunctionSpecialization( to_string_func, Instruction::PromiseToString, promise_type );

  m_builtins.AddFunction( "get_url", Instruction::GetURL, string_type, string_type );
  m_builtins.AddFunction( "get_url_creds", Instruction::GetURL_USERPWD, string_type, string_type, string_type );

  m_ast.modules.push_back( std::move(m_builtins.module) );

  m_builtin_module = m_code_gen.InitModule(m_ast.modules[0].get());
  m_code_gen.ImportCFunctions( m_builtin_module.get(), m_builtins.cfunctions );
}

bool ExecutionEnvironment::ImportModuleFromFile( std::string file_name )
{
  const char* buffer;
  const char* buffer_end;

  if( !loadFile( file_name, buffer, buffer_end ) )
  {
    Log::Error("eve> Opening of file '%s' failed.\n", file_name.c_str());
    return false;
  }

  const char* file_no_path = file_name.c_str();
  const char* last_slash = std::strrchr(file_no_path, '/');
  if( last_slash )
    file_no_path = last_slash + 1;

  Log::Debug("eve> Importing module '%s'.\n", file_no_path);

  Log::PushContext(file_name.c_str());

  typedef HighResClock clock;
  auto start_time = clock::now();
  // initialize lexer with input buffer
  Lexer lexer( buffer, buffer_end );
  // create new module for file
  m_ast.modules.push_back(NodeBuilder::GetModuleNode(file_no_path));
  auto module = m_ast.modules.back().get();
  // auto import builtins into module
  module->imported.push_back(m_builtins.raw_module);
  // parse input / transform into ast, and store in module
  m_parser.ParseModule(lexer, module);

  delete[] buffer;

  Log::PopContext();

  u32 error_count = m_parser.GetErrorCount();

  auto total_ms = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start_time).count();
  if( total_ms < 1000000 )
  {
    Log::Debug( "eve> Total time for import: %f ms.\n", (f64)total_ms/1000.0 );
  }
  else
  {
    Log::Debug( "eve> Total time for import: %f seconds.\n", (f64)total_ms/1000000.0 );
  }

  if( error_count )
  {
    Log::Error("\n");
    Log::Error("eve> Import of '%s' failed with %d errors.\n", file_name.c_str(), error_count);
    // How many people would get a Black Books reference?
    // Log::Error("eve> I'm sorry: import of '%s' has failed with %d errors.\n", file_name.c_str(), error_count);
    // Log::Error("\n");
    // Log::Error("eve> As Manny would say:\n");
    // srand( (u32)time(nullptr) );
    // size_t quote_count = sizeof(s_calming_quotes)/sizeof(s_calming_quotes[0]);
    // Log::Error("%s\n", s_calming_quotes[rand()%quote_count]);
    Log::Error("\n");
  }
  else
  {
    Log::Debug("eve> Import of '%s' succeeded. 0 Errors.\n", file_name.c_str());
  }

  return !error_count;
}

bool ExecutionEnvironment::ExecuteBuffer( std::string code )
{
  const char* buffer = code.c_str();
  const char* buffer_end = buffer + code.size();

  //typedef HighResClock clock;
  //auto start_time = clock::now();
  Lexer lexer( buffer, buffer_end );
  m_parser.IncrementalParse(lexer, &m_repl_module);

  u32 error_count = m_parser.GetErrorCount();
  if( !error_count )
  {
    m_type_infer.IncrementalInfer(&m_repl_module);
    error_count += m_type_infer.GetErrorCount();
  }

  if( !error_count )
  {
    // kludge to get when/whenever statements working with persistant vars in repl
    m_repl_module.functions[0]->is_repl = true;
    m_repl_module.functions[0]->is_template[0] = false;
    m_loaded_modules.push_back(m_code_gen.InitModule(&m_repl_module));
    auto &executable_module = m_loaded_modules.back();
    executable_module->imported.push_back(m_builtin_module.get());
    m_code_gen.GenModule(executable_module.get());
    error_count = m_code_gen.GetErrorCount();

    if( !error_count && executable_module->functions.size() )
    {
      // hot swap support
      if( m_loaded_modules.size() > 1 )
      {
        auto &last_module = m_loaded_modules[m_loaded_modules.size()-2];
        for( auto &func : executable_module->functions )
        {
          for( auto &old_func : last_module->functions )
          {
            if( func->name == old_func->name )
            {
              old_func->updated_function = func.get();
              break;
            }
          }
        }
      }
      m_vm.ExecuteFunctionIncremental( CallData(executable_module->functions[0].get(), m_main_process) );
    }
  }

  return !error_count;
}

bool ExecutionEnvironment::ExecuteFunction( std::string function_name, bool spawn_in_new_process )
{
  Log::Debug("eve> Spawning process '%s'.\n", function_name.c_str());

  typedef HighResClock clock;
  auto start_time = clock::now();

  bool entry_found = false;
  Function* function = nullptr;
  u32 error_count = 0;

  // check if we've already run code gen for this function
  // TODO: don't do linear search through all modules/functions
  for( auto &module : m_loaded_modules )
  {
    for( auto &func : module->functions )
    {
      if( func->name == function_name )
      {
        Log::Debug("eve> Found cached function.\n");
        function = func.get();
        break;
      }
    }
  }

  // didn't find compiled function, run code gen
  if( !function )
  {
    auto module = m_ast.GetModuleForFunction( function_name );
    if( module )
    {
      // walk over ast starting from entry point, and propagate types
      m_type_infer.InferTypes( module, function_name );
//      m_type_infer.InferModuleTypes(module);
      error_count = m_type_infer.GetErrorCount();
    }
    else
    {
      Log::Error("eve> Could not find function '%s'.\n", function_name.c_str());
      ++error_count;
    }

    if( !error_count )
    {
      // prepare byte code module for code gen
      m_loaded_modules.push_back(m_code_gen.InitModule(module));
      auto &executable_module = m_loaded_modules.back();
      executable_module->imported.push_back(m_builtin_module.get());
      // transform ast to byte code
      m_code_gen.GenModule(executable_module.get());
      error_count = m_code_gen.GetErrorCount();

      if( !error_count )
      {
        // function entry function in generated byte code module
        for( size_t i = 0; i < executable_module->functions.size(); ++i )
        {
          if( executable_module->functions[i]->name == function_name )
          {
            function = executable_module->functions[i].get();
            entry_found = true;
            break;
          }
        }
      }
    }

    auto total_ms = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start_time).count();
    if( total_ms < 1000000 )
    {
      Log::Debug( "eve> Total time for code generation: %f ms.\n", (f64)total_ms/1000.0 );
    }
    else
    {
      Log::Debug( "eve> Total time for code generation: %f seconds.\n", (f64)total_ms/1000000.0 );
    }

    if( error_count )
    {
      Log::Error("\n");
      Log::Error("eve> Code generation for '%s' failed with %d errors.\n", function_name.c_str(), error_count);
      // How many people would get a Black Books reference?
      // Log::Error("eve> I'm sorry: code generation of '%s' has failed with %d errors.\n", function_name.c_str(), error_count);
      // Log::Error("\n");
      // Log::Error("eve> As Manny would say:\n");
      // srand( (u32)time(nullptr) );
      // size_t quote_count = sizeof(s_calming_quotes)/sizeof(s_calming_quotes[0]);
      // Log::Error("%s\n", s_calming_quotes[rand()%quote_count]);
      Log::Error("\n");
    }

    if( !error_count )
    {
      Log::Debug("eve> Code generation of %s succeeded. %d Errors.\n", function_name.c_str(), error_count);

      if( !entry_found )
      {
        Log::Error("eve> Could not find function: %s.\n", function_name.c_str());
      }
    }
  }

  Log::ClearContext();
  Log::Debug("\n");

  if( function )
  {
    auto start_time = clock::now();
    if( spawn_in_new_process )
    {
      // create new process to execute entry function
      m_vm.SendMessage( CallData(function, m_vm.GenerateUniqueProcess()) );
    }
    else
    {
      // execute entry function in the current process
      m_vm.ExecuteFunction( CallData(function, m_main_process) );
    }
    auto total_ms = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start_time).count();
    Log::Debug("\neve> Execution completed in %f seconds.\n", (f64)total_ms/1000000.0 );
  }

  return !error_count;
}

void ExecutionEnvironment::ListImportedFunctions()
{
  Log::Print("eve> Imported functions:\n");
  Log::AutoContext tab("\t");

  for( auto &module : m_ast.modules )
  {
    Log::Print("Module(%s):\n", module->name.c_str());
    Log::AutoContext tab("\t");
    for( auto &function : module->functions )
    {
      Log::Print("%s\n", function->name.c_str());
    }
  }
}

} // namespace Eople
