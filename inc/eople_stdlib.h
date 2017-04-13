#pragma once
//#include "eople_opcodes.h"

namespace Eople
{
namespace Instruction
{

bool PrintI( process_t process_ref );
bool PrintF( process_t process_ref );
bool PrintS( process_t process_ref );
bool PrintIArr( process_t process_ref );
bool PrintFArr( process_t process_ref );
bool PrintSArr( process_t process_ref );
bool GetLine( process_t process_ref );
bool ArrayConstructor( process_t process_ref );
bool ArrayPush( process_t process_ref );
bool ArrayPushArray( process_t process_ref );
bool ArrayPushString( process_t process_ref );
bool ArraySize( process_t process_ref );
bool ArrayTop( process_t process_ref );
bool ArrayTopArray( process_t process_ref );
bool ArrayTopString( process_t process_ref );
bool ArrayPop( process_t process_ref );
bool ArrayClear( process_t process_ref );
bool ArrayDeref( process_t process_ref );
bool GetTime( process_t process_ref );
bool Timer( process_t process_ref );
bool SleepMilliseconds( process_t process_ref );

bool IntToFloat( process_t process_ref );
bool FloatToInt( process_t process_ref );

bool IntToString( process_t process_ref );
bool FloatToString( process_t process_ref );
bool PromiseToString( process_t process_ref );

bool GetURL( process_t process_ref );

} // namespace Instruction
} // namespace Eople
