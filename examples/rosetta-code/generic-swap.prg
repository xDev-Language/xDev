/*
 * $DOC$
 * $NAME$
 * Generic swap
 *
 * $ONELINER$
 * Demonstrates the Generic swap task.
 *
 * $DESCRIPTION$
 * Implements the Generic swap task in xDev.
 * Parameters passed by reference (@) may hold any type.
 *
 * $EXAMPLES$
 * examples/rosetta-code/generic-swap.prg
 *
 * $OUTPUT$
 * two 1
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL a := 1
   LOCAL b := "two"

   Swap( @a, @b )
   QOut( a, b )

   RETURN
STATIC PROCEDURE Swap( x, y )

   LOCAL t := x

   x := y
   y := t

   RETURN
