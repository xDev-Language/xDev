/*
 * $DOC$
 * $NAME$
 * Leap year
 *
 * $ONELINER$
 * Demonstrates the Leap year task.
 *
 * $DESCRIPTION$
 * Implements the Leap year task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/leap-year.prg
 *
 * $OUTPUT$
 * 1900 is not a leap year
 * 1994 is not a leap year
 * 1996 is a leap year
 * 2000 is a leap year
 * 2024 is a leap year
 * 2100 is not a leap year
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL nYear

   FOR EACH nYear IN { 1900, 1994, 1996, 2000, 2024, 2100 }
      QOut( Str( nYear, 4 ) + IIF( LeapYear( nYear ), " is", " is not" ) + " a leap year" )
   NEXT

   RETURN

STATIC FUNCTION LeapYear( nYear AS INT ) AS LOGIC

   RETURN ( nYear % 4 == 0 .AND. nYear % 100 != 0 ) .OR. nYear % 400 == 0
