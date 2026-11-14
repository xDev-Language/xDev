/*
 * $DOC$
 * $NAME$
 * Error
 *
 * $ONELINER$
 * Tests the Error class: Error():New(), ErrorNew() and the object of a runtime error.
 *
 * $SYNTAX$
 * Error():New() --> oError
 * ErrorNew( [cSubSystem], [nGenCode], [nSubCode], [cOperation], [cDescription], [aArgs] ) --> oError
 * Throw( oError ) --> NIL
 *
 * $DESCRIPTION$
 * An Error object describes a runtime error. A new object has empty texts, zero numbers, .F. in the logical variables and NIL in args and cargo. The variables have types, so a text assigned to genCode is an error. The block of ErrorBlock() receives an object of the same class. Throw() raises an Error object of the program as a runtime error; the block of ErrorBlock() receives that very object, so cargo and the other variables keep their values.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Error():New() gives an object     OK
 * description is empty              OK
 * genCode is 0                      OK
 * canRetry is .F.                   OK
 * args is NIL                       OK
 * description can be changed        OK
 * tries can be changed              OK
 * cargo keeps any value             OK
 * ErrorNew(): subSystem             OK
 * ErrorNew(): genCode               OK
 * ErrorNew(): subCode               OK
 * ErrorNew(): operation             OK
 * ErrorNew(): description           OK
 * ErrorNew(): args                  OK
 * filename can be changed           OK
 * text for genCode is an error      OK
 * runtime error gives an object     OK
 * runtime error: subCode            OK
 * runtime error: tries is 0         OK
 * Throw(): the same object          OK
 * Throw(): cargo survives           OK
 * Throw(): subCode                  OK
 * Throw( "text" ) is an error       OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oError := Error():New()
   LOCAL oFile
   LOCAL oRuntime
   LOCAL oThrown

   s_Check( "Error():New() gives an object     ", ValType( oError ), "O" )
   s_Check( "description is empty              ", oError:description, "" )
   s_Check( "genCode is 0                      ", oError:genCode, 0 )
   s_Check( "canRetry is .F.                   ", oError:canRetry, .F. )
   s_Check( "args is NIL                       ", oError:args, NIL )

   oError:description := "Disk full"
   oError:tries       := 3
   oError:cargo       := "any value"

   s_Check( "description can be changed        ", oError:description, "Disk full" )
   s_Check( "tries can be changed              ", oError:tries, 3 )
   s_Check( "cargo keeps any value             ", oError:cargo, "any value" )

   oFile := ErrorNew( "MYLIB", 21, 2001, "FileOpen", "File not found", { "data.dbf" } )

   s_Check( "ErrorNew(): subSystem             ", oFile:subSystem, "MYLIB" )
   s_Check( "ErrorNew(): genCode               ", oFile:genCode, 21 )
   s_Check( "ErrorNew(): subCode               ", oFile:subCode, 2001 )
   s_Check( "ErrorNew(): operation             ", oFile:operation, "FileOpen" )
   s_Check( "ErrorNew(): description           ", oFile:description, "File not found" )
   s_Check( "ErrorNew(): args                  ", oFile:args[ 1 ], "data.dbf" )

   oFile:filename := "data.dbf"

   s_Check( "filename can be changed           ", oFile:filename, "data.dbf" )

   s_Check( "text for genCode is an error      ", ValType( s_Caught( {|| oError:genCode := "text" } ) ), "O" )

   oRuntime := s_Caught( {|| AAdd( 1, 2 ) } )

   s_Check( "runtime error gives an object     ", ValType( oRuntime ), "O" )
   s_Check( "runtime error: subCode            ", oRuntime:subCode, 1102 )
   s_Check( "runtime error: tries is 0         ", oRuntime:tries, 0 )

   oFile:cargo := "kept"
   oThrown := s_Caught( {|| Throw( oFile ) } )

   s_Check( "Throw(): the same object          ", oThrown == oFile, .T. )
   s_Check( "Throw(): cargo survives           ", oThrown:cargo, "kept" )
   s_Check( "Throw(): subCode                  ", oThrown:subCode, 2001 )
   s_Check( 'Throw( "text" ) is an error       ', s_Caught( {|| Throw( "text" ) } ):subCode, 1102 )

RETURN

// Run the block and return the Error object of its runtime error, or NIL
STATIC FUNCTION s_Caught( bAction )

   LOCAL oError
   LOCAL bError := ErrorBlock( {| oCaught | Break( oCaught ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER USING oError
   END SEQUENCE
   ErrorBlock( bError )

RETURN oError

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
