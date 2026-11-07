/*
 * $DOC$
 * $NAME$
 * Loops/N plus one half
 *
 * $ONELINER$
 * Demonstrates the Loops/N plus one half task.
 *
 * $DESCRIPTION$
 * Implements the Loops/N plus one half task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/loops-n-plus-one-half.prg
 *
 * $OUTPUT$
 * 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL i AS INT

   FOR i := 1 TO 10
      QQOut( LTrim( Str( i ) ) )
      IF i == 10
         EXIT
      ENDIF
      QQOut( ", " )
   NEXT
   QOut( "" )

   RETURN
