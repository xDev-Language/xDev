/*
 * $DOC$
 * $NAME$
 * Greatest common divisor
 *
 * $ONELINER$
 * Demonstrates the Greatest common divisor task.
 *
 * $DESCRIPTION$
 * Implements the Greatest common divisor task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/greatest-common-divisor.prg
 *
 * $OUTPUT$
 * 21
 * 34
 * 6
 *
 * $DOCEND$
 */

PROCEDURE Main()

   QOut( GCD( 1071, 1029 ) )
   QOut( GCD( 40902, 24140 ) )
   QOut( GCD( -48, 18 ) )

   RETURN

STATIC FUNCTION GCD( a AS INT, b AS INT ) AS INT

   LOCAL t AS INT

   DO WHILE b != 0
      t := b
      b := a % b
      a := t
   ENDDO

   RETURN Abs( a )
