#include "eople_ast.h"
#include "eople_symbol_table.h"

namespace Eople
{

size_t SymbolTable::NOT_FOUND = 0xFFFFFFFF;


TypeBuilder::PrimitiveTypeVector BuildPrimitiveTypes()
{
  TypeBuilder::PrimitiveTypeVector types;

  types.push_back( std::unique_ptr<Type>(new Type(ValueType::NIL)) );
  types.push_back( std::unique_ptr<Type>(new Type(ValueType::FLOAT)) );
  types.push_back( std::unique_ptr<Type>(new Type(ValueType::INT)) );
  types.push_back( std::unique_ptr<Type>(new Type(ValueType::BOOL)) );
  types.push_back( std::unique_ptr<Type>(new Type(ValueType::STRING)) );

  return types;
}

TypeBuilder::PrimitiveTypeVector TypeBuilder::primitive_types = BuildPrimitiveTypes();
TypeBuilder::ProcessTypeMap       TypeBuilder::process_types;
TypeBuilder::FunctionTypeMap     TypeBuilder::function_types;
TypeBuilder::PromiseTypeMap      TypeBuilder::promise_types;
TypeBuilder::ArrayTypeMap        TypeBuilder::array_types;
TypeBuilder::KindTypeMap         TypeBuilder::kind_types;
TypePtr                          TypeBuilder::any_type(new Type(ValueType::ANY));

#ifdef WIN32
// -http://stackoverflow.com/questions/16299029/resolution-of-stdchronohigh-resolution-clock-doesnt-correspond-to-measureme
#include "windows.h"

const long long HighResClock::frequency = []() -> long long 
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}();

HighResClock::time_point HighResClock::now()
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return time_point(duration(count.QuadPart * static_cast<rep>(period::den) / HighResClock::frequency));
}
#endif

} // namespace Eople
