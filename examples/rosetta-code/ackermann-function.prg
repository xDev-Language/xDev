/*
 * Ackermann function
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
