/*
 * $DOC$
 * $NAME$
 * Loops/For
 *
 * $ONELINER$
 * Demonstrates the Loops/For task.
 *
 * $DESCRIPTION$
 * Implements the Loops/For task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/loops-for.prg
 *
 * $OUTPUT$
 * *
 * **
 * ***
 * ****
 * *****
 *
 * $DOCEND$
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
