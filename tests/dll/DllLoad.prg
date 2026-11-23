/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: DllLoad.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * DLL modules loaded while the program runs
 *
 * $ONELINER$
 * ModuleLoad(), ModuleCall() and ModuleUnload() with the modules DllModule.dll and DllClass.dll.
 *
 * $DESCRIPTION$
 * The program is not linked with the modules. ModuleLoad() loads a DLL, sets its statics and runs its INIT procedures; ModuleCall() calls its public functions by name; ModuleUnload() runs its EXIT procedures and frees it. A module whose class is registered is not freed. A missing file, a missing function and a freed handle are runtime errors. xDev must be built with make dynamic. Build the modules first, then the program:
 * xdev --shared DllModule.prg
 * xdev --shared DllClass.prg
 * xdev --dynamic DllLoad.prg
 * DllLoad.exe needs the modules, libxdev.dll and libxrdd.dll next to it or on PATH.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Module INIT                     OK
 * ModuleLoad() gives a handle     OK
 * ModuleCall() with an argument   OK
 * STATIC of the loaded module     OK
 * Second load gives same handle   OK
 * BREAK through ModuleCall()      OK
 * Missing function is an error    OK
 * Module EXIT after 2 calls       OK
 * ModuleUnload() frees the module OK
 * Freed handle is an error        OK
 * Second unload gives .F.         OK
 * Missing file is an error        OK
 * Object of a module class        OK
 * Module with a class stays       OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL nModule
   LOCAL nClass
   LOCAL xValue
   LOCAL oPoint

   nModule := ModuleLoad( "DllModule.dll" )
   s_Check( "ModuleLoad() gives a handle     ", ValType( nModule ), "N" )
   s_Check( "ModuleCall() with an argument   ", ModuleCall( nModule, "ModHello", "Ania" ), "Zażółć, Ania!" )
   ModuleCall( nModule, "ModHello", "Ola" )
   s_Check( "STATIC of the loaded module     ", ModuleCall( nModule, "ModCalls" ), 2 )
   s_Check( "Second load gives same handle   ", ModuleLoad( "DllModule.dll" ), nModule )

   BEGIN SEQUENCE
      ModuleCall( nModule, "ModBreak" )
   RECOVER USING xValue
   ENDSEQUENCE
   s_Check( "BREAK through ModuleCall()      ", xValue, "from the module" )

   s_Check( "Missing function is an error    ", s_SubCode( {|| ModuleCall( nModule, "NoSuchFunction" ) } ), 1954 )
   s_Check( "ModuleUnload() frees the module ", ModuleUnload( nModule ), .T. )
   s_Check( "Freed handle is an error        ", s_SubCode( {|| ModuleCall( nModule, "ModCalls" ) } ), 1953 )
   s_Check( "Second unload gives .F.         ", ModuleUnload( nModule ), .F. )
   s_Check( "Missing file is an error        ", s_SubCode( {|| ModuleLoad( "NoSuchModule.dll" ) } ), 1950 )

   nClass := ModuleLoad( "DllClass.dll" )
   oPoint := ModuleCall( nClass, "ModPoint", 2, 3 )
   s_Check( "Object of a module class        ", oPoint:Sum(), 5 )
   s_Check( "Module with a class stays       ", ModuleUnload( nClass ), .F. )

RETURN

// The subCode of the runtime error of the block; 0 when there is none
STATIC FUNCTION s_SubCode( bAction )

   LOCAL oError
   LOCAL bError := ErrorBlock( {| oCaught | Break( oCaught ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER USING oError
   ENDSEQUENCE
   ErrorBlock( bError )

RETURN IIF( oError == NIL, 0, oError:subCode )

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN


