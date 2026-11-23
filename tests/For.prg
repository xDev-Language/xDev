/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: For.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * FOR
 *
 * $ONELINER$
 * Tests numeric FOR loops, bounds, STEP, LOOP and EXIT.
 *
 * $DESCRIPTION$
 * Checks inclusive bounds, final counter values, positive, negative and zero
 * steps, changing limit and step expressions, LOOP, EXIT and integer overflow.
 * A failed check gives exit code 1. Select FOR_INVALID from 1 through 8 for
 * a declaration or syntax error rejected by the compiler.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Default STEP includes the upper bound                                  OK
 * FOR: 18 passed, 0 failed
 *
 * $DOCEND$
 */

STATIC s_nPassed := 0
STATIC s_nFailed := 0

FUNCTION Main()

   s_DefaultStep()
   s_ExplicitSteps()
   s_DynamicExpressions()
   s_ControlFlow()
   s_Overflow()

   QOut( "FOR: " + LTrim( Str( s_nPassed ) ) + " passed, " + ;
         LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_DefaultStep()

   LOCAL n, nSum := 0, nVisits := 0

   FOR n := 1 TO 3
      nSum += n
   NEXT N

   Check( "Default STEP includes the upper bound", nSum, 6 )
   Check( "Counter advances once beyond an ascending limit", n, 4 )

   FOR n := 3 TO 1
      nVisits++
   NEXT

   Check( "Ascending FOR skips a reversed range", nVisits, 0 )
   Check( "Skipped FOR still receives its start value", n, 3 )

RETURN

STATIC PROCEDURE s_ExplicitSteps()

   LOCAL n, nFloat AS FLOAT
   LOCAL nSum := 0
   LOCAL cPath := ""

   FOR nFloat := 1.5 TO 0.5 STEP -0.5
      nSum += nFloat
   NEXT

   Check( "Negative fractional STEP includes the lower bound", nSum, 3 )
   Check( "Fractional counter advances beyond the limit", nFloat, 0 )

   FOR n := 5 TO 1 STEP -2
      cPath += LTrim( Str( n ) )
   NEXT

   Check( "Negative integer STEP selects descending values", cPath, "531" )
   Check( "Descending counter advances beyond the limit", n, -1 )

   FOR n := 4 TO 4 STEP 0
      cPath += "Z"
      EXIT
   NEXT

   Check( "STEP zero enters an equal-bound loop until EXIT", cPath, "531Z" )
   Check( "EXIT preserves the current counter", n, 4 )

RETURN

STATIC PROCEDURE s_DynamicExpressions()

   LOCAL n, nLimit := 9, nStep := 1, nSum := 0, nStartCalls := 0
   LOCAL nLimitCalls := 0, nStepCalls := 0

   FOR n := s_Value( @nStartCalls, 1 ) TO s_Value( @nLimitCalls, nLimit ) ;
         STEP s_Value( @nStepCalls, nStep )
      nSum += n
      nLimit := 5
      nStep := 2
   NEXT

   Check( "Start expression is evaluated once", nStartCalls, 1 )
   Check( "Changing limit and STEP affect later iterations", nSum, 9 )
   Check( "Dynamic STEP leaves counter beyond new limit", n, 7 )
   Check( "Limit is evaluated before every loop test", nLimitCalls, 4 )
   Check( "STEP is evaluated for tests and increments", nStepCalls, 7 )

RETURN

STATIC FUNCTION s_Value( nCalls, nValue )

   nCalls++

RETURN nValue

STATIC PROCEDURE s_ControlFlow()

   LOCAL n, cPath := ""

   FOR n := 1 TO 5
      IF n == 1
         LOOP
      ENDIF

      cPath += LTrim( Str( n ) )

      IF n == 3
         EXIT
      ENDIF
   NEXT

   Check( "LOOP applies STEP and EXIT stops the loop", cPath, "23" )
   Check( "EXIT leaves the counter at its current value", n, 3 )

RETURN

STATIC PROCEDURE s_Overflow()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL n AS INT8
   LOCAL oError

   BEGIN SEQUENCE
      FOR n := 127 TO 127
      NEXT
   RECOVER USING oError
   ENDSEQUENCE

   ErrorBlock( bOld )
   Check( "Typed counter overflow raises a range error", oError:subCode, 3105 )

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

#if FOR_INVALID
STATIC PROCEDURE s_Invalid()

   LOCAL n, nOther, cText AS STRING

#if FOR_INVALID == 1
   FOR 1 := 1 TO 2
#elif FOR_INVALID == 2
   FOR n 1 TO 2
#elif FOR_INVALID == 3
   FOR n := 1
#elif FOR_INVALID == 4
   FOR n := 1 TO 2 STEP
#elif FOR_INVALID == 5
   FOR n := 1 TO 2
   NEXT nOther
#elif FOR_INVALID == 6
   FOR cText := "a" TO "z"
   NEXT
#elif FOR_INVALID == 7
   FOR n := 1 TO "z"
   NEXT
#else
   FOR n := 1 TO 2
#endif

#if FOR_INVALID <= 4
   NEXT
#endif

RETURN

#endif


