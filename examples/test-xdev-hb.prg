/*
 * $DOC$
 * $NAME$
 * Prime-counting benchmark
 *
 * $ONELINER$
 * Compares prime counting by trial division across xDev, Harbour, and Python.
 *
 * $DESCRIPTION$
 * Counts primes up to a configurable limit; the default is 500000 and accepted limits are 0 to 10000000. Measurements recorded on 2026-11-06 used an Intel Core i7-13620H on Windows 10 Pro, 64-bit: xDev 0.0.0 (r2026-11-06 10:35), Harbour 3.2.1dev (r2609180937), Clang 22.1.8, and CPython 3.13.11 (MSYS2 CLANG64). At limit 1000000, one warm-up and five sequential runs per variant were timed in rotating order. Median compute times excluding startup and output were xDev 0.420 s, Harbour C 1.124 s, Harbour VM 1.737 s, and Python 1.423 s; whole-process times measured with the same external clock were 0.423, 1.130, 1.742, and 1.443 s. xDev Seconds() has 0.01 s resolution. All variants returned 78498 primes with sum 37550402023, independently checked by sieve. This single integer workload is not a general language-performance comparison.
 *
 * $EXAMPLES$
 * examples/test-xdev-hb.prg
 *
 * $OUTPUT$
 * Limit: 10000
 * Primes: 1229
 * Sum: 5736396
 * Seconds: 0.000000
 *
 * $DOCEND$
 */

#ifdef __HARBOUR__
   #define DEMO_INT
   #define DEMO_FLOAT
   #define DEMO_LOGIC
#else
   #define DEMO_INT AS INT
   #define DEMO_FLOAT AS FLOAT
   #define DEMO_LOGIC AS LOGIC
#endif

PROCEDURE Main( cLimit )

   LOCAL nLimit   DEMO_INT := 500000
   LOCAL n        DEMO_INT
   LOCAL nCount   DEMO_INT := 0
   LOCAL nSum     DEMO_INT := 0
   LOCAL nStart   DEMO_FLOAT
   LOCAL nElapsed DEMO_FLOAT

   IF cLimit != NIL
      nLimit := Int( Val( cLimit ) )
   ENDIF
   IF nLimit < 0 .OR. nLimit > 10000000
      QOut( "Use a limit from 0 to 10000000." )
      RETURN
   ENDIF

   nStart := Seconds()
   FOR n := 2 TO nLimit
      IF IsPrime( n )
         nCount++
         nSum += n
      ENDIF
   NEXT
   nElapsed := Seconds() - nStart
   IF nElapsed < 0
      nElapsed += 86400
   ENDIF

   QOut( "Limit: " + LTrim( Str( nLimit, 20, 0 ) ) )
   QOut( "Primes: " + LTrim( Str( nCount, 20, 0 ) ) )
   QOut( "Sum: " + LTrim( Str( nSum, 20, 0 ) ) )
   QOut( "Seconds: " + LTrim( Str( nElapsed, 12, 6 ) ) )

   RETURN

STATIC FUNCTION IsPrime( n DEMO_INT ) DEMO_LOGIC

   LOCAL nDivisor DEMO_INT := 3

   IF n == 2
      RETURN .T.
   ENDIF
   IF n % 2 == 0
      RETURN .F.
   ENDIF
   DO WHILE nDivisor * nDivisor <= n
      IF n % nDivisor == 0
         RETURN .F.
      ENDIF
      nDivisor += 2
   ENDDO

   RETURN .T.
