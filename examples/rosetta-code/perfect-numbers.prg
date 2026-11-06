/*
 * Perfect numbers
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

// sum of the proper divisors
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
