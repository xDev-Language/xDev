/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Examples
 * File......: Shell.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Plugin shell
 *
 * $ONELINER$
 * A program that loads every DLL module of the folder plugins and starts it.
 *
 * $DESCRIPTION$
 * The shell knows no module by name. It loads each plugins\*.dll with ModuleLoad() and calls its
 * function Start() with ModuleCall(). A new module is a new DLL in the folder; the shell is not built
 * again. The shell and the modules share one runtime, libxdev.dll. Build xDev with make dynamic, then:
 * xdev --shared plugins\Invoice.prg -o plugins\Invoice.dll
 * xdev --shared plugins\Payroll.prg -o plugins\Payroll.dll
 * xdev --dynamic Shell.prg
 *
 * $EXAMPLES$
 * examples/plugin/Shell.prg
 *
 * $OUTPUT$
 * Invoice.dll: invoices ready, VAT 23%
 * Payroll.dll: payroll ready, 3 employees
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aFile
   LOCAL nModule

   FOR EACH aFile IN Directory( "plugins\*.dll" )
      nModule := ModuleLoad( "plugins\" + aFile[ 1 ] )
      QOut( aFile[ 1 ] + ": " + ModuleCall( nModule, "Start" ) )
   NEXT

   RETURN


