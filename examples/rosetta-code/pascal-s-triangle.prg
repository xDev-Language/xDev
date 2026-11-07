/*
 * $DOC$
 * $NAME$
 * Pascal's triangle
 *
 * $ONELINER$
 * Demonstrates the Pascal's triangle task.
 *
 * $DESCRIPTION$
 * Implements the Pascal's triangle task in xDev.
 * Prints nothing when nRows <= 0.
 *
 * $EXAMPLES$
 * examples/rosetta-code/pascal-s-triangle.prg
 *
 * $OUTPUT$
 *                      1
 *                    1   1
 *                  1   2   1
 *                1   3   3   1
 *              1   4   6   4   1
 *            1   5  10  10   5   1
 *          1   6  15  20  15   6   1
 *        1   7  21  35  35  21   7   1
 *      1   8  28  56  70  56  28   8   1
 *    1   9  36  84 126 126  84  36   9   1
 *
 * $DOCEND$
 */

PROCEDURE Main()

   Pascal( 10 )

   RETURN
STATIC PROCEDURE Pascal( nRows AS INT )

   LOCAL aRow := { 1 }
   LOCAL aNext
   LOCAL nRow AS INT
   LOCAL i    AS INT
   LOCAL cLine

   FOR nRow := 1 TO nRows
      cLine := Space( ( nRows - nRow ) * 2 )
      AEval( aRow, {| n | cLine += Str( n, 4 ) } )
      QOut( cLine )

      aNext := Array( nRow + 1 )
      aNext[ 1 ] := 1
      aNext[ nRow + 1 ] := 1
      FOR i := 2 TO nRow
         aNext[ i ] := aRow[ i - 1 ] + aRow[ i ]
      NEXT
      aRow := aNext
   NEXT

   RETURN
