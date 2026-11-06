/*
 * Date format
 */

PROCEDURE Main()

   LOCAL dToday := Date()
   LOCAL cDate  := DToS( dToday )

   QOut( Left( cDate, 4 ) + "-" + SubStr( cDate, 5, 2 ) + "-" + Right( cDate, 2 ) )
   QOut( CDoW( dToday ) + ", " + CMonth( dToday ) + " " + LTrim( Str( Day( dToday ) ) ) + ", " + LTrim( Str( Year( dToday ) ) ) )

   RETURN
