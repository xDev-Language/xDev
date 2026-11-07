/*
 * $DOC$
 * $NAME$
 * Binary search
 *
 * $ONELINER$
 * Demonstrates the Binary search task.
 *
 * $DESCRIPTION$
 * Implements the Binary search task in xDev.
 * Returns the position, or 0 when the value is not there.
 *
 * $EXAMPLES$
 * examples/rosetta-code/binary-search.prg
 *
 * $OUTPUT$
 *  2 -> 1
 * 23 -> 9
 * 47 -> 15
 * 24 -> 0
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aList := { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47 }
   LOCAL nFind

   FOR EACH nFind IN { 2, 23, 47, 24 }
      QOut( Str( nFind, 2 ) + " -> " + LTrim( Str( BinarySearch( aList, nFind ) ) ) )
   NEXT

   RETURN
STATIC FUNCTION BinarySearch( aList AS ARRAY, xValue ) AS INT

   LOCAL nLow  AS INT := 1
   LOCAL nHigh AS INT := Len( aList )
   LOCAL nMid  AS INT

   DO WHILE nLow <= nHigh
      nMid := Int( ( nLow + nHigh ) / 2 )
      DO CASE
      CASE aList[ nMid ] > xValue
         nHigh := nMid - 1
      CASE aList[ nMid ] < xValue
         nLow := nMid + 1
      OTHERWISE
         RETURN nMid
      ENDCASE
   ENDDO

   RETURN 0
