/*
 * Amicable pairs
 */

PROCEDURE Main()

   LOCAL n AS INT
   LOCAL m AS INT

   FOR n := 2 TO 20000
      m := s_DivisorSum( n )
      IF m > n .AND. m <= 20000 .AND. s_DivisorSum( m ) == n
         QOut( Str( n, 5 ) + Str( m, 7 ) )
      ENDIF
   NEXT

   RETURN

// sum of the proper divisors
STATIC FUNCTION s_DivisorSum( n AS INT ) AS INT

   LOCAL nSum AS INT := 1
   LOCAL i    AS INT := 2

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
