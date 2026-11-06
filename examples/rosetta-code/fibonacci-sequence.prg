/*
 * Fibonacci sequence
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 0 TO 20
      QQOut( LTrim( Str( FibRec( n ) ) ) + " " )
   NEXT
   QOut( "F(90) = " + LTrim( Str( FibIter( 90 ) ) ) )

   RETURN

// recursive
STATIC FUNCTION FibRec( n AS INT ) AS INT

   RETURN IIF( n < 2, n, FibRec( n - 1 ) + FibRec( n - 2 ) )

// iterative, exact in a 64-bit INT
STATIC FUNCTION FibIter( n AS INT ) AS INT

   LOCAL a AS INT := 0
   LOCAL b AS INT := 1
   LOCAL t AS INT
   LOCAL i AS INT

   FOR i := 1 TO n
      t := a + b
      a := b
      b := t
   NEXT

   RETURN a
