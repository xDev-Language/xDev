/*
 * $DOC$
 * $NAME$
 * Sort using a custom comparator
 *
 * $ONELINER$
 * Demonstrates the Sort using a custom comparator task.
 *
 * $DESCRIPTION$
 * Implements the Sort using a custom comparator task in xDev.
 * Longest first, then alphabetically without regard to case.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sort-using-a-custom-comparator.prg
 *
 * $OUTPUT$
 * strings sample sorted Here some are be to
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aWords := { "Here", "are", "some", "sample", "strings", "to", "be", "sorted" }
   ASort( aWords, , , {| a, b | IIF( Len( a ) == Len( b ), Upper( a ) < Upper( b ), Len( a ) > Len( b ) ) } )
   AEval( aWords, {| c | QQOut( c + " " ) } )

   RETURN
