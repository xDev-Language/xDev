/*
 * Sieve of Eratosthenes
 */

PROCEDURE Main()

   LOCAL nPrime

   FOR EACH nPrime IN Sieve( 100 )
      QQOut( LTrim( Str( nPrime ) ) + " " )
   NEXT

   RETURN

STATIC FUNCTION Sieve( nLimit AS INT ) AS ARRAY

   LOCAL aComposite := Array( nLimit )
   LOCAL aPrimes    := {}
   LOCAL i          AS INT
   LOCAL j          AS INT

   AFill( aComposite, .F. )

   FOR i := 2 TO nLimit
      IF .NOT. aComposite[ i ]
         AAdd( aPrimes, i )
         FOR j := i * i TO nLimit STEP i
            aComposite[ j ] := .T.
         NEXT
      ENDIF
   NEXT

   RETURN aPrimes
