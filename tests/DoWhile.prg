/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: DoWhile.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * DO WHILE
 *
 * $ONELINER$
 * Tests DO WHILE conditions, LOOP, EXIT and nested loops.
 *
 * $DESCRIPTION$
 * Checks the initial condition, repeated evaluation, condition side effects,
 * LOOP, EXIT, nesting, RETURN and invalid dynamic conditions. A failed check
 * gives exit code 1. Select DO_WHILE_INVALID from 1 through 8 for a construct
 * rejected by the compiler.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * False initial condition skips the body                                OK
 * DO WHILE: 12 passed, 0 failed
 *
 * $DOCEND$
 */

STATIC s_nPassed := 0
STATIC s_nFailed := 0

FUNCTION Main()

   s_Precondition()
   s_RepeatedCondition()
   s_ControlFlow()
   s_Nested()
   s_DynamicError()
   Check( "RETURN can leave a DO WHILE body", s_ReturnFromLoop(), 7 )

   QOut( "DO WHILE: " + LTrim( Str( s_nPassed ) ) + " passed, " + ;
         LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_Precondition()

   LOCAL nCalls := 0, nVisits := 0

   DO WHILE s_Logical( @nCalls, .F. )
      nVisits++
   ENDDO

   Check( "False initial condition skips the body", nVisits, 0 )
   Check( "Initial condition is evaluated once", nCalls, 1 )

RETURN

STATIC PROCEDURE s_RepeatedCondition()

   LOCAL nCalls := 0, nValue := 0
   LOCAL cPath := ""

   DO WHILE s_Logical( @nCalls, nValue < 3 )
      nValue++
      cPath += LTrim( Str( nValue ) )
   ENDDO

   Check( "DO WHILE repeats while the condition is true", cPath, "123" )
   Check( "Condition runs once more after the last body", nCalls, 4 )

RETURN

STATIC PROCEDURE s_ControlFlow()

   LOCAL nCalls := 0, nValue := 0
   LOCAL cPath := ""

   DO WHILE s_Logical( @nCalls, nValue < 5 )
      nValue++

      IF nValue == 2
         LOOP
      ENDIF

      cPath += LTrim( Str( nValue ) )

      IF nValue == 4
         EXIT
      ENDIF
   ENDDO

   Check( "LOOP skips the remainder and rechecks the condition", cPath, "134" )
   Check( "EXIT preserves values from the current iteration", nValue, 4 )
   Check( "EXIT avoids another condition evaluation", nCalls, 4 )

RETURN

STATIC PROCEDURE s_Nested()

   LOCAL nOuter := 0, nInner
   LOCAL cPath := ""

   DO WHILE nOuter < 2
      nOuter++
      nInner := 0

      DO WHILE .T.
         nInner++

         IF nInner == 1
            LOOP
         ENDIF

         cPath += LTrim( Str( nOuter ) ) + LTrim( Str( nInner ) )
         EXIT
      ENDDO
   ENDDO

   Check( "Nested LOOP and EXIT target the inner loop", cPath, "1222" )
   Check( "Inner EXIT leaves the outer loop running", nOuter, 2 )

RETURN

STATIC FUNCTION s_ReturnFromLoop()

   DO WHILE .T.
      RETURN 7
   ENDDO

RETURN 0

STATIC PROCEDURE s_DynamicError()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL nVisits := 0, oError

   BEGIN SEQUENCE
      DO WHILE { 1 }[ 1 ]
         nVisits++
      ENDDO
   RECOVER USING oError
   ENDSEQUENCE

   ErrorBlock( bOld )
   Check( "Dynamic non-logical condition raises a type error", oError:subCode, 3107 )
   Check( "Invalid condition does not enter the body", nVisits, 0 )

RETURN

STATIC FUNCTION s_Logical( nCalls, lValue ) AS LOGIC

   nCalls++

RETURN lValue

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

#if DO_WHILE_INVALID
STATIC PROCEDURE s_Invalid()

   LOCAL nValue

#if DO_WHILE_INVALID == 1
   DO WHILE
#elif DO_WHILE_INVALID == 2
   DO WHILE 1
   ENDDO
#elif DO_WHILE_INVALID == 3
   DO WHILE .T. nValue
   ENDDO
#elif DO_WHILE_INVALID == 4
   DO WHILE .T.
#elif DO_WHILE_INVALID == 5
   DO WHILE .T.
   NEXT
#elif DO_WHILE_INVALID == 6
   LOOP
#elif DO_WHILE_INVALID == 7
   EXIT
#else
   DO WHILE .T.
      DO WHILE .F.
      ENDDO
#endif

#if DO_WHILE_INVALID <= 3
   ENDDO
#endif

RETURN

#endif


