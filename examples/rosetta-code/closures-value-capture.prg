/*
 * $DOC$
 * $NAME$
 * Closures/Value capture
 *
 * $ONELINER$
 * Demonstrates the Closures/Value capture task.
 *
 * $DESCRIPTION$
 * Implements the Closures/Value capture task in xDev.
 * Every code block keeps its own copy of the parameter n.
 *
 * $EXAMPLES$
 * examples/rosetta-code/closures-value-capture.prg
 *
 * $OUTPUT$
 * 9
 * 49
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aFunc := {}
   LOCAL i     AS INT
   FOR i := 1 TO 10
      AAdd( aFunc, s_MakeSquare( i ) )
   NEXT

   QOut( Eval( aFunc[ 3 ] ) )
   QOut( Eval( aFunc[ 7 ] ) )

   RETURN

STATIC FUNCTION s_MakeSquare( n )

   RETURN {|| n * n }
