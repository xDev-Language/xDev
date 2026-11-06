/*
 * String case
 */

PROCEDURE Main()

   LOCAL cText := "alphaBETA"

   QOut( Upper( cText ) )
   QOut( Lower( cText ) )
   QOut( Upper( Left( cText, 1 ) ) + SubStr( cText, 2 ) )

   RETURN
