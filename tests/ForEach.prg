/*
 * $DOC$
 * $NAME$
 * FOR EACH
 *
 * $ONELINER$
 * Tests FOR EACH over arrays, hashes and text.
 *
 * $DESCRIPTION$
 * Checks iteration order in both directions, byte-by-byte text iteration,
 * write-back to array and hash elements, and text updates through IN @.
 * Parallel iteration checks that the shortest source ends the loop and
 * that each source uses its own position with DESCEND.
 *
 * The test exercises the enum properties and their argument forms, including
 * moving to another index and replacing a source. It checks nested loops,
 * LOOP, EXIT, restoration of loop variables, empty sources and runtime errors
 * for unsupported source types.
 *
 * The program prints a summary and returns 1 if any assertion fails.
 * To check syntax errors, compile with FOR_EACH_INVALID set to one value
 * from 1 through 8. Each selected case must be rejected by the compiler.
 *
 * $EXAMPLES$
 * xdev tests/ForEach.prg
 * xdev -fsyntax-only -DFOR_EACH_INVALID=1 tests/ForEach.prg
 *
 * $OUTPUT$
 * FOR EACH: 144 passed, 0 failed
 *
 * $DOCEND$
 */

STATIC s_nPassed := 0
STATIC s_nFailed := 0

CREATE CLASS ForEachElement
   DATA enumIndex AS INT INIT 99
ENDCLASS

FUNCTION Main()

   s_Array()
   s_Hash()
   s_Text()
   s_Parallel()
   s_ControlFlow()
   s_EnumIndex()
   s_EnumBase()
   s_MessageDispatch()
   s_Empty()
   s_InvalidSources()

   QOut( "FOR EACH: " + LTrim( Str( s_nPassed ) ) + " passed, " + LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_Array()

   LOCAL aValues := { "A", "B", "C" }
   LOCAL cOrder := ""
   LOCAL cValue := "saved"
   LOCAL nIndex := 0

   FOR EACH cValue IN aValues
      nIndex++
      cOrder += cValue
      Check( "Array index", cValue:enumIndex, nIndex )
      Check( "Array value property", cValue:enumValue(), cValue )
      Check( "Array base property", cValue:enumBase[ cValue:enumIndex ], cValue )
      Check( "Array key", cValue:enumKey == NIL, .T. )
      Check( "Array first", cValue:enumIsFirst, nIndex == 1 )
      Check( "Array last", cValue:enumIsLast(), nIndex == 3 )
      cValue := Lower( cValue )
   NEXT cValue

   Check( "Array order", cOrder, "ABC" )
   Check( "Array write-back, including last element", aValues[ 1 ] + aValues[ 2 ] + aValues[ 3 ], "abc" )
   Check( "Array loop variable restored after NEXT", cValue, "saved" )

   cOrder := ""
   FOR EACH cValue IN @aValues DESCEND
      cOrder += cValue
      Check( "Descending array index", cValue:enumIndex, nIndex )
      Check( "Descending array first position", cValue:enumIsFirst, nIndex == 1 )
      Check( "Descending array last position", cValue:enumIsLast, nIndex == 3 )
      nIndex--
      cValue := Upper( cValue )
   NEXT
   
   Check( "Descending array order", cOrder, "cba" )
   Check( "Descending array write-back", aValues[ 1 ] + aValues[ 2 ] + aValues[ 3 ], "ABC" )

RETURN

STATIC PROCEDURE s_Hash()

   LOCAL hValues := { "b" => 20, 7 => 70, "a" => 10 }
   LOCAL aKeys := HGetKeys( hValues )
   LOCAL nDescend := Len( aKeys )
   LOCAL nIndex := 0
   LOCAL nValue := -1
   LOCAL xKey

   FOR EACH nValue IN hValues
      nIndex++
      xKey := nValue:enumKey
      Check( "Hash key at current position", xKey, aKeys[ nIndex ] )
      Check( "Hash value for current key", nValue, hValues[ xKey ] )
      Check( "Hash index", nValue:enumIndex(), nIndex )
      Check( "Hash base", nValue:enumBase()[ xKey ], nValue )
      Check( "Hash enumValue setter returns old value", nValue:enumValue( nValue + 1 ), hValues[ xKey ] )
      Check( "Hash enumValue setter updates variable", nValue:enumValue, hValues[ xKey ] + 1 )
   NEXT
   Check( "Hash visits all keys", nIndex, 3 )
   Check( "Hash string key b write-back", hValues[ "b" ], 21 )
   Check( "Hash numeric key write-back", hValues[ 7 ], 71 )
   Check( "Hash string key a write-back", hValues[ "a" ], 11 )

   FOR EACH nValue IN hValues DESCEND
      Check( "Descending hash key", nValue:enumKey(), aKeys[ nDescend ] )
      Check( "Descending hash value", nValue, hValues[ aKeys[ nDescend ] ] )
      nDescend--
   NEXT
   Check( "Descending hash visits all keys", nDescend, 0 )
   Check( "Hash loop variable restored", nValue, -1 )

RETURN

STATIC PROCEDURE s_Text()

   LOCAL cText := "ab-c"
   LOCAL cCopy := cText
   LOCAL cOrder := ""
   LOCAL cByte := "saved"
   LOCAL nIndex := 0

   FOR EACH cByte IN cText
      cOrder += cByte
      Check( "Text element is one byte", Len( cByte ), 1 )
      Check( "Text key", cByte:enumKey == NIL, .T. )
      cByte := Upper( cByte )
   NEXT
   Check( "Text order", cOrder, "ab-c" )
   Check( "Text by value keeps source", cText, "ab-c" )

   FOR EACH cByte IN @cText DESCEND
      nIndex++
      DO CASE
      CASE nIndex == 1
         cByte := "CD"
      CASE nIndex == 2
         cByte := "/"
      CASE nIndex == 3
         cByte := ""
      OTHERWISE
         cByte := 42
      ENDCASE
   NEXT
   Check( "Descending text writes first byte and ignores empty or non-text values", cText, "ab/C" )
   Check( "Text write-back preserves another variable holding original text", cCopy, "ab-c" )
   Check( "Text loop variable restored", cByte, "saved" )

   cText := Chr( 195 ) + Chr( 179 ) + Chr( 0 )
   cOrder := ""
   FOR EACH cByte IN cText DESCEND
      Check( "Non-ASCII and NUL elements have one byte", Len( cByte ), 1 )
      cOrder += cByte
   NEXT
   Check( "Text iterates bytes in reverse", cOrder, Chr( 0 ) + Chr( 179 ) + Chr( 195 ) )

RETURN

STATIC PROCEDURE s_Parallel()

   LOCAL aValues := { 1, 2, 3 }
   LOCAL cText := "ab"
   LOCAL hValues := { "one" => "!" }
   LOCAL cSeen := ""
   LOCAL nCalls := 0
   LOCAL nValue := -1
   LOCAL cByte := "saved"
   LOCAL cMark

   FOR EACH nValue, cByte IN s_Source( aValues, @nCalls ), @cText
      cSeen += LTrim( Str( nValue ) ) + cByte
      cByte := Upper( cByte )
   NEXT
   Check( "Parallel sources stop at shortest", cSeen, "1a2b" )
   Check( "Source expression evaluated once", nCalls, 1 )
   Check( "Parallel text write-back", cText, "AB" )
   Check( "Parallel first variable restored", nValue, -1 )
   Check( "Parallel second variable restored", cByte, "saved" )

   cSeen := ""
   FOR EACH nValue, cByte, cMark IN aValues, cText, hValues DESCEND
      cSeen += LTrim( Str( nValue ) ) + cByte + cMark
      Check( "Parallel descending uses first base length", nValue:enumIndex, 3 )
      Check( "Parallel descending uses second base length", cByte:enumIndex, 2 )
      Check( "Parallel descending uses third base length", cMark:enumIndex, 1 )
      Check( "Singleton is first and last", cMark:enumIsFirst .AND. cMark:enumIsLast, .T. )
   NEXT
   Check( "Descending parallel over array, text, hash", cSeen, "3B!" )

RETURN

STATIC FUNCTION s_Source( aValues, nCalls )

   nCalls++

RETURN aValues

STATIC PROCEDURE s_ControlFlow()

   LOCAL aValues := { 1, 2, 3, 4 }
   LOCAL cText := "abc"
   LOCAL cSeen := ""
   LOCAL nValue := -1
   LOCAL cByte := "saved"

   FOR EACH nValue IN aValues
      nValue *= 10
      IF nValue == 10
         LOOP
      ENDIF
      IF nValue == 30
         EXIT
      ENDIF
      cSeen += LTrim( Str( nValue ) )
   NEXT
   Check( "LOOP skips the rest of the body and EXIT stops iteration", cSeen, "20" )
   Check( "LOOP writes back the current element", aValues[ 1 ], 10 )
   Check( "EXIT writes back the current element", aValues[ 3 ], 30 )
   Check( "EXIT leaves following element untouched", aValues[ 4 ], 4 )
   Check( "EXIT restores loop variable", nValue, -1 )

   FOR EACH cByte IN @cText
      cByte := "X"
      EXIT
   NEXT
   Check( "EXIT writes back IN @ text", cText, "Xbc" )
   Check( "EXIT restores string loop variable", cByte, "saved" )

   cSeen := ""
   FOR EACH nValue IN { 10, 20 }
      FOR EACH cByte IN "ab"
         cSeen += LTrim( Str( nValue:enumIndex ) ) + cByte
         EXIT
      NEXT
      FOR EACH nValue IN { 99 }
         Check( "Nested same-name variable uses inner index", nValue:enumIndex, 1 )
         Check( "Nested same-name variable uses inner value", nValue, 99 )
      NEXT
      Check( "Inner loop restores outer variable", nValue, nValue:enumIndex * 10 )
      Check( "Inner EXIT restores its variable", cByte, "saved" )
   NEXT
   Check( "Inner EXIT leaves the outer loop running", cSeen, "1a2a" )
   Check( "Outer loop restores original variable", nValue, -1 )

RETURN

STATIC PROCEDURE s_EnumIndex()

   LOCAL aValues := { 10, 20, 30, 40 }
   LOCAL cSeen := ""
   LOCAL nValue
   LOCAL cByte
   LOCAL nCalls := 0

   FOR EACH nValue IN aValues
      IF nValue:enumIndex == 1
         nValue := 11
         Check( "enumIndex setter returns old index", nValue:enumIndex( 3 ), 1 )
         Check( "enumIndex setter loads the target element", nValue, 30 )
         Check( "enumIndex setter flushes prior value", aValues[ 1 ], 11 )
      ENDIF
      cSeen += LTrim( Str( nValue:enumIndex ) )
   NEXT
   Check( "Iteration resumes from the new index", cSeen, "34" )

   cSeen := ""
   FOR EACH nValue, cByte IN aValues, "abcd" DESCEND
      IF nValue:enumIndex == 4
         Check( "Descending enumIndex setter returns old index", nValue:enumIndex( 2 ), 4 )
      ELSE
         cSeen += cByte + LTrim( Str( cByte:enumIndex ) )
      ENDIF
   NEXT
   Check( "Descending enumIndex changes the shared iteration step", cSeen, "a1" )

   cSeen := ""
   FOR EACH nValue IN aValues
      Check( "Out-of-range enumIndex returns previous position", nValue:enumIndex( 5 ), 1 )
      Check( "Out-of-range enumIndex loads NIL", nValue == NIL, .T. )
      cSeen += "x"
   NEXT
   Check( "Index beyond end terminates the loop", cSeen, "x" )

   FOR EACH nValue IN { 1 }
      Check( "enumKey argument keeps property value", nValue:enumKey( s_Source( {}, @nCalls ) ) == NIL, .T. )
      Check( "enumIsFirst argument keeps property value", nValue:enumIsFirst( s_Source( {}, @nCalls ) ), .T. )
      Check( "enumIsLast argument keeps property value", nValue:enumIsLast( s_Source( {}, @nCalls ) ), .T. )
   NEXT
   Check( "Read-only enum property arguments are evaluated", nCalls, 3 )

RETURN

STATIC PROCEDURE s_EnumBase()

   LOCAL aOld := { 1, 2, 3 }
   LOCAL aNew := { 10, 20, 30, 40 }
   LOCAL aReturned
   LOCAL cText := "ab"
   LOCAL cReturned
   LOCAL cSeen := ""
   LOCAL nValue
   LOCAL cByte

   FOR EACH nValue, cByte IN aOld, "xyz"
      IF nValue:enumIsFirst
         nValue := 5
         aReturned := nValue:enumBase( aNew )
         Check( "enumBase returns the old base with the current write", aReturned[ 1 ], 5 )
         Check( "enumBase changes the current value at the same index", nValue, 10 )
         Check( "enumBase getter sees replacement", Len( nValue:enumBase ), 4 )
      ENDIF
      cSeen += cByte + LTrim( Str( nValue ) )
      nValue++
   NEXT
   Check( "enumBase recomputes length against other sources", cSeen, "x10y20z30" )
   Check( "enumBase previous array receives pending write", aOld[ 1 ], 5 )
   Check( "enumBase replacement receives writes", aNew[ 3 ], 31 )
   Check( "enumBase replacement stops at shorter peer", aNew[ 4 ], 40 )

   cSeen := ""
   FOR EACH nValue IN aNew
      IF nValue:enumIndex == 1
         nValue:enumBase( { 7 } )
      ENDIF
      cSeen += LTrim( Str( nValue ) )
   NEXT
   Check( "enumBase can shorten iteration", cSeen, "7" )

   cSeen := ""
   FOR EACH cByte IN @cText
      IF cByte:enumIsFirst
         cByte := "A"
         cReturned := cByte:enumBase( "xyz" )
      ENDIF
      cSeen += cByte
      cByte := Upper( cByte )
   NEXT
   Check( "enumBase returns old text with pending write", cReturned, "Ab" )
   Check( "enumBase writes old text to the linked source", cText, "Ab" )
   Check( "enumBase can extend iteration", cSeen, "xyz" )

RETURN

STATIC PROCEDURE s_MessageDispatch()

   LOCAL oValue := ForEachElement()

   oValue:enumIndex := 77
   FOR EACH oValue IN { ForEachElement() }
      Check( "Bare receiver uses the enumeration position", oValue:enumIndex, 1 )
      Check( "Parenthesized receiver uses the object's message", ( oValue ):enumIndex, 99 )
   NEXT
   Check( "After NEXT the original object receives the message", oValue:enumIndex, 77 )

RETURN

STATIC PROCEDURE s_Empty()

   LOCAL nVisits := 0
   LOCAL cText := ""
   LOCAL nValue := 99
   LOCAL cByte := "saved"

   FOR EACH nValue IN {}
      nVisits++
   NEXT
   Check( "Empty array skips body", nVisits, 0 )
   FOR EACH nValue IN { => } DESCEND
      nVisits++
   NEXT
   Check( "Empty hash skips body", nVisits, 0 )
   FOR EACH cByte IN @cText
      nVisits++
   NEXT
   Check( "Empty text skips body", nVisits, 0 )
   FOR EACH nValue, cByte IN { 1 }, "" DESCEND
      nVisits++
   NEXT
   Check( "One empty parallel source skips body", nVisits, 0 )
   Check( "Empty by-reference source remains empty", cText, "" )
   Check( "Empty loops preserve first variable", nValue, 99 )
   Check( "Empty loops preserve second variable", cByte, "saved" )

RETURN

STATIC PROCEDURE s_InvalidSources()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL nCode := 0
   LOCAL nVisits := 0
   LOCAL oError
   LOCAL xValue
   LOCAL xOther

   BEGIN SEQUENCE
      FOR EACH xValue IN 42
         nVisits++
      NEXT
   RECOVER USING oError
      nCode := oError:subCode
   END SEQUENCE
   Check( "Numeric source raises argument error", nCode, 1068 )

   nCode := 0
   BEGIN SEQUENCE
      FOR EACH xValue, xOther IN {}, NIL
         nVisits++
      NEXT
   RECOVER USING oError
      nCode := oError:subCode
   END SEQUENCE
   Check( "Invalid parallel source raises even beside empty source", nCode, 1068 )

   nCode := 0
   BEGIN SEQUENCE
      FOR EACH xValue IN { 1 }
         xValue:enumBase( .F. )
      NEXT
   RECOVER USING oError
      nCode := oError:subCode
   END SEQUENCE
   ErrorBlock( bOld )
   Check( "Invalid enumBase replacement raises argument error", nCode, 1068 )
   Check( "Invalid source skips loop body", nVisits, 0 )

RETURN

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   IF ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected
      s_nPassed++
      RETURN
   ENDIF
   s_nFailed++
   QOut( "ERROR: " + cTitle, "expected", xExpected, "got", xResult )

RETURN

#if FOR_EACH_INVALID
STATIC PROCEDURE s_InvalidSyntax()

   LOCAL xValue, xOther

#if FOR_EACH_INVALID == 1
   FOR EACH 1 IN { 1 }
#elif FOR_EACH_INVALID == 2
   FOR EACH xValue { 1 }
#elif FOR_EACH_INVALID == 3
   FOR EACH xValue, xOther IN { 1 }
#elif FOR_EACH_INVALID == 4
   FOR EACH xValue IN { 1 }, { 2 }
#elif FOR_EACH_INVALID == 5
   FOR EACH xValue IN @"abc"
#elif FOR_EACH_INVALID == 6
   FOR EACH xValue IN { 1 } DESCEND DESCEND
#else
   FOR EACH xValue IN { 1 }
#endif
#if FOR_EACH_INVALID == 7
   NEXT xOther
#elif FOR_EACH_INVALID != 8
   NEXT
#endif

RETURN
#endif
