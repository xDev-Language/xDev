/*
 * Higher-order functions
 */

PROCEDURE Main()

   LOCAL bSquare := {| x | x * x }

   QOut( s_Apply( bSquare, 7 ) )                           // a code block as an argument
   QOut( s_Apply( {| x | x + 1 }, 7 ) )                    // an anonymous code block
   QOut( s_Fold( { 1, 2, 3, 4, 5 }, {| x, y | x + y }, 0 ) )
   QOut( Eval( s_Compose( bSquare, {| x | x + 1 } ), 4 ) ) // ( 4 + 1 ) ^ 2

   RETURN

STATIC FUNCTION s_Apply( bFunc, x )

   RETURN Eval( bFunc, x )

STATIC FUNCTION s_Fold( aList, bFunc, xStart )

   AEval( aList, {| x | xStart := Eval( bFunc, xStart, x ) } )

   RETURN xStart

// returns a new code block: f( g( x ) )
STATIC FUNCTION s_Compose( bF, bG )

   RETURN {| x | Eval( bF, Eval( bG, x ) ) }
