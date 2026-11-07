/*
 * $DOC$
 * $NAME$
 * Loops/Downward for
 *
 * $ONELINER$
 * Demonstrates the Loops/Downward for task.
 *
 * $DESCRIPTION$
 * Implements the Loops/Downward for task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/loops-downward-for.prg
 *
 * $OUTPUT$
 * 10
 * 9
 * 8
 * 7
 * 6
 * 5
 * 4
 * 3
 * 2
 * 1
 * 0
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL i AS INT

   FOR i := 10 TO 0 STEP -1
      QOut( i )
   NEXT

   RETURN
