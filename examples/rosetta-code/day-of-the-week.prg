/*
 * Day of the week
 */

#include "set.ch"
PROCEDURE Main()

   LOCAL nYear AS INT

   Set( _SET_DATEFORMAT, "yyyy-mm-dd" )

   // DoW() returns 1 for Sunday
   FOR nYear := 2008 TO 2121
      IF DoW( CToD( Str( nYear, 4 ) + "-12-25" ) ) == 1
         QQOut( Str( nYear, 4 ) + " " )
      ENDIF
   NEXT

   RETURN
