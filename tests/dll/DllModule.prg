/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: DllModule.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * DLL module
 *
 * $ONELINER$
 * A module built with --shared, used by the test DllProgram.prg.
 *
 * $DESCRIPTION$
 * The module has a STATIC variable, an INIT and an EXIT procedure, public functions and a STATIC function. Only the public functions are exported from the DLL. ModBreak() raises a BREAK and ModError() a runtime error; both must reach the program that called them. ModArray() changes an array of the program and returns a new one. ModOpenTable() opens a DBF table that the program then uses, because the program and the module share libxrdd.dll. Build it with xdev --shared DllModule.prg; xdev writes DllModule.dll and the import library DllModule.dll.a. A module may not have Main().
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

STATIC s_nCalls := 0

INIT PROCEDURE ModuleStart()

   QOut( "Module INIT                     OK" )

RETURN

EXIT PROCEDURE ModuleStop()

   QOut( "Module EXIT after " + LTrim( Str( s_nCalls ) ) + " calls       " + IIF( s_nCalls == 2, "OK", "ERROR" ) )

RETURN

// A greeting in UTF-8; counts the calls in the STATIC variable of the module
FUNCTION ModHello( cName )

   s_nCalls++

RETURN s_Greeting( cName )

// Number of calls of ModHello()
FUNCTION ModCalls()

RETURN s_nCalls

// BREAK with a value, caught by the program
FUNCTION ModBreak()

   BREAK "from the module"

RETURN NIL

// A runtime error when xValue is not a number
FUNCTION ModError( xValue )

RETURN 1 + xValue

// Adds an element to the array of the program and returns a new array
FUNCTION ModArray( aList )

   AAdd( aList, "ż" )

RETURN { Len( aList ), "Łódź" }

// A new table with one record, left open in the selected work area for the program
FUNCTION ModOpenTable()

   FErase( "dlltest.dbf" )
   dbCreate( "dlltest", { { "NAME", "C", 20, 0 } } )
   dbUseArea( .T., NIL, "dlltest", "DLLTEST", .F. )
   dbAppend()
   FieldPut( 1, "Łucja" )

RETURN Alias()

STATIC FUNCTION s_Greeting( cName )

RETURN "Zażółć, " + cName + "!"


