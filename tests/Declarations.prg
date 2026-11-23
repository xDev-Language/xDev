/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: Declarations.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * LOCAL and STATIC declarations
 *
 * $ONELINER$
 * Tests LOCAL and STATIC variables, initializers and array dimensions.
 *
 * $DESCRIPTION$
 * Checks defaults, declaration order, dynamic LOCAL dimensions, persistent
 * STATIC values and dimension errors. A failed check gives exit code 1.
 * Select DECLARATIONS_INVALID from 1 through 8 for a rejected declaration.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Untyped LOCAL defaults to NIL                                         OK
 * Declarations: 18 passed, 0 failed
 *
 * $DOCEND$
 */

STATIC s_nPassed := 0
STATIC s_nFailed := 0

FUNCTION Main()

   s_Defaults()
   s_Initializers()
   s_Arrays()
   s_Static()
   s_Errors()

   QOut( "Declarations: " + LTrim( Str( s_nPassed ) ) + " passed, " + ;
         LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_Defaults()

   LOCAL xValue
   LOCAL nValue AS INT
   LOCAL cValue AS STRING
   LOCAL lValue AS LOGIC
   LOCAL dValue AS DATE

   Check( "Untyped LOCAL defaults to NIL", xValue == NIL, .T. )
   Check( "Typed numeric LOCAL defaults to zero", nValue, 0 )
   Check( "Typed STRING LOCAL defaults to empty", cValue, "" )
   Check( "Typed LOGIC LOCAL defaults to false", lValue, .F. )
   Check( "Typed DATE LOCAL is empty", Empty( dValue ), .T. )

RETURN

STATIC PROCEDURE s_Initializers()

   LOCAL nFirst := 2, nSecond := nFirst + 3
   LOCAL aValue := { nFirst, nSecond }

   Check( "LOCAL initializers run from left to right", nSecond, 5 )
   Check( "A later initializer can use earlier LOCAL values", aValue[ 1 ] + aValue[ 2 ], 7 )

   nFirst := 9
   LOCAL nLate := nFirst + 1

   Check( "LOCAL may appear after executable statements", nLate, 10 )

RETURN

STATIC PROCEDURE s_Arrays()

   LOCAL nRows := 2
   LOCAL aGrid[ nRows, 3 ]
   LOCAL aCube[ 2 ][ 3 ]

   Check( "LOCAL array evaluates a dynamic dimension", Len( aGrid ), 2 )
   Check( "Comma dimensions create nested arrays", Len( aGrid[ 1 ] ), 3 )
   Check( "Repeated brackets add dimensions", Len( aCube[ 1 ] ), 3 )
   Check( "New LOCAL array elements default to NIL", aGrid[ 2 ][ 3 ] == NIL, .T. )

   aGrid[ 1 ][ 1 ] := "A"
   Check( "Rows of a multidimensional array are independent", aGrid[ 2 ][ 1 ] == NIL, .T. )

RETURN

STATIC PROCEDURE s_Static()

   Check( "STATIC initializer is available on first call", s_State(), "2:A" )
   Check( "STATIC values persist between calls", s_State(), "3:AB" )
   Check( "STATIC array dimensions persist between calls", s_StaticArray(), 1 )
   Check( "STATIC array keeps an assigned element", s_StaticArray(), 2 )

RETURN

STATIC FUNCTION s_State()

   STATIC nValue := 1
   STATIC cValue := ""

   nValue++
   cValue += Chr( 63 + nValue )

RETURN LTrim( Str( nValue ) ) + ":" + cValue

STATIC FUNCTION s_StaticArray()

   STATIC aValue[ 2, 2 ]

   aValue[ 1 ][ 1 ] := IIF( aValue[ 1 ][ 1 ] == NIL, 1, aValue[ 1 ][ 1 ] + 1 )

RETURN aValue[ 1 ][ 1 ]

STATIC PROCEDURE s_Errors()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL oError

   BEGIN SEQUENCE
      s_BadDimension( -1 )
   RECOVER USING oError
   ENDSEQUENCE

   Check( "Negative LOCAL dimension raises a bound error", oError:subCode, 1131 )
   ErrorBlock( bOld )

RETURN

STATIC PROCEDURE s_BadDimension( nSize )

   LOCAL aValue[ nSize ]

   aValue := NIL

RETURN

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   LOCAL cLine := PadR( cTitle, Max( 72, Len( cTitle ) + 2 ) )

   IF ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected
      s_nPassed++
      QOut( cLine + "OK" )
   ELSE
      s_nFailed++
      QOut( cLine + "ERROR", "expected", xExpected, "got", xResult )
   ENDIF

RETURN

#if DECLARATIONS_INVALID
STATIC PROCEDURE s_Invalid()

   LOCAL nSize := 2

#if DECLARATIONS_INVALID == 1
   LOCAL
#elif DECLARATIONS_INVALID == 2
   LOCAL aValue[]
#elif DECLARATIONS_INVALID == 3
   LOCAL aValue[ 2 ] := {}
#elif DECLARATIONS_INVALID == 4
   STATIC nValue := nSize
#elif DECLARATIONS_INVALID == 5
   STATIC aValue[ nSize ]
#elif DECLARATIONS_INVALID == 6
   STATIC aValue[ 2 ] := {}
#elif DECLARATIONS_INVALID == 7
   LOCAL nValue AS INT, nValue AS INT
#else
   LOCAL xValue :=
#endif

RETURN

#endif


