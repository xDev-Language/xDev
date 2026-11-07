/*
 * $DOC$
 * $NAME$
 * Remove duplicate elements
 *
 * $ONELINER$
 * Demonstrates the Remove duplicate elements task.
 *
 * $DESCRIPTION$
 * Implements the Remove duplicate elements task in xDev.
 * A hash keeps the order of insertion, so the first occurrence wins.
 *
 * $EXAMPLES$
 * examples/rosetta-code/remove-duplicate-elements.prg
 *
 * $OUTPUT$
 * 1 a 2 b 3
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aList  := { 1, "a", 2, 1, "b", "a", 3, 2, 1 }
   LOCAL hSeen  := { => }
   LOCAL xValue
   FOR EACH xValue IN aList
      hSeen[ xValue ] := .T.
   NEXT
   AEval( HGetKeys( hSeen ), {| x | QQOut( x, "" ) } )

   RETURN
