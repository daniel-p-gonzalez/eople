#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include "eople_symbol_table.h"

SCENARIO( "symbol table allows constants to be pushed", "[symbol_table]" ) {

    GIVEN( "An empty symbol table" ) {
        Eople::SymbolTable symbols;

        REQUIRE( symbols.symbol_count() == 0 );
        REQUIRE( symbols.constant_count() == 0 );

        WHEN( "a constant is pushed" ) {
            size_t idx = symbols.GetTableEntryIndex( "const1", true );

            THEN( "constant count changes, but not locals" ) {
                REQUIRE( symbols.constant_count() == 1 );
                REQUIRE( symbols.symbol_count() - symbols.constant_count() == 0 );
            }
        }
        WHEN( "a non-constant symbol is pushed" ) {
            size_t idx = symbols.GetTableEntryIndex( "non-const1", false );

            THEN( "symbol count changes, but not constant count" ) {
                REQUIRE( symbols.symbol_count() == 1 );
                REQUIRE( symbols.constant_count() == 0 );
            }
        }
    }
}

SCENARIO( "symbol table does not combine locals and string literals with same text", "[symbol_table]" ) {

    GIVEN( "A symbol table with a local named 'x'" ) {
        Eople::SymbolTable symbols;

        size_t idx = symbols.GetTableEntryIndex( "x", false );
        REQUIRE( symbols.symbol_count() == 1 );
        REQUIRE( symbols.constant_count() == 0 );

        WHEN( "a literal string with the same text is pushed" ) {
            size_t idx = symbols.GetTableEntryIndex( "x", true );

            THEN( "constant count increases, and total symbol count increases" ) {
                REQUIRE( symbols.constant_count() == 1 );
                REQUIRE( symbols.symbol_count() == 2 );
            }
        }
    }
}
