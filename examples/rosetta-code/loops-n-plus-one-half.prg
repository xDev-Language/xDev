/*
 * Loops/N plus one half
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
