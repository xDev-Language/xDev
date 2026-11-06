/*
 * Closures/Value capture
 */

PROCEDURE Main()

   LOCAL aFunc := {}
   LOCAL i     AS INT

   // every code block keeps its own copy of the parameter n
   FOR i := 1 TO 10
      AAdd( aFunc, s_MakeSquare( i ) )
   NEXT

   QOut( Eval( aFunc[ 3 ] ) )
   QOut( Eval( aFunc[ 7 ] ) )

   RETURN

STATIC FUNCTION s_MakeSquare( n )

   RETURN {|| n * n }
