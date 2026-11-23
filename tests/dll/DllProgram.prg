/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: DllProgram.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * DLL program
 *
 * $ONELINER$
 * A program built with --dynamic that calls the functions of the module DllModule.dll.
 *
 * $DESCRIPTION$
 * The program and the module share one runtime, libxdev.dll, so a STATIC variable of the module keeps its value between calls, UTF-8 text passes both ways, and a BREAK or a runtime error raised in the module is caught in the program. They share libxrdd.dll as well, so a table opened in the module is open in the program. xDev must be built with make dynamic. Build the module first, then the program with the import library of the module:
 * xdev --shared DllModule.prg
 * xdev --dynamic DllProgram.prg DllModule.dll.a
 * DllProgram.exe needs DllModule.dll, libxdev.dll and libxrdd.dll next to it or on PATH.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Module INIT                     OK
 * Function of the module          OK
 * UTF-8 text from the module      OK
 * STATIC of the module            OK
 * BREAK from the module           OK
 * Runtime error from the module   OK
 * Array changed by the module     OK
 * Array made by the module        OK
 * DBF opened in the module        OK
 * Work area seen by the program   OK
 * Record written by the module    OK
 * Module EXIT after 2 calls       OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL xValue
   LOCAL bError
   LOCAL aList := { "a" }
   LOCAL aResult

   s_Check( "Function of the module          ", ModHello( "Ania" ), "Zażółć, Ania!" )
   s_Check( "UTF-8 text from the module      ", ModHello( "Łucja" ), "Zażółć, Łucja!" )
   s_Check( "STATIC of the module            ", ModCalls(), 2 )

   BEGIN SEQUENCE
      ModBreak()
   RECOVER USING xValue
   ENDSEQUENCE
   s_Check( "BREAK from the module           ", xValue, "from the module" )

   xValue := NIL
   bError := ErrorBlock( {| oError | Break( oError ) } )
   BEGIN SEQUENCE
      ModError( "a" )
   RECOVER USING xValue
   ENDSEQUENCE
   ErrorBlock( bError )
   s_Check( "Runtime error from the module   ", ValType( xValue ), "O" )

   aResult := ModArray( aList )
   s_Check( "Array changed by the module     ", aList[ 2 ], "ż" )
   s_Check( "Array made by the module        ", aResult[ 1 ] == 2 .AND. aResult[ 2 ] == "Łódź", .T. )

   s_Check( "DBF opened in the module        ", ModOpenTable(), "DLLTEST" )
   s_Check( "Work area seen by the program   ", Alias(), "DLLTEST" )
   s_Check( "Record written by the module    ", RTrim( FieldGet( 1 ) ), "Łucja" )
   dbCloseArea()
   FErase( "dlltest.dbf" )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN


