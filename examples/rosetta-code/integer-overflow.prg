/*
 * $DOC$
 * $NAME$
 * Integer overflow
 *
 * $ONELINER$
 * Demonstrates the Integer overflow task.
 *
 * $DESCRIPTION$
 * Implements the Integer overflow task in xDev.
 * A typed integer never wraps around: a result outside its range raises a runtime error.
 *
 * $EXAMPLES$
 * examples/rosetta-code/integer-overflow.prg
 *
 * $OUTPUT$
 * 32-bit signed
 *    -(-2147483647-1)                            -> Value outside declared type range
 *    2000000000 + 2000000000                     -> Value outside declared type range
 *    -2147483647 - 2147483647                    -> Value outside declared type range
 *    46341 * 46341                               -> Value outside declared type range
 *    (-2147483647-1) / -1                        -> Value outside declared type range
 * 64-bit signed
 *    -(-9223372036854775807-1)                   -> Value outside declared type range
 *    5000000000000000000 + 5000000000000000000   -> Value outside declared type range
 *    -9223372036854775807 - 9223372036854775807  -> Value outside declared type range
 *    3037000500 * 3037000500                     -> Value outside declared type range
 *    (-9223372036854775807-1) / -1               -> Value outside declared type range
 * 32-bit unsigned
 *    -4294967295                                 -> Value outside declared type range
 *    3000000000 + 3000000000                     -> Value outside declared type range
 *    2147483647 - 4294967295                     -> Value outside declared type range
 *    65537 * 65537                               -> Value outside declared type range
 * 64-bit unsigned
 *    -18446744073709551615                       -> Value outside declared type range
 *    10000000000000000000 + 10000000000000000000 -> Value outside declared type range
 *    9223372036854775807 - 18446744073709551615  -> Value outside declared type range
 *    4294967296 * 4294967296                     -> Value outside declared type range
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL n32 AS INT32
   LOCAL n64 AS INT64
   LOCAL u32 AS UINT32
   LOCAL u64 AS UINT64

   QOut( "32-bit signed" )
   s_Try( "-(-2147483647-1)", {|| n32 := -( -2147483647 - 1 ) } )
   s_Try( "2000000000 + 2000000000", {|| n32 := 2000000000 + 2000000000 } )
   s_Try( "-2147483647 - 2147483647", {|| n32 := -2147483647 - 2147483647 } )
   s_Try( "46341 * 46341", {|| n32 := 46341 * 46341 } )
   s_Try( "(-2147483647-1) / -1", {|| n32 := ( -2147483647 - 1 ) / -1 } )

   QOut( "64-bit signed" )
   s_Try( "-(-9223372036854775807-1)", {|| n64 := -( -9223372036854775807 - 1 ) } )
   s_Try( "5000000000000000000 + 5000000000000000000", {|| n64 := 5000000000000000000 + 5000000000000000000 } )
   s_Try( "-9223372036854775807 - 9223372036854775807", {|| n64 := -9223372036854775807 - 9223372036854775807 } )
   s_Try( "3037000500 * 3037000500", {|| n64 := 3037000500 * 3037000500 } )
   s_Try( "(-9223372036854775807-1) / -1", {|| n64 := ( -9223372036854775807 - 1 ) / -1 } )

   QOut( "32-bit unsigned" )
   s_Try( "-4294967295", {|| u32 := -4294967295 } )
   s_Try( "3000000000 + 3000000000", {|| u32 := 3000000000 + 3000000000 } )
   s_Try( "2147483647 - 4294967295", {|| u32 := 2147483647 - 4294967295 } )
   s_Try( "65537 * 65537", {|| u32 := 65537 * 65537 } )

   QOut( "64-bit unsigned" )
   s_Try( "-18446744073709551615", {|| u64 := -18446744073709551615 } )
   s_Try( "10000000000000000000 + 10000000000000000000", {|| u64 := 10000000000000000000 + 10000000000000000000 } )
   s_Try( "9223372036854775807 - 18446744073709551615", {|| u64 := 9223372036854775807 - 18446744073709551615 } )
   s_Try( "4294967296 * 4294967296", {|| u64 := 4294967296 * 4294967296 } )

   RETURN
STATIC PROCEDURE s_Try( cText AS STRING, bAction AS CODEBLOCK )

   LOCAL bError := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL oError

   BEGIN SEQUENCE
      QOut( "   " + PadR( cText, 44 ) + "= " + LTrim( Str( Eval( bAction ) ) ) )
   RECOVER USING oError
      QOut( "   " + PadR( cText, 44 ) + "-> " + oError:description )
   END SEQUENCE
   ErrorBlock( bError )

   RETURN
