/*
 * Prime counting by trial division.
 *
 * xDev:     xdev test-xdev-hb.prg
 *           test-xdev-hb.exe 1000000
 *
 * Harbour:  hbmk2 test-xdev-hb.prg -gc3 -cflag=-O2 -gtstd -otest-hb-c
 *           test-hb-c.exe 1000000
 *
 * Harbour VM: hbmk2 test-xdev-hb.prg -gtstd -otest-hb-vm
 * Python counterpart: python test-xdev-py.py 1000000
 *
 * Without an argument the limit is 500000.
 *
 * Measured on 2026-11-06: Intel Core i7-13620H, Windows 10 Pro, 64-bit.
 * xDev 0.0.0 (r2026-11-06 10:35), Harbour 3.2.1dev (r2609180937),
 * Clang 22.1.8, CPython 3.13.11 (MSYS2 CLANG64).
 * Limit: 1000000. One warm-up, then five sequential runs per variant,
 * rotating their order. Median computation times, excluding startup/output:
 *
 *   xDev                         0.420 s
 *   Harbour C (-gc3, -O2)        1.124 s
 *   Harbour VM (default)         1.737 s
 *   CPython 3.13.11              1.423 s
 *
 * All variants: 78498 primes, sum 37550402023; independently checked by sieve.
 * xDev Seconds() resolution: 0.01 s. Median whole-process times measured
 * with the same external clock: 0.423 / 1.130 / 1.742 / 1.443 s respectively.
 *
 * AS declarations give xDev machine integer types. Harbour uses dynamic values.
 * This measures one integer workload, not overall language performance.
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
