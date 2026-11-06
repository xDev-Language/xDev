/*
 * Factorial
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 0 TO 20
      QOut( Str( n, 2 ) + "! = " + Str( FactRec( n ), 19 ) + Str( FactIter( n ), 20 ) )
   NEXT

   RETURN

// recursive
STATIC FUNCTION FactRec( n AS INT ) AS INT

   RETURN IIF( n < 2, 1, n * FactRec( n - 1 ) )

// iterative
STATIC FUNCTION FactIter( n AS INT ) AS INT

   LOCAL nResult AS INT := 1
   LOCAL i       AS INT

   FOR i := 2 TO n
      nResult *= i
   NEXT

   RETURN nResult
