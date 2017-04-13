#include "eople_exec_env.h"
#include "eople_log.h"
#include "optionparser.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <readline/readline.h>
#include <readline/history.h>

static std::string GetHistoryFilePath()
{
  std::string home = getenv("HOME");
  return home + "/.eople_history";
}

static void InitReadlineHistory()
{
  using_history();
  read_history(GetHistoryFilePath().c_str());
}

static void SaveReadlineHistory()
{
  write_history(GetHistoryFilePath().c_str());
  history_truncate_file(GetHistoryFilePath().c_str(), 1000);
}

static std::string GetLine(std::string prefix="")
{
  std::string line;

  char* input = readline(prefix.c_str());
  line = input;
  if(!line.empty())
  {
    add_history(input);
  }
  free(input);

  return line;
}

static int Eve( std::string filename, std::string entry_function, bool verbose )
{
  if( verbose )
  {
    Eople::Log::SetVerbosityLevel(Eople::Log::VerbosityLevel::ALL);
  }
  else
  {
    Eople::Log::SetVerbosityLevel(Eople::Log::VerbosityLevel::NO_DEBUG);
  }

  Eople::ExecutionEnvironment ee;

  if( filename.empty() )
  {
    Eople::Log::Print("|==========  Eople E.V.E. REPL Version 0.1 ===================\n");
    Eople::Log::Print("\n\n");
    Eople::Log::Print("For help, try typing:\n");
    Eople::Log::Print("    help()\n");
    Eople::Log::Print("\n");
    for(;;)
    {
      std::string command = GetLine(">>> ");
      if( !command.empty() && command.back() == ':' )
      {
        std::string line = GetLine("... ");
        while( !line.empty() )
        {
          command += '\n' + line;
          line = GetLine("... ");
        }
      }

      if( command == "exit()" || command == "quit()" )
      {
        ee.Shutdown();
        return 0;
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
        bool force_print = command[0] == '!';
        if( force_print || !ee.ExecuteBuffer(command) )
        {
          if( force_print )
          {
            command.erase(0, 1);
          }
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
        // else
        // {
        //   Eople::Log::Print("Ok.\n");
        // }
      }
      Eople::Log::Print("");
    }
  }

  try
  {
    ee.ImportModuleFromFile( filename );
    ee.ExecuteFunction( entry_function, false );
    ee.Shutdown();
    return 0;
  }
  catch(const char* s)
  {
    Eople::Log::Print("Unhandled exception: %s\n", s);
    return 1;
  }
}

int main(int argc, char* argv[])
{
  InitReadlineHistory();
  std::atexit(SaveReadlineHistory);

  enum  optionIndex { UNKNOWN, HELP, VERBOSE, VERSION };
  const option::Descriptor usage[] =
  {
    {UNKNOWN, 0, "", "",option::Arg::None, "USAGE: eople [options] [file] [entry_function]\n\n"
                                            "Options:" },
    {HELP, 0,"", "help",option::Arg::None, "  --help  \tPrint usage and exit." },
    {VERSION, 0,"V","version",option::Arg::None, "  --version, -V  \tDisplay version info." },
    {VERBOSE, 0,"v","verbose",option::Arg::None, "  --verbose, -v  \tEnable verbose output from eople runtime." },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                  "  eople hello.eop\n"
                                  "  eople --version\n"
                                  "  eople -v examples/area_test.eop Test::main\n" },
    {0,0,0,0,0,0}
  };

  // skip program name argv[0] if present
  argc-=(argc>0); argv+=(argc>0);
  option::Stats  stats(usage, argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

  if( parse.error() )
    return 1;

  if( options[HELP] )
  {
    option::printUsage(std::cout, usage);
    return 0;
  }

  if( options[VERSION] )
  {
    std::cout << "Eople 0.1.0" << std::endl;
    return 0;
  }

  bool verbose = options[VERBOSE] ? true : false;

  bool unknown_options = false;
  for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next())
    std::cout << "Unknown option: " << std::string(opt->name,opt->namelen) << "\n";
    unknown_options = false;

  if( unknown_options )
  {
    option::printUsage(std::cout, usage);
    return 1;
  }

  std::string filename = parse.nonOptionsCount() > 0 ? parse.nonOption(0) : "";
  std::string entry_function = parse.nonOptionsCount() > 1 ? parse.nonOption(1) : "main";

  return Eve(filename, entry_function, verbose);
}
