/*
 * $DOC$
 * $NAME$
 * Perfect numbers
 *
 * $ONELINER$
 * Demonstrates the Perfect numbers task.
 *
 * $DESCRIPTION$
 * Implements the Perfect numbers task in xDev.
 * Sum of the proper divisors.
 *
 * $EXAMPLES$
 * examples/rosetta-code/perfect-numbers.prg
 *
 * $OUTPUT$
 * 6
 * 28
 * 496
 * 8128
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 2 TO 10000
      IF IsPerfect( n )
         QOut( n )
      ENDIF
   NEXT

   RETURN

STATIC FUNCTION IsPerfect( n AS INT ) AS LOGIC

   RETURN n > 1 .AND. DivisorSum( n ) == n
STATIC FUNCTION DivisorSum( n AS INT ) AS INT

   LOCAL nSum AS INT := 1
   LOCAL i    AS INT := 2

   IF n <= 1
      RETURN 0
   ENDIF

   DO WHILE i * i <= n
      IF n % i == 0
         nSum += i
         IF i * i != n
            nSum += n / i
         ENDIF
      ENDIF
      i++
   ENDDO

   RETURN nSum
