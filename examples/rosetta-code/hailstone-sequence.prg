/*
 * Hailstone sequence
 */

PROCEDURE Main()

   LOCAL aSeq    := Hailstone( 27 )
   LOCAL nLen    := Len( aSeq )
   LOCAL nLong   AS INT := 0
   LOCAL nNumber AS INT := 0
   LOCAL n       AS INT

   QOut( "hailstone(27) has " + Num( nLen ) + " elements: " + ;
         Num( aSeq[ 1 ] ) + ", " + Num( aSeq[ 2 ] ) + ", " + Num( aSeq[ 3 ] ) + ", " + Num( aSeq[ 4 ] ) + ", ..., " + ;
         Num( aSeq[ nLen - 3 ] ) + ", " + Num( aSeq[ nLen - 2 ] ) + ", " + Num( aSeq[ nLen - 1 ] ) + ", " + Num( aSeq[ nLen ] ) )

   FOR n := 1 TO 99999
      nLen := Len( Hailstone( n ) )
      IF nLen > nLong
         nLong   := nLen
         nNumber := n
      ENDIF
   NEXT
   QOut( "The longest sequence below 100000 starts at " + Num( nNumber ) + " and has " + Num( nLong ) + " elements" )

   RETURN

STATIC FUNCTION Hailstone( n AS INT ) AS ARRAY

   LOCAL aSeq := { n }

   DO WHILE n != 1
      n := IIF( n % 2 == 0, n / 2, 3 * n + 1 )
      AAdd( aSeq, n )
   ENDDO

   RETURN aSeq

STATIC FUNCTION Num( n ) AS STRING

   RETURN LTrim( Str( n ) )
