/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: ControlFlow.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Control flow
 *
 * $ONELINER$
 * Tests branches, loops, sequences and procedure calls.
 *
 * $DESCRIPTION$
 * Checks branch selection, SWITCH fall-through, FOR bounds and steps, LOOP,
 * EXIT, BREAK and RECOVER, calls with omitted arguments and RETURN. A failed check gives exit
 * code 1. Select CONTROL_FLOW_INVALID from 1 through 8 for a rejected case.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * IF selects the first true branch                                        OK
 * Control flow: 17 passed, 0 failed
 *
 * $DOCEND$
 */

STATIC s_nPassed := 0
STATIC s_nFailed := 0

FUNCTION Main()

   s_Branches()
   s_Switch()
   s_Loops()
   s_Sequence()
   s_Calls()

   QOut( "Control flow: " + LTrim( Str( s_nPassed ) ) + " passed, " + LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_Branches()

   LOCAL nCalls := 0
   LOCAL cPath := ""

   IF s_Read( @nCalls, .F. )
      cPath += "wrong"
   ELSEIF s_Read( @nCalls, .T. )
      cPath += "A"
   ELSEIF s_Read( @nCalls, .T. )
      cPath += "wrong"
   ELSE
      cPath += "wrong"
   ENDIF

   Check( "IF selects the first true branch", cPath, "A" )
   Check( "IF skips later conditions", nCalls, 2 )

   cPath := ""
   nCalls := 0

   DO CASE
   CASE s_Read( @nCalls, .F. )
      cPath += "wrong"
   CASE s_Read( @nCalls, .T. )
      cPath += "B"
   CASE s_Read( @nCalls, .T. )
      cPath += "wrong"
   ENDCASE

   Check( "DO CASE runs only the matching branch", cPath, "B" )
   Check( "DO CASE skips later conditions", nCalls, 2 )

   DO CASE
   CASE .F.
      cPath += "wrong"
   OTHERWISE
      cPath += "C"
   ENDCASE

   Check( "OTHERWISE handles an unmatched DO CASE", cPath, "BC" )

RETURN

STATIC PROCEDURE s_Switch()

   LOCAL nCalls := 0
   LOCAL nTyped AS INT := -2
   LOCAL cPath := ""

   SWITCH s_Read( @nCalls, "ab" )
   CASE "a"
      cPath += "wrong"
      EXIT

   CASE "ab"
      cPath += "A"
   CASE "later"
      cPath += "B"
   OTHERWISE
      cPath += "C"
   ENDSWITCH

   Check( "SWITCH matches exact text and falls through", cPath, "ABC" )
   Check( "SWITCH evaluates the selector once", nCalls, 1 )

   SWITCH nTyped
   CASE -2
      cPath += "D"
      EXIT

   CASE +2
      cPath += "wrong"
   ENDSWITCH

   Check( "Signed CASE and EXIT before END", cPath, "ABCD" )

RETURN

STATIC PROCEDURE s_Loops()

   LOCAL n AS INT
   LOCAL nInner AS INT
   LOCAL nCalls := 0
   LOCAL cPath := ""

   FOR n := s_Read( @nCalls, 1 ) TO 5 STEP 2
      SWITCH n
      CASE 1
         LOOP

      CASE 3
         EXIT
      ENDSWITCH

      nInner := 0

      DO WHILE nInner < 3
         nInner++

         IF nInner == 1
            LOOP
         ENDIF

         cPath += LTrim( Str( n ) ) + LTrim( Str( nInner ) )
         EXIT
      ENDDO
   NEXT n

   Check( "LOOP skips SWITCH and EXIT leaves its nearest block", cPath, "3252" )
   Check( "FOR evaluates start once and includes its limit", nCalls == 1 .AND. n == 7, .T. )

   FOR n := 4 TO 1 STEP -2
      cPath += LTrim( Str( n ) )
   NEXT

   Check( "Negative STEP stops below the limit", cPath == "325242" .AND. n == 0, .T. )

   FOR n := 3 TO 1
      cPath += "wrong"
   NEXT

   DO WHILE s_Read( @nCalls, .F. )
      cPath += "wrong"
   ENDDO

   Check( "FOR and DO WHILE can skip the body", cPath == "325242" .AND. n == 3 .AND. nCalls == 2, .T. )

RETURN

STATIC PROCEDURE s_Sequence()

   LOCAL cPath := ""
   LOCAL xValue := "saved"

   BEGIN SEQUENCE
      cPath += "A"
   RECOVER USING xValue
      cPath += "wrong"
   ENDSEQUENCE

   Check( "A successful sequence skips RECOVER", xValue, "saved" )

   BEGIN SEQUENCE
      BEGIN SEQUENCE
         BREAK "B"
      RECOVER USING xValue
         cPath += xValue
         BREAK "C"
      ENDSEQUENCE
   RECOVER USING xValue
      cPath += xValue
   ENDSEQUENCE

   Check( "BREAK in RECOVER reaches the outer sequence", cPath, "ABC" )

   BEGIN SEQUENCE
      BREAK
   RECOVER USING xValue
   ENDSEQUENCE

   Check( "BREAK without a value passes NIL", xValue == NIL, .T. )

RETURN

STATIC PROCEDURE s_Calls()

   LOCAL nValue := 3

   s_Arguments( , @nValue, nValue + 2, )

   Check( "a call updates its reference before early RETURN", nValue, 8 )

RETURN

STATIC PROCEDURE s_Arguments( xFirst, nValue, nAdd, xLast )

   Check( "a call preserves omitted slots and evaluates expressions", PCount() == 4 .AND. xFirst == NIL .AND. xLast == NIL .AND. nAdd == 5, .T. )
   nValue += nAdd

   IF nAdd == 5
      RETURN
   ENDIF

   nValue := -1

RETURN

STATIC FUNCTION s_Read( nCalls, xValue )

   nCalls++

RETURN xValue

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

#if CONTROL_FLOW_INVALID == 8
STATIC FUNCTION s_Invalid()

RETURN

#elif CONTROL_FLOW_INVALID
STATIC PROCEDURE s_Invalid()

   LOCAL n, nOther
#if CONTROL_FLOW_INVALID == 1
   IF 1
   ENDIF

#elif CONTROL_FLOW_INVALID == 2
   DO CASE
   CASE "text"
   ENDCASE

#elif CONTROL_FLOW_INVALID == 3
   SWITCH 1
   CASE 1
   CASE +1
   ENDSWITCH

#elif CONTROL_FLOW_INVALID == 4
   FOR n := 1 STEP 2
   NEXT

#elif CONTROL_FLOW_INVALID == 5
   FOR n := 1 TO 2
   NEXT nOther

#elif CONTROL_FLOW_INVALID == 6
   DO WHILE 1
   ENDDO

#elif CONTROL_FLOW_INVALID == 7
   SWITCH 1
   CASE 1
      LOOP
   ENDSWITCH

#endif

RETURN

#endif


