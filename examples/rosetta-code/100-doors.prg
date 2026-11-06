/*
 * 100 doors
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
