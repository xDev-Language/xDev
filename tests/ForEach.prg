/*
 * $DOC$
 * $NAME$
 * FOR EACH
 *
 * $ONELINER$
 * Tests FOR EACH over arrays, hashes and text.
 *
 * $DESCRIPTION$
 * Checks direction, parallel sources, write-back, enum properties and loop
 * control. A failed assertion gives exit code 1. FOR_EACH_INVALID=1 through
 * 8 selects a compile-time rejection.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * Array order, index and base                                             OK
 * FOR EACH: 71 passed, 0 failed
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
   s_Nested()
   s_EnumIndex()
   s_EnumBase()
   s_EmptyAndInvalid()

   QOut( "FOR EACH: " + LTrim( Str( s_nPassed ) ) + " passed, " + LTrim( Str( s_nFailed ) ) + " failed" )

RETURN IIF( s_nFailed == 0, 0, 1 )

STATIC PROCEDURE s_Array()

   LOCAL aValues := { "A", "B", "C" }
   LOCAL cValue := "saved"
   LOCAL cPath := ""
   LOCAL nIndex := 0

   FOR EACH cValue IN aValues
      nIndex++
      Check( "Array order, index and base", cValue:enumIndex == nIndex .AND. cValue == aValues[ nIndex ] .AND. ;
             cValue:enumBase[ nIndex ] == cValue, .T. )
      Check( "Array value", cValue:enumValue(), cValue )
      Check( "Array key is NIL", cValue:enumKey == NIL, .T. )
      cValue := Lower( cValue )
   NEXT cValue

   Check( "Array write-back", aValues[ 1 ] + aValues[ 2 ] + aValues[ 3 ], "abc" )
   Check( "NEXT restores array loop variable", cValue, "saved" )
   FOR EACH cValue IN @aValues DESCEND
      cPath += cValue
      Check( "Descending array position", cValue:enumIndex(), nIndex )
      Check( "Descending first position", cValue:enumIsFirst, nIndex == 1 )
      Check( "Descending last position", cValue:enumIsLast(), nIndex == 3 )
      nIndex--
      cValue := Upper( cValue )
   NEXT

   Check( "DESCEND order and write-back", cPath + aValues[ 1 ] + aValues[ 2 ] + aValues[ 3 ], "cbaABC" )

RETURN

STATIC PROCEDURE s_Hash()

   LOCAL hValues := { "b" => 20, 7 => 70, "a" => 10 }
   LOCAL aKeys := HGetKeys( hValues )
   LOCAL nIndex := Len( aKeys )
   LOCAL nValue, xKey

   FOR EACH nValue IN hValues DESCEND
      xKey := nValue:enumKey()
      Check( "Descending hash key", xKey, aKeys[ nIndex ] )
      Check( "Hash value for current key", nValue, hValues[ xKey ] )
      Check( "Hash enumBase and enumValue setter", nValue:enumValue( nValue + 1 ), nValue:enumBase()[ xKey ] )
      Check( "Hash enumValue setter updates variable", nValue:enumValue, hValues[ xKey ] + 1 )
      nIndex--
   NEXT

   Check( "Hash visits all keys", nIndex, 0 )
   Check( "Hash writes all values back to their keys", hValues[ "a" ] == 11 .AND. hValues[ "b" ] == 21 .AND. hValues[ 7 ] == 71, .T. )

RETURN

STATIC PROCEDURE s_Text()

   LOCAL cText := Chr( 195 ) + Chr( 179 ) + Chr( 0 )
   LOCAL cCopy := cText
   LOCAL cByte, cPath := ""
   LOCAL aWrites := { "XY", "", 42 }

   FOR EACH cByte IN cText
      Check( "Text yields one byte without a key", Len( cByte ) == 1 .AND. cByte:enumKey == NIL, .T. )
      cPath += cByte
      cByte := "X"
   NEXT

   Check( "Text by value reads bytes and keeps source", cPath == cCopy .AND. cText == cCopy, .T. )
   cPath := ""
   FOR EACH cByte IN @cText DESCEND
      cPath += cByte
      cByte := aWrites[ cByte:enumIndex ]
   NEXT

   Check( "DESCEND reads bytes in reverse", cPath, Chr( 0 ) + Chr( 179 ) + Chr( 195 ) )
   Check( "IN @ writes first byte and skips invalid replacements", cText, "X" + Chr( 179 ) + Chr( 0 ) )
   Check( "Text write-back preserves original copy", cCopy, Chr( 195 ) + Chr( 179 ) + Chr( 0 ) )

RETURN

STATIC PROCEDURE s_Parallel()

   LOCAL cPath := ""
   LOCAL nCalls := 0
   LOCAL nValue := -1
   LOCAL cByte := "saved", cMark

   FOR EACH nValue, cByte, cMark IN s_Source( { 1, 2, 3 }, @nCalls ), "ab", { "mark" => "!" } DESCEND
      cPath += LTrim( Str( nValue ) ) + cByte + cMark
      Check( "Parallel sources use separate positions", nValue:enumIndex == 3 .AND. cByte:enumIndex == 2 .AND. cMark:enumIndex == 1, .T. )
      Check( "Singleton is both first and last", cMark:enumIsFirst .AND. cMark:enumIsLast, .T. )
   NEXT

   Check( "Parallel iteration stops at shortest source", cPath, "3b!" )
   Check( "Source expression evaluated once", nCalls, 1 )
   Check( "Both parallel variables restored", nValue == -1 .AND. cByte == "saved", .T. )

RETURN

STATIC FUNCTION s_Source( xValue, nCalls )

   nCalls++

RETURN xValue

STATIC PROCEDURE s_Nested()

   LOCAL cText := "abc"
   LOCAL oValue := ForEachElement()
   LOCAL cByte := "saved"
   LOCAL cPath := ""

   oValue:enumIndex := 77
   FOR EACH oValue IN { ForEachElement(), ForEachElement() }
      FOR EACH oValue IN { ForEachElement() }
         Check( "Nested same-name loop uses its own index", oValue:enumIndex, 1 )
      NEXT

      Check( "Outer enum index and object property restored", oValue:enumIndex == Len( cPath ) + 1 .AND. ( oValue ):enumIndex == 99, .T. )
      FOR EACH cByte IN @cText
         IF cByte:enumIndex == 1
            cByte := "A"
            LOOP
         ENDIF

         cPath += cByte
         cByte := "B"
         EXIT
      NEXT

      Check( "Inner EXIT restores variable and writes back through IN @", cByte == "saved" .AND. cText == "ABc", .T. )
   NEXT

   Check( "Inner EXIT keeps outer loop running", cPath, "bB" )
   Check( "Original object receives messages after NEXT", oValue:enumIndex, 77 )

RETURN

STATIC PROCEDURE s_EnumIndex()

   LOCAL aValues := { 1, 2, 3, 4 }
   LOCAL nValue, cByte
   LOCAL cPath := ""
   LOCAL nCalls := 0

   FOR EACH nValue, cByte IN aValues, "abcd" DESCEND
      IF nValue:enumIndex == 4
         nValue := 5
         Check( "enumIndex returns old position and writes back", nValue:enumIndex( 2 ), 4 )
         Check( "enumIndex loads replacement position", nValue == 2 .AND. aValues[ 4 ] == 5, .T. )
         LOOP
      ENDIF

      cPath += cByte
   NEXT

   Check( "enumIndex changes the shared iteration step", cPath, "a" )
   FOR EACH nValue IN { 1 }
      Check( "Read-only enum arguments preserve values", nValue:enumKey( s_Source( NIL, @nCalls ) ) == NIL .AND. ;
             nValue:enumIsFirst( s_Source( NIL, @nCalls ) ) .AND. nValue:enumIsLast( s_Source( NIL, @nCalls ) ), .T. )
      Check( "All read-only arguments were evaluated", nCalls, 3 )
      Check( "Out-of-range enumIndex loads NIL", nValue:enumIndex( 2 ) == 1 .AND. nValue == NIL, .T. )
   NEXT

RETURN

STATIC PROCEDURE s_EnumBase()

   LOCAL cText := "ab"
   LOCAL cByte, cPeer
   LOCAL cPath := ""

   FOR EACH cByte, cPeer IN @cText, "123"
      IF cByte:enumIndex == 1
         cByte := "A"
         Check( "enumBase returns the old source with its pending write", cByte:enumBase( "wxyz" ), "Ab" )
         Check( "enumBase loads replacement at the same position", cByte, "w" )
      ENDIF

      cPath += cByte + cPeer
   NEXT

   Check( "enumBase recomputes count using the shortest source", cPath, "w1x2y3" )
   Check( "enumBase writes back and detaches the original IN @ source", cText, "Ab" )

RETURN

STATIC PROCEDURE s_EmptyAndInvalid()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL aSources := { { "Empty array", {}, 0 }, { "Empty hash", { => }, 0 }, { "Empty text", "", 0 }, ;
                       { "Numeric source", 42, 1068 }, { "NIL source", NIL, 1068 } }
   LOCAL nCase, oError
   LOCAL xValue := "saved"
   LOCAL nVisits := 0

   FOR nCase := 1 TO Len( aSources )
      oError := NIL
      BEGIN SEQUENCE
         FOR EACH xValue IN aSources[ nCase ][ 2 ]
            nVisits++
         NEXT
      RECOVER USING oError
      END SEQUENCE

      Check( aSources[ nCase ][ 1 ], IIF( oError == NIL, 0, oError:subCode ), aSources[ nCase ][ 3 ] )
   NEXT

   ErrorBlock( bOld )
   Check( "Empty and invalid sources skip body and preserve variable", nVisits == 0 .AND. xValue == "saved", .T. )

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
