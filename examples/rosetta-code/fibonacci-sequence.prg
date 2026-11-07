/*
 * $DOC$
 * $NAME$
 * Fibonacci sequence
 *
 * $ONELINER$
 * Demonstrates the Fibonacci sequence task.
 *
 * $DESCRIPTION$
 * Implements the Fibonacci sequence task in xDev.
 * Recursive.
 * Iterative, exact in a 64-bit INT.
 *
 * $EXAMPLES$
 * examples/rosetta-code/fibonacci-sequence.prg
 *
 * $OUTPUT$
 * 0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 2584 4181 6765
 * F(90) = 2880067194370816120
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 0 TO 20
      QQOut( LTrim( Str( FibRec( n ) ) ) + " " )
   NEXT
   QOut( "F(90) = " + LTrim( Str( FibIter( 90 ) ) ) )

   RETURN
STATIC FUNCTION FibRec( n AS INT ) AS INT

   RETURN IIF( n < 2, n, FibRec( n - 1 ) + FibRec( n - 2 ) )
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
