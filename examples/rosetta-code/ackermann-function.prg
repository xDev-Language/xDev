/*
 * $DOC$
 * $NAME$
 * Ackermann function
 *
 * $ONELINER$
 * Demonstrates the Ackermann function task.
 *
 * $DESCRIPTION$
 * Implements the Ackermann function task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/ackermann-function.prg
 *
 * $OUTPUT$
 *     1    2    3    4    5    6    7
 *     2    3    4    5    6    7    8
 *     3    5    7    9   11   13   15
 *     5   13   29   61  125  253  509
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL m     AS INT
   LOCAL n     AS INT
   LOCAL cLine AS STRING

   FOR m := 0 TO 3
      cLine := ""
      FOR n := 0 TO 6
         cLine += Str( Ackermann( m, n ), 5 )
      NEXT
      QOut( cLine )
   NEXT

   RETURN

STATIC FUNCTION Ackermann( m AS INT, n AS INT ) AS INT

   DO CASE
   CASE m == 0
      RETURN n + 1
   CASE n == 0
      RETURN Ackermann( m - 1, 1 )
   ENDCASE

   RETURN Ackermann( m - 1, Ackermann( m, n - 1 ) )
