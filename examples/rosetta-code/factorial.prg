/*
 * $DOC$
 * $NAME$
 * Factorial
 *
 * $ONELINER$
 * Demonstrates the Factorial task.
 *
 * $DESCRIPTION$
 * Implements the Factorial task in xDev.
 * Recursive.
 * Iterative.
 *
 * $EXAMPLES$
 * examples/rosetta-code/factorial.prg
 *
 * $OUTPUT$
 *  0! =                   1                   1
 *  1! =                   1                   1
 *  2! =                   2                   2
 *  3! =                   6                   6
 *  4! =                  24                  24
 *  5! =                 120                 120
 *  6! =                 720                 720
 *  7! =                5040                5040
 *  8! =               40320               40320
 *  9! =              362880              362880
 * 10! =             3628800             3628800
 * 11! =            39916800            39916800
 * 12! =           479001600           479001600
 * 13! =          6227020800          6227020800
 * 14! =         87178291200         87178291200
 * 15! =       1307674368000       1307674368000
 * 16! =      20922789888000      20922789888000
 * 17! =     355687428096000     355687428096000
 * 18! =    6402373705728000    6402373705728000
 * 19! =  121645100408832000  121645100408832000
 * 20! = 2432902008176640000 2432902008176640000
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 0 TO 20
      QOut( Str( n, 2 ) + "! = " + Str( FactRec( n ), 19 ) + Str( FactIter( n ), 20 ) )
   NEXT

   RETURN
STATIC FUNCTION FactRec( n AS INT ) AS INT

   RETURN IIF( n < 2, 1, n * FactRec( n - 1 ) )
STATIC FUNCTION FactIter( n AS INT ) AS INT

   LOCAL nResult AS INT := 1
   LOCAL i       AS INT

   FOR i := 2 TO n
      nResult *= i
   NEXT

   RETURN nResult
