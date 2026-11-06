/*
 * Pascal's triangle
 */

PROCEDURE Main()

   Pascal( 10 )

   RETURN

// Prints nothing when nRows <= 0.
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
