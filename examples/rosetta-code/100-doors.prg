/*
 * $DOC$
 * $NAME$
 * 100 doors
 *
 * $ONELINER$
 * Demonstrates the 100 doors task.
 *
 * $DESCRIPTION$
 * Implements the 100 doors task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/100-doors.prg
 *
 * $OUTPUT$
 * Open doors: 1 4 9 16 25 36 49 64 81 100
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aDoor := Array( 100 )
   LOCAL nPass AS INT
   LOCAL nDoor AS INT

   AFill( aDoor, .F. )

   FOR nPass := 1 TO 100
      FOR nDoor := nPass TO 100 STEP nPass
         aDoor[ nDoor ] := .NOT. aDoor[ nDoor ]
      NEXT
   NEXT

   QOut( "Open doors:" )
   FOR nDoor := 1 TO 100
      IF aDoor[ nDoor ]
         QQOut( " " + LTrim( Str( nDoor ) ) )
      ENDIF
   NEXT
   QOut( "" )

   RETURN
