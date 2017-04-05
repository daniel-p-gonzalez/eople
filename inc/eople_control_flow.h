#pragma once
//#include "eople_opcodes.h"

namespace Eople
{
namespace Instruction
{

bool ForI( process_t process_ref );
bool ForF( process_t process_ref );
bool ForA( process_t process_ref );
bool While( process_t process_ref );
bool WhenRegister( process_t process_ref );
bool WheneverRegister( process_t process_ref );
bool Ready( process_t process_ref );
bool GetValue( process_t process_ref );
bool When( process_t process_ref );
bool Whenever( process_t process_ref );
bool Jump( process_t process_ref );
bool JumpIf( process_t process_ref );
bool JumpGT( process_t process_ref );
bool JumpLT( process_t process_ref );
bool JumpEQ( process_t process_ref );
bool JumpNEQ( process_t process_ref );
bool JumpLEQ( process_t process_ref );
bool JumpGEQ( process_t process_ref );
bool Return( process_t process_ref );
bool ReturnValue( process_t process_ref );
bool FunctionCall( process_t process_ref );
bool ProcessMessage( process_t process_ref );

} // namespace Instruction
} // namespace Eople
