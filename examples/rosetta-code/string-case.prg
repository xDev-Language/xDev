/*
 * $DOC$
 * $NAME$
 * String case
 *
 * $ONELINER$
 * Demonstrates the String case task.
 *
 * $DESCRIPTION$
 * Implements the String case task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/string-case.prg
 *
 * $OUTPUT$
 * ALPHABETA
 * alphabeta
 * AlphaBETA
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cText := "alphaBETA"

   QOut( Upper( cText ) )
   QOut( Lower( cText ) )
   QOut( Upper( Left( cText, 1 ) ) + SubStr( cText, 2 ) )

   RETURN
