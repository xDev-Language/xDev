/*
 * $DOC$
 * $NAME$
 * Day of the week
 *
 * $ONELINER$
 * Demonstrates the Day of the week task.
 *
 * $DESCRIPTION$
 * Implements the Day of the week task in xDev.
 * DoW() returns 1 for Sunday.
 *
 * $EXAMPLES$
 * examples/rosetta-code/day-of-the-week.prg
 *
 * $OUTPUT$
 * 2011 2016 2022 2033 2039 2044 2050 2061 2067 2072 2078 2089 2095 2101 2107 2112 2118
 *
 * $DOCEND$
 */

#include "set.ch"
PROCEDURE Main()

   LOCAL nYear AS INT

   Set( _SET_DATEFORMAT, "yyyy-mm-dd" )
   FOR nYear := 2008 TO 2121
      IF DoW( CToD( Str( nYear, 4 ) + "-12-25" ) ) == 1
         QQOut( Str( nYear, 4 ) + " " )
      ENDIF
   NEXT

   RETURN
