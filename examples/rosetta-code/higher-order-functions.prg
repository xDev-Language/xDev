/*
 * $DOC$
 * $NAME$
 * Higher-order functions
 *
 * $ONELINER$
 * Demonstrates the Higher-order functions task.
 *
 * $DESCRIPTION$
 * Implements the Higher-order functions task in xDev.
 * Returns a new code block: f( g( x ) ).
 * Passes named and anonymous code blocks as arguments; the composition example evaluates (4 + 1) squared.
 *
 * $EXAMPLES$
 * examples/rosetta-code/higher-order-functions.prg
 *
 * $OUTPUT$
 * 49
 * 8
 * 15
 * 25
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL bSquare := {| x | x * x }

   QOut( s_Apply( bSquare, 7 ) )
   QOut( s_Apply( {| x | x + 1 }, 7 ) )
   QOut( s_Fold( { 1, 2, 3, 4, 5 }, {| x, y | x + y }, 0 ) )
   QOut( Eval( s_Compose( bSquare, {| x | x + 1 } ), 4 ) )

   RETURN

STATIC FUNCTION s_Apply( bFunc, x )

   RETURN Eval( bFunc, x )

STATIC FUNCTION s_Fold( aList, bFunc, xStart )

   AEval( aList, {| x | xStart := Eval( bFunc, xStart, x ) } )

   RETURN xStart
STATIC FUNCTION s_Compose( bF, bG )

   RETURN {| x | Eval( bF, Eval( bG, x ) ) }
