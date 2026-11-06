/*
 * Loops/For
 */

PROCEDURE Main()

   LOCAL i     AS INT
   LOCAL j     AS INT
   LOCAL cLine AS STRING

   FOR i := 1 TO 5
      cLine := ""
      FOR j := 1 TO i
         cLine += "*"
      NEXT
      QOut( cLine )
   NEXT

   RETURN
