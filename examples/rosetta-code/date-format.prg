/*
 * $DOC$
 * $NAME$
 * Date format
 *
 * $ONELINER$
 * Demonstrates the Date format task.
 *
 * $DESCRIPTION$
 * Implements the Date format task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/date-format.prg
 *
 * $OUTPUT$
 * 2026-11-07
 * Saturday, November 7, 2026
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL dToday := Date()
   LOCAL cDate  := DToS( dToday )

   QOut( Left( cDate, 4 ) + "-" + SubStr( cDate, 5, 2 ) + "-" + Right( cDate, 2 ) )
   QOut( CDoW( dToday ) + ", " + CMonth( dToday ) + " " + LTrim( Str( Day( dToday ) ) ) + ", " + LTrim( Str( Year( dToday ) ) ) )

   RETURN
