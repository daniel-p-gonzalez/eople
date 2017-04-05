#include "eople.h"
#include "eople_log.h"
#include <cstdio>
#include <cstdarg>
#include <iostream>
#include <vector>
#include <thread>

#ifdef WIN32
#include "windows.h"
#endif

static void Eve( int argc, char* argv[] )
{
  Eople::Log::Print("|==========  Eople E.V.E.  Version 0.1 ===================\n");
  Eople::Log::Print("\n\n");
  Eople::Log::Print("For help, try typing:\n");
  Eople::Log::Print("    help()\n");
  Eople::Log::Print("\n\n");

  Eople::ExecutionEnvironment ee;

  if( argc < 2 )
  {
    for(;;)
    {
      Eople::Log::Print("eople> ");

      std::string command;
      std::string line;
      std::getline(std::cin, command);

      if( command == "exit()" || command == "quit()" )
      {
        Eople::Log::Print("Waiting for queued tasks to finish...\n");
        ee.Shutdown();
        break;
      }
      else if( command == "imported()" )
      {
        ee.ListImportedFunctions();
      }
      else if( command == "clear()" )
      {
        std::cout << std::string( 100, '\n' );
        Eople::Log::Print("|==========  Eople E.V.E.  Version 0.1 ===================\n|\n");
        Eople::Log::Print("\n\n");
     }
      else if( command == "help()" )
      {
        Eople::Log::Print("     To import an Eople source file, use:\n");
        Eople::Log::Print("       import(file_name.eop)\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     To list imported functions, type:\n");
        Eople::Log::Print("       imported()\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     To execute a function, use:\n");
        Eople::Log::Print("       call(function_name)\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     To spawn a function as a process, use:\n");
        Eople::Log::Print("       spawn(function_name)\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     To exit, type:\n");
        Eople::Log::Print("       exit()\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     To clear the screen, type:\n");
        Eople::Log::Print("       clear()\n");
        Eople::Log::Print("\n");
        Eople::Log::Print("     If this is a genuine emergency, please hang up and dial 9-1-1.\n");
        Eople::Log::Print("\n");
      }
      else if( command.find("import(") != std::string::npos )
      {
        if( command.back() != ')' )
        {
          Eople::Log::Print("\n     Missing closing ')'.");
          continue;
        }
        command.pop_back();
        auto start = command.find_first_of("(") + 1;
        ee.ImportModuleFromFile( &command.c_str()[start] );
        Eople::Log::Print("\n");
      }
      else if( command.find("call(") != std::string::npos )
      {
        if( command.back() != ')' )
        {
          Eople::Log::Print("\n     Missing closing ')'.");
          continue;
        }
        command.pop_back();
        auto start = command.find_first_of("(") + 1;
        ee.ExecuteFunction( &command.c_str()[start], false );
        Eople::Log::Print("\n");
      }
      else if( command.find("spawn(") != std::string::npos )
      {
        if( command.back() != ')' )
        {
          Eople::Log::Print("\n     Missing closing ')'.");
          continue;
        }
        command.pop_back();
        auto start = command.find_first_of("(") + 1;
        ee.ExecuteFunction( &command.c_str()[start], true );
        Eople::Log::Print("\n");
      }
      else if( command != "" )
      {
        command.push_back('\n');
        if( !ee.ExecuteBuffer(command) )
        {
          // maybe we're trying to get the value of an expression?
          command.pop_back();
          command = "print(" + command + ")";
          // swallow error messages second time through
          Eople::Log::SilenceErrors(true);
          Eople::AutoScope unsilence_log( []{ Eople::Log::SilenceErrors(false); } );
          if( !ee.ExecuteBuffer(command) )
          {
            unsilence_log.Trigger();
            Eople::Log::Print("\n");
            Eople::Log::Print("     Sorry, something didn't go quite as planned.\n");
            Eople::Log::Print("\n");
            Eople::Log::Print("     For help, try typing:\n");
            Eople::Log::Print("         help()\n");
            Eople::Log::Print("\n");
          }
        }
        else
        {
          Eople::Log::Print("Ok.\n");
        }
      }
      Eople::Log::Print("");
    }
  }

  try
  {
    if( argc == 2 )
    {
      Eople::Log::Print("Invalid argument count. Please specify entry function to execute.\n");;
    }
    else if( argc == 3 )
    {
      std::cout << argv[1] << std::endl;
      std::cout << argv[2] << std::endl;
      ee.ImportModuleFromFile( argv[1] );
      ee.ExecuteFunction( argv[2], false );
      ee.Shutdown();
    }
  }
  catch(const char* s)
  {
    Eople::Log::Print("Unhandled exception: %s\n", s);;
  }
}

int main(int argc, char* argv[])
{
  Eve(argc, argv);

  return 0;
}
