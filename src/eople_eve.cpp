#include "eople.h"
#include "eople_log.h"
#include "eople_ui.h"
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <thread>

#ifdef WIN32
#include "windows.h"
#endif

static void Eve( int argc, char* argv[], Eople::AppWindow* app )
{
  Eople::Log::Print("|==========  Eople E.V.E.  Version 0.1 ===================\n");
  Eople::Log::Print("\n\n");
  Eople::Log::Print("For help, try typing:\n");
  Eople::Log::Print("    help()\n");
  Eople::Log::Print("\n\n");

  Eople::ExecutionEnvironment ee;
  // TODO: This is highly questionable... need a clearer relationship between executing code and terminal output
  ee.SetCurrentTextBuffer(app->GetTextBuffer());

  if( argc < 2 )
  {
    for(;;)
    {
      std::shared_ptr<Eople::TextBuffer> text_buffer = app->GetTextBuffer().lock();
      if( !text_buffer )
      {
        break;
      }
      Eople::Log::Print("eople> ");
      std::string command;
      {
        command = text_buffer->GetInput();
      }
      if( command == "exit()" || command == "quit()" )
      {
        Eople::Log::Print("Waiting for queued tasks to finish...");
        ee.Shutdown();
        app->Shutdown();
        break;
      }
      else if( command == "imported()" )
      {
        ee.ListImportedFunctions();
      }
      else if( command == "clear()" )
      {
        text_buffer->Clear();
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
      ee.ImportModuleFromFile( argv[1] );
      ee.ExecuteFunction( argv[2], false );
    }
  }
  catch(const char* s)
  {
    Eople::Log::Print("Unhandled exception: %s\n", s);;
  }
}

int main(int argc, char* argv[])
{
  Eople::AppWindow app;

  // TODO: dependencies here are a bit tied in knots
  std::thread eve_thread( Eve, argc, argv, &app );
  app.EnterMessageLoop();
  eve_thread.join();

  return 0;
}

#ifdef WIN32
int CALLBACK WinMain( HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/ )
{
  return main(__argc, __argv);
}
#endif
