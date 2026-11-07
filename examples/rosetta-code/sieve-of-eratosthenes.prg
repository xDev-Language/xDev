/*
 * $DOC$
 * $NAME$
 * Sieve of Eratosthenes
 *
 * $ONELINER$
 * Demonstrates the Sieve of Eratosthenes task.
 *
 * $DESCRIPTION$
 * Implements the Sieve of Eratosthenes task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sieve-of-eratosthenes.prg
 *
 * $OUTPUT$
 * 2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 73 79 83 89 97
 *
 * $DOCEND$
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
