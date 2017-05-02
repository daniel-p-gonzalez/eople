#pragma once
//#include "eople_opcodes.h"

namespace Eople
{
namespace Instruction
{

bool AddI( process_t process_ref );
bool SubI( process_t process_ref );
bool MulI( process_t process_ref );
bool DivI( process_t process_ref );
bool ModI( process_t process_ref );

bool AddF( process_t process_ref );
bool SubF( process_t process_ref );
bool MulF( process_t process_ref );
bool DivF( process_t process_ref );

bool ShiftLeft( process_t process_ref );
bool ShiftRight( process_t process_ref );
bool BAnd( process_t process_ref );
bool BXor( process_t process_ref );
bool BOr( process_t process_ref );

bool ConcatS( process_t process_ref );
bool EqualS( process_t process_ref );
bool NotEqualS( process_t process_ref );

bool GreaterThanI( process_t process_ref );
bool LessThanI( process_t process_ref );
bool EqualI( process_t process_ref );
bool NotEqualI( process_t process_ref );
bool LessEqualI( process_t process_ref );
bool GreaterEqualI( process_t process_ref );

bool GreaterThanF( process_t process_ref );
bool LessThanF( process_t process_ref );
bool EqualF( process_t process_ref );
bool NotEqualF( process_t process_ref );
bool LessEqualF( process_t process_ref );
bool GreaterEqualF( process_t process_ref );

bool And( process_t process_ref );
bool Or( process_t process_ref );

bool Store( process_t process_ref );
bool StoreArrayElement( process_t process_ref );
bool StoreArrayStringElement( process_t process_ref );
bool StringCopy( process_t process_ref );

bool SpawnProcess( process_t process_ref );

} // namespace Instruction
} // namespace Eople
